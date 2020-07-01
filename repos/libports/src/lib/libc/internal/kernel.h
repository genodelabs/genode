/*
 * \brief  Libc kernel for main and pthreads user contexts
 * \author Christian Helmuth
 * \author Emery Hemingway
 * \author Norman Feske
 * \date   2016-01-22
 */

/*
 * Copyright (C) 2016-2020 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _LIBC__INTERNAL__KERNEL_H_
#define _LIBC__INTERNAL__KERNEL_H_

/* base-internal includes */
#include <internal/call_func.h>

/* libc includes */
#include <libc/select.h>

/* libc-internal includes */
#include <internal/types.h>
#include <internal/malloc_ram_allocator.h>
#include <internal/cloned_malloc_heap_range.h>
#include <internal/timer.h>
#include <internal/pthread_pool.h>
#include <internal/init.h>
#include <internal/env.h>
#include <internal/vfs_plugin.h>
#include <internal/suspend.h>
#include <internal/resume.h>
#include <internal/select.h>
#include <internal/kernel_routine.h>
#include <internal/current_time.h>
#include <internal/kernel_timer_accessor.h>
#include <internal/watch.h>
#include <internal/signal.h>
#include <internal/monitor.h>
#include <internal/pthread.h>

namespace Libc {
	class Kernel;
	class Main_blockade;
	class Main_job;
}


class Libc::Main_blockade : public Blockade
{
	private:

		uint64_t   _timeout_ms;
		bool const _timeout_valid { _timeout_ms != 0 };

		struct Check : Suspend_functor
		{
			bool const &woken_up;

			Check(bool const &woken_up) : woken_up(woken_up) { }

			bool suspend() override { return !woken_up; }
		};

	public:

		Main_blockade(uint64_t timeout_ms) : _timeout_ms(timeout_ms) { }

		void block() override;
		void wakeup() override;
};


class Libc::Main_job : public Monitor::Job
{
	private:

		Main_blockade _blockade;

	public:

		Main_job(Monitor::Function &fn, uint64_t timeout_ms)
		: Job(fn, _blockade), _blockade(timeout_ms)
		{ }
};


/**
 * Libc "kernel"
 *
 * This class represents the "kernel" of the libc-based application
 * Blocking and deblocking happens here on libc functions like read() or
 * select(). This combines blocking of the VFS backend and other signal sources
 * (e.g., timers). The libc task runs on the component thread and allocates a
 * secondary stack for the application task. Context switching uses
 * setjmp/longjmp.
 */
struct Libc::Kernel final : Vfs::Io_response_handler,
                            Entrypoint::Io_progress_handler,
                            Reset_malloc_heap,
                            Resume,
                            Suspend,
                            Monitor,
                            Select,
                            Kernel_routine_scheduler,
                            Current_time,
                            Watch
{
	private:

		/**
		 * Pointer to singleton instance
		 */
		static Kernel *_kernel_ptr;

		Genode::Env &_env;

		/**
		 * Allocator for libc-internal data
		 *
		 * Not mirrored to forked processes. Preserved across 'execve' calls.
		 */
		Genode::Allocator &_heap;

		/**
		 * Name of the current binary's ROM module
		 *
		 * Used by fork, modified by execve.
		 */
		Binary_name _binary_name { "binary" };

		/**
		 * Allocator for application-owned data
		 *
		 * Mirrored to forked processes. Not preserved across 'execve' calls.
		 */
		Reconstructible<Malloc_ram_allocator> _malloc_ram { _heap, _env.ram() };

		Constructible<Heap> _malloc_heap { };

		Registry<Registered<Cloned_malloc_heap_range> > _cloned_heap_ranges { };

		/**
		 * Reset_malloc_heap interface used by execve
		 */
		void reset_malloc_heap() override;

		Env_implementation _libc_env { _env, _heap };

		bool const _update_mtime = _libc_env.libc_config().attribute_value("update_mtime", true);

		Vfs_plugin _vfs { _libc_env, _libc_env.vfs_env(), _heap, *this,
		                  _update_mtime ? Vfs_plugin::Update_mtime::YES
		                                : Vfs_plugin::Update_mtime::NO,
		                  _libc_env.config() };

		bool  const _cloned = _libc_env.libc_config().attribute_value("cloned", false);
		pid_t const _pid    = _libc_env.libc_config().attribute_value("pid", 0U);

		Xml_node _passwd_config()
		{
			return _libc_env.libc_config().has_sub_node("passwd")
			     ? _libc_env.libc_config().sub_node("passwd")
			     : Xml_node("<empty/>");
		}

		Xml_node _pthread_config()
		{
			return _libc_env.libc_config().has_sub_node("pthread")
			     ? _libc_env.libc_config().sub_node("pthread")
			     : Xml_node("<pthread/>");
		}

		typedef String<Vfs::MAX_PATH_LEN> Config_attr;

		Config_attr const _rtc_path = _libc_env.libc_config().attribute_value("rtc", Config_attr());

		/* handler for watching the stdout's info pseudo file */
		Constructible<Watch_handler<Kernel>> _terminal_resize_handler { };

		void _handle_terminal_resize();

		/* handler for watching user interrupts (control-c) */
		Constructible<Watch_handler<Kernel>> _user_interrupt_handler { };

		void _handle_user_interrupt();

		Signal _signal { _pid };

		Reconstructible<Io_signal_handler<Kernel>> _resume_main_handler {
			_env.ep(), *this, &Kernel::_resume_main };

		jmp_buf _kernel_context;
		jmp_buf _user_context;
		bool    _valid_user_context          = false;
		bool    _dispatch_pending_io_signals = false;

		/* io_progress_handler marker */
		bool _io_ready { false };

		Thread &_myself { *Thread::myself() };

		addr_t _kernel_stack = Thread::mystack().top;

		size_t _user_stack_size();

		void *_user_stack = {
			_myself.alloc_secondary_stack(_myself.name().string(),
			                              _user_stack_size()) };

		void (*_original_suspended_callback)() = nullptr;

		enum State { KERNEL, USER };

		State _state = KERNEL;

		Application_code *_nested_app_code = nullptr;

		Application_code *_app_code     = nullptr;
		bool              _app_returned = false;

		bool _resume_main_once  = false;
		bool _suspend_scheduled = false;

		Select_handler_base *_scheduled_select_handler = nullptr;

		Kernel_routine *_kernel_routine = nullptr;

		void _resume_main() { _resume_main_once = true; }

		Kernel_timer_accessor _timer_accessor { _env };

		struct Main_timeout : Timeout_handler
		{
			Timer_accessor        &_timer_accessor;
			Constructible<Timeout> _timeout;
			Kernel                &_kernel;

			void _construct_timeout_once()
			{
				if (!_timeout.constructed())
					_timeout.construct(_timer_accessor, *this);
			}

			Main_timeout(Timer_accessor &timer_accessor, Kernel &kernel)
			: _timer_accessor(timer_accessor), _kernel(kernel)
			{ }

			void timeout(uint64_t timeout_ms)
			{
				_construct_timeout_once();
				_timeout->start(timeout_ms);
			}

			uint64_t duration_left()
			{
				_construct_timeout_once();
				return _timeout->duration_left();
			}

			void handle_timeout() override
			{
				_kernel._resume_main();
			}
		};

		Main_timeout _main_timeout { _timer_accessor, *this };

		Pthread_pool _pthreads { _timer_accessor };

		Monitor::Pool _monitors { };

		Reconstructible<Io_signal_handler<Kernel>> _execute_monitors {
			_env.ep(), *this, &Kernel::_monitors_handler };

		void _monitors_handler()
		{
			_monitors.execute_monitors();
		}

		Constructible<Clone_connection> _clone_connection { };

		struct Resumer
		{
			GENODE_RPC(Rpc_resume, void, resume);
			GENODE_RPC_INTERFACE(Rpc_resume);
		};

		struct Resumer_component : Rpc_object<Resumer, Resumer_component>
		{
			Kernel &_kernel;

			Resumer_component(Kernel &kernel) : _kernel(kernel) { }

			void resume() { _kernel.run_after_resume(); }
		};

		/**
		 * Trampoline to application (user) code
		 *
		 * This function is called by the main thread.
		 */
		static void _user_entry(Kernel *kernel)
		{
			struct Check : Suspend_functor {
				bool suspend() override { return true; }
			} check;

			kernel->_app_code->execute();
			kernel->_app_returned = true;
			kernel->_suspend_main(check, 0);
		}

		bool _main_context() const { return &_myself == Thread::myself(); }

		/**
		 * Utility to switch main context to kernel
		 *
		 * User context must be saved explicitly before this function is called
		 * to enable _switch_to_user() later.
		 */
		void _switch_to_kernel()
		{
			_state = KERNEL;
			_longjmp(_kernel_context, 1);
		}

		/**
		 * Utility to switch main context to user
		 *
		 * Kernel context must be saved explicitly before this function is called
		 * to enable _switch_to_kernel() later.
		 */
		void _switch_to_user()
		{
			if (!_valid_user_context)
				error("switching to invalid user context");

			_resume_main_once = false;
			_state = USER;
			_longjmp(_user_context, 1);
		}

		uint64_t _suspend_main(Suspend_functor &check, uint64_t timeout_ms)
		{
			/* check that we're not running on libc kernel context */
			if (Thread::mystack().top == _kernel_stack) {
				error("libc suspend() called from non-user context (",
				      __builtin_return_address(0), ") - aborting");
				exit(1);
			}

			if (!check.suspend() && !_kernel_routine)
				return timeout_ms;

			if (timeout_ms > 0)
				_main_timeout.timeout(timeout_ms);

			if (!_setjmp(_user_context)) {
				_valid_user_context = true;
				_switch_to_kernel();
			} else {
				_valid_user_context = false;
				_signal.execute_signal_handlers();
			}

			/*
			 * During the suspension of the application code a nested
			 * Libc::with_libc() call took place, which will be executed
			 * before returning to the first Libc::with_libc() call.
			 */
			if (_nested_app_code) {

				/*
				 * We have to explicitly set the user context back to true
				 * because we are borrowing it to execute our nested application
				 * code.
				 */
				_valid_user_context = true;

				_nested_app_code->execute();
				_nested_app_code = nullptr;
				_longjmp(_kernel_context, 1);
			}

			return timeout_ms > 0 ? _main_timeout.duration_left() : 0;
		}

		void _init_file_descriptors();

		void _clone_state_from_parent();

	public:

		Kernel(Genode::Env &env, Genode::Allocator &heap);

		~Kernel() { error(__PRETTY_FUNCTION__, " should not be executed!"); }

		Libc::Env & libc_env() { return _libc_env; }

		/**
		 * Setup kernel context and run libc application main context
		 *
		 * This function is called by the component thread on with_libc().
		 */
		void run(Application_code &app_code)
		{
			if (!_main_context() || _state != KERNEL) {
				error(__PRETTY_FUNCTION__, " called from non-kernel context");
				return;
			}

			_resume_main_once = false;
			_app_returned     = false;
			_app_code         = &app_code;

			/* save continuation of libc kernel (incl. current stack) */
			if (!_setjmp(_kernel_context)) {
				/* _setjmp() returned directly -> switch to user stack and call application code */

				if (_cloned) {
					_switch_to_user();
				} else {
					_state = USER;
					call_func(_user_stack, (void *)_user_entry, (void *)this);
				}

				/* never reached */
			}

			/* _setjmp() returned after _longjmp() - user context suspended */

			while ((!_app_returned) && (!_suspend_scheduled)) {

				if (_kernel_routine) {
					Kernel_routine &routine = *_kernel_routine;

					/* the 'kernel_routine' may install another kernel routine */
					_kernel_routine = nullptr;
					routine.execute_in_kernel();
					if (!_kernel_routine)
						_switch_to_user();
				}

				if (_dispatch_pending_io_signals) {
					/* dispatch pending signals but don't block */
					while (_env.ep().dispatch_pending_io_signal()) ;
				} else {
					/* block for signals */
					_env.ep().wait_and_dispatch_one_io_signal();
				}

				if (!_kernel_routine && _resume_main_once && !_setjmp(_kernel_context))
					_switch_to_user();
			}

			_suspend_scheduled = false;
		}

		/*
		 * Run libc application main context after suspend and resume
		 */
		void run_after_resume()
		{
			if (!_setjmp(_kernel_context))
				_switch_to_user();

			while ((!_app_returned) && (!_suspend_scheduled)) {
				_env.ep().wait_and_dispatch_one_io_signal();
				if (_resume_main_once && !_setjmp(_kernel_context))
					_switch_to_user();
			}

			_suspend_scheduled = false;
		}

		/**
		 * Resume interface
		 */
		void resume_all() override
		{
			if (_app_returned) {
				if (_scheduled_select_handler)
					_scheduled_select_handler->dispatch_select();
			} else {
				if (_main_context())
					_resume_main();
				else
					Signal_transmitter(*_resume_main_handler).submit();
			}

			_pthreads.resume_all();
		}

		/**
		 * Suspend interface
		 */
		uint64_t suspend(Suspend_functor &check, uint64_t timeout_ms) override
		{
			if (timeout_ms > 0
			 && timeout_ms > _timer_accessor.timer().max_timeout()) {
				warning("libc: limiting exceeding timeout of ",
				        timeout_ms, " ms to maximum of ",
				        _timer_accessor.timer().max_timeout(), " ms");

				timeout_ms = min(timeout_ms, _timer_accessor.timer().max_timeout());
			}

			return _main_context() ? _suspend_main(check, timeout_ms)
			                       : _pthreads.suspend_myself(check, timeout_ms);
		}

		void dispatch_pending_io_signals()
		{
			if (!_main_context()) return;

			if (!_setjmp(_user_context)) {
				_valid_user_context          = true;
				_dispatch_pending_io_signals = true;
				_resume_main_once            = true; /* afterwards resume main */
				_switch_to_kernel();
			} else {
				_valid_user_context          = false;
				_dispatch_pending_io_signals = false;
				_signal.execute_signal_handlers();
			}
		}

		/**
		 * Monitor interface
		 */
		bool _monitor(Mutex &mutex, Function &fn, uint64_t timeout_ms) override
		{
			if (_main_context()) {
				Main_job job { fn, timeout_ms };

				_monitors.monitor(mutex, job);
				return job.completed();

			} else {
				Pthread_job job { fn, _timer_accessor, timeout_ms };

				_monitors.monitor(mutex, job);
				return job.completed();
			}
		}

		void _charge_monitors() override
		{
			if (_monitors.charge_monitors())
				Signal_transmitter(*_execute_monitors).submit();
		}

		/**
		 * Current_time interface
		 */
		Duration current_time() override
		{
			return _timer_accessor.timer().curr_time();
		}

		/**
		 * Called from the main context (by fork)
		 */
		void schedule_suspend(void(*original_suspended_callback) ());

		/**
		 * Select interface
		 */
		void schedule_select(Select_handler_base &h) override
		{
			_scheduled_select_handler = &h;
		}

		/**
		 * Select interface
		 */
		void deschedule_select() override
		{
			_scheduled_select_handler = nullptr;
		}

		/**
		 * Called from the context of the initial thread (on fork)
		 */
		void entrypoint_suspended()
		{
			_resume_main_handler.destruct();

			_original_suspended_callback();
		}

		/**
		 * Called from the context of the initial thread (after fork)
		 */
		void entrypoint_resumed()
		{
			_resume_main_handler.construct(_env.ep(), *this, &Kernel::_resume_main);

			Resumer_component resumer { *this };

			Capability<Resumer> resumer_cap =
				_env.ep().rpc_ep().manage(&resumer);

			resumer_cap.call<Resumer::Rpc_resume>();

			_env.ep().rpc_ep().dissolve(&resumer);
		}

		/**
		 * Return if main is currently suspended
		 */
		bool main_suspended() { return _state == KERNEL; }

		/**
		 * Public alias for _main_context()
		 */
		bool main_context() const { return _main_context(); }

		void resume_main()
		{
			if (_main_context())
				_resume_main();
			else
				Signal_transmitter(*_resume_main_handler).submit();
		}

		/**
		 * Execute application code while already executing in run()
		 */
		void nested_execution(Application_code &app_code)
		{
			_nested_app_code = &app_code;

			if (!_setjmp(_kernel_context)) {
				_switch_to_user();
			}
		}

		/**
		 * Watch interface
		 */
		Vfs::Vfs_watch_handle *alloc_watch_handle(char const *path) override
		{
			Vfs::Vfs_watch_handle *watch_handle { nullptr };
			typedef Vfs::Directory_service::Watch_result Result;
			return _libc_env.vfs().watch(path, &watch_handle, _heap) == Result::WATCH_OK
				? watch_handle : nullptr;
		}


		/****************************************
		 ** Vfs::Io_response_handler interface **
		 ****************************************/

		void read_ready_response() override {
			_io_ready = true; }

		void io_progress_response() override {
			_io_ready = true; }


		/**********************************************
		 * Entrypoint::Io_progress_handler interface **
		 **********************************************/

		void handle_io_progress() override;

		/**
		 * Kernel_routine_scheduler interface
		 */
		void register_kernel_routine(Kernel_routine &kernel_routine) override
		{
			_kernel_routine = &kernel_routine;
		}


		/********************************
		 ** Access to kernel singleton **
		 ********************************/

		struct Kernel_called_prior_initialization : Exception { };

		static Kernel &kernel()
		{
			if (!_kernel_ptr)
				throw Kernel_called_prior_initialization();

			return *_kernel_ptr;
		}
};

#endif /* _LIBC__INTERNAL__KERNEL_H_ */
