#ifndef _I_NET_TCP_H
#define _I_NET_TCP_H

#include <uv.h>
#include <i_net.h>

void channel_shutdown(channel_t *channel, code_t icode);
int setup_tcp_server(server_t *server, uv_loop_t *uvloop);
int setup_tcp_read(uv_loop_t *uvloop, server_worker_t *me,
				   server_t *server, int fd);

#endif
