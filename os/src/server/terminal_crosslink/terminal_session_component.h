/*
 * \brief  Terminal session component
 * \author Christian Prochaska
 * \date   2012-05-16
 */

/*
 * Copyright (C) 2012 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _TERMINAL_SESSION_COMPONENT_H_
#define _TERMINAL_SESSION_COMPONENT_H_

/* Genode includes */
#include <base/rpc_server.h>
#include <os/attached_ram_dataspace.h>
#include <os/ring_buffer.h>
#include <terminal_session/terminal_session.h>

namespace Terminal {

	using namespace Genode;

	enum { STACK_SIZE = sizeof(addr_t)*1024 };
	enum { BUFFER_SIZE = 4096 };

	class Session_component : public Rpc_object<Terminal::Session,
	                                            Session_component>
	{
		private:

			Session_component          &_partner;
			Rpc_entrypoint              _ep;
			Genode::Session_capability  _session_cap;

			Attached_ram_dataspace  _io_buffer;

			typedef Ring_buffer<unsigned char, BUFFER_SIZE+1> Local_buffer;

			Local_buffer              _buffer;
			size_t                    _cross_num_bytes_avail;
			Lock                      _write_avail_lock;
			Signal_context_capability _read_avail_sigh;

		public:

			/**
			 * Constructor
			 */
			Session_component(Session_component &partner, Cap_session &cap_session, const char *ep_name);

			Session_capability cap();

			/**
			 * Return true if capability belongs to session object
			 */
            bool belongs_to(Genode::Session_capability cap);

			/* to be called by the partner component */
			bool cross_avail();
			size_t cross_read(unsigned char *buf, size_t dst_len);
			void cross_write();

			/********************************
			 ** Terminal session interface **
			 ********************************/

			Size size();

			bool avail();

			Genode::size_t _read(Genode::size_t dst_len);

			void _write(Genode::size_t num_bytes);

			Genode::Dataspace_capability _dataspace();

			void connected_sigh(Genode::Signal_context_capability sigh);

			void read_avail_sigh(Genode::Signal_context_capability sigh);

			Genode::size_t read(void *, Genode::size_t);
			Genode::size_t write(void const *, Genode::size_t);
	};

}

#endif /* _TERMINAL_SESSION_COMPONENT_H_ */
