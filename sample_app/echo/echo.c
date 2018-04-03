#include <i_mem.h>

#include <echo/echo.h>
#include <i_print.h>

#if 0
static icode_t active_stage1(ichannelhandler_ctx_t *ctx,
							 void *data, ssize_t datalen)
{
	prlog(LOGD, "%s", ctx->name);
	return ctx->next(ctx, data, datalen);
}
#endif

static icode_t read_stage1(ichannelhandler_ctx_t *ctx,
						   void *data, ssize_t datalen)
{
	prlog(LOGD, "%s", ctx->name);
	return ctx->next(ctx, data, datalen);
}

static icode_t write_stage1(ichannelhandler_ctx_t *ctx,
							void *data, ssize_t datalen)
{
	prlog(LOGD, "%s", ctx->name);
	return ctx->next(ctx, data, datalen);
}

static icode_t active_stage2(ichannelhandler_ctx_t *ctx,
							 void *data, ssize_t datalen)
{
	prlog(LOGD, "%s", ctx->name);
	return ctx->next(ctx, data, datalen);
}

static icode_t read_stage2(ichannelhandler_ctx_t *ctx,
						   void *data, ssize_t datalen)
{
	prlog(LOGD, "%s", ctx->name);
	return ctx->next(ctx, data, datalen);
}

static icode_t write_stage2(ichannelhandler_ctx_t *ctx,
							void *data, ssize_t datalen)
{
	prlog(LOGD, "%s", ctx->name);
	return ctx->next(ctx, data, datalen);
}

static icode_t write_and_close(calllater_t *c, void *data, int status)
{
	channel_shutdown(c->ctx->mychannel, ILOCALCLOSED);
	return ISUCCESS;
}

static icode_t read_stage3(ichannelhandler_ctx_t *ctx,
						   void *data, ssize_t datalen)
{
	calllater_t *c = create_calllater();

	prlog(LOGD, "%s", ctx->name);

	c->ctx = ctx;
	c->write.data = data;
	c->write.datalen = datalen;

	add_calllater(c, write_and_close, NULL, NULL);

	return fire_ctx_event(ctx, IEVENT_WRITE, c, sizeof(calllater_t));
}

static icode_t write_stage3(ichannelhandler_ctx_t *ctx,
							void *data, ssize_t datalen)
{
	prlog(LOGD, "%s", ctx->name);
	return ctx->next(ctx, data, datalen);
}

static icode_t write_done_stage1(ichannelhandler_ctx_t *ctx,
								 void *data, ssize_t datalen)
{
	prlog(LOGD, "%s", ctx->name);
	return ctx->next(ctx, data, datalen);
}

static icode_t write_done_stage3(ichannelhandler_ctx_t *ctx,
								 void *data, ssize_t datalen)
{
	prlog(LOGD, "%s", ctx->name);
	return ctx->next(ctx, data, datalen);
}

void setup_echo_channel(ichannel_t *channel)
{
	add_channelhandler(channel, "echo_stage1",
					   NULL,
					   read_stage1,
					   NULL,
					   write_stage1,
					   write_done_stage1,
					   NULL,
					   NULL,
					   NULL, NULL);

	add_channelhandler(channel, "echo_stage2",
					   active_stage2,
					   NULL,
					   NULL,
					   NULL,
					   NULL,
					   NULL,
					   NULL,
					   NULL, NULL);

	add_channelhandler(channel, "echo_stage3",
					   NULL,
					   read_stage3,
					   NULL,
					   write_stage3,
					   write_done_stage3,
					   NULL,
					   NULL,
					   NULL, NULL);
}
