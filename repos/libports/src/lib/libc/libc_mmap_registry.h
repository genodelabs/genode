/*
 * \brief  Registry for keeping track of mmapped regions
 * \author Norman Feske
 * \date   2012-08-16
 */

#ifndef _LIBC_MMAP_REGISTRY_H_
#define _LIBC_MMAP_REGISTRY_H_

/* Genode includes */
#include <base/lock.h>
#include <base/env.h>
#include <base/log.h>

/* libc-internal includes */
#include <libc-plugin/plugin.h>

/* libc includes */
#include <errno.h>

namespace Libc {

	class Mmap_registry;

	/**
	 * Return singleton instance of mmap registry
	 */
	Mmap_registry *mmap_registry();
}


class Libc::Mmap_registry
{
	public:

		struct Entry : Genode::List<Entry>::Element
		{
			void   * const start;
			Plugin * const plugin;

			Entry(void *start, Plugin *plugin)
			: start(start), plugin(plugin) { }
		};

	private:

		Genode::List<Mmap_registry::Entry> _list;

		Genode::Lock mutable _lock;

		/*
		 * Common for both const and non-const lookup functions
		 */
		template <typename ENTRY>
		static ENTRY *_lookup_by_addr_unsynchronized(ENTRY *curr, void * const start)
		{
			for (; curr; curr = curr->next())
				if (curr->start == start)
					return curr;

			return 0;
		}

		Entry const *_lookup_by_addr_unsynchronized(void * const start) const
		{
			return _lookup_by_addr_unsynchronized(_list.first(), start);
		}

		Entry *_lookup_by_addr_unsynchronized(void * const start)
		{
			return _lookup_by_addr_unsynchronized(_list.first(), start);
		}

	public:

		void insert(void *start, Genode::size_t len, Plugin *plugin)
		{
			Genode::Lock::Guard guard(_lock);

			if (_lookup_by_addr_unsynchronized(start)) {
				Genode::warning(__func__, ": mmap region at ", start, " "
				                "is already registered");
				return;
			}

			_list.insert(new (Genode::env()->heap()) Entry(start, plugin));
		}

		Plugin *lookup_plugin_by_addr(void *start) const
		{
			Genode::Lock::Guard guard(_lock);

			Entry const * const e = _lookup_by_addr_unsynchronized(start);
			return e ? e->plugin : 0;
		}

		bool registered(void *start) const
		{
			Genode::Lock::Guard guard(_lock);

			return _lookup_by_addr_unsynchronized(start) != 0;
		}

		void remove(void *start)
		{
			Genode::Lock::Guard guard(_lock);

			Entry *e = _lookup_by_addr_unsynchronized(start);

			if (!e) {
				Genode::warning("lookup for address ", start, " "
				                "in in mmap registry failed");
				return;
			}

			_list.remove(e);
			destroy(Genode::env()->heap(), e);
		}
};


#endif /* _LIBC_MMAP_REGISTRY_H_ */
