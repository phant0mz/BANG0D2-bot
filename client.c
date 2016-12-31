#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <netdb.h>
#include <sys/ioctl.h>
#include <arpa/inet.h>

#include <unistd.h>
#include <ctype.h>
#include <stdarg.h>
#include <pthread.h>

#include <errno.h>

#include <sys/types.h>
#include <sys/socket.h>

#include <fcntl.h>

#include "client.h"

#define EID_SHOWUSER            1001
#define EID_JOIN                1002
#define EID_LEAVE               1003
#define EID_WHISPER             1004
#define EID_TALK                1005
#define EID_BROADCAST           1006
#define EID_CHANNEL             1007
#define EID_USERFLAGS           1009
#define EID_WHISPERSENT         1010
#define EID_CHANNELFULL         1013
#define EID_CHANNELDOESNOTEXIST 1014
#define EID_CHANNELRESTRICTED   1015
#define EID_INFO                1018
#define EID_ERROR               1019
#define EID_EMOTE               1023
#define EID_UNIQUENAME          2010

#define BUFSIZE			2048

struct client {
	int s;

	void *dta;

	enum client_state state;
	enum client_error lasterror;

	struct msg_ctx {
		char *p;
		char *end;
		char buff[256];
	} msg;

	struct client_callbacks	cb;
};

int raw(client_t *ctx, char *fmt, ...) {
	char str[256];
	va_list argptr;

	va_start(argptr, fmt);
	ssize_t n = vsnprintf(str, 256, fmt, argptr);
	va_end(argptr);

	if (send(ctx->s, str, n, 0) < 0)
		return -1;

	return 0;
}

static const char *client_errstr[ERR_MAX] = {
"No error",
"Could not resolve hostname",
"Could not Bind",
"Socket error",
"Could not connect",
"Out of memory",
"Remote connection closed",
"Read error"
};

const char *client_err_str(client_t *ctx){
	if (ctx->lasterror >= ERR_MAX) {
		return "Unknown Error";
	}

	return client_errstr[ctx->lasterror];
}

client_t *client_init(struct client_callbacks *cb, void *dta){
	client_t *ctx = malloc(sizeof(*ctx));

	memset(ctx, 0, sizeof(*ctx));

	if (cb)
		ctx->cb = *cb;

	ctx->dta = dta;

	ctx->state = STATE_INIT;

	return ctx;
}

void make_nonblock(int s){
	int flags;
	/* Set socket to non-blocking */ 
	if ((flags = fcntl(s, F_GETFL, 0)) < 0) { 
	/* Handle error */ 
		perror("fcntl1");
	} 


	if (fcntl(s, F_SETFL, flags | O_NONBLOCK) < 0) {
		/* Handle error */
		perror("fcntl2");
	} 
}


void make_block(int s){
	int flags;
	/* Set socket to non-blocking */ 
	if ((flags = fcntl(s, F_GETFL, 0)) < 0) { 
	/* Handle error */ 
		perror("fcntl1");
	} 

	flags &= ~O_NONBLOCK;

	if (fcntl(s, F_SETFL, flags) < 0) {
		/* Handle error */
		perror("fcntl2");
	} 
}

int timed_connect(client_t *ctx, struct sockaddr *saddr, size_t size, struct timeval *tv){
	make_nonblock(ctx->s);
	int err;

	err = connect(ctx->s, saddr, size);

	//connected right away
	if (err == 0) {
		ctx->state = STATE_CONNECTED;
		return 1;
	}

	//connection not in progress
	if (err < 0 && errno != EINPROGRESS){
		ctx->lasterror = ERR_CONNECT;
		perror("connect");
		return -1;
	}

	//connetion in progress wait for a bit...
	fd_set fdw;
	FD_ZERO(&fdw);
	FD_SET(ctx->s,&fdw);

	err = select(ctx->s+1, NULL, &fdw, NULL, tv);

	make_block(ctx->s);

	if (err == 0) {
		perror("timeout");
		return 0;
	} else if (err == 1) {
		socklen_t errlen = sizeof(err);
		if (getsockopt(ctx->s, SOL_SOCKET, SO_ERROR, &err, &errlen)<0) {
			//perror("getsockopt");
			ctx->lasterror = ERR_CONNECT;
			return -1;
		}

		if (err != 0) {
			//fprintf(stderr, "error : %s\n", strerror(err));
			ctx->lasterror = ERR_CONNECT;
			return -1;
		}

		ctx->state = STATE_CONNECTED;
		return 1;
	}

//	perror("select");

	ctx->lasterror = ERR_CONNECT;
	return -1;
}

int client_connect(client_t *ctx, const char *server, unsigned int port, const char *bindaddr, int delay){
	struct sockaddr_in saddr;
	struct sockaddr_in caddr;

	//create socket
	memset( &saddr, 0, sizeof(saddr) );
	saddr.sin_family = AF_INET;
	saddr.sin_port = htons(port);
	saddr.sin_addr.s_addr = inet_addr(server);

	if (saddr.sin_addr.s_addr == INADDR_NONE) {
		int tmp_errno;
		struct hostent *hp;
		struct hostent tmp_hostent;
		char buf[2048];

	      	if ( gethostbyname_r(server, &tmp_hostent, buf, sizeof(buf), &hp, &tmp_errno) ) {
			ctx->lasterror = ERR_RESOLVE;
			goto ERROR;
		}

		memcpy(&saddr.sin_addr, hp->h_addr, (size_t) hp->h_length);
	}

	ctx->s = socket(AF_INET, SOCK_STREAM, 0);
	if (!ctx->s) {
		ctx->lasterror = ERR_SOCKET;
		goto ERROR;
	}

	//bind socket
	if (bindaddr) {
		memset( &caddr, 0, sizeof(caddr) );
		caddr.sin_family = AF_INET;
		caddr.sin_port = 0;
		caddr.sin_addr.s_addr = inet_addr(bindaddr);

		if (caddr.sin_addr.s_addr == INADDR_NONE) {
			int tmp_errno;
			struct hostent *hp;
			struct hostent tmp_hostent;
			char buf[2048];

		      	if (gethostbyname_r(bindaddr, &tmp_hostent, buf, sizeof(buf), &hp, &tmp_errno)) {
				ctx->lasterror = ERR_RESOLVE;
				goto ERROR;
			}

			memcpy(&caddr.sin_addr, hp->h_addr, (size_t) hp->h_length);
		}

		if (bind(ctx->s, (struct sockaddr *)&caddr, sizeof(caddr)) == -1) {
			ctx->lasterror = ERR_BIND;
			goto ERROR;
		}
	}

	// and connect to the server
	struct timeval tv = {.tv_sec=0, .tv_sec=delay*1000};
	return timed_connect(ctx, (struct sockaddr *)&saddr, sizeof(saddr), &tv);

ERROR:;
	if (ctx->s) {
		close(ctx->s);
		ctx->s = 0;
	}
	return -1;
}

void client_close(client_t *ctx){
	if (ctx->state == STATE_CONNECTED)
		close(ctx->s);

	ctx->state = STATE_CLOSED;
}

void client_destroy(client_t *ctx){
	free(ctx);
}






int ParseEvent(char *pszEvent, int *pnEventId, char *pszSpeaker, unsigned long *puFlags, char *pszEventText){
	char *p;

	if (!pszEvent || !pnEventId || !pszSpeaker || !puFlags || !pszEventText)
		return 1;

	*pszSpeaker = '\0';
	*pszEventText = '\0';
	*puFlags = 0;

	*pnEventId = atoi(pszEvent);

	/* some event messages have no speaker or flag fields */
	if (
		(*pnEventId != EID_INFO) && (*pnEventId != EID_CHANNELFULL) &&
		(*pnEventId != EID_CHANNEL) &&
		(*pnEventId != EID_CHANNELDOESNOTEXIST) &&
		(*pnEventId != EID_CHANNELRESTRICTED) && (*pnEventId != EID_ERROR)) {

		char szJunk[256];
		sscanf(pszEvent, "%d %s %s %lx", pnEventId, szJunk, pszSpeaker, puFlags);
	}

	/* the event text is enclosed in quotes */
	p = strchr(pszEvent, '"');
	if (p) {
		strncpy(pszEventText, p+1, 256);
		pszEventText[256-1] = '\0';

		/* nix the trailing quote */
		p = strrchr(pszEventText, '"');
		if (p)
			*p = '\0';
	}

	return 1;
}

static int dispatch(client_t *ctx, char *msg){
	int eventid;
	unsigned long uflags;
	char speaker[256] = "";
	char text[256] = "";
	if (!ParseEvent(msg, &eventid, speaker, &uflags, text)) {
		return 0;
	}

	/* dispatch to the appropriate event handler */
	switch (eventid) {
	case EID_SHOWUSER:
		if (ctx->cb.show_user)
			ctx->cb.show_user(ctx, ctx->dta, speaker, uflags, text);
		break;

	case EID_JOIN:
		if (ctx->cb.join)
			ctx->cb.join(ctx, ctx->dta, speaker, uflags, text);
		break;

	case EID_USERFLAGS:
		if (ctx->cb.user_flags)
			ctx->cb.user_flags(ctx, ctx->dta, speaker, uflags, text);
		break;

	case EID_LEAVE:
		if (ctx->cb.leave)
			ctx->cb.leave(ctx, ctx->dta, speaker, uflags, text);
		break;

	case EID_TALK:
		if (ctx->cb.talk)
			ctx->cb.talk(ctx, ctx->dta, speaker, uflags, text);
		break;

	case EID_BROADCAST:
		if (ctx->cb.broadcast)
			ctx->cb.broadcast(ctx, ctx->dta, speaker, uflags, text);
		break;

	case EID_CHANNEL:
		if (ctx->cb.channel)
			ctx->cb.channel(ctx, ctx->dta, speaker, uflags, text);
		break;

	case EID_WHISPER:
		if (ctx->cb.whisper)
			ctx->cb.whisper(ctx, ctx->dta, speaker, uflags, text);
		break;

	case EID_WHISPERSENT:
		if (ctx->cb.whisper_sent)
			ctx->cb.whisper_sent(ctx, ctx->dta, speaker, uflags, text);
		break;

	case EID_EMOTE:
		if (ctx->cb.emote)
			ctx->cb.emote(ctx, ctx->dta, speaker, uflags, text);
		break;

	case EID_CHANNELFULL:
		if (ctx->cb.channel_full)
			ctx->cb.channel_full(ctx, ctx->dta, speaker, uflags, text);
		break;

	case EID_CHANNELDOESNOTEXIST:
		if (ctx->cb.channel_not_exist)
			ctx->cb.channel_not_exist(ctx, ctx->dta, speaker, uflags, text);
		break;

	case EID_CHANNELRESTRICTED:
		if (ctx->cb.channel_restricted)
			ctx->cb.channel_restricted(ctx, ctx->dta, speaker, uflags, text);
		break;

	case EID_INFO:
		if (ctx->cb.info)
			ctx->cb.info(ctx, ctx->dta, speaker, uflags, text);
		break;

	case EID_ERROR:
		if (ctx->cb.error)
			ctx->cb.error(ctx, ctx->dta, speaker, uflags, text);
		break;

	case EID_UNIQUENAME:
		if (ctx->cb.unique_name)
			ctx->cb.unique_name(ctx, ctx->dta, speaker, uflags, text);
		break;

	default:
		return 1;
	}

	return 0;
}

static int get_msg(struct msg_ctx *ctx, client_t *pb, char *msg, int len){

	if (!ctx->p){
		ctx->p = &(ctx->buff[0]);
		ctx->end = &(ctx->buff[0]);
	}

	if(!ctx->end) {
		ctx->p = &(ctx->buff[0]);
		ctx->end = &(ctx->buff[0]);
	}

	if (len <= 128) {
		fprintf(stderr, "len too small\n");
		return 0;
	}

	int n;
	int xlen = 0;

	while(1){
		while (*(ctx->p)!='\n' && (ctx->p < ctx->end) && (xlen<len)) {
			*msg = *(ctx->p);
			ctx->p++;
			msg++;
			xlen++;
		}

		if (xlen >= len) {
			//really long message... just discard the rest
			do {
				n = recv(pb->s, ctx->buff, 128, 0);
				if (n<0) {
					return -1;
				} else if (n == 0) {
					ctx->end = &(ctx->buff[0]);
					ctx->p = &(ctx->buff[0]);
					return 0;
				}
				ctx->end = &(ctx->buff[n]);
				ctx->p = &(ctx->buff[0]);

				while (*(ctx->p)!='\n' && ctx->p<ctx->end ){
					ctx->p++;
				}

			} while(*(ctx->p)!='\n');

			ctx->p++;

			*msg = '\0';
			return xlen+1;
		}

		if (*ctx->p == '\n' ) {
			ctx->p++;
			*msg = '\0';
			return xlen+1;
		} else {
			n = recv(pb->s, ctx->buff, 128, 0);

			if (n<0) {
				return -1;
			} else if (n == 0) {
				ctx->end = &(ctx->buff[0]);
				ctx->p = &(ctx->buff[0]);
				return 0;
			}
			ctx->p = &(ctx->buff[0]);
			ctx->end = &(ctx->buff[n]);
		}
	}
}

int client_process(client_t *ctx, struct timeval *tm){
	fd_set rfds;
	int n;
	char msg[BUFSIZE];

	FD_ZERO(&rfds);
	FD_SET(ctx->s, &rfds);

	int err = select(ctx->s+1, &rfds, NULL, NULL, tm);

	if (err < 0) {
		ctx->lasterror = ERR_READ;
		return -1;
	} else if (err == 0) {
		return 0;
	}

	n = get_msg(&ctx->msg, ctx, msg, BUFSIZE);

	if (n==0) {
		ctx->lasterror = ERR_REMOTE;
		return -1;
	} else if(n<0) {
		ctx->lasterror = ERR_READ;
		return -1;
	}

//	fprintf(stderr, "msg %.*s\n", n-1, msg);

	if (isdigit(*msg)) {
		dispatch(ctx, msg);
	}

	return 1;
}

