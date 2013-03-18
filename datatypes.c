#include "datatypes.h"
#include "../mybot-xmpp.h"

LIST_t *mlist_init(int count, char *name, DATATYPE dtType) {
    int n;
    LIST_t *t = malloc(sizeof (LIST_t));

    _message(MSG_DEBUG | MSG_SUB, "datatype.list_init( %d, %s, %d );\n", count, name, dtType);
    t->name = strmalloc(name);

    t->iCount = 0;
    t->iMax = count;

    
    t->pKey = malloc(sizeof (void*) * (count+1));
    switch (dtType) {
        case TINT: // integer type
            t->pItem = malloc(sizeof (int) * (count + 1));
            for (n = 0; n < count; n++) t->pItem[n] = -1;
            break;
        case TBOOL: // boolean type
            t->pItem = malloc(sizeof (bool) * (count + 1));
            for (n = 0; n < count; n++) t->pItem[n] = false;
            break;
        default:
            t->pItem = malloc(sizeof (void*) * (count + 1));
            for (n = 0; n < count; n++) t->pItem[n] = NULL;
    }
    
    if(t->pKey == NULL || t->pItem == NULL) {
        printf("EE Can't allocate memory!\n");
        return NULL;
    } for (n = 0; n < count; n++) t->pKey[n] = NULL;

    t->dtType = dtType;

    return t;
}

void *mlist_get(LIST_t *t, char *sKey, int iIndex) {
    int i=0;
    
    _message(MSG_DEBUG | MSG_SUB, "datatype.list_get( %s, %s, %d );\n", t->name, sKey, iIndex);
    if (t != NULL) {
        if (sKey == NULL) {
            if (iIndex < t->iMax && iIndex > 0)
                return t->pItem[iIndex];
        } else if (strlen(sKey) > 0) {
            for (i = 0; i < t->iMax; i++)
                if (t->pKey[i] != NULL && !strcmp(t->pKey[i], sKey))
                    return t->pItem[i];
        }
    }
    
    _message(MSG_DEBUG | MSG_SUB, "datatype.list_get : return NULL\n", sKey, iIndex);
    
    return NULL;
}

int mlist_find(LIST_t *t, char *str) {
    int n;
    
    _message(MSG_DEBUG | MSG_SUB, "datatype.list_find( %s, %s );", t->name, str);
    if (str == NULL || t == NULL || strlen(str) == 0) {
        _message(MSG_DEBUG | MSG_SUB, "datatype.list_find : return -1");
        return -1;
    }
    
    for (n = 0; n < t->iMax; n++) {
        if (t->pKey[n] != NULL && strcasestr(t->pKey[n], str) != NULL)
            return n;
    }
    
    return -1;
}

int mlist_add(LIST_t *t, void *item, char *sKey) {
    int n,r=0,fn=-1;
    
    _message(MSG_DEBUG | MSG_SUB, "datatype.list_add( %s, ..., %s );", t->name, sKey);
    if (sKey == NULL || t == NULL || strlen(sKey) == 0) {
        _message(MSG_DEBUG | MSG_SUB, "datatype.list_add : return -1");
        return -1;
    }
    
    for (n = 0; n < t->iMax; n++) { // find repeat & first null
        if (t->pKey[n] != NULL) {
            if(!strcmp(t->pKey[n], sKey)) r++;
        } else if (fn == -1) fn = n;
    }

    if (!r) { // it's not in list
        char *key = strmalloc(sKey);
        if (fn != -1) { // there is a NULL in the list
            t->pKey[fn] = key;
            t->pItem[fn] = item;
            t->iCount++;
        } else { // there is no NULL in the list, so expand it!
            mlist_expand(t, 10); // expand the list for 10 blocks
            t->pItem[t->iCount] = item;
            t->pKey[t->iCount++] = key;
        }
        
        return 1;
    }
    
    _message(MSG_DEBUG | MSG_SUB, "datatype.list_add : return 0");
    return 0;
}

void _list_remove(LIST_t *t, int iIndex, char *sKey) {
    _message(MSG_DEBUG | MSG_SUB, "datatype.list_remove( %s, %d, %s );\n", t->name, iIndex, sKey);
    if (sKey == NULL) {
        if (iIndex < t->iMax) {
            switch (t->dtType) {
                case TUSER:
                    user_free(t->pItem[iIndex]);
                    free(t->pItem[iIndex]);
                    t->pItem[iIndex] = NULL;
                    break;
                case TINT:
                    t->pItem[iIndex] = 0;
                    break;
                case TBOOL:
                    t->pItem[iIndex] = 0;
                    break;
                default: free(t->pItem[iIndex]);
            }
            free(t->pKey[iIndex]); t->pKey[iIndex] = NULL;
            t->iCount--;
        }
    } else {
        int i;
        for (i = 0; i < t->iMax; i++)
            if (t->pKey[i] != NULL && !strcmp(t->pKey[i], sKey)) {
                switch (t->dtType) {
                    case TUSER:
                        user_free(t->pItem[i]);
                        free(t->pItem[i]);
                        t->pItem[i] = NULL;
                        break;
                    case TINT:
                        t->pItem[i] = 0;
                        break;
                    case TBOOL:
                        t->pItem[i] = 0;
                        break;
                    default: free(t->pItem[i]);
                }
                free(t->pKey[i]); t->pKey[i] = NULL;
                t->iCount--;
            }
    }
}

int mlist_expand(LIST_t *t, int iBlocks) {
    _message(MSG_DEBUG | MSG_SUB, "datatype.list_expand( %s, %d );\n", t->name, iBlocks);
    if(t!=NULL && iBlocks>0) {
        int n;
        
        void **tItem = malloc(sizeof(void**) * ( t->iMax + iBlocks )); // allocate new size
        for(n=0;n<( t->iMax + iBlocks );n++) tItem[n] = NULL;
        memcpy(tItem, t->pItem, sizeof(void**) * t->iMax);
        free(t->pItem);
        t->pItem = tItem;
        
        void **tKey = malloc(sizeof(void**) * ( t->iMax + iBlocks )); // allocate new size
        for(n=0;n<( t->iMax + iBlocks );n++) tKey[n] = NULL;
        memcpy(tKey, t->pKey, sizeof(void**) * t->iMax);
        free(t->pKey); // free previous memory
        t->pKey = tKey; // new address
        
        t->iMax += iBlocks;
        
        return t->iMax;
    }
    
    
    _message(MSG_DEBUG | MSG_SUB, "datatype.list_expand : return 0\n");
    return 0;
}

void mlist_free(LIST_t *t) {
    int n;
    
    if(t == NULL) return;
    _message(MSG_DEBUG | MSG_SUB, "datatype.list_free( %s );\n", t->name);
    for (n = 0; n < t->iMax; n++) {
        if (t->pKey[n] != NULL) {
            if (t->pItem[n] != NULL) { // free sub-item
                switch(t->dtType) {
                    case TUSER: user_free(t->pItem[n]); break;
                    default: free(t->pItem[n]);
                }
                t->pItem[n] = NULL;
            }
            free(t->pKey[n]);
            t->pKey[n] = NULL;
        }
    }
    free(t->pItem); t->pItem = NULL;
    free(t->pKey); t->pKey = NULL;
    free(t->name);
    free(t);
}

int mlist_get_size(LIST_t *t) {
    int size = 0;

    if (t != NULL)
        size = ((t->iMax * sizeof (void*))*2) + sizeof (LIST_t);

    _message(MSG_DEBUG | MSG_SUB, "datatype.list_get_size : return %d\n", size);
    return size;
}

void list_print(LIST_t *t) {
    int n = 0;
    USER_t *u;
    char buf[32];

    _message(MSG_DEBUG | MSG_SUB, "datatype.list_print( %s );\n", t->name);
    printf("> list %s\nI\tKEY\t\tVALUE\n", t->name);
    for (n = 0; n < t->iMax; n++) {
        if (t->pKey[n] != NULL) {
            switch (t->dtType) {
                case TSTRING: printf("%d\t%s\t%s\n", n, t->pKey[n], t->pItem[n]);
                    break;
                case TUSER:
                    u = t->pItem[n];
                    aff2str(buf, u->aAffiliation);
                    printf("%d\t%s\t{%s UN:'%s',AF:'%s',AC:%d,AT:%d,LS:'%s'}\n", n, t->pKey[n], (u->bAvailable?"In ":"Out"), u->sUsername, buf, u->iActivity, u->iAttention, u->sLastSaid);
                    break;
                default: printf("%d\t%s\tNULL\n", n, t->pKey[n]);
            }
        }
    }
}
/* these functions would save/load a list into/from a file, or sending/receiving it through a pipe, etc.
 * FORMAT:
 * BLOCK name;
 * BLOCK Key;
 * BLOCK pItem;
 * int iCount;
 * int iMax;
 * DATATYPE dtType;
 */
void mlist_raw_out(LIST_t *t, RAW_t *raw/*[out]*/) { // used to convert a list to raw

}

void mlist_raw_in(LIST_t *t, RAW_t *raw/*[in]*/) { // raw to list

}

USER_t *user_init(char *username, AFFILIATION aAffiliation) {
    USER_t *t = malloc(sizeof(USER_t));
    
    _message(MSG_DEBUG | MSG_SUB, "datatype.user_init( %s, %d );\n", username, aAffiliation);
    t->bAvailable = true;
    t->iAttention = 0;
    t->iActivity = 0;
    t->sLastSaid = NULL;
    t->lSaid = mlist_init(10, "user messages", TSTRING);
    t->lInfo = mlist_init(10, "user information", TSTRING);
    t->sUsername = strmalloc(username);
    t->aAffiliation = aAffiliation;
    t->tLastSaid = 0;
    t->tSeen = time(NULL);
    
    return t;
}

void user_free(USER_t *t) {
    _message(MSG_DEBUG | MSG_SUB, "datatype.user_free( %s );\n", t->sUsername);
    if(t == NULL) return;
    if (t->sLastSaid != NULL) {
        free(t->sLastSaid);
        t->sLastSaid = NULL;
    }
    if (t->sUsername != NULL) {
        free(t->sUsername);
        t->sUsername = NULL;
    }
    if (t->lSaid != NULL) {
        mlist_free(t->lSaid);
        t->lSaid = NULL;
    }
    if (t->lInfo != NULL) {
        mlist_free(t->lInfo);
        t->lInfo = NULL;
    }
    
}

char *strmalloc(char *str) {
    if (str != NULL) {
        char *r = malloc(strlen(str) + 2);
        strcpy(r, str);
        return r;
    } else {
        _message(MSG_DEBUG | MSG_SUB, "datatype.strmalloc( %s ): return NULL;\n", str);
        return NULL;
    }
}

AFFILIATION str2aff(char *str) {
    if(!strcmp(str, "owner")) return AFF_OWNER;
    else if(!strcmp(str, "admin")) return AFF_ADMIN;
    else if(!strcmp(str, "member")) return AFF_MEMBER;
    else if(!strcmp(str, "outcast")) return AFF_OUTCAST;
    else if(!strcmp(str, "none")) return AFF_NONE;
    else return -1;
}

void aff2str(char *str, AFFILIATION aff) {
    switch(aff) {
        case AFF_OWNER: strcpy(str, "owner"); break;
        case AFF_ADMIN: strcpy(str, "admin"); break;
        case AFF_MEMBER: strcpy(str, "member"); break;
        case AFF_OUTCAST: strcpy(str, "outcast"); break;
        case AFF_NONE:  strcpy(str, "none"); break;
        default: str[0] = 0;
    }
}