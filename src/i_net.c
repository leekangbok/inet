#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include <i_mem.h>
#include <i_net.h>
#include <i_net_tcp.h>
#include <i_net_udp.h>
#include <i_print.h>

typedef struct {
	struct ll_head ll;
	server_t *server;
	void (*signal_cb)(server_t *server, int signum);
} signal_handle_t;

static uv_signal_t uvsignals[MAXSIGNUM];

static LL_HEAD(servers);

static int init_count = 0;
static pthread_mutex_t init_lock;
static pthread_cond_t init_cond;

unsigned long call_next = 0;
#define NUM_WORKERS 8

static server_worker_t workers[NUM_WORKERS];

static void signal_cb(uv_signal_t *handle, int signum)
{
	struct ll_head *head = handle->data;
	signal_handle_t *signal_handle;

	prlog(LOGD, "signal(%d) received.", signum);

	switch (signum) {
	case SIGPIPE:
		prlog(LOGD, "SIGPIPE received.");
		break;
	default:
		ll_for_each_entry(signal_handle, head, ll)
			signal_handle->signal_cb(signal_handle->server, signum);
		break;
	}
}

static int signal_setup(server_t *server, uv_loop_t *uvloop)
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
		prlog(LOGD, "'%s' server signal(%d) add handler.",
			  server->config.name, i);
	}
	return 1;
}

static void expire_timer(uv_timer_t *handle)
{
	prlog(LOGD, "amount of alloc: %zu bytes", iamount_of());
}

static server_t predefined_server[] = {
	{ .config = {
					.servertype = "tcp_ip4",
				},
	.setup_server = setup_tcp_server,},
	{ .config = {
					.servertype = "udp_ip4",
				},
	.setup_server = setup_udp_server,},

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

static calllater_t *create_calllater(void)
{
	calllater_t *cl = icalloc(sizeof(calllater_t));

	INIT_LL_HEAD(&cl->callbacks);

	return cl;
}

calllater_t *add_calllater(calllater_t *cl, call_later f, void *data,
						   data_destroy_f df)
{
	callback_t *cb = icalloc(sizeof(callback_t));

	cb->f = f; cb->data = data; cb->data_destroy = df;

	ll_add_tail(&cb->ll, &cl->callbacks);

	return cl;
}

void run_calllater(calllater_t *cl, int status)
{
	callback_t *cb, *_cb;

	ll_for_each_entry_safe(cb, _cb, &cl->callbacks, ll) {
		cb->f(cl, cb->data, status);
		ll_del(&cb->ll);
		ifree(cb);
	}
}

calllater_t *create_write_req(void *data, ssize_t datalen)
{
	calllater_t *cl = create_calllater();

	cl->write.data = data;
	cl->write.datalen = datalen;

	return cl;
}

static code_t def_outbound_handler(channelhandlerctx_t *ctx, void *data,
								   ssize_t datalen)
{
	calllater_t *cl = (calllater_t *)data;
	int status = UV_ENOTCONN;

	if (cl == NULL || datalen <= 0)
		return SUCCESS;

	hold_channel(ctx->mychannel);
	cl->write.channel = ctx->mychannel;
	cl->write.buf = uv_buf_init(cl->write.data, cl->write.datalen);

	switch (ctx->mychannel->server->servertype) {
	case SERVERTYPE_TCP:
		if (!uv_is_closing(&ctx->mychannel->h.handle)) {
			status = uv_write(&cl->write.req.write, &ctx->mychannel->h.stream,
							  &cl->write.buf, 1, done_stream_write);
		}
		if (status != 0) {
			done_stream_write(&cl->write.req.write, status);
		}
		break;
	case SERVERTYPE_UDP:
		if (!uv_is_closing(&ctx->myserver->h.handle)) {
			status = uv_udp_send(&cl->write.req.udp_write,
								 &ctx->myserver->h.udp, &cl->write.buf, 1,
								 &ctx->mychannel->conn, done_datagram_send);
		}
		if (status != 0) {
			done_datagram_send(&cl->write.req.udp_write, status);
		}
		break;
	default:
		break;
	}
	return SUCCESS;
}

static code_t def_inbound_handler(channelhandlerctx_t *ctx, void *data,
								  ssize_t datalen)
{
	if (datalen > 0)
		ifree(data);
	return SUCCESS;
}

static void set_def_inbound_handler(channelhandler_t *channelhandler)
{
	channelhandler->ctx[EVENT_ACTIVE].handler = channelhandler;
	channelhandler->ctx[EVENT_ACTIVE].operation = def_inbound_handler;
	channelhandler->ctx[EVENT_IDLE].handler = channelhandler;
	channelhandler->ctx[EVENT_IDLE].operation = def_inbound_handler;
	channelhandler->ctx[EVENT_READ].handler = channelhandler;
	channelhandler->ctx[EVENT_READ].operation = def_inbound_handler;
	channelhandler->ctx[EVENT_READCOMPLETE].handler = channelhandler;
	channelhandler->ctx[EVENT_READCOMPLETE].operation = def_inbound_handler;
	channelhandler->ctx[EVENT_ERROR].handler = channelhandler;
	channelhandler->ctx[EVENT_ERROR].operation = def_inbound_handler;
	channelhandler->ctx[EVENT_ERRORCOMPLETE].handler = channelhandler;
	channelhandler->ctx[EVENT_ERRORCOMPLETE].operation = def_inbound_handler;
}

static void set_def_outbound_handler(channelhandler_t *channelhandler)
{
	channelhandler->ctx[EVENT_WRITE].handler = channelhandler;
	channelhandler->ctx[EVENT_WRITE].operation = def_outbound_handler;
	channelhandler->ctx[EVENT_WRITECOMPLETE].handler = channelhandler;
	channelhandler->ctx[EVENT_WRITECOMPLETE].operation = def_outbound_handler;
}

static code_t init_channelpipeline(channelpipeline_t *channelpipeline,
								   size_t numofhandlers)
{
	channelpipeline->handler = icalloc((numofhandlers + 2) * \
									   sizeof(channelhandler_t));
	channelpipeline->numofhandler = 2;

	set_def_inbound_handler(&channelpipeline->handler[1]);
	channelpipeline->handler[1].pipeline = channelpipeline;

	set_def_outbound_handler(&channelpipeline->handler[0]);
	channelpipeline->handler[0].pipeline = channelpipeline;

	return SUCCESS;
}

code_t set_channel_data(channel_t *channel,
						void *data, data_destroy_f data_destroy)
{
	if (channel->data) {
		if (channel->data_destroy)
			channel->data_destroy(channel->data);
		else
			ifree(channel->data);
	}

	channel->data = data;
	channel->data_destroy = data_destroy;

	return SUCCESS;
}

channel_t *hold_channel(channel_t *channel)
{
	channel->refs++;
	return channel;
}

void release_channel(channel_t *channel)
{
	int i;

	channel->refs--;

	assert(channel->refs >= 0);

	prlog(LOGD, "Channel(%p) - refs: %d", channel, channel->refs);

	if (channel->refs > 0)
		return;

	prlog(LOGD, "Channel destroy.");

	for (i = 0; i < channel->pipeline.numofhandler; i++) {
		channelhandler_t *handler = channel->pipeline.handler + i;
		if (handler->data) {
			if (handler->data_destroy)
				handler->data_destroy(handler->data);
			else
				ifree(handler->data);
			handler->data = NULL;
		}
	}
	ifree(channel->pipeline.handler);
	if (channel->data) {
		if (channel->data_destroy)
			channel->data_destroy(channel->data);
		else
			ifree(channel->data);
		channel->data = NULL;
	}

	if (!ll_empty(&channel->ll)) {
		pthread_spin_lock(&channel->server->channels_spinlock);
		ll_del(&channel->ll);
		pthread_spin_unlock(&channel->server->channels_spinlock);
	}
	ifree(channel);
}

int channel_fd(channel_t *channel)
{
	int fd, ret;

	ret = uv_fileno(&channel->h.handle, &fd);
	if (ret < 0)
		return ret;
	return fd;
}

channel_t *create_channel(void *data, data_destroy_f data_destroy)
{
	channel_t *channel = icalloc(sizeof(channel_t));

	INIT_LL_HEAD(&channel->queueworks);
	INIT_LL_HEAD(&channel->ll);

	channel->pipeline.channel = channel;

	set_channel_data(channel, data, data_destroy);

	return channel;
}

code_t callup(channelhandlerctx_t *ctx, void *data, ssize_t datalen)
{
	return ctx->next_ctx->operation(ctx->next_ctx, data, datalen);
}

static void locate_handler_context(channelhandler_t *handler,
								   channelhandlerctx_t *curr,
								   channelhandlerctx_t *prev,
								   channelhandlerctx_t *next,
								   channelhandler_f operation)
{
	curr->handler = handler;
	curr->operation = operation ? operation : callup;
	curr->next_ctx = next;
	prev->next_ctx = curr;
}

channelhandler_t *find_channelhandler(channel_t *channel, const char *name)
{
	int i;

	for (i = 0; i < channel->pipeline.numofhandler; i++) {
		if (strstr((channel->pipeline.handler + i)->name, name)) {
			return channel->pipeline.handler + i;
		}
	}
	return NULL;
}

code_t add_channelhandlers(channel_t *channel, channelhandlers_t handlers[],
						   size_t numofhandlers)
{
	channelpipeline_t *channelpipeline = &channel->pipeline;
	channelhandler_t *curr, *prev, *next;
	size_t i;

	init_channelpipeline(channelpipeline, numofhandlers);

	for (i = 0; i < numofhandlers; i++) {
		next = channelpipeline->handler + channelpipeline->numofhandler;
		curr = next - 1;
		prev = curr - 1;

		memcpy(next, curr, sizeof(channelhandler_t));

		snprintf(curr->name, HANDLER_NAME_MAX, "%s", handlers[i].name);
		curr->data = handlers[i].data;
		curr->data_destroy = handlers[i].data_destroy;
		curr->pipeline = channelpipeline;

		sprintf(curr->ctx[EVENT_ACTIVE].name, "%s>A", curr->name);
		locate_handler_context(curr,
							   &curr->ctx[EVENT_ACTIVE],
							   &prev->ctx[EVENT_ACTIVE],
							   &next->ctx[EVENT_ACTIVE],
							   handlers[i].active);
		sprintf(curr->ctx[EVENT_IDLE].name, "%s>I", curr->name);
		locate_handler_context(curr,
							   &curr->ctx[EVENT_IDLE],
							   &prev->ctx[EVENT_IDLE],
							   &next->ctx[EVENT_IDLE],
							   handlers[i].idle);
		sprintf(curr->ctx[EVENT_READ].name, "%s>R", curr->name);
		locate_handler_context(curr,
							   &curr->ctx[EVENT_READ],
							   &prev->ctx[EVENT_READ],
							   &next->ctx[EVENT_READ],
							   handlers[i].read);
		sprintf(curr->ctx[EVENT_READCOMPLETE].name, "%s>RC", curr->name);
		locate_handler_context(curr,
							   &curr->ctx[EVENT_READCOMPLETE],
							   &prev->ctx[EVENT_READCOMPLETE],
							   &next->ctx[EVENT_READCOMPLETE],
							   handlers[i].read_complete);
		sprintf(curr->ctx[EVENT_WRITE].name, "%s>W", curr->name);
		locate_handler_context(curr,
							   &curr->ctx[EVENT_WRITE],
							   &next->ctx[EVENT_WRITE],
							   &prev->ctx[EVENT_WRITE],
							   handlers[i].write);
		sprintf(curr->ctx[EVENT_WRITECOMPLETE].name, "%s>WC", curr->name);
		locate_handler_context(curr,
							   &curr->ctx[EVENT_WRITECOMPLETE],
							   &next->ctx[EVENT_WRITECOMPLETE],
							   &prev->ctx[EVENT_WRITECOMPLETE],
							   handlers[i].write_complete);
		sprintf(curr->ctx[EVENT_ERROR].name, "%s>E", curr->name);
		locate_handler_context(curr,
							   &curr->ctx[EVENT_ERROR],
							   &prev->ctx[EVENT_ERROR],
							   &next->ctx[EVENT_ERROR],
							   handlers[i].error);
		sprintf(curr->ctx[EVENT_ERRORCOMPLETE].name, "%s>EC", curr->name);
		locate_handler_context(curr,
							   &curr->ctx[EVENT_ERRORCOMPLETE],
							   &prev->ctx[EVENT_ERRORCOMPLETE],
							   &next->ctx[EVENT_ERRORCOMPLETE],
							   handlers[i].error_complete);
		channelpipeline->numofhandler += 1;
	}
	return SUCCESS;
}

code_t callup_channel(channel_t *channel, int event,
					  void *data, ssize_t datalen)
{
	channelhandlerctx_t *ctx;

	switch (event) {
	case EVENT_ACTIVE:
	case EVENT_IDLE:
	case EVENT_READ:
	case EVENT_READCOMPLETE:
	case EVENT_ERROR:
	case EVENT_ERRORCOMPLETE:
		ctx = &channel->pipeline.handler[0].ctx[event];
		break;
	default:
		ctx = &channel->pipeline.handler[channel->pipeline.numofhandler - 1]\
			  .ctx[event];
		break;
	}

	return callup(ctx, data, datalen);
}

code_t callup_context(channelhandlerctx_t *ctx, int event,
					  void *data, ssize_t datalen)
{
	ctx = &ctx->handler->ctx[event];
	return callup(ctx, data, datalen);
}

static void on_after_working(uv_work_t *req, int status)
{
	/* event loop thread */
	queue_work_t *qwork = containerof(req, queue_work_t, work);
	calllater_t *cl = containerof(qwork, calllater_t, qwork);

	if (qwork->after_work)
		qwork->after_work(cl, qwork->data, status);
	ll_del(&qwork->ll);
	release_channel(qwork->ctx->mychannel);
	ifree(cl);
}

static void on_working(uv_work_t *req)
{
	/* thread from threads pool */
	queue_work_t *qwork = containerof(req, queue_work_t, work);
	calllater_t *cl = containerof(qwork, calllater_t, qwork);

	qwork->on_work(cl, qwork->data, 1);
}

void queue_work(channelhandlerctx_t *ctx, void *data, call_later on_work,
				call_later after_work)
{
	calllater_t *cl = create_calllater();

	cl->qwork.ctx = ctx; cl->qwork.data = data;
	cl->qwork.on_work = on_work; cl->qwork.after_work = after_work;

	ll_add_tail(&cl->qwork.ll, &ctx->mychannel->queueworks);
	hold_channel(ctx->mychannel);
	uv_queue_work(ctx->mychannel->uvloop, &cl->qwork.work,
				  on_working, on_after_working);
}

code_t add_server(server_config_t *config)
{
	int i;

	for (i = 0; predefined_server[i].config.servertype; i++) {
		if (strcmp(config->servertype,
				   predefined_server[i].config.servertype) == 0) {
			server_t *server = icalloc(sizeof(server_t));

			pthread_spin_init(&server->channels_spinlock, PTHREAD_PROCESS_PRIVATE);
			pthread_spin_init(&server->data_spinlock, PTHREAD_PROCESS_PRIVATE);
			INIT_LL_HEAD(&server->channels);

			memcpy(&server->config, config, sizeof(server_config_t));
			server->config.servertype = istrdup(config->servertype);
			server->config.name = istrdup(config->name);
			server->config.bindaddr = istrdup(config->bindaddr);
			server->setup_server = predefined_server[i].setup_server;
			ll_add_tail(&server->ll, &servers);
			return SUCCESS;
		}
	}
	return FAIL;
}

void notify_async_cmd(async_cmd_t *cmd)
{
	unsigned long idx = ++call_next % NUM_WORKERS;
	server_worker_t *me = &workers[idx];

	pthread_spin_lock(&me->cmd_spinlock);
	ll_add_tail(&cmd->ll, &me->new_cmds);
	pthread_spin_unlock(&me->cmd_spinlock);

	uv_async_send(&me->cmd_handle);
}

static void consume_async_cmd(uv_async_t *handle)
{
	LL_HEAD(tmp_list);
	server_worker_t *me = containerof(handle,
									  server_worker_t,
									  cmd_handle);
	async_cmd_t *cmd, *_cmd;

	pthread_spin_lock(&me->cmd_spinlock);
	ll_splice(&me->new_cmds, &tmp_list);
	INIT_LL_HEAD(&me->new_cmds);
	pthread_spin_unlock(&me->cmd_spinlock);

	ll_for_each_entry_safe(cmd, _cmd, &tmp_list, ll) {
		switch (cmd->cmd) {
		case ACMD_NEW_TCP_CONN:
			create_tcp_channel(handle->loop, me, cmd->new_connection.server,
							   cmd->new_connection.fd);
			break;
		default:
			break;
		}
		ll_del(&cmd->ll);
		ifree(cmd);
	}
}

static void worker_thread_f(void *arg)
{
	server_worker_t *me = (server_worker_t *)arg;
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
		server_worker_t *worker = &workers[i];
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

code_t set_server_data(server_t *server,
					   void *data, data_destroy_f data_destroy)
{
	if (server->data) {
		if (server->data_destroy)
			server->data_destroy(server->data);
		else
			ifree(server->data);
	}

	server->data = data;
	server->data_destroy = data_destroy;

	return SUCCESS;
}

int server_fd(server_t *server)
{
	int fd, ret;

	ret = uv_fileno(&server->h.handle, &fd);
	if (ret < 0)
		return ret;
	return fd;
}

code_t start_server(void)
{
	uv_signal_t sigpipe;
	uv_timer_t h_timer;
	server_t *server;

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
	uv_timer_start(&h_timer, expire_timer, 1000, 1000);

	uv_run(uv_default_loop(), UV_RUN_DEFAULT);

	return 1;
}
