/*
 * \brief  Capability allocation service
 * \author Alexander Boettcher
 * \date   2012-07-27
 */

/*
 * Copyright (C) 2006-2013 Genode Labs GmbH
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

			struct Cap_object : List<Cap_object>::Element
			{
				Genode::addr_t _cap_sel;

				Cap_object(addr_t cap_sel) : _cap_sel(cap_sel) {}
			};

			Tslab<Cap_object, 128> _cap_slab;
			List<Cap_object>       _cap_list;
			Lock                   _cap_lock;

		public:

			/**
			 * Constructor
			 */
			Cap_session_component(Allocator *md_alloc, const char *args)
			: _cap_slab(md_alloc) { }

			/**
			 * Destructor
			 */
			~Cap_session_component()
			{
				Lock::Guard cap_lock(_cap_lock);

				for (Cap_object *obj; (obj = _cap_list.first()); ) {
					Nova::revoke(Nova::Obj_crd(obj->_cap_sel, 0));
					cap_map()->remove(obj->_cap_sel, 0, false);

					_cap_list.remove(obj);
					destroy(&_cap_slab, obj);
				}
			}

			Native_capability alloc(Native_capability ep, addr_t entry,
			                        addr_t mtd)
			{
				addr_t pt_sel = cap_map()->insert();
				addr_t pd_sel = Platform_pd::pd_core_sel();
				addr_t ec_sel = ep.local_name();

				using namespace Nova;

				Lock::Guard cap_lock(_cap_lock);

				/* create cap object */
				Cap_object * pt_cap = new (&_cap_slab) Cap_object(pt_sel);
				if (!pt_cap)
					return Native_capability::invalid_cap();

				_cap_list.insert(pt_cap);

				/* create portal */
				uint8_t res = create_pt(pt_sel, pd_sel, ec_sel, Mtd(mtd),
				                        entry);
				if (res == NOVA_OK)
					return Native_capability(pt_sel);

				PERR("cap_session - cap=%lx:%lx"
				     " addr=%lx flags=%lx xpt=%lx res=%u",
				     ec_sel, ep.local_name(),
				     entry, mtd, pt_sel, res);
				
				_cap_list.remove(pt_cap);
				destroy(&_cap_slab, pt_cap);

				/* cleanup unused selectors */	
				cap_map()->remove(pt_sel, 0, false);

				return Native_capability::invalid_cap();
			}

			void free(Native_capability cap)
			{
				if (!cap.valid()) return;

				Lock::Guard cap_lock(_cap_lock);

				for (Cap_object *obj = _cap_list.first(); obj ; obj = obj->next()) {
					if (cap.local_name() == obj->_cap_sel) {
						Nova::revoke(Nova::Obj_crd(obj->_cap_sel, 0));
						cap_map()->remove(obj->_cap_sel, 0, false);

						_cap_list.remove(obj);
						destroy(&_cap_slab, obj);
						return;
					}
				}
				PDBG("invalid cap object");
			}
	};
}

#endif /* _CORE__INCLUDE__CAP_SESSION_COMPONENT_H_ */
