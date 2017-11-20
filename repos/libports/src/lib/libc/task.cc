/*
 * \brief  Libc kernel for main and pthreads user contexts
 * \author Christian Helmuth
 * \author Emery Hemingway
 * \date   2016-01-22
 */

/*
 * Copyright (C) 2016-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Genode includes */
#include <base/component.h>
#include <base/log.h>
#include <base/thread.h>
#include <base/rpc_server.h>
#include <base/rpc_client.h>
#include <base/heap.h>
#include <base/attached_rom_dataspace.h>
#include <vfs/file_system_factory.h>
#include <vfs/dir_file_system.h>
#include <timer_session/connection.h>

/* libc includes */
#include <libc/component.h>
#include <libc/select.h>
#include <libc-plugin/plugin_registry.h>

/* libc-internal includes */
#include <internal/call_func.h>
#include <base/internal/unmanaged_singleton.h>
#include "vfs_plugin.h"
#include "libc_init.h"
#include "task.h"

extern char **environ;

namespace Libc {
	class Env_implementation;
	class Kernel;
	class Pthreads;
	class Timer;
	class Timer_accessor;
	class Timeout;
	class Timeout_handler;
	class Io_response_handler;

	using Genode::Microseconds;
}


class Libc::Env_implementation : public Libc::Env
{
	private:

		Genode::Env &_env;

		Genode::Attached_rom_dataspace _config { _env, "config" };

		Genode::Xml_node _vfs_config()
		{
			try { return _config.xml().sub_node("vfs"); }
			catch (Genode::Xml_node::Nonexistent_sub_node) { }
			try {
				Genode::Xml_node node =
					_config.xml().sub_node("libc").sub_node("vfs");
				Genode::warning("'<config> <libc> <vfs/>' is deprecated, "
				                "please move to '<config> <vfs/>'");
				return node;
			}
			catch (Genode::Xml_node::Nonexistent_sub_node) { }

			return Genode::Xml_node("<vfs/>");
		}

		Genode::Xml_node _libc_config()
		{
			try { return _config.xml().sub_node("libc"); }
			catch (Genode::Xml_node::Nonexistent_sub_node) { }

			return Genode::Xml_node("<libc/>");
		}

		Vfs::Global_file_system_factory _file_system_factory;
		Vfs::Dir_file_system            _vfs;

		Genode::Xml_node _config_xml() const override {
			return _config.xml(); };

	public:

		Env_implementation(Genode::Env &env, Genode::Allocator &alloc,
		                   Vfs::Io_response_handler &io_response_handler)
		:
			_env(env), _file_system_factory(alloc),
			_vfs(_env, alloc, _vfs_config(), io_response_handler,
			     _file_system_factory, Vfs::Dir_file_system::Root())
		{ }


		/*************************
		 ** Libc::Env interface **
		 *************************/

		Vfs::File_system &vfs() override {
			return _vfs; }

		Genode::Xml_node libc_config() override {
			return _libc_config(); }


		/***************************
		 ** Genode::Env interface **
		 ***************************/

		Parent &parent() override {
			return _env.parent(); }

		Ram_session &ram() override {
			return _env.ram(); }

		Cpu_session &cpu() override {
			return _env.cpu(); }

		Region_map &rm() override {
			return _env.rm(); }

		Pd_session &pd() override {
			return _env.pd(); }

		Entrypoint &ep() override {
			return _env.ep(); }

		Ram_session_capability ram_session_cap() override {
			return _env.ram_session_cap(); }
		Cpu_session_capability cpu_session_cap() override {
			return _env.cpu_session_cap(); }

		Pd_session_capability pd_session_cap() override {
			return _env.pd_session_cap(); }

		Id_space<Parent::Client> &id_space() override {
			return _env.id_space(); }

		Session_capability session(Parent::Service_name const &name,
	                                   Parent::Client::Id id,
	                                   Parent::Session_args const &args,
	                                   Affinity             const &aff) override {
			return _env.session(name, id, args, aff); }


		void upgrade(Parent::Client::Id id,
		             Parent::Upgrade_args const &args) override {
			return _env.upgrade(id, args); }

		void close(Parent::Client::Id id) override {
			return _env.close(id); }

		/* already done by the libc */
		void exec_static_constructors() override { }

		void reinit(Native_capability::Raw raw) override {
			_env.reinit(raw); }

		void reinit_main_thread(Capability<Region_map> &stack_area_rm) override {
			_env.reinit_main_thread(stack_area_rm); }
};


struct Libc::Timer
{
	::Timer::Connection _timer;

	Timer(Genode::Env &env) : _timer(env) { }

	unsigned long curr_time()
	{
		return _timer.curr_time().trunc_to_plain_us().value/1000;
	}

	static Microseconds microseconds(unsigned long timeout_ms)
	{
		return Microseconds(1000*timeout_ms);
	}

	static unsigned long max_timeout()
	{
		return ~0UL/1000;
	}
};


/**
 * Interface for obtaining the libc-global timer instance
 *
 * The 'Timer' is instantiated on demand whenever the 'Timer_accessor::timer'
 * method is first called. This way, libc-using components do not depend of a
 * timer connection unless they actually use time-related functionality.
 */
struct Libc::Timer_accessor
{
	virtual Timer &timer() = 0;
};


struct Libc::Timeout_handler
{
	virtual void handle_timeout() = 0;
};


/*
 * TODO curr_time wrapping
 */
struct Libc::Timeout
{
	Libc::Timer_accessor               &_timer_accessor;
	Timeout_handler                    &_handler;
	::Timer::One_shot_timeout<Timeout>  _timeout;

	bool          _expired             = true;
	unsigned long _absolute_timeout_ms = 0;

	void _handle(Duration now)
	{
		_expired             = true;
		_absolute_timeout_ms = 0;
		_handler.handle_timeout();
	}

	Timeout(Timer_accessor &timer_accessor, Timeout_handler &handler)
	:
		_timer_accessor(timer_accessor),
		_handler(handler),
		_timeout(_timer_accessor.timer()._timer, *this, &Timeout::_handle)
	{ }

	void start(unsigned long timeout_ms)
	{
		unsigned long const now = _timer_accessor.timer().curr_time();

		_expired             = false;
		_absolute_timeout_ms = now + timeout_ms;

		_timeout.schedule(_timer_accessor.timer().microseconds(timeout_ms));
	}

	unsigned long duration_left() const
	{
		unsigned long const now = _timer_accessor.timer().curr_time();

		if (_expired || _absolute_timeout_ms < now)
			return 0;

		return _absolute_timeout_ms - now;
	}
};


struct Libc::Pthreads
{
	struct Pthread : Timeout_handler
	{
		Genode::Lock  lock { Genode::Lock::LOCKED };
		Pthread      *next { nullptr };

		Timer_accessor         &_timer_accessor;
		Constructible<Timeout>  _timeout;

		void _construct_timeout_once()
		{
			if (!_timeout.constructed())
				_timeout.construct(_timer_accessor, *this);
		}

		Pthread(Timer_accessor &timer_accessor, unsigned long timeout_ms)
		: _timer_accessor(timer_accessor)
		{
			if (timeout_ms > 0) {
				_construct_timeout_once();
				_timeout->start(timeout_ms);
			}
		}

		unsigned long duration_left()
		{
			_construct_timeout_once();
			return _timeout->duration_left();
		}

		void handle_timeout() override
		{
			lock.unlock();
		}
	};

	Genode::Lock    mutex;
	Pthread        *pthreads = nullptr;
	Timer_accessor &timer_accessor;


	Pthreads(Timer_accessor &timer_accessor)
	: timer_accessor(timer_accessor) { }

	void resume_all()
	{
		Genode::Lock::Guard g(mutex);

		for (Pthread *p = pthreads; p; p = p->next)
			p->lock.unlock();
	}

	unsigned long suspend_myself(Suspend_functor & check, unsigned long timeout_ms)
	{
		Pthread myself { timer_accessor, timeout_ms };
		{
			Genode::Lock::Guard g(mutex);

			myself.next = pthreads;
			pthreads    = &myself;
		}

		if (check.suspend())
			myself.lock.lock();

		{
			Genode::Lock::Guard g(mutex);

			/* address of pointer to next pthread allows to change the head */
			for (Pthread **next = &pthreads; *next; next = &(*next)->next) {
				if (*next == &myself) {
					*next = myself.next;
					break;
				}
			}
		}

		return timeout_ms > 0 ? myself.duration_left() : 0;
	}
};


extern void (*libc_select_notify)();

struct Libc::Io_response_handler : Vfs::Io_response_handler
{
	void handle_io_response(Vfs::Vfs_handle::Context *) override
	{
		/* some contexts may have been deblocked from select() */
		if (libc_select_notify)
			libc_select_notify();

		/* resume all as any context may have been deblocked from blocking I/O */
		Libc::resume_all();
	}
};


/* internal utility */
static void resumed_callback();
static void suspended_callback();


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
struct Libc::Kernel
{
	private:

		Genode::Env         &_env;
		Genode::Allocator   &_heap;
		Io_response_handler  _io_response_handler;
		Env_implementation   _libc_env { _env, _heap, _io_response_handler };
		Vfs_plugin           _vfs { _libc_env, _heap };

		Genode::Reconstructible<Genode::Io_signal_handler<Kernel>> _resume_main_handler {
			_env.ep(), *this, &Kernel::_resume_main };

		jmp_buf _kernel_context;
		jmp_buf _user_context;
		bool    _valid_user_context          = false;
		bool    _dispatch_pending_io_signals = false;

		Genode::Thread &_myself { *Genode::Thread::myself() };

		void *_user_stack = {
			_myself.alloc_secondary_stack(_myself.name().string(),
			                              Component::stack_size()) };

		void (*_original_suspended_callback)() = nullptr;

		enum State { KERNEL, USER };

		State _state = KERNEL;

		Application_code *_nested_app_code = nullptr;

		Application_code *_app_code     = nullptr;
		bool              _app_returned = false;

		bool _resume_main_once  = false;
		bool _suspend_scheduled = false;

		Select_handler_base *_scheduled_select_handler = nullptr;

		void _resume_main() { _resume_main_once = true; }

		struct Timer_accessor : Libc::Timer_accessor
		{
			Genode::Env &_env;

			/*
			 * The '_timer' is constructed by whatever thread (main thread
			 * of pthread) that uses a time-related function first. Hence,
			 * the construction must be protected by a lock.
			 */
			Genode::Lock _lock;

			Genode::Constructible<Timer> _timer;

			Timer_accessor(Genode::Env &env) : _env(env) { }

			Timer &timer() override
			{
				Lock::Guard guard(_lock);

				if (!_timer.constructed())
					_timer.construct(_env);

				return *_timer;
			}
		};

		Timer_accessor _timer_accessor { _env };

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

			void timeout(unsigned long timeout_ms)
			{
				_construct_timeout_once();
				_timeout->start(timeout_ms);
			}

			unsigned long duration_left()
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

		Pthreads _pthreads { _timer_accessor };

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
		static void _user_entry(Libc::Kernel *kernel)
		{
			struct Check : Suspend_functor {
				bool suspend() override { return true; }
			} check;

			kernel->_app_code->execute();
			kernel->_app_returned = true;
			kernel->_suspend_main(check, 0);
		}

		bool _main_context() const { return &_myself == Genode::Thread::myself(); }

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
				Genode::error("switching to invalid user context");

			_resume_main_once = false;
			_state = USER;
			_longjmp(_user_context, 1);
		}

		unsigned long _suspend_main(Suspend_functor &check,
		                            unsigned long timeout_ms)
		{
			/* check if we're running on the user context */
			if (Thread::myself()->mystack().top != (Genode::addr_t)_user_stack) {
				error("libc suspend() called from non-user context (",
				      __builtin_return_address(0), ") - aborting");
				exit(1);
			}

			if (!check.suspend())
				return 0;

			if (timeout_ms > 0)
				_main_timeout.timeout(timeout_ms);

			if (!_setjmp(_user_context)) {
				_valid_user_context = true;
				_switch_to_kernel();
			} else {
				_valid_user_context = false;
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

	public:

		Kernel(Genode::Env &env, Genode::Allocator &heap)
		: _env(env), _heap(heap) { }

		~Kernel() { Genode::error(__PRETTY_FUNCTION__, " should not be executed!"); }

		Libc::Env & libc_env() { return _libc_env; }

		/**
		 * Setup kernel context and run libc application main context
		 *
		 * This function is called by the component thread on with_libc().
		 */
		void run(Libc::Application_code &app_code)
		{
			if (!_main_context() || _state != KERNEL) {
				Genode::error(__PRETTY_FUNCTION__, " called from non-kernel context");
				return;
			}

			_resume_main_once = false;
			_app_returned     = false;
			_app_code         = &app_code;

			/* save continuation of libc kernel (incl. current stack) */
			if (!_setjmp(_kernel_context)) {
				/* _setjmp() returned directly -> switch to user stack and call application code */
				_state = USER;
				call_func(_user_stack, (void *)_user_entry, (void *)this);

				/* never reached */
			}

			/* _setjmp() returned after _longjmp() - user context suspended */

			while ((!_app_returned) && (!_suspend_scheduled)) {
				if (_dispatch_pending_io_signals) {
					/* dispatch pending signals but don't block */
					while (_env.ep().dispatch_pending_io_signal()) ;
				} else {
					/* block for signals */
					_env.ep().wait_and_dispatch_one_io_signal();
				}

				if (_resume_main_once && !_setjmp(_kernel_context))
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
		 * Resume all contexts (main and pthreads)
		 */
		void resume_all()
		{
			if (_app_returned) {
				if (_scheduled_select_handler)
					_scheduled_select_handler->dispatch_select();
			} else {
				if (_main_context())
					_resume_main();
				else
					Genode::Signal_transmitter(*_resume_main_handler).submit();
			}

			_pthreads.resume_all();
		}

		/**
		 * Suspend this context (main or pthread)
		 */
		unsigned long suspend(Suspend_functor &check, unsigned long timeout_ms)
		{
			if (timeout_ms > 0
			 && timeout_ms > _timer_accessor.timer().max_timeout()) {
				Genode::warning("libc: limiting exceeding timeout of ",
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
			}
		}

		unsigned long current_time()
		{
			return _timer_accessor.timer().curr_time();
		}

		/**
		 * Called from the main context (by fork)
		 */
		void schedule_suspend(void(*original_suspended_callback) ())
		{
			if (_state != USER) {
				Genode::error(__PRETTY_FUNCTION__, " called from non-user context");
				return;
			}

			/*
			 * We hook into suspend-resume callback chain to destruct and
			 * reconstruct parts of the kernel from the context of the initial
			 * thread, i.e., without holding any object locks.
			 */
			_original_suspended_callback = original_suspended_callback;
			_env.ep().schedule_suspend(suspended_callback, resumed_callback);

			if (!_setjmp(_user_context)) {
				_valid_user_context = true;
				_suspend_scheduled = true;
				_switch_to_kernel();
			} else {
				_valid_user_context = false;
			}
		}

		void schedule_select(Select_handler_base *h)
		{
			_scheduled_select_handler = h;
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
		 * Execute application code while already executing in run()
		 */
		void nested_execution(Libc::Application_code &app_code)
		{
			_nested_app_code = &app_code;

			if (!_setjmp(_kernel_context)) {
				_switch_to_user();
			}
		}
};


/**
 * Libc kernel singleton
 *
 * The singleton is implemented with the unmanaged-singleton utility
 * in Component::construct() to ensure it is never destructed
 * like normal static global objects. Otherwise, the task object may be
 * destructed in a RPC to Rpc_resume, which would result in a deadlock.
 */
static Libc::Kernel *kernel;


/**
 * Main context execution was suspended (on fork)
 *
 * This function is executed in the context of the initial thread.
 */
static void suspended_callback() { kernel->entrypoint_suspended(); }


/**
 * Resume main context execution (after fork)
 *
 * This function is executed in the context of the initial thread.
 */
static void resumed_callback() { kernel->entrypoint_resumed(); }


/*******************
 ** Libc task API **
 *******************/

void Libc::resume_all() { kernel->resume_all(); }


unsigned long Libc::suspend(Suspend_functor &s, unsigned long timeout_ms)
{
	if (!kernel) {
		error("libc kernel not initialized, needed for suspend()");
		exit(1);
	}
	return kernel->suspend(s, timeout_ms);
}


void Libc::dispatch_pending_io_signals()
{
	kernel->dispatch_pending_io_signals();
}


unsigned long Libc::current_time()
{
	return kernel->current_time();
}


void Libc::schedule_suspend(void (*suspended) ())
{
	if (!kernel) {
		error("libc kernel not initialized, needed for fork()");
		exit(1);
	}
	kernel->schedule_suspend(suspended);
}


void Libc::schedule_select(Libc::Select_handler_base *h)
{
	if (!kernel) {
		error("libc kernel not initialized, needed for select()");
		exit(1);
	}
	kernel->schedule_select(h);
}


void Libc::execute_in_application_context(Libc::Application_code &app_code)
{
	/*
	 * XXX We don't support a second entrypoint - pthreads should work as they
	 *     don't use this code.
	 */

	if (!kernel) {
		error("libc kernel not initialized, needed for with_libc()");
		exit(1);
	}

	static bool nested = false;

	if (nested) {

		if (kernel->main_suspended()) {
			kernel->nested_execution(app_code);
		} else {
			app_code.execute();
		}
		return;
	}

	nested = true;
	kernel->run(app_code);
	nested = false;
}


/***************************
 ** Component entry point **
 ***************************/

Genode::size_t Component::stack_size() { return Libc::Component::stack_size(); }


void Component::construct(Genode::Env &env)
{
	/* initialize the global pointer to environment variables */
	static char *null_env = nullptr;
	if (!environ) environ = &null_env;

	Genode::Allocator &heap =
		*unmanaged_singleton<Genode::Heap>(env.ram(), env.rm());

	/* pass Genode::Env to libc subsystems that depend on it */
	Libc::init_malloc(heap);
	Libc::init_mem_alloc(env);
	Libc::init_dl(env);
	Libc::sysctl_init(env);

	kernel = unmanaged_singleton<Libc::Kernel>(env, heap);

	Libc::libc_config_init(kernel->libc_env().libc_config());

	/*
	 * XXX The following two steps leave us with the dilemma that we don't know
	 * which linked library may depend on the successfull initialization of a
	 * plugin. For example, some high-level library may try to open a network
	 * connection in its constructor before the network-stack library is
	 * initialized. But, we can't initialize plugins before calling static
	 * constructors as those are needed to know about the libc plugin. The only
	 * solution is to remove all libc plugins beside the VFS implementation,
	 * which is our final goal anyway.
	 */

	/* finish static construction of component and libraries */
	Libc::with_libc([&] () { env.exec_static_constructors(); });

	/* initialize plugins that require Genode::Env */
	auto init_plugin = [&] (Libc::Plugin &plugin) {
		plugin.init(env);
	};
	Libc::plugin_registry()->for_each_plugin(init_plugin);

	/* construct libc component on kernel stack */
	Libc::Component::construct(kernel->libc_env());
}


/**
 * Default stack size for libc-using components
 */
Genode::size_t Libc::Component::stack_size() __attribute__((weak));
Genode::size_t Libc::Component::stack_size() { return 32UL*1024*sizeof(long); }
