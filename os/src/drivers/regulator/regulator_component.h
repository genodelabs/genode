/*
 * \brief  Regulator session component
 * \author Alexander Tarasikov <tarasikov@ksyslabs.org>
 * \date   2013-02-18
 */

/*
 * Copyright (C) 2013 Ksys Labs LLC
 * Copyright (C) 2011-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _REGULATOR_COMPONENT_H_
#define _REGULATOR_COMPONENT_H_

/* Genode includes */
#include <base/rpc_server.h>
#include <util/arg_string.h>
#include <os/session_policy.h>
#include <root/component.h>
#include <regulator_session/regulator_session.h>

/* local includes */
#include "regulator_driver.h"

namespace Regulator {

	using namespace Genode;

	class Session_component : public Rpc_object<Regulator::Session,
	                                            Session_component>
	{
		private:

			Regulator::Driver_factory &_driver_factory;
			Regulator::Driver         &_driver;

		public:

			/**
			 * Constructor
			 */
			Session_component(Regulator::Driver_factory &driver_factory,
			                  NameList *allowed_regulators)
			:
				_driver_factory(driver_factory),
				_driver(*_driver_factory.create(allowed_regulators))
			{ }
	
			bool enable(unsigned regulator_id) {
				return _driver.enable(regulator_id);
			}
	
			bool disable(unsigned regulator_id) {
				return _driver.disable(regulator_id);
			}
			
			bool is_enabled(unsigned regulator_id) {
				return _driver.is_enabled(regulator_id);
			}

			enum regulator_state get_state(unsigned regulator_id) {
				return _driver.get_state(regulator_id);
			}

			bool set_state(unsigned regulator_id, enum regulator_state state) {
				return _driver.set_state(regulator_id, state);
			}

			unsigned min_level(unsigned regulator_id) {
				return _driver.min_level(regulator_id);
			}

			unsigned num_level_steps(unsigned regulator_id) {
				return _driver.num_level_steps(regulator_id);
			}

			int get_level(unsigned regulator_id) {
				return _driver.get_level(regulator_id);
			}

			bool set_level(unsigned regulator_id, 
				unsigned min, unsigned max)
			{
				return _driver.set_level(regulator_id, min, max);
			}
	};


	typedef Root_component<Session_component, Multiple_clients> Root_component;


	class Root : public Root_component
	{
		private:

			Driver_factory &_driver_factory;

		protected:

			Session_component *_create_session(const char *args)
			{
				try {
					Session_policy policy(args);


					Genode::Xml_node regs_node =
						policy.sub_node("allowed_regulators");
					Genode::Xml_node reg_node = regs_node.sub_node();
					
					NameList *allowed_regulators = new (env()->heap()) NameList;

					for (;;) {
						try {
							char tmp[256] = {0};
							reg_node.type_name(tmp, sizeof(tmp));
							char *name = new (env()->heap()) char[strlen(tmp) + 1];
							memcpy(name, tmp, strlen(tmp) + 1);
							NameListElt *elt = new (env()->heap()) NameListElt(name);
							allowed_regulators->insert(elt);
							reg_node = reg_node.next();
						}
						catch(Xml_node::Nonexistent_sub_node) {
							break;
						}
					}

					return new (md_alloc())
						Session_component(_driver_factory, allowed_regulators);

				} catch (Xml_node::Nonexistent_sub_node) {
					PERR("Missing \"allowed_regulators\" subnode in policy definition");
					throw Root::Unavailable();
				} catch (Session_policy::No_policy_defined) {
					PERR("Invalid session request, no matching policy");
					throw Root::Unavailable();
				}
			}

		public:

			/**
			 * Constructor
			 */
			Root(Rpc_entrypoint *ep, Allocator *md_alloc,
				Driver_factory &driver_factory)
			: Root_component(ep, md_alloc), _driver_factory(driver_factory) {}
	};
}

#endif /* _REGULATOR_COMPONENT_H_ */
