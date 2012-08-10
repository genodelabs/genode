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

	struct Native_thread
	{
		enum { INVALID_INDEX = ~0UL };

		addr_t ec_sel;    /* NOVA cap selector for execution context */
		addr_t exc_pt_sel; /* base of event portal window */
		bool   is_vcpu;

		Native_thread() : ec_sel(INVALID_INDEX),
		                  exc_pt_sel (INVALID_INDEX), is_vcpu(false) {}
	};

	typedef Native_thread Native_thread_id;

	inline bool operator == (Native_thread_id t1, Native_thread_id t2)
	{
		return (t1.ec_sel == t2.ec_sel) &&
		       (t1.exc_pt_sel == t2.exc_pt_sel); 
	}
	inline bool operator != (Native_thread_id t1, Native_thread_id t2)
	{
		return (t1.ec_sel != t2.ec_sel) &&
		       (t1.exc_pt_sel != t2.exc_pt_sel);
	}

	class Native_utcb
	{
		private:

			/**
			 * Size of the NOVA-specific user-level thread-control
			 * block
			 */
			enum { UTCB_SIZE = 4096 };

			/**
			 * User-level thread control block
			 *
			 * The UTCB is one 4K page, shared between the kernel
			 * and the user process. It is not backed by a
			 * dataspace but provided by the kernel.
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
			addr_t _rcv_window;

			enum { INVALID_INDEX = ~0UL };

		protected:

			explicit
			Native_capability(void* ptr)
			: _cap(), _trans_map(true), _ptr(ptr),
			  _rcv_window(INVALID_INDEX) {}

		public:

			/**
			 * Constructors
			 */

			Native_capability()
			: _cap(), _trans_map(true), _ptr(0), _rcv_window(INVALID_INDEX) {}

			explicit
			Native_capability(addr_t sel, unsigned rights = 0x1f)
			{
				if (sel == INVALID_INDEX)
					_cap = _Raw();
				else
					_cap = _Raw(sel, rights);

				_trans_map = true;
				_ptr = 0;
			  	_rcv_window = INVALID_INDEX;
			}

			Native_capability(const Native_capability &o)
			: _cap(o._cap), _trans_map(o._trans_map), _ptr(o._ptr),
			  _rcv_window(o._rcv_window) {}

			/**
			 * Copy constructor
			 */
			Native_capability& operator=
				(const Native_capability &o)
			{
				if (this == &o)
					return *this;

				_cap        = o._cap;
				_trans_map  = o._trans_map;
				_ptr        = o._ptr;
				_rcv_window = o._rcv_window;
				return *this;
			}

			/**
			 * Check whether the selector of the Native_cap and 
			 * the capability type is valid.
			 */
			bool valid() const { return !_cap.dst.is_null(); }

			Dst dst()   const { return _cap.dst; }

			/**
			 * Return pointer to the server object identified by
			 * this cap
			 */
			void * local() const { return _ptr; }

			/**
			 * Return the local_name. On NOVA it is the same as the
			 * destination value.
			 */
			addr_t local_name() const
			{
				if (valid())
					return _cap.dst.base();
				else
					return INVALID_INDEX;
			}

			/**
			 * Set one specific cap selector index as receive
			 * window for the next IPC. This can be used to make
			 * sure that the to be received mapped capability will
			 * be placed at a specific index.
			 */
			void   rcv_window(addr_t rcv) { _rcv_window = rcv; }

			/**
			 * Return the selector of the rcv_window.
			 */
			addr_t rcv_window() const { return _rcv_window; }

			/**
			 * Return an invalid Dst object
			 */
			static Dst invalid() { return Dst(); }

			/**
			 * Return a invalid Native_capability
			 */
			static Native_capability invalid_cap()
			{
				return Native_capability();
			}

			/**
			 * Invoke map syscall instead of translate_map call
			 */
			void solely_map() { _trans_map = false; }

			/**
			 * Return true if the cap should be tried first to
			 * be translated and if this fails it should be mapped.
			 */
			bool trans_map() const { return _trans_map; } 
	};

	typedef int Native_connection_state;
}

#endif /* _INCLUDE__BASE__NATIVE_TYPES_H_ */
