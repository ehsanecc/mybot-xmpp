#include "../mybot-xmpp.h"
#include "responser.h"

/* responser v1.0 */

int responses_init(char *responsefile/*[IN]*/)
{
    if(debug) printf("initializing responses...\n");
    /* response file format:
     * 1:%c%s:"%s       < (exact/partial/intext match)question:response
     * 2:{%c%s|...}:%s  < {multiinput}:response
     * 3:%c%s:{%s|%s}   < question(could be multiple):{multiple output(pick random)}
     * 4:...
     * 5:.              < newline for end
     * for exact match use   '.'
     * for partial match use ','
     * for intext match use  '*'
     * for multiple          { }
     *  >for separating      '|'
     * string's should be in '"'
     */
    FILE *rf = fopen(responsefile, "rt");
    
    if(!rf) { return -1; }
    
    int num=0, i=0;
    
    while(!feof(rf)) if(fgetc(rf) == '\n') num++; // count lines( get the maximum possible QR's )
    rewind(rf); // back to start of file
    
    __qr = (QR_t*)malloc(sizeof(QR_t)*num);
    if(__qr == NULL) return 0;
    
    for(i=0;i<num && !feof(rf);i++)
        if(!_read_qr(rf, &__qr[i])) i--;
    __response_count = i;
    fclose(rf);
    
    return i;
}

int responses_clear()
{
    int i;
    
    for(i=0;i<__response_count;i++) {
        free(__qr[i].q);
        free(__qr[i].r);
    } free(__qr);
    __response_count = 0;
    
    return 0;
}

int responses_get(char *msg/*[IN]*/, bool to_us/*[IN]*/, char *response/*[OUT]*/)
{
    int i;
    int addressed = (to_us==true?PRIVAT_ADDRSSD:PUBLIC_ADDRSSD);
    
    for (i = 0; i < __response_count; i++) {
        if (__qr[i].addressed == addressed) {
            switch (__qr[i].match_type) {
                case EXACT_MATCH: if (!strcasecmp(msg, __qr[i].q)) { _get_response(response, &__qr[i]); return 1; } break;
                case INTEXT_MATCH: if (strcasestr(msg, __qr[i].q) != NULL) { _get_response(response, &__qr[i]); return 1; } break;
                case PARTIAL_MATCH: if (!strncasecmp(msg, __qr[i].q, strlen(__qr[i].q))) { _get_response(response, &__qr[i]); return 1; } break;
                case MULTIPLE_MATCH: if (!_multiplecmp(msg, __qr[i].q)) { _get_response(response, &__qr[i]); return 1; } break;
            }
        }
    }
    strcpy(response, ""); // no response
    
    return 0;
}

int _qparse(char *msg, char *q) {
    int i=0, n=0, type=0, l;
    char Q[MAX_BUFSIZE];
    
    // expected sample q is: {{."text1"|."text2"}&,"text3"}
    // or more complex
    
    while(q[i]) {
        if(q[i] == EXACT_MATCH)
            if (!strcasecmp(msg, q +1)) return 1;
        else if(q[i] == INTEXT_MATCH)
            if (strcasestr(msg,  q +1) != NULL) return 1;
        else if(q[i] == PARTIAL_MATCH)
            if (!strncasecmp(msg, q +1, strlen(q +1))) return 1;
        else if(q[i] == MULTIPLE_MATCH) {
            
        }
        
        i++; n=0;
    }
}

void _get_response(char *response, QR_t *qr) // low level get_response(mostly for handling multiple answers)
{
    if(qr->response_count > 1) {
        int n=_rand()%qr->response_count, i, p=1;
        for(i=0;i<n;i++) { while(qr->r[p++] != '|'); } i=0; // skip
        while(qr->r[p] && qr->r[p] != '|' && qr->r[p] != '}') response[i++] = qr->r[p++];
        response[i]=0;
    } else strcpy(response, qr->r);
}

int _multiplecmp(char *msg, char *q)
{
    int i=0, n=0, type=0, l;
    char buf[MAX_BUFSIZE];
    
    // {{.salam|.khodafez}&.bye}
    //  {.salam|.khodafez}&.bye}
    
    while(q[i] && q[i]!=':') {
        if(q[i]=='|' || q[i]=='}') // separate character
        {
            buf[n] = 0; n=0;
            type = buf[0];
            buf[MAX_BUFSIZE-1] = strlen(buf); // store length of buf somewhere safe!
            for(l=0;l<buf[MAX_BUFSIZE-1];l++) buf[l] = buf[l+1]; // shift left
            switch(type) {
                case EXACT_MATCH: if(!strcasecmp(msg, buf)) return 0; break;
                case INTEXT_MATCH: if(strcasestr(msg, buf) != NULL) return 0; break;
                case PARTIAL_MATCH: if(!strncasecmp(msg, buf, strlen(buf))) return 0; break;
                case MULTIPLE_MATCH: if(!_multiplecmp(msg, buf)) return 0; break;
            }
        } else buf[n++] = q[i];
        
        i++;
    }
    
    return 1;
}

int _read_qr(FILE *file, QR_t *qr)
{
    int n=0, b=fgetc(file), type, r_count = 1;
    char q[MAX_BUFSIZE], r[MAX_BUFSIZE];
    static int addressed = PRIVAT_ADDRSSD; // by default, all messages are for private addressed
    
    if(feof(file)) return 0;
    
    switch (b) {
        case '.': type = EXACT_MATCH; break;
        case ',': type = PARTIAL_MATCH; break;
        case '*': type = INTEXT_MATCH; break;
        case '{': type = MULTIPLE_MATCH; break;
        case '!':
            b = fgetc(file);
            while (b != '\n' && !feof(file)) { q[n++] = b; b = fgetc(file); } q[n] = 0; // read-line
            if (!strcasecmp(q, "public")) addressed = PUBLIC_ADDRSSD;
            else if (!strcasecmp(q, "private")) addressed = PRIVAT_ADDRSSD;
            return 0;
            break;
        case '\n': return 0; break;
        default: // skip a line then return
            while (fgetc(file) != '\n' && !feof(file));
            return 0;
    }

    b=fgetc(file);
    while(b!=':' && n < MAX_BUFSIZE && !feof(file)) {
        q[n++] = b;
        b=fgetc(file);
    } q[n] = 0;
    
    n=0, b=fgetc(file);
    while(b!='\n' && n < MAX_BUFSIZE && !feof(file)) {
        r[n++] = b;
        b=fgetc(file);
    } r[n] = 0;
    
    // count responses
    n = 0; if (r[0] == '{') while (r[n]) if(r[n++] == '|') r_count++;
    
    qr->q = malloc(strlen(q)+1); strcpy(qr->q, q);
    qr->r = malloc(strlen(r)+1); strcpy(qr->r, r);
    qr->addressed = addressed;
    qr->match_type = type;
    qr->response_count = r_count;
    
    if(debug) printf("%s %s:%s\n",(qr->addressed==PRIVAT_ADDRSSD?"private":"public"),qr->q,qr->r);
    
    return type;
}

/* clear message from "from" string
 */
int clean_message(char *msg, char *from, char *cmsg)
{
    if(strcasestr(msg, from) == NULL) { strcpy(cmsg,msg); return 0; }
    
    int ml = strlen(msg), fl = strlen(from), n, m, i=0;

    n = abs((int)msg - (int)strcasestr(msg, from));
    for(i=0;i<n;i++) cmsg[i] = msg[i];
    for(m=n+fl+1;m<ml;m++) cmsg[i++] = msg[m];
    while(cmsg[0]==' ') // strip string
    {
        ml = strlen(cmsg);
        for(n=0;n<ml;n++) cmsg[n] = cmsg[n+1]; // shift left
    }
    
    if(!clean_message(cmsg, from, cmsg)) return 0;
}

/* try to answer security questions of specially nimbuzz, it's a guess
 * based on specified format, if format changes, this will not useful
 */
void get_security_answer(char *question, char *answer)
{
    int n=0,a=0,l=strlen(question);
    
    /*internal function*/
    int read_number(const char *b) {
        if(b[0] <= '9' && b[0] >= '0') return atoi(b);
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
    
    while(question[a++]!='\n'); // skip first line
    
    // if '= ...' is in there
    if(strstr((char*)(question+a), "= ...") != NULL) {
        int x,y=0;
        x = read_number((char*) (question+a));
        for(n=a;n<l;n++) {
            if(question[n] == '-' || question[n] == '+' || question[n] == '*') {
                y = read_number((question+n+2));
                if(question[n] == '-') sprintf(answer, "%d", x-y);
                else if(question[n] == '+') sprintf(answer, "%d", x+y);
                else if(question[n] == '*') sprintf(answer, "%d", x*y);
                break;
            }
        }
    }
    // What is ninety-five thousand nine hundred sixteen as digits?
    else if(strstr((char*)(question+a), "as digits") != NULL) {
        int result = 0, r[10]={0}, p=8, i=0;
        do {
            r[i] = read_number(question + a + p);
            while (((char*) question + a)[p] && ((char*) question + a)[p] != '-' && ((char*) question + a)[p] != ' ') p++;
            if (((char*) question + a)[p] == 0) break;
            else p++;
        } while (r[i++] != -1 && i < 10); i--;
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
    }
    // What is the 2nd digit of 99100?
    else if(strstr((char*)(question+a), "digit") != NULL) {
        int x,y;
        x = read_number((char*)(question+a+12));
        sprintf(answer, "%c", question[a+24+x]);
    }
    //Of the numbers fourteen, 29 and 16, which is the smallest?
    //Of the numbers sixty-six, seven, 35, sixty-six and 45, which is the smallest?
    else if(strstr((char*)(question+a), "Of the numbers") != NULL) {
        int result=0, r[10]={0}, p=15, i=0;
        do {
            r[i] = read_number(question + a + p);
            while (((char*) question + a)[p] && ((char*) question + a)[p] != '-' && ((char*) question + a)[p] != ' ') p++;
            if (((char*) question + a)[p] == 0) break;
            else if (((char*) question + a)[p] == '-') {
                p++;
                r[i] += read_number(question + a + p);
                while (((char*) question + a)[p] && ((char*) question + a)[p] != '-' && ((char*) question + a)[p] != ' ') p++;
                if (((char*) question + a)[p] != 0) p++;
            }
            else p++;
            
            if (((char*) question + a)[p] == 'a' && ((char*) question + a)[p+1] == 'n' && ((char*) question + a)[p+2] == 'd') p+=4;
        } while (r[i++] != -1 && i < 10); i--;
        
        if(strstr((char*)(question+a), "smallest") != NULL) { // find smallest
            result = r[0];
            for(p=0;p<i;p++) if(r[p] < result) result = r[p];
        } else { // find biggest!
            for(p=0;p<i;p++) if(r[p] > result) result = r[p];
        }
        sprintf(answer, "%d", result);
    }
    // The 3rd number of 85, zero, ninety-one and 0 is?
    else if(strstr((char*)(question+a), "The ") != NULL && strstr((char*)(question+a), "number of ") != NULL) {
        int r[10]={0}, p=18, i=1;
        r[0] = read_number(question+a+4);
        do {
            r[i] = read_number(question + a + p);
            while (((char*) question + a)[p] && ((char*) question + a)[p] != '-' && ((char*) question + a)[p] != ' ') p++;
            if (((char*) question + a)[p] == 0) break;
            else if (((char*) question + a)[p] == '-') {
                p++;
                r[i] += read_number(question + a + p);
                while (((char*) question + a)[p] && ((char*) question + a)[p] != '-' && ((char*) question + a)[p] != ' ') p++;
                if (((char*) question + a)[p] != 0) p++;
            }
            else p++;
            
            if (((char*) question + a)[p] == 'a' && ((char*) question + a)[p+1] == 'n' && ((char*) question + a)[p+2] == 'd') p+=4;
        } while (r[i++] != -1 && i < 10); i--;
        sprintf(answer, "%d", r[r[0]]);
    }
}

