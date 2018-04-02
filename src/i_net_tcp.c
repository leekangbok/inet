#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include <i_mem.h>
#include <i_net.h>
#include <i_print.h>

static void on_channel_close(uv_handle_t *handle)
{
	ichannel_t *channel = icontainer_of(handle, ichannel_t, h);

	channel->refcnt--;
	prlog(LOGD, "on_channel_close - refcnt: %d", channel->refcnt);

	if (channel->refcnt <= 0)
		destroy_channel(channel);
}

static void on_channel_idle_timer_close(uv_handle_t *handle)
{
	ichannel_t *channel = icontainer_of(handle, ichannel_t, idle_timer_handle);

	channel->refcnt--;
	prlog(LOGD, "on_channel_idle_timer_close - refcnt: %d", channel->refcnt);

	if (channel->refcnt <= 0)
		destroy_channel(channel);
}

static void on_channel_shutdown(uv_shutdown_t *req, int status)
{
	ichannel_t *channel = icontainer_of(req, ichannel_t, shutdown_handle);

	if (status < 0) {
		prlog(LOGC, "on_channel_shutdown error: %d", status);
		return;
	}

	fire_pipeline_event(channel, IEVENT_ERROR, req->data, -1);

	uv_read_stop(&channel->h.stream);
	uv_timer_stop(&channel->idle_timer_handle);

	if (!uv_is_closing(&channel->h.handle)) {
		uv_close(&channel->h.handle, on_channel_close);
	}
	if (!uv_is_closing((uv_handle_t *)&channel->idle_timer_handle)) {
		uv_close((uv_handle_t *)&channel->idle_timer_handle,
				 on_channel_idle_timer_close);
	}
}

void channel_shutdown(ichannel_t *channel, icode_t icode)
{
	channel->shutdown_handle.data = (void *)icode;
	uv_shutdown(&channel->shutdown_handle, &channel->h.stream,
				on_channel_shutdown);
}

static void channel_idle_timer_expire(uv_timer_t *handle)
{
	channel_shutdown(icontainer_of(handle, ichannel_t, idle_timer_handle),
					 ITIMEOUT);
}

static void channel_idle_timer_reset(ichannel_t *channel)
{
	assert(0 == uv_timer_start(&channel->idle_timer_handle,
							   channel_idle_timer_expire,
							   channel->server->config.idle_timeout * 1000,
							   0));
}

static void origin_conn_closed(uv_handle_t *handle)
{
	async_cmd_t *cmd = icontainer_of(handle, async_cmd_t, new_connection.tcp);

	cmd->cmd = ACMD_NEW_TCP_CONN;
	notify_async_cmd(cmd);
}

static void on_connect_client(uv_stream_t *stream, int status)
{
	iserver_t *server = icontainer_of(stream, iserver_t, h);
	async_cmd_t *cmd = imalloc(sizeof(async_cmd_t));
	uv_os_fd_t client_fd;

	assert(0 == uv_tcp_init(server->uvloop, &cmd->new_connection.tcp));
	assert(0 == uv_accept(stream, (uv_stream_t *)&cmd->new_connection.tcp));

	uv_fileno((uv_handle_t *)&cmd->new_connection.tcp, &client_fd);
	cmd->new_connection.fd = dup(client_fd);
	cmd->new_connection.server = server;

	uv_close((uv_handle_t *)&cmd->new_connection.tcp, origin_conn_closed);
}

static void on_data(uv_stream_t *stream, ssize_t datalen, const uv_buf_t *buf)
{
	ichannel_t *channel = icontainer_of(stream, ichannel_t, h);

	if (datalen > 0) {
		/* idle timer reset */
		channel_idle_timer_reset(channel);
		fire_pipeline_event(channel, IEVENT_READ, buf->base, datalen);
		fire_pipeline_event(channel, IEVENT_READCOMPLETE, NULL, 0);
		return;
	}
	if (datalen == UV_EOF) {
	}
	if (datalen < 0) {
	}

	channel_shutdown(channel, IPEERCLOSED);
	ifree(buf->base);
}

int setup_tcp_read(uv_loop_t *uvloop, iserver_worker_t *me,
				   iserver_t *server, int fd)
{
	ichannel_t *channel = create_channel(NULL, NULL);

	prlog(LOGD, "new connection %lu, %d, %s", me->thread_id, fd,
		  server->config.name);

	assert(0 == uv_tcp_init(uvloop, &channel->h.tcp));
	assert(0 == uv_tcp_open(&channel->h.tcp, fd));

	channel->server = server;
	channel->uvloop = uvloop;

	channel->refcnt = 2;

	server->config.setup_channel(channel);

	uv_timer_init(uvloop, &channel->idle_timer_handle);
	channel_idle_timer_reset(channel);

	ll_add_tail(&channel->ll, &server->channels);

	fire_pipeline_event(channel, IEVENT_ACTIVE, NULL, 0);

	uv_read_start(&channel->h.stream, uvbuf_alloc, on_data);

	return 1;
}

int setup_tcp_server(iserver_t *server, uv_loop_t *uvloop)
{
	server->uvloop = uvloop;
	server->data = NULL;
	assert(0 == uv_ip4_addr(server->config.bindaddr,
							server->config.bindport, &server->addr.addr4));
	assert(0 == uv_tcp_init(uvloop, &server->h.tcp));
	assert(0 == uv_tcp_bind(&server->h.tcp, &server->addr.addr, 0));

	if (server->config.setup_server)
		server->config.setup_server(server);
	if (server->config.setup_uvhandle)
		server->config.setup_uvhandle(&server->h.handle, "TCPServerBind");

	assert(0 == uv_listen(&server->h.stream, 128, on_connect_client));

	server->servertype = SERVERTYPE_TCP;

	prlog(LOGD, "%s server(%p) listening on %s:%d",
		  server->config.name, server,
		  server->config.bindaddr, server->config.bindport);
	return 1;
}
