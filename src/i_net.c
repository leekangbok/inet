#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <i_mem.h>
#include <i_net.h>

static icode_t pass_handler(ichannel_t *channel,
							ichannelhandler_ctx_t *ctx,
							void **data)
{
	return ctx->next(channel, ctx->next_ctx, data);
}

static icode_t def_outbound_handler(ichannel_t *channel,
									ichannelhandler_ctx_t *ctx,
									void **data)
{
	printf("def_outbound_handler\n");
	return ISUCCESS;
}

static icode_t def_inbound_handler(ichannel_t *channel,
								   ichannelhandler_ctx_t *ctx,
								   void **data)
{
	printf("def_inbound_handler\n");
	return ISUCCESS;
}

static void set_def_inbound_handler(ichannelhandler_t *channelhandler)
{
	memset(channelhandler, 0x00, sizeof(ichannelhandler_t));

	channelhandler->read.operation = def_inbound_handler;
}

static void set_def_outbound_handler(ichannelhandler_t *channelhandler)
{
	memset(channelhandler, 0x00, sizeof(ichannelhandler_t));

	channelhandler->write.operation = def_outbound_handler;
}

icode_t init_channelpipeline(ichannelpipeline_t *channelpipeline)
{
	channelpipeline->numofhandler = 0;
	channelpipeline->handler = NULL;

	channelpipeline->handler = irealloc(channelpipeline->handler,
										2 * sizeof(ichannelhandler_t));

	channelpipeline->numofhandler = 2;

	set_def_inbound_handler(channelpipeline->handler + 1);
	set_def_outbound_handler(channelpipeline->handler);

	return ISUCCESS;
}

icode_t set_channel_data(ichannel_t *channel,
						 void *data, data_destroy_f data_destroy)
{
	if (channel->data)
		channel->data_destroy(&channel->data);

	channel->data = data;
	channel->data_destroy = data_destroy;

	return ISUCCESS;
}

ichannel_t *create_channel(void *data, data_destroy_f data_destroy)
{
	ichannel_t *channel = icalloc(sizeof(ichannel_t));

	init_channelpipeline(&channel->pipeline);
	set_channel_data(channel, data, data_destroy);

	return channel;
}

icode_t add_channelhandler(ichannel_t *channel, channelhandler_f read,
						   channelhandler_f read_complete,
						   channelhandler_f write,
						   channelhandler_f write_complete,
						   channelhandler_f error,
						   channelhandler_f error_complete,
						   void *data, data_destroy_f data_destroy)
{
	ichannelpipeline_t *channelpipeline = &channel->pipeline;
	ichannelhandler_t *cur, *prev, *next;

	channelpipeline->handler = irealloc(channelpipeline->handler,
										(channelpipeline->numofhandler + 1) * \
										sizeof(ichannelhandler_t));

	next = channelpipeline->handler + channelpipeline->numofhandler;
	cur = next - 1;
	prev = cur - 1;

	memcpy(next, cur, sizeof(ichannelhandler_t));

	cur->data = data;
	cur->data_destroy = data_destroy;
	cur->pipeline = channelpipeline;

	cur->read.operation = read ? read : pass_handler;
	cur->read.next = next->read.operation;
	cur->read.next_ctx = &next->read;
	prev->read.next = cur->read.operation;
	prev->read.next_ctx = &cur->read;

	cur->write.operation = write ? write : pass_handler;
	cur->write.next = prev->write.operation;
	cur->write.next_ctx = &prev->write;
	next->write.next = cur->write.operation;
	next->write.next_ctx = &cur->write;

	channelpipeline->numofhandler += 1;

	return ISUCCESS;
}

icode_t on_fire_read(ichannel_t *channel, void **data)
{
	ichannelhandler_ctx_t *ctx = &channel->pipeline.handler[0].read;
	return ctx->next(channel, ctx->next_ctx, data);
}

icode_t on_fire_write(ichannel_t *channel, void **data)
{
	int last = channel->pipeline.numofhandler - 1;
	ichannelhandler_ctx_t *ctx = &channel->pipeline.handler[last].write;
	return ctx->next(channel, ctx->next_ctx, data);
}
