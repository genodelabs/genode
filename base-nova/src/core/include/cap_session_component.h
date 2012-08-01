/*
 * \brief  Capability allocation service
 * \author Alexander Boettcher
 * \date   2012-07-27
 */

/*
 * Copyright (C) 2006-2012 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _CORE__INCLUDE__CAP_SESSION_COMPONENT_H_
#define _CORE__INCLUDE__CAP_SESSION_COMPONENT_H_

#include <cap_session/cap_session.h>
#include <base/rpc_server.h>
#include <base/lock.h>

/* core includes */
#include <platform_pd.h>

namespace Genode {

	class Cap_session_component : public Rpc_object<Cap_session>
	{
		private:

			static long _unique_id_cnt;

			static Lock &_lock()
			{
				static Lock static_lock;
				return static_lock;
			}

		public:

			Native_capability alloc(Native_capability ep,
			                        addr_t entry,
			                        addr_t mtd)
			{
				addr_t pt_sel = cap_selector_allocator()->alloc(0);
				addr_t pd_sel = Platform_pd::pd_core_sel();
				addr_t ec_sel = ep.local_name();
				
				using namespace Nova;
				
				/* create portal */
				uint8_t res = create_pt(pt_sel, pd_sel, ec_sel,
				                        Mtd(mtd), entry);
				if (res == NOVA_OK)
					return Native_capability(pt_sel);

				PERR("cap_session - cap=%lx:%lx"
				     " addr=%lx flags=%lx xpt=%lx res=%u",
				     ec_sel, ep.local_name(),
				     entry, mtd, pt_sel, res);
				
				/* cleanup unused selectors */	
				cap_selector_allocator()->free(pt_sel, 0);

				/* XXX revoke ec_sel if it was mapped !!!! */

				return Native_capability::invalid_cap();
			}

			void free(Native_capability cap) { }
	};
}

#endif /* _CORE__INCLUDE__CAP_SESSION_COMPONENT_H_ */
