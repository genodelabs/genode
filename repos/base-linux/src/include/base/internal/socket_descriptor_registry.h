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
 * Copyright (C) 2012-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__BASE__INTERNAL__SOCKET_DESCRIPTOR_REGISTRY_H_
#define _INCLUDE__BASE__INTERNAL__SOCKET_DESCRIPTOR_REGISTRY_H_

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

			void mark_as_free() { fd = -1; }
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

		Entry &_find_entry_by_fd(int fd)
		{
			for (unsigned i = 0; i < MAX_FDS; i++)
				if (_entries[i].fd == fd)
					return _entries[i];

			throw Limit_reached();
		}

		/**
		 * Lookup file descriptor that belongs to specified global ID
		 *
		 * \return file descriptor or -1 if lookup failed
		 */
		int _lookup_fd_by_global_id(int global_id) const
		{
			for (unsigned i = 0; i < MAX_FDS; i++)
				if (_entries[i].global_id == global_id)
					return _entries[i].fd;

			return -1;
		}

	public:

		void disassociate(int sd)
		{
			Genode::Lock::Guard guard(_lock);

			for (unsigned i = 0; i < MAX_FDS; i++)
				if (_entries[i].fd == sd) {
					_entries[i].mark_as_free();
					return;
				}
		}

		/**
		 * Try to associate socket descriptor with corresponding ID
		 *
		 * \return socket descriptor associated with the ID
		 * \throw  Limit_reached
		 *
		 * If the ID was already associated, the return value is the originally
		 * registered socket descriptor. In this case, the caller should drop
		 * the new socket descriptor and use the one returned by this function.
		 */
		int try_associate(int sd, int global_id)
		{
			/* ignore invalid capabilities */
			if (sd == -1)
				return sd;

			/* ignore invalid capabilities */
			if (sd == -1 || global_id == -1)
				return sd;

			Genode::Lock::Guard guard(_lock);

			int const existing_sd = _lookup_fd_by_global_id(global_id);

			if (existing_sd < 0) {
				Entry &entry = _find_free_entry();
				entry = Entry(sd, global_id);
				return sd;
			} else {
				return existing_sd;
			}
		}
};

#endif /* _INCLUDE__BASE__INTERNAL__SOCKET_DESCRIPTOR_REGISTRY_H_ */
