/*
 * \brief  Linux-specific extension of the CPU session implementation
 * \author Norman Feske
 * \date   2012-08-09
 */

/* core includes */
#include <cpu_session_component.h>

/* Linux includes */
#include <linux_socket.h>

using namespace Genode;


void Cpu_session_component::thread_id(Thread_capability thread_cap, int pid, int tid)
{
	Lock::Guard lock_guard(_thread_list_lock);

	Cpu_thread_component *thread = _lookup_thread(thread_cap);
	if (!thread) return;

	thread->platform_thread()->thread_id(pid, tid);
}


Untyped_capability Cpu_session_component::server_sd(Thread_capability thread_cap)
{
	Lock::Guard lock_guard(_thread_list_lock);

	Cpu_thread_component *thread = _lookup_thread(thread_cap);
	if (!thread) return Untyped_capability();

	enum { DUMMY_LOCAL_NAME = 0 };
	typedef Native_capability::Dst Dst;
	return Untyped_capability(Dst(thread->platform_thread()->server_sd()),
	                          DUMMY_LOCAL_NAME);
}


Untyped_capability Cpu_session_component::client_sd(Thread_capability thread_cap)
{
	Lock::Guard lock_guard(_thread_list_lock);

	Cpu_thread_component *thread = _lookup_thread(thread_cap);
	if (!thread) return Untyped_capability();

	enum { DUMMY_LOCAL_NAME = 0 };
	typedef Native_capability::Dst Dst;
	return Untyped_capability(Dst(thread->platform_thread()->client_sd()),
	                          DUMMY_LOCAL_NAME);
}
