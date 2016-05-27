/*
 * \brief  Platform-specific type definitions
 * \author Norman Feske
 * \author Alexander Boettcher
 * \date   2009-10-02
 */

/*
 * Copyright (C) 2009-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _INCLUDE__BASE__NATIVE_TYPES_H_
#define _INCLUDE__BASE__NATIVE_TYPES_H_

/* Genode includes */
#include <base/native_capability.h>
#include <base/stdint.h>
#include <base/cap_map.h>

/* NOVA includes */
#include <nova/syscalls.h>

namespace Genode {

	class Native_capability
	{
		public:

			typedef Nova::Obj_crd Dst;

			struct Raw
			{
				Dst dst;

				/*
				 * It is obsolete and unused in NOVA, however still used by
				 * generic base part
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

			addr_t _rcv_window;

			enum { INVALID_INDEX = ~0UL };

		protected:

			inline void _inc() const
			{
				Cap_index idx(cap_map()->find(local_name()));
				idx.inc();
			}

			inline void _dec() const
			{
				Cap_index idx(cap_map()->find(local_name()));
				idx.dec();
			}

		public:

			/**
			 * Constructors
			 */

			Native_capability()
			: _cap(), _rcv_window(INVALID_INDEX) {}

			explicit
			Native_capability(addr_t sel, unsigned rights = 0x1f)
			{
				if (sel == INVALID_INDEX)
					_cap = _Raw();
				else {
					_cap = _Raw(sel, rights);
					_inc();
				}

				_rcv_window = INVALID_INDEX;
			}

			Native_capability(const Native_capability &o)
			: _cap(o._cap), _rcv_window(o._rcv_window)
			{ if (valid()) _inc(); }

			~Native_capability() { if (valid()) _dec(); }

			/**
			 * Overloaded comparison operator
			 */
			bool operator==(const Native_capability &o) const {
				return local_name() == o.local_name(); }

			/**
			 * Copy constructor
			 */
			Native_capability& operator=
				(const Native_capability &o)
			{
				if (this == &o)
					return *this;

				if (valid()) _dec();

				_cap        = o._cap;
				_rcv_window = o._rcv_window;

				if (valid()) _inc();

				return *this;
			}

			/**
			 * Check whether the selector of the Native_cap and
			 * the capability type is valid.
			 */
			bool valid() const { return !_cap.dst.is_null(); }

			Dst dst()   const { return _cap.dst; }

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
	};
}

#endif /* _INCLUDE__BASE__NATIVE_TYPES_H_ */
