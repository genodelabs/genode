/*
 * \brief  Crosslink terminal test
 * \author Christian Prochaska
 * \date   2012-05-21
 */

/*
 * Copyright (C) 2012 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#include <base/printf.h>
#include <base/signal.h>
#include <base/sleep.h>
#include <base/thread.h>
#include <terminal_session/connection.h>

using namespace Genode;


enum {
	STACK_SIZE          = sizeof(addr_t)*1024,
	SERVICE_BUFFER_SIZE = 4096,
	TEST_DATA_SIZE      = 4097,
	READ_BUFFER_SIZE    = 8192
};

static const char *client_text = "Hello from client.";
static const char *server_text = "Hello from server, too.";

static char test_data[TEST_DATA_SIZE];


class Partner : public Thread<STACK_SIZE>
{
	protected:

		Terminal::Connection _terminal;

		char _read_buffer[READ_BUFFER_SIZE];

		Signal_receiver _sig_rec;
		Signal_context  _sig_ctx;

	public:

		Partner(const char *name) : Thread<STACK_SIZE>(name)
		{
			_terminal.read_avail_sigh(_sig_rec.manage(&_sig_ctx));
		}
};


class Client : public Partner
{
	public:

		Client() : Partner("client") { }

		void entry()
		{
			printf("Short message test\n");

			/* write client text */

			_terminal.write(client_text, strlen(client_text) + 1);

			/* read server text */

			_sig_rec.wait_for_signal();
			_terminal.read(_read_buffer, sizeof(_read_buffer));

			printf("Client received: %s\n", _read_buffer);

			if (strcmp(_read_buffer, server_text) != 0) {
				printf("Error: received data is not as expected\n");
				sleep_forever();
			}

			/* write test data */

			printf("Long message test\n");

			memset(test_data, 5, sizeof(test_data));
			_terminal.write(test_data, sizeof(test_data));
		}
};


class Server : public Partner
{
	public:

		Server() : Partner("server") { }

		void entry()
		{
			/* read client text */

			_sig_rec.wait_for_signal();
			_terminal.read(_read_buffer, sizeof(_read_buffer));

			printf("Server received: %s\n", _read_buffer);

			if (strcmp(_read_buffer, client_text) != 0) {
				printf("Error: received data is not as expected\n");
				sleep_forever();
			}

			/* write server text */

			_terminal.write(server_text, strlen(server_text) + 1);

			/* read test data */

			size_t num_read = 0;
			size_t num_read_total = 0;

			do {
				_sig_rec.wait_for_signal();
				num_read = _terminal.read(_read_buffer, sizeof(_read_buffer));
				num_read_total += num_read;

				for (size_t i = 0; i < num_read; i++)
					if (_read_buffer[i] != 5) {
						printf("Error: received data is not as expected\n");
						sleep_forever();
					}
			} while(num_read == SERVICE_BUFFER_SIZE);

			if (num_read_total != TEST_DATA_SIZE) {
				printf("Error: received an unexpected number of bytes\n");
				sleep_forever();
			}

			printf("Test succeeded\n");
		}
};


int main(int, char **)
{
	static Server server;
	static Client client;

	server.start();
	client.start();

	sleep_forever();

	return 0;
}
