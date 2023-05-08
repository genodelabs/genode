/*
 * \brief  GDB stub
 * \author Norman Feske
 * \date   2023-05-11
 */

/*
 * Copyright (C) 2023 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _GDB_STUB_H_
#define _GDB_STUB_H_

#include <inferior_cpu.h>
#include <memory_accessor.h>
#include <gdb_command.h>
#include <gdb_response.h>
#include <gdb_arch.h>

namespace Monitor { namespace Gdb { struct State; } }


struct Monitor::Gdb::State : Noncopyable
{
	Inferiors &inferiors;

	struct Thread_list
	{
		char   _buf[1024*16] { };
		size_t _len = 0;

		Thread_list(Inferiors const &inferiors)
		{
			Xml_generator xml(_buf, sizeof(_buf), "threads", [&] {
				inferiors.for_each<Inferior_pd const &>([&] (Inferior_pd const &inferior) {
					inferior.for_each_thread([&] (Monitored_thread const &thread) {
						xml.node("thread", [&] {
							String<32> const id("p", inferior.id(), ".", thread.id());
							xml.attribute("id",   id);
							xml.attribute("core", 0);
							xml.attribute("name", thread._name); }); }); }); });

			_len = strlen(_buf);
		}

		void with_bytes(auto const &fn) const
		{
			Const_byte_range_ptr const ptr { _buf, _len };
			fn(ptr);
		}
	};

	Memory_accessor &_memory_accessor;

	struct Current : Noncopyable
	{
		Inferior_pd &pd;

		struct Thread
		{
			Monitored_thread &thread;

			Thread(Monitored_thread &thread) : thread(thread) { }
		};

		Constructible<Thread> thread { };

		Current(Inferior_pd &pd) : pd(pd) { }
	};

	Constructible<Current> _current { };

	void flush(Inferior_pd &pd)
	{
		if (_current.constructed() && _current->pd.id() == pd.id())
			_current.destruct();
	}

	size_t read_memory(Memory_accessor::Virt_addr at, Byte_range_ptr const &dst)
	{
		if (_current.constructed())
			return _memory_accessor.read(_current->pd, at, dst);

		warning("attempt to read memory without a current target");
		return 0;
	}

	bool current_defined() const { return _current.constructed(); }

	void current(Inferiors::Id pid, Threads::Id tid)
	{
		_current.destruct();

		inferiors.for_each<Inferior_pd &>([&] (Inferior_pd &inferior) {
			if (inferior.id() != pid.value)
				return;

			_current.construct(inferior);

			inferior._threads.for_each<Monitored_thread &>([&] (Monitored_thread &thread) {
				if (thread.id() == tid.value)
					_current->thread.construct(thread); });
		});
	}

	void with_current_thread_state(auto const &fn)
	{
		Thread_state thread_state { };

		if (_current.constructed() && _current->thread.constructed()) {
			try {
				thread_state = _current->thread->thread._real.call<Cpu_thread::Rpc_get_state>();
			} catch (Cpu_thread::State_access_failed) {
				warning("unable to access state of thread ", _current->thread->thread.id());
			}
		}

		fn(thread_state);
	};

	State(Inferiors &inferiors, Memory_accessor &memory_accessor)
	:
		inferiors(inferiors), _memory_accessor(memory_accessor)
	{ }
};


/*
 * The command types within the 'Gdb::Cmd' namespace deliberately do not follow
 * Genode's coding style regarding the use of upper/lower case letters.
 *
 * The types are named precisely atfer the corresponding commands of the GDB
 * prototol, which are case sensitive.
 */

namespace Monitor { namespace Gdb { namespace Cmd {


/**
 * Protocol negotiation
 */
struct qSupported : Command_with_separator
{
	qSupported(Commands &c) : Command_with_separator(c, "qSupported") { }

	void execute(State &, Const_byte_range_ptr const &args, Output &out) const override
	{
		/* check for expected GDB features */
		bool gdb_supports_multiprocess = false,
		     gdb_supports_vcont        = false;

		for_each_argument(args, Sep {';'}, [&] (Const_byte_range_ptr const &arg) {
			if (equal(arg, "multiprocess+"))   gdb_supports_multiprocess = true;
			if (equal(arg, "vContSupported+")) gdb_supports_vcont        = true;
		});

		if (!gdb_supports_multiprocess) warning("GDB lacks multi-process support");
		if (!gdb_supports_vcont)        warning("GDB lacks vcont support");

		/* tell GDB about our features */
		gdb_response(out, [&] (Output &out) {
			print(out, "PacketSize=", Gdb_hex(GDB_PACKET_MAX_SIZE), ";");
			print(out, "vContSupported+;");
			print(out, "qXfer:features:read+;");  /* XML target descriptions */
			print(out, "qXfer:threads:read+;");
			print(out, "multiprocess+;");
			print(out, "QNonStop+;");
		});
	}
};


extern "C" char _binary_gdb_target_xml_start[];
extern "C" char _binary_gdb_target_xml_end[];


/**
 * Query XML-based information
 */
struct qXfer : Command_with_separator
{
	qXfer(Commands &c) : Command_with_separator(c, "qXfer") { }

	struct Raw_data_ptr : Const_byte_range_ptr
	{
		Raw_data_ptr(char const *start, char const *end)
		:
			Const_byte_range_ptr(start, end - start)
		{ }
	};

	struct Window
	{
		size_t offset, len;

		static Window from_args(Const_byte_range_ptr const &args)
		{
			return { .offset = comma_separated_hex_value(args, 0, 0UL),
			         .len    = comma_separated_hex_value(args, 1, 0UL) };
		}
	};

	static void _send_window(Output &out, Const_byte_range_ptr const &total_bytes, Window const window)
	{
		with_skipped_bytes(total_bytes, window.offset, [&] (Const_byte_range_ptr const &bytes) {
			with_max_bytes(bytes, window.len, [&] (Const_byte_range_ptr const &bytes) {
				gdb_response(out, [&] (Output &out) {
					char const *prefix = (window.offset + window.len < total_bytes.num_bytes)
					                   ? "m" : "l"; /* 'last' marker */
					print(out, prefix, Cstring(bytes.start, bytes.num_bytes)); }); }); });
	}

	void execute(State &state, Const_byte_range_ptr const &args, Output &out) const override
	{
		bool handled = false;

		with_skipped_prefix(args, "features:read:target.xml:", [&] (Const_byte_range_ptr const &args) {
			Raw_data_ptr const total_bytes { _binary_gdb_target_xml_start, _binary_gdb_target_xml_end };
			_send_window(out, total_bytes, Window::from_args(args));
			handled = true;
		});

		with_skipped_prefix(args, "threads:read::", [&] (Const_byte_range_ptr const &args) {
			State::Thread_list const thread_list(state.inferiors);
			thread_list.with_bytes([&] (Const_byte_range_ptr const &bytes) {
				_send_window(out, bytes, Window::from_args(args)); });
			handled = true;
		});

		if (!handled)
			warning("GDB ", name, " command unsupported: ", Cstring(args.start, args.num_bytes));
	}
};


struct vMustReplyEmpty : Command_without_separator
{
	vMustReplyEmpty(Commands &c) : Command_without_separator(c, "vMustReplyEmpty") { }

	void execute(State &, Const_byte_range_ptr const &, Output &out) const override
	{
		gdb_response(out, [&] (Output &) { });
	}
};


/**
 * Set current thread
 */
struct H : Command_without_separator
{
	H(Commands &commands) : Command_without_separator(commands, "H") { }

	void execute(State &state, Const_byte_range_ptr const &args, Output &out) const override
	{
		log("H command args: ", Cstring(args.start, args.num_bytes));

		/* 'g' for other operations, 'p' as prefix of thread-id syntax */
		with_skipped_prefix(args, "gp", [&] (Const_byte_range_ptr const &args) {

			auto dot_separated_arg_value = [&] (unsigned i, auto &value)
			{
				with_argument(args, Sep { '.' }, i, [&] (Const_byte_range_ptr const &arg) {
					with_null_terminated(arg, [&] (char const * const str) {
						ascii_to(str, value); }); });
			};

			unsigned pid = 0, tid = 0;

			dot_separated_arg_value(0, pid);
			dot_separated_arg_value(1, tid);

			/*
			 * GDB initially sends a Hgp0.0 command but assumes that inferior 1
			 * is current. Avoid losing the default current inferior as set by
			 * 'Main::_create_session'.
			 */
			if (pid > 0)
				state.current(Inferiors::Id { pid }, Threads::Id { tid });

			gdb_ok(out);
		});

		with_skipped_prefix(args, "c-", [&] (Const_byte_range_ptr const &) {
			gdb_error(out, 1); });
	}
};


/**
 * Enable/disable non-stop mode
 */
struct QNonStop : Command_with_separator
{
	QNonStop(Commands &commands) : Command_with_separator(commands, "QNonStop") { }

	void execute(State &, Const_byte_range_ptr const &args, Output &out) const override
	{
		log("QNonStop command args: ", Cstring(args.start, args.num_bytes));

		gdb_ok(out);
	}
};


/**
 * Symbol-lookup prototol (not used)
 */
struct qSymbol : Command_with_separator
{
	qSymbol(Commands &commands) : Command_with_separator(commands, "qSymbol") { }

	void execute(State &, Const_byte_range_ptr const &, Output &out) const override
	{
		gdb_ok(out);
	}
};


/**
 * Query trace status
 */
struct qTStatus : Command_without_separator
{
	qTStatus(Commands &commands) : Command_without_separator(commands, "qTStatus") { }

	void execute(State &, Const_byte_range_ptr const &, Output &out) const override
	{
		gdb_response(out, [&] (Output &) { });
	}
};


/**
 * Query current thread ID
 */
struct qC : Command_without_separator
{
	qC(Commands &commands) : Command_without_separator(commands, "qC") { }

	void execute(State &, Const_byte_range_ptr const &args, Output &out) const override
	{
		log("qC: ", Cstring(args.start, args.num_bytes));

		gdb_response(out, [&] (Output &) { });
	}
};


/**
 * Query attached state
 */
struct qAttached : Command_without_separator
{
	qAttached(Commands &commands) : Command_without_separator(commands, "qAttached") { }

	void execute(State &, Const_byte_range_ptr const &, Output &out) const override
	{
		gdb_response(out, [&] (Output &out) {
			print(out, "1"); });
	}
};


/**
 * Query text/data segment offsets
 */
struct qOffsets : Command_without_separator
{
	qOffsets(Commands &commands) : Command_without_separator(commands, "qOffsets") { }

	void execute(State &, Const_byte_range_ptr const &args, Output &out) const override
	{
		log("qOffsets: ", Cstring(args.start, args.num_bytes));

		gdb_response(out, [&] (Output &) { });
	}
};


/**
 * Query halt reason
 */
struct ask : Command_without_separator
{
	ask(Commands &c) : Command_without_separator(c, "?") { }

	void execute(State &, Const_byte_range_ptr const &args, Output &out) const override
	{
		log("? command args: ", Cstring(args.start, args.num_bytes));

		gdb_response(out, [&] (Output &out) {
			print(out, "T05"); });
	}
};


/**
 * Read registers
 */
struct g : Command_without_separator
{
	g(Commands &c) : Command_without_separator(c, "g") { }

	void execute(State &state, Const_byte_range_ptr const &, Output &out) const override
	{
		log("-> execute g");

		gdb_response(out, [&] (Output &out) {
			state.with_current_thread_state([&] (Thread_state const &thread_state) {
				print_registers(out, thread_state); }); });
	}
};


/**
 * Read memory
 */
struct m : Command_without_separator
{
	m(Commands &c) : Command_without_separator(c, "m") { }

	void execute(State &state, Const_byte_range_ptr const &args, Output &out) const override
	{
		addr_t const addr = comma_separated_hex_value(args, 0, addr_t(0));
		size_t const len  = comma_separated_hex_value(args, 1, 0UL);

		gdb_response(out, [&] (Output &out) {

			for (size_t pos = 0; pos < len; ) {

				char chunk[16*1024] { };

				size_t const remain_len = len - pos;
				size_t const num_bytes  = min(sizeof(chunk), remain_len);

				size_t const read_len =
					state.read_memory(Memory_accessor::Virt_addr { addr + pos },
					                  Byte_range_ptr(chunk, num_bytes));

				for (unsigned i = 0; i < read_len; i++)
					print(out, Gdb_hex(chunk[i]));

				pos += read_len;

				if (read_len < num_bytes)
					break;
			}
		});
	}
};


/**
 * Query liveliness of thread ID
 */
struct T : Command_without_separator
{
	T(Commands &c) : Command_without_separator(c, "T") { }

	void execute(State &, Const_byte_range_ptr const &args, Output &out) const override
	{
		log("T command args: ", Cstring(args.start, args.num_bytes));

		gdb_ok(out);
	}
};


/**
 * Disconnect
 */
struct D : Command_with_separator
{
	D(Commands &c) : Command_with_separator(c, "D") { }

	void execute(State &, Const_byte_range_ptr const &, Output &out) const override
	{
		gdb_ok(out);
	}
};


} /* namespace Cmd */ } /* namespace Gdb */ } /* namespace Monitor */


/*
 * Registry of all supported commands
 */

namespace Monitor { namespace Gdb { struct Supported_commands; } }

struct Monitor::Gdb::Supported_commands : Commands
{
	template <typename...>
	struct Instances;

	template <typename LAST>
	struct Instances<LAST>
	{
		LAST _last;
		Instances(Commands &registry) : _last(registry) { };
	};

	template <typename HEAD, typename... TAIL>
	struct Instances<HEAD, TAIL...>
	{
		HEAD               _head;
		Instances<TAIL...> _tail;
		Instances(Commands &registry) : _head(registry), _tail(registry) { }
	};

	Instances<
		Cmd::qSupported,
		Cmd::qXfer,
		Cmd::vMustReplyEmpty,
		Cmd::H,
		Cmd::QNonStop,
		Cmd::qSymbol,
		Cmd::qTStatus,
		Cmd::qC,
		Cmd::qAttached,
		Cmd::qOffsets,
		Cmd::g,
		Cmd::m,
		Cmd::D,
		Cmd::T,
		Cmd::ask
		> _instances { *this };
};

#endif /* _GDB_STUB_H_ */
