/*
 * \brief  Platform-specific capability type
 * \author Norman Feske
 * \date   2009-10-02
 */

/*
 * Copyright (C) 2009-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _INCLUDE__BASE__NATIVE_CAPABILITY_H_
#define _INCLUDE__BASE__NATIVE_CAPABILITY_H_

/* Fiasco includes */
namespace Fiasco {
#include <l4/sys/types.h>
}

/* Genode includes */
#include <base/cap_map.h>

namespace Genode {

	/**
	 * Native_capability in Fiasco.OC is just a reference to a Cap_index.
	 *
	 * As Cap_index objects cannot be copied around, but Native_capability
	 * have to, we have to use this indirection.
	 */
	class Native_capability
	{
		public:

			typedef Fiasco::l4_cap_idx_t Dst;

			struct Raw
			{
				long local_name;
			};

		private:

			Cap_index* _idx;

		protected:

			inline void _inc()
			{
				if (_idx)
					_idx->inc();
			}

			inline void _dec()
			{
				if (_idx && !_idx->dec()) {
					cap_map()->remove(_idx);
				}
			}

		public:

			/**
			 * Default constructor creates an invalid capability
			 */
			Native_capability() : _idx(0) { }

			/**
			 * Construct capability manually
			 */
			Native_capability(Cap_index* idx)
				: _idx(idx) { _inc(); }

			Native_capability(const Native_capability &o)
			: _idx(o._idx) { _inc(); }

			~Native_capability() { _dec(); }

			/**
			 * Return Cap_index object referenced by this object
			 */
			Cap_index* idx() const { return _idx; }

			/**
			 * Overloaded comparision operator
			 */
			bool operator==(const Native_capability &o) const {
				return _idx == o._idx; }

			Native_capability& operator=(const Native_capability &o){
				if (this == &o)
					return *this;

				_dec();
				_idx = o._idx;
				_inc();
				return *this;
			}

			/*******************************************
			 **  Interface provided by all platforms  **
			 *******************************************/

			long  local_name() const { return _idx ? _idx->id() : 0;        }
			Dst   dst()        const { return _idx ? Dst(_idx->kcap()) : Dst(); }
			bool  valid()      const { return (_idx != 0) && _idx->valid(); }

			Raw raw() const { return { local_name() }; }
	};
}

#endif /* _INCLUDE__BASE__NATIVE_CAPABILITY_H_ */
