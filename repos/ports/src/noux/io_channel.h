/*
 * \brief  I/O channel
 * \author Norman Feske
 * \date   2011-02-17
 *
 * An 'Io_channel' is the interface for the operations on an open file
 * descriptor.
 */

/*
 * Copyright (C) 2011-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _NOUX__IO_CHANNEL_H_
#define _NOUX__IO_CHANNEL_H_

/* Genode includes */
#include <base/lock.h>
#include <vfs/file_system.h>

/* Noux includes */
#include <noux_session/sysio.h>
#include <shared_pointer.h>
#include <wake_up_notifier.h>
#include <io_channel_listener.h>

namespace Noux {
	extern Genode::Lock &signal_lock();

	class Io_channel_backend;
	class Io_channel;

	class Terminal_io_channel;
}


/**
 * Input/output channel backend that is used for calling
 * different methods, which does not belong to the original
 * interface, e.g. network methods.
 */
struct Noux::Io_channel_backend
{
	virtual ~Io_channel_backend() { }

	virtual int type() const { return -1; }
};


/**
 * Input/output channel interface
 */
class Noux::Io_channel : public Reference_counter
{
	private:

		/**
		 * List of notifiers (i.e., processes) used by threads that block
		 * for an I/O-channel event
		 */
		List<Wake_up_notifier>    _notifiers;
		Lock                      _notifiers_lock;
		List<Io_channel_listener> _interrupt_handlers;
		Lock                      _interrupt_handlers_lock;

	public:

		bool close_on_execve;

		Io_channel() : close_on_execve(false) { }

		virtual ~Io_channel() { }

		virtual Io_channel_backend *backend() { return nullptr; }

		virtual bool     write(Sysio &sysio, size_t &offset) { return false; }
		virtual bool      read(Sysio &sysio)                 { return false; }
		virtual bool     fstat(Sysio &sysio)                 { return false; }
		virtual bool ftruncate(Sysio &sysio)                 { return false; }
		virtual bool     fcntl(Sysio &sysio)                 { return false; }
		virtual bool    dirent(Sysio &sysio)                 { return false; }
		virtual bool     ioctl(Sysio &sysio)                 { return false; }
		virtual bool     lseek(Sysio &sysio)                 { return false; }

		/**
		 * Return true if an unblocking condition of the channel is satisfied
		 *
		 * \param rd  if true, check for data available for reading
		 * \param wr  if true, check for readiness for writing
		 * \param ex  if true, check for exceptions
		 */
		virtual bool check_unblock(bool rd, bool wr, bool ex) const {
			return false; }

		/**
		 * Return true if the channel is set to non-blocking mode
		 */
		virtual bool nonblocking() { return false; }

		/**
		 * Register blocker for getting waked up on an I/O channel event
		 *
		 * This function is normally called by the to-be-blocked thread
		 * prior blocking itself, e.g., during a 'select' syscall.
		 */
		void register_wake_up_notifier(Wake_up_notifier *notifier)
		{
			Lock::Guard guard(_notifiers_lock);

			_notifiers.insert(notifier);
		}

		/**
		 * Unregister wake-up notifier
		 *
		 * This function is normally called after a blocker has left the
		 * blocking condition, e.g., during the return from the 'select'
		 * syscall'.
		 */
		void unregister_wake_up_notifier(Wake_up_notifier *notifier)
		{
			Lock::Guard guard(_notifiers_lock);

			_notifiers.remove(notifier);
		}

		/**
		 * Tell all registered notifiers about an occurred I/O event
		 *
		 * This function is called by I/O channel implementations that
		 * respond to external signals, e.g., the availability of new
		 * input from a terminal session.
		 */
		void invoke_all_notifiers()
		{
			Lock::Guard guard(_notifiers_lock);

			for (Wake_up_notifier *n = _notifiers.first(); n; n = n->next())
				n->wake_up();
		}

		/**
		 * Register interrupt handler
		 *
		 * This function is called by Child objects to get woken up if the
		 * terminal sends, for example, Ctrl-C.
		 */
		void register_interrupt_handler(Io_channel_listener *handler)
		{
			Lock::Guard guard(_interrupt_handlers_lock);

			_interrupt_handlers.insert(handler);
		}

		/**
		 * Unregister interrupt handler
		 */
		void unregister_interrupt_handler(Io_channel_listener *handler)
		{
			Lock::Guard guard(_interrupt_handlers_lock);

			_interrupt_handlers.remove(handler);
		}

		/**
		  * Find the 'Io_channel_listener' object which contains the given
		  * 'Interrupt_handler' pointer
		  */
		Io_channel_listener *lookup_io_channel_listener(Interrupt_handler *handler)
		{
			for (Io_channel_listener *l = _interrupt_handlers.first();
			     l; l = l->next())
				if (l->object() == handler)
					return l;
			return 0;
		}

		/**
		 * Tell all registered handlers about an interrupt event
		 */
		void invoke_all_interrupt_handlers()
		{
			Lock::Guard signal_lock_guard(signal_lock());
			Lock::Guard guard(_interrupt_handlers_lock);

			for (Io_channel_listener *l = _interrupt_handlers.first();
			     l; l = l->next())
				l->object()->handle_interrupt();
		}

		/**
		 * Get the path of the file associated with the I/O channel
		 *
		 * This function is used to simplify the implemenation of SYSCALL_FSTAT
		 * and is only implemented by Vfs_io_channel.
		 */
		virtual bool path(char *path, size_t len) { return false; }
};

#endif /* _NOUX__IO_CHANNEL_H_ */
