/*
 * \brief  Crosslink terminal test
 * \author Christian Prochaska
 * \date   2012-05-21
 */

/*
 * Copyright (C) 2012-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#include <base/component.h>
#include <base/sleep.h>
#include <base/thread.h>
#include <terminal_session/connection.h>

namespace Test_terminal_crosslink {

	using namespace Genode;

	class Partner;
	class Client;
	class Server;
	struct Main;

	enum {
		STACK_SIZE          = sizeof(addr_t)*1024,
		TEST_DATA_SIZE      = 4097,
		READ_BUFFER_SIZE    = 8192
	};

	static const char *client_text = "Hello from client.";
	static const char *server_text = "Hello from server, too.";

	static char test_data[TEST_DATA_SIZE];
}


class Test_terminal_crosslink::Partner : public Thread
{
	protected:

		Terminal::Connection _terminal;

		char _read_buffer[READ_BUFFER_SIZE];

		Signal_receiver _sig_rec;
		Signal_context  _sig_ctx;

		void _write_all(void const *buf, Genode::size_t num_bytes)
		{
			Genode::size_t written_bytes = 0;
			char const * const src       = (char const *)buf;

			while (written_bytes < num_bytes) {
				written_bytes += _terminal.write(&src[written_bytes],
				                                 num_bytes - written_bytes);
			}
		}

		void _read_all(void *buf, Genode::size_t buf_size)
		{
			Genode::size_t read_bytes = 0;
			char * const dst = (char *)buf;

			while (read_bytes < buf_size) {
				_sig_rec.wait_for_signal();
				read_bytes += _terminal.read(&dst[read_bytes],
				                             buf_size - read_bytes);
			}
		}

	public:

		Partner(Env &env, char const *name)
		: Thread(env, name, STACK_SIZE),
		  _terminal(env)
		{
			_terminal.read_avail_sigh(_sig_rec.manage(&_sig_ctx));
		}
};


class Test_terminal_crosslink::Client : public Partner
{
	public:

		Client(Env &env) : Partner(env, "client") { }

		void entry()
		{
			log("Short message test");

			/* write client text */

			_write_all(client_text, strlen(client_text) + 1);

			/* read server text */

			_read_all(_read_buffer, strlen(server_text) + 1);

			log("Client received: ", Cstring(_read_buffer));

			if (strcmp(_read_buffer, server_text) != 0) {
				error("Received data is not as expected");
				sleep_forever();
			}

			/* write test data */

			log("Long message test");

			memset(test_data, 5, sizeof(test_data));
			_write_all(test_data, sizeof(test_data));
		}
};


class Test_terminal_crosslink::Server : public Partner
{
	public:

		Server(Env &env) : Partner(env, "server") { }

		void entry()
		{
			/* read client text */

			_read_all(_read_buffer, strlen(client_text) + 1);

			log("Server received: ", Cstring(_read_buffer));

			if (strcmp(_read_buffer, client_text) != 0) {
				error("Received data is not as expected");
				sleep_forever();
			}

			/* write server text */

			_write_all(server_text, strlen(server_text) + 1);

			/* read test data */

			_read_all(_read_buffer, TEST_DATA_SIZE);

			for (size_t i = 0; i < TEST_DATA_SIZE; i++)
				if (_read_buffer[i] != 5) {
					error("Received data is not as expected");
					sleep_forever();
				}

			log("Test succeeded");
		}
};


struct Test_terminal_crosslink::Main
{
	Env &_env;

	Server server { _env };
	Client client { _env };

	Main(Env &env) : _env(env)
	{
		server.start();
		client.start();
	}
};


void Component::construct(Genode::Env &env)
{
	static Test_terminal_crosslink::Main main(env);
}
