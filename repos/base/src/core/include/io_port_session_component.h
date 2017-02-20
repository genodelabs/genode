/*
 * \brief  Core-specific instance of the IO_PORT session interface
 * \author Christian Helmuth
 * \date   2007-04-17
 *
 * We assume Core is running on IOPL3.
 */

/*
 * Copyright (C) 2007-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _CORE__INCLUDE__IO_PORT_SESSION_COMPONENT_H_
#define _CORE__INCLUDE__IO_PORT_SESSION_COMPONENT_H_

/* Genode includes */
#include <base/allocator.h>
#include <base/rpc_server.h>
#include <io_port_session/io_port_session.h>

/* core includes */
#include <dataspace_component.h>

namespace Genode {

	class Io_port_session_component : public Rpc_object<Io_port_session>
	{
		private:

			Range_allocator *_io_port_alloc;
			unsigned short   _base;
			unsigned short   _size;

			/**
			 * Check if access exceeds range
             */
			bool _in_bounds(unsigned short address, unsigned width) {
				return (address >= _base) && (address + width <= _base + _size); }


		public:

			/**
			 * Constructor
			 *
			 * \param io_port_alloc  IO_PORT region allocator
			 * \param args           session construction arguments, in
			 *                       particular port base and size
			 * \throw                Root::Invalid_args
			 */
			Io_port_session_component(Range_allocator *io_port_alloc,
			                          const char      *args);

			/**
			 * Destructor
			 */
			~Io_port_session_component();


			/*******************************
			 ** Io-port session interface **
			 *******************************/

			unsigned char  inb(unsigned short);
			unsigned short inw(unsigned short);
			unsigned       inl(unsigned short);

			void outb(unsigned short, unsigned char);
			void outw(unsigned short, unsigned short);
			void outl(unsigned short, unsigned);
	};
}

#endif /* _CORE__INCLUDE__IO_PORT_SESSION_COMPONENT_H_ */
