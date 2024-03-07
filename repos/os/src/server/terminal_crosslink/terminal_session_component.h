/*
 * \brief  Terminal session component
 * \author Christian Prochaska
 * \date   2012-05-16
 */

/*
 * Copyright (C) 2012-2024 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _TERMINAL_SESSION_COMPONENT_H_
#define _TERMINAL_SESSION_COMPONENT_H_

/* Genode includes */
#include <base/heap.h>
#include <base/rpc_server.h>
#include <base/attached_ram_dataspace.h>
#include <terminal_session/terminal_session.h>

namespace Terminal_crosslink {

	using namespace Genode;

	static constexpr size_t IO_BUFFER_SIZE = 4096;

	class Ring_buffer
	{
		private:

			size_t _head = 0;
			size_t _tail = 0;

			char *_queue { };

			size_t _queue_size;

			/*
			 * Noncopyable
			 */
			Ring_buffer(Ring_buffer const &);
			Ring_buffer &operator = (Ring_buffer const &);

		public:

			/**
			 * Constructor
			 */
			Ring_buffer(Byte_range_ptr const &buffer)
			: _queue(buffer.start), _queue_size(buffer.num_bytes) { }

			struct     Add_ok    { };
			enum class Add_error { OVERFLOW };

			using Add_result = Attempt<Add_ok, Add_error>;

			/**
			 * Place element into ring buffer
			 */
			Add_result add(char ev)
			{
				if ((_head + 1)%_queue_size != _tail) {
					_queue[_head] = ev;
					_head = (_head + 1)%_queue_size;
					return Add_ok();
				} else
					return Add_error::OVERFLOW;
			}

			/**
			 * Take element from ring buffer
			 *
			 * \return  element
			 */
			char get()
			{
				unsigned char e = _queue[_tail];
				_tail = (_tail + 1)%_queue_size;
				return e;
			}

			/**
			 * Return true if ring buffer is empty
			 */
			bool empty() const { return _tail == _head; }
	};


	class Allocated_ring_buffer : public Ring_buffer
	{
		private:

			Genode::Allocator &_alloc;

			char *_buffer { };

			char *_init_buffer(Genode::Allocator &alloc, size_t size)
			{
				_buffer = static_cast<char*>(alloc.alloc(size));
				return _buffer;
			};

			/*
			 * Noncopyable
			 */
			Allocated_ring_buffer(Allocated_ring_buffer const &);
			Allocated_ring_buffer &operator = (Allocated_ring_buffer const &);

		public:

			Allocated_ring_buffer(Genode::Allocator &alloc, size_t queue_size)
			: Ring_buffer(Byte_range_ptr(_init_buffer(alloc, queue_size),
			                             queue_size)),
			  _alloc(alloc)
			{ }

			~Allocated_ring_buffer()
			{
				destroy(_alloc, _buffer);
			}
	};


	class Session_component : public Rpc_object<Terminal::Session,
	                                            Session_component>
	{
		private:

			Env                        &_env;
			Heap                        _alloc { _env.ram(), _env.rm() };

			Session_component          &_partner;

			size_t                      _buffer_size;

			Genode::Session_capability  _session_cap;

			Attached_ram_dataspace      _io_buffer;

			Allocated_ring_buffer       _buffer { _alloc, _buffer_size + 1 };
			size_t                      _cross_num_bytes_avail;
			Signal_context_capability   _read_avail_sigh { };

		public:

			/**
			 * Constructor
			 */
			Session_component(Env &env, Session_component &partner,
			                  size_t buffer_size);

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

			Size size() override;

			bool avail() override;

			Genode::size_t _read(Genode::size_t dst_len);

			Genode::size_t _write(Genode::size_t num_bytes);

			Genode::Dataspace_capability _dataspace();

			void connected_sigh(Genode::Signal_context_capability sigh) override;

			void read_avail_sigh(Genode::Signal_context_capability sigh) override;

			void size_changed_sigh(Genode::Signal_context_capability) override { }

			Genode::size_t read(void *, Genode::size_t) override;
			Genode::size_t write(void const *, Genode::size_t) override;
	};

}

#endif /* _TERMINAL_SESSION_COMPONENT_H_ */
