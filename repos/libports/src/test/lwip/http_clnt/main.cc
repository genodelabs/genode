/*
 * \brief  HTTP client test
 * \author Ivan Loskutov
 * \date   2012-12-21
 */

/*
 * Copyright (C) 2012 Ksys Labs LLC
 * Copyright (C) 2012-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Genode includes */
#include <base/attached_rom_dataspace.h>
#include <base/log.h>
#include <libc/component.h>
#include <nic/packet_allocator.h>
#include <timer_session/connection.h>
#include <util/string.h>

extern "C" {
#include <lwip/sockets.h>
#include <lwip/api.h>
#include <netif/etharp.h>
}

#include <lwip_legacy/genode.h>


/**
 * The client simply loops endless,
 * and sends as much 'http get' requests as possible,
 * printing out the response.
 */
void Libc::Component::construct(Libc::Env &env)
{
	using namespace Genode;
	using Address = Genode::String<16>;

	enum { BUF_SIZE = Nic::Packet_allocator::DEFAULT_PACKET_SIZE * 128 };

	static Timer::Connection _timer(env);
	_timer.msleep(2000);
	lwip_tcpip_init();

	uint32_t ip = 0, nm = 0, gw = 0;
	Address serv_addr, ip_addr, netmask, gateway;

	Attached_rom_dataspace config(env, "config");
	Xml_node config_node = config.xml();
	Xml_node libc_node   = env.libc_config();
	try {
		libc_node.attribute("ip_addr").value(&ip_addr);
		libc_node.attribute("netmask").value(&netmask);
		libc_node.attribute("gateway").value(&gateway);
		ip = inet_addr(ip_addr.string());
		nm = inet_addr(netmask.string());
		gw = inet_addr(gateway.string());
	} catch (...) {}
	config_node.attribute("server_ip").value(&serv_addr);

	if (lwip_nic_init(ip, nm, gw, BUF_SIZE, BUF_SIZE)) {
		error("We got no IP address!");
		exit(1);
	}

	for (unsigned trial_cnt = 0, success_cnt = 0; trial_cnt < 10; trial_cnt++)
	{
		_timer.msleep(2000);

		log("Create new socket ...");
		int s = lwip_socket(AF_INET, SOCK_STREAM, 0 );
		if (s < 0) {
			error("no socket available!");
			continue;
		}

		log("Connect to server ...");

		unsigned port = 0;
		try { config_node.attribute("server_port").value(&port); }
		catch (...) {
			error("Missing \"server_port\" attribute.");
			throw Xml_node::Nonexistent_attribute();
		}

		struct sockaddr_in addr;
		addr.sin_port = htons(port);
		addr.sin_family = AF_INET;
		addr.sin_addr.s_addr = inet_addr(serv_addr.string());

		if((lwip_connect(s, (struct sockaddr *)&addr, sizeof(addr))) < 0) {
			error("Could not connect!");
			lwip_close(s);
			continue;
		}

		log("Send request...");

		/* simple HTTP request header */
		static const char *http_get_request =
			"GET / HTTP/1.0\r\nHost: localhost:80\r\n\r\n";

		unsigned long bytes = lwip_send(s, (char*)http_get_request,
		                                Genode::strlen(http_get_request), 0);
		if ( bytes < 0 ) {
			error("couldn't send request ...");
			lwip_close(s);
			continue;
		}

		/* Receive http header and content independently in 2 packets */
		for(int i=0; i<2; i++) {
			char buf[1024];
			ssize_t buflen;
			buflen = lwip_recv(s, buf, 1024, 0);
			if(buflen > 0) {
				buf[buflen] = 0;
				log("Received \"", String<64>(buf), " ...\"");
				;
				if (++success_cnt >= 5) {
					log("Test done");
					env.parent().exit(0);
				}
			} else
				break;
		}

		/* Close socket */
		lwip_close(s);
	}
	log("Test failed");
	env.parent().exit(-1);
}
