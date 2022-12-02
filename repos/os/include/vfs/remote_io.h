/*
 * \brief  Mechanism for waking up remote I/O peers
 * \author Norman Feske
 * \date   2022-12-01
 */

/*
 * Copyright (C) 2022 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__VFS__REMOTE_IO_H_
#define _INCLUDE__VFS__REMOTE_IO_H_

#include <base/registry.h>
#include <vfs/types.h>

namespace Vfs { class Remote_io; }


struct Vfs::Remote_io : Interface
{
	virtual void wakeup_remote_peer() = 0;

	class Deferred_wakeups;
	class Peer;
};


class Vfs::Remote_io::Peer : Genode::Noncopyable
{
	private:

		struct Deferred_wakeup;

		using Wakeup_registry = Genode::Registry<Deferred_wakeup>;

		class Deferred_wakeup : Wakeup_registry::Element, Interface
		{
			private:

				Peer &_peer;

			public:

				Deferred_wakeup(Wakeup_registry &registry, Peer &peer)
				:
					Wakeup_registry::Element(registry, *this), _peer(peer)
				{ }

				void trigger() { _peer._wakeup(); }
		};

		friend class Deferred_wakeups;

		Deferred_wakeups &_deferred_wakeups;

		Remote_io &_remote_io;

		Genode::Constructible<Deferred_wakeup> _deferred_wakeup { };

		void _wakeup()
		{
			_remote_io.wakeup_remote_peer();
			_deferred_wakeup.destruct();
		}

	public:

		Peer(Deferred_wakeups &deferred_wakeups, Remote_io &remote_io)
		:
			_deferred_wakeups(deferred_wakeups), _remote_io(remote_io)
		{ }

		inline void schedule_wakeup();
};


class Vfs::Remote_io::Deferred_wakeups : Genode::Noncopyable
{
	private:

		Peer::Wakeup_registry _registry { };

		friend class Peer;

	public:

		void trigger()
		{
			_registry.for_each([&] (Peer::Deferred_wakeup &deferred_wakeup) {
				deferred_wakeup.trigger(); });
		}
};


void Vfs::Remote_io::Peer::schedule_wakeup()
{
	_deferred_wakeup.construct(_deferred_wakeups._registry, *this);
}

#endif /* _INCLUDE__VFS__REMOTE_IO_H_ */
