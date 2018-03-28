#include <stdio.h>

#include <i_mem.h>
#include <i_net.h>

static icode_t read1(ichannel_t *channel,
					 ichannelhandler_ctx_t *channelhandler_ctx,
					 void **data)
{
	printf("read1\n");
	return channelhandler_ctx->next(channel, channelhandler_ctx->next_ctx, data);
}

static icode_t read2(ichannel_t *channel,
					 ichannelhandler_ctx_t *channelhandler_ctx,
					 void **data)
{
	printf("read2\n");
	return channelhandler_ctx->next(channel, channelhandler_ctx->next_ctx, data);
}

static icode_t write1(ichannel_t *channel,
					  ichannelhandler_ctx_t *channelhandler_ctx,
					  void **data)
{
	printf("write1\n");
	return channelhandler_ctx->next(channel, channelhandler_ctx->next_ctx, data);
}

static icode_t write2(ichannel_t *channel,
					  ichannelhandler_ctx_t *channelhandler_ctx,
					  void **data)
{
	printf("write2\n");
	return channelhandler_ctx->next(channel, channelhandler_ctx->next_ctx, data);
}

int main(int argc, char *argv[])
{
	ichannel_t *channel;

	iallocator_init();

	channel = create_channel(NULL, NULL);
	add_channelhandler(channel, read1,
					   NULL,
					   write1,
					   NULL,
					   NULL,
					   NULL,
					   NULL, NULL);
	add_channelhandler(channel, read2,
					   NULL,
					   write2,
					   NULL,
					   NULL,
					   NULL,
					   NULL, NULL);
	add_channelhandler(channel, read1,
					   NULL,
					   write1,
					   NULL,
					   NULL,
					   NULL,
					   NULL, NULL);
	add_channelhandler(channel, read2,
					   NULL,
					   write2,
					   NULL,
					   NULL,
					   NULL,
					   NULL, NULL);
	add_channelhandler(channel, read1,
					   NULL,
					   write1,
					   NULL,
					   NULL,
					   NULL,
					   NULL, NULL);
	add_channelhandler(channel, read2,
					   NULL,
					   write2,
					   NULL,
					   NULL,
					   NULL,
					   NULL, NULL);

	on_fire_read(channel, NULL);
	on_fire_write(channel, NULL);
	return 1;
}
