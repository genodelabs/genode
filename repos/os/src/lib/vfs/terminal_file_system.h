/*
 * \brief  Terminal file system
 * \author Christian Prochaska
 * \author Norman Feske
 * \author Christian Helmuth
 * \date   2012-05-23
 */

/*
 * Copyright (C) 2012-2019 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__VFS__TERMINAL_FILE_SYSTEM_H_
#define _INCLUDE__VFS__TERMINAL_FILE_SYSTEM_H_

#include <terminal_session/connection.h>
#include <vfs/single_file_system.h>
#include <vfs/dir_file_system.h>
#include <vfs/readonly_value_file_system.h>
#include <util/xml_generator.h>
#include <os/ring_buffer.h>


namespace Vfs { class Terminal_file_system; }


struct Vfs::Terminal_file_system
{
	using Name = String<64>;

	struct Local_factory;
	struct Data_file_system;
	struct Compound_file_system;
};


/**
 * File system node for processing the terminal data read/write streams
 */
class Vfs::Terminal_file_system::Data_file_system : public Single_file_system
{
	public:

		/**
		 * Interface for propagating user interrupts (control-c)
		 */
		struct Interrupt_handler : Interface
		{
			virtual void handle_interrupt() = 0;
		};

	private:

		Name const _name;

		Genode::Entrypoint &_ep;

		Vfs::Env::User &_vfs_user;

		Terminal::Connection &_terminal;

		Interrupt_handler &_interrupt_handler;

		bool const _raw;

		enum { READ_BUFFER_SIZE = 4000 };

		using Read_buffer = Genode::Ring_buffer<char, READ_BUFFER_SIZE + 1,
		                                        Genode::Ring_buffer_unsynchronized>;

		Read_buffer _read_buffer { };

		static void _fetch_data_from_terminal(Terminal::Connection &terminal,
		                                      Read_buffer          &read_buffer,
		                                      Interrupt_handler    &interrupt_handler,
		                                      bool                  raw)
		{
			while (terminal.avail()) {

				/*
				 * Copy new data into read buffer, detect user-interrupt
				 * characters (control-c)
				 */
				unsigned const buf_size = read_buffer.avail_capacity();
				if (buf_size == 0)
					break;

				char buf[buf_size];

				using size_t = Genode::size_t;

				size_t const received = terminal.read(buf, buf_size);

				for (size_t i = 0; i < received; i++) {

					char const c = buf[i];

					enum { INTERRUPT = 3 };

					if (c == INTERRUPT && !raw) {
						interrupt_handler.handle_interrupt();
					} else {
						read_buffer.add(c);
					}
				}
			}
		}

		struct Terminal_vfs_handle : Single_vfs_handle
		{
			Terminal::Connection &_terminal;
			Vfs::Env::User       &_vfs_user;
			Read_buffer          &_read_buffer;
			Interrupt_handler    &_interrupt_handler;

			bool const _raw;

			bool notifying = false;

			Terminal_vfs_handle(Terminal::Connection &terminal,
			                    Vfs::Env::User       &vfs_user,
			                    Read_buffer          &read_buffer,
			                    Interrupt_handler    &interrupt_handler,
			                    Directory_service    &ds,
			                    File_io_service      &fs,
			                    Genode::Allocator    &alloc,
			                    int                   flags,
			                    bool                  raw)
			:
				Single_vfs_handle(ds, fs, alloc, flags),
				_terminal(terminal),
				_vfs_user(vfs_user),
				_read_buffer(read_buffer),
				_interrupt_handler(interrupt_handler),
				_raw(raw)
			{ }

			bool read_ready() const override {
				return !_read_buffer.empty(); }

			bool write_ready() const override { return true; }

			bool notify_read_ready() override
			{
				notifying = true;
				return true;
			}

			Read_result read(Byte_range_ptr const &dst, size_t &out_count) override
			{
				if (_read_buffer.empty())
					_fetch_data_from_terminal(_terminal, _read_buffer,
					                          _interrupt_handler, _raw);

				if (_read_buffer.empty())
					return READ_QUEUED;

				unsigned consumed = 0;
				for (; consumed < dst.num_bytes && !_read_buffer.empty(); consumed++)
					dst.start[consumed] = _read_buffer.get();

				out_count = consumed;

				return READ_OK;
			}

			Write_result write(Const_byte_range_ptr const &src, size_t &out_count) override
			{
				out_count = _terminal.write(src.start, src.num_bytes);
				return WRITE_OK;
			}
		};

		using Registered_handle = Genode::Registered<Terminal_vfs_handle>;
		using Handle_registry   = Genode::Registry<Registered_handle>;

		Handle_registry _handle_registry { };

		Genode::Io_signal_handler<Data_file_system> _read_avail_handler {
			_ep, *this, &Data_file_system::_handle_read_avail };

		void _handle_read_avail()
		{
			/*
			 * On non-raw sessions, fetch as much data from the terminal as
			 * possible to detect user-interrupt characters (control-c), even
			 * before the VFS client attempts to read from the terminal.
			 *
			 * Note that a user interrupt that follows a large chunk of data
			 * (exceeding the capacity of the read buffer) cannot be detected
			 * without reading the data first. In the case where the VFS client
			 * never reads data (e.g., it just blocks for a timeout),
			 * consecutive user interrupts will never be delivered once such a
			 * situation occurs. This can be provoked by pasting a large amount
			 * of text into the terminal.
			 */
			_fetch_data_from_terminal(_terminal, _read_buffer, _interrupt_handler,
			                          _raw);

			_handle_registry.for_each([] (Registered_handle &handle) {
				if (handle.notifying) {
					handle.notifying = false;
					handle.read_ready_response();
				}
			});

			_vfs_user.wakeup_vfs_user();
		}

	public:

		Data_file_system(Genode::Entrypoint   &ep,
		                 Vfs::Env::User       &vfs_user,
		                 Terminal::Connection &terminal,
		                 Name           const &name,
		                 Interrupt_handler    &interrupt_handler,
		                 bool                  raw)
		:
			Single_file_system(Node_type::TRANSACTIONAL_FILE, name.string(),
			                   Node_rwx::rw(), Genode::Xml_node("<data/>")),
			_name(name), _ep(ep), _vfs_user(vfs_user), _terminal(terminal),
			_interrupt_handler(interrupt_handler),
			_raw(raw)
		{
			/* register for read-avail notification */
			_terminal.read_avail_sigh(_read_avail_handler);
		}

		static const char *name()   { return "data"; }
		char const *type() override { return "data"; }

		Open_result open(char const  *path, unsigned flags,
		                 Vfs_handle **out_handle,
		                 Allocator   &alloc) override
		{
			if (!_single_file(path))
				return OPEN_ERR_UNACCESSIBLE;

			try {
				*out_handle = new (alloc)
					Registered_handle(_handle_registry, _terminal, _vfs_user,
					                  _read_buffer, _interrupt_handler,
					                  *this, *this, alloc, flags, _raw);
				return OPEN_OK;
			}
			catch (Genode::Out_of_ram)  { return OPEN_ERR_OUT_OF_RAM; }
			catch (Genode::Out_of_caps) { return OPEN_ERR_OUT_OF_CAPS; }
		}


		/********************************
		 ** File I/O service interface **
		 ********************************/

		Ftruncate_result ftruncate(Vfs_handle *, file_size) override
		{
			return FTRUNCATE_OK;
		}
};


struct Vfs::Terminal_file_system::Local_factory : File_system_factory,
                                                  Data_file_system::Interrupt_handler
{
	using Label = Genode::String<64>;
	Label const _label;

	Name const _name;

	Genode::Env &_env;

	Vfs::Env::User &_vfs_user;

	Terminal::Connection _terminal { _env, _label.string() };

	bool const _raw;

	Data_file_system _data_fs { _env.ep(), _vfs_user, _terminal, _name, *this, _raw };

	struct Info
	{
		Terminal::Session::Size size;

		void print(Genode::Output &out) const
		{
			char buf[128] { };
			Genode::Xml_generator xml(buf, sizeof(buf), "terminal", [&] () {
				xml.attribute("rows",    size.lines());
				xml.attribute("columns", size.columns());
			});
			Genode::print(out, Genode::Cstring(buf));
		}
	};

	/**
	 * Number of occurred user interrupts (control-c)
	 */
	unsigned _interrupts = 0;

	Readonly_value_file_system<Info>     _info_fs       { "info",       Info{} };
	Readonly_value_file_system<unsigned> _rows_fs       { "rows",       0 };
	Readonly_value_file_system<unsigned> _columns_fs    { "columns",    0 };
	Readonly_value_file_system<unsigned> _interrupts_fs { "interrupts", _interrupts };

	Genode::Io_signal_handler<Local_factory> _size_changed_handler {
		_env.ep(), *this, &Local_factory::_handle_size_changed };

	void _handle_size_changed()
	{
		Info const info { .size = _terminal.size() };

		_info_fs   .value(info);
		_rows_fs   .value(info.size.lines());
		_columns_fs.value(info.size.columns());
	}

	/**
	 * Interrupt_handler interface
	 */
	void handle_interrupt() override
	{
		_interrupts++;
		_interrupts_fs.value(_interrupts);
	}

	static Name name(Xml_node config)
	{
		return config.attribute_value("name", Name("terminal"));
	}

	Local_factory(Vfs::Env &env, Xml_node config)
	:
		_label(config.attribute_value("label", Label(""))),
		_name(name(config)),
		_env(env.env()),
		_vfs_user(env.user()),
		_raw(config.attribute_value("raw", false))
	{
		_terminal.size_changed_sigh(_size_changed_handler);
		_handle_size_changed();
	}

	Vfs::File_system *create(Vfs::Env&, Xml_node node) override
	{
		if (node.has_type("data"))       return &_data_fs;
		if (node.has_type("info"))       return &_info_fs;
		if (node.has_type("rows"))       return &_rows_fs;
		if (node.has_type("columns"))    return &_columns_fs;
		if (node.has_type("interrupts")) return &_interrupts_fs;

		return nullptr;
	}
};


class Vfs::Terminal_file_system::Compound_file_system : private Local_factory,
                                                        public  Vfs::Dir_file_system
{
	private:

		using Name = Terminal_file_system::Name;

		using Config = String<200>;
		static Config _config(Name const &name)
		{
			char buf[Config::capacity()] { };

			/*
			 * By not using the node type "dir", we operate the
			 * 'Dir_file_system' in root mode, allowing multiple sibling nodes
			 * to be present at the mount point.
			 */
			Genode::Xml_generator xml(buf, sizeof(buf), "compound", [&] () {

				xml.node("data", [&] () {
					xml.attribute("name", name); });

				xml.node("dir", [&] () {
					xml.attribute("name", Name(".", name));
					xml.node("info",       [&] () {});
					xml.node("rows",       [&] () {});
					xml.node("columns",    [&] () {});
					xml.node("interrupts", [&] () {});
				});
			});

			return Config(Genode::Cstring(buf));
		}

	public:

		Compound_file_system(Vfs::Env &vfs_env, Genode::Xml_node node)
		:
			Local_factory(vfs_env, node),
			Vfs::Dir_file_system(vfs_env,
			                     Xml_node(_config(Local_factory::name(node)).string()),
			                     *this)
		{ }

		static const char *name() { return "terminal"; }

		char const *type() override { return name(); }
};

#endif /* _INCLUDE__VFS__TERMINAL_FILE_SYSTEM_H_ */
