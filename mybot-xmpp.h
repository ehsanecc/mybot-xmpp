/* 
 * File:   mybot-xmpp.h
 * Author: ehsan
 *
 * Created on November 27, 2012, 10:54 AM
 */

#ifndef MYBOT_XMPP_H
#define	MYBOT_XMPP_H

#ifdef	__cplusplus
extern "C" {
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <pthread.h>
#include <stdarg.h>
#include <signal.h>
    
    
    // ANSI output text format
#define _TCBLACK       "\x1b[30m"
#define _TCRED         "\x1b[31m"
#define _TCGREEN       "\x1b[32m"
#define _TCYELLOW      "\x1b[33m"
#define _TCBLUE        "\x1b[34m"
#define _TCMAGENTA     "\x1b[35m"
#define _TCCYAN        "\x1b[36m"
#define _TCWHITE       "\x1b[37m"
    
#define _BCBLACK       "\x1b[40m"
#define _BCRED         "\x1b[41m"
#define _BCGREEN       "\x1b[42m"
#define _BCYELLOW      "\x1b[43m"
#define _BCBLUE        "\x1b[44m"
#define _BCMAGENTA     "\x1b[45m"
#define _BCCYAN        "\x1b[46m"
#define _BCWHITE       "\x1b[47m"
#define _BOLD          "\x1b[1m"
#define _BLINK         "\x1b[5m"
#define _RESET         "\x1b[0m"

// cursor control     
#define _CURSER_UP(u)   "\x1b["u"A"
#define _CURSER_DOWN(d) "\x1b["d"B"
#define _CURSER_ABS(x,y)"\x1b["x";"y"f"
#define _CURSER_ELEFT   "\x1b[100D"
#define _ERASESCREEN    "\x1b[2J"
#define _SAVE           "\x1b[s"
#define _RESTORE        "\x1b[u"


#define MSGTYPE         char
#define MSG_DEBUG       1
#define MSG_ERROR       2
#define MSG_WARNG       4
#define MSG_INFOR       8
#define MSG_MESSG       16
#define MSG_ATTEN       32
#define MSG_SUB         64
   

    char debug;
    
    int scr_x, scr_y;
    
    void _message(MSGTYPE msgtype, char *msg, ...);
    void _init_screen(char *user, char *room);

#ifdef	__cplusplus
}
#endif

#endif	/* MYBOT_XMPP_H */

