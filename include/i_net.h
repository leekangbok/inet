#ifndef _I_NET_H
#define _I_NET_H

#include <uv.h>
#include <llist.h>

#undef icontainer_of
#define icontainer_of(ptr, type, field) \
	((type *) ((char *) (ptr) - ((char *) &((type *) 0)->field)))

typedef enum icode_ {
	ISUCCESS,
	IFAIL,
	ISTOLEN
} icode_t;

typedef struct ichannel_ ichannel_t;
typedef struct ichannelpipeline_ ichannelpipeline_t;
typedef struct ichannelhandler_ ichannelhandler_t;
typedef struct ichannelhandler_ctx_ ichannelhandler_ctx_t;
typedef struct iserver_ iserver_t;
typedef struct iserver_config_ iserver_config_t;
typedef struct iserver_worker_ iserver_worker_t;

typedef icode_t (*channelhandler_f)(ichannel_t *channel,
									ichannelhandler_ctx_t *ctx,
									void *data, ssize_t datalen);
typedef icode_t (*call_later)(void *data);
typedef void (*data_destroy_f)(void *data);

typedef struct {
} calllater_t;

#define ACMD_NEW_TCP_CONN 1

typedef struct {
	struct ll_head ll;
	int cmd;
	union {
		struct {
			iserver_t *server;
			uv_tcp_t tcp;
			int fd;
		} new_connection;
	};
} async_cmd_t;

struct iserver_worker_ {
	int index;
	pthread_spinlock_t cmd_spinlock;
	struct ll_head new_cmds;
	uv_async_t cmd_handle;
	uv_thread_t thread_id;
};

#define IHANDLER_NAME_MAX 32

struct ichannelhandler_ctx_ {
	channelhandler_f next;
	ichannelhandler_ctx_t *next_ctx;
	channelhandler_f operation;
	ichannelhandler_t *handler;
	char name[IHANDLER_NAME_MAX + 32];
};

#define IEVENT_ACTIVE		0
#define IEVENT_READ			1
#define IEVENT_READCOMPLETE	2
#define IEVENT_WRITE			3
#define IEVENT_WRITECOMPLETE	4
#define IEVENT_ERROR			5
#define IEVENT_ERRORCOMPLETE	6
#define IEVENT_MAX			7

struct ichannelhandler_ {
	ichannelhandler_ctx_t ctx[IEVENT_MAX];
	void *data;
	data_destroy_f data_destroy;
	ichannelpipeline_t *pipeline;
	char name[IHANDLER_NAME_MAX];
};

struct ichannelpipeline_ {
	int numofhandler;
	ichannelhandler_t *handler;
	ichannel_t *channel;
};

struct ichannel_ {
	struct ll_head ll;
	ichannelpipeline_t pipeline;
	union {
		uv_handle_t handle;
		uv_stream_t stream;
		uv_tcp_t tcp;
		uv_udp_t udp;
	} h;
	int refcnt;
	uv_timer_t idle_timer_handle;
	uv_shutdown_t shutdown_handle;
	void *data;
	data_destroy_f data_destroy;
	uv_loop_t *uvloop;
	iserver_t *server;
};

struct iserver_config_ {
	const char *servertype;
	const char *name;
	const char *bindaddr;
	int bindport;
	int idle_timeout;
#define MAXSIGNUM 64
	void (*signals_cb[MAXSIGNUM])(iserver_t *server, int signum);
	void (*setup_server)(iserver_t *server);
	void (*setup_uvhandle)(uv_handle_t *handle, const char *what);
	void (*setup_channel)(ichannel_t *channel);
};

struct iserver_ {
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
	iserver_config_t config;
	int (*setup_server)(iserver_t *server, uv_loop_t *uvloop);
	uv_loop_t *uvloop;
	void *data;
};

void uvbuf_alloc(uv_handle_t *handle, size_t suggested_size,
				 uv_buf_t *buf);
void notify_async_cmd(async_cmd_t *cmd);
icode_t fire_pipeline_event(ichannel_t *channel, int ihandler_type,
							void *data, ssize_t datalen);
icode_t fire_ctx_event(ichannelhandler_ctx_t *ctx,
					   int event, void *data, ssize_t datalen);
icode_t add_channelhandler(ichannel_t *channel, const char *name,
						   channelhandler_f active,
						   channelhandler_f read,
						   channelhandler_f read_complete,
						   channelhandler_f write,
						   channelhandler_f write_complete,
						   channelhandler_f error,
						   channelhandler_f error_complete,
						   void *data, data_destroy_f data_destroy);
ichannel_t *create_channel(void *data, data_destroy_f data_destroy);
void destroy_channel(ichannel_t *channel);
icode_t add_iserver(iserver_config_t *config);
icode_t start_iserver(void);

#endif
