#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include <i_mem.h>
#include <i_net.h>
#include <i_net_tcp.h>

typedef struct {
	struct ll_head ll;
	iserver_t *server;
	void (*signal_cb)(iserver_t *server, int signum);
} signal_handle_t;

static uv_signal_t uvsignals[MAXSIGNUM];

static LL_HEAD(servers);

static int init_count = 0;
static pthread_mutex_t init_lock;
static pthread_cond_t init_cond;

unsigned long call_next = 0;
#define NUM_WORKERS 8

static iserver_worker_t workers[NUM_WORKERS];

static void signal_cb(uv_signal_t *handle, int signum)
{
	struct ll_head *head = handle->data;
	signal_handle_t *signal_handle;

	printf("signal(%d) received.\n", signum);

	switch (signum) {
	case SIGPIPE:
		printf("SIGPIPE received.\n");
		break;
	default:
		ll_for_each_entry(signal_handle, head, ll) {
			signal_handle->signal_cb(signal_handle->server, signum);
		}
		break;
	}
}

static int signal_setup(iserver_t *server, uv_loop_t *uvloop)
{
	int i;
	struct ll_head *head = NULL;
	uv_signal_t *handle;
	signal_handle_t *signal_handle;

	for (i = 0; i < MAXSIGNUM; i++) {
		if (server->config.signals_cb[i] == NULL)
			continue;
		handle = &uvsignals[i];
		if (handle->signum)
			goto next;
		uv_signal_init(uvloop, handle);
		handle->data = icalloc(sizeof(struct ll_head));
		assert(handle->data);
		head = handle->data;
		INIT_LL_HEAD(head);
		uv_signal_start(handle, signal_cb, i);
next:
		signal_handle = icalloc(sizeof(signal_handle_t));
		assert(signal_handle);
		signal_handle->signal_cb = server->config.signals_cb[i];
		signal_handle->server = server;
		ll_add_tail(&signal_handle->ll, head);
		printf("'%s' server signal(%d) add handler.\n", server->config.name, i);
	}
	return 1;
}

static void expire_gh_timer(uv_timer_t *handle)
{
	printf("amount of alloc: %zu bytes\n", iamount_of());
}

static iserver_t predefined_server[] = {
	{ .config = {
					.servertype = "tcp_ip4",
				},
	.setup_server = setup_tcp_server,},
	{ .config = {
					.servertype = NULL,
				},
	.setup_server = NULL,},
};

void uvbuf_alloc(uv_handle_t *handle, size_t suggested_size,
				 uv_buf_t *buf)
{
	buf->base = imalloc(suggested_size);
	buf->len = suggested_size;
}

static icode_t pass_handler(ichannel_t *channel, ichannelhandler_ctx_t *ctx,
							void *data, ssize_t datalen)
{
	return ctx->next(channel, ctx->next_ctx, data, datalen);
}

static icode_t def_outbound_handler(ichannel_t *channel,
									ichannelhandler_ctx_t *ctx,
									void *data, ssize_t datalen)
{
	printf("def_outbound_handler\n");
	return ISUCCESS;
}

static icode_t def_inbound_handler(ichannel_t *channel,
								   ichannelhandler_ctx_t *ctx,
								   void *data, ssize_t datalen)
{
	printf("def_inbound_handler\n");
	ifree(data);
	return ISUCCESS;
}

static void set_def_inbound_handler(ichannelhandler_t *channelhandler)
{
	memset(channelhandler, 0x00, sizeof(ichannelhandler_t));

	channelhandler->ctx[IEVENT_ACTIVE].operation = def_inbound_handler;
	channelhandler->ctx[IEVENT_READ].operation = def_inbound_handler;
	channelhandler->ctx[IEVENT_READCOMPLETE].operation = def_inbound_handler;
	channelhandler->ctx[IEVENT_ERROR].operation = def_inbound_handler;
	channelhandler->ctx[IEVENT_ERRORCOMPLETE].operation = def_inbound_handler;
}

static void set_def_outbound_handler(ichannelhandler_t *channelhandler)
{
	memset(channelhandler, 0x00, sizeof(ichannelhandler_t));

	channelhandler->ctx[IEVENT_WRITE].operation = def_outbound_handler;
	channelhandler->ctx[IEVENT_WRITECOMPLETE].operation = def_outbound_handler;
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
		channel->data_destroy(channel->data);

	channel->data = data;
	channel->data_destroy = data_destroy;

	return ISUCCESS;
}

void destroy_channel(ichannel_t *channel)
{
	int i;

	for (i = 0; i < channel->pipeline.numofhandler; i++) {
		ichannelhandler_t *handler = channel->pipeline.handler + i;
		if (handler->data) {
			handler->data_destroy(handler->data);
			handler->data = NULL;
		}
	}
	ifree(channel->pipeline.handler);
	if (channel->data) {
		channel->data_destroy(channel->data);
		channel->data = NULL;
	}
	ll_del(&channel->ll);
	ifree(channel);
}

ichannel_t *create_channel(void *data, data_destroy_f data_destroy)
{
	ichannel_t *channel = icalloc(sizeof(ichannel_t));

	init_channelpipeline(&channel->pipeline);

	channel->pipeline.channel = channel;
	set_channel_data(channel, data, data_destroy);

	return channel;
}

static void locate_handler_context(ichannelhandler_t *handler,
								   ichannelhandler_ctx_t *curr,
								   ichannelhandler_ctx_t *prev,
								   ichannelhandler_ctx_t *next,
								   channelhandler_f operation)
{
	curr->handler = handler;
	curr->operation = operation ? operation : pass_handler;
	curr->next = next->operation;
	curr->next_ctx = next;
	prev->next = curr->operation;
	prev->next_ctx = curr;
}

icode_t add_channelhandler(ichannel_t *channel, const char *name,
						   channelhandler_f active,
						   channelhandler_f read,
						   channelhandler_f read_complete,
						   channelhandler_f write,
						   channelhandler_f write_complete,
						   channelhandler_f error,
						   channelhandler_f error_complete,
						   void *data, data_destroy_f data_destroy)
{
	ichannelpipeline_t *channelpipeline = &channel->pipeline;
	ichannelhandler_t *curr, *prev, *next;

	channelpipeline->handler = irealloc(channelpipeline->handler,
										(channelpipeline->numofhandler + 1) * \
										sizeof(ichannelhandler_t));

	next = channelpipeline->handler + channelpipeline->numofhandler;
	curr = next - 1;
	prev = curr - 1;

	memcpy(next, curr, sizeof(ichannelhandler_t));

	snprintf(curr->name, IHANDLER_NAME_MAX - 1, "%s", name);
	curr->data = data;
	curr->data_destroy = data_destroy;
	curr->pipeline = channelpipeline;

	sprintf(curr->ctx[IEVENT_ACTIVE].name, "%s::ACTIVE", curr->name);
	locate_handler_context(curr,
						   &curr->ctx[IEVENT_ACTIVE],
						   &prev->ctx[IEVENT_ACTIVE],
						   &next->ctx[IEVENT_ACTIVE],
						   active);
	sprintf(curr->ctx[IEVENT_READ].name, "%s::READ", curr->name);
	locate_handler_context(curr,
						   &curr->ctx[IEVENT_READ],
						   &prev->ctx[IEVENT_READ],
						   &next->ctx[IEVENT_READ],
						   read);
	sprintf(curr->ctx[IEVENT_READCOMPLETE].name, "%s::READCOMPLETE", curr->name);
	locate_handler_context(curr,
						   &curr->ctx[IEVENT_READCOMPLETE],
						   &prev->ctx[IEVENT_READCOMPLETE],
						   &next->ctx[IEVENT_READCOMPLETE],
						   read_complete);
	sprintf(curr->ctx[IEVENT_WRITE].name, "%s::WRITE", curr->name);
	locate_handler_context(curr,
						   &curr->ctx[IEVENT_WRITE],
						   &next->ctx[IEVENT_WRITE],
						   &prev->ctx[IEVENT_WRITE],
						   write);
	sprintf(curr->ctx[IEVENT_WRITECOMPLETE].name, "%s::WRITECOMPLETE", curr->name);
	locate_handler_context(curr,
						   &curr->ctx[IEVENT_WRITECOMPLETE],
						   &next->ctx[IEVENT_WRITECOMPLETE],
						   &prev->ctx[IEVENT_WRITECOMPLETE],
						   write_complete);
	sprintf(curr->ctx[IEVENT_ERROR].name, "%s::ERROR", curr->name);
	locate_handler_context(curr,
						   &curr->ctx[IEVENT_ERROR],
						   &prev->ctx[IEVENT_ERROR],
						   &next->ctx[IEVENT_ERROR],
						   error);
	sprintf(curr->ctx[IEVENT_ERRORCOMPLETE].name, "%s::ERRORCOMPLETE", curr->name);
	locate_handler_context(curr,
						   &curr->ctx[IEVENT_ERRORCOMPLETE],
						   &prev->ctx[IEVENT_ERRORCOMPLETE],
						   &next->ctx[IEVENT_ERRORCOMPLETE],
						   error_complete);
	channelpipeline->numofhandler += 1;

	return ISUCCESS;
}

icode_t fire_pipeline_event(ichannel_t *channel, int event,
							void *data, ssize_t datalen)
{
	ichannelhandler_ctx_t *ctx;

	switch (event) {
	case IEVENT_ACTIVE:
	case IEVENT_READ:
	case IEVENT_READCOMPLETE:
	case IEVENT_ERROR:
	case IEVENT_ERRORCOMPLETE:
		ctx = &channel->pipeline.handler[0].ctx[event];
		break;
	default:
		ctx = &channel->pipeline.handler[channel->pipeline.numofhandler - 1]\
			  .ctx[event];
		break;
	}

	return ctx->next(channel, ctx->next_ctx, data, datalen);
}

icode_t fire_ctx_event(ichannelhandler_ctx_t *ctx,
					   int event, void *data, ssize_t datalen)
{
	ctx = &ctx->handler->ctx[event];
	return ctx->next(ctx->handler->pipeline->channel, ctx->next_ctx,
					 data, datalen);
}

icode_t add_iserver(iserver_config_t *config)
{
	int i;

	for (i = 0; predefined_server[i].config.servertype; i++) {
		if (strcmp(config->servertype,
				   predefined_server[i].config.servertype) == 0) {
			iserver_t *server = icalloc(sizeof(iserver_t));

			INIT_LL_HEAD(&server->channels);

			memcpy(&server->config, config, sizeof(iserver_config_t));
			server->config.servertype = istrdup(config->servertype);
			server->config.name = istrdup(config->name);
			server->config.bindaddr = istrdup(config->bindaddr);
			server->setup_server = predefined_server[i].setup_server;
			ll_add_tail(&server->ll, &servers);
			return ISUCCESS;
		}
	}
	return IFAIL;
}

void notify_async_cmd(async_cmd_t *cmd)
{
	unsigned long idx = ++call_next % NUM_WORKERS;
	iserver_worker_t *me = &workers[idx];

	pthread_spin_lock(&me->cmd_spinlock);
	ll_add_tail(&cmd->ll, &me->new_cmds);
	pthread_spin_unlock(&me->cmd_spinlock);

	uv_async_send(&me->cmd_handle);
}

static void consume_async_cmd(uv_async_t *handle)
{
	LL_HEAD(tmp_list);
	iserver_worker_t *me = icontainer_of(handle,
										 iserver_worker_t,
										 cmd_handle);
	async_cmd_t *cmd, *_cmd;

	pthread_spin_lock(&me->cmd_spinlock);
	ll_splice(&me->new_cmds, &tmp_list);
	INIT_LL_HEAD(&me->new_cmds);
	pthread_spin_unlock(&me->cmd_spinlock);

	ll_for_each_entry_safe(cmd, _cmd, &tmp_list, ll) {
		switch (cmd->cmd) {
		case ACMD_NEW_TCP_CONN:
			setup_tcp_read(handle->loop, me, cmd->new_connection.server,
						   cmd->new_connection.fd);
		default:
			break;
		}
		ll_del(&cmd->ll);
		ifree(cmd);
	}
}

static void worker_thread_f(void *arg)
{
	iserver_worker_t *me = (iserver_worker_t *)arg;
	uv_loop_t *uvloop;

	uvloop = uv_loop_new();
	uv_async_init(uvloop, &me->cmd_handle, consume_async_cmd);

	pthread_mutex_lock(&init_lock);
	init_count++;
	pthread_cond_signal(&init_cond);
	pthread_mutex_unlock(&init_lock);

	uv_run(uvloop, UV_RUN_DEFAULT);
}

static void start_workers(void)
{
	int i;

	for (i = 0; i < NUM_WORKERS; i++) {
		iserver_worker_t *worker = &workers[i];
		worker->index = i;
		pthread_spin_init(&worker->cmd_spinlock, PTHREAD_PROCESS_PRIVATE);
		INIT_LL_HEAD(&worker->new_cmds);
		uv_thread_create(&worker->thread_id, worker_thread_f, worker);
	}
}

static void wait_for_thread_start(int nthreads)
{
	while (init_count < nthreads) {
		pthread_cond_wait(&init_cond, &init_lock);
	}
}

icode_t start_iserver(void)
{
	uv_signal_t sigpipe;
	uv_timer_t h_timer;
	iserver_t *server;

	pthread_mutex_init(&init_lock, NULL);
	pthread_cond_init(&init_cond, NULL);

	start_workers();

	wait_for_thread_start(NUM_WORKERS);

	ll_for_each_entry(server, &servers, ll)
		server->setup_server(server, uv_default_loop());

	ll_for_each_entry(server, &servers, ll)
		signal_setup(server, uv_default_loop());

	uv_signal_init(uv_default_loop(), &sigpipe);
	uv_signal_start(&sigpipe, signal_cb, SIGPIPE);

	uv_timer_init(uv_default_loop(), &h_timer);
	uv_timer_start(&h_timer, expire_gh_timer, 1000, 1000);

	uv_run(uv_default_loop(), UV_RUN_DEFAULT);

	return 1;
}
