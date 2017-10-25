/*
 * \brief  Linux file as a block session
 * \author Josef Soentgen
 * \date   2017-07-05
 */

/*
 * Copyright (C) 2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* Genode includes */
#include <base/attached_rom_dataspace.h>
#include <base/component.h>
#include <base/heap.h>
#include <base/log.h>
#include <block/component.h>
#include <block/driver.h>
#include <util/string.h>

/* libc includes */
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdio.h> /* perror */


static bool xml_attr_ok(Genode::Xml_node node, char const *attr)
{
	return node.attribute_value(attr, false);
}


class Lx_block_driver : public Block::Driver
{
	private:

		Genode::Env &_env;

		Block::sector_t            _block_count {   0 };
		Genode::size_t             _block_size  { 512 };
		Block::Session::Operations _block_ops;

		int _fd { -1 };

	public:

		struct Could_not_open_file : Genode::Exception { };

		Lx_block_driver(Genode::Env &env, Genode::Xml_node config)
		: Block::Driver(env.ram()), _env(env)
		{
			Genode::String<256> file;
			try {
				config.attribute("file").value(&file);
			} catch (...) {
				Genode::error("mandatory file attribute missing");
				throw Could_not_open_file();
			}

			try {
				Genode::Number_of_bytes bytes;
				config.attribute("block_size").value(&bytes);
				_block_size = bytes;
			} catch (...) {
				Genode::warning("block size missing, assuming 512b");
			}

			bool const writeable = xml_attr_ok(config, "writeable");

			struct stat st;
			if (stat(file.string(), &st)) {
				perror("stat");
				throw Could_not_open_file();
			}

			_block_count = st.st_size / _block_size;

			/* open file */
			_fd = open(file.string(), writeable ? O_RDWR : O_RDONLY);
			if (_fd == -1) {
				perror("open");
				throw Could_not_open_file();
			}

			_block_ops.set_operation(Block::Packet_descriptor::READ);
			if (writeable) {
				_block_ops.set_operation(Block::Packet_descriptor::WRITE);
			}

			Genode::log("Provide '", file.string(), "' as block device "
			            "block_size: ", _block_size, " block_count: ",
			            _block_count, " writeable: ", writeable ? "yes" : "no");
		}

		~Lx_block_driver() { close(_fd); }


		/*****************************
		 ** Block::Driver interface **
		 *****************************/

		Genode::size_t      block_size() override { return _block_size;  }
		Block::sector_t    block_count() override { return _block_count; }
		Block::Session::Operations ops() override { return _block_ops;   }

		void read(Block::sector_t           block_number,
		          Genode::size_t            block_count,
		          char                     *buffer,
		          Block::Packet_descriptor &packet) override
		{
			/* range check is done by Block::Driver */
			if (!_block_ops.supported(Block::Packet_descriptor::READ)) {
				throw Io_error();
			}

			off_t const offset = block_number * _block_size;
			size_t const count = block_count * _block_size;

			ssize_t const n = pread(_fd, buffer, count, offset);
			if (n == -1) {
				perror("pread");
				throw Io_error();
			}

			ack_packet(packet);
		}

		void write(Block::sector_t           block_number,
		           Genode::size_t            block_count,
		           char const               *buffer,
		           Block::Packet_descriptor &packet) override
		{
			/* range check is done by Block::Driver */
			if (!_block_ops.supported(Block::Packet_descriptor::WRITE)) {
				throw Io_error();
			}

			off_t const offset = block_number * _block_size;
			size_t const count = block_count * _block_size;

			ssize_t const n = pwrite(_fd, buffer, count, offset);
			if (n == -1) {
				perror("pwrite");
				throw Io_error();
			}

			ack_packet(packet);
		}

		void sync() { }
};


struct Main
{
	Genode::Env  &_env;
	Genode::Heap  _heap { _env.ram(), _env.rm() };

	Genode::Attached_rom_dataspace _config_rom { _env, "config" };

	struct Factory : Block::Driver_factory
	{
		Genode::Constructible<Lx_block_driver> _driver;

		Factory(Genode::Env &env, Genode::Xml_node config)
		{
			_driver.construct(env, config);
		}

		~Factory() { _driver.destruct(); }

		/***********************
		 ** Factory interface **
		 ***********************/

		Block::Driver *create() { return &*_driver; }
		void destroy(Block::Driver *) { }
	} factory { _env, _config_rom.xml() };

	Block::Root root { _env.ep(), _heap, _env.rm(), factory,
	                   xml_attr_ok(_config_rom.xml(), "writeable") };

	Main(Genode::Env &env) : _env(env)
	{
		_env.parent().announce(_env.ep().manage(root));
	}
};


void Component::construct(Genode::Env &env) { static Main main(env); }
