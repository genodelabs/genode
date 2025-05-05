/*
 * \brief  Init component with builtin debug monitor
 * \author Norman Feske
 * \date   2023-05-08
 */

/*
 * Copyright (C) 2023 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Genode includes */
#include <base/component.h>
#include <base/attached_rom_dataspace.h>
#include <region_map/client.h>
#include <sandbox/sandbox.h>
#include <os/reporter.h>
#include <terminal_connection.h>

/* local includes */
#include <pd_intrinsics.h>
#include <gdb_stub.h>
#include <gdb_packet_handler.h>
#include <monitored_vm.h>

namespace Monitor {

	template <typename CONNECTION>
	struct Connection : Interface
	{
		CONNECTION _connection;

		template <typename... ARGS>
		Connection(ARGS &&... args) : _connection(args...) { }

		void upgrade(Session::Resources const &resources)
		{
			_connection.upgrade(resources);
		}
	};
}


namespace Monitor { struct Main; }

struct Monitor::Main : Sandbox::State_handler,
                       Thread_monitor
{
	struct Local_pd_session : Connection<Pd_connection>, Inferior_pd
	{
		Local_pd_session(Env &env, Session::Label const &label, auto &&... args)
		:
			Connection<Pd_connection>(env, label),
			Inferior_pd(env.ep(), _connection.cap(), label, args...)
		{ }
	};

	struct Local_cpu_session : Connection<Cpu_connection>, Inferior_cpu
	{
		Local_cpu_session(Env &env, Session::Label const &label,
		                  Affinity affinity, Priority priority, Allocator &alloc,
		                  Thread_monitor &thread_monitor)
		:
			Connection<Cpu_connection>(env, label, priority.value, affinity),
			Inferior_cpu(env.ep(), _connection.cap(), label, alloc,
			             thread_monitor)
		{ }
	};

	struct Local_vm_session : Connection<Vm_connection>, Monitored_vm_session
	{
		Local_vm_session(Env &env, Session::Label const &label, Priority priority)
		:
			Connection<Vm_connection>(env, label, priority.value),
			Monitored_vm_session(env.ep(), _connection.cap(), label)
		{ }
	};

	using Pd_service  = Sandbox::Local_service<Local_pd_session>;
	using Cpu_service = Sandbox::Local_service<Local_cpu_session>;
	using Vm_service  = Sandbox::Local_service<Local_vm_session>;

	Env &_env;

	Heap _heap { _env.ram(), _env.rm() };

	Pd_intrinsics _pd_intrinsics { _env };

	Sandbox _sandbox { _env, *this, _pd_intrinsics };

	Inferior_cpu::Kernel _detect_kernel()
	{
		Attached_rom_dataspace info { _env, "platform_info" };

		Inferior_cpu::Kernel result = Inferior_cpu::Kernel::GENERIC;

		info.xml().with_optional_sub_node("kernel", [&] (Xml_node const &kernel) {
			if (kernel.attribute_value("name", String<10>()) == "nova")
				result = Inferior_cpu::Kernel::NOVA; });

		return result;
	}

	Inferior_cpu::Kernel const _kernel = _detect_kernel();

	Attached_rom_dataspace _config { _env, "config" };

	Inferiors::Id _last_inferior_id { }; /* counter for unique inferior IDs */

	Inferiors _inferiors { };

	struct Gdb_stub
	{
		Env &_env;

		Terminal_connection _terminal  { _env };

		Signal_handler<Gdb_stub> _terminal_read_avail_handler {
			_env.ep(), *this, &Gdb_stub::_handle_terminal_read_avail };

		Memory_accessor _memory_accessor { _env };

		Gdb::Packet_handler     _packet_handler { };
		Gdb::State              _state;
		Gdb::Supported_commands _commands { };

		struct Terminal_output
		{
			struct Write_fn
			{
				Terminal_connection &_terminal;
				void operator () (char const *str)
				{
					size_t const num_bytes = strlen(str);
					size_t const written_bytes = _terminal.write(str, num_bytes);
					if (written_bytes != num_bytes)
						Genode::warning("Could not send the debug response "
						                "message completely.");
				}
			} _write_fn;

			Buffered_output<1024, Write_fn> buffered { _write_fn };
		};

		void _handle_terminal_read_avail()
		{
			Terminal_output output { ._write_fn { _terminal } };

			for (;;) {
				char buffer[1024] { };
				size_t const num_bytes = _terminal.read(buffer, sizeof(buffer));
				if (!num_bytes)
					return;

				_packet_handler.execute(_state, _commands,
				                        Const_byte_range_ptr { buffer, num_bytes },
				                        output.buffered);
			}
		}

		void flush(Inferior_pd &pd)
		{
			Terminal_output output { ._write_fn { _terminal } };
			_state.flush(pd, output.buffered);
			_memory_accessor.flush();
		}

		void flush(Monitored_thread &thread)
		{
			_state.flush(thread);
		}

		void thread_stopped(Inferior_pd &inferior, Monitored_thread &thread)
		{
			if (_state.gdb_connected && !_state.notification_in_progress) {

				_state.notification_in_progress = true;

				using Stop_state = Monitored_thread::Stop_state;
				using Stop_reply_signal = Monitored_thread::Stop_reply_signal;

				thread.stop_state = Stop_state::STOPPED_REPLY_SENT;

				Terminal_output output { ._write_fn { _terminal } };
				gdb_notification(output.buffered, [&] (Output &out) {
					print(out, "Stop:T",
					           Gdb_hex((uint8_t)thread.stop_reply_signal),
					           "thread:p",
					           Gdb_hex(inferior.id()),
					           ".",
					           Gdb_hex(thread.id()),
					           ";");
					if (thread.stop_reply_signal == Stop_reply_signal::TRAP)
						print(out, "swbreak:;");
				});
			}
		}

		Gdb_stub(Env &env, Inferiors &inferiors, Xml_node const &config)
		:
			_env(env), _state(inferiors, _memory_accessor, config)
		{
			_terminal.read_avail_sigh(_terminal_read_avail_handler);
			_handle_terminal_read_avail();
		}
	};

	Constructible<Gdb_stub> _gdb_stub { };

	void _handle_resource_avail() { }

	Signal_handler<Main> _resource_avail_handler {
		_env.ep(), *this, &Main::_handle_resource_avail };

	Constructible<Reporter> _reporter { };

	size_t _report_buffer_size = 0;

	template <typename SERVICE>
	void _handle_service(SERVICE &);

	void _handle_pd_service()  { _handle_service<Pd_service> (_pd_service);  }
	void _handle_cpu_service() { _handle_service<Cpu_service>(_cpu_service); }
	void _handle_vm_service()  { _handle_service<Vm_service> (_vm_service);  }

	struct Service_handler : Sandbox::Local_service_base::Wakeup
	{
		Main &_main;

		using Member = void (Main::*) ();
		Member _member;

		void wakeup_local_service() override { (_main.*_member)(); }

		Service_handler(Main &main, Member member)
		: _main(main), _member(member) { }
	};

	Service_handler _pd_handler  { *this, &Main::_handle_pd_service  };
	Service_handler _cpu_handler { *this, &Main::_handle_cpu_service };
	Service_handler _vm_handler  { *this, &Main::_handle_vm_service  };

	Pd_service  _pd_service  { _sandbox, _pd_handler  };
	Cpu_service _cpu_service { _sandbox, _cpu_handler };
	Vm_service  _vm_service  { _sandbox, _vm_handler  };

	using Session_request = Sandbox::Local_service_base::Request;

	Priority _priority_from_args(Session_request::Args const &args)
	{
		return Priority {
			(long)Arg_string::find_arg(args.string(), "priority")
			                 .ulong_value(Cpu_session::DEFAULT_PRIORITY) };
	}

	Local_pd_session &_create_session(Pd_service &, Session_request const &request)
	{
		_last_inferior_id.value++;

		Inferiors::Id const id = _last_inferior_id;

		Local_pd_session &session = *new (_heap)
			Local_pd_session(_env, request.label, _inferiors, id, _env.rm(), _heap, _env.ram());

		_apply_monitor_config_to_inferiors();

		/* set first monitored PD as current inferior */
		if (_gdb_stub.constructed() && !_gdb_stub->_state.current_defined())
			_gdb_stub->_state.current(id, Threads::Id { });

		return session;
	}

	Local_cpu_session &_create_session(Cpu_service &, Session_request const &request)
	{
		Local_cpu_session &session = *new (_heap)
			Local_cpu_session(_env, request.label, request.affinity,
			                  _priority_from_args(request.args), _heap, *this);

		session.init_native_cpu(_kernel);

		return session;
	}

	Local_vm_session &_create_session(Vm_service &, Session_request const &request)
	{
		return *new (_heap)
			Local_vm_session(_env, request.label, _priority_from_args(request.args));
	}

	void _destroy_session(Local_pd_session &session)
	{
		if (_gdb_stub.constructed())
			_gdb_stub->flush(session);

		destroy(_heap, &session);
	}

	void _destroy_session(Local_cpu_session &session)
	{
		destroy(_heap, &session);
	}

	void _destroy_session(Local_vm_session &session)
	{
		destroy(_heap, &session);
	}

	void _apply_monitor_config_to_inferiors()
	{
		_config.xml().with_sub_node("monitor",
			[&] (Xml_node const monitor) {
				_inferiors.for_each<Inferior_pd>([&] (Inferior_pd &pd) {
					pd.apply_monitor_config(monitor); }); },
			[&] {
				_inferiors.for_each<Inferior_pd>([&] (Inferior_pd &pd) {
					pd.apply_monitor_config("<monitor/>"); }); });
	}

	void _handle_config()
	{
		_config.update();

		Xml_node const config = _config.xml();

		bool reporter_enabled = false;
		config.with_optional_sub_node("report", [&] (Xml_node report) {

			reporter_enabled = true;

			/* (re-)construct reporter whenever the buffer size is changed */
			Number_of_bytes const buffer_size =
				report.attribute_value("buffer", Number_of_bytes(4096));

			if (buffer_size != _report_buffer_size || !_reporter.constructed()) {
				_report_buffer_size = buffer_size;
				_reporter.construct(_env, "state", "state", _report_buffer_size);
			}
		});

		if (_reporter.constructed())
			_reporter->enabled(reporter_enabled);

		_gdb_stub.conditional(config.has_sub_node("monitor"), _env, _inferiors,
		                      config);

		_apply_monitor_config_to_inferiors();

		_sandbox.apply_config(config);
	}

	Signal_handler<Main> _config_handler {
		_env.ep(), *this, &Main::_handle_config };

	/**
	 * Sandbox::State_handler interface
	 */
	void handle_sandbox_state() override
	{
		Reporter::Xml_generator xml(*_reporter, [&] () {
			_sandbox.generate_state_report(xml); });

		if (xml.exceeded())

			error("state report exceeds maximum size");

			/* try to reflect the error condition as state report */
			Reporter::Xml_generator xml(*_reporter, [&] () {
				xml.attribute("error", "report buffer exceeded"); });
		}
	}

	Main(Env &env) : _env(env)
	{
		_config.sigh(_config_handler);

		/* prevent blocking for resource upgrades (never satisfied by core) */
		_env.parent().resource_avail_sigh(_resource_avail_handler);

		_handle_config();

		/* cue for run scripts to start GDB */
		log("monitor ready");
	}

	/**
	 * Thread_monitor interface
	 */

	void set_initial_breakpoint(Capability<Pd_session> pd,
	                            addr_t addr,
	                            char original_instruction[]) override
	{
		if (!_gdb_stub.constructed()) {
			Genode::error("set_initial_breakpoint() called without monitor config");
			return;
		}

		Inferior_pd::with_inferior_pd(_env.ep(), pd,
		                              [&] (Inferior_pd &inferior) {
			_gdb_stub->_state.read_memory(inferior,
				Memory_accessor::Virt_addr { addr },
				Byte_range_ptr { original_instruction,
				                 Gdb::breakpoint_instruction_len() });

			_gdb_stub->_state.write_memory(inferior,
				Memory_accessor::Virt_addr { addr },
				Const_byte_range_ptr { Gdb::breakpoint_instruction(),
				                       Gdb::breakpoint_instruction_len() });
		}, [] { });
	}

	void remove_initial_breakpoint(Capability<Pd_session> pd,
	                               addr_t addr,
	                               char const original_instruction[]) override
	{
		if (!_gdb_stub.constructed()) {
			Genode::error("remove_initial_breakpoint() called without monitor config");
			return;
		}

		Inferior_pd::with_inferior_pd(_env.ep(), pd,
		                              [&] (Inferior_pd &inferior) {
			_gdb_stub->_state.write_memory(inferior,
				Memory_accessor::Virt_addr { addr },
				Const_byte_range_ptr { original_instruction,
				                       Gdb::breakpoint_instruction_len() });
		}, [] { });
	}

	void flush(Monitored_thread &thread) override
	{
		if (_gdb_stub.constructed())
			_gdb_stub->flush(thread);
	}

	void thread_stopped(Capability<Pd_session> pd, Monitored_thread &thread) override
	{
		if (!_gdb_stub.constructed()) {
			Genode::error("thread_stopped() called without monitor config");
			return;
		}
		Inferior_pd::with_inferior_pd(_env.ep(), pd,
		                              [&] (Inferior_pd &inferior) {
			_gdb_stub->thread_stopped(inferior, thread);
		}, [] { });
	}
};


template <typename SERVICE>
void Monitor::Main::_handle_service(SERVICE &service)
{
	using Local_session = typename SERVICE::Local_session;

	service.for_each_requested_session([&] (typename SERVICE::Request &request) {
		request.deliver_session(_create_session(service, request));
	});

	service.for_each_upgraded_session([&] (Local_session &session,
	                                       Session::Resources const &amount) {
		session.upgrade(amount);
		return SERVICE::Upgrade_response::CONFIRMED;
	});

	service.for_each_session_to_close([&] (Local_session &session) {
		_destroy_session(session);
		return SERVICE::Close_response::CLOSED;
	});
}


void Component::construct(Genode::Env &env) { static Monitor::Main main(env); }

