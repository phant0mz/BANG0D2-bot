
enum client_error {ERR_OK = 0, ERR_RESOLVE, ERR_BIND, ERR_SOCKET, ERR_CONNECT, ERR_MEM, ERR_REMOTE, ERR_READ, ERR_MAX};
enum client_state {STATE_INIT = 0, STATE_CONNECTED, STATE_CLOSED};

typedef struct client client_t;

typedef void client_cb_t(client_t *ctx, void *, const char *speaker, unsigned long flags, const char *text);

struct client_callbacks {
	void (*show_user) (client_t *ctx, void *, const char *speaker, unsigned long flags, const char *text);
	void (*join) (client_t *ctx, void *, const char *speaker, unsigned long flags, const char *text);
	void (*user_flags) (client_t *ctx, void *, const char *speaker, unsigned long flags, const char *text);
	void (*leave) (client_t *ctx, void *, const char *speaker, unsigned long flags, const char *text);
	void (*talk) (client_t *ctx, void *, const char *speaker, unsigned long flags, const char *text);
	void (*broadcast) (client_t *ctx, void *, const char *speaker, unsigned long flags, const char *text);
	void (*channel) (client_t *ctx, void *, const char *speaker, unsigned long flags, const char *text);
	void (*whisper) (client_t *ctx, void *, const char *speaker, unsigned long flags, const char *text);
	void (*whisper_sent) (client_t *ctx, void *, const char *speaker, unsigned long flags, const char *text);
	void (*emote) (client_t *ctx, void *, const char *speaker, unsigned long flags, const char *text);
	void (*channel_full) (client_t *ctx, void *, const char *speaker, unsigned long flags, const char *text);
	void (*channel_not_exist) (client_t *ctx, void *, const char *speaker, unsigned long flags, const char *text);
	void (*channel_restricted) (client_t *ctx, void *, const char *speaker, unsigned long flags, const char *text);
	void (*channel_info) (client_t *ctx, void *, const char *speaker, unsigned long flags, const char *text);
	void (*unique_name) (client_t *ctx, void *, const char *speaker, unsigned long flags, const char *text);
	void (*error) (client_t *ctx, void *, const char *speaker, unsigned long flags, const char *text);
	void (*info) (client_t *ctx, void *, const char *speaker, unsigned long flags, const char *text);
};

client_t *client_init(struct client_callbacks *cb, void *dta);

void client_destroy(client_t *);

int client_connect(client_t *ctx, const char *server, unsigned int port, const char *bindaddr, int delay);

const char *client_err_str(client_t *ctx);

void client_close(client_t *);

int raw(client_t *ctx, char *fmt, ...);

int client_process(client_t *ctx, struct timeval *tm);

