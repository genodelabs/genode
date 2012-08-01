/*
 * \brief  Platform-specific type definitions
 * \author Norman Feske
 * \author Alexander Boettcher
 * \date   2009-10-02
 */

/*
 * Copyright (C) 2009-2012 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _INCLUDE__BASE__NATIVE_TYPES_H_
#define _INCLUDE__BASE__NATIVE_TYPES_H_

#include <base/native_capability.h>
#include <base/stdint.h>

#include <nova/syscalls.h>

namespace Genode {

	typedef volatile int  Native_lock;

	struct Native_thread
	{
		addr_t ec_sel;      /* NOVA cap selector for execution context */
		addr_t pd_sel;      /* NOVA cap selector of protection domain */
		addr_t exc_pt_sel;  /* base of event portal window */
	};

	typedef Native_thread Native_thread_id;

	inline bool operator == (Native_thread_id t1, Native_thread_id t2)
	{
		return (t1.ec_sel == t2.ec_sel) && (t1.pd_sel == t2.pd_sel); 
	}
	inline bool operator != (Native_thread_id t1, Native_thread_id t2)
	{
		return (t1.ec_sel != t2.ec_sel) && (t1.pd_sel != t2.pd_sel);
	}

	class Native_utcb
	{
		private:

			/**
			 * Size of the NOVA-specific user-level thread-control block
			 */
			enum { UTCB_SIZE = 4096 };

			/**
			 * User-level thread control block
			 *
			 * The UTCB is one 4K page, shared between the kernel and the
			 * user process. It is not backed by a dataspace but provided
			 * by the kernel.
			 */
			addr_t _utcb[UTCB_SIZE/sizeof(addr_t)];
	};

	class Native_capability
	{
		public:
			typedef Nova::Obj_crd Dst;

			struct Raw
			{
				Dst    dst;
 				/*
				 * It is obsolete and unused in NOVA,
				 * however still used by generic base part
				 */
				addr_t local_name;
			};

		private:
			struct _Raw
			{
				Dst    dst;

				_Raw() : dst() { }

				_Raw(addr_t sel, unsigned rights)
				: dst(sel, 0, rights) { }
			} _cap;

			bool   _trans_map;
			void * _ptr;

		protected:

			explicit
			Native_capability(void* ptr) : _cap(),
			                               _trans_map(true),
			                               _ptr(ptr) { }

		public:
			Native_capability() : _cap(), _trans_map(true),
			                      _ptr(0) { }

			explicit
			Native_capability(addr_t sel, unsigned rights = 0x1f)
			: _cap(sel, rights), _trans_map(true), _ptr(0) { }

			Native_capability(const Native_capability &o)
			: _cap(o._cap), _trans_map(o._trans_map), _ptr(o._ptr) { }

			Native_capability& operator=(const Native_capability &o){
				if (this == &o)
					return *this;

				_cap        = o._cap;
				_trans_map  = o._trans_map;
				_ptr        = o._ptr;
				return *this;
			}

			bool valid() const
			{
				 return !_cap.dst.is_null() &&
					 _cap.dst.base() != ~0UL;
			 }

			Dst dst()   const { return _cap.dst; }
			void * local() const { return _ptr; }
			addr_t local_name() const { return _cap.dst.base(); }

			static Dst invalid() { return Dst(); }

			static Native_capability invalid_cap()
			{
				return Native_capability(~0UL);
			}

			/** Invoke map syscall instead of translate_map call */
			void solely_map() { _trans_map = false; }
			bool trans_map() const { return _trans_map; } 
	};

	typedef int Native_connection_state;
}

#endif /* _INCLUDE__BASE__NATIVE_TYPES_H_ */
