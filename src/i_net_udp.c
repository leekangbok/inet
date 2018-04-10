#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include <i_mem.h>
#include <i_net.h>
#include <i_print.h>

void done_datagram_send(uv_udp_send_t *req, int status)
{
	calllater_t *cl = containerof(req, calllater_t, write.req.udp_write);
	channel_t *channel = cl->write.channel;

	if (status)
		prlog(LOGC, "Write error %s.", uv_err_name(status));

	run_calllater(cl, status);

	callup_channel(channel, EVENT_WRITECOMPLETE, (void *)(long)status, -1);

	ifree(cl->write.data);
	ifree(cl);

	release_channel(channel);
}

static void on_data(uv_udp_t *handle, ssize_t datalen, const uv_buf_t *buf,
					const struct sockaddr *addr, unsigned flags)
{
	server_t *server;
	channel_t *channel;

	if (datalen <= 0) {
		ifree(buf->base);
		return;
	}

	server = containerof(handle, server_t, h);
	channel = create_channel(NULL, NULL);

	channel->server = server;
	channel->uvloop = server->uvloop;

	memcpy(&channel->conn, addr, sizeof(struct sockaddr));

	hold_channel(channel);

	server->config.setup_channel(channel);

	pthread_spin_lock(&server->channels_spinlock);
	ll_add_tail(&channel->ll, &server->channels);
	pthread_spin_unlock(&server->channels_spinlock);

	prlog(LOGD, "New Data received(%zubytes) on Server(%s)",
		  datalen, server->config.name);

	callup_channel(channel, EVENT_ACTIVE, NULL, 0);
	callup_channel(channel, EVENT_READ, buf->base, datalen);
	callup_channel(channel, EVENT_READCOMPLETE, NULL, 0);

	release_channel(channel);
}

static void worker_thread_f(void *arg)
{
	server_t *server = arg;
	uv_loop_t *uvloop;

	uvloop = uv_loop_new();

	server->uvloop = uvloop;
	server->data = NULL;
	assert(0 == uv_ip4_addr(server->config.bindaddr,
							server->config.bindport, &server->addr.addr4));
	assert(0 == uv_udp_init(uvloop, &server->h.udp));
	assert(0 == uv_udp_bind(&server->h.udp, &server->addr.addr, 0));

	if (server->config.setup_server)
		server->config.setup_server(server);

	server->servertype = SERVERTYPE_UDP;

	assert(0 == uv_udp_recv_start(&server->h.udp, uvbuf_alloc, on_data));

	prlog(LOGD, "%s server(%p) listening on %s:%d(%s)",
		  server->config.name, server,
		  server->config.bindaddr, server->config.bindport,
		  server->config.servertype);

	uv_run(uvloop, UV_RUN_DEFAULT);
}

int setup_udp_server(server_t *server, uv_loop_t *uvloop)
{
	uv_thread_t thread_id;

	uv_thread_create(&thread_id, worker_thread_f, server);
	return 1;
}
