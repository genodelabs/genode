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

		Block::Session::Info const _info;

		typedef Genode::String<256> File_name;

		static File_name _file_name(Genode::Xml_node const &config)
		{
			return config.attribute_value("file", File_name());
		}

		static Block::Session::Info _init_info(Genode::Xml_node const &config)
		{
			Genode::Number_of_bytes const default_block_size(512);

			if (!config.has_attribute("file")) {
				Genode::error("mandatory file attribute missing");
				throw Could_not_open_file();
			}

			struct stat st;
			if (stat(_file_name(config).string(), &st)) {
				perror("stat");
				throw Could_not_open_file();
			}

			if (!config.has_attribute("block_size"))
				Genode::warning("block size missing, assuming ", default_block_size);

			Genode::size_t const block_size =
				config.attribute_value("block_size", default_block_size);

			return {
				.block_size  = block_size,
				.block_count = st.st_size / block_size,
				.align_log2  = Genode::log2(block_size),
				.writeable   = xml_attr_ok(config, "writeable")
			};
		}

		int _fd { -1 };

	public:

		struct Could_not_open_file : Genode::Exception { };

		Lx_block_driver(Genode::Env &env, Genode::Xml_node config)
		:
			Block::Driver(env.ram()),
			_env(env),
			_info(_init_info(config))
		{
			/* open file */
			File_name const file_name = _file_name(config);
			_fd = open(file_name.string(), _info.writeable ? O_RDWR : O_RDONLY);
			if (_fd == -1) {
				Genode::error("open ", file_name.string());
				throw Could_not_open_file();
			}

			Genode::log("Provide '", file_name, "' as block device "
			            "block_size:  ", _info.block_size, " "
			            "block_count: ", _info.block_count, " "
			            "writeable:   ", _info.writeable ? "yes" : "no");
		}

		~Lx_block_driver() { close(_fd); }


		/*****************************
		 ** Block::Driver interface **
		 *****************************/

		Block::Session::Info info() const override { return _info; }

		void read(Block::sector_t           block_number,
		          Genode::size_t            block_count,
		          char                     *buffer,
		          Block::Packet_descriptor &packet) override
		{
			off_t  const offset = block_number * _info.block_size;
			size_t const count  = block_count  * _info.block_size;

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
			if (!_info.writeable) {
				throw Io_error();
			}

			off_t  const offset = block_number * _info.block_size;
			size_t const count  = block_count  * _info.block_size;

			ssize_t const n = pwrite(_fd, buffer, count, offset);
			if (n == -1) {
				perror("pwrite");
				throw Io_error();
			}

			ack_packet(packet);
		}

		void sync() override { }
};


struct Main
{
	Genode::Env  &_env;
	Genode::Heap  _heap { _env.ram(), _env.rm() };

	Genode::Attached_rom_dataspace _config_rom { _env, "config" };

	struct Factory : Block::Driver_factory
	{
		Genode::Constructible<Lx_block_driver> _driver { };

		Factory(Genode::Env &env, Genode::Xml_node config)
		{
			_driver.construct(env, config);
		}

		~Factory() { _driver.destruct(); }

		/***********************
		 ** Factory interface **
		 ***********************/

		Block::Driver *create() override { return &*_driver; }
		void destroy(Block::Driver *) override { }
	} factory { _env, _config_rom.xml() };

	Block::Root root { _env.ep(), _heap, _env.rm(), factory,
	                   xml_attr_ok(_config_rom.xml(), "writeable") };

	Main(Genode::Env &env) : _env(env)
	{
		_env.parent().announce(_env.ep().manage(root));
	}
};


void Component::construct(Genode::Env &env) { static Main main(env); }
