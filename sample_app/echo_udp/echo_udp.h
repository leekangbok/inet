#ifndef _ECHO_UDP_H
#define _ECHO_UDP_H

#include <i_net.h>
#include <i_net_tcp.h>

void setup_echo_udp_server(server_t *server);
void setup_echo_udp_channel(channel_t *channel);

#endif
