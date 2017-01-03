/*
 * \brief  User-level task based libc
 * \author Christian Helmuth
 * \date   2016-01-22
 */

/*
 * Copyright (C) 2016 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* Genode includes */
#include <base/component.h>
#include <base/thread.h>
#include <base/rpc_server.h>
#include <base/rpc_client.h>
#include <base/heap.h>
#include <base/attached_rom_dataspace.h>
#include <vfs/dir_file_system.h>

/* libc includes */
#include <libc/component.h>

/* libc-internal includes */
#include <internal/call_func.h>
#include <base/internal/unmanaged_singleton.h>
#include "vfs_plugin.h"
#include "libc_init.h"


/* escape sequences for highlighting debug message prefixes */
#define LIBC_ESC_START "\033[32m"
#define LIBC_ESC_END   "\033[0m"

#define P(...)                                           \
	do {                                                 \
		int dummy;                                       \
		using namespace Genode;                          \
		Hex ctx((addr_t)&dummy >> 20, Hex::OMIT_PREFIX); \
		log(LIBC_ESC_START "[", ctx, "] ",               \
		    __PRETTY_FUNCTION__, ":", __LINE__,          \
		    LIBC_ESC_END "  ", ##__VA_ARGS__);           \
	} while (0)


namespace Libc {
	class Env_implementation;
	class Task;
}


struct Task_resume
{
	GENODE_RPC(Rpc_resume, void, resume);
	GENODE_RPC_INTERFACE(Rpc_resume);
};


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

		Vfs::Dir_file_system _vfs;

		Genode::Xml_node _config_xml() const override {
			return _config.xml(); };

	public:

		Env_implementation(Genode::Env &env, Genode::Allocator &alloc)
		:
			_env(env),
			_vfs(_env, alloc, _vfs_config(),
			     Vfs::global_file_system_factory())
		{ }


		/*************************
		 ** Libc::Env interface **
		 *************************/

		Vfs::File_system &vfs() override {
			return _vfs; }


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
};


/**
 * Libc task
 *
 * The libc task represents the "kernel" of the libc-based application.
 * Blocking and deblocking happens here on libc functions like read() or
 * select(). This combines blocking of the VFS backend and other signal sources
 * (e.g., timers). The libc task runs on the component thread and allocates a
 * secondary stack for the application task. Context switching uses
 * setjmp/longjmp.
 */
class Libc::Task : public Genode::Rpc_object<Task_resume, Libc::Task>
{
	private:

		Genode::Env       &_env;
		Genode::Heap       _heap { _env.ram(), _env.rm() };
		Env_implementation _libc_env { _env, _heap };
		Vfs_plugin         _vfs { _libc_env, _heap };

		/**
		 * Application context and execution state
		 */
		bool    _app_runnable = true;
		jmp_buf _app_task;

		Genode::Thread &_myself = *Genode::Thread::myself();

		void *_app_stack = {
			_myself.alloc_secondary_stack(_myself.name().string(),
			                              Component::stack_size()) };

		/**
		 * Libc context
		 */
		jmp_buf _libc_task;

		/**
		 * Trampoline to application code
		 */
		static void _app_entry(Task *);

		/* executed in the context of the main thread */
		static void _resumed_callback();

	public:

		Task(Genode::Env &env) : _env(env) { }

		~Task() { Genode::error(__PRETTY_FUNCTION__, " should not be executed!"); }

		void run()
		{
			/* save continuation of libc task (incl. current stack) */
			if (!_setjmp(_libc_task)) {
				/* _setjmp() returned directly -> switch to app stack and launch component */
				call_func(_app_stack, (void *)_app_entry, (void *)this);

				/* never reached */
			}

			/* _setjmp() returned after _longjmp() -> we're done */
		}

		/**
		 * Called in the context of the entrypoint via RPC
		 */
		void resume()
		{
			if (!_setjmp(_libc_task))
				_longjmp(_app_task, 1);
		}

		/**
		 * Called from the app context (by fork)
		 */
		void schedule_suspend(void(*suspended_callback) ())
		{
			if (_setjmp(_app_task))
				return;

			_env.ep().schedule_suspend(suspended_callback, _resumed_callback);

			/* switch to libc task, which will return to entrypoint */
			_longjmp(_libc_task, 1);
		}

		/**
		 * Called from the context of the initial thread
		 */
		void resumed()
		{
			Genode::Capability<Task_resume> cap = _env.ep().manage(*this);
			cap.call<Task_resume::Rpc_resume>();
			_env.ep().dissolve(*this);
		}
};


/******************************
 ** Libc task implementation **
 ******************************/

extern "C" void wait_for_continue(void);

void Libc::Task::_app_entry(Task *task)
{
	Libc::Component::construct(task->_libc_env);

	/* returned from task - switch stack to libc and return to dispatch loop */
	_longjmp(task->_libc_task, 1);
}


/**
 * Libc task singleton
 *
 * The singleton is implemented with the unmanaged-singleton utility to ensure
 * it is never destructed like normal static global objects. Otherwise, the
 * task object may be destructed in a RPC to Rpc_resume, which would result in
 * a deadlock.
 */
static Libc::Task *task;


void Libc::Task::_resumed_callback() { task->resumed(); }


namespace Libc {

	void schedule_suspend(void (*suspended) ())
	{
		if (!task) {
			error("libc task handling not initialized, needed for suspend");
			return;
		}
		task->schedule_suspend(suspended);
	}
}


/***************************
 ** Component entry point **
 ***************************/

Genode::size_t Component::stack_size() { return Libc::Component::stack_size(); }


void Component::construct(Genode::Env &env)
{
	/* pass Genode::Env to libc subsystems that depend on it */
	Libc::init_dl(env);

	task = unmanaged_singleton<Libc::Task>(env);
	task->run();
}


/**
 * Default stack size for libc-using components
 */
Genode::size_t Libc::Component::stack_size() __attribute__((weak));
Genode::size_t Libc::Component::stack_size() {
	return 32UL * 1024 * sizeof(Genode::addr_t); }


