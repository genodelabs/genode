/*
 * \brief  Test for using the VFS block file system
 * \author Norman Feske
 * \date   2017-01-28
 */

/*
 * Copyright (C) 2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Genode includes */
#include <libc/component.h>
#include <base/attached_rom_dataspace.h>

/* libc includes */
#include <unistd.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>

namespace Test {
	class Buffer;
	class Block_device;
	class Main;
}


/**
 * Dynamically allocated buffer that can be read/written from/to a file
 */
class Test::Buffer
{
	private:

		size_t const _size;
		char * const _ptr = (char *)malloc(_size);

	public:

		struct Offset { size_t value; };
		struct Fd { int value; };

		class Alloc_failed : Genode::Exception { };
		class Out_of_range : Genode::Exception { };

		/**
		 * Constructor
		 *
		 * \throw Alloc_failed
		 */
		Buffer(size_t size) : _size(size)
		{
			if (!_ptr)
				throw Alloc_failed();

			memset(_ptr, 0, _size);
		}

		/**
		 * Write char value to specified offset
		 *
		 * \throw Out_of_range
		 */
		void content_at(Offset at, char c)
		{
			if (at.value >= _size)
				throw Out_of_range();

			_ptr[at.value] = c;
		}

		/**
		 * Return char at specified offset
		 *
		 * \throw Out_of_range
		 */
		char content_at(Offset at)
		{
			if (at.value >= _size)
				throw Out_of_range();

			return _ptr[at.value];
		}

		~Buffer() { free(_ptr); }

		/**
		 * Write buffer to a file at the specified offset
		 */
		void write(Fd fd, Offset offset)
		{
			::lseek(fd.value, offset.value, SEEK_SET);
			if ((size_t)::write(fd.value, _ptr, _size) != _size)
				Genode::error("unexpected 'write' result");
		}

		/**
		 * Fetch buffer content from file at specified offset
		 */
		void read(Fd fd, Offset offset)
		{
			::lseek(fd.value, offset.value, SEEK_SET);
			if ((size_t)::read(fd.value, _ptr, _size) != _size)
				Genode::error("unexpected 'read' result");
		}
};


class Test::Block_device
{
	private:

		Buffer::Fd const _fd;

		enum { BLOCK_SIZE = 512 };

	public:

		typedef Genode::String<128> Path;

		class Unavailable : Genode::Exception { };

		/**
		 * Constructor
		 *
		 * \throw Unavailable
		 */
		Block_device(Path const &path)
		:
			_fd(Buffer::Fd{open(path.string(), O_RDWR)})
		{
			if (_fd.value < 0)
				throw Unavailable();
		}

		~Block_device() { close(_fd.value); }

		struct Block_number { unsigned long value; };

		/**
		 * Issue write operation
		 *
		 * \param block_number  first block number
		 * \param content       pattern to write
		 *
		 * The pattern is a null-terminated character string. Each character
		 * is written to the begin of one block. Hence, the length of the
		 * string corresponds to the size of the number of blocks written.
		 */
		void write(Block_number block_number, char const *content)
		{
			/* create local buffer populated with 'content' */
			size_t const content_len = strlen(content);
			Buffer buffer(BLOCK_SIZE*content_len);

			for (unsigned i = 0; i < content_len; i++)
				buffer.content_at(Buffer::Offset{BLOCK_SIZE*i}, content[i]);

			/* write buffer to the block device */
			buffer.write(_fd, Buffer::Offset{BLOCK_SIZE*block_number.value});
		}

		/**
		 * Check if content of the block device matches expectation
		 *
		 * The argument correspond to the 'write' method.
		 */
		bool expect(Block_number block_number, char const *content)
		{
			/* read data from block device into local buffer */
			size_t const content_len = strlen(content);
			Buffer buffer(BLOCK_SIZE*content_len);
			buffer.read(_fd, Buffer::Offset{BLOCK_SIZE*block_number.value});

			bool result = true;

			/* validate content */
			for (unsigned i = 0; i < content_len; i++) {

				char const c = buffer.content_at(Buffer::Offset{BLOCK_SIZE*i});
				if (c == content[i])
					continue;

				Genode::error("unexpected content "
				              "at block ", block_number.value + i, ", "
				              "got ",      Genode::Char(c), ", "
				              "expected ", Genode::Char(content[i]));

				result = false;
			};

			return result;
		}
};


struct Test::Main
{
	Genode::Env &_env;

	Genode::Attached_rom_dataspace _config { _env, "config" };

	typedef Genode::String<128> Content;

	class Step_failed : Genode::Exception { };

	/*
	 * \throw Step_failed
	 */
	static void _exec_step(Genode::Xml_node step, Block_device &);

	/*
	 * \throw Step_failed
	 */
	void _exec_sequence(Genode::Xml_node sequence);

	/*
	 * \throw Step_failed
	 */
	void _exec_sequences(Genode::Xml_node config);

	/*
	 * \throw Step_failed
	 */
	Main(Genode::Env &env) : _env(env)
	{
		_exec_sequences(_config.xml());
		_env.parent().exit(0);
	}
};


void Test::Main::_exec_step(Genode::Xml_node step, Block_device &block_device)
{
	if (step.has_type("write")) {
		Block_device::Block_number const at{step.attribute_value("at", 0UL)};
		Content const content = step.attribute_value("content", Content());
		Genode::log("write at=", at.value, " content=\"", content, "\"");
		block_device.write(at, content.string());
		return;
	}

	if (step.has_type("expect")) {
		Block_device::Block_number const at{ step.attribute_value("at", 0UL)};
		Content const content = step.attribute_value("content", Content());
		Genode::log("expect at=", at.value, " content=\"", content, "\"");
		if (block_device.expect(at, content.string()))
			return;

		Genode::error("step '", step, "' failed");
		throw Step_failed();
	}
}


void Test::Main::_exec_sequence(Genode::Xml_node sequence)
{
	Block_device dev{"/dev/block"};

	sequence.for_each_sub_node([&] (Genode::Xml_node step) {
		_exec_step(step, dev); });
}


void Test::Main::_exec_sequences(Genode::Xml_node config)
{
	config.for_each_sub_node("sequence", [&] (Genode::Xml_node sequence) {
		_exec_sequence(sequence); });
}


void Libc::Component::construct(Libc::Env &env) {
	static Test::Main main(env);
}

