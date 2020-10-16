/*
 * \brief  libc_fifo_pipe test
 * \author Sid Hussmann
 * \date   2019-12-12
 */

/*
 * Copyright (C) 2018-2020 gapfruit AG
 * Copyright (C) 2019 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Genode includes */
#include <base/attached_rom_dataspace.h>
#include <base/log.h>
#include <libc/component.h>
#include <os/reporter.h>
#include <util/string.h>
#include <util/xml_node.h>


/* libc includes */
#include <errno.h>
#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>

namespace Test_fifo_pipe {
	using namespace Genode;
	class Main;
	class Test;
	class Echo;
	static char const*  TEST_DATA_FILENAME  { "/ro/test-data.bin" };
 	static char const*  SEND_FILENAME       { "/dev/send-pipe/in" };
 	static char const*  RECEIVE_FILENAME    { "/dev/receive-pipe/out" };
	enum { BUF_SIZE = 4*1024 };
}

static size_t copy(int src, int dest)
{
	using namespace Genode;
	size_t total = 0;
	char buf[Test_fifo_pipe::BUF_SIZE] { };
	while (true) {
		auto const nr = read(src, buf, sizeof(buf));
		if (nr == 0) {
			break;
		}

		if (nr < 0) {
			int res = errno;
			error((char const *)strerror(res));
			exit(res);
		}

		auto remain = nr;
		auto off = 0;
		while (remain > 0) {
			auto const nw = write(dest, buf+off, remain);
			if (nw < 0 || nw > remain) {
				int res = errno;
				error((char const *)strerror(res));
				exit(res);
			}

			remain -= nw;
			off += nw;
			total += nw;
		}
	}
	return total;
}

class Test_fifo_pipe::Test
{
	private:

		Env                    &_env;
		Expanding_reporter      _init_config         { _env, "config", "init.config" };
		Attached_rom_dataspace  _run_echo_template   { _env, "init_template" };

		pthread_attr_t          _worker_settings;
		pthread_t               _sender_thread       { 0 };
		pthread_t               _receiver_thread     { 0 };

		static void *send_data(void*)
		{
			int send_file { open(SEND_FILENAME, O_WRONLY) };
			if (send_file < 0) {
				error("Cannot open send file ", SEND_FILENAME);
				exit(1);
			}
			int test_data_file { open(TEST_DATA_FILENAME, O_RDONLY) };
			if (test_data_file < 0) {
				error("Cannot open test data file ", TEST_DATA_FILENAME);
				exit(1);
			}
			auto const num { copy(test_data_file, send_file) };
			log("sent ", num, " bytes");
			close(send_file);
			close(test_data_file);
			pthread_exit(NULL);
			return NULL;
		}

		static void *handle_output_data(void*)
		{
			/* test values */
			char test_data[BUF_SIZE] { };
			char receive_buffer[BUF_SIZE] { };

			int receive_file = open(RECEIVE_FILENAME, O_RDONLY);
			if (receive_file < 0) {
				error("Cannot open receive file ", RECEIVE_FILENAME);
				exit(1);
			}
			int test_data_file = open(TEST_DATA_FILENAME, O_RDONLY);
			if (test_data_file < 0) {
				error("Cannot open test data file ", TEST_DATA_FILENAME);
				exit(1);
			}

			size_t total_received_bytes { 0 };
			while (true) {
				/* read test data */
				auto const test_data_num = read(test_data_file, test_data, BUF_SIZE);
				/* read piped data */
				auto const pipe_data_num = read(receive_file, receive_buffer, BUF_SIZE);
				if (pipe_data_num > 0) {
					/* compare piped data to test data */
					auto const diff_offset = Genode::memcmp(test_data, receive_buffer, test_data_num);
					if (pipe_data_num != test_data_num || (0 != diff_offset)) {
						error("writing to pipe failed. Data sent not equal data received. diff_offset=", diff_offset);
						error("total_received_bytes=", total_received_bytes);
						error("pipe_data_num=", pipe_data_num, " test_data_num=", test_data_num);
						throw Exception();
					}
				}
				if (test_data_num < 0 || pipe_data_num < 0) {
					int res = errno;
					error((char const *)strerror(res));
					exit(res);
				}
				total_received_bytes += pipe_data_num;
				if (test_data_num == 0) {
					break;
				}
				if (pipe_data_num == 0) {
					break;
				}
			}
			log("received a total of ", total_received_bytes, " bytes");

			close(test_data_file);
			close(receive_file);
			pthread_exit(NULL);
			return NULL;
		}

		void _write_init_config(Attached_rom_dataspace& rom, unsigned iteration)
		{
			_init_config.generate([&rom, iteration] (Xml_generator& xml) {
				rom.xml().for_each_sub_node([&xml, iteration] (const Xml_node& node) {
					if (node.type() != "start") {
						node.with_raw_node([&xml] (char const *addr, const size_t size) {
						xml.append(addr, size);
					});
					} else {
						auto const name { node.attribute_value("name", Genode::String<128> { }) };
						xml.node("start", [&xml, &node, &name, iteration] ( ) {
							xml.attribute("name", name);
							xml.attribute("version", iteration);

							node.with_raw_content([&xml] (char const *addr, size_t const size) {
								xml.append(addr, size);
							});
						});
					}
				});
			});
		}

	public:

		Test(Env &env) : _env(env)
		{
			_run_echo_template.update();
		}

		~Test() = default;

		void start_threads()
		{
			Libc::with_libc([this] () {
				if (0 != pthread_attr_init(&_worker_settings)) {
					error("error setting thread settings");
					exit(1);
				}
				if (0 != pthread_attr_setdetachstate(&_worker_settings, PTHREAD_CREATE_JOINABLE)) {
					error("error setting thread settings");
					exit(1);
				}
				log("starting thread to send data to pipe");
				if (0 != pthread_create(&_sender_thread, &_worker_settings, send_data, nullptr)) {
					error("error opening connection thread");
					exit(1);
				}
				log("starting thread to receive data from pipe");
				if (0 != pthread_create(&_receiver_thread, &_worker_settings, handle_output_data, nullptr)) {
					error("error opening connection thread");
					exit(1);
				}
			});
		}

		void stop_threads()
		{
			Libc::with_libc([this] () {
				log("joining sender thread ");
				auto const ret_sender = pthread_join(_sender_thread, nullptr);
				if (0 != ret_sender) {
					warning("pthread_join unexpectedly returned "
					        "with ", ret_sender, " (errno=", errno, ")");
				}
				log("joined sender thread");
				log("joining receiver thread ");
				auto const ret_receiver = pthread_join(_receiver_thread, nullptr);
				if (0 != ret_receiver) {
					warning("pthread_join unexpectedly returned "
					        "with ", ret_receiver, " (errno=", errno, ")");
				}
				log("joined receiver thread");
				pthread_attr_destroy(&_worker_settings);
			});
		}

		void start_echo(unsigned iteration)
		{
			log("re-starting echo");
			_write_init_config(_run_echo_template, iteration);
		}

		void access_control()
		{
			log("test access control");
			Libc::with_libc([this] () {
				bool failed { false };
				int send_file { open(SEND_FILENAME, O_RDONLY) };
				if (send_file >= 0) {
					error("should not have read access to ", SEND_FILENAME);
					failed = true;
				}
				int receive_file { open(RECEIVE_FILENAME, O_WRONLY) };
				if (receive_file >= 0) {
					error("should not have write access to ", RECEIVE_FILENAME);
					failed = true;
				}
				if (failed) 
					exit(-1);
			});
		}
};

class Test_fifo_pipe::Echo
{
	public:

		Echo() { }

		void run() const
		{
			Libc::with_libc([] () {
				auto const num { copy(STDIN_FILENO, STDOUT_FILENO) };
				log("piped ", num, " bytes");
			});
		}

		~Echo()
		{
			/* send EOF */
			Libc::with_libc([] () { close(STDOUT_FILENO); });
		}
};

class Test_fifo_pipe::Main
{
	private:

		Env                    &_env;
		Attached_rom_dataspace  _config      { _env, "config" };

	public:

		Main(Env &env) : _env(env)
		{
			/* the type attribute describes whether we are running as test or as echo */
			auto type = _config.xml().attribute_value("type", Genode::String<64> { });
			if (type == "echo") {
				log("echo started");
				Echo echo;
				echo.run();
			} else {
				Test test(_env);
				auto max_iterations { _config.xml().attribute_value("iterations", 1u) };
				log("test started with ", max_iterations, " iterations");
				for (unsigned i = 0; i < max_iterations; ++i) {
					log("--- test iteration ", i, " started ---");
					test.start_echo(i);
					test.start_threads();
					test.stop_threads();
				}
				test.access_control();
				log("--- test succeeded ---");
			}
		}

};


void Libc::Component::construct(Libc::Env& env) { static Test_fifo_pipe::Main main(env); }
