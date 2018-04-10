#include <unistd.h>
#include <string.h>
#include <assert.h>

#include <i_mem.h>
#include <i_macro.h>
#include <echo_udp/echo_udp.h>
#include <i_print.h>

static code_t on_after_work(calllater_t *cl, void *data, int status)
{
	prlog(LOGD, "complete(%p), status: %d", cl, status);
	if (status < 0)
		return FAIL;

	{
		int i;

		for (i = 0; i < 100; i++) {
			char *senddata = icalloc(100);

			sprintf(senddata, "%s\n", "hello telnet");
			calllater_t *wcl = create_write_req(senddata, strlen(senddata));

			callup_context(cl->qwork.ctx, EVENT_WRITE, wcl, sizeof(calllater_t));
		}
	}
	return SUCCESS;
}

static code_t on_work(calllater_t *cl, void *data, int status)
{
	prlog(LOGD, "Sleeping...");
	sleep(10);
	return SUCCESS;
}

static code_t idle_stage1(channelhandlerctx_t *ctx,
						  void *data, ssize_t datalen)
{
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

static code_t read_stage3(channelhandlerctx_t *ctx,
						  void *data, ssize_t datalen)
{
	{
		int i;

		for (i = 0; i < 100; i++) {
			char *senddata = icalloc(100);

			sprintf(senddata, "%s\n", "hello telnet");
			calllater_t *wcl = create_write_req(senddata, strlen(senddata));

			callup_context(ctx, EVENT_WRITE, wcl, sizeof(calllater_t));
		}
	}

	calllater_t *cl = create_write_req(data, datalen);
	prlog(LOGD, "%s", ctx->name);
	//return callup(ctx, data, datalen);
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

void setup_echo_udp_server(server_t *server)
{
	int listen_fd = server_fd(server);

	set_server_data(server, NULL, NULL);
}

void setup_echo_udp_channel(channel_t *channel)
{
	int fd = channel_fd(channel);

	channelhandlers_t handlers[] = {{
		.name = "es1",
			.read = read_stage1,
			.write = write_stage1,
			.write_complete = write_done_stage1,
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
