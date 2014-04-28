/*
 * \brief  Basic Genode types
 * \author Martin Stein
 * \author Stefan Kalkowski
 * \date   2012-01-02
 */

/*
 * Copyright (C) 2012-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _BASE__NATIVE_TYPES_H_
#define _BASE__NATIVE_TYPES_H_

/* Genode includes */
#include <kernel/interface.h>
#include <base/native_capability.h>
#include <base/stdint.h>
#include <util/string.h>

/* base-hw includes */
#include <kernel/log.h>

namespace Genode
{
	class Platform_thread;
	class Tlb;

	typedef unsigned Native_thread_id;

	struct Native_thread
	{
		Platform_thread  * platform_thread;
		Native_thread_id   thread_id;
	};

	typedef int Native_connection_state;

	/* FIXME needs to be MMU dependent */
	enum { MIN_MAPPING_SIZE_LOG2 = 12 };

	/**
	 * Return kernel thread-name of the caller
	 */
	Native_thread_id thread_get_my_native_id();

	/**
	 * Return an invalid kernel thread-name
	 */
	inline Native_thread_id thread_invalid_id() { return 0; }

	/**
	 * Data bunch with variable size that is communicated between threads
	 *
	 * \param MAX_SIZE  maximum size the object is allowed to take
	 */
	template <size_t MAX_SIZE>
	struct Message_tpl;

	/**
	 * Information that a thread creator hands out to a new thread
	 */
	class Start_info;

	/**
	 * Info that a core-thread creator hands out to Platform_thread::start
	 */
	class Core_start_info;

	/**
	 * Memory region that is exclusive to every thread and known by the kernel
	 */
	class Native_utcb;

	struct Cap_dst_policy
	{
		typedef Native_thread_id Dst;

		/**
		 * Validate capability destination
		 */
		static bool valid(Dst pt) { return pt != 0; }

		/**
		 * Get invalid capability destination
		 */
		static Dst invalid() { return 0; }

		/**
		 * Copy capability 'src' to a given memory destination 'dst'
		 */
		static void
		copy(void * dst, Native_capability_tpl<Cap_dst_policy> * src);
	};

	typedef Native_capability_tpl<Cap_dst_policy> Native_capability;

	/**
	 * Coherent address region
	 */
	struct Native_region
	{
		addr_t base;
		size_t size;
	};

	struct Native_config
	{
		/**
		 * Thread-context area configuration.
		 */
		static constexpr addr_t context_area_virtual_base() {
			return 0x40000000UL; }
		static constexpr addr_t context_area_virtual_size() {
			return 0x10000000UL; }

		/**
		 * Size of virtual address region holding the context of one thread
		 */
		static constexpr addr_t context_virtual_size() { return 0x00100000UL; }
	};

	struct Native_pd_args { };
}

template <Genode::size_t MAX_SIZE>
class Genode::Message_tpl
{
	private:

		size_t  _data_size;
		uint8_t _data[];

		/**
		 * Return size of payload-preceding meta data
		 */
		size_t _header_size() const { return (addr_t)_data - (addr_t)this; }

		/**
		 * Return maximum payload size
		 */
		size_t _max_data_size() const { return MAX_SIZE - _header_size(); }

		/**
		 * Return size of header and current payload
		 */
		size_t _size() const { return _header_size() + _data_size; }

	public:

		/**
		 * Get properties of receive buffer
		 *
		 * \return buf_base  base of receive buffer
		 * \return buf_size  size of receive buffer
		 */
		void buffer_info(void * & buf_base, size_t & buf_size) const
		{
			buf_base = (void *)this;
			buf_size = MAX_SIZE;
		}

		/**
		 * Get properties of request message and receive buffer
		 *
		 * \return buf_base  base of receive buffer and request message
		 * \return buf_size  size of receive buffer
		 * \return msg_size  size of request message
		 */
		void request_info(void * & buf_base, size_t & buf_size,
		                  size_t & msg_size) const
		{
			buf_base = (void *)this;
			buf_size = MAX_SIZE;
			msg_size = _size();
		}

		/**
		 * Get properties of reply message
		 *
		 * \return msg_base  base of reply message
		 * \return msg_size  size of reply message
		 */
		void reply_info(void * & msg_base, size_t & msg_size) const
		{
			msg_base = (void *)this;
			msg_size = _size();
		}

		/**
		 * Install message that shall be send
		 *
		 * \param data       base of payload
		 * \param data_size  size of payload
		 * \param name       local name that shall be the first payload word
		 */
		void prepare_send(void * const data, size_t data_size,
		                  unsigned const name)
		{
			/* limit data size */
			if (data_size > _max_data_size()) {
				Kernel::log() << "oversized message outgoing\n";
				data_size = _max_data_size();
			}
			/* copy data */
			*(unsigned *)_data = name;
			void * const data_dst = (void *)((addr_t)_data + sizeof(name));
			void * const data_src  = (void *)((addr_t)data + sizeof(name));
			memcpy(data_dst, data_src, data_size - sizeof(name));
			_data_size = data_size;
		}

		/**
		 * Read out message that was received
		 *
		 * \param buf_base  base of read buffer
		 * \param buf_size  size of read buffer
		 */
		void finish_receive(void * const buf_base, size_t const buf_size)
		{
			/* limit data size */
			if (_data_size > buf_size) {
				Kernel::log() << "oversized message incoming\n";
				_data_size = buf_size;
			}
			/* copy data */
			memcpy(buf_base, _data, _data_size);
		}
};

class Genode::Start_info
{
	private:

		Native_thread_id  _thread_id;
		Native_capability _utcb_ds;

	public:

		/**
		 * Set-up valid startup message
		 *
		 * \param thread_id  kernel name of the thread that is started
		 */
		void init(Native_thread_id const thread_id,
		          Native_capability const & utcb_ds)
		{
			_thread_id = thread_id;
			_utcb_ds = utcb_ds;
		}


		/***************
		 ** Accessors **
		 ***************/

		Native_thread_id thread_id() const { return _thread_id; }
		Native_capability utcb_ds() const { return _utcb_ds; }
};

class Genode::Core_start_info
{
	private:

		unsigned _processor_id;

	public:

		/**
		 * Set-up valid core startup-message
		 *
		 * \param processor_id  kernel name of the processor to start on
		 */
		void init(unsigned const processor_id)
		{
			_processor_id = processor_id;
		}


		/***************
		 ** Accessors **
		 ***************/

		unsigned processor_id() const { return _processor_id; }
};

class Genode::Native_utcb
{
	private:

		uint8_t _data[1 << MIN_MAPPING_SIZE_LOG2];

	public:

		typedef Message_tpl<sizeof(_data)/sizeof(_data[0])> Message;


		/***************
		 ** Accessors **
		 ***************/

		Message * message() const { return (Message *)_data; }

		Start_info * start_info() const { return (Start_info *)_data; }

		Core_start_info * core_start_info() const
		{
			return (Core_start_info *)_data;
		}

		size_t size() const { return sizeof(_data)/sizeof(_data[0]); }

		void * base() const { return (void *)_data; }
};

namespace Genode
{
	static constexpr addr_t VIRT_ADDR_SPACE_START = 0x1000;
	static constexpr size_t VIRT_ADDR_SPACE_SIZE  = 0xfffef000;

	static constexpr Native_utcb * UTCB_MAIN_THREAD = (Native_utcb *)
		((VIRT_ADDR_SPACE_START + VIRT_ADDR_SPACE_SIZE - sizeof(Native_utcb))
		 & ~((1 << MIN_MAPPING_SIZE_LOG2) - 1));
}

#endif /* _BASE__NATIVE_TYPES_H_ */

