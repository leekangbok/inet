#include <unistd.h>
#include <string.h>
#include <assert.h>

#include <i_mem.h>
#include <i_macro.h>
#include <echo/echo.h>
#include <i_print.h>

typedef struct {
	int hello_timeout;
} echo_stage1_t;

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

static code_t idle_stage1(channelhandlerctx_t *ctx,
						  void *data, ssize_t datalen)
{
	echo_stage1_t *echo_stage1 = ctx->myhandler->data;

	prlog(LOGD, "%s(idle_timeout: %d)", ctx->name, (long)data);

	echo_stage1->hello_timeout--;
	if (echo_stage1->hello_timeout == 0) {
		char *senddata = icalloc(100);

		sprintf(senddata, "%s\n", "hello ping");
		calllater_t *wcl = create_write_req(senddata, strlen(senddata));

		callup_context(ctx, EVENT_WRITE, wcl, sizeof(calllater_t));
		echo_stage1->hello_timeout = 5;
	}

	return callup(ctx, data, datalen);
}

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
	channelhandler_t *stage1_handler = find_channelhandler(ctx->mychannel, "es1");

	assert(stage1_handler);
	((echo_stage1_t *)(stage1_handler->data))->hello_timeout = 5;

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
	echo_stage1_t *echo_stage1 = icalloc(sizeof(*echo_stage1));

	channelhandlers_t handlers[] = {{
		.name = "es1",
			.idle = idle_stage1,
			.read = read_stage1,
			.write = write_stage1,
			.write_complete = write_done_stage1,
			.data = echo_stage1,
	}, {
		.name = "es2",
			.active = active_stage2,
	}, {
		.name = "es3",
			.read = read_stage3,
			.write = write_stage3,
			.write_complete = write_done_stage3,
	}};

	add_channelhandlers(channel, handlers, I_COUNT_OF(handlers));
}
