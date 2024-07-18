/*
 * \brief  kqueue/kevent implementation
 * \author Benjamin Lamowski
 * \date   2024-06-12
 */

/*
 * Copyright (C) 2024 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Libc includes */
#include <sys/event.h>
#include <errno.h>
#include <assert.h>

/* internal includes */
#include <internal/fd_alloc.h>
#include <internal/file.h>
#include <internal/kernel.h>
#include <internal/monitor.h>
#include <internal/kqueue.h>
#include <sys/poll.h>

/* Genode includes */
#include <base/mutex.h>
#include <util/avl_tree.h>
#include <util/register.h>

using namespace Libc;

namespace Libc {
	class Kqueue;

	bool read_ready_from_kernel(File_descriptor *);
	void notify_read_ready_from_kernel(File_descriptor *);
	bool write_ready_from_kernel(File_descriptor *);
}

namespace { using Fn = Libc::Monitor::Function_result; }


static Monitor             *_monitor_ptr;
static Libc::Kqueue_plugin *_kqueue_plugin_ptr;


static Libc::Monitor & monitor()
{
	struct Missing_call_of_init_kqueue_support : Genode::Exception { };
	if (!_monitor_ptr)
		throw Missing_call_of_init_kqueue_support();
	return *_monitor_ptr;
}


/*
 * kqueue(2): "A kevent is identified by the (ident, filter) pair;
 * there may only be one  unique  kevent  per kqueue.
 */
bool operator==(struct kevent a, struct kevent b)
{
        return ((a.ident == b.ident) && (a.filter == b.filter));
}


bool operator>(struct kevent a, struct kevent b)
{
        if (a.ident > b.ident)
                return true;
        if ((a.ident == b.ident) && (a.filter > b.filter))
                return true;

        return false;
}


static consteval int pos(int flag)
{
        for (size_t i = 0; i < sizeof(flag) * 8; i++)
                if ((1 << i) & flag)
                        return i;
}


/*
 * Kqueue backend implementation
 */

struct Libc::Kqueue
{
	const unsigned short flags_whitelist =
		EV_ADD     |
		EV_DELETE  |
		EV_CLEAR   |
		EV_ONESHOT |
		EV_ENABLE  |
		EV_DISABLE;

	const int filter_whitelist =
		EVFILT_READ |
		EVFILT_WRITE;

	struct Kqueue_flags : Genode::Register<32> {
		struct Add     : Bitfield<    pos(EV_ADD), 1> { };
		struct Delete  : Bitfield< pos(EV_DELETE), 1> { };
		struct Enable  : Bitfield< pos(EV_ENABLE), 1> { };
		struct Disable : Bitfield<pos(EV_DISABLE), 1> { };
		struct Clear   : Bitfield<  pos(EV_CLEAR), 1> { };
		struct Oneshot : Bitfield<pos(EV_ONESHOT), 1> { };
	};

	struct Kqueue_elements;

	struct Kqueue_element : kevent, public Avl_node<Kqueue_element>
	{
		Kqueue_element *find_by_kevent(struct kevent const & k)
		{
			if (*this == k) return this;
			Kqueue_element *ele = this->child(k > *this);
			return ele ? ele->find_by_kevent(k) : nullptr;
		}

		bool higher(Kqueue_element *e)
		{
			return *e > *this;
		}

		/*
		 * Run fn arcoss the element's subtree until fn returns false.
		 */
		bool with_all_elements(auto const &fn)
		{
			if (!fn(*this))
				return false;

			if (Kqueue_element * l = child(Avl_node<Kqueue_element>::LEFT))
				if (!l->with_all_elements(fn))
					return false;

			if (Kqueue_element * r = child(Avl_node<Kqueue_element>::RIGHT))
				if (!r->with_all_elements(fn))
					return false;

			return true;
		}
	};

	struct Kqueue_elements : Avl_tree<Kqueue_element>
	{
		bool with_any_element(auto const &fn)
		{
			Kqueue_element *curr_ptr = first();
			if (!curr_ptr)
				return false;

			fn(*curr_ptr);
			return true;
		}

		template <typename FN>
		auto with_element(struct kevent const & k, FN const &match_fn, auto const &no_match_fn)
		-> typename Trait::Functor<decltype(&FN::operator())>::Return_type
		{
			Kqueue_element *ele = (this->first()) ?
				              this->first()->find_by_kevent(k) :
					      nullptr;
			if (ele)
				return match_fn(*ele);
			else
				return no_match_fn();
		}

		/*
		 * Run fn arcoss the tree until fn returns false.
		 */
		void with_all_elements(auto const &fn) const
		{
			if (first()) first()->with_all_elements(fn);
		}
	};


	Genode::Allocator &_alloc;
	Mutex              _requests_mutex;
	Kqueue_elements    _requests;

	/*
	 * Collect invalid elements for deletion.
	 * This needs to be done out of band because otherwise the reshuffling of the AVL tree
	 * might lead to missed valid events.
	 */
	struct Kqueue_element_container : Fifo<Kqueue_element_container>::Element
	{
		Kqueue_element const & ele;
		Kqueue_element_container(Kqueue_element const & e) : ele(e)
		{ }
	};

	Fifo<Kqueue_element_container> _delete_queue;

	void _queue_for_deletion(Kqueue_element &ele)
	{
		Kqueue_element_container &c = *new (_alloc) Kqueue_element_container(ele);
		_delete_queue.enqueue(c);
	}

	/*
	 * This may be called only when we can safely reshuffle the AVL tree
	 */
	void _delete_elements()
	{
		auto cancel_fn = [&](Kqueue_element_container& c) {
			/* At this point we are sure that we can reshuffle the AVL tree. */
			Kqueue_element * non_const_ptr = &(const_cast<Kqueue_element &>(c.ele));
			_requests.remove(non_const_ptr);
			destroy(_alloc, non_const_ptr);
			destroy(_alloc, &c);
		};

		{
			Mutex::Guard guard(_requests_mutex);
			_delete_queue.dequeue_all(cancel_fn);
		}
	}

	int _add_event(struct kevent const& k)
	{
		if (k.filter & ~filter_whitelist) {
			warning("kqueue: filter not implemented: ", k.filter);
			return EINVAL;
		}

		Kqueue_element *ele = new (_alloc) Kqueue_element(k);
		{
			Mutex::Guard guard(_requests_mutex);
			_requests.insert(ele);
		}

		return 0;
	}

	int _delete_event(struct kevent const& k)
	{
		Mutex::Guard guard(_requests_mutex);

		auto match_fn = [&](Kqueue_element & ele) {
			/* Since we know we won't match another element, we can safely remove the element here. */
			_requests.remove(&ele);
			destroy(_alloc, &ele);

			return 0;
		};

		auto no_match_fn = [&]() {
			error("kqueue: did not find kevent to delete: ident: ", k.ident, " filter: ", k.filter);
			return EINVAL;
		};

		return _requests.with_element(k, match_fn, no_match_fn);
	}

	int _enable_event(struct kevent const& k)
	{
		auto match_fn = [&](Kqueue_element & ele) {
			Kqueue_flags::Disable::clear((Kqueue_flags::access_t &)ele.flags);
			Kqueue_flags::Enable::set((Kqueue_flags::access_t &)ele.flags);
			return 0;
		};

		auto no_match_fn = [&]() {
			error("kqueue: did not find kevent to enable: ident: ", k.ident, " filter: ", k.filter);
			return EINVAL;
		};

		return _requests.with_element(k, match_fn, no_match_fn);
	}

	int _disable_event(struct kevent const& k)
	{
		auto match_fn = [&](Kqueue_element & ele) {
			Kqueue_flags::Enable::clear((Kqueue_flags::access_t &)ele.flags);
			Kqueue_flags::Disable::set((Kqueue_flags::access_t &)ele.flags);
			return 0;
		};

		auto no_match_fn = [&]() {
			error("kqueue: did not find kevent to disable: ident: ", k.ident, " filter: ", k.filter);
			return EINVAL;
		};

		return _requests.with_element(k, match_fn, no_match_fn);
	}

	Kqueue(Genode::Allocator & alloc) : _alloc(alloc)
	{ }

	~Kqueue()
	{
		auto destroy_fn = [&] (Kqueue_element &e) {
			_requests.remove(&e);
			destroy(_alloc, &e); };
		while (_requests.with_any_element(destroy_fn));
	}

	int process_events(const struct kevent * changelist, int nchanges,
	                   struct kevent * eventlist, int nevents)
	{
		int num_errors { 0 };

		for (int i = 0; i < nchanges; i++) {
			const int flags = changelist[i].flags;
			int err { 0 };

			if (flags & ~flags_whitelist) {
				error("kqueue: unsupported flags detected: ", flags & ~flags_whitelist);
				return Errno(EINVAL);
			}

			if (Kqueue_flags::Add::get(flags))
				err = _add_event(changelist[i]);
			else if (Kqueue_flags::Delete::get(flags))
				err = _delete_event(changelist[i]);
			else if (Kqueue_flags::Enable::get(flags))
				err = _enable_event(changelist[i]);
			else if (Kqueue_flags::Disable::get(flags))
				err = _disable_event(changelist[i]);
			/* We ignore setting EV_CLEAR for now. */

			if (err) {
				if (num_errors < nevents) {
					eventlist[num_errors] = changelist[i];
					eventlist[num_errors].flags = EV_ERROR;
					eventlist[num_errors].data  = err;
					num_errors++;
				} else {
					return Errno(err);
				}
			}
		}

		return num_errors;
	}

	int collect_completed_events(struct kevent * eventlist, int nevents, const struct timespec *timeout)
	{
		if (nevents == 0)
			return 0;

		/*
		 * event collection mode depending ont 'timeout'
		 *
		 * - timeout pointer == nullptr ... block infinitely for events
		 * - timeout value   == 0       ... poll for events and return
		 *                                  immediately
		 * - timeout value   != 0       ... block for events but don't return
		 *                                  later than timeout
		 */
		enum class Mode { INFINITE, POLL, TIMEOUT };

		Mode     mode       =  Mode::INFINITE;
		uint64_t timeout_ms = 0;

		if (timeout) {
			timeout_ms = timeout->tv_sec * 1000 + timeout->tv_nsec / 1000000;
			mode       = (timeout_ms == 0) ? Mode::POLL : Mode::TIMEOUT;
		}

		int num_events { 0 };

		auto monitor_fn = [&] ()
		{
			/*
			 * kqueue(2):
			 * "The filter is also run when the user attempts to retrieve the kevent
			 * from the kqueue. If the filter indicates that the condition that triggered
			 * the event no longer holds, the kevent is removed from the kqueue and is
			 * not returned."
			 *
			 * Since we need to check the condition on retrieval anyway, we *only* check
			 * the condition on retrieval and not asynchronously.
			 */
			auto check_fn = [&](Kqueue_element &ele) {
				File_descriptor *fd = libc_fd_to_fd(ele.ident, "kevent_collect");

				/*
				 * kqueue(2): "Calling close() on a file  descriptor will remove any
				 * kevents that reference the descriptor."
				 *
				 * Instead of removing the kqueue entry from close(), we collect
				 * invalid entries for deletion here.
				 */
				if (!fd || !fd->plugin || !fd->context) {
					_queue_for_deletion(ele);
					return true;
				}

				/*
				 *  If an event is disabled, ignore it.
				 */
				if (Kqueue_flags::Disable::get(ele.flags))
					return true;

				/*
				 * Right now we do not support tracking newly available read data via
				 * the clear flag, as that would entail tracking the availability of new
				 * data across file system implementations. For the case that a kqueue
				 * client sets EV_CLEAR and does not read the available data after receiving
				 * a kevent, this will lead to extraneous kevents for the already existing data.
				 */
				switch (ele.filter) {
				case EVFILT_READ:
					if (Libc::read_ready_from_kernel(fd)) {
						eventlist[num_events] = ele;
						eventlist[num_events].flags = 0;
						num_events++;
					} else {
						Libc::notify_read_ready_from_kernel(fd);
					}
					break;
				case EVFILT_WRITE:
					if (Libc::write_ready_from_kernel(fd)) {
						eventlist[num_events] = ele;
						eventlist[num_events].flags = 0;
						num_events++;
					}
					break;
				default:
					assert(false && "Element with unknown filter inserted");
				}

				/* Delete oneshot event */
				if (Kqueue_flags::Oneshot::get(ele.flags))
					_queue_for_deletion(ele);

				return num_events < nevents;
			};

			{
				Mutex::Guard guard(_requests_mutex);
				_requests.with_all_elements(check_fn);

			}

			_delete_elements();

			if (mode != Mode::POLL && num_events == 0)
				return Monitor::Function_result::INCOMPLETE;

			return Monitor::Function_result::COMPLETE;
		};

		Monitor::Result const monitor_result =
			monitor().monitor(monitor_fn, timeout_ms);

		if (monitor_result == Monitor::Result::TIMEOUT)
			return 0;

		return num_events;
	}
};


void Libc::init_kqueue(Genode::Allocator &alloc, Monitor &monitor)
{
	_kqueue_plugin_ptr = new (alloc) Kqueue_plugin(alloc);
	_monitor_ptr       = &monitor;
}


static Kqueue_plugin *kqueue_plugin()
{
	if (!_kqueue_plugin_ptr) {
		error("libc kqueue not initialized - aborting");
		exit(1);
	}

	return _kqueue_plugin_ptr;
}



int Libc::Kqueue_plugin::create_kqueue()
{
	Kqueue *kq = new (_alloc) Kqueue(_alloc);

	Plugin_context *context = reinterpret_cast<Libc::Plugin_context *>(kq);
	File_descriptor *fd =
		file_descriptor_allocator()->alloc(this, context, Libc::ANY_FD);

	return fd->libc_fd;
}


int Libc::Kqueue_plugin::close(File_descriptor *fd)
{
	if (fd->plugin != this)
		return -1;

	if (fd->context)
		_alloc.free(fd->context, sizeof(Kqueue));


	file_descriptor_allocator()->free(fd);

	return 0;
}


extern "C" int
kevent(int libc_fd, const struct kevent *changelist, int nchanges,
       struct kevent *eventlist, int nevents, const struct timespec *timeout)
{
	File_descriptor *fd = libc_fd_to_fd(libc_fd, "kevent");

	if (fd->plugin != kqueue_plugin()) {
		error("File descriptor not reqistered to kqueue plugin");
		return Errno(EBADF);
	}

	Kqueue *kq = reinterpret_cast<Libc::Kqueue *>(fd->context);
	assert(kq && "Kqueue not set in kqueue file descriptor");

	if (nchanges < 0 || nevents < 0)
		return Errno(EINVAL);

	int err { 0 };

	if (changelist && nchanges) {
		int num_errors = kq->process_events(changelist, nchanges, eventlist, nevents);

		/*
		 * kqueue(2):
		 * If an error occurs while processing an element of the
		 * changelist and there is enough roomin the eventlist, then the
		 * event will be placed in the eventlist with EV_ERROR set in
		 * flags and the system error in data. Otherwise, -1 will be
		 * returned, and errno will be set to indicate the error
		 * condition.
		 */
		if (num_errors < 0)
			return -1;

		/* reduce space available in the eventlist */
		if (num_errors) {
			nevents -= num_errors;
			eventlist = &eventlist[num_errors];
		}
	}

	if (eventlist && nevents)
		err = kq->collect_completed_events(eventlist, nevents, timeout);

	return err;
}


extern "C"
int kqueue(void)
{
	return kqueue_plugin()->create_kqueue();
}
