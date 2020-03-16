/*
 * \brief  Libc fork mechanism
 * \author Norman Feske
 * \date   2019-08-13
 */

/*
 * Copyright (C) 2019 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Genode includes */
#include <base/log.h>
#include <base/thread.h>
#include <base/registry.h>
#include <base/child.h>
#include <base/session_object.h>
#include <base/service.h>
#include <base/shared_object.h>
#include <base/attached_ram_dataspace.h>
#include <util/retry.h>

/* libc includes */
#include <sys/types.h>
#include <sys/resource.h>
#include <sys/wait.h>
#include <unistd.h>
#include <stdlib.h>
#include <errno.h>
#include <libc-plugin/fd_alloc.h>

/* libc-internal includes */
#include <internal/init.h>
#include <internal/clone_session.h>
#include <internal/legacy.h>
#include <internal/kernel_routine.h>
#include <internal/suspend.h>
#include <internal/resume.h>
#include <internal/signal.h>

namespace Libc {
	struct Child_config;
	struct Parent_services;
	struct Local_rom_service;
	struct Local_rom_services;
	struct Local_clone_service;
	struct Forked_child_policy;
	struct Forked_child;

	struct Child_ready : Interface
	{
		virtual void child_ready() = 0;
	};

	typedef Registry<Registered<Forked_child> > Forked_children;
}

using namespace Libc;


static pid_t fork_result;

static Env                      *_env_ptr;
static Allocator                *_alloc_ptr;
static Suspend                  *_suspend_ptr;
static Resume                   *_resume_ptr;
static Libc::Signal             *_signal_ptr;
static Kernel_routine_scheduler *_kernel_routine_scheduler_ptr;
static Heap                     *_malloc_heap_ptr;
static void                     *_user_stack_base_ptr;
static size_t                    _user_stack_size;
static int                       _pid;
static int                       _pid_cnt;
static Config_accessor    const *_config_accessor_ptr;
static Binary_name        const *_binary_name_ptr;
static Forked_children          *_forked_children_ptr;


struct Libc::Child_config
{
	Constructible<Attached_ram_dataspace> _ds { };

	Env &_env;

	pid_t const _pid;

	void _generate(Xml_generator &xml, Xml_node config);

	Child_config(Env &env, Config_accessor const &config_accessor, pid_t pid)
	:
		_env(env), _pid(pid)
	{
		Xml_node const config = config_accessor.config();

		size_t buffer_size = 4096;

		retry<Xml_generator::Buffer_exceeded>(

			[&] () {
				_ds.construct(env.ram(), env.rm(), buffer_size);

				Xml_generator
					xml(_ds->local_addr<char>(), buffer_size, "config", [&] () {
						_generate(xml, config); });
			},

			[&] () { buffer_size += 4096; }
		);
	}

	Rom_dataspace_capability ds_cap() const
	{
		Dataspace_capability cap = _ds->cap();
		return static_cap_cast<Rom_dataspace>(cap);
	}
};


void Libc::Child_config::_generate(Xml_generator &xml, Xml_node config)
{
	typedef String<30> Addr;

	/*
	 * Disarm the dynamic linker's sanity check for the
	 * execution of static constructors for the forked child
	 * because those constructors were executed by the parent
	 * already.
	 */
	xml.attribute("ld_check_ctors", "no");

	xml.node("libc", [&] () {

		xml.attribute("pid", _pid);

		typedef String<Vfs::MAX_PATH_LEN> Path;
		config.with_sub_node("libc", [&] (Xml_node node) {
			if (node.has_attribute("rtc"))
				xml.attribute("rtc", node.attribute_value("rtc", Path()));
			if (node.has_attribute("pipe"))
				xml.attribute("pipe", node.attribute_value("pipe", Path()));
			if (node.has_attribute("socket"))
				xml.attribute("socket", node.attribute_value("socket", Path()));
		});

		{
			char buf[Vfs::MAX_PATH_LEN] { };
			if (getcwd(buf, sizeof(buf)))
				xml.attribute("cwd", Path(Cstring(buf)));
		}

		file_descriptor_allocator()->generate_info(xml);

		auto gen_range_attr = [&] (auto at, auto size)
		{
			xml.attribute("at",   Addr(at));
			xml.attribute("size", Addr(size));
		};

		xml.attribute("cloned", "yes");
		xml.node("stack", [&] () {
			gen_range_attr(_user_stack_base_ptr, _user_stack_size); });

		typedef Dynamic_linker::Object_info Info;
		Dynamic_linker::for_each_loaded_object(_env, [&] (Info const &info) {
			xml.node("rw", [&] () {
				xml.attribute("name", info.name);
				gen_range_attr(info.rw_start, info.rw_size); }); });

		_malloc_heap_ptr->for_each_region([&] (void *start, size_t size) {
			xml.node("heap", [&] () {
				gen_range_attr(start, size); }); });
	});

	xml.append("\n");

	/* copy non-libc config as is */
	config.for_each_sub_node([&] (Xml_node node) {
		if (node.type() != "libc") {
			node.with_raw_node([&] (char const *start, size_t len) {
				xml.append("\t");
				xml.append(start, len);
			});
			xml.append("\n");
		}
	});
}


class Libc::Parent_services : Noncopyable
{
	private:

		Env       &_env;
		Allocator &_alloc;

		typedef Registered<Parent_service> Registered_service;

		Registry<Registered_service> _services { };

	public:

		Parent_services(Env &env, Allocator &alloc) : _env(env), _alloc(alloc) { }

		~Parent_services()
		{
			_services.for_each([&] (Registered_service &service) {
				destroy(_alloc, &service); });
		}

		/**
		 * Return parent service matching the specified name
		 */
		Service &matching_service(Service::Name const &name)
		{
			Service *service = nullptr;
			_services.for_each([&] (Parent_service &s) {
				if (!service && name == s.name())
					service = &s; });

			if (service)
				return *service;

			/* expand list of parent services on demand */
			return *new (_alloc) Registered_service(_services, _env, name);
		}
};


struct Libc::Local_rom_service : Noncopyable
{
	typedef Session_label Rom_name;

	struct Session : Session_object<Rom_session>
	{
		Rom_dataspace_capability _ds;

		static Session::Resources _resources()
		{
			return { .ram_quota = { 0 },
			         .cap_quota = { Rom_session::CAP_QUOTA } };
		}

		Session(Entrypoint &ep, Rom_name const &name, Rom_dataspace_capability ds)
		:
			Session_object<Rom_session>(ep.rpc_ep(), _resources(), name, Session::Diag()),
			_ds(ds)
		{ }

		Rom_dataspace_capability dataspace() override { return _ds; }

		void sigh(Signal_context_capability) override { }

	} _session;

	typedef Local_service<Session> Service;

	Service::Single_session_factory _factory { _session };

	Service service { _factory };

	bool matches(Session_label const &label) const
	{
		return label.last_element() == _session.label();
	}

	Local_rom_service(Entrypoint &ep, Rom_name const &name,
	                  Rom_dataspace_capability ds)
	: _session(ep, name, ds) { }

	virtual ~Local_rom_service() { }
};


struct Libc::Local_rom_services : Noncopyable
{
	Allocator &_alloc;

	typedef Registered<Local_rom_service> Registered_service;

	Registry<Registered_service> _services { };

	Local_rom_services(Env &env, Entrypoint &fork_ep, Allocator &alloc)
	:
		_alloc(alloc)
	{
		typedef Dynamic_linker::Object_info Info;
		Dynamic_linker::for_each_loaded_object(env, [&] (Info const &info) {
			new (alloc)
				Registered_service(_services, fork_ep, info.name, info.ds_cap); });
	}

	~Local_rom_services()
	{
		_services.for_each([&] (Registered_service &service) {
			destroy(_alloc, &service); });
	}

	/*
	 * \throw Service_denied
	 */
	Service &matching_service(Service::Name const &name, Session_label const &label)
	{
		if (name != Rom_session::service_name())
			throw Service_denied();

		Service *service_ptr = nullptr;

		_services.for_each([&] (Local_rom_service &service) {
			if (service.matches(label))
				service_ptr = &service.service; });

		if (!service_ptr)
			throw Service_denied();

		return *service_ptr;
	}
};


struct Libc::Local_clone_service : Noncopyable
{
	struct Session : Session_object<Clone_session, Session>
	{
		Attached_ram_dataspace _ds;

		static Session::Resources _resources()
		{
			return { .ram_quota = { Clone_session::RAM_QUOTA },
			         .cap_quota = { Clone_session::CAP_QUOTA } };
		}

		Session(Env &env, Entrypoint &ep)
		:
			Session_object<Clone_session, Session>(ep.rpc_ep(), _resources(),
			                                       "cloned", Session::Diag()),
			_ds(env.ram(), env.rm(), Clone_session::BUFFER_SIZE)
		{ }

		Dataspace_capability dataspace() { return _ds.cap(); }

		void memory_content(Memory_range range)
		{
			::memcpy(_ds.local_addr<void>(), range.start, range.size);
		}

	} _session;

	typedef Local_service<Session> Service;

	Child_ready &_child_ready;

	Resume &_resume;

	Io_signal_handler<Local_clone_service> _child_ready_handler;

	void _handle_child_ready()
	{
		_child_ready.child_ready();

		_resume.resume_all();
	}

	struct Factory : Local_service<Session>::Factory
	{
		Session                  &_session;
		Signal_context_capability _started_sigh;

		Factory(Session &session, Signal_context_capability started_sigh)
		: _session(session), _started_sigh(started_sigh) { }

		Session &create(Args const &, Affinity) override { return _session; }

		void upgrade(Session &, Args const &) override { }

		void destroy(Session &) override { Signal_transmitter(_started_sigh).submit(); }

	} _factory;

	Service service { _factory };

	Local_clone_service(Env &env, Entrypoint &ep, Child_ready &child_ready,
	                    Resume &resume)
	:
		_session(env, ep), _child_ready(child_ready), _resume(resume),
		_child_ready_handler(env.ep(), *this, &Local_clone_service::_handle_child_ready),
		_factory(_session, _child_ready_handler)
	{ }
};


struct Libc::Forked_child : Child_policy, Child_ready
{
	Env &_env;

	Binary_name const _binary_name;

	Resume &_resume;

	Signal &_signal;

	pid_t const _pid;

	enum class State { STARTING_UP, RUNNING, EXITED } _state { State::STARTING_UP };

	int _exit_code = 0;

	Name const _name { _pid };

	/*
	 * Signal handler triggered at the main entrypoint, waking up the libc
	 * suspend mechanism.
	 */
	Io_signal_handler<Libc::Forked_child> _exit_handler {
		_env.ep(), *this, &Forked_child::_handle_exit };

	void _handle_exit()
	{
		_signal.charge(SIGCHLD);
		_resume.resume_all();
	}

	Child_config _child_config;

	Parent_services    &_parent_services;
	Local_rom_services &_local_rom_services;
	Local_clone_service _local_clone_service;
	Local_rom_service   _config_rom_service;

	struct Wait_fork_ready : Kernel_routine
	{
		Forked_child const &child;

		Wait_fork_ready(Forked_child const &child) : child(child) { }

		void execute_in_kernel() override
		{
			/* keep executing this kernel routine until child is running */
			if (!child.running() && !child.exited())
				_kernel_routine_scheduler_ptr->register_kernel_routine(*this);
		}
	} wait_fork_ready { *this };

	pid_t pid() const { return _pid; }

	bool running() const { return _state == State::RUNNING; }

	bool exited() const { return _state == State::EXITED; }

	int exit_code() const { return _exit_code; }


	/***************************
	 ** Child_ready interface **
	 ***************************/

	void child_ready() override
	{
		/*
		 * Don't modify the state if the child already exited.
		 * This can happen for short-lived children where the asynchronous
		 * notification for '_handle_exit' arrives before '_handle_child_ready'
		 * (while the parent is still blocking in the fork call).
		 */
		if (_state == State::STARTING_UP)
			_state = State::RUNNING;
	}


	/****************************
	 ** Child_policy interface **
	 ****************************/

	Name name() const override { return _name; }

	Binary_name binary_name() const override { return _binary_name; }

	Pd_session           &ref_pd()           override { return _env.pd(); }
	Pd_session_capability ref_pd_cap() const override { return _env.pd_session_cap(); }

	void init(Pd_session &session, Pd_session_capability cap) override
	{
		session.ref_account(_env.pd_session_cap());

		_env.pd().transfer_quota(cap, Ram_quota{2500*1000});
		_env.pd().transfer_quota(cap, Cap_quota{100});
	}

	Route resolve_session_request(Service::Name const &name,
	                              Session_label const &label) override
	{
		Session_label rewritten_label = label;

		Service *service_ptr = nullptr;
		if (_state == State::STARTING_UP && name == Clone_session::service_name())
			service_ptr = &_local_clone_service.service;

		if (name == Rom_session::service_name()) {

			/*
			 * Strip off the originating child name to allow the application of
			 * routing rules based on the leading path elements, regardless
			 * of which child in the process hierarchy requests a ROM.
			 */
			rewritten_label = label.last_element();

			try { service_ptr = &_local_rom_services.matching_service(name, label); }
			catch (...) { }

			if (!service_ptr && label.last_element() == "config")
				service_ptr = &_config_rom_service.service;
		}

		if (!service_ptr)
			service_ptr = &_parent_services.matching_service(name);

		if (service_ptr)
			return Route { .service = *service_ptr,
			               .label   = rewritten_label,
			               .diag    = Session::Diag() };

		throw Service_denied();
	}

	void resource_request(Parent::Resource_args const &args) override
	{
		Session::Resources resources = session_resources_from_args(args.string());

		if (resources.ram_quota.value)
			_env.pd().transfer_quota(_child.pd_session_cap(), resources.ram_quota);

		if (resources.cap_quota.value)
			_env.pd().transfer_quota(_child.pd_session_cap(), resources.cap_quota);

		_child.notify_resource_avail();
	}

	void exit(int code) override
	{
		_exit_code = code;
		_state     = State::EXITED;

		Signal_transmitter(_exit_handler).submit();
	}

	Child _child;

	Forked_child(Env                   &env,
	             Entrypoint            &fork_ep,
	             Allocator             &alloc,
	             Binary_name     const &binary_name,
	             Resume                &resume,
	             Signal                &signal,
	             pid_t                  pid,
	             Config_accessor const &config_accessor,
	             Parent_services       &parent_services,
	             Local_rom_services    &local_rom_services)
	:
		_env(env), _binary_name(binary_name),
		_resume(resume), _signal(signal), _pid(pid),
		_child_config(env, config_accessor, pid),
		_parent_services(parent_services),
		_local_rom_services(local_rom_services),
		_local_clone_service(env, fork_ep, *this, resume),
		_config_rom_service(fork_ep, "config", _child_config.ds_cap()),
		_child(env.rm(), fork_ep.rpc_ep(), *this)
	{ }

	virtual ~Forked_child() { }
};


static void fork_kernel_routine()
{
	fork_result = 0;

	if (!_env_ptr || !_alloc_ptr || !_config_accessor_ptr) {
		error("missing call of 'init_fork'");
		abort();
	}

	Env          &env    = *_env_ptr;
	Allocator    &alloc  = *_alloc_ptr;
	Resume       &resume = *_resume_ptr;
	Libc::Signal &signal = *_signal_ptr;

	pid_t const child_pid = ++_pid_cnt;

	enum { STACK_SIZE = 1024*16 };
	static Entrypoint fork_ep(env, STACK_SIZE, "fork_ep", Affinity::Location());

	static Libc::Parent_services parent_services(env, alloc);

	static Local_rom_services local_rom_services(env, fork_ep, alloc);

	Registered<Forked_child> &child = *new (alloc)
		Registered<Forked_child>(*_forked_children_ptr, env, fork_ep, alloc,
		                         *_binary_name_ptr, resume,
		                         signal, child_pid, *_config_accessor_ptr,
		                         parent_services, local_rom_services);

	fork_result = child_pid;

	_kernel_routine_scheduler_ptr->register_kernel_routine(child.wait_fork_ready);
}


/**********
 ** fork **
 **********/

/*
 * We provide weak symbols to allow 'libc_noux' to override them.
 */

extern "C" pid_t __sys_fork(void) __attribute__((weak));
extern "C" pid_t __sys_fork(void)
{
	fork_result = -1;

	/* obtain current stack info, which might have changed since the startup */
	Thread::Stack_info const mystack = Thread::mystack();
	_user_stack_base_ptr = (void *)mystack.base;
	_user_stack_size     = mystack.top - mystack.base;

	struct Fork_kernel_routine : Kernel_routine
	{
		void execute_in_kernel() override { fork_kernel_routine(); }

	} kernel_routine { };

	struct Missing_call_of_init_fork : Exception { };
	if (!_kernel_routine_scheduler_ptr || !_suspend_ptr)
		throw Missing_call_of_init_fork();

	_kernel_routine_scheduler_ptr->register_kernel_routine(kernel_routine);

	struct Suspend_functor_impl : Suspend_functor
	{
		bool suspend() override { return false; }

	} suspend_functor { };

	_suspend_ptr->suspend(suspend_functor, 0);

	return fork_result;
}


pid_t fork(void)  __attribute__((weak, alias("__sys_fork")));
pid_t vfork(void) __attribute__((weak, alias("__sys_fork")));


/************
 ** getpid **
 ************/

extern "C" pid_t __sys_getpid(void) __attribute__((weak));
extern "C" pid_t __sys_getpid(void) { return _pid; }

pid_t getpid(void) __attribute__((weak, alias("__sys_getpid")));


/***********
 ** wait4 **
 ***********/

namespace Libc { struct Wait4_suspend_functor; }

struct Libc::Wait4_suspend_functor : Suspend_functor
{
	Forked_children &_children;

	pid_t const _pid;

	Wait4_suspend_functor(pid_t pid, Forked_children &children)
	: _children(children), _pid(pid) { }

	template <typename FN>
	bool with_exited_child(FN const &fn)
	{
		Registered<Forked_child> *child_ptr = nullptr;

		_children.for_each([&] (Registered<Forked_child> &child) {

			if (child_ptr || !child.exited())
				return;

			if (_pid == child.pid() || _pid == -1)
				child_ptr = &child;
		});

		if (!child_ptr)
			return false;

		fn(*child_ptr);
		return true;
	}

	bool suspend() override
	{
		bool const any_child_exited =
			with_exited_child([] (Forked_child const &) { });

		return !any_child_exited;
	}
};


extern "C" pid_t __sys_wait4(pid_t, int *, int, rusage *) __attribute__((weak));
extern "C" pid_t __sys_wait4(pid_t pid, int *status, int options, rusage *rusage)
{
	pid_t result    = -1;
	int   exit_code = 0;  /* code and status */

	using namespace Libc;

	if (!_forked_children_ptr) {
		errno = ECHILD;
		return -1;
	}

	struct Missing_call_of_init_fork : Exception { };
	if (!_suspend_ptr)
		throw Missing_call_of_init_fork();

	Wait4_suspend_functor suspend_functor { pid, *_forked_children_ptr };

	for (;;) {

		suspend_functor.with_exited_child([&] (Registered<Forked_child> &child) {
			result    = child.pid();
			exit_code = child.exit_code();
			destroy(*_alloc_ptr, &child);
		});

		if (result >= 0 || (options & WNOHANG))
			break;

		_suspend_ptr->suspend(suspend_functor, 0);
	}

	/*
	 * The libc expects status information in bits 0..6 and the exit value
	 * in bits 8..15 (according to 'wait.h').
	 *
	 * The status bits carry the information about the terminating signal.
	 */
	if (status)
		*status = ((exit_code >> 8) & 0x7f) | ((exit_code & 0xff) << 8);

	return result;
}

extern "C" pid_t wait4(pid_t, int *, int, rusage *) __attribute__((weak, alias("__sys_wait4")));


void Libc::init_fork(Env &env, Config_accessor const &config_accessor,
                     Allocator &alloc, Heap &malloc_heap, pid_t pid,
                     Suspend &suspend, Resume &resume, Signal &signal,
                     Kernel_routine_scheduler &kernel_routine_scheduler,
                     Binary_name const &binary_name)
{
	_env_ptr                      = &env;
	_alloc_ptr                    = &alloc;
	_suspend_ptr                  = &suspend;
	_resume_ptr                   = &resume;
	_signal_ptr                   = &signal;
	_kernel_routine_scheduler_ptr = &kernel_routine_scheduler;
	_malloc_heap_ptr              = &malloc_heap;
	_config_accessor_ptr          = &config_accessor;
	_pid                          =  pid;
	_binary_name_ptr              = &binary_name;

	static Forked_children forked_children { };
	_forked_children_ptr = &forked_children;
}
