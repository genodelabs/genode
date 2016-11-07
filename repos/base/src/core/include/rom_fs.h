/**
 * \brief  Read-only memory modules
 * \author Christian Helmuth
 * \date   2006-05-15
 */

/*
 * Copyright (C) 2006-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _CORE__INCLUDE__ROM_FS_H_
#define _CORE__INCLUDE__ROM_FS_H_

#include <base/stdint.h>
#include <base/log.h>
#include <util/avl_tree.h>
#include <util/avl_string.h>

namespace Genode {

	/**
	 * Convert module command line to module base name
	 *
	 * The conversion is performed in place. The returned
	 * pointer refers to a substring of the 'name' argument.
	 */
	inline char *commandline_to_basename(char *name)
	{
		for (char *c = name; *c != 0; c++) {
			if (*c == '/') name = c + 1;
			if (*c == ' ') {
				*c = 0;
				break;
			}
		}
		return name;
	}

	class Rom_module : public Avl_string_base
	{
		private:

			/* Location of module in memory and size */
			addr_t _addr;
			size_t _size;

		public:

			/** Standard constructor creates invalid object */
			Rom_module()
			: Avl_string_base(0), _addr(0), _size(0) { }

			Rom_module(addr_t addr, size_t size, const char *name)
			: Avl_string_base(name), _addr(addr), _size(size) { }

			/** Check validity */
			bool valid() { return _size ? true : false; }

			/** Accessor functions */
			addr_t addr() const { return _addr; }
			size_t size() const { return _size; }

			void print(Output &out) const
			{
				Genode::print(out, Hex_range<addr_t>(_addr, _size), " ", name());
			}
	};

	class Rom_fs : public Avl_tree<Avl_string_base>
	{
		public:

			Rom_module * find(const char *name)
			{
				return first() ? (Rom_module *)first()->find_by_name(name) : 0;
			}

			/* DEBUG */
			void print_fs(Rom_module *r = 0)
			{
				if (!r) {
					Rom_module *first_module = (Rom_module *)first();
					if (first_module) {
						log("ROM modules:");
						print_fs(first_module);
					} else {
						log("No modules in Rom_fs ", this);
					}
				} else {

					log(" ROM: ", Hex_range<addr_t>(r->addr(), r->size()), " ",
					    r->name());

					Rom_module *child;
					if ((child = (Rom_module *)r->child(Rom_module::LEFT)))  print_fs(child);
					if ((child = (Rom_module *)r->child(Rom_module::RIGHT))) print_fs(child);
				}
			}
	};
}

#endif /* _CORE__INCLUDE__ROM_FS_H_ */
