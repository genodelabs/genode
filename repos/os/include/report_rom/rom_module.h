/*
 * \brief  ROM module written by report service, read by ROM service
 * \author Norman Feske
 * \date   2014-01-11
 */

/*
 * Copyright (C) 2014-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__REPORT_ROM__ROM_MODULE_H_
#define _INCLUDE__REPORT_ROM__ROM_MODULE_H_

/* Genode includes */
#include <util/reconstructible.h>
#include <os/session_policy.h>
#include <base/attached_ram_dataspace.h>

namespace Rom {
	using Genode::size_t;
	using Genode::Constructible;
	using Genode::Attached_ram_dataspace;

	class Module;
	class Readable_module;
	class Registry;
	class Writer;
	class Reader;
	class Buffer;

	typedef Genode::List<Module> Module_list;
	typedef Genode::List<Reader> Reader_list;
	typedef Genode::List<Writer> Writer_list;
}


struct Rom::Writer : Writer_list::Element
{
	virtual Genode::Session_label label() const = 0;
};


struct Rom::Reader : Reader_list::Element
{
	virtual void notify_module_changed()     = 0;
	virtual void notify_module_invalidated() = 0;
};


struct Rom::Readable_module
{
	/**
	 * Exception type
	 */
	class Buffer_too_small { };

	/**
	 * Read content of ROM module
	 *
	 * Called by ROM service when a dataspace is obtained by the client.
	 *
	 * \throw Buffer_too_small
	 */
	virtual size_t read_content(Reader const &reader, char *dst,
	                            size_t dst_len) const = 0;

	virtual size_t size() const = 0;
};


/**
 * A Rom::Module gets created as soon as either a ROM client or a Report client
 * refers to it.
 *
 * XXX We never know which of both types of client is actually connected. How
 * should pay for it? There are two choices: The backing store could be paid
 * by the server, thereby exposing the server to possibe resource exhaustion
 * triggered by a malicious client. Alternatively, we could make all clients of
 * either kind of service pay that refer to the Rom::Module. In the worst case,
 * however, if there are many client for a single report, the paid-for RAM
 * quota will never be used. For now, we simply allocate the backing store from
 * the server's quota.
 *
 * The Rom::Module gets destroyed when no client refers to it anymore.
 */
struct Rom::Module : Module_list::Element, Readable_module
{
	public:

		typedef Genode::String<200> Name;

		struct Read_policy
		{
			/**
			 * Return true if the reader is allowed to read the module content
			 */
			virtual bool read_permitted(Module const &,
			                            Writer const &, Reader const &) const = 0;
		};

		struct Write_policy
		{
			/**
			 * Return true of the writer is permitted to write content
			 *
			 * This policy hook can be used to implement dynamic policies
			 * as employed by the clipboard, which blocks reports from
			 * unfocused clients.
			 */
			virtual bool write_permitted(Module const &, Writer const &) const = 0;
		};

	private:

		Name _name;

		Genode::Ram_session &_ram;
		Genode::Region_map  &_rm;

		Read_policy  const &_read_policy;
		Write_policy const &_write_policy;

		Reader_list mutable _readers;
		Writer_list mutable _writers;

		/**
		 * Origin of the content currently stored in the module
		 */
		Writer const *_last_writer = nullptr;

		/**
		 * Dataspace used as backing store
		 *
		 * The buffer for the content is not allocated from the heap to
		 * allow for the immediate release of the underlying backing store when
		 * the module gets destructed.
		 */
		Constructible<Attached_ram_dataspace> _ds;

		/**
		 * Content size, which may less than the capacilty of '_ds'.
		 */
		size_t _size = 0;


		/********************************
		 ** Interface used by registry **
		 ********************************/

		friend class Registry;

		/**
		 * Constructor
		 *
		 * \param ram           RAM session from which to allocate the module's
		 *                      backing store
		 * \param rm            region map of the local address space, needed
		 *                      to access the allocated backing store
		 * \param name          module name
		 * \param read_policy   policy hook function that is evaluated each
		 *                      time when the module content is obtained
		 * \param write_policy  policy hook function that is evaluated each
		 *                      time when the module content is changed
		 */
		Module(Genode::Ram_session &ram,
		       Genode::Region_map  &rm,
		       Name          const &name,
		       Read_policy   const &read_policy,
		       Write_policy  const &write_policy)
		:
			_name(name), _ram(ram), _rm(rm),
			_read_policy(read_policy), _write_policy(write_policy)
		{ }


		/*************************************************
		 ** Interface to be used by the 'Registry' only **
		 *************************************************/

		bool _reader_registered(Reader const &reader) const
		{
			for (Reader const *r = _readers.first(); r; r = r->next())
				if (r == &reader)
					return true;

			return false;
		}

		void _register(Reader &reader) { _readers.insert(&reader); }

		void _unregister(Reader &reader) { _readers.remove(&reader); }

		void _register(Writer &writer)
		{
			_writers.insert(&writer);
		}

		void _unregister(Writer const &writer)
		{
			_writers.remove(&writer);

			/* clear content if its origin disappears */
			if (_last_writer == &writer) {
				Genode::memset(_ds->local_addr<char>(), 0, _size);
				_size = 0;
				_last_writer = nullptr;
			}
		}

		bool _has_name(Name const &name) const { return name == _name; }

		bool _in_use() const
		{
			return _readers.first() || _writers.first();
		}

		unsigned _num_writers() const
		{
			unsigned cnt = 0;
			for (Writer const *w = _writers.first(); w; w = w->next())
				cnt++;

			return cnt;
		}

	public:

		/**
		 * Assign new content to the ROM module
		 *
		 * Called by report service when a new report comes in.
		 */
		void write_content(Writer const &writer, char const * const src, size_t const src_len)
		{
			if (!_write_policy.write_permitted(*this, writer))
				return;

			_size = 0;

			_last_writer = &writer;

			/*
			 * Realloc backing store if needed
			 *
			 * Take a terminating zero into account, which we append to each
			 * report. This way, we do not need to trust report clients to
			 * append a zero termination to textual reports.
			 */
			if (!_ds.constructed() || _ds->size() < (src_len + 1))
				_ds.construct(_ram, _rm, (src_len + 1));

			/* copy content into backing store */
			_size = src_len;
			Genode::memcpy(_ds->local_addr<char>(), src, _size);

			/* append zero termination */
			_ds->local_addr<char>()[src_len] = 0;

			/* notify ROM clients that access the module */
			for (Reader *r = _readers.first(); r; r = r->next()) {

				if (_read_policy.read_permitted(*this, *_last_writer, *r))
					r->notify_module_changed();
				else
					r->notify_module_invalidated();
			}
		}

		/**
		 * Readable_module interface
		 */
		size_t read_content(Reader const &reader, char *dst, size_t dst_len) const override
		{
			if (!_ds.constructed() || !_last_writer)
				return 0;

			if (!_read_policy.read_permitted(*this, *_last_writer, reader))
				return 0;

			if (dst_len < _size)
				throw Buffer_too_small();

			Genode::memcpy(dst, _ds->local_addr<char>(), _size);
			return _size;
		}

		virtual size_t size() const override { return _size; }

		Name name() const { return _name; }
};

#endif /* _INCLUDE__REPORT_ROM__ROM_MODULE_H_ */
