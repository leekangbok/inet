#ifndef _I_NET_H
#define _I_NET_H

#include <llist.h>

typedef enum icode_ {
	ISUCCESS,
	IFAIL,
	ISTOLEN
} icode_t;

typedef struct ichannel_ ichannel_t;
typedef struct ichannelpipeline_ ichannelpipeline_t;
typedef struct ichannelhandler_ ichannelhandler_t;
typedef struct ichannelhandler_ctx_ ichannelhandler_ctx_t;

typedef icode_t (*channelhandler_f)(ichannel_t *channel,
									ichannelhandler_ctx_t *ctx,
									void **data);
typedef void (*data_destroy_f)(void **data);

struct ichannelhandler_ctx_ {
	channelhandler_f next;
	ichannelhandler_ctx_t *next_ctx;
	channelhandler_f operation;
	ichannelhandler_t *handler;
};

struct ichannelhandler_ {
	ichannelhandler_ctx_t read;
	ichannelhandler_ctx_t read_complete;
	ichannelhandler_ctx_t write;
	ichannelhandler_ctx_t write_complete;
	ichannelhandler_ctx_t error;
	ichannelhandler_ctx_t error_complete;
	void *data;
	data_destroy_f data_destroy;
	ichannelpipeline_t *pipeline;
};

struct ichannelpipeline_ {
	int numofhandler;
	ichannelhandler_t *handler;
	ichannel_t *channel;
};

struct ichannel_ {
	ichannelpipeline_t pipeline;
	void *data;
	data_destroy_f data_destroy;
};

icode_t on_fire_read(ichannel_t *channel, void **data);
icode_t on_fire_write(ichannel_t *channel, void **data);
icode_t add_channelhandler(ichannel_t *channel, channelhandler_f read,
						   channelhandler_f read_complete,
						   channelhandler_f write,
						   channelhandler_f write_complete,
						   channelhandler_f error,
						   channelhandler_f error_complete,
						   void *data, data_destroy_f data_destroy);
ichannel_t *create_channel(void *data, data_destroy_f data_destroy);

#endif
