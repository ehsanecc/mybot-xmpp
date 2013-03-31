#include "../mybot-xmpp.h"
#include "responser.h"
#include <pcre.h>

/* responser v2.0
 *
 * + Using the PCRE( Perl Compatible Regular Expression ).
 * + 
 */

int responser_init(char *responsefile/*[IN]*/) {
    if (global.system.debug) printf("initializing responses...\n");
    /* response file format:
     * !private          << make all qr's after it for private response
     * !public           << make all qr's after it for public response
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

    if (!rf) {
        return -1;
    }

    int num = 0, i = 0;

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
    int i;

    for (i = 0; i < __response_count; i++) {
        free(__qr[i].q);
        free(__qr[i].r);
    }
    free(__qr);
    __response_count = 0;

    return 0;
}

int responser_get(char *msg/*[IN]*/, bool to_us/*[IN]*/, char *response/*[OUT]*/) {
    int i, r=0;
    int ovector[30]={0};
    int addressed = (to_us == true ? PRIVAT_ADDRSSD : PUBLIC_ADDRSSD);

    for (i = 0; i < __response_count; i++) {
        if (__qr[i].addressed == addressed) {
            r = _pcre_compare(msg, __qr[i].q, ovector, 30);
            if (r >= 0) { // match
                if(__qr[i].response_count > 1) _get_substring(__qr[i].r, ((unsigned)_rand())%__qr[i].response_count, response);
                else strcpy(response, __qr[i].r);
                _pcre_replace(response, msg, ovector, r);
                return 1;
            }

        }
    }
    strcpy(response, ""); // no response

    return 0;
}

int _read_qr(FILE *file, QR_t *qr) {
    int n = 0, b = fgetc(file), r_count = 1;
    bool bstring=false;
    char q[MAX_BUFSIZE]={0}, r[MAX_BUFSIZE]={0};
    static int line = 0, addressed = PRIVAT_ADDRSSD; // by default, all messages are for private addressed

    if (feof(file)) return 0;
    
    switch (b) {
        case '"': bstring = true; break;
        case OPTION_FLAG:
            read_line(file, q, MAX_BUFSIZE); line++;
            if (!strcasecmp(q, "public")) addressed = PUBLIC_ADDRSSD;
            else if (!strcasecmp(q, "private")) addressed = PRIVAT_ADDRSSD;
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

    qr->q = strmalloc(q);
    qr->r = strmalloc(r);
    qr->addressed = addressed;
    qr->response_count = r_count;

    _message(MSG_DEBUG, "%s %s:%s[%d]\n", (qr->addressed == PRIVAT_ADDRSSD ? "private" : "public"), qr->q, qr->r, qr->response_count);

    return 1;
}

/* will separate and select a sub string in a multiple string { } */
void _get_substring(char *string, uint index, char *out) {
    _message(MSG_DEBUG, "responser._get_substring(%s, %u, [OUT])", string, index);
    uint i, p=0, len = strlen(string), string_length = 0;
    char *string_start = string;
    bool bstring = false;

    /* format: "str"|"str"|...
     * str as string!
     */
    for(i=0;i<=index;i++) {
        string_length = 0;
        do {
            if(string[p] == '\\') p++; // skip the next character
            else if(string[p] == '"') {
                bstring = !bstring;
                if(bstring) string_start = string+p+1; // start of a string
            } else string_length++;
            p++;
        } while(p < len && bstring);
        if(string[p] != '|' && string[p] != 0) _message(MSG_ERROR, "responser: unexpected character.");
        else p++;
    }
    
    sprintf(out, "%.*s", string_length, string_start);
}

int _pcre_compare(char *msg, char *regex, int *ovector, uint vectorsize) {
    pcre *p;
    const char *error;
    unsigned char tptr[32];
    int of, r;

    p = pcre_compile(regex, 0, &error, &of, tptr);
    if (p == NULL) {
        _message(MSG_ERROR, "responser._pcre error: %s", error);
        return -1;
    }
    r = pcre_exec(p, NULL, msg, strlen(msg), 0, 0, ovector, vectorsize);

    return r;
}

void _pcre_replace(char *str, char *msg, int *ovector, uint vectors) {
    uint i=0, n;
    char buf[MAX_BUFSIZE] = "";

    while(str[i])
    {
        if (str[i] == '$') {
            n = atoi(str+(++i));
            if(n <= vectors) {
                char *substring_start = msg + ovector[2 * n];
                uint substring_length = ovector[2 * n + 1] - ovector[2 * n];
                sprintf(buf, "%.*s%.*s%s", i-1, str, substring_length, substring_start, str + i + 1);
                strcpy(str, buf);
            }
        } i++;
    }
}

/* 
 * clear message from "from" string
 */
int clean_message(char *msg, char *from, char *cmsg) {
    if (strcasestr(msg, from) == NULL) {
        strcpy(cmsg, msg);
        return 0;
    }

    int ml = strlen(msg), fl = strlen(from), n, m, i = 0;

    n = abs((int) msg - (int) strcasestr(msg, from));
    for (i = 0; i < n; i++) cmsg[i] = msg[i];
    for (m = n + fl + 1; m < ml; m++) cmsg[i++] = msg[m];
    while (cmsg[0] == ' ') // strip string
    {
        ml = strlen(cmsg);
        for (n = 0; n < ml; n++) cmsg[n] = cmsg[n + 1]; // shift left
    }

    if (!clean_message(cmsg, from, cmsg)) return 0;
}

/* try to answer security questions of specially nimbuzz, it's a guess
 * based on specified format, if format changes, this will not useful(should change in time)
 */
void get_security_answer(char *question, char *answer) {
    int n = 0, a = 0, l = strlen(question);

    /*internal function*/
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

