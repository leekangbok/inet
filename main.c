#include <stdio.h>
#include <string.h>

#include <i_mem.h>
#include <i_net.h>

#include <echo/echo.h>
#include <echo_udp/echo_udp.h>

int main(int argc, char *argv[])
{
	server_config_t config;

	//daemon(0, 0);

	iallocator_init();

	memset(&config, 0x00, sizeof(server_config_t));
	config.name = "127.0.0.1@10000";
	config.servertype = "tcp_ip4";
	config.bindaddr = "127.0.0.1";
	config.bindport = 10000;
	config.idle_timeout = 2;
	config.setup_server = setup_echo_server;
	config.setup_channel = setup_echo_channel;
	add_server(&config);

	memset(&config, 0x00, sizeof(server_config_t));
	config.name = "127.0.0.1@10001";
	config.servertype = "tcp_ip4";
	config.bindaddr = "127.0.0.1";
	config.bindport = 10001;
	config.idle_timeout = 2;
	config.setup_server = setup_echo_server;
	config.setup_channel = setup_echo_channel;
	add_server(&config);

	memset(&config, 0x00, sizeof(server_config_t));
	config.name = "127.0.0.1@10000";
	config.servertype = "udp_ip4";
	config.bindaddr = "127.0.0.1";
	config.bindport = 10000;
	config.setup_server = setup_echo_udp_server;
	config.setup_channel = setup_echo_udp_channel;
	add_server(&config);

	memset(&config, 0x00, sizeof(server_config_t));
	config.name = "127.0.0.1@10001";
	config.servertype = "udp_ip4";
	config.bindaddr = "127.0.0.1";
	config.bindport = 10001;
	config.setup_server = setup_echo_udp_server;
	config.setup_channel = setup_echo_udp_channel;
	add_server(&config);

	start_server();

	return 1;
}
