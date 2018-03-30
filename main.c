#include <stdio.h>
#include <string.h>

#include <i_mem.h>
#include <i_net.h>

#include <echo/echo.h>

int main(int argc, char *argv[])
{
	iserver_config_t config;

	iallocator_init();

	memset(&config, 0x00, sizeof(iserver_config_t));
	config.name = "127.0.0.1@10000";
	config.servertype = "tcp_ip4";
	config.bindaddr = "127.0.0.1";
	config.bindport = 10000;
	config.idle_timeout = 10;
	config.setup_channel = setup_echo_channel;
	add_iserver(&config);

	memset(&config, 0x00, sizeof(iserver_config_t));
	config.name = "127.0.0.1@10001";
	config.servertype = "tcp_ip4";
	config.bindaddr = "127.0.0.1";
	config.bindport = 10001;
	config.idle_timeout = 10;
	config.setup_channel = setup_echo_channel;
	add_iserver(&config);

	start_iserver();

	return 1;
}
