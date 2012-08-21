/*
 * \brief  I/O channel
 * \author Norman Feske
 * \date   2011-02-17
 *
 * An 'Io_channel' is the interface for the operations on an open file
 * descriptor.
 */

/*
 * Copyright (C) 2011-2012 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _NOUX__IO_CHANNEL_H_
#define _NOUX__IO_CHANNEL_H_

/* Genode includes */
#include <base/lock.h>

/* Noux includes */
#include <noux_session/sysio.h>
#include <pwd.h>
#include <shared_pointer.h>
#include <wake_up_notifier.h>

namespace Noux {

	/**
	 * Input/output channel interface
	 */
	class Io_channel : public Reference_counter
	{
		private:

			/**
			 * List of notifiers (i.e., processes) used by threads that block
			 * for an I/O-channel event
			 */
			List<Wake_up_notifier> _notifiers;
			Lock                   _notifiers_lock;

		public:

			bool close_on_execve;

			Io_channel() : close_on_execve(false) { }

			virtual ~Io_channel() { }

			virtual bool     write(Sysio *sysio, size_t &count) { return false; }
			virtual bool      read(Sysio *sysio)                { return false; }
			virtual bool     fstat(Sysio *sysio)                { return false; }
			virtual bool ftruncate(Sysio *sysio)                { return false; }
			virtual bool     fcntl(Sysio *sysio)                { return false; }
			virtual bool    fchdir(Sysio *sysio, Pwd *pwd)      { return false; }
			virtual bool    dirent(Sysio *sysio)                { return false; }
			virtual bool     ioctl(Sysio *sysio)                { return false; }
			virtual bool     lseek(Sysio *sysio)                { return false; }

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
			virtual bool is_nonblocking() { return false; }

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
	};
}

#endif /* _NOUX__IO_CHANNEL_H_ */
