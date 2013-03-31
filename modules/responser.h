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
    
#define OPTION_FLAG     '!'
#define COMMENT_FLAG    '#'
    
#define INSYSTEM_USE    '$'
#define PUBLIC_ADDRSSD  100 // public addressed, to everyone
#define PRIVAT_ADDRSSD  200 // private addressed, just to me(bot)
#define UNKNOWN         ~0  // unknown

    typedef struct {
        char *q; // question
        char *r; // response
        int response_count; // count of response
        int addressed; // to us, to a group
    } QR_t;
    
// define function & variables here
    QR_t *__qr;
    int __response_count;
    
    int responser_init(char *responsefile/*[IN]*/);
    int responser_clear();
    int responser_get(char *msg/*[IN]*/, bool to_us/*[IN]*/, char *response/*[OUT]*/);
    
    int _read_qr(FILE *file, QR_t *qr);
    int _pcre_compare(char *msg, char *regex, int *ovector, uint vectorsize);
    void _pcre_replace(char *str, char *msg, int *ovector, uint vectors);
    void _get_substring(char *string, uint index, char *out);
    
    int clean_message(char *msg, char *from, char *cmsg); // clean message from unneeded spaces, our ID(from)
    
    void get_security_answer(char *question, char *answer); // cuz it's responser, so it should response security questions too!


#ifdef	__cplusplus
}
#endif

#endif	/* RESPONSER_H */

