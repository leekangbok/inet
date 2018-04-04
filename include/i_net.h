#ifndef _I_NET_H
#define _I_NET_H

#include <uv.h>
#include <llist.h>

#undef containerof
#define containerof(ptr, type, field) \
	((type *) ((char *) (ptr) - ((char *) &((type *) 0)->field)))

typedef enum code_ {
	SUCCESS,
	FAIL,
	STOLEN,
	PEERCLOSED,
	LOCALCLOSED,
	TIMEOUT,
} code_t;

typedef struct channel_ channel_t;
typedef struct channelpipeline_ channelpipeline_t;
typedef struct channelhandler_ channelhandler_t;
typedef struct channelhandlerctx_ channelhandlerctx_t;
typedef struct server_ server_t;
typedef struct server_config_ server_config_t;
typedef struct server_worker_ server_worker_t;
typedef struct callback_ callback_t;
typedef struct calllater_ calllater_t;

typedef code_t (*channelhandler_f)(channelhandlerctx_t *ctx, void *data,
								   ssize_t datalen);
typedef code_t (*call_later)(calllater_t *c, void *data, int status);
typedef void (*data_destroy_f)(void *data);

typedef struct {
	struct ll_head ll;
	channelhandlerctx_t *ctx;
	call_later on_work;
	call_later after_work;
	void *data;
	uv_work_t work;
} queue_work_t;

typedef struct {
	channel_t *channel;
	void *data;
	ssize_t datalen;
	uv_buf_t buf;
	const struct sockaddr *addr;
	union {
		uv_req_t req;
		uv_write_t write;
		uv_udp_send_t udp_write;
	} req;
} write_calllater_t;

struct callback_ {
	struct ll_head ll;
	call_later f;
	void *data;
	data_destroy_f data_destroy;
};

struct calllater_ {
	struct ll_head callbacks;
	union {
		write_calllater_t write;
		queue_work_t qwork;
	};
};

#define ACMD_NEW_TCP_CONN 1

typedef struct {
	struct ll_head ll;
	int cmd;
	union {
		struct {
			server_t *server;
			uv_tcp_t tcp;
			int fd;
		} new_connection;
	};
} async_cmd_t;

struct server_worker_ {
	int index;
	pthread_spinlock_t cmd_spinlock;
	struct ll_head new_cmds;
	uv_async_t cmd_handle;
	uv_thread_t thread_id;
};

#define HANDLER_NAME_MAX 32

struct channelhandlerctx_ {
	channelhandlerctx_t *next_ctx;
	channelhandler_f operation;
	channelhandler_t *handler;
	char name[HANDLER_NAME_MAX + 32];
#define mychannel handler->pipeline->channel
};

#define EVENT_ACTIVE		0
#define EVENT_READ			1
#define EVENT_READCOMPLETE	2
#define EVENT_WRITE			3
#define EVENT_WRITECOMPLETE	4
#define EVENT_ERROR			5
#define EVENT_ERRORCOMPLETE	6
#define EVENT_MAX			7

struct channelhandler_ {
	channelhandlerctx_t ctx[EVENT_MAX];
	void *data;
	data_destroy_f data_destroy;
	channelpipeline_t *pipeline;
	char name[HANDLER_NAME_MAX];
};

struct channelpipeline_ {
	int numofhandler;
	channelhandler_t *handler;
	channel_t *channel;
};

struct channel_ {
	struct ll_head ll;
	channelpipeline_t pipeline;
	union {
		uv_handle_t handle;
		uv_stream_t stream;
		uv_tcp_t tcp;
		uv_udp_t udp;
	} h;
	struct ll_head queueworks;
	int refcnt;
	uv_timer_t idle_timer_handle;
	uv_shutdown_t shutdown_handle;
	void *data;
	data_destroy_f data_destroy;
	uv_loop_t *uvloop;
	server_t *server;
};

struct server_config_ {
	const char *servertype;
	const char *name;
	const char *bindaddr;
	int bindport;
	int idle_timeout;
#define MAXSIGNUM 64
	void (*signals_cb[MAXSIGNUM])(server_t *server, int signum);
	void (*setup_server)(server_t *server);
	void (*setup_uvhandle)(uv_handle_t *handle, const char *what);
	void (*setup_channel)(channel_t *channel);
};

struct server_ {
	struct ll_head ll;
	struct ll_head channels;
	union {
		uv_handle_t handle;
		uv_stream_t stream;
		uv_tcp_t tcp;
		uv_udp_t udp;
	} h;
	union {
		struct sockaddr addr;
		struct sockaddr_in addr4;
		struct sockaddr_in6 addr6;
	} addr;
#define SERVERTYPE_TCP 1
#define SERVERTYPE_UDP 2
	int servertype;
	server_config_t config;
	int (*setup_server)(server_t *server, uv_loop_t *uvloop);
	uv_loop_t *uvloop;
	void *data;
};

calllater_t *create_calllater(void);
void add_calllater(calllater_t *c, call_later f, void *data, data_destroy_f df);
void call_callbacks(calllater_t *c, int status);
void queue_channel_work(channelhandlerctx_t *ctx, void *data,
						call_later on_work, call_later after_work);
void uvbuf_alloc(uv_handle_t *handle, size_t suggested_size,
				 uv_buf_t *buf);
void notify_async_cmd(async_cmd_t *cmd);
code_t callup(channelhandlerctx_t *ctx, void *data, ssize_t datalen);
code_t callup_channel(channel_t *channel, int event,
					  void *data, ssize_t datalen);
code_t callup_context(channelhandlerctx_t *ctx, int event,
					  void *data, ssize_t datalen);
code_t add_channelhandler(channel_t *channel, const char *name,
						  channelhandler_f active,
						  channelhandler_f read,
						  channelhandler_f read_complete,
						  channelhandler_f write,
						  channelhandler_f write_complete,
						  channelhandler_f error,
						  channelhandler_f error_complete,
						  void *data, data_destroy_f data_destroy);
channel_t *create_channel(void *data, data_destroy_f data_destroy);
void destroy_channel(channel_t *channel);
code_t add_server(server_config_t *config);
code_t start_server(void);

#endif
