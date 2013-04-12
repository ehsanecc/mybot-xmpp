/* 
 * File:   responser.h
 * Author: ehsan
 *
 * Created on November 27, 2012, 10:58 AM
 */

#ifndef RESPONSER_H
#define	RESPONSER_H

#ifdef	__cplusplus
extern "C" {
#endif
    
#include "../datatypes.h"
#include <pcre.h>
    
#define OPTION_FLAG     '!'
#define COMMENT_FLAG    '#'
    
#define INSYSTEM_USE    '$'
    
#define FLG_PUBLIC      0x00000001 // public addressed, to everyone
#define FLG_PRIVATE     0x00000002 // private addressed, just to me(bot)
#define FLG_CASESENSE   0x00000004 // case sensitive
#define FLG_CASEINSEN   0x00000008 // case in-sensitive
#define FLG_WELCOME     0x10000000 // welcome strings
#define FLG_DEFAULT     (FLG_PRIVATE | FLG_CASEINSEN)
    
#define UNKNOWN         ~0  // unknown

    typedef struct {
        pcre *q; // question regex
        char *r; // response
        int response_count; // count of response
        int options; // options
    } QR_t;
    
// define function & variables here
    QR_t *__qr;
    int __response_count;
    char *__addressed_user; // addressed user
    
    int responser_init(char *responsefile/*[IN]*/);
    int responser_clear();
    int responser_get(char *msg/*in*/, uint options/*in*/, char *userid/*in*/, char *response/*out*/);
    void responser_userid(char *id); // set the addressed user we talking to
    
    int _read_qr(FILE *file, QR_t *qr);
    void _pcre_replace(char *str, char *msg, int *ovector, uint vectors);
    void _get_substring(char *string, uint index, char *out);
    
    int responser_clean_message(char *msg, char *from, char *cmsg); // clean message from unneeded spaces, our ID(from)
    
    void get_security_answer(char *question, char *answer); // cuz it's responser, so it should response security questions too!


#ifdef	__cplusplus
}
#endif

#endif	/* RESPONSER_H */

