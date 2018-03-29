#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include <i_mem.h>
#include <i_net.h>
#include <i_net_tcp.h>

static LL_HEAD(servers);

unsigned long call_next = 0;
#define NUM_WORKERS 8

typedef struct tmpclient_ {
	struct ll_head ll;
	iserver_t *iserver;
	int fd;
} tmpclient_t;

typedef struct iserver_worker_ {
	int index;
	pthread_spinlock_t new_conn_spinlock;
	struct ll_head new_conns;
	uv_async_t notify_new_conn;
	uv_thread_t thread_id;
} iserver_worker_t;

static iserver_worker_t workers[NUM_WORKERS];

static void expire_gh_timer(uv_timer_t *handle)
{
	printf("amount of alloc: %zu bytes\n", iamount_of());
	uv_async_send(&workers[++call_next % NUM_WORKERS].notify_new_conn);
}

static iserver_t predefined_server[] = {
	{ .config = {
					.servertype = "tcp_ip4",
				},
	.setup_iserver = setup_tcp_server,},
	{ .config = {
					.servertype = NULL,
				},
	.setup_iserver = NULL,},
};

static icode_t pass_handler(ichannel_t *channel,
							ichannelhandler_ctx_t *ctx,
							void **data)
{
	return ctx->next(channel, ctx->next_ctx, data);
}

static icode_t def_outbound_handler(ichannel_t *channel,
									ichannelhandler_ctx_t *ctx,
									void **data)
{
	printf("def_outbound_handler\n");
	return ISUCCESS;
}

static icode_t def_inbound_handler(ichannel_t *channel,
								   ichannelhandler_ctx_t *ctx,
								   void **data)
{
	printf("def_inbound_handler\n");
	return ISUCCESS;
}

static void set_def_inbound_handler(ichannelhandler_t *channelhandler)
{
	memset(channelhandler, 0x00, sizeof(ichannelhandler_t));

	channelhandler->ctx[IEVENT_ACTIVE].operation = def_inbound_handler;
	channelhandler->ctx[IEVENT_READ].operation = def_inbound_handler;
}

static void set_def_outbound_handler(ichannelhandler_t *channelhandler)
{
	memset(channelhandler, 0x00, sizeof(ichannelhandler_t));

	channelhandler->ctx[IEVENT_WRITE].operation = def_outbound_handler;
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
		channel->data_destroy(&channel->data);

	channel->data = data;
	channel->data_destroy = data_destroy;

	return ISUCCESS;
}

ichannel_t *create_channel(void *data, data_destroy_f data_destroy)
{
	ichannel_t *channel = icalloc(sizeof(ichannel_t));

	init_channelpipeline(&channel->pipeline);
	set_channel_data(channel, data, data_destroy);

	return channel;
}

static void locate_handler_context(ichannelhandler_ctx_t *curr,
								   ichannelhandler_ctx_t *prev,
								   ichannelhandler_ctx_t *next,
								   channelhandler_f operation)
{
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
	locate_handler_context(&curr->ctx[IEVENT_ACTIVE],
						   &prev->ctx[IEVENT_ACTIVE],
						   &next->ctx[IEVENT_ACTIVE],
						   active);
	sprintf(curr->ctx[IEVENT_READ].name, "%s::READ", curr->name);
	locate_handler_context(&curr->ctx[IEVENT_READ],
						   &prev->ctx[IEVENT_READ],
						   &next->ctx[IEVENT_READ],
						   read);
	sprintf(curr->ctx[IEVENT_WRITE].name, "%s::WRITE", curr->name);
	locate_handler_context(&curr->ctx[IEVENT_WRITE],
						   &next->ctx[IEVENT_WRITE],
						   &prev->ctx[IEVENT_WRITE],
						   write);
	channelpipeline->numofhandler += 1;

	return ISUCCESS;
}

icode_t fire_event(ichannel_t *channel, int ihandler_type, void **data)
{
	ichannelhandler_ctx_t *ctx;
	int i;

	switch (ihandler_type) {
	case IEVENT_ACTIVE:
	case IEVENT_READ:
		ctx = &channel->pipeline.handler[0].ctx[ihandler_type];
		break;
	default:
		i= channel->pipeline.numofhandler - 1;
		ctx = &channel->pipeline.handler[i].ctx[ihandler_type];
		break;
	}

	return ctx->next(channel, ctx->next_ctx, data);
}

icode_t add_iserver(iserver_config_t *config)
{
	int i;

	for (i = 0; predefined_server[i].config.servertype; i++) {
		if (strcmp(config->servertype,
				   predefined_server[i].config.servertype) == 0) {
			iserver_t *iserver = icalloc(sizeof(iserver_t));

			INIT_LL_HEAD(&iserver->ichannels);

			memcpy(&iserver->config, config, sizeof(iserver_config_t));
			iserver->config.servertype = istrdup(config->servertype);
			iserver->config.name = istrdup(config->name);
			iserver->config.bindaddr = istrdup(config->bindaddr);
			iserver->setup_iserver = predefined_server[i].setup_iserver;
			ll_add_tail(&iserver->ll, &servers);
			return ISUCCESS;
		}
	}
	return IFAIL;
}

void notify_new_connection(iserver_t *iserver, int fd)
{
	unsigned long idx = ++call_next % NUM_WORKERS;
	iserver_worker_t *me = &workers[idx];
	tmpclient_t *client = imalloc(sizeof(tmpclient_t));

	client->iserver = iserver;
	client->fd = fd;

	pthread_spin_lock(&me->new_conn_spinlock);
	ll_add_tail(&client->ll, &me->new_conns);
	pthread_spin_unlock(&me->new_conn_spinlock);

	uv_async_send(&me->notify_new_conn);
}

static void new_connection(uv_async_t *handle)
{
	iserver_worker_t *me = icontainer_of(handle,
										 iserver_worker_t, notify_new_conn);
	tmpclient_t *client, *_t;

	pthread_spin_lock(&me->new_conn_spinlock);

	ll_for_each_entry_safe(client, _t, &me->new_conns, ll) {
		ll_del(&client->ll);
		printf("new connection %lu, %d, %s\n", me->thread_id,
			   client->fd, client->iserver->config.name);
		ifree(client);
	}

	pthread_spin_unlock(&me->new_conn_spinlock);
}

static void worker_thread_f(void *arg)
{
	iserver_worker_t *me = (iserver_worker_t *)arg;
	uv_loop_t *uvloop;

	uvloop = uv_loop_new();
	uv_async_init(uvloop, &me->notify_new_conn, new_connection);

	uv_run(uvloop, UV_RUN_DEFAULT);
}

static void start_workers(void)
{
	int i;

	for (i = 0; i < NUM_WORKERS; i++) {
		iserver_worker_t *worker = &workers[i];
		worker->index = i;
		pthread_spin_init(&worker->new_conn_spinlock, PTHREAD_PROCESS_PRIVATE);
		INIT_LL_HEAD(&worker->new_conns);
		uv_thread_create(&worker->thread_id, worker_thread_f, worker);
	}
}

icode_t start_iserver(void)
{
	uv_timer_t h_timer;
	iserver_t *iserver;

	start_workers();

	sleep(2);

	ll_for_each_entry(iserver, &servers, ll)
		iserver->setup_iserver(iserver, uv_default_loop());

	uv_timer_init(uv_default_loop(), &h_timer);
	uv_timer_start(&h_timer, expire_gh_timer, 1000, 1000);

	uv_run(uv_default_loop(), UV_RUN_DEFAULT);
	return 1;
}
