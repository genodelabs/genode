/*
 * \brief  Kernel specific data types
 * \author Martin stein
 * \date   2010-10-01
 */

/*
 * Copyright (C) 2010-2012 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _INCLUDE__KERNEL__TYPES_H_
#define _INCLUDE__KERNEL__TYPES_H_

#include <base/fixed_stdint.h>
#include <base/stdint.h>
#include <kernel/config.h>

namespace Kernel {

	using namespace Cpu;

	enum {THREAD_CREATE_PARAMS_ROOTRIGHT_LSHIFT=0};


	struct Utcb_unaligned 
	{
		enum {
			ALIGNMENT_LOG2 = 0,
			SIZE_LOG2      = Cpu::_4KB_SIZE_LOG2,
		};

		union
		{
			volatile Cpu::byte_t byte[1<<SIZE_LOG2];
			volatile Cpu::word_t word[];
		};

		static inline Cpu::size_t size();

		static inline unsigned int size_log2();
	};


	struct Utcb : Utcb_unaligned
	{
		enum {
			ALIGNMENT_LOG2 = Kernel::DEFAULT_PAGE_SIZE_LOG2,
		};
	} __attribute__((aligned(1 << Kernel::DEFAULT_PAGE_SIZE_LOG2)));


	/* syscall type idents */
	/* XXX changes at THREAD_YIELD have to be manually XXX
	 * XXX commited to src/platform/xmb/atomic.s       XXX
	 * XXX in _atomic_syscall_yield                    XXX
	 */
	enum Syscall_id{ 
		TLB_LOAD      = 1,
		TLB_FLUSH     = 2,
		THREAD_CREATE = 3,
		THREAD_KILL   = 4,
		THREAD_SLEEP  = 5,
		THREAD_WAKE   = 6,
		THREAD_YIELD  = 7,
		THREAD_PAGER  = 8,
		IPC_REQUEST   = 9,
		IPC_SERVE     = 10,
		PRINT_CHAR    = 11,
		PRINT_INFO    = 12,
		IRQ_ALLOCATE  = 13,
		IRQ_FREE      = 14,
		IRQ_WAIT      = 15,
		IRQ_RELEASE   = 16,
		INVALID_SYSCALL_ID = 17 };

	enum{THREAD_CREATE__PARAM__IS_ROOT_LSHIFT=0};

	namespace Thread_create_types {

		enum Result { 
			SUCCESS                  =  0,
			INSUFFICIENT_PERMISSIONS = -1,
			INAPPROPRIATE_THREAD_ID  = -2 };
	}


	namespace Thread_kill_types {

		enum Result { 
			SUCCESS                  =  0,
			INSUFFICIENT_PERMISSIONS = -1,
			SUICIDAL                 = -2 };
	}


	namespace Thread_wake_types {

		enum Result{ 
			SUCCESS                  =  0,
			INSUFFICIENT_PERMISSIONS = -1,
			INAPPROPRIATE_THREAD_ID  = -2 };
	}


	namespace Ipc {

		typedef unsigned Payload_size;
	}


	namespace Ipc_serve_types {

		typedef Ipc::Payload_size Result;

		struct Argument{ Ipc::Payload_size reply_size; };
	}


	namespace Paging {

		enum { UNIVERSAL_PROTECTION_ID = 0 };

		class Virtual_page
		{
			addr_t        _address;
			Protection_id _protection_id;
			bool          _valid;

			public:

				void *operator new(size_t, void *addr) { return addr; }

				/**
				 * Invalid construction
				 */
				Virtual_page() : _valid(false) { }

				/**
				 * Construction
				 */
				Virtual_page(addr_t a, Protection_id pid)
				: _address(a), _protection_id(pid), _valid(true) { }

				bool valid(){ return _valid; }

				addr_t address(){ return _address; }

				Protection_id protection_id(){ return _protection_id; }

				void invalidate(){ _valid=false; }
		};


		class Physical_page
		{
			public:

				enum size_t{ 
					_1KB           = 0,
					_4KB           = 1,
					_16KB          = 2,
					_64KB          = 3,
					_256KB         = 4,
					_1MB           = 5,
					_4MB           = 6,
					_16MB          = 7,
					MIN_VALID_SIZE = _4KB,
					MAX_VALID_SIZE = _16MB,
					INVALID_SIZE   = 8};

				enum { MAX_SIZE = INVALID_SIZE, MAX_SIZE_LOG2 = 24 };

				enum Permissions{ R, RW, RX, RWX };

			private:

				addr_t        _address;
				size_t        _size;
				Permissions _permissions;
				bool        _valid;

			public:

				void *operator new(Kernel::size_t, void *addr) { return addr; }

				static inline int size_by_size_log2(size_t& s, int const &size_log2);

				/**
				 * Invalid construction
				 */
				Physical_page() : _valid(false) { }

				/**
				 * Construction
				 */
				Physical_page(addr_t a, size_t ps, Permissions pp) :
					_address(a),
					_size(ps),
					_permissions(pp),
					_valid(true) { }

				bool valid() { return _valid; }

				size_t size() { return _size; }

				addr_t address() { return _address; }

				Permissions permissions() { return _permissions; }

				void invalidate() { _valid = false; }
		};


		static unsigned const
			size_log2_by_physical_page_size [Physical_page::MAX_SIZE + 1] =
			{ 10, 12, 14, 16, 18, 20, 22, 24, 0 };


		struct Resolution
		{
			Virtual_page  virtual_page;
			Physical_page physical_page;
			bool          write_access;

			Resolution() { }

			Resolution(Virtual_page* vp, Physical_page* pp) :
				virtual_page(*vp),
				physical_page(*pp),
				write_access(false)
			{ }

			Resolution(Virtual_page vp, Physical_page pp) :
				virtual_page(vp),
				physical_page(pp),
				write_access(false)
			{ }

			void invalidate()
			{
				virtual_page.invalidate();
				physical_page.invalidate();
			}

			bool valid()
			{
				return virtual_page.valid() & physical_page.valid();
			}
		};


		struct Request
		{
			void *operator new(Kernel::size_t, void *addr) { return addr; }

			enum Access { R, RW, RX, RWX };

			struct Source
			{
				Thread_id tid;
				addr_t      ip;
			};

			Virtual_page virtual_page;
			Source       source;
			Access       access;

			Request(){}

			Request(Virtual_page *vp, Source s, Access as)
			: virtual_page(*vp), source(s), access(as) { }
		};
	}
}


int Kernel::Paging::Physical_page::size_by_size_log2(size_t &s, int const& size_log2)
{
	static size_t const size_by_size_log2[MAX_SIZE_LOG2 + 1] = {

		/* Index 0..9 */
		INVALID_SIZE, INVALID_SIZE,
		INVALID_SIZE, INVALID_SIZE,
		INVALID_SIZE, INVALID_SIZE,
		INVALID_SIZE, INVALID_SIZE,
		INVALID_SIZE, INVALID_SIZE,

		/* Index 10..19 */
		_1KB,   INVALID_SIZE,
		_4KB,   INVALID_SIZE,
		_16KB,  INVALID_SIZE,
		_64KB,  INVALID_SIZE,
		_256KB, INVALID_SIZE,

		/* Index 20..24 */
		_1MB,   INVALID_SIZE,
		_4MB,   INVALID_SIZE,
		_16MB
	};

	if (size_log2 < 0)
		return -1;

	if ((unsigned)size_log2 >= sizeof(size_by_size_log2) / sizeof(size_by_size_log2[0]))
		return -2;

	if (size_by_size_log2[size_log2] == INVALID_SIZE)
		return -3;

	s = size_by_size_log2[(unsigned)size_log2];
	return 0;
}


Cpu::size_t Kernel::Utcb_unaligned::size()
{ 
	return (Cpu::size_t)1<<size_log2();
}


unsigned int Kernel::Utcb_unaligned::size_log2()
{ 
	return (Cpu::size_t)SIZE_LOG2;
}

#endif /* _INCLUDE__KERNEL__TYPES_H_ */
