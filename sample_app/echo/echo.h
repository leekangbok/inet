#ifndef _ECHO_H
#define _ECHO_H

#include <i_net.h>
#include <i_net_tcp.h>

void setup_echo_server(server_t *server);
void setup_echo_channel(channel_t *channel);

#endif
