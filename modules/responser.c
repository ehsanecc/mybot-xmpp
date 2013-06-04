#include "../mybot-xmpp.h"
#include "responser.h"

/* responser v2.0
 *
 * + Using the PCRE( Perl Compatible Regular Expression ).
 * + 
 */

int responser_init(char *responsefile/*[IN]*/) {
    _message(MSG_DEBUG | MSG_SUB, "responser.init(\"%s\")", responsefile);
    /* response file format:
     * !private          << make all qr's after it for private response
     * !public           << make all qr's after it for public response
     * !welcome          << make qr's after welcome strings
     * 1 "%s":"%s"       < (exact/partial/intext match)question:response
     * 2 "%s":{"%s"|"%s"}< {multiinput}:response
     * 3 ...
     * 4 .              < newline for end
     *  multiple             { }
     *  >for separating      '|'
     * string's should be in '"'
     * example:
     * "i'm ([0-9]*) years old":"so you are $1 years old, right?"
     */
    FILE *rf = fopen(responsefile, "rb");

    if (!rf) return -1;

    int num = 0, i = 0;
    
    __addressed_user = malloc(MAX_JID);

    while (!feof(rf)) if (fgetc(rf) == '\n') num++; // count lines( get the maximum possible QR's )
    rewind(rf); // back to start of file

    __qr = (QR_t*) malloc(sizeof (QR_t) * num);
    if (__qr == NULL) return 0;

    for (i = 0; i < num && !feof(rf); i++)
        if (!_read_qr(rf, &__qr[i])) i--;
    __response_count = i;
    fclose(rf);

    return i;
}

int responser_clear() {
    _message(MSG_DEBUG | MSG_SUB, "responser.clear()");
    int i;

    for (i = 0; i < __response_count; i++) {
        pcre_free(__qr[i].q);
        free(__qr[i].r);
    }
    free(__qr);
    __response_count = 0;
    free(__addressed_user);

    return 0;
}

int responser_get(char *msg/*in*/, uint options/*in*/, char *userid/*in*/, char *response/*out*/) {
    _message(MSG_DEBUG | MSG_SUB, "responser.get(\"%s\", %d, \"%s\")", msg, options, userid);
    char *buf;
    int i, r=0, ovector[30]={0};
    
    buf = strmalloc(msg); tolowers(buf); // case-insensitive
    if(userid != NULL && __addressed_user != NULL) strcpy(__addressed_user, userid);
    else if(__addressed_user != NULL) strcpy(__addressed_user, "");
    
    for (i = 0; i < __response_count; i++) {
        if ((__qr[i].q != NULL) && (__qr[i].options & options)) {
            if(__qr[i].options & FLG_CASEINSEN)
                r = pcre_exec(__qr[i].q, NULL, buf, strlen(buf), 0, PCRE_NOTEMPTY, ovector, 30);
            else if(__qr[i].options & FLG_CASESENSE)
                r = pcre_exec(__qr[i].q, NULL, msg, strlen(msg), 0, PCRE_NOTEMPTY, ovector, 30);

            if (r >= 0) { // match
                _message(MSG_DEBUG, "responser: question %d matched.", i);
                if(__qr[i].response_count > 1) _get_substring(__qr[i].r, ((unsigned)_rand())%__qr[i].response_count, response);
                else strcpy(response, __qr[i].r);
                _pcre_replace(response, msg, ovector, r);
                free(buf);
                
                return 1;
            }

        }
    }  
    free(buf);
    strcpy(response, ""); // no response

    return 0;
}

int _read_qr(FILE *file, QR_t *qr) {
    _message(MSG_DEBUG | MSG_SUB, "responser._read_qr(FILE*, QR_t*)");
    bool bstring=false;
    int n = 0, b = fgetc(file), r_count = 1;
    char q[MAX_BUFSIZE]={0}, r[MAX_BUFSIZE]={0};
    static int line = 0, options = FLG_DEFAULT; // by default, all messages are for private addressed

    if (feof(file)) return 0;
    
    switch (b) {
        case '"': bstring = true; break;
        case OPTION_FLAG:
            read_line(file, q, MAX_BUFSIZE); line++;
            
            if (!strcasecmp(q, "public")) { options |= FLG_PUBLIC; options &= ~FLG_PRIVATE; }
            else if (!strcasecmp(q, "private")) { options |= FLG_PRIVATE; options &= ~FLG_PUBLIC; }
            else if (!strcasecmp(q, "case-insensitive")) { options |= FLG_CASEINSEN; options &= ~FLG_CASESENSE; }
            else if (!strcasecmp(q, "case-sensitive")) { options |= FLG_CASESENSE; options &= ~FLG_CASEINSEN; }
            else if (!strcasecmp(q, "welcome")) options = FLG_WELCOME;
            else _message(MSG_ERROR, "responser._read_qr error: unknown option %s.", q);
            return 0;
        case '\n':
            line++;
            return 0;
        default:
            if(b != COMMENT_FLAG) _message(MSG_ERROR, "%d: unexpected character '%c'.", line, b);
            read_line(file, NULL, MAX_BUFSIZE); line++;
            return 0;
    }

    b = fgetc(file);
    while ((bstring == true || b != ':') && n < MAX_BUFSIZE && !feof(file)) {
        if(b == '"') bstring = !bstring;
        else if(b == '\\') {
            b = fgetc(file);
            switch(b) {
                case 'n': b = '\n'; break;
                case 't': b = '\t'; break;
                case 'r': b = '\r'; break;
            }
        }
        q[n++] = b;
        b = fgetc(file);
    }
    q[n-1] = 0;
    
    read_line(file, r, MAX_BUFSIZE); line++;
    strshift(r, -1, MAX_BUFSIZE);
    r[strlen(r)-1] = 0;
    
    // count responses
    n = 0;
    bstring = false;
    while (r[n]) {
        if (r[n] == '|' && bstring == false) r_count++;
        else if (r[n] == '\\') n++;
        else if (r[n] == '"') bstring = !bstring;
        n++;
    }

    {
        const char *error;
        unsigned char tptr[32];
        int of;

        qr->q = pcre_compile(q, 0, &error, &of, tptr);
        if (qr->q == NULL) {
            _message(MSG_ERROR, "responser._pcre error: %s", error);
            return 0;
        }
    }
    
    qr->r = strmalloc(r);
    qr->options = options;
    qr->response_count = r_count;

    _message(MSG_DEBUG, "%s %s:%s[%d]\n", (qr->options & FLG_WELCOME ? "welcome" : (qr->options & FLG_PRIVATE ? "private" : "public")), q, qr->r, qr->response_count);
    if(options == FLG_WELCOME) options = FLG_DEFAULT;

    return 1;
}

/* will separate and select a sub string in a multiple string { } */
void _get_substring(char *string, uint index, char *out) {
    _message(MSG_DEBUG, "responser._get_substring(\"%s\", %u)", string, index);
    uint i, p=0, len = strlen(string), string_length = 0;
    char *string_start = string;
    bool bstring = false, bflag = false;

    /* format: "str"|"str"|...
     * str as string!
     */
    for(i=0;i<=index;i++) {
        string_length = 0;
        do {
            if(string[p] == '\\') {
                string_length+=2;
                bflag = true; p++;
            }
            else if(string[p] == '"') {
                bstring = !bstring;
                if(bstring) string_start = string+p+1; // start of a string
            } else string_length++;
            p++;
        } while(p < len && bstring);
        if(string[p] != '|' && string[p] != 0) _message(MSG_ERROR, "responser: unexpected character '%c'.", string[p]);
        else p++;
    }
    
    sprintf(out, "%.*s", string_length, string_start);
    for(i=0;i<string_length && bflag;i++) {
        if(out[i] == '\\') {
            switch(out[i+1]) {
                case 't': out[i] = '\t'; break;
                case 'n': out[i] = '\n'; break;
                case 'r': out[i] = '\r'; break;
                default: out[i]= out[i+1]; break;
            }
            strshift(out+i+1, -1, MAX_BUFSIZE);
        }
    }
}

void _thread_bash(char *command) {
    system(command);
}

/***************************************************
 * this is not only pcre's substring replace,
 * but some additional flags will take effect here:
 * $id 	   id of bot (can be used in question string)
 * $room   room name (can be used in question string)
 * $time   get's current time
 * $day    get current's day
 * $bash{} execute bash and get result
 * $bash[] execute bash ( silence )
 * $0-9    regex substrings
 ***************************************************
 */
void _pcre_replace(char *str, char *msg, int *ovector, uint vectors) {
    _message(MSG_DEBUG | MSG_SUB, "responser._pcre_replace(\"%s\", \"%s\", int*, %u)", str, msg, vectors);
    uint i=0, n;
    char buf[MAX_BUFSIZE] = "";

    while (str[i]) {
        if (str[i] == '$') {
            i += 1;
            if (isdigit(str[i])) {
                n = atoi(str + i);
                if (n <= vectors) {
                    char *substring_start = msg + ovector[2 * n];
                    uint substring_length = ovector[2 * n + 1] - ovector[2 * n];
                    sprintf(buf, "%.*s%.*s%s", i - 1, str, substring_length, substring_start, str + i + 1);
                    strcpy(str, buf);
                }
            } else if(!strncmp(str+i, "id", 2)) {
                // replace pjid
                sprintf(buf, "%.*s%.*s%s", i - 1, str, (int)strlen(global.config.pjid), global.config.pjid, str + i + 2);
                strcpy(str, buf);
            } else if(!strncmp(str+i, "uid", 3)) {
                // replace userid
                sprintf(buf, "%.*s%.*s%s", i - 1, str, (int)strlen(__addressed_user), __addressed_user, str + i + 3);
                strcpy(str, buf);
            } else if(!strncmp(str+i, "room", 4)) {
                sprintf(buf, "%.*s%.*s%s", i - 1, str, (int)strlen(global.config.room), global.config.room, str + i + 4);
                strcpy(str, buf);
                // replace room name
            } else if(!strncmp(str+i, "time", 4)) {
                // replace time with format %h:%m:%s
                char buf2[16];
                time_t rawtime;
                time(&rawtime);
                strftime(buf2, 16, "%H:%M", localtime(&rawtime));
                sprintf(buf, "%.*s%.*s%s", i - 1, str, (int)strlen(buf2), buf2, str + i + 4);
                strcpy(str, buf);
            } else if(!strncmp(str+i, "day", 3)) {
                // replace day of week, Saturday, Sunday, Monday, ...
                char buf2[16];
                time_t rawtime;
                time(&rawtime);
                strftime(buf2, 16, "%A", localtime(&rawtime));
                sprintf(buf, "%.*s%.*s%s", i - 1, str, (int)strlen(buf2), buf2, str + i + 3);
                strcpy(str, buf);
            } else if (!strncmp(str + i, "bash", 4)) {
                // 
                int m = 0;
                char buf2[strlen(str) - i];
                _pcre_replace(str + i + 5, msg, ovector, vectors);
                n = i + 5;
                while (str[n] != '}' && str[n] != ']') {
                    if (str[n] == '\\') n++;
                    buf2[m++] = str[n++];
                }
                buf2[m] = '\0';
                if(str[i+4] == '[') {
                    pthread_t t;
                    pthread_create(&t, NULL, _thread_bash, buf2);
                }
                else {
                    FILE *pipe = popen(buf2, "r");
                    buf2[fread(buf2, 1, 512, pipe)] = '\0';
                    fclose(pipe);
                }
                if (str[i + 4] == '{')
                    sprintf(buf, "%.*s%.*s%s", i - 1, str, (int) strlen(buf2), buf2, str + n + 1);
                else
                    sprintf(buf, "%.*s%s", i - 1, str, str + n + 1);
                strcpy(str, buf);
            } else if (str[i] == '{') { // script

            }
        }
        i += 1;
    }
}

/* 
 * clear message from "from" string
 */
int responser_clean_message(char *msg, char *from, char *cmsg) {
    _message(MSG_DEBUG | MSG_SUB, "responser.clean_message(\"%s\", \"%s\", \"%s\")", msg, from, cmsg);
    int ml = strlen(msg), fl = strlen(from), n=0, m, i = 0;
    
    strcpy(cmsg, msg);
    
    // if multi-line, cut last line
    while(msg[i]) {
        if(msg[i]=='\n') { n++; m=i; }
        i++;
    } if(n>0) strshift(cmsg,-(m), MAX_BUFSIZE);
    
    if (strcstr(cmsg, from) != NULL)
        strshift(strcstr(cmsg, from), -(fl+1), MAX_BUFSIZE); // shift to left
    
    ml = strlen(cmsg) - 1; n=0; i=0;
    
    while(cmsg[ml] == ' ') // strip string(right)
        cmsg[ml--] = 0;

    while (cmsg[0] == ' ') // strip string(left)
        strshift(cmsg, -1, MAX_BUFSIZE);
    
    while(cmsg[i]) { // remove duplicate spaces
        if(cmsg[i] == ' ' && cmsg[i+1] == ' ')
            strshift(cmsg+i, -1, MAX_BUFSIZE);
        i++;
    }

    if (strcstr(cmsg, from) == NULL)
        return 0;

    if (!responser_clean_message(cmsg, from, cmsg)) return 0;
}

/* only used by get_security_answer function */
int read_number(const char *b) {
    if (b[0] <= '9' && b[0] >= '0') return atoi(b);
    else if (!strncmp(b, "eleven", 6)) return 11;
    else if (!strncmp(b, "twelve", 6)) return 12;
    else if (!strncmp(b, "thirteen", 8)) return 13;
    else if (!strncmp(b, "fourteen", 8)) return 14;
    else if (!strncmp(b, "fifteen", 7)) return 15;
    else if (!strncmp(b, "sixteen", 7)) return 16;
    else if (!strncmp(b, "seventeen", 9)) return 17;
    else if (!strncmp(b, "eighteen", 8)) return 18;
    else if (!strncmp(b, "nineteen", 8)) return 19;
    else if (!strncmp(b, "twenty", 6)) return 20;
    else if (!strncmp(b, "thirty", 6)) return 30;
    else if (!strncmp(b, "forty", 5)) return 40;
    else if (!strncmp(b, "fifty", 5)) return 50;
    else if (!strncmp(b, "sixty", 5)) return 60;
    else if (!strncmp(b, "seventy", 7)) return 70;
    else if (!strncmp(b, "eighty", 6)) return 80;
    else if (!strncmp(b, "ninety", 6)) return 90;
    else if (!strncmp(b, "zero", 3)) return 0;
    else if (!strncmp(b, "one", 3)) return 1;
    else if (!strncmp(b, "two", 3)) return 2;
    else if (!strncmp(b, "three", 5)) return 3;
    else if (!strncmp(b, "four", 4)) return 4;
    else if (!strncmp(b, "five", 4)) return 5;
    else if (!strncmp(b, "six", 3)) return 6;
    else if (!strncmp(b, "seven", 5)) return 7;
    else if (!strncmp(b, "eight", 5)) return 8;
    else if (!strncmp(b, "nine", 4)) return 9;
    else if (!strncmp(b, "ten", 3)) return 10;
    else if (!strncmp(b, "hundred", 7)) return 100;
    else if (!strncmp(b, "thousand", 8)) return 1000;
    else return -1;
}

/* try to answer security questions of specially nimbuzz, it's a guess
 * based on specified format, if format changes, this will not useful(should change in time)
 */
void get_security_answer(char *question, char *answer) {
    _message(MSG_DEBUG | MSG_SUB, "responser.get_security_answer( \"%s\" )", question);
    int n = 0, a = 0, l = strlen(question);

    while (question[a++] != '\n'); // skip first line

    // if '= ...' is in there
    if (strstr((char*) (question + a), "= ...") != NULL) {
        int x, y = 0;
        x = read_number((char*) (question + a));
        for (n = a; n < l; n++) {
            if (question[n] == '-' || question[n] == '+' || question[n] == '*') {
                y = read_number((question + n + 2));
                if (question[n] == '-') sprintf(answer, "%d", x - y);
                else if (question[n] == '+') sprintf(answer, "%d", x + y);
                else if (question[n] == '*') sprintf(answer, "%d", x * y);
                break;
            }
        }
    }        // What is ninety-five thousand nine hundred sixteen as digits?
    else if (strstr((char*) (question + a), "as digits") != NULL) {
        int result = 0, r[10] = {0}, p = 8, i = 0;
        do {
            r[i] = read_number(question + a + p);
            while (((char*) question + a)[p] && ((char*) question + a)[p] != '-' && ((char*) question + a)[p] != ' ') p++;
            if (((char*) question + a)[p] == 0) break;
            else p++;
        } while (r[i++] != -1 && i < 10);
        i--;
        for (p = 0; p < i; p++) {
            if (r[p + 1] == 1000) {
                result += r[p];
                result *= 1000;
                p++;
            } else if (r[p + 1] == 100) {
                result += r[p] * 100;
                p++;
            } else {
                result += r[p];
            }
        }
        sprintf(answer, "%d", result);
    }        // What is the 2nd digit of 99100?
    else if (strstr((char*) (question + a), "digit") != NULL) {
        int x, y;
        x = read_number((char*) (question + a + 12));
        sprintf(answer, "%c", question[a + 24 + x]);
    }        //Of the numbers fourteen, 29 and 16, which is the smallest?
        //Of the numbers sixty-six, seven, 35, sixty-six and 45, which is the smallest?
    else if (strstr((char*) (question + a), "Of the numbers") != NULL) {
        int result = 0, r[10] = {0}, p = 15, i = 0;
        do {
            r[i] = read_number(question + a + p);
            while (((char*) question + a)[p] && ((char*) question + a)[p] != '-' && ((char*) question + a)[p] != ' ') p++;
            if (((char*) question + a)[p] == 0) break;
            else if (((char*) question + a)[p] == '-') {
                p++;
                r[i] += read_number(question + a + p);
                while (((char*) question + a)[p] && ((char*) question + a)[p] != '-' && ((char*) question + a)[p] != ' ') p++;
                if (((char*) question + a)[p] != 0) p++;
            } else p++;

            if (((char*) question + a)[p] == 'a' && ((char*) question + a)[p + 1] == 'n' && ((char*) question + a)[p + 2] == 'd') p += 4;
        } while (r[i++] != -1 && i < 10);
        i--;

        if (strstr((char*) (question + a), "smallest") != NULL) { // find smallest
            result = r[0];
            for (p = 0; p < i; p++) if (r[p] < result) result = r[p];
        } else { // find biggest!
            for (p = 0; p < i; p++) if (r[p] > result) result = r[p];
        }
        sprintf(answer, "%d", result);
    }        // The 3rd number of 85, zero, ninety-one and 0 is?
    else if (strstr((char*) (question + a), "The ") != NULL && strstr((char*) (question + a), "number of ") != NULL) {
        int r[10] = {0}, p = 18, i = 1;
        r[0] = read_number(question + a + 4);
        do {
            r[i] = read_number(question + a + p);
            while (((char*) question + a)[p] && ((char*) question + a)[p] != '-' && ((char*) question + a)[p] != ' ') p++;
            if (((char*) question + a)[p] == 0) break;
            else if (((char*) question + a)[p] == '-') {
                p++;
                r[i] += read_number(question + a + p);
                while (((char*) question + a)[p] && ((char*) question + a)[p] != '-' && ((char*) question + a)[p] != ' ') p++;
                if (((char*) question + a)[p] != 0) p++;
            } else p++;

            if (((char*) question + a)[p] == 'a' && ((char*) question + a)[p + 1] == 'n' && ((char*) question + a)[p + 2] == 'd') p += 4;
        } while (r[i++] != -1 && i < 10);
        i--;
        sprintf(answer, "%d", r[r[0]]);
    }
}

