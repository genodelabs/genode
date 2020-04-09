/*
 * \brief  Native thread implementation for using epoll on base-linux
 * \author Stefan Thoeni
 * \author Norman Feske
 * \date   2019-12-13
 */

/*
 * Copyright (C) 2006-2020 Genode Labs GmbH
 * Copyright (C) 2019 gapfruit AG
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#include <base/internal/native_thread.h>
#include <base/internal/capability_space_tpl.h>
#include <linux_syscalls.h>

using namespace Genode;


struct Epoll_error : Exception { };


Native_thread::Epoll::Epoll()
:
	_epoll(lx_epoll_create())
{
	_add(_control.local);
}


Native_thread::Epoll::~Epoll()
{
	_remove(_control.local);

	lx_close(_control.local.value);
	lx_close(_control.remote.value);

	lx_close(_epoll.value);
}


void Native_thread::Epoll::_add(Lx_sd sd)
{
	epoll_event event;
	event.events = EPOLLIN;
	event.data.fd = sd.value;
	int ret = lx_epoll_ctl(_epoll, EPOLL_CTL_ADD, sd, &event);
	if (ret < 0) {
		warning(lx_getpid(), ":", lx_gettid(), " lx_epoll_ctl add failed with ", ret);
		throw Epoll_error();
	}
}


void Native_thread::Epoll::_remove(Lx_sd sd)
{
	epoll_event event;
	event.events = EPOLLIN;
	event.data.fd = sd.value;
	int ret = lx_epoll_ctl(_epoll, EPOLL_CTL_DEL, sd, &event);
	if (ret == -2) {
		/* ignore file already closed */
	} else if (ret == -9) {
		/* ignore file already closed */
	} else if (ret < 0) {
		warning(lx_getpid(), ":", lx_gettid(), " lx_epoll_ctl remove failed with ", ret);
		throw Epoll_error();
	}
}


Lx_sd Native_thread::Epoll::poll()
{
	for (;;) {
		epoll_event events[1] { };

		int const event_count = lx_epoll_wait(_epoll, events, 1, -1);

		if ((event_count == 1) && (events[0].events == POLLIN)) {

			Lx_sd const sd { events[0].data.fd };

			if (!sd.valid())
				continue;

			/* dispatch control messages issued via '_exec_control' */
			if (sd.value == _control.local.value) {

				Control_function *control_function_ptr = nullptr;

				struct iovec iovec { };
				iovec.iov_base = &control_function_ptr;
				iovec.iov_len  = sizeof(control_function_ptr);

				struct msghdr msg { };
				msg.msg_iov    = &iovec;
				msg.msg_iovlen = 1;
				int const ret = lx_recvmsg(sd, &msg, 0);

				if (ret != sizeof(control_function_ptr) || !control_function_ptr) {
					error("epoll interrupted by invalid control message");
					continue;
				}

				control_function_ptr->execute();

				struct msghdr ack { };
				(void)lx_sendmsg(sd, &ack, 0);
				continue;
			}

			return sd;
		}

		if (event_count > 1)
			warning(lx_getpid(), ":", lx_gettid(), " too many events on epoll_wait");
	}
}


template <typename FN>
void Native_thread::Epoll::_exec_control(FN const &fn)
{
	Thread * const myself_ptr = Thread::myself();

	/*
	 * If 'myself_ptr' is nullptr, the caller is the initial thread w/o
	 * a valid 'Thread' object associated yet. This thread is never polling.
	 */
	bool const myself_is_polling = (myself_ptr != nullptr)
	                            && (&myself_ptr->native_thread().epoll == this);

	/*
	 * If caller runs in the context of the same thread that executes 'poll' we
	 * can perform the control function immediately because 'poll' cannot
	 * block at this time. If the RPC entrypoint has existed its dispatch
	 * loop, it also cannot poll anymore.
	 */
	if (myself_is_polling || _rpc_ep_exited) {
		fn();
		return;
	}

	/*
	 * If caller is a different thread than the polling thread, interrupt the
	 * polling with a control message, prompting the polling thread to
	 * execute the control function, and resume polling afterwards.
	 */
	struct Control_function_fn : Control_function
	{
		FN const &fn;
		Control_function_fn(FN const &fn) : fn(fn) { }
		void execute() override { fn(); }

	} control_function_fn { fn };

	/* send control message with pointer to control function as argument */
	{
		Control_function *control_function_ptr = &control_function_fn;

		struct iovec iovec { };
		iovec.iov_base = &control_function_ptr;
		iovec.iov_len  = sizeof(control_function_ptr);

		struct msghdr msg { };
		msg.msg_iov    = &iovec;
		msg.msg_iovlen = 1;

		int const ret = lx_sendmsg(_control.remote, &msg, 0);
		if (ret < 0) {
			raw(lx_getpid(), ":", lx_gettid(), " _exec_control ",
			    _control.remote.value, " lx_sendmsg failed ", ret);
			sleep_forever();
		}
	}

	/* block for the completion of the control function */
	{
		struct msghdr ack { };
		int const ret = lx_recvmsg(_control.remote, &ack, 0);
		if (ret < 0)
			warning("invalid acknowledgement for control message");
	}
}


Native_capability Native_thread::Epoll::alloc_rpc_cap()
{
	Lx_socketpair socketpair;

	Rpc_destination dst(socketpair.remote);

	dst.foreign = false;

	_exec_control([&] () { _add(socketpair.local); });

	return Capability_space::import(dst, Rpc_obj_key(socketpair.local.value));
}


void Native_thread::Epoll::free_rpc_cap(Native_capability cap)
{
	int const local_socket = Capability_space::ipc_cap_data(cap).rpc_obj_key.value();

	_exec_control([&] () { _remove(Lx_sd{local_socket}); });
}
