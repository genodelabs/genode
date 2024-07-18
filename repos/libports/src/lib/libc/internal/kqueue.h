/*
 * \brief  kqueue plugin interface
 * \author Benjamin Lamowski
 * \date   2024-08-07
 */

/*
 * Copyright (C) 2024 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _LIBC__INTERNAL__KQUEUE_H_
#define _LIBC__INTERNAL__KQUEUE_H_

/* Libc includes */
#include <sys/event.h>

#include <base/allocator.h>
#include <internal/plugin.h>

namespace Libc { class Kqueue_plugin; }


class Libc::Kqueue_plugin : public Libc::Plugin
{
	private:

		Genode::Allocator & _alloc;

	public:

		Kqueue_plugin(Genode::Allocator & alloc) : _alloc(alloc) { }

		int create_kqueue();
		int close(File_descriptor *) override;
};

#endif /* _LIBC__INTERNAL__KQUEUE_H_ */
