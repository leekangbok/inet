#ifndef _I_NET_UDP_H
#define _I_NET_UDP_H

#include <uv.h>
#include <i_net.h>

void done_datagram_send(uv_udp_send_t *req, int status);
int setup_udp_server(server_t *server, uv_loop_t *uvloop);

#endif
