/*
 * \brief  Board driver for core on pandaboard
 * \author Stefan Kalkowski
 * \author Martin Stein
 * \date   2014-06-02
 */

/*
 * Copyright (C) 2014-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _CORE__SPEC__PANDA__BOARD_H_
#define _CORE__SPEC__PANDA__BOARD_H_

#include <hw/spec/arm/panda_trustzone_firmware.h>
#include <spec/cortex_a9/board_support.h>

namespace Genode { class Board; }


class Genode::Board : public Cortex_a9::Board
{
	public:

		using Base = Cortex_a9::Board;

		class L2_cache : public Base::L2_cache
		{
			private:

				unsigned long _debug_value()
				{
					Debug::access_t v = 0;
					Debug::Dwb::set(v, 1);
					Debug::Dcl::set(v, 1);
					return v;
				}

			public:

				L2_cache(Genode::addr_t mmio) : Base::L2_cache(mmio) { }

				void clean_invalidate()
				{
					using namespace Hw;
					call_panda_firmware(L2_CACHE_SET_DEBUG_REG, _debug_value());
					Base::L2_cache::clean_invalidate();
					call_panda_firmware(L2_CACHE_SET_DEBUG_REG, 0);
				}
		};

		L2_cache & l2_cache() { return _l2_cache; }

	private:

		L2_cache _l2_cache { Base::l2_cache().base() };
};

#endif /* _CORE__SPEC__PANDA__BOARD_H_ */
