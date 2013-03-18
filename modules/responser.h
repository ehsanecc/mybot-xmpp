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
    
#define EXACT_MATCH     '.'
#define INTEXT_MATCH    '*'
#define PARTIAL_MATCH   ','
#define MULTIPLE_MATCH  '{'
#define MULTIPLE_OR     '|'
#define MULTIPLE_AND    '&'
#define INSYSTEM_USE    '$'
#define PUBLIC_ADDRSSD  100 // public addressed, to everyone
#define PRIVAT_ADDRSSD  200 // private addressed, just to me(bot)
#define UNKNOWN         ~0  // unknown
    
    typedef struct {
        char *q; // question
        char *r; // response
        int response_count; // count of response
        int match_type; // exact, partial, intext, multiple
        int addressed; // to us, to a group
    } QR_t;
    
// define function & variables here
    QR_t *__qr;
    int __response_count;
    
    int responses_init(char *responsefile/*[IN]*/);
    int responses_clear();
    int responses_get(char *msg/*[IN]*/, bool to_us/*[IN]*/, char *response/*[OUT]*/);
    
    // private functions(i wish i programmed this in C++ and class based)
    void _get_response(char *response, QR_t *qr);
    int _read_qr(FILE *file, QR_t *qr);
    int _multiplecmp(char *a, char *b);
    
    int clean_message(char *msg, char *from, char *cmsg); // clean message from unneeded spaces, our ID(from)
    
    void get_security_answer(char *question, char *answer); // cuz it's responser, so it should response security questions too!


#ifdef	__cplusplus
}
#endif

#endif	/* RESPONSER_H */

