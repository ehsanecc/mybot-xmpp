#include "mybot-xmpp.h"
#include "datatypes.h"

void _message(MSGTYPE msgtype, char *msg, ...) {
    va_list va;
    va_start(va, msg);
    char mmsg[MAX_BUFSIZE]={0};
    
#ifndef __unix__
    /* windows command line does not support unicode characters,
     * so it should be filtered for windows */
    uint i, l = strlen(msg);
    for (i = 0; i < l; i++)
        if (!isprint(msg[i])) msg[i] = '?';
#endif
    
    switch(msgtype & (~MSG_SUB)) {
        case MSG_DEBUG:
            if(global.system.log_level == 4)
                sprintf(mmsg, _TCBLACK _BOLD "DD" _RESET" " _TCBLACK _BCWHITE " %s" _RESET, msg);
            break;
        case MSG_ERROR:
            sprintf(mmsg, _TCRED _BOLD "EE" _RESET _TCRED " %s" _RESET, msg);
            break;
        case MSG_WARNG:
            if(global.system.log_level >= 1)
                sprintf(mmsg, _TCYELLOW _BOLD "WW" _RESET _TCYELLOW " %s" _RESET, msg);
            break;
        case MSG_INFOR:
            if(global.system.log_level >= 3)
                sprintf(mmsg, _TCGREEN _BOLD "II" _RESET _TCGREEN " %s" _RESET, msg);
            break;
        case MSG_ATTEN:
            if(global.system.log_level >= 3)
                sprintf(mmsg, _TCCYAN _BOLD "@@" _RESET _TCCYAN " %s" _RESET, msg);
            break;
        case MSG_MESSG:
            if(global.system.log_level >= 2)
                sprintf(mmsg, _BOLD "**" _RESET " %s", msg);
            break;
    }
    
    if(strlen(mmsg) > 0) {
        if(msg[strlen(msg)-1] != '\n') mmsg[strlen(mmsg)] = '\n'; // add a newline if not have
        if (msgtype & MSG_SUB) {
            char buf[MAX_BUFSIZE];
            strcpy(buf, _TCWHITE "SUB ");
            strcat(buf, mmsg);
            strcpy(mmsg, buf);
        }
        vfprintf(stderr, mmsg, va);
    }
}

inline int _rand()
{
    int r=0;
    
#ifdef __unix__
    FILE *fr = fopen("/dev/urandom", "rb");
    last_return = fread(&r, sizeof(int), 1, fr);
    fclose(fr);
#else
    srand(time(NULL));
    r = rand();
#endif
    
    return r;
}

void log_unknown(char *msg) {
    FILE *f = fopen(global.config.unknownfile, "a+");
    fprintf(f, "\n>>\"%s\"<<", msg);
    fclose(f);
}

void read_line(FILE *fp, char *buf, int bufsize) {
    _message(MSG_DEBUG, "read_line(FILE*, char*, bufsize)");
    int p=0, b=0;
    
    do {
        b = fgetc(fp);
        if(buf != NULL)
            if(b != '\r' && b != '\n')
                buf[p++] = b;
        
    } while(b != '\n' && b != '\r' && !feof(fp) && p < bufsize);
    
    if(buf != NULL) {
        if(buf[p-1] == '\n') buf[p-1] = 0;
        else buf[p] = 0;
    }
}

inline void tolowers(char *string) {
    _message(MSG_DEBUG, "tolowers(\"%s\")", string);
    unsigned int n=0;
    
    if (string != NULL)
        while (string[n]) string[n] = tolower(string[n++]);
}

char *strcstr(char *string, char *needle) {
    _message(MSG_DEBUG, "strcstr(\"%s\", \"%s\")", string, needle);
    if(needle == NULL || string == NULL || strlen(string) == 0 || strlen(needle) == 0) return NULL;
    char *buf1 = strmalloc(string), *buf2 = strmalloc(needle), *ret = NULL;
    
    tolowers(buf1); tolowers(buf2);
    ret = strstr(buf1, buf2);
    if(ret != NULL) ret = string + (strlen(string) - strlen(ret));
    free(buf1); free(buf2);
    
    return ret;
}

/* getting pure JID */
inline char *pure_jid(char *jid) {
    _message(MSG_DEBUG, "pure_jid(\"%s\")", jid);
    char buf[MAX_JID];
    
    strcpy(buf, jid);
    buf[strlen(jid) - strlen(strchr(jid, '@'))] = 0;
    
    return strmalloc(buf);
}

bool isbotadmin(char *jid)
{
    _message(MSG_DEBUG, "isbotadmin(\"%s\")", jid);
    // the format of global.system.botadmins is: <pjid>;<pjid>;<pjid>;...
    char pjid[MAX_JID];
    uint p=0, q=0;
    
    if(jid == NULL || global.system.botadmins == NULL) return false;
    
    while(global.system.botadmins[p]) {
        while((global.system.botadmins[p] != ';') && (global.system.botadmins[p]))
            pjid[q++] = global.system.botadmins[p++];
        pjid[q] = 0; q=0;
        if(!strcmp(jid, pjid)) return true;
        else p++;
    }
    
    return false;
}

/* ****************************************
 * try to find flooders, it's easy and not!
 * because of some flooders, some !
 * ****************************************/
int isflooder(char *msg, char *from, LIST_t *lUsers) {
    _message(MSG_DEBUG, "isflooder(\"%s\", \"%s\", LIST_t*)", msg, from);
    int flood = 0, len = strlen(msg), v = 0, w = 0;

    for (v = 0; v < len; v++) if (msg[v] == '\n') w++;
    if (w > 20 || len > 250) flood++;

    USER_t *u = mlist_get(lUsers, from, 0);
    if (u != NULL) {
        if (u->sLastSaid != NULL && !strcmp(u->sLastSaid, msg)) flood++;
        if (u->iActivity <= 2) flood++;
        if ((u->tLastSaid - time(NULL)) < 5) flood++;
    }

    if (flood > 3) return 1; // possible flooder

    return 0;
}

// }}