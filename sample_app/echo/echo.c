#include <i_mem.h>

#include <echo/echo.h>
#include <i_print.h>

code_t on_after_work(calllater_t *cl, void *data, int status)
{
	prlog(LOGD, "complete(%p), status: %d", cl, status);
	/*
	calllater_t *c = create_calllater();
	calllater_t *work_req = containerof(req, calllater_t, work);
	channelhandlerctx_t *ctx = req->data;

	prlog(LOGD, "complete(%p)", work_req);
	ifree(work_req);

	c->write.data = icalloc(100);
	sprintf(c->write.data, "%s\n", "hello telnet");
	c->write.datalen = strlen(c->write.data);

	callup_context(ctx, EVENT_WRITE, c, sizeof(calllater_t));

	destroy_channel(ctx->mychannel);
	*/
	return SUCCESS;
}

code_t on_work(calllater_t *cl, void *data, int status)
{
	// "Work"
	//if (random() % 5 == 0) {
	prlog(LOGD, "Sleeping...");
	sleep(3);
	//}
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
	queue_channel_work(ctx, NULL, on_work, on_after_work);
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
	calllater_t *c = create_calllater();

	prlog(LOGD, "%s", ctx->name);

	c->write.data = data;
	c->write.datalen = datalen;

	add_calllater(c, write_and_close, ctx, NULL);

	return callup_context(ctx, EVENT_WRITE, c, sizeof(calllater_t));
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
