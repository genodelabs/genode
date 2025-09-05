/*
 * \brief  Protection-domain facility
 * \author Norman Feske
 * \date   2009-10-02
 */

/*
 * Copyright (C) 2009-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _CORE__INCLUDE__PLATFORM_PD_H_
#define _CORE__INCLUDE__PLATFORM_PD_H_

#include <base/allocator.h>
#include <platform_thread.h>
#include <address_space.h>

namespace Core {

	class Platform_thread;
	class Platform_pd;
}


class Core::Platform_pd : public Address_space
{
	private:

		Native_capability _parent { };
		addr_t const      _pd_sel;

		/*
		 * Noncopyable
		 */
		Platform_pd(Platform_pd const &);
		Platform_pd &operator = (Platform_pd const &);

	public:

		using Name = String<160>;

		Name const name;

		bool has_any_threads = false;

		/**
		 * Constructor
		 */
		Platform_pd(Allocator &md_alloc, Name const &);

		/**
		 * Destructor
		 */
		~Platform_pd();

		/**
		 * Assign parent interface to protection domain
		 */
		void assign_parent(Native_capability parent);

		/**
		 * Return portal capability selector for parent interface
		 */
		addr_t parent_pt_sel() { return _parent.local_name(); }

		/**
		 * Capability selector of this task.
		 *
		 * \return PD selector
		 */
		addr_t pd_sel() const { return _pd_sel; }


		/*****************************
		 ** Address-space interface **
		 *****************************/

		void flush(addr_t, size_t, Core_local_addr) override;
};

#endif /* _CORE__INCLUDE__PLATFORM_PD_H_ */
