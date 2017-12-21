/*
 * \brief  Nitpicker buffer
 * \author Norman Feske
 * \date   2017-11-16
 */

/*
 * Copyright (C) 2006-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _BUFFER_H_
#define _BUFFER_H_

/* Genode includes */
#include <base/attached_ram_dataspace.h>
#include <framebuffer_session/framebuffer_session.h>

/* local includes */
#include "types.h"


namespace Nitpicker { class Buffer; }


class Nitpicker::Buffer
{
	private:

		Area                      _size;
		Framebuffer::Mode::Format _format;
		Attached_ram_dataspace    _ram_ds;

	public:

		/**
		 * Constructor - allocate and map dataspace for virtual frame buffer
		 *
		 * \throw Out_of_ram
		 * \throw Out_of_caps
		 * \throw Region_map::Region_conflict
		 */
		Buffer(Ram_session &ram, Region_map &rm,
		       Area size, Framebuffer::Mode::Format format, size_t bytes)
		:
			_size(size), _format(format), _ram_ds(ram, rm, bytes)
		{ }

		/**
		 * Accessors
		 */
		Ram_dataspace_capability  ds_cap() const { return _ram_ds.cap(); }
		Area                        size() const { return _size; }
		Framebuffer::Mode::Format format() const { return _format; }
		void                 *local_addr() const { return _ram_ds.local_addr<void>(); }
};


namespace Nitpicker { struct Buffer_provider; }


/**
 * Interface for triggering the re-allocation of a virtual framebuffer
 *
 * Used by 'Framebuffer::Session_component',
 * implemented by 'Nitpicker::Session_component'
 */
struct Nitpicker::Buffer_provider : Interface
{
	virtual Buffer *realloc_buffer(Framebuffer::Mode mode, bool use_alpha) = 0;
};

#endif /* _BUFFER_H_ */
