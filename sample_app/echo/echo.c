#include <unistd.h>
#include <string.h>

#include <i_mem.h>

#include <echo/echo.h>
#include <i_print.h>

code_t on_after_work(calllater_t *cl, void *data, int status)
{
	prlog(LOGD, "complete(%p), status: %d", cl, status);
	if (status < 0)
		return FAIL;

	char *senddata = icalloc(100);

	sprintf(senddata, "%s\n", "hello telnet");
	calllater_t *wcl = create_write_req(senddata, strlen(senddata));

	callup_context(cl->qwork.ctx, EVENT_WRITE, wcl, sizeof(calllater_t));
	return SUCCESS;
}

code_t on_work(calllater_t *cl, void *data, int status)
{
	prlog(LOGD, "Sleeping...");
	sleep(1);
	return SUCCESS;
}

#if 0
static code_t active_stage1(channelhandlerctx_t *ctx,
							void *data, ssize_t datalen)
{
	prlog(LOGD, "%s", ctx->name);
	return callup(ctx, data, datalen);
}
#endif

static code_t read_stage1(channelhandlerctx_t *ctx,
						  void *data, ssize_t datalen)
{
	prlog(LOGD, "%s", ctx->name);
	return callup(ctx, data, datalen);
}

static code_t write_stage1(channelhandlerctx_t *ctx,
						   void *data, ssize_t datalen)
{
	prlog(LOGD, "%s", ctx->name);
	return callup(ctx, data, datalen);
}

static code_t active_stage2(channelhandlerctx_t *ctx,
							void *data, ssize_t datalen)
{
	queue_work(ctx, NULL, on_work, on_after_work);

	prlog(LOGD, "%s", ctx->name);
	return callup(ctx, data, datalen);
}

static code_t read_stage2(channelhandlerctx_t *ctx,
						  void *data, ssize_t datalen)
{
	prlog(LOGD, "%s", ctx->name);
	return callup(ctx, data, datalen);
}

static code_t write_stage2(channelhandlerctx_t *ctx,
						   void *data, ssize_t datalen)
{
	prlog(LOGD, "%s", ctx->name);
	return callup(ctx, data, datalen);
}

static code_t write_and_close(calllater_t *c, void *data, int status)
{
	channel_shutdown(((channelhandlerctx_t *)data)->mychannel, LOCALCLOSED);
	return SUCCESS;
}

static code_t read_stage3(channelhandlerctx_t *ctx,
						  void *data, ssize_t datalen)
{
	calllater_t *cl = add_calllater(create_write_req(data, datalen),
									write_and_close, ctx, NULL);
	prlog(LOGD, "%s", ctx->name);

	return callup_context(ctx, EVENT_WRITE, cl, sizeof(calllater_t));
}

static code_t write_stage3(channelhandlerctx_t *ctx,
						   void *data, ssize_t datalen)
{
	prlog(LOGD, "%s", ctx->name);
	return callup(ctx, data, datalen);
}

static code_t write_done_stage1(channelhandlerctx_t *ctx,
								void *data, ssize_t datalen)
{
	prlog(LOGD, "%s", ctx->name);
	return callup(ctx, data, datalen);
}

static code_t write_done_stage3(channelhandlerctx_t *ctx,
								void *data, ssize_t datalen)
{
	prlog(LOGD, "%s", ctx->name);
	return callup(ctx, data, datalen);
}

void setup_echo_channel(channel_t *channel)
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
