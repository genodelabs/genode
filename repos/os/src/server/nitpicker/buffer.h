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
#include <types.h>


namespace Nitpicker { class Buffer; }


struct Nitpicker::Buffer : private Attached_ram_dataspace
{
	/**
	 * Constructor - allocate and map dataspace for virtual frame buffer
	 *
	 * \throw Out_of_ram
	 * \throw Out_of_caps
	 * \throw Region_map::Region_conflict
	 */
	Buffer(Ram_allocator &ram, Region_map &rm, size_t num_bytes)
	:
		Attached_ram_dataspace(ram, rm, num_bytes)
	{ }

	using Attached_ram_dataspace::bytes;
	using Attached_ram_dataspace::cap;
};


namespace Nitpicker { struct Buffer_provider; }


/**
 * Interface for triggering the re-allocation of a virtual framebuffer
 *
 * Used by 'Framebuffer::Session_component', implemented by 'Gui_session'
 */
struct Nitpicker::Buffer_provider : Interface
{
	virtual Dataspace_capability realloc_buffer(Framebuffer::Mode) = 0;

	virtual void blit(Rect from, Point to) = 0;

	virtual void panning(Point) = 0;
};

#endif /* _BUFFER_H_ */
