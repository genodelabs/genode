/*
 * \brief  ROM module written by report service, read by ROM service
 * \author Norman Feske
 * \date   2014-01-11
 */

/*
 * Copyright (C) 2014 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _ROM_MODULE_
#define _ROM_MODULE_

/* Genode includes */
#include <util/volatile_object.h>
#include <os/attached_ram_dataspace.h>

namespace Rom {
	using Genode::size_t;
	using Genode::Lazy_volatile_object;
	using Genode::Attached_ram_dataspace;

	class Module;
	class Registry;
	class Writer;
	class Reader;
	class Buffer;

	typedef Genode::List<Module> Module_list;
	typedef Genode::List<Reader> Reader_list;
}


struct Rom::Writer { };


struct Rom::Reader : Reader_list::Element
{
	virtual void notify_module_changed() const = 0;
};


/**
 * A Rom::Module gets created as soon as either a ROM client or a Report client
 * refers to it.
 *
 * XXX We never know which of both types of client is actually connected. How
 * should pay for it? There are two choices: The backing store could be payed
 * by the server, thereby exposing the server to possibe resource exhaustion
 * triggered by a malicious client. Alternatively, we could make all clients of
 * either kind of service pay that refer to the Rom::Module. In the worst case,
 * however, if there are many client for a single report, the paid-for RAM
 * quota will never be used. For now, we simply allocate the backing store from
 * the server's quota.
 *
 * The Rom::Module gets destroyed no client refers to it anymore.
 */
struct Rom::Module : Module_list::Element
{
	public:

		typedef Genode::String<200> Name;

	private:

		Name _name;

		/**
		 * The ROM module may be read by any number of ROM clients
		 */
		Reader_list mutable _readers;

		/**
		 * There must be only one or no writer
		 */
		Writer const mutable * _writer = 0;

		/**
		 * Dataspace used as backing store
		 *
		 * The buffer for the content is not allocated from the heap to
		 * allow for the immediate release of the underlying backing store when
		 * the module gets destructed.
		 */
		Lazy_volatile_object<Attached_ram_dataspace> _ds;

		/**
		 * Content size, which may less than the capacilty of '_ds'.
		 */
		size_t _size = 0;

		void _notify_readers() const
		{
			for (Reader const *r = _readers.first(); r; r = r->next())
				r->notify_module_changed();
		}


		/********************************
		 ** Interface used by registry **
		 ********************************/

		friend class Registry;

		/**
		 * Constructor
		 */
		Module(Name const &name) : _name(name) { }

		bool _reader_is_registered(Reader const &reader) const
		{
			for (Reader const *r = _readers.first(); r; r = r->next())
				if (r == &reader)
					return true;

			return false;
		}

		void _register(Reader const &reader) const { _readers.insert(&reader); }

		void _unregister(Reader const &reader) const { _readers.remove(&reader); }

		void _register(Writer const &writer) const
		{
			class Unexpected_multiple_writers { };
			if (_writer)
				throw Unexpected_multiple_writers();

			_writer = &writer;
		}

		void _unregister(Writer const &writer) const
		{
			class Unexpected_unknown_writer { };
			if (_writer != &writer)
				throw Unexpected_unknown_writer();

			_writer = 0;
		}

		bool _has_name(Name const &name) const { return name == _name; }

		bool _is_in_use() const { return _readers.first() || _writer; }

	public:

		/**
		 * Assign new content to the ROM module
		 *
		 * Called by report service when a new report comes in.
		 */
		void write_content(char const * const src, size_t const src_len)
		{
			_size = 0;

			/* realloc backing store if needed */
			if (!_ds.is_constructed() || _ds->size() < src_len)
				_ds.construct(Genode::env()->ram_session(), src_len);

			/* copy content into backing store */
			_size = src_len;
			Genode::memcpy(_ds->local_addr<char>(), src, _size);

			/* notify ROM clients that access the module */
			for (Reader const *r = _readers.first(); r; r = r->next())
				r->notify_module_changed();
		}

		/**
		 * Exception type
		 */
		class Buffer_too_small { };

		/**
		 * Read content of ROM module
		 *
		 * Called by ROM service when a dataspace is obtained by the client.
		 */
		size_t read_content(char *dst, size_t dst_len) const
		{
			if (!_ds.is_constructed())
				return 0;

			if (dst_len < _size)
				throw Buffer_too_small();

			Genode::memcpy(dst, _ds->local_addr<char>(), _size);
			return _size;
		}

		size_t size() const { return _size; }
};

#endif /* _ROM_MODULE_ */
