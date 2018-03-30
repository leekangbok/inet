#ifndef _I_NET_TCP_H
#define _I_NET_TCP_H

#include <uv.h>
#include <i_net.h>

int setup_tcp_server(iserver_t *server, uv_loop_t *uvloop);
int setup_tcp_read(uv_loop_t *uvloop, iserver_worker_t *me,
				   iserver_t *server, int fd);

#endif
