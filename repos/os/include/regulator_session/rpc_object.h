/*
 * \brief  Server-side block regulator interface
 * \author Stefan Kalkowski
 * \date   2013-06-13
 */

/*
 * Copyright (C) 2013-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__REGULATOR_SESSION__SERVER_H_
#define _INCLUDE__REGULATOR_SESSION__SERVER_H_

#include <base/rpc_server.h>
#include <regulator/consts.h>
#include <regulator_session/regulator_session.h>

namespace Regulator { class Session_rpc_object; }


class Regulator::Session_rpc_object : public Genode::Rpc_object<Session, Session_rpc_object>
{
	protected:

		Regulator_id _id; /* regulator identifier */

	public:

		/**
		 * Constructor
		 *
		 * \param id     identifies the specific regulator
		 */
		Session_rpc_object(Regulator_id id) : _id(id) { }
};

#endif /* _INCLUDE__REGULATOR_SESSION__SERVER_H_ */
