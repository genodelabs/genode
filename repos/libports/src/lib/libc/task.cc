/*
 * \brief  Libc kernel for main and pthreads user contexts
 * \author Christian Helmuth
 * \author Emery Hemingway
 * \date   2016-01-22
 */

/*
 * Copyright (C) 2016-2019 Genode Labs GmbH
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
#include <vfs/simple_env.h>
#include <timer_session/connection.h>

/* libc includes */
#include <libc/component.h>
#include <libc/select.h>
#include <libc-plugin/plugin_registry.h>

/* libc-internal includes */
#include <internal/call_func.h>
#include <base/internal/unmanaged_singleton.h>
#include <clone_session.h>
#include "vfs_plugin.h"
#include "libc_init.h"
#include "task.h"

extern char **environ;

namespace Libc {
	class Env_implementation;
	class Cloned_malloc_heap_range;
	class Malloc_ram_allocator;
	class Kernel;
	class Pthreads;
	class Timer;
	class Timer_accessor;
	class Timeout;
	class Timeout_handler;

	using Genode::Microseconds;
}


class Libc::Env_implementation : public Libc::Env, public Config_accessor
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

		Vfs::Simple_env _vfs_env;

		Genode::Xml_node _config_xml() const override {
			return _config.xml(); };

	public:

		Env_implementation(Genode::Env &env, Genode::Allocator &alloc)
		: _env(env), _vfs_env(_env, alloc, _vfs_config()) { }


		/*************************
		 ** Libc::Env interface **
		 *************************/

		Vfs::File_system &vfs() override {
			return _vfs_env.root_dir(); }

		Genode::Xml_node libc_config() override {
			return _libc_config(); }


		/*************************************
		 ** Libc::Config_accessor interface **
		 *************************************/

		Xml_node config() const override { return _config.xml(); }


		/***************************
		 ** Genode::Env interface **
		 ***************************/

		Parent        &parent() override { return _env.parent(); }
		Cpu_session   &cpu()    override { return _env.cpu(); }
		Region_map    &rm()     override { return _env.rm(); }
		Pd_session    &pd()     override { return _env.pd(); }
		Entrypoint    &ep()     override { return _env.ep(); }

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

	Genode::Duration curr_time()
	{
		return _timer.curr_time();
	}

	static Microseconds microseconds(Genode::uint64_t timeout_ms)
	{
		return Microseconds(1000*timeout_ms);
	}

	static Genode::uint64_t max_timeout()
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

	bool             _expired             = true;
	Genode::uint64_t _absolute_timeout_ms = 0;

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

	void start(Genode::uint64_t timeout_ms)
	{
		Milliseconds const now = _timer_accessor.timer().curr_time().trunc_to_plain_ms();

		_expired             = false;
		_absolute_timeout_ms = now.value + timeout_ms;

		_timeout.schedule(_timer_accessor.timer().microseconds(timeout_ms));
	}

	Genode::uint64_t duration_left() const
	{
		Milliseconds const now = _timer_accessor.timer().curr_time().trunc_to_plain_ms();

		if (_expired || _absolute_timeout_ms < now.value)
			return 0;

		return _absolute_timeout_ms - now.value;
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

		Pthread(Timer_accessor &timer_accessor, Genode::uint64_t timeout_ms)
		: _timer_accessor(timer_accessor)
		{
			if (timeout_ms > 0) {
				_construct_timeout_once();
				_timeout->start(timeout_ms);
			}
		}

		Genode::uint64_t duration_left()
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

	Genode::uint64_t suspend_myself(Suspend_functor & check, Genode::uint64_t timeout_ms)
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


/* internal utility */
static void resumed_callback();
static void suspended_callback();


struct Libc::Cloned_malloc_heap_range
{
	Genode::Ram_allocator &ram;
	Genode::Region_map    &rm;

	Genode::Ram_dataspace_capability ds;

	size_t const size;
	addr_t const local_addr;

	Cloned_malloc_heap_range(Genode::Ram_allocator &ram, Genode::Region_map &rm,
	                         void *start, size_t size)
	try :
		ram(ram), rm(rm), ds(ram.alloc(size)), size(size),
		local_addr(rm.attach_at(ds, (addr_t)start))
	{ }
	catch (Region_map::Region_conflict) {
		error("could not clone heap region ", Hex_range(local_addr, size));
		throw;
	}

	void import_content(Clone_connection &clone_connection)
	{
		clone_connection.memory_content((void *)local_addr, size);
	}

	virtual ~Cloned_malloc_heap_range()
	{
		rm.detach(local_addr);
		ram.free(ds);
	}
};


/*
 * Utility for tracking the allocation of dataspaces by the malloc heap
 */
struct Libc::Malloc_ram_allocator : Genode::Ram_allocator
{
	Genode::Allocator     &_md_alloc;
	Genode::Ram_allocator &_ram;

	struct Dataspace
	{
		Genode::Ram_dataspace_capability cap;
		Dataspace(Genode::Ram_dataspace_capability cap) : cap(cap) { }
		virtual ~Dataspace() { }
	};

	Genode::Registry<Genode::Registered<Dataspace> > _dataspaces { };

	void _release(Genode::Registered<Dataspace> &ds)
	{
		_ram.free(ds.cap);
		destroy(_md_alloc, &ds);
	}

	Malloc_ram_allocator(Allocator &md_alloc, Ram_allocator &ram)
	: _md_alloc(md_alloc), _ram(ram) { }

	~Malloc_ram_allocator()
	{
		_dataspaces.for_each([&] (Registered<Dataspace> &ds) {
			_release(ds); });
	}

	Ram_dataspace_capability alloc(size_t size, Cache_attribute cached) override
	{
		Ram_dataspace_capability cap = _ram.alloc(size, cached);
		new (_md_alloc) Registered<Dataspace>(_dataspaces, cap);
		return cap;
	}

	void free(Ram_dataspace_capability ds_cap) override
	{
		_dataspaces.for_each([&] (Registered<Dataspace> &ds) {
			if (ds_cap == ds.cap)
				_release(ds); });
	}

	size_t dataspace_size(Ram_dataspace_capability ds_cap) const override
	{
		return _ram.dataspace_size(ds_cap);
	}
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
                            Genode::Entrypoint::Io_progress_handler,
                            Reset_malloc_heap
{
	private:

		Genode::Env &_env;

		/**
		 * Allocator for libc-internal data
		 *
		 * Not mirrored to forked processes. Preserved across 'execve' calls.
		 */
		Genode::Allocator &_heap;

		/**
		 * Allocator for application-owned data
		 *
		 * Mirrored to forked processes. Not preserved across 'execve' calls.
		 */
		Genode::Reconstructible<Malloc_ram_allocator> _malloc_ram { _heap, _env.ram() };

		Genode::Constructible<Heap> _malloc_heap { };

		Genode::Registry<Registered<Cloned_malloc_heap_range> > _cloned_heap_ranges { };

		/**
		 * Reset_malloc_heap interface used by execve
		 */
		void reset_malloc_heap() override;

		Env_implementation   _libc_env { _env, _heap };
		Vfs_plugin           _vfs { _libc_env, _heap, *this };

		bool  const _cloned = _libc_env.libc_config().attribute_value("cloned", false);
		pid_t const _pid    = _libc_env.libc_config().attribute_value("pid", 0U);

		Genode::Reconstructible<Genode::Io_signal_handler<Kernel>> _resume_main_handler {
			_env.ep(), *this, &Kernel::_resume_main };

		jmp_buf _kernel_context;
		jmp_buf _user_context;
		bool    _valid_user_context          = false;
		bool    _dispatch_pending_io_signals = false;

		/* io_progress_handler marker */
		bool _io_ready { false };

		Genode::Thread &_myself { *Genode::Thread::myself() };

		addr_t _kernel_stack = Thread::mystack().top;

		size_t _user_stack_size()
		{
			size_t size = Component::stack_size();
			if (!_cloned)
				return size;

			_libc_env.libc_config().with_sub_node("stack", [&] (Xml_node stack) {
				size = stack.attribute_value("size", 0UL); });

			return size;
		}

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

			void timeout(Genode::uint64_t timeout_ms)
			{
				_construct_timeout_once();
				_timeout->start(timeout_ms);
			}

			Genode::uint64_t duration_left()
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

		Genode::uint64_t _suspend_main(Suspend_functor &check,
		                               Genode::uint64_t timeout_ms)
		{
			/* check that we're not running on libc kernel context */
			if (Thread::mystack().top == _kernel_stack) {
				error("libc suspend() called from non-user context (",
				      __builtin_return_address(0), ") - aborting");
				exit(1);
			}

			if (!check.suspend() && !_kernel_routine)
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

		void _init_file_descriptors();

		void _clone_state_from_parent();

	public:

		Kernel(Genode::Env &env, Genode::Allocator &heap)
		:
			_env(env), _heap(heap)
		{
			_env.ep().register_io_progress_handler(*this);

			if (_cloned) {
				_clone_state_from_parent();

			} else {
				_malloc_heap.construct(*_malloc_ram, _env.rm());
				init_malloc(*_malloc_heap);
			}

			Libc::init_fork(_env, _libc_env, _heap, *_malloc_heap, _pid);
			Libc::init_execve(_env, _heap, _user_stack, *this);

			_init_file_descriptors();
		}

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
		Genode::uint64_t suspend(Suspend_functor &check, Genode::uint64_t timeout_ms)
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

		Genode::Duration current_time()
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
		 * Public alias for _main_context()
		 */
		bool main_context() const { return _main_context(); }

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

		/**
		 * Alloc new watch handler for given path
		 */
		Vfs::Vfs_watch_handle *alloc_watch_handle(char const *path)
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

		void handle_io_progress() override
		{
			/*
			 * TODO: make VFS I/O completion checks during
			 * kernel time to avoid flapping between stacks
			 */

			if (_io_ready) {
				_io_ready = false;

				/* some contexts may have been deblocked from select() */
				if (libc_select_notify)
					libc_select_notify();

				/*
				 * resume all as any VFS context may have
				 * been deblocked from blocking I/O
				 */
				Kernel::resume_all();
			}
		}

		void register_kernel_routine(Kernel_routine &kernel_routine)
		{
			_kernel_routine = &kernel_routine;
		}
};


void Libc::Kernel::reset_malloc_heap()
{
	_malloc_ram.construct(_heap, _env.ram());

	_cloned_heap_ranges.for_each([&] (Registered<Cloned_malloc_heap_range> &r) {
		destroy(_heap, &r); });

	Heap &raw_malloc_heap = *_malloc_heap;
	Genode::construct_at<Heap>(&raw_malloc_heap, *_malloc_ram, _env.rm());

	reinit_malloc(raw_malloc_heap);
}


void Libc::Kernel::_init_file_descriptors()
{
	auto init_fd = [&] (Genode::Xml_node const &node, char const *attr,
	                    int libc_fd, unsigned flags)
	{
		if (!node.has_attribute(attr))
			return;

		typedef Genode::String<Vfs::MAX_PATH_LEN> Path;
		Path const path = node.attribute_value(attr, Path());

		struct stat out_stat { };
		if (stat(path.string(), &out_stat) != 0)
			return;

		Libc::File_descriptor *fd = _vfs.open(path.string(), flags, libc_fd);
		if (fd->libc_fd != libc_fd) {
			Genode::error("could not allocate fd ",libc_fd," for ",path,", "
			              "got fd ",fd->libc_fd);
			_vfs.close(fd);
			return;
		}

		/*
		 * We need to manually register the path. Normally this is done
		 * by '_open'. But we call the local 'open' function directly
		 * because we want to explicitly specify the libc fd ID.
		 */
		if (fd->fd_path)
			Genode::warning("may leak former FD path memory");

		{
			char *dst = (char *)_heap.alloc(path.length());
			Genode::strncpy(dst, path.string(), path.length());
			fd->fd_path = dst;
		}

		::off_t const seek = node.attribute_value("seek", 0ULL);
		if (seek)
			_vfs.lseek(fd, seek, SEEK_SET);
	};

	if (_vfs.root_dir_has_dirents()) {

		Xml_node const node = _libc_env.libc_config();

		typedef Genode::String<Vfs::MAX_PATH_LEN> Path;

		if (node.has_attribute("cwd"))
			chdir(node.attribute_value("cwd", Path()).string());

		init_fd(node, "stdin",  0, O_RDONLY);
		init_fd(node, "stdout", 1, O_WRONLY);
		init_fd(node, "stderr", 2, O_WRONLY);

		node.for_each_sub_node("fd", [&] (Xml_node fd) {

			unsigned const id = fd.attribute_value("id", 0U);

			bool const rd = fd.attribute_value("readable",  false);
			bool const wr = fd.attribute_value("writeable", false);

			unsigned const flags = rd ? (wr ? O_RDWR : O_RDONLY)
			                          : (wr ? O_WRONLY : 0);

			if (!fd.has_attribute("path"))
				warning("Invalid <fd> node, 'path' attribute is missing");

			init_fd(fd, "path", id, flags);
		});

		/* prevent use of IDs of stdin, stdout, and stderr for other files */
		for (unsigned fd = 0; fd <= 2; fd++)
			Libc::file_descriptor_allocator()->preserve(fd);
	}
}


void Libc::Kernel::_clone_state_from_parent()
{
	struct Range { void *at; size_t size; };

	auto range_attr = [&] (Xml_node node)
	{
		return Range {
			.at   = (void *)node.attribute_value("at",   0UL),
			.size =         node.attribute_value("size", 0UL)
		};
	};

	/*
	 * Allocate local memory for the backing store of the application heap,
	 * mirrored from the parent.
	 *
	 * This step must precede the creation of the 'Clone_connection' because
	 * the shared-memory buffer of the clone session may otherwise potentially
	 * interfere with such a heap region.
	 */
	_libc_env.libc_config().for_each_sub_node("heap", [&] (Xml_node node) {
		Range const range = range_attr(node);
		new (_heap)
			Registered<Cloned_malloc_heap_range>(_cloned_heap_ranges,
			                                     _env.ram(), _env.rm(),
			                                     range.at, range.size); });

	Clone_connection clone_connection(_env);

	/* fetch heap content */
	_cloned_heap_ranges.for_each([&] (Cloned_malloc_heap_range &heap_range) {
		heap_range.import_content(clone_connection); });

	/* fetch user contex of the parent's application */
	clone_connection.memory_content(&_user_context, sizeof(_user_context));
	_valid_user_context = true;

	_libc_env.libc_config().for_each_sub_node([&] (Xml_node node) {

		auto copy_from_parent = [&] (Range range)
		{
			clone_connection.memory_content(range.at, range.size);
		};

		/* clone application stack */
		if (node.type() == "stack")
			copy_from_parent(range_attr(node));

		/* clone RW segment of a shared library or the binary */
		if (node.type() == "rw") {
			typedef String<64> Name;
			Name const name = node.attribute_value("name", Name());

			/*
			 * The blacklisted segments are initialized via the
			 * regular startup of the child.
			 */
			bool const blacklisted = (name == "ld.lib.so")
			                      || (name == "libc.lib.so")
			                      || (name == "libm.lib.so")
			                      || (name == "posix.lib.so")
			                      || (strcmp(name.string(), "vfs", 3) == 0);
			if (!blacklisted)
				copy_from_parent(range_attr(node));
		}
	});

	/* import application-heap state from parent */
	clone_connection.object_content(_malloc_heap);
	init_malloc_cloned(clone_connection);
}


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


Genode::uint64_t Libc::suspend(Suspend_functor &s, Genode::uint64_t timeout_ms)
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


Vfs::Vfs_watch_handle *Libc::watch(char const *path)
{
	return kernel->alloc_watch_handle(path);
}


Genode::Duration Libc::current_time()
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
	if (!kernel) {
		error("libc kernel not initialized, needed for with_libc()");
		exit(1);
	}

	/*
	 * The libc execution model builds on the main entrypoint, which handles
	 * all relevant signals (e.g., timing and VFS). Additional component
	 * entrypoints or pthreads should never call with_libc() but we catch this
	 * here and just execute the application code directly.
	 */
	if (!kernel->main_context()) {
		app_code.execute();
		return;
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


void Libc::register_kernel_routine(Kernel_routine &kernel_routine)
{
	kernel->register_kernel_routine(kernel_routine);
}


Genode::Xml_node Libc::libc_config()
{
	return kernel->libc_env().libc_config();
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
	Libc::init_fd_alloc(heap);
	Libc::init_mem_alloc(env);
	Libc::init_dl(env);
	Libc::sysctl_init(env);
	Libc::init_pthread_support(env);

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
