/*
 * \brief  Regulator-session component
 * \author Stefan Kalkowski
 * \date   2013-06-13
 */

/*
 * Copyright (C) 2013-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__REGULATOR__COMPONENT_H_
#define _INCLUDE__REGULATOR__COMPONENT_H_

#include <root/component.h>
#include <regulator_session/rpc_object.h>
#include <regulator/driver.h>


namespace Regulator {
	class Session_component;
	class Root;
}


class Regulator::Session_component : public Regulator::Session_rpc_object
{
	private:

		Driver_factory & _driver_factory;
		Driver         & _driver;

	public:

		Session_component(Regulator_id     regulator_id,
		                  Driver_factory & driver_factory)
		: Session_rpc_object(regulator_id),
		  _driver_factory(driver_factory),
		  _driver(_driver_factory.create(regulator_id)) { }

		~Session_component()
		{
			_driver.state(_id, false);
			_driver_factory.destroy(_driver);
		}


		/***********************************
		 **  Regulator session interface  **
		 ***********************************/

		void level(unsigned long level) { _driver.level(_id, level);  }
		unsigned long level()           { return _driver.level(_id);  }
		void state(bool enable)         { _driver.state(_id, enable); }
		bool state()                    { return _driver.state(_id);  }
};


class Regulator::Root :
	public Genode::Root_component<Regulator::Session_component>
{
	private:

		Regulator::Driver_factory & _driver_factory;

	protected:

		Session_component *_create_session(const char *args)
		{
			using namespace Genode;

			char reg_name[64];
			Arg_string::find_arg(args, "regulator").string(reg_name,
			                                               sizeof(reg_name), 0);
			size_t ram_quota =
				Arg_string::find_arg(args, "ram_quota").ulong_value(0);

			/* delete ram quota by the memory needed for the session */
			size_t session_size = max((size_t)4096,
			                          sizeof(Session_component));
			if (ram_quota < session_size)
				throw Insufficient_ram_quota();

			if (!strlen(reg_name))
				throw Service_denied();

			return new (md_alloc())
				Session_component(regulator_id_by_name(reg_name),
			                      _driver_factory);
		}

	public:

		Root(Genode::Env &               env,
		     Genode::Allocator &         md_alloc,
		     Regulator::Driver_factory & driver_factory)
		: Genode::Root_component<Regulator::Session_component>(env.ep(),
		                                                       md_alloc),
		  _driver_factory(driver_factory) { }
};

#endif /* _INCLUDE__REGULATOR__COMPONENT_H_ */
