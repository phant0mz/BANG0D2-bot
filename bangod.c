#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <ctype.h>
#include <unistd.h>
#include <argp.h>

#include "client.h"
#include "bangod.h"

/*
Remember to lock shared data...

pthread_rwlock_wrlock(&g.datalock);
//write
pthread_rwlock_unlock(&g.datalock);

pthread_rwlock_rdlock(&g.datalock);
//read
pthread_rwlock_unlock(&g.datalock);
*/
struct globals {
	pthread_rwlock_t datalock;
	struct master *master;
	struct ban *ban;
	char *userfile;
	char *banfile;
	int issaved;
} g;

void save(void){
	pthread_rwlock_wrlock(&g.datalock);

	if (g.issaved==1)
		goto RETURN;

	FILE *fp = fopen(g.banfile, "w");

	if (!fp)
		goto RETURN;

	struct ban *b = g.ban;
	while(b){
		fprintf(fp, "%s\n", b->needle);

		b = b->next;
	}

	fclose(fp);

	fp = fopen(g.userfile, "w");

	if (!fp)
		goto RETURN;

	struct master *m = g.master;
	while(m){
		fprintf(fp, "%s\n", m->name);

		m = m->next;
	}

	fclose(fp);

	g.issaved = 1;

RETURN:;
	pthread_rwlock_unlock(&g.datalock);
}

void add_master(const char *str){
	pthread_rwlock_wrlock(&g.datalock);

	struct master **m = &g.master;

	while (*m) {

		if (strcmp((*m)->name, str) == 0) {
			pthread_rwlock_unlock(&g.datalock);
			return;
		}

		m = &(*m)->next;
	}

	*m = malloc(sizeof(**m));
	(*m)->next = NULL;
	(*m)->name = strdup(str);

	g.issaved = 0;

	pthread_rwlock_unlock(&g.datalock);
}

void rem_master(const char *str){
	pthread_rwlock_wrlock(&g.datalock);

	struct master **m = &g.master;

	while (*m) {

		if (strcmp((*m)->name, str) == 0) {
			struct master *tmp = (*m);
			*m = (*m)->next;

			free(tmp->name);
			free(tmp);

			g.issaved = 0;

			pthread_rwlock_unlock(&g.datalock);
			return;
		}

		m = &(*m)->next;
	}
	pthread_rwlock_unlock(&g.datalock);
}

void add_ban(const char *str){
	pthread_rwlock_wrlock(&g.datalock);

	struct ban **m = &g.ban;

	while (*m) {

		if (strcmp((*m)->needle, str) == 0) {
			pthread_rwlock_unlock(&g.datalock);
			return;
		}

		m = &(*m)->next;
	}

	*m = malloc(sizeof(**m));
	(*m)->next = NULL;
	(*m)->needle = strdup(str);

	g.issaved = 0;

	pthread_rwlock_unlock(&g.datalock);
}

void rem_ban(const char *str){
	pthread_rwlock_wrlock(&g.datalock);

	struct ban **m = &g.ban;

	while (*m) {

		if (strcmp((*m)->needle, str) == 0) {
			struct ban *tmp = (*m);
			*m = (*m)->next;

			free(tmp->needle);
			free(tmp);

			g.issaved = 0;

			pthread_rwlock_unlock(&g.datalock);
			return;
		}

		m = &(*m)->next;
	}

	pthread_rwlock_unlock(&g.datalock);
}

void list_ban(client_t *ctx){
	pthread_rwlock_rdlock(&g.datalock);

	raw(ctx, "%s\r\n", "--ban-list--");

	struct ban *m = g.ban;

	while (m) {
		raw(ctx, "%s\r\n", m->needle);

		m = m->next;
	}

	raw(ctx, "%s\r\n", "--end-list--");

	pthread_rwlock_unlock(&g.datalock);
}

void list_master(client_t *ctx){
	pthread_rwlock_rdlock(&g.datalock);

	raw(ctx, "%s\r\n", "--master-list--");

	struct master *m = g.master;

	while (m) {
		raw(ctx, "%s\r\n", m->name);

		m = m->next;
	}

	raw(ctx, "%s\r\n", "--end-list--");

	pthread_rwlock_unlock(&g.datalock);
}



int wildcmp(const char *wild, const char *string) {
	// Written by Jack Handy - jakkhandy@hotmail.com

	const char *cp = NULL, *mp = NULL;

	while ((*string) && (*wild != '*')) {
		if ((toupper(*wild) != toupper(*string)) && (*wild != '?')) {
			return 0;
		}
		wild++;
		string++;
	}

	while (*string) {
		if (*wild == '*') {
			if (!*++wild) {
				return 1;
			}
			mp = wild;
			cp = string+1;
		} else if ((toupper(*wild) == toupper(*string)) || (*wild == '?')) {
			wild++;
			string++;
		} else {
			wild = mp;
			string = cp++;
		}
	}

	while (*wild == '*') {
		wild++;
	}

	return !*wild;
}



void join_cb(client_t *ctx, struct data *pb, char *speaker, unsigned long flags, char *text){
	pthread_rwlock_rdlock(&g.datalock);

	struct ban *ban = g.ban;

	while (ban) {
		if (wildcmp(ban->needle, speaker)) {
			raw(ctx, "/ban %s some reason\r\n", speaker);
		}

		ban = ban->next;
	}

	pthread_rwlock_unlock(&g.datalock);
}

void talk_cb(client_t *ctx, struct data *pb, char *szSpeaker, unsigned long uFlags, char *szEventText){
	char *text;
	char *com;

	char pi[512];
	FILE *fp1;
	char pingStr[250];

	pthread_rwlock_rdlock(&g.datalock);

	struct master *m = g.master;

	while (m) {
		if (strcasecmp(szSpeaker,m->name) == 0)
			break;

		m = m->next;
	}

	pthread_rwlock_unlock(&g.datalock);

	if (!m)
		return;

	m = NULL;

	text = szEventText;

	if ( text[0] == pb->trigger ) {
		text=&text[1];
	} else {
		return;
	}

	com = strsep( &text, " " );
	if (com == NULL) return;

//	printf("com: %s\n", com);

	if (!strcasecmp("trigger",com)) {
		raw(ctx, "Trigger: %c\r\n", pb->trigger);
	} else if (!strcasecmp(com,"settrigger")) {
		if ( text == NULL ) return;
		pb->trigger = text[0];
		raw(ctx, "Trigger is now: %c\r\n",pb->trigger);

	} else if (!strcasecmp( com, "say")) {
		if ( text == NULL ) return;
		raw(ctx, "%s\r\n", text );
	} else if (!strcasecmp("ver",com)) {
		raw(ctx, "[BANG0D] - Version 2.0 - Linux - Private\r\n");
	} else if (!strcasecmp("reconnect",com)) {
		pb->reconnect = 1;
	} else if (!strcasecmp("quit",com)) {
		pb->reconnect = 0;
		pb->quit = 1;
	} else if (!strcasecmp("place",com)) {
		raw(ctx, "I was user number %d to logon.\r\n", pb->place);
	} else if (strcasecmp("ping",com) == 0) {
		sprintf(pingStr, "ping -c1 %s", pb->server);
		fp1 = popen(pingStr,"r");
		fgets(pi,512,fp1);
		fgets(pi,512,fp1);
		raw(ctx, "%s\r\n",pi);
		fclose(fp1);
	} else if (!strcasecmp("delay",com)) {
		raw(ctx, "Delay: %d\r\n",pb->delay);
	} else if (!strcasecmp("setdelay", com)) {
		if ( text == NULL ) return;
		pb->delay = atoi(text);
		raw(ctx, "Delay is now: %d\r\n",pb->delay);
	} else if (!strcasecmp(com, "ban")) {
		if ( text == NULL ) return;
		raw(ctx, "/ban %s\r\n", text );
	} else if (!strcasecmp(com, "kick")) {
		if ( text == NULL ) return;
		raw(ctx, "/kick %s\r\n", text );
	} else if (!strcasecmp(com, "+master")) {
		add_master(text);
		raw(ctx, "%s added successfully\r\n",text);
		save();
	} else if (!strcasecmp(com, "-master")) {
		raw(ctx, "%s removed successfully\r\n",text);
		rem_master(text);
		save();
//	} else if (!strcasecmp(com, "?master")) {
//		list_master(ctx);
	} else if (!strcasecmp(com, "+ban")) {
		add_ban(text);
		raw(ctx, "%s added successfully\r\n",text);
		save();
	} else if (!strcasecmp(com, "-ban")) {
		rem_ban(text);
		raw(ctx, "%s removed successfully\r\n",text);
		save();
//	} else if (!strcasecmp(com, "?ban")) {
//		list_ban(ctx);
	} else if (!strcasecmp(com, "rejoin")) {
		raw(ctx, "/rejoin\r\n");
	} else if (!strcasecmp(com, "join")) {
		if ( text == NULL ) return;
		raw(ctx, "/join %s\r\n", text);
	} else if (!strcasecmp("home",com)) {
		raw(ctx, "Home: %s\r\n",pb->channel);
	} else if (!strcasecmp("sethome",com)) {
		if ( text == NULL ) return;

		free(pb->channel);
		pb->channel = strdup(text);

		raw(ctx, "Home is now: %s\r\n",pb->channel);
	} else if (!strcasecmp(com, "op")) {
		if (text == NULL) return;
		raw(ctx, "/designate %s\r\n", text );
		raw(ctx, "/resign\r\n");
	} else if (!strcasecmp( com, "designate")) {
		if ( text == NULL ) return;
		raw(ctx, "/designate %s\r\n", text );
	} else if (!strcasecmp( com, "resign")) {
		raw(ctx, "/resign\r\n");
	}

	return;
}

void channel_cb(client_t *ctx, struct data *pb, char *szSpeaker, unsigned long uFlags, char *szEventText){
	/* Have we just logged on, or have we been kicked out? */
	if ( strcasecmp( szEventText, pb->channel ) != 0 ) {
		pb->hammer = 1;
	} else {
		pb->hammer = 0;
	}
	return;
}


void *connthread(void *arg);

int spawn(struct data *pb, int n) {
	pthread_attr_t attr;
	pthread_attr_init(&attr);
	int err = pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);

	if (err) {
		fprintf(stderr, "could not setattr %d\n", err);
		return -1;
	}

	int i;
	for (i = 0; i < n; i++) {
		fprintf(stderr, "threading <%s> %d\n", pb->username, i);
		err = pthread_create(&pb->self, NULL, connthread, pb);
		if (err) {
			fprintf(stderr, "could not thread %d\n", err);
			return -1;
		}
	}

	pthread_attr_destroy(&attr);

	return 0;
}

void *connthread(void *arg) {

	int err;
	err = pthread_detach ( pthread_self () );
	if (err == EINVAL){
		fprintf(stderr, "thread not joinable\n");
	} else if (err == EINVAL){
		fprintf(stderr, "thread not found\n");
	}else if (err != 0) {
		fprintf(stderr, "thread other %d\n", err);
	}

	struct data *pb = (struct data *)arg;

	struct client_callbacks cb = {
		NULL,//show_user
		(client_cb_t *)join_cb,//join
		NULL,//user_flags
		NULL,//leave
		(client_cb_t *)talk_cb,//talk
		NULL,//broadcast
		(client_cb_t *)channel_cb,//channel
		(client_cb_t *)talk_cb,//whisper
		NULL,//whisper_sent
		NULL,//emote
		NULL,//channel_full
		NULL,//channel_not_exist
		NULL,//channel_restricted
		NULL,//channel_info
		NULL,//unique_name
		NULL,//error
		NULL,//info
	};

	client_t *ctx = client_init(&cb, pb);
	if (!ctx) {
		fprintf(stderr, "client_init()\n");
		return NULL;
	}

	pb->reconnect = 0;

	//try make connection
	while (__sync_bool_compare_and_swap(&pb->first, 0, 0)) {
		int err = client_connect(ctx, pb->server, pb->port, pb->bindaddr, pb->delay);
		if (err == 0) {
			//timeout
			continue;
		} else if (err < 0) {
			//error
			fprintf(stderr, "client_connect() %s\n", client_err_str(ctx));
			client_close(ctx);
			continue;
		} else {
			//connected!
			break;
		}
	}

	//check if first connection
	if (!__sync_bool_compare_and_swap(&pb->first, 0, 1)) {
		//we were not first...
		fprintf(stderr, "not first!\n");
		client_close(ctx);
		client_destroy(ctx);
		pthread_exit(0);
		return NULL;
	}


	raw(ctx,"%c%c%s\r\n%s\r\n", 0x03, 0x04, pb->username, pb->password);
	raw(ctx,"/join %s\r\n", pb->channel);
	pb->hammer = 1;

	struct timeval tm = {.tv_sec = 1, .tv_usec = 0};
	while (1) {
		int ret = client_process(ctx, &tm);

		if (ret < 0) {
			fprintf(stderr, "client_process() %s\n", client_err_str(ctx));
			break;
		}

		if (ret == 0) {
			//timeout
			if (pb->hammer) {
				raw(ctx,"/join %s\r\n", pb->channel);
				tm.tv_sec = 3;
			} else {
				tm.tv_sec = 3;
			}
		}

		if (ret == 1) {
			//ordinary message recieved
			if (pb->hammer && tm.tv_sec > 1) {
				//reset timer
				tm.tv_sec = 3;
			}
		}

		if (pb->reconnect) {
			pb->reconnect = 0;
			break;
		}

		if (pb->quit) {
			break;
		}
	}

	client_close(ctx);

	client_destroy(ctx);

	__sync_bool_compare_and_swap(&pb->first, 1, 0);

	if (pb->quit) {
		fprintf(stderr,"Shutdown\n");
	} else {
		fprintf(stderr,"Reconnecting\n");
		spawn(pb, pb->sockets);
	}

	pthread_exit(0);
	return NULL;
}

char *strxdup(char *x){
	if (x)
		return strdup(x);

	return NULL;
}

int launch_bot(const char *file, const char *section){
	fprintf(stderr, "launching %s\n", section);

	struct data *pb;

	pb = malloc(sizeof(*pb));
	memset(pb, 0, sizeof(*pb));

	pb->port = 6112;
	pb->delay = 1000;
	pb->trigger = '`';

	int line = 0;
	char r[1024];
	FILE *cfg;

	cfg = fopen(file, "r");
	if (!cfg) {
		return -1;
	}

	while(fgets(r, 1024, cfg)) {
		line++;
		strtok(r, "\r\n");
		if(r[0] == '[') {
			r[strlen(r)-1] = 0;
			if (strcmp(section, r+1) == 0) {

	while(fgets(r, 1024, cfg)) {
		line++;
		strtok(r, "\r\n");
		if (r[0] == '[') {
			break;
		} else if(!memcmp(r, "home=", 5)) {
			pb->channel = strdup(r+5);
		} else if(!memcmp(r, "username=", 9)) {
			pb->username = strdup(r+9);
		} else if(!memcmp(r, "server=", 7)) {
			pb->server = strdup(r+7);
		} else if(!memcmp(r, "bind=", 5)) {
			pb->bindaddr = strdup(r+5);
		} else if(!memcmp(r, "port=", 5)) {
			pb->port = atoi(r+5);
		} else if(!memcmp(r, "password=", 9)) {
			pb->password = strdup(r+9);
		} else if(!memcmp(r, "delay=", 6)) {
			pb->delay = atoi(r+6);
		} else if(!memcmp(r, "sockets=", 8)) {
			pb->sockets = atoi(r+8);
		} else if(!memcmp(r, "trigger=", 8)) {
			pb->trigger = r[8];
		}
	}

			}
		}
	}
	fclose(cfg);

	//check args
	if (!pb->server){
		fprintf(stderr, "[%s] bot without server\n",section);
		return -1;
	}

	if (!pb->password){
		fprintf(stderr, "[%s] bot without password\n",section);
		return -1;
	}

	if (!pb->username){
		fprintf(stderr, "[%s] bot without username\n",section);
		return -1;
	}

	if (!pb->channel){
		fprintf(stderr, "[%s] bot without home\n",section);
		return -1;
	}

	//create n threads!
	if (pb->sockets < 1) {
		return 0;
	}



	return spawn(pb, pb->sockets);
}

void loadc(const char *file, const char *section){
	FILE *cfg;
	int line = 0;

	if(section == NULL)
		section = "default";

	cfg = fopen(file, "r");

	if (!cfg) {
		fprintf(stderr,"Error opening cfg!\n");
		return;
	}

	char r[1024];
	while(fgets(r, 1024, cfg)) {
		line++;
		strtok(r, "\r\n");
		if(r[0] == '[') {
			r[strlen(r)-1] = '\0';
			if (strcmp(section, r+1) == 0) {
				while(fgets(r, 1024, cfg)) {
					strtok(r, "\r\n");
					if(!memcmp(r, "load=", 5)) {
						char *p = (char *)malloc(strlen(r+5)+1);
						strcpy(p, r+5);

						int ret = launch_bot(file, p);

						if (ret<0) {
							fprintf(stderr,"launching bot %s failed\n", p);
						}
						free(p);
					}
				}
			}
		}
	}

	fclose(cfg);
}

void load_bans(const char *file) {
	FILE *fp = fopen(file, "r");
	if (!fp) {
		perror(file);
		return;
	}

	char r[1024];
	while(fgets(r, 1024, fp)) {
		strtok(r, "\r\n");
		if (strlen(r) > 0 ) {
			add_ban(r);
		}
	}
	fclose(fp);
}

void load_users(const char *file) {
	FILE *fp = fopen(file, "r");
	if (!fp) {
		perror(file);
		return;
	}

	char r[1024];
	while(fgets(r, 1024, fp)) {
		strtok(r, "\r\n");
		if (strlen(r) > 0 ) {
			add_master(r);
		}
	}
	fclose(fp);
}

/* This structure is used by main to communicate with parse_opt. */
struct arguments {
	char *config;
	char *ban;
	char *user;
	int bg;
} arguments = {"bot.cfg", "ban.cfg", "user.cfg", 0};

const char *argp_program_version = "2.0";
const char *argp_program_bug_address = "/dev/null";

/* OPTIONS.  Field 1 in ARGP. | Order of fields: {NAME, KEY, ARG, FLAGS, DOC}. */
static struct argp_option options[] = {
	{"config",    'c',    "FILE",       0, "Config file to load"},
	{"bans",      'b',    "FILE",       0, "Bans file to load"},
	{"users",     'u',    "FILE",       0, "User file to load"},
	{"background", 1001,    NULL,       0, "Run in background"},
	{0}
};

/* PARSER. Field 2 in ARGP. | Order of parameters: KEY, ARG, STATE. */
static error_t parse_opt (int key, char *arg, struct argp_state *state){
	struct arguments *arguments = state->input;

	switch (key){
		case 'c':;
			arguments->config = arg;
			break;
		case 'b':;
			arguments->ban = arg;
			break;
		case 'u':;
			arguments->user = arg;
			break;
		case 1001:;
			arguments->bg = 1;
			break;
		default:
			return ARGP_ERR_UNKNOWN;
	}
	return 0;
}

/* ARGS_DOC. Field 3 in ARGP. | A description of the non-option command-line arguments that we accept. */
static char args_doc[] = "";
/* DOC.  Field 4 in ARGP. | Program documentation. */
static char doc[] = "Rewrite of eRecon by PHANT0M";
/* The ARGP structure itself. */
static struct argp argp = {options, parse_opt, args_doc, doc};

int main(int argc, char* argv[]) {
	argp_parse(&argp, argc, argv, 0, 0, &arguments);

	fprintf(stderr, "BANG0D v%s || By Rob - Revised by PHANT0M\n", argp_program_version);

	if (arguments.bg) {
		int err = daemon(1,1);

		if (err == -1) {
			perror("could not daemonize");
			exit(0);
		}
	}

	printf("PID: %d\n", getpid());

	//init globals
	memset(&g, 0, sizeof(g));

	pthread_rwlock_init(&g.datalock, NULL);

	g.userfile = arguments.user;
	g.banfile = arguments.ban;
	g.issaved = 1;

	load_bans(arguments.ban);
	load_users(arguments.user);

	loadc(arguments.config, "_load");

	//after threading all the bots. simply let them do their thing.
	while (1) sleep(100);

	return 0;
}

