#include <i_mem.h>

#include <echo/echo.h>

static icode_t active_stage1(ichannel_t *channel,
							 ichannelhandler_ctx_t *ctx,
							 void *data, ssize_t datalen)
{
	printf("%s\n", ctx->name);
	return ctx->next(channel, ctx->next_ctx, data, datalen);
}

static icode_t read_stage1(ichannel_t *channel,
						   ichannelhandler_ctx_t *ctx,
						   void *data, ssize_t datalen)
{
	printf("%s\n", ctx->name);
	return ctx->next(channel, ctx->next_ctx, data, datalen);
}

static icode_t write_stage1(ichannel_t *channel,
							ichannelhandler_ctx_t *ctx,
							void *data, ssize_t datalen)
{
	printf("%s\n", ctx->name);
	return ctx->next(channel, ctx->next_ctx, data, datalen);
}

static icode_t active_stage2(ichannel_t *channel,
							 ichannelhandler_ctx_t *ctx,
							 void *data, ssize_t datalen)
{
	printf("%s\n", ctx->name);
	return ctx->next(channel, ctx->next_ctx, data, datalen);
}

static icode_t read_stage2(ichannel_t *channel,
						   ichannelhandler_ctx_t *ctx,
						   void *data, ssize_t datalen)
{
	printf("%s\n", ctx->name);
	ifree(data);
	return fire_ctx_event(ctx, IEVENT_WRITE, NULL, 0);
}

static icode_t write_stage2(ichannel_t *channel,
							ichannelhandler_ctx_t *ctx,
							void *data, ssize_t datalen)
{
	printf("%s\n", ctx->name);
	return ctx->next(channel, ctx->next_ctx, data, datalen);
}

void setup_echo_channel(ichannel_t *channel)
{
	add_channelhandler(channel, "echo_stage1",
					   active_stage1,
					   read_stage1,
					   NULL,
					   write_stage1,
					   NULL,
					   NULL,
					   NULL,
					   NULL, NULL);

	add_channelhandler(channel, "echo_stage2",
					   active_stage2,
					   read_stage2,
					   NULL,
					   write_stage2,
					   NULL,
					   NULL,
					   NULL,
					   NULL, NULL);
}
