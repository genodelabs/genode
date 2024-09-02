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
#include <util/reconstructible.h>
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
#include <internal/current_time.h>
#include <internal/kernel_timer_accessor.h>
#include <internal/watch.h>
#include <internal/signal.h>
#include <internal/monitor.h>
#include <internal/pthread.h>
#include <internal/cwd.h>
#include <internal/atexit.h>
#include <internal/rtc.h>

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
struct Libc::Kernel final : Vfs::Read_ready_response_handler,
                            Entrypoint::Io_progress_handler,
                            Reset_malloc_heap,
                            Resume,
                            Suspend,
                            Monitor,
                            Select,
                            Current_time,
                            Current_real_time,
                            Watch,
                            Cwd
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

		/* io_progress_handler marker */
		bool _io_progressed = false;

		struct Vfs_user : Vfs::Env::User
		{
			bool &_io_progressed;

			Vfs_user(bool &io_progressed) : _io_progressed(io_progressed) { }

			void wakeup_vfs_user() override
			{
				_io_progressed = true;
			}
		};

		Vfs_user _vfs_user { _io_progressed };

		Env_implementation _libc_env { _env, _heap, _vfs_user };

		bool const _update_mtime = _libc_env.libc_config().attribute_value("update_mtime", true);

		Vfs_plugin _vfs { _libc_env, _libc_env.vfs_env(), _heap, *this,
		                  _update_mtime ? Vfs_plugin::Update_mtime::YES
		                                : Vfs_plugin::Update_mtime::NO,
		                  *this /* current_real_time */,
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

		using Config_attr = String<Vfs::MAX_PATH_LEN>;

		Config_attr const _rtc_path = _libc_env.libc_config().attribute_value("rtc", Config_attr());

		Constructible<Rtc> _rtc { };

		/* handler for watching the stdout's info pseudo file */
		Constructible<Io::Watch_handler<Kernel>> _terminal_resize_handler { };

		void _handle_terminal_resize();

		/* handler for watching user interrupts (control-c) */
		Constructible<Io::Watch_handler<Kernel>> _user_interrupt_handler { };

		void _handle_user_interrupt();

		Signal _signal { _pid };

		Atexit _atexit { _heap };

		Io_signal_handler<Kernel> _resume_main_handler {
			_env.ep(), *this, &Kernel::_resume_main };

		jmp_buf _kernel_context;
		jmp_buf _user_context;
		bool    _valid_user_context          = false;

		Thread &_myself { *Thread::myself() };

		addr_t _kernel_stack = Thread::mystack().top;

		size_t _user_stack_size();

		void *_user_stack = {
			_myself.alloc_secondary_stack(_myself.name().string(),
			                              _user_stack_size()) };

		enum State { KERNEL, USER };

		State _state = KERNEL;

		Application_code *_nested_app_code = nullptr;

		Application_code *_app_code     = nullptr;
		bool              _app_returned = false;

		bool _resume_main_once  = false;

		Select_handler_base *_scheduled_select_handler = nullptr;

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

		Monitor::Pool _monitors { *this };

		Io_signal_handler<Kernel> _execute_monitors {
			_env.ep(), *this, &Kernel::_monitors_handler };

		Monitor::Pool::State _execute_monitors_pending = Monitor::Pool::State::ALL_COMPLETE;

		Constructible<Main_job> _main_monitor_job { };

		void _monitors_handler()
		{
			/* mark monitors for execution when running in kernel only */
			_execute_monitors_pending = Monitor::Pool::State::JOBS_PENDING;
			_io_progressed = true;
		}

		Constructible<Clone_connection> _clone_connection { };

		Absolute_path _cwd { "/" };


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

			if (!check.suspend())
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
					_main_monitor_job->complete();
					_switch_to_user();
				} else {
					_state = USER;
					call_func(_user_stack, (void *)_user_entry, (void *)this);
				}

				/* never reached */
			}

			/* _setjmp() returned after _longjmp() - user context suspended */

			while ((!_app_returned)) {

				/*
				 * Dispatch all pending I/O signals at once and execute
				 * monitors that may now become able to complete.
				 */
				auto dispatch_all_pending_io_signals = [&] ()
				{
					while (_env.ep().dispatch_pending_io_signal());
				};

				dispatch_all_pending_io_signals();

				if (_io_progressed)
					Kernel::resume_all();

				_io_progressed = false;

				/*
				 * Execute monitors on kernel entry regardless of any I/O
				 * because the monitor function may be unrelated to I/O.
				 */
				if (_execute_monitors_pending == Monitor::Pool::State::JOBS_PENDING)
					_execute_monitors_pending = _monitors.execute_monitors();

				/*
				 * Process I/O signals without returning to the application
				 * as long as the main thread depends on I/O.
				 */

				auto main_blocked_in_monitor = [&] ()
				{
					/*
					 * In general, 'resume_all()' only flags the main state but
					 * does not alter the main monitor job. For exmaple in case
					 * of a sleep timeout, main is resumed by 'resume_main()'
					 * in 'Main_blockade::wakeup()' but did not yet return from
					 * 'suspend()'. The expired state in the main job is set
					 * only after 'suspend()' returned.
					 */
					if (_resume_main_once)
						return false;

					return _main_monitor_job.constructed()
					   && !_main_monitor_job->completed()
					   && !_main_monitor_job->expired();
				};

				auto main_suspended_for_io = [&] {
					return _resume_main_once == false; };

				while (main_blocked_in_monitor() || main_suspended_for_io()) {

					wakeup_remote_peers();

					/*
					 * Block for one I/O signal and process all pending ones
					 * before executing the monitor functions. This avoids
					 * superflous executions of the monitor functions when
					 * receiving bursts of I/O signals.
					 */
					_env.ep().wait_and_dispatch_one_io_signal();

					dispatch_all_pending_io_signals();

					handle_io_progress();
				}

				/*
				 * Return to the application
				 */
				if (_resume_main_once && !_setjmp(_kernel_context)) {
					_switch_to_user();
				}
			}
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
					_resume_main_handler.local_submit();
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

		/**
		 * Monitor interface
		 */
		Monitor::Result _monitor(Function &fn, uint64_t timeout_ms) override
		{
			if (_main_context()) {

				_main_monitor_job.construct(fn, timeout_ms);

				_monitors.monitor(*_main_monitor_job);

				Monitor::Result const job_result = _main_monitor_job->completed()
				                                 ? Monitor::Result::COMPLETE
				                                 : Monitor::Result::TIMEOUT;
				_main_monitor_job.destruct();

				return job_result;

			} else {
				Pthread_job job { fn, _timer_accessor, timeout_ms };

				_monitors.monitor(job);
				return job.completed() ? Monitor::Result::COMPLETE
				                       : Monitor::Result::TIMEOUT;
			}
		}

		/**
		 * Monitor interface
		 */
		void monitor_async(Job &job) override
		{
			_monitors.monitor_async(job);
		}

		void _trigger_monitor_examination() override
		{
			if (_main_context())
				_monitors_handler();
			else
				_execute_monitors.local_submit();
		}

		/**
		 * Current_time interface
		 */
		Duration current_time() override
		{
			return _timer_accessor.timer().curr_time();
		}

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
				_resume_main_handler.local_submit();
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
			using Result = Vfs::Directory_service::Watch_result;
			return _libc_env.vfs().watch(path, &watch_handle, _heap) == Result::WATCH_OK
				? watch_handle : nullptr;
		}

		/**
		 * Cwd interface
		 */
		Absolute_path &cwd() override { return _cwd; }


		/*********************************
		 ** Current_real_time interface **
		 *********************************/

		bool has_real_time() const override
		{
			return (_rtc_path != "");
		}

		timespec current_real_time() override
		{
			if (!_rtc.constructed())
				_rtc.construct(_vfs, _heap, _rtc_path, *this);

			return _rtc->read(current_time());
		}


		/************************************************
		 ** Vfs::Read_ready_response_handler interface **
		 ************************************************/

		void read_ready_response() override  { _io_progressed = true; }


		/**********************************************
		 * Entrypoint::Io_progress_handler interface **
		 **********************************************/

		void handle_io_progress() override;


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


		/******************************
		 ** Vfs::Remote_io mechanism **
		 ******************************/

		void wakeup_remote_peers()
		{
			_libc_env.vfs_env().io().commit();
		}
};

#endif /* _LIBC__INTERNAL__KERNEL_H_ */
