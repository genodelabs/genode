/*
 * \brief  Linux-specific socket-descriptor registry
 * \author Norman Feske
 * \date   2012-07-26
 *
 * We use the names of Unix-domain sockets as keys to uniquely identify
 * entrypoints. When receiving a socket descriptor as IPC payload, we first
 * lookup the corresponding entrypoint ID. If we already possess a socket
 * descriptor pointing to the same entrypoint, we close the received one and
 * use the already known descriptor instead.
 */

/*
 * Copyright (C) 2012 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _BASE__IPC__SOCKET_DESCRIPTOR_REGISTRY_H_
#define _BASE__IPC__SOCKET_DESCRIPTOR_REGISTRY_H_

#include <base/lock.h>


namespace Genode
{
	template <unsigned MAX_FDS>
	class Socket_descriptor_registry;

	typedef Socket_descriptor_registry<100> Ep_socket_descriptor_registry;

	/**
	 * Return singleton instance of registry for tracking entrypoint sockets
	 */
	Ep_socket_descriptor_registry *ep_sd_registry();
}


template <unsigned MAX_FDS>
class Genode::Socket_descriptor_registry
{
	public:

		class Limit_reached { };
		class Aliased_global_id { };

	private:

		struct Entry
		{
			int fd;
			int global_id;

			/**
			 * Default constructor creates empty entry
			 */
			Entry() : fd(-1), global_id(-1) { }

			Entry(int fd, int global_id) : fd(fd), global_id(global_id) { }

			bool is_free() const { return fd == -1; }
		};

		Entry _entries[MAX_FDS];

		Genode::Lock mutable _lock;

		Entry &_find_free_entry()
		{
			for (unsigned i = 0; i < MAX_FDS; i++)
				if (_entries[i].is_free())
					return _entries[i];

			throw Limit_reached();
		}

		bool _is_registered(int global_id) const
		{
			for (unsigned i = 0; i < MAX_FDS; i++)
				if (_entries[i].global_id == global_id)
					return true;

			return false;
		}

	public:

		/**
		 * Register association of socket descriptor and its corresponding ID
		 *
		 * \throw Limit_reached
		 * \throw Aliased_global_id  if global ID is already registered
		 */
		void associate(int sd, int global_id)
		{
			Genode::Lock::Guard guard(_lock);

			/* ignore invalid capabilities */
			if (sd == -1 || global_id == -1)
				return;

			/*
			 * Check for potential aliasing
			 *
			 * We allow any global ID to be present in the registry only once.
			 */
			if (_is_registered(global_id))
				throw Aliased_global_id();

			Entry &entry = _find_free_entry();
			entry = Entry(sd, global_id);
		}

		/**
		 * Lookup file descriptor that belongs to specified global ID
		 *
		 * \return file descriptor or -1 if lookup failed
		 */
		int lookup_fd_by_global_id(int global_id) const
		{
			Genode::Lock::Guard guard(_lock);

			for (unsigned i = 0; i < MAX_FDS; i++)
				if (_entries[i].global_id == global_id)
					return _entries[i].fd;

			return -1;
		}
};

#endif /* _BASE__IPC__SOCKET_DESCRIPTOR_REGISTRY_H_ */
