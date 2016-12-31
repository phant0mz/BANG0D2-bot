
#define MAX_THREAD 1000
#define MAX_CHILDTHREAD 3
#define BUFSIZE			2048
#define INVALID_SOCKET		-1
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

struct master {
	char *name;
	struct master *next;
};

struct ban {
	char *needle;
	struct ban *next;
};

struct data {
	pthread_t self;
	int place;

	int reconnect;
	int quit;
	int rejoin;

	char *password;
	char *username;
	char *cdkey;
	char *type;
	char *server;
	char *bindaddr;
	char *channel;

	char trigger;

	int port;
	int hammer;

	int sockets;
	int delay;//useconds_t

	int first;
};

