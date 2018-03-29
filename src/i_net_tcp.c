#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include <i_mem.h>
#include <i_net.h>

typedef struct tmpclient_ {
	iserver_t *iserver;
	uv_tcp_t tcp;
	int fd;
} tmpclient_t;

static void close_cb(uv_handle_t *handle)
{
	tmpclient_t *client = icontainer_of(handle, tmpclient_t, tcp);

	notify_new_connection(client->iserver, client->fd);
	ifree(client);
}

static void on_connect_client(uv_stream_t *server, int status)
{
	iserver_t *iserver = icontainer_of(server, iserver_t, h);
	tmpclient_t *client = imalloc(sizeof(tmpclient_t));
	uv_os_fd_t client_fd;

	assert(0 == uv_tcp_init(iserver->uvloop, &client->tcp));
	assert(0 == uv_accept(server, (uv_stream_t *)&client->tcp));

	uv_fileno((uv_handle_t *)&client->tcp, &client_fd);
	client->fd = dup(client_fd);
	client->iserver = iserver;

	uv_close((uv_handle_t *)&client->tcp, close_cb);
}

int setup_tcp_server(iserver_t *iserver, uv_loop_t *uvloop)
{
	iserver->uvloop = uvloop;
	iserver->data = NULL;
	assert(0 == uv_ip4_addr(iserver->config.bindaddr,
							iserver->config.bindport, &iserver->addr.addr4));
	assert(0 == uv_tcp_init(uvloop, &iserver->h.tcp));
	assert(0 == uv_tcp_bind(&iserver->h.tcp, &iserver->addr.addr, 0));

	if (iserver->config.setup_iserver)
		iserver->config.setup_iserver(iserver);
	if (iserver->config.setup_uvhandle)
		iserver->config.setup_uvhandle(&iserver->h.handle, "TCPServerBind");

	assert(0 == uv_listen(&iserver->h.stream, 128, on_connect_client));

	printf("%s iserver(%p) listening on %s:%d\n",
		   iserver->config.name, iserver,
		   iserver->config.bindaddr, iserver->config.bindport);
	return 1;
}
