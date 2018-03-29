#include <stdio.h>

#include <i_mem.h>
#include <i_net.h>

static icode_t read1(ichannel_t *channel,
					 ichannelhandler_ctx_t *ctx,
					 void **data)
{
	printf("read1 %s\n", ctx->name);
	return ctx->next(channel, ctx->next_ctx, data);
}

static icode_t read2(ichannel_t *channel,
					 ichannelhandler_ctx_t *ctx,
					 void **data)
{
	printf("read2 %s\n", ctx->name);
	return ctx->next(channel, ctx->next_ctx, data);
}

static icode_t read3(ichannel_t *channel,
					 ichannelhandler_ctx_t *ctx,
					 void **data)
{
	printf("read3 %s\n", ctx->name);
	return ctx->next(channel, ctx->next_ctx, data);
}

static icode_t write1(ichannel_t *channel,
					  ichannelhandler_ctx_t *ctx,
					  void **data)
{
	printf("write1 %s\n", ctx->name);
	return ctx->next(channel, ctx->next_ctx, data);
}

static icode_t write2(ichannel_t *channel,
					  ichannelhandler_ctx_t *ctx,
					  void **data)
{
	printf("write2 %s\n", ctx->name);
	return ctx->next(channel, ctx->next_ctx, data);
}

static icode_t write3(ichannel_t *channel,
					  ichannelhandler_ctx_t *ctx,
					  void **data)
{
	printf("write3 %s\n", ctx->name);
	return ctx->next(channel, ctx->next_ctx, data);
}

int main(int argc, char *argv[])
{
	ichannel_t *channel;

	iallocator_init();

	channel = create_channel(NULL, NULL);
	add_channelhandler(channel, "x",
					   NULL,
					   read1,
					   NULL,
					   write1,
					   NULL,
					   NULL,
					   NULL,
					   NULL, NULL);
	add_channelhandler(channel, "y",
					   NULL,
					   read2,
					   NULL,
					   write2,
					   NULL,
					   NULL,
					   NULL,
					   NULL, NULL);
	add_channelhandler(channel, "z",
					   NULL,
					   read3,
					   NULL,
					   write3,
					   NULL,
					   NULL,
					   NULL,
					   NULL, NULL);

	fire_event(channel, IEVENT_ACTIVE, NULL);
	fire_event(channel, IEVENT_READ, NULL);
	fire_event(channel, IEVENT_WRITE, NULL);

	{
		iserver_config_t config;

		memset(&config, 0x00, sizeof(iserver_config_t));

		config.name = "tst";
		config.servertype = "tcp_ip4";
		config.bindaddr = "127.0.0.1";
		config.bindport = 10000;
		add_iserver(&config);
		start_iserver();
	}

	return 1;
}
