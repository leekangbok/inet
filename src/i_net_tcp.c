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
	channel_t *channel = containerof(handle, channel_t, h);

	release_channel(channel);
}

static void on_channel_idle_timer_close(uv_handle_t *handle)
{
	channel_t *channel = containerof(handle, channel_t, idle_timer_handle);

	release_channel(channel);
}

static void on_channel_shutdown(uv_shutdown_t *req, int status)
{
	channel_t *channel = containerof(req, channel_t, shutdown_handle);

	if (status < 0)
		prlog(LOGC, "Channel Shutdown error: %s", uv_err_name(status));

	callup_channel(channel, EVENT_ERROR, req->data, -1);

	uv_read_stop(&channel->h.stream);
	uv_timer_stop(&channel->idle_timer_handle);

	if (!uv_is_closing(&channel->h.handle))
		uv_close(&channel->h.handle, on_channel_close);
	if (!uv_is_closing((uv_handle_t *)&channel->idle_timer_handle))
		uv_close((uv_handle_t *)&channel->idle_timer_handle,
				 on_channel_idle_timer_close);
}

void channel_shutdown(channel_t *channel, code_t icode)
{
	queue_work_t *qwork, *_qwork;

	prlog(LOGD, "Channel shutdown(icode: %d).", icode);

	ll_for_each_entry_safe(qwork, _qwork, &channel->queueworks, ll) {
		ll_del_init(&qwork->ll);
		uv_cancel((uv_req_t *)&qwork->work);
	}

	channel->shutdown_handle.data = (void *)icode;
	uv_shutdown(&channel->shutdown_handle, &channel->h.stream,
				on_channel_shutdown);
}

static void channel_idle_timer_expire(uv_timer_t *handle)
{
	channel_t *channel = containerof(handle, channel_t, idle_timer_handle);

	--channel->idle_timeout;
	if (channel->server->config.idle_timeout && (channel->idle_timeout == 0)) {
		callup_channel(channel, EVENT_ERROR, (void *)(long)TIMEOUT, -1);
		channel_shutdown(channel, TIMEOUT);
	}
	else
		callup_channel(channel, EVENT_IDLE,
					   (void *)(long)(channel->idle_timeout), 0);
}

void channel_idle_timer_reset(channel_t *channel)
{
	channel->idle_timeout = channel->server->config.idle_timeout;
}

static void origin_conn_closed(uv_handle_t *handle)
{
	async_cmd_t *cmd = containerof(handle, async_cmd_t, new_connection.tcp);

	cmd->cmd = ACMD_NEW_TCP_CONN;
	notify_async_cmd(cmd);
}

static void on_connect_client(uv_stream_t *stream, int status)
{
	server_t *server = containerof(stream, server_t, h);
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
	channel_t *channel = containerof(stream, channel_t, h);

	if (datalen > 0) {
		/* idle timer reset */
		channel_idle_timer_reset(channel);
		callup_channel(channel, EVENT_READ, buf->base, datalen);
		callup_channel(channel, EVENT_READCOMPLETE, NULL, 0);
		return;
	}
	if (datalen == UV_EOF) {
	}
	if (datalen < 0) {
	}

	channel_shutdown(channel, PEERCLOSED);
	ifree(buf->base);
}

void done_stream_write(uv_write_t *req, int status)
{
	calllater_t *cl = containerof(req, calllater_t, write.req.write);
	channel_t *channel = cl->write.channel;

	if (status)
		prlog(LOGC, "Write error %s.", uv_err_name(status));
	else
		channel_idle_timer_reset(channel);

	run_calllater(cl, status);

	callup_channel(channel, EVENT_WRITECOMPLETE, (void *)(long)status, -1);

	ifree(cl->write.data);
	ifree(cl);

	release_channel(channel);
}

int create_tcp_channel(uv_loop_t *uvloop, server_worker_t *me,
					   server_t *server, int fd)
{
	channel_t *channel = create_channel(NULL, NULL);

	assert(0 == uv_tcp_init(uvloop, &channel->h.tcp));
	assert(0 == uv_tcp_open(&channel->h.tcp, fd));

	channel->server = server;
	channel->uvloop = uvloop;

	hold_channel(channel);
	hold_channel(channel);

	server->config.setup_channel(channel);

	uv_timer_init(uvloop, &channel->idle_timer_handle);
	channel_idle_timer_reset(channel);
	assert(0 == uv_timer_start(&channel->idle_timer_handle,
							   channel_idle_timer_expire,
							   1000,
							   1000));

	pthread_spin_lock(&server->channels_spinlock);
	ll_add_tail(&channel->ll, &server->channels);
	pthread_spin_unlock(&server->channels_spinlock);

	prlog(LOGD, "New Channel(%p) Thread(%lu), FD(%d), Server(%s)",
		  channel, me->thread_id, fd, server->config.name);

	callup_channel(channel, EVENT_ACTIVE, NULL, 0);

	uv_read_start(&channel->h.stream, uvbuf_alloc, on_data);

	return 1;
}

int setup_tcp_server(server_t *server, uv_loop_t *uvloop)
{
	server->uvloop = uvloop;
	server->data = NULL;
	assert(0 == uv_ip4_addr(server->config.bindaddr,
							server->config.bindport, &server->addr.addr4));
	assert(0 == uv_tcp_init(uvloop, &server->h.tcp));
	assert(0 == uv_tcp_bind(&server->h.tcp, &server->addr.addr, 0));

	if (server->config.setup_server)
		server->config.setup_server(server);

	assert(0 == uv_listen(&server->h.stream, 128, on_connect_client));

	server->servertype = SERVERTYPE_TCP;

	prlog(LOGD, "%s server(%p) listening on %s:%d(%s)",
		  server->config.name, server,
		  server->config.bindaddr, server->config.bindport,
		  server->config.servertype);
	return 1;
}
