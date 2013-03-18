#include "strophe.h"
#include "datatypes.h"
#include "mybot-xmpp.h"
#include "modules/responser.h"

/*
 * This project finally started!
 * since all features SHOULD be configurable thorugh config files,
 * it's really better to make it script base(in response of course)
 */

typedef struct {
    char *to;
    char *msg;
    xmpp_conn_t *conn;
    xmpp_ctx_t *ctx;
} msg_t;

#define IMMODERATOR           (global.status.affilation == AFF_OWNER || global.status.affilation == AFF_ADMIN)  // check if I'm moderator in room
#define ISMODERATOR(user)     ((user)->aAffiliation == AFF_OWNER || (user)->aAffiliation == AFF_ADMIN)          // check if He/She is moderator in room

LIST_t *lFlood, *lUsers;

void *loop_thread(void *ptr);

void mybot_init(xmpp_conn_t *conn, xmpp_ctx_t *ctx) {
    _message(MSG_DEBUG, "main.mybot_init(...);");
    global.status.room_joined = false;
    global.status.connected = false;
    global.status.wait = false;
    global.config.attackflooders = false;
    global.status.banned = false; // we think first that we are not banned
    
    fprintf(stderr, _ERASESCREEN _CURSER_ABS("0","0"));
    {
        pthread_t tt;
	msg_t *a = malloc(sizeof(msg_t));
	a->conn = conn; a->ctx = ctx; a->msg = " "; a->to = global.config.room;
	pthread_create(&tt, NULL, loop_thread, (void*)a);
    }
    if(global.config.responsefile) {
        int r = responses_init(global.config.responsefile);
        _message(MSG_INFOR, "%d responses loaded successfully.", r);
    }
    
    lFlood = mlist_init(30, "lFlood", TSTRING); // blocking list(for flooding)
    lUsers = mlist_init(30, "lUsers", TUSER); // users list
}

void xmpp_room_command(msg_t v, char *command) {
    _message(MSG_DEBUG, "main.xmpp_room_command(...);");
    if(!strcmp(command, "kick")) {
        xmpp_stanza_t *kick = xmpp_stanza_new(v.ctx);
        xmpp_stanza_set_name(kick, "iq");
        xmpp_stanza_set_attribute(kick, "to", global.config.room);
        xmpp_stanza_set_type(kick, "set");
        
        xmpp_stanza_t *query = xmpp_stanza_new(v.ctx);
        xmpp_stanza_set_name(query, "query");
        xmpp_stanza_set_ns(query, "http://jabber.org/protocol/muc#admin");
        
        xmpp_stanza_t *item = xmpp_stanza_new(v.ctx);
        xmpp_stanza_set_name(item, "item");
        xmpp_stanza_set_attribute(item, "nick", v.to);
        xmpp_stanza_set_attribute(item, "role", "none");
        
        if(v.msg!=NULL) { // reason
            xmpp_stanza_t *reason = xmpp_stanza_new(v.ctx);
            xmpp_stanza_set_name(reason, v.msg);
            
            xmpp_stanza_t *text = xmpp_stanza_new(v.ctx);
            xmpp_stanza_set_text(text, "Kicked.");
            
            xmpp_stanza_add_child(reason, text);
            xmpp_stanza_add_child(item, reason);
        }
        
        xmpp_stanza_add_child(query, item);
        xmpp_stanza_add_child(kick, query);
        
        xmpp_send(v.conn, kick);
        xmpp_stanza_release(kick);
        
    } else if(!strncmp(command, "affiliation:", 12) || !strncmp(command, "list:affiliation:", 17)) { // affiliation:<affiliation> : outcast, none, member, admin, owner
        xmpp_stanza_t *affiliation_set = xmpp_stanza_new(v.ctx);
        xmpp_stanza_set_name(affiliation_set, "iq");
        xmpp_stanza_set_attribute(affiliation_set, "to", global.config.room);
        xmpp_stanza_set_type(affiliation_set, "set");
        
        xmpp_stanza_t *query = xmpp_stanza_new(v.ctx);
        xmpp_stanza_set_name(query, "query");
        xmpp_stanza_set_ns(query, "http://jabber.org/protocol/muc#admin");
        
        // *********
        char jid[MAX_BUFSIZE];
        xmpp_stanza_t *item;
        
        if (!strncmp(command, "list:affiliation:", 17)) {
            LIST_t *litem = (LIST_t*) v.to; int n;
            for(n=0;n<litem->iCount;n++) {
                item = xmpp_stanza_new(v.ctx);
                sprintf(jid, "%s%s", (char*)(litem->pItem)[n], global.config.jid + strlen(global.config.pjid));
                xmpp_stanza_set_name(item, "item");
                xmpp_stanza_set_attribute(item, "jid", (char*) jid);
                xmpp_stanza_set_attribute(item, "affiliation", (char*)(command+17));
                xmpp_stanza_add_child(query, item);
            }
        } else {
            item = xmpp_stanza_new(v.ctx);
            sprintf(jid, "%s%s", v.to, global.config.jid + strlen(global.config.pjid));
            xmpp_stanza_set_name(item, "item");
            xmpp_stanza_set_attribute(item, "jid", (char*) jid);
            xmpp_stanza_set_attribute(item, "affiliation", (char*)(command+12));
            xmpp_stanza_add_child(query, item);
        }
        
        // ********
        xmpp_stanza_add_child(affiliation_set, query);
        
        xmpp_send(v.conn, affiliation_set);
        xmpp_stanza_release(affiliation_set);
    }
}

/* Every command received from a user should divide into [administrators] and [guest] commands
 * then in case of administrator commands, if the user requested the command is in the list of 
 * administrators, we have two modes:
 * 1. private response
 * 2. public response
 */
void exec_cmd(char *msg, char *from, char *out, msg_t v) {
    _message(MSG_DEBUG, "main.exec_cmd(%s, %s, ...);", msg, from);
    
    if (!strncmp(msg, "gettime", 7) || !strncmp(msg, "time", 4)) {
        time_t t;
        t = time(NULL);
        sprintf(out, "%s: %s", from, ctime(&t));

    } else if (!strncmp(msg, "info", 4)) {
        USER_t *t = mlist_get(lUsers, NULL, mlist_find(lUsers, msg + 5));
        if (t != NULL) {
            char stime[32], mtime[32], aff[32];
            strftime(stime, 32, "%H:%M:%S", localtime(&t->tLastSaid));
            strftime(mtime, 32, "%H:%M:%S", localtime(&t->tSeen));
            aff2str(aff, t->aAffiliation);
            sprintf(out, "info:\n{\n-user: %s (%s),\n-activity(%s): %d msgs,\n-interest: %d,\n-last_said(%s): \"%s\"\n}", t->sUsername, aff, mtime, t->iActivity, t->iAttention, stime, t->sLastSaid);
        }

    } else if (!strncasecmp(msg, "dice", 4)) // dice between 1to6
        sprintf(out, "*** %d", (_rand()%6)+1);
    else if (!strncasecmp(msg, "pick", 4)) { // pick a user randomly between all users
        unsigned char sel = _rand()%lUsers->iCount;
        while(mlist_get(lUsers, NULL, sel) == NULL || ((USER_t*)mlist_get(lUsers, NULL, sel))->bAvailable == false)
            sel = _rand()%lUsers->iCount; // roll until a valid user found( available )
        sprintf(out, "*** %s", ((USER_t*)mlist_get(lUsers, NULL, sel))->sUsername);
    } else if (!strncasecmp(msg, "wiki", 4)) { // wiki a word, phrase
        
        //sprintf(out, "*** %s", ((USER_t*)mlist_get(lUsers, NULL, sel))->sUsername);
    } else if (!strncasecmp(msg, "say", 3)) strcpy(out, (char*) (msg + 3));
    
    //else if (!strcmp(from, "pero_p") || !strcmp(from, "lorenzo_adin")) {
    else if (isbotadmin(from)) {

        if (IMMODERATOR) {
            if (!strncmp(msg, "kick", 4)) {
                msg_t p; p = v; p.to = (char*) (msg + 5); p.msg = NULL;
                xmpp_room_command(p, "kick");

            } else if (!strncmp(msg, "ban", 3)) {
                msg_t p; p = v; p.to = (char*) (msg + 4); p.msg = NULL;
                xmpp_room_command(p, "affiliation:outcast");

            } else if (!strncmp(msg, "unban", 5)) {
                msg_t p; p = v; p.to = (char*) (msg + 4); p.msg = NULL;
                xmpp_room_command(p, "affiliation:none");

            } else if (!strncmp(msg, "admin", 5)) {
                msg_t p; p = v; p.to = (char*) (msg + 6); p.msg = NULL;
                xmpp_room_command(p, "affiliation:admin");

            } else if (!strncmp(msg, "member", 6)) {
                msg_t p; p = v; p.to = (char*) (msg + 7); p.msg = NULL;
                xmpp_room_command(p, "affiliation:member");

            }
        }
        
        if (!strncmp(msg, "wlco", 4)) { // toggle say-welcome system
            global.config.welcome = !global.config.welcome;
            _message(MSG_WARNG, "welcome toggled by %s: %s", from, (global.config.welcome) ? "enabled" : "disabled");

        } else if (!strncmp(msg, "reload", 6)) {
            responses_clear();
            _message(MSG_INFOR, "%d responses loaded.", responses_init(global.config.responsefile));

        } else if (!strncmp(msg, "list", 4)) {
            list_print(lUsers);
            
        } else if (!strncmp(msg, "afld", 4)) {
            global.config.attackflooders = !global.config.attackflooders; // toggle anti-flood system
            _message(MSG_WARNG, "anti-flood toggled by %s: %s", from, global.config.attackflooders ? "enabled" : "disabled");
            
        } else if (!strncmp(msg, "kill", 4)) {
            // killing spree!
            exit(0);
        }
    }
}

void send_message(char *to, char *msg, char *type, xmpp_conn_t *conn, xmpp_ctx_t *ctx) {
    _message(MSG_DEBUG, "main.send_message(%s, %s, %s, ...);", to, msg, type);
    
    if (strlen(msg) > 180) { // split message
        int n = 0, i = 0, len = strlen(msg);
        char msg2[MAX_BUFSIZE];
        strcpy(msg2, msg);

        for (n = 0; n < len; n++) {
            if (msg[n] == '\n') {
                if (n < 180) i = n;
                else if (i > 0) {
                    msg2[i] = 0;
                    send_message(to, msg2, type, conn, ctx);
                    strcpy(msg2, msg + i); // other part
                    send_message(to, msg2, type, conn, ctx);
                    break;
                }
            }
        }
        return;
    }
    
    xmpp_stanza_t *reply = xmpp_stanza_new(ctx);
    xmpp_stanza_set_name(reply, "message");
    xmpp_stanza_set_type(reply, type);
    if(!strcmp(type, "groupchat")) xmpp_stanza_set_attribute(reply, "to", global.config.room);
    else xmpp_stanza_set_attribute(reply, "to", to);

    xmpp_stanza_t *body = xmpp_stanza_new(ctx);
    xmpp_stanza_set_name(body, "body");

    xmpp_stanza_t *text = xmpp_stanza_new(ctx);
    xmpp_stanza_set_text(text, msg);
    xmpp_stanza_add_child(body, text);
    xmpp_stanza_add_child(reply, body);

    xmpp_send(conn, reply);
    xmpp_stanza_release(reply);
}

void *loop_thread(void *ptr) {
    _message(MSG_DEBUG, "main.thread_send(...);");
    msg_t *a = (msg_t*)ptr;
    int delay, pl=0;
    
    while (1) {
        if (global.status.connected) {
            if (global.status.room_joined) {
                for (delay = 0; delay < 40 && global.status.room_joined == true; delay++) {
                    sleep(6);
                    if (global.config.attackflooders == true && (lFlood != NULL && (lFlood->iCount >= 4 || ((delay % 6) == 0) && lFlood->iCount > 0))) {
                        if (pl == lFlood->iCount) {
                            if (IMMODERATOR) { // if we can, so we BAN them
                                _message(MSG_INFOR, "THREAD: trying to BAN flooders....(%d in list)", lFlood->iCount);
                                msg_t p; p.to = lFlood; p.msg = NULL; p.conn = a->conn; p.ctx = a->ctx;
                                list_print(lFlood);
                                xmpp_room_command(p, "list:affiliation:outcast");
                                mlist_free(lFlood);
                                lFlood = mlist_init(30, "lFlood", TSTRING);
                                pl = 0;
                            } else { // otherwise, try to find an admin to report the flooders
                                int n, r = 0;
                                char report[MAX_BUFSIZE] = "Flooders detected report:", id[MAX_BUFSIZE];

                                for (n = 0; n < lFlood->iMax; n++) { // build the report first!
                                    if (mlist_get(lFlood, NULL, n) != NULL)
                                        sprintf(report, "%s\n%s@nimbuzz.com", report, mlist_get(lFlood, NULL, n));
                                }

                                for (n = 0; n < lUsers->iMax; n++) {
                                    if ((USER_t*) mlist_get(lUsers, NULL, n) != NULL && ISMODERATOR((USER_t*) mlist_get(lUsers, NULL, n))) {
                                        _message(MSG_INFOR, "THREAD: trying to REPORT flooders to %s....(%d in list)", ((USER_t*) mlist_get(lUsers, NULL, n))->sUsername, lFlood->iCount);
                                        sprintf(id, "%s/%s", global.config.room, ((USER_t*) mlist_get(lUsers, NULL, n))->sUsername);
                                        send_message(id, report, "chat", a->conn, a->ctx);
                                        r++; // reports
                                    }
                                }

                                if (r > 0) { // clear the list, because reported
                                    list_print(lFlood);
                                    mlist_free(lFlood);
                                    lFlood = mlist_init(30, "lFlood", TSTRING);
                                    pl = 0;
                                }
                            }
                        } else pl = lFlood->iCount;
                    }
                }
                send_message(a->to, a->msg, "groupchat", a->conn, a->ctx);
            } else if (global.status.banned == false && !global.status.wait && global.config.room != NULL) {
                _message(MSG_DEBUG, "looper->connect_to_room");
                connect_to_room(a->conn, a->ctx);
                global.status.wait = true;
            }
        }
        sleep(4);
    }
    free(ptr);
}

int version_handler(xmpp_conn_t * const conn, xmpp_stanza_t * const stanza, void * const userdata) {
    _message(MSG_DEBUG, "main.version_handler(...);");
    xmpp_stanza_t *reply, *query, *name, *version, *text;
    char *ns;
    xmpp_ctx_t *ctx = (xmpp_ctx_t*) userdata;
    _message(MSG_INFOR, "received version request from %s", xmpp_stanza_get_attribute(stanza, "from"));

    reply = xmpp_stanza_new(ctx);
    xmpp_stanza_set_name(reply, "iq");
    xmpp_stanza_set_type(reply, "result");
    xmpp_stanza_set_id(reply, xmpp_stanza_get_id(stanza));
    xmpp_stanza_set_attribute(reply, "to", xmpp_stanza_get_attribute(stanza, "from"));

    query = xmpp_stanza_new(ctx);
    xmpp_stanza_set_name(query, "query");
    ns = xmpp_stanza_get_ns(xmpp_stanza_get_children(stanza));
    if (ns) {
        xmpp_stanza_set_ns(query, ns);
    }

    name = xmpp_stanza_new(ctx);
    xmpp_stanza_set_name(name, "name");
    xmpp_stanza_add_child(query, name);

    text = xmpp_stanza_new(ctx);
    xmpp_stanza_set_text(text, "trying to make it more comfortable, more intelligent, hope i reach my goal.\n\npero.");
    xmpp_stanza_add_child(name, text);

    version = xmpp_stanza_new(ctx);
    xmpp_stanza_set_name(version, "version");
    xmpp_stanza_add_child(query, version);

    text = xmpp_stanza_new(ctx);
    xmpp_stanza_set_text(text, "0.9");
    xmpp_stanza_add_child(version, text);

    xmpp_stanza_add_child(reply, query);

    xmpp_send(conn, reply);
    xmpp_stanza_release(reply);
    
    return 1;
}

int presence_handler(xmpp_conn_t * const conn, xmpp_stanza_t * const stanza, void * const userdata) {
    _message(MSG_DEBUG, "main.presence_handler(...);");
    
    if (global.config.room != NULL && strstr(xmpp_stanza_get_attribute(stanza, "from"), global.config.room) != NULL) // in room
    {
        if (xmpp_stanza_get_type(stanza) != NULL && !strcmp(xmpp_stanza_get_type(stanza), "unavailable") && !strncmp(xmpp_stanza_get_attribute(stanza, "from"), global.config.pjid, strlen(global.config.pjid)) != NULL) // we lost our connection to room
        {
            /* probably kicked or banned */
            global.status.room_joined = false;
            global.status.wait = false;
            _message(MSG_WARNG, "connection lost.");
            
        } else if (xmpp_stanza_get_attribute(stanza, "from") != NULL && strcmp((char*) (xmpp_stanza_get_attribute(stanza, "from") + strlen(global.config.room) + 1), "admin")) { // not admin
            // will say welcome to new joined room mates!
            char *from = strchr(xmpp_stanza_get_attribute(stanza, "from"), '/') + 1;
            xmpp_stanza_t *item = (xmpp_stanza_get_child_by_name(stanza, "x") != NULL ? xmpp_stanza_get_child_by_name(xmpp_stanza_get_child_by_name(stanza, "x"), "item") : NULL);
            
            if (xmpp_stanza_get_type(stanza) == NULL && item != NULL && xmpp_stanza_get_child_by_name(item, "actor") == NULL) {
                char msg[MAX_BUFSIZE], id[MAX_BUFSIZE];
                if (xmpp_stanza_get_attribute(item, "jid") != NULL) {
                    strcpy(id, xmpp_stanza_get_attribute(item, "jid"));
                    id[strlen(id) - strlen(strchr(id, '@'))] = 0;
                }

                if (mlist_get(lFlood, id, -1) != NULL) return 1; // if flooder, skip
                if (strcmp(id, global.config.pjid)) { // not say hello to myself!
                    if (global.config.welcome) {
                        // welcome
                        sprintf(msg, rpckup("salam %s", rpckup(rpckup("khosh umadi %s", rpckup(rpckup("welcome %s", rpckup("kheili khosh umadi %s", "Wb %s")), "slm %s")), "dorood bar to %s")), id);
                        send_message(xmpp_stanza_get_attribute(stanza, "from"), msg, "groupchat", conn, (xmpp_ctx_t*) userdata);
                    }
                } else { // presence of us
                    if (!global.status.room_joined) {
                        global.status.room_joined = true;
                        _message(MSG_INFOR, "connection to room success.");
                        _message(MSG_INFOR, "our role is %s, affiliation is ", xmpp_stanza_get_attribute(item, "role"), xmpp_stanza_get_attribute(item, "affiliation"));
                        global.status.affilation = str2aff(xmpp_stanza_get_attribute(item, "affiliation"));
                    }
                }
                if (!mlist_add(lUsers, user_init(id, str2aff(xmpp_stanza_get_attribute(item, "affiliation"))), id)) {
                    USER_t *user = (USER_t*) mlist_get(lUsers, id, -1);
                    user->tSeen = time(NULL); // update join time
                    free(user->sLastSaid);
                    user->sLastSaid = NULL; // clear lastSaid
                    user->bAvailable = true; // it's available

                } else {
                    AFFILIATION aff = str2aff(xmpp_stanza_get_attribute(item, "affiliation"));
                    _message(MSG_INFOR, ">> %s%s %s joined our list!", (aff == AFF_OWNER || aff == AFF_ADMIN ? _TCCYAN : ""), (aff == AFF_OWNER || aff == AFF_ADMIN ? xmpp_stanza_get_attribute(item, "affiliation") : ""), id);
                }
            }
            if (xmpp_stanza_get_type(stanza) != NULL && !strcmp(xmpp_stanza_get_type(stanza), "unavailable")) {// user left the room
                if (mlist_get(lFlood, from, -1) == NULL) {
                    USER_t *user = ((USER_t*) mlist_get(lUsers, from, -1));
                    
                    if (user != NULL) {
                        user->bAvailable = false;
                        _message(MSG_INFOR, "<< %s left the room!", from);
                    } else _message(MSG_ERROR, "can't find user %s in list!", from);
                    
                    if(!strcmp(from, global.config.pjid)) // it's me!
                        global.status.room_joined = false;
                }
            }
            
            if (item != NULL && xmpp_stanza_get_child_by_name(item, "actor") != NULL) { // affiliation change
                char *affiliation = xmpp_stanza_get_attribute(item, "affiliation");
                char *role = xmpp_stanza_get_attribute(item, "role");
                char *actor = xmpp_stanza_get_attribute(xmpp_stanza_get_child_by_name(item, "actor"), "jid");
                
                if(!strcmp(affiliation, "outcast")) {
                    _message(MSG_ATTEN, "%s banned by %s!", from, actor);
                    global.status.banned = true;
                }
                else if(!strcmp(affiliation, "none")) _message(MSG_ATTEN, "%s kicked by %s!", from, actor);
                else _message(MSG_ATTEN, "%s is now (affiliation:%s & role:%s) by %s!", from, affiliation, role, actor);
                
                // update user list
                USER_t *user = mlist_get(lUsers, from, 0);
                if(user!=NULL) user->aAffiliation = str2aff(affiliation);
            }
        }
    }
    if(xmpp_stanza_get_type(stanza) != NULL && !strcmp(xmpp_stanza_get_type(stanza),"error")) {
        global.status.room_joined = false;
        global.status.wait = false;
        _message(MSG_WARNG, "connection lost.");
    }
    
    if(xmpp_stanza_get_type(stanza) != NULL && !strcmp(xmpp_stanza_get_type(stanza),"subscribe")) // friend request
    {
        xmpp_stanza_t *pres = xmpp_stanza_new((xmpp_ctx_t*)userdata);
        
        _message(MSG_INFOR, "friendship request from '%s'. %s.", xmpp_stanza_get_attribute(stanza, "from"), (global.config.acceptfriendship?"accepted":"rejected"));
        xmpp_stanza_set_name(pres, "presence"); // presence
        xmpp_stanza_set_attribute(pres, "to", xmpp_stanza_get_attribute(stanza, "from"));
        if(global.config.acceptfriendship)
            xmpp_stanza_set_type(pres, "subscribed"); // approval
        else 
            xmpp_stanza_set_type(pres, "unsubscribed"); // reject
        
        xmpp_send(conn, pres);
        xmpp_stanza_release(pres);
    }

    return 1;
}

int groupchat_message_handler(xmpp_conn_t * const conn, xmpp_stanza_t * const stanza, void * const userdata) {
    _message(MSG_DEBUG, "main.groupchat_message_handler(...);");
    char *intext, replytext[MAX_BUFSIZE] = {0}, *id;
    xmpp_ctx_t *ctx = (xmpp_ctx_t*) userdata;
    
    if (!xmpp_stanza_get_child_by_name(stanza, "body")) return 1;
    if (!strcmp(xmpp_stanza_get_attribute(stanza, "type"), "error")) return 1;
    
    intext = xmpp_stanza_get_text(xmpp_stanza_get_child_by_name(stanza, "body"));
    if (intext == NULL || strlen(intext) == 0) return 1;
    
    id = (char*) (xmpp_stanza_get_attribute(stanza, "from") + strlen(global.config.room) + 1);
    
    if (mlist_get(lFlood, id, 0) != NULL) // it's flooder!
        return 1;
    
    else {
        char stime[32];
        time_t tt = time(NULL);
        USER_t *user = mlist_get(lUsers, id, 0);
        strftime(stime, 32, "%H:%M:%S", localtime(&tt));
        _message(MSG_MESSG, "%s%s[%s]%s>"_RESET"%s", ((strstr(intext, global.config.pjid) != NULL) ? _BOLD _TCYELLOW : _TCYELLOW), ((user != NULL) && (user->aAffiliation == AFF_ADMIN || user->aAffiliation == AFF_OWNER) ? _TCRED : ""), stime, (strchr(xmpp_stanza_get_attribute(stanza, "from"), '/') != NULL) ? strchr(xmpp_stanza_get_attribute(stanza, "from"), '/') + 1 : "(null)", intext);
    }
    // if admin says "Enter the right answer to start chatting" then we should answer:
    if (!strcmp(id, "admin") && strstr(intext, "Enter the right answer to start chatting") != NULL) {
        char guess[32] = "none";
        get_security_answer(intext, guess);
        if (global.config.securitypass) {
            _message(MSG_MESSG, "auto-answer: %s", guess);
            strcpy(replytext, guess);
        } else {
            _message(MSG_MESSG, "<(%s) :", guess);
            scanf("%s", replytext);
        }
    } else {
        if (strcmp(id, global.config.pjid)) { // if not my message loopback to me, then
            USER_t* user = mlist_get(lUsers, id, 0);
            bool repeat = (user != NULL && user->sLastSaid != NULL && !strcmp(user->sLastSaid, intext)) ? true : false;

            if (user != NULL) {
                if (!repeat) { // not repeated message
                    user->iActivity++;
                    if (user->sLastSaid != NULL) free(user->sLastSaid);
                    user->sLastSaid = strmalloc(intext);
                    user->tLastSaid = time(NULL);
                }
            }

            if (intext[0] == ':') { // if it's command
                msg_t t;
                t.conn = conn;
                t.ctx = ctx;
                exec_cmd((char*) (intext + 1), id, replytext, t);

            } else if (user != NULL && isflooder(intext, id, lUsers)) {
                _message(MSG_WARNG, "possible flooding detected.");
                char *i = strmalloc(id);
                if (!mlist_add(lFlood, i, id)) free(i); // add it to flood list
                mlist_removek(lUsers, id); // remove it from users list
                _message(MSG_INFOR, "currently %d flooders in our list.", lFlood->iCount);

            } else if (!repeat) { // not repeated message
                char msg[MAX_BUFSIZE] = "";
                clean_message(intext, global.config.pjid, msg);
                if (strcasestr(intext, global.config.pjid) != NULL || lUsers->iCount == 1) { // talking to us
                    if(user != NULL) user->iAttention++;
                    if (responses_get(msg, true, msg)) {
                        if (strlen(msg) > 0) sprintf(replytext, "%s: %s", id, msg);
                    } else {
                        _message(MSG_INFOR, "unknown message detected from %s: %s", id, (strlen(intext) < 64) ? intext : "[TOO LARGE]");
                        if (global.config.unknownfile != NULL) {
                            FILE *f = fopen(global.config.unknownfile, "a+");
                            clean_message(intext, global.config.pjid, msg);
                            fprintf(f, "\n%s", msg);
                            fclose(f);
                        }
                    }
                } else { // not talking to us
                    responses_get(msg, false, msg);
                    if (strlen(msg) > 0) sprintf(replytext, "%s: %s", id, msg);
                }
            }
        }
    }
    
    if (strlen(replytext) > 0)
        send_message(xmpp_stanza_get_attribute(stanza, "from"), replytext, xmpp_stanza_get_type(stanza), conn, (xmpp_ctx_t*) userdata);

    return 1;
}

int chat_message_handler(xmpp_conn_t * const conn, xmpp_stanza_t * const stanza, void * const userdata) {
    _message(MSG_DEBUG, "main.chat_message_handler(...);");
    char *intext, replytext[MAX_BUFSIZE] = {0}, *id;
    xmpp_ctx_t *ctx = (xmpp_ctx_t*) userdata;
    
    if (!xmpp_stanza_get_child_by_name(stanza, "body")) return 1;
    if (!strcmp(xmpp_stanza_get_attribute(stanza, "type"), "error")) return 1;
    
    intext = xmpp_stanza_get_text(xmpp_stanza_get_child_by_name(stanza, "body"));
    if (intext == NULL || strlen(intext) == 0) return 1;
    
    {
        char stime[32];
        time_t tt = time(NULL);
        USER_t *user = mlist_get(lUsers, id, 0);
        strftime(stime, 32, "%H:%M:%S", localtime(&tt));
        _message(MSG_MESSG, _BOLD _TCYELLOW "%s[%s]%s>"_RESET"%s", ((user != NULL) && (user->aAffiliation == AFF_ADMIN || user->aAffiliation == AFF_OWNER) ? _TCRED : ""), stime, xmpp_stanza_get_attribute(stanza, "from"), intext);
    }
    
    if (intext[0] == ':') { // command
        msg_t t; t.conn = conn; t.ctx = ctx;
        exec_cmd((char*) (intext + 1), xmpp_stanza_get_attribute(stanza, "from"), replytext, t);
    } else responses_get(intext, true, replytext);
    _message(MSG_MESSG, "response: %s", replytext);
    
    if (strlen(replytext) > 0)
        send_message(xmpp_stanza_get_attribute(stanza, "from"), replytext, xmpp_stanza_get_type(stanza), conn, (xmpp_ctx_t*) userdata);

    return 1;
}

void connect_to_room(xmpp_conn_t * const conn, xmpp_ctx_t *ctx) {
    _message(MSG_DEBUG, "main.connect_to_room(...);");
    xmpp_stanza_t* pres;
    
    _message(MSG_INFOR, "connecting to room %s", global.config.room);
    char *rroom = malloc(strlen(global.config.room) + strlen(global.config.pjid) + 4);
    strcpy(rroom, global.config.room); strcat(rroom, "/"); strcat(rroom, global.config.pjid);
    pres = xmpp_stanza_new(ctx);
    xmpp_stanza_set_name(pres, "presence");
    xmpp_stanza_set_attribute(pres, "to", rroom);
    xmpp_send(conn, pres);
    xmpp_stanza_release(pres);
    free(rroom);
}
/* define a handler for connection events */
void conn_handler(xmpp_conn_t * const conn, const xmpp_conn_event_t status, const int error, xmpp_stream_error_t * const stream_error, void * const userdata) {
    _message(MSG_DEBUG, "main.conn_handler(...);");
    xmpp_ctx_t *ctx = (xmpp_ctx_t *) userdata;

    if (status == XMPP_CONN_CONNECT) {
        xmpp_stanza_t* pres;
        _message(MSG_DEBUG, "connected");
        global.status.connected = true;
        xmpp_handler_add(conn, version_handler, "jabber:iq:version", "iq", NULL, ctx);
        
        xmpp_handler_add(conn, groupchat_message_handler, NULL, "message", "groupchat", ctx);
        xmpp_handler_add(conn, chat_message_handler, NULL, "message", "chat", ctx);
        
        xmpp_handler_add(conn, presence_handler, NULL, "presence", NULL, ctx);

        /* Send initial <presence/> so that we appear online to contacts */
        pres = xmpp_stanza_new(ctx);
        xmpp_stanza_set_name(pres, "presence");
        xmpp_send(conn, pres);
        _message(MSG_DEBUG, "%s", xmpp_stanza_get_type(pres));
        xmpp_stanza_release(pres);

        // try to make a connection to room
        // connect_to_room(conn, ctx);
    } else {
        global.status.connected = false;
        _message(MSG_DEBUG, "disconnected");
        xmpp_stop(ctx);
    }
}

#ifdef __unix__
void sigterm() { // termination process
    _message(MSG_WARNG, "\nTermination process...");
    // termination process {{
    // }}
    exit(0);
}
#endif

int main(int argc, char **argv)
{
    xmpp_ctx_t *ctx;
    xmpp_conn_t *conn;
    xmpp_log_t *log;
    char *jid, *pass;

    /* take a JID and password on the command line */
    if (argc < 3) {
        fprintf(stderr, 
                "Usage: mybot-xmpp <jid> <pass> [[--room | -r] <room-to-join>] [--accept | -a] [--response <responsefile:in>] [[--unknown | -u] <unknownquestionsfile:out>] [-d | --debug] [--security-answer | -s]\n\n"\
                "  --room -r\t\tRoom to join\n"\
                "  --config -c\t\tConfiguration file to load\n"\
                "  --admins \t\tBot admins pjid's, separated with ';' character\n"\
                "  --accept -a\t\tAccept friendship request\n"\
                "  --response\t\tRead responses from file\n"\
                "  --unknown \t\tWrite unknown(not in response list) questions in file\n"\
                "  --debug -d\t\tEnable debug mode\n"\
                "  --security-answer\tAnswer automatically to security questions\n");
	return 1;
    }
    
#ifdef __unix__
    signal(SIGINT, sigterm); // register signal for answer on exit(ctrl+c)
#endif
    
    // initial values
    jid = argv[1];
    pass = argv[2];
    global.system.debug=0;
    global.config.jid = jid;
    global.config.room = NULL;
    global.config.responsefile = NULL;
    global.config.unknownfile = NULL;
    global.config.acceptfriendship = false;
    
    {
        /* read command line */
        int v;
        for(v=3;v<argc;v++) {
            if(!strcmp(argv[v], "-r") || !strcmp(argv[v], "--room")) global.config.room = argv[++v]; // room to join
            else if(!strcmp(argv[v], "--admins")) global.system.botadmins = argv[++v]; // bot administrators
            else if(!strcmp(argv[v], "--response")) global.config.responsefile = argv[++v]; // response file
            else if(!strcmp(argv[v], "-a") || !strcmp(argv[v], "--accept")) global.config.acceptfriendship = true; // accept friendrequest
            else if(!strcmp(argv[v], "-c") || !strcmp(argv[v], "--config")) global.system.config_file = argv[++v]; // configuration
            else if(!strcmp(argv[v], "-u") || !strcmp(argv[v], "--unknown")) global.config.unknownfile = argv[++v]; // unknown questions file
            else if(!strcmp(argv[v], "-d") || !strcmp(argv[v], "--debug")) global.system.debug = 1; // debug
            else if(!strcmp(argv[v], "-s") || !strcmp(argv[v], "--security-answer")) global.config.securitypass = 1;
        }
        
        global.config.pjid = pure_jid(jid);
    }
    
    /* load configuration file if any */

    /* init library */
    xmpp_initialize();

    /* create a context */
    if(global.system.debug) log = xmpp_get_default_logger(XMPP_LEVEL_DEBUG); /* pass NULL instead to silence output */
    else log = xmpp_get_default_logger(XMPP_LEVEL_WARN);
    ctx = xmpp_ctx_new(NULL, log);

    /* create a connection */
    conn = xmpp_conn_new(ctx);

    /* setup authentication information */
    xmpp_conn_set_jid(conn, jid);
    xmpp_conn_set_pass(conn, pass);

    /* initiate connection */
    xmpp_connect_client(conn, NULL, 0, conn_handler, ctx);
    
    /* initiate bot */
    mybot_init(conn, ctx);
    
    /* enter the event loop - 
       our connect handler will trigger an exit */
    xmpp_run(ctx);

    /* release our connection and context */
    xmpp_conn_release(conn);
    xmpp_ctx_free(ctx);

    /* final shutdown of the library */
    xmpp_shutdown();

    return 0;
}
