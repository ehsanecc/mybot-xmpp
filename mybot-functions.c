#include "mybot-xmpp.h"
#include "modules/datatypes.h"

// not-grouped functions, well collapse here {{
inline int _rand();
inline char *rpckup(char *a, char *b); // random pickup

void _message(MSGTYPE msgtype, char *msg, ...) {
    va_list va;
    va_start(va, msg);
    char mmsg[MAX_BUFSIZE]={0};
    
    switch(msgtype & (~MSG_SUB)) {
        case MSG_DEBUG:
            if(debug)
                sprintf(mmsg, _TCBLACK _BOLD "DD" _RESET" " _TCBLACK " %s" _RESET, msg);
            break;
        case MSG_ERROR:
            sprintf(mmsg, _TCRED _BOLD "EE" _RESET _TCRED " %s" _RESET, msg);
            break;
        case MSG_WARNG:
            sprintf(mmsg, _TCYELLOW _BOLD "WW" _RESET _TCYELLOW " %s" _RESET, msg);
            break;
        case MSG_INFOR:
            sprintf(mmsg, _TCGREEN _BOLD "II" _RESET _TCGREEN " %s" _RESET, msg);
            break;
        case MSG_ATTEN:
            sprintf(mmsg, _TCCYAN _BOLD "@@" _RESET _TCCYAN " %s" _RESET, msg);
            break;
        case MSG_MESSG:
            sprintf(mmsg, _BOLD "**" _RESET " %s", msg);
            break;
        default:
            ;
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
    FILE *fr = fopen("/dev/urandom", "rb");
    int r=0;
    r = fgetc(fr);
    fclose(fr);
    return r;
}

inline char *rpckup(char *a, char *b) // random pickup
{
    switch(_rand()%2) {
        case 0: return a; break;
        case 1: return b; break;
    }
}
/* ****************************************
 * try to find flooders, it's easy and not!
 * because of some flooders, some !
 * ****************************************/
int isflooder(char *msg, char *from, LIST_t *lUsers) {
    int flood = 0, len = strlen(msg), v = 0, w = 0;

    for (v = 0; v < len; v++) if (msg[v] == '\n') w++;
    if (w > 20 || len > 150) flood++;

    USER_t *u = mlist_get(lUsers, from, 0);
    if (u != NULL) {
        if (u->sLastSaid != NULL && !strcmp(u->sLastSaid, msg)) flood++;
        if (u->iActivity <= 2) flood++;
    }

    if (flood > 2) return 1; // possible flooder

    return 0;
}

// }}