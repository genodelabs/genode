/*
 * \brief  Interface to access binary data at local address space
 * \author Norman Feske
 * \date   2010-10-28
 *
 * This is an alternative implementation of the 'Rom_file' interfaces,
 * which returns linked-in binaries rather than ROM files.
 */

/*
 * Copyright (C) 2010-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _ROM_FILE_LINKED_H_
#define _ROM_FILE_LINKED_H_

#include <base/env.h>
#include <util/string.h>

class Rom_file
{
	private:

		void *_local_addr;

	public:

		Rom_file(const char *name) : _local_addr(0)
		{
			extern char _binary_cube1_png_start[];
			extern char _binary_cube2_png_start[];
			extern char _binary_cube3_png_start[];
			extern char _binary_cube4_png_start[];
			extern char _binary_cube5_png_start[];
			extern char _binary_cube6_png_start[];
			extern char _binary_default_png_start[];
			extern char _binary_hover_png_start[];
			extern char _binary_selected_png_start[];
			extern char _binary_hselected_png_start[];

			struct {
				const char *name;
				void *data;
			} file_list[] = {
				{ "cube1.png",     _binary_cube1_png_start },
				{ "cube2.png",     _binary_cube2_png_start },
				{ "cube3.png",     _binary_cube3_png_start },
				{ "cube4.png",     _binary_cube4_png_start },
				{ "cube5.png",     _binary_cube5_png_start },
				{ "cube6.png",     _binary_cube6_png_start },
				{ "default.png",   _binary_default_png_start },
				{ "hover.png",     _binary_hover_png_start },
				{ "selected.png",  _binary_selected_png_start },
				{ "hselected.png", _binary_hselected_png_start },
				{ 0, 0 }
			};

			for (unsigned i = 0; file_list[i].name; i++)
				if (Genode::strcmp(file_list[i].name, name) == 0)
					_local_addr = file_list[i].data;

			if (!_local_addr) {
				PERR("ROM file lookup failed for \"%s\"", name);
				throw Genode::Rom_connection::Rom_connection_failed();
			}
		}

		void *local_addr() const { return _local_addr; }
};

#endif /* _ROM_FILE_LINKED_H_ */
