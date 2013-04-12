/* 
 * File:   datatypes.h
 * Author: ehsan
 *
 * Created on December 7, 2012, 12:36 PM
 */

#ifndef DATATYPES_H
#define	DATATYPES_H

#ifdef	__cplusplus
extern "C" {
#endif
    
#include <time.h>
#include <sys/types.h>
    
    typedef enum {
        TSTRING,
        TUSER,
        TLIST,
        TBOOL,
        TINT,
        TAFFILATION,
        TNULL
    } DATATYPE;
    
    typedef enum {
        AFF_OWNER,
        AFF_ADMIN,
        AFF_MEMBER,
        AFF_OUTCAST,
        AFF_NONE
    } AFFILIATION;
    
    typedef enum {
        ROLE_MODERATOR,
        ROLE_NONE,
        ROLE_PARTICIPANT,
        ROLE_VISITOR
    } ROLE;

    typedef struct {
        int blocksize; // size
        void *block; // block address
    } RAW_t; // raw data structure
    
    typedef struct {
        char *name; // list name
        char **pKey; // every item COULD have a string key(like what we have in python dictionaries)
        void **pItem;
        int iCount;
        int iMax; // maximum list entries
        DATATYPE dtType;
    } LIST_t;
    typedef struct {
        RAW_t name;
        RAW_t Key;
        RAW_t pItem;
        int iCount;
        int iMax;
        DATATYPE dtType;
    } _LIST_t;          // used to store/restore

    typedef struct {
        char *sUsername;
        char *sLastSaid;
        LIST_t *lSaid; // may used, or not. to cache all messages of user
        LIST_t *lInfo; // some information cached from user
        AFFILIATION aAffiliation; // user affilation
        time_t tSeen; // seen since
        time_t tLastSaid; // last said time
        int bAvailable; // boolean
        int iActivity; // count of messages user sent to public
        int iAttention; // counts of users call us
    } USER_t;
    typedef struct {
        RAW_t sUsername;
        RAW_t sLastSaid;
        RAW_t lSaid; 
        RAW_t lInfo; 
        AFFILIATION aAffiliation;
        time_t tSeen; 
        time_t tLastSaid; 
        int bAvailable; 
        int iActivity; 
        int iAttention; 
    } _USER_t;       // used to store/restore

    typedef unsigned char bool;
#define MAX_BUFSIZE     8192    // 8KiB
#define MAX_JID         1024    // 256 * 4 (for unicode)
#define true               1
#define false              0

char *strmalloc(char *str); // allocate an string of str in ram
void strshift(char *str, int shifttoright, uint maxlength); // shift string to right(left)
    
LIST_t *mlist_init(int count, char *name, DATATYPE dtType);
int mlist_add(LIST_t *t, void *item, char *sKey);
void *mlist_get(LIST_t *t, char *sKey, int iIndex);
int mlist_find(LIST_t *t, char *str);
int mlist_expand(LIST_t *t, int iBlocks);  // expand the list to lists iMax + iBlocks
int mlist_get_size(LIST_t *t);
void _list_remove(LIST_t *t, int iIndex, char *sKey); // remove an ITEM from LIST
#define mlist_removei(list,index)   _list_remove(list, index, NULL); // index
#define mlist_removek(list,key)     _list_remove(list, -1, key);     // key
void mlist_raw_out(LIST_t *t, RAW_t *block/*[out]*/);
void mlist_raw_in(LIST_t *t, RAW_t *block/*[out]*/);
void mlist_free(LIST_t *t);

USER_t *user_init(char *username, AFFILIATION aAffiliation);
void user_free(USER_t *t);

AFFILIATION str2aff(char *str);
void aff2str(char *str, AFFILIATION aff);

#ifdef	__cplusplus
}
#endif

#endif	/* DATATYPES_H */

