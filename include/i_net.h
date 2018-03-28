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
									ichannelhandler_ctx_t *channelhandler_ctx,
									void **data);
typedef void (*data_destroy_f)(void **data);

struct ichannelhandler_ctx_ {
	channelhandler_f next;
	channelhandler_f operation;
	ichannelhandler_t *channelhandler;
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
	ichannelpipeline_t *channelpipeline;
	int index;
};

struct ichannelpipeline_ {
	int numofhandlers;
	ichannelhandler_t *channelhandler;
	ichannel_t *channel;
};

struct ichannel_ {
	ichannelpipeline_t channelpipeline;
	void *data;
	data_destroy_f data_destroy;
};

#endif
