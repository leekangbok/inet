#ifndef _I_NET_TCP_H
#define _I_NET_TCP_H

#include <uv.h>
#include <i_net.h>

int setup_tcp_server(iserver_t *iserver, uv_loop_t *uvloop);

#endif
