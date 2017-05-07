/*
 * \brief  Connection to a local child
 * \author Norman Feske
 * \date   2016-11-10
 *
 * The 'Local_connection' can be used to locally establish a connection
 * to a 'Local_service' or a 'Parent_service'.
 */

/*
 * Copyright (C) 2016-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__BASE__LOCAL_CONNECTION_H_
#define _INCLUDE__BASE__LOCAL_CONNECTION_H_

#include <util/arg_string.h>
#include <base/service.h>

namespace Genode {

	class Local_connection_base;
	template <typename> class Local_connection;
}


struct Genode::Local_connection_base : Noncopyable
{
	public:

		typedef Session_state::Args Args;

	protected:

		Constructible<Session_state> _session_state;

	private:

		static Args _init_args(Args const &args, Session::Resources resources,
		                       Session::Diag diag)
		{
			/* copy original arguments into modifiable buffer */
			char buf[Args::capacity()];
			strncpy(buf, args.string(), sizeof(buf));

			Arg_string::set_arg(buf, sizeof(buf), "ram_quota",
			                    String<64>(resources.ram_quota.value).string());
			Arg_string::set_arg(buf, sizeof(buf), "cap_quota",
			                    String<64>(resources.cap_quota.value).string());
			Arg_string::set_arg(buf, sizeof(buf), "diag", diag.enabled);

			/* return result as a copy  */
			return Args(Cstring(buf));
		}

	protected:

		Local_connection_base(Service                          &service,
		                      Id_space<Parent::Client>         &id_space,
		                      Parent::Client::Id                id,
		                      Args const &args, Affinity const &affinity,
		                      Session::Label             const &label,
		                      Session::Diag                     diag,
		                      Session::Resources                resources)
		{
			enum { NUM_ATTEMPTS = 10 };
			for (unsigned i = 0; i < NUM_ATTEMPTS; i++) {
				_session_state.construct(service, id_space, id, label,
				                         _init_args(args, resources, diag),
				                         affinity);

				_session_state->service().initiate_request(*_session_state);

				if (_session_state->alive())
					break;

				switch (_session_state->phase) {

				case Session_state::INSUFFICIENT_RAM_QUOTA:
					resources.ram_quota.value += 4096;
					break;

				case Session_state::INSUFFICIENT_CAP_QUOTA:
					resources.cap_quota.value += 1;
					break;

				default: break;
				}
			}

			if (_session_state->phase == Session_state::INSUFFICIENT_RAM_QUOTA
			 || _session_state->phase == Session_state::INSUFFICIENT_CAP_QUOTA)
				warning("giving up to increase session quota for ", service.name(), " session "
				        "after ", (int)NUM_ATTEMPTS, " attempts");
		}

		~Local_connection_base()
		{
			if (_session_state->alive()) {
				_session_state->phase = Session_state::CLOSE_REQUESTED;
				_session_state->service().initiate_request(*_session_state);
			}
		}
};


template <typename CONNECTION>
class Genode::Local_connection : Local_connection_base
{
	private:

		typedef typename CONNECTION::Session_type SESSION;

		Constructible <typename SESSION::Client> _client;

	public:

		Capability<SESSION> cap() const
		{
			return reinterpret_cap_cast<SESSION>(_session_state->cap);
		}

		SESSION &session()
		{
			/*
			 * If session comes from a local service (e.g,. a virtualized
			 * RAM session, we return the reference to the corresponding
			 * component object, which can be called directly.
			 */
			if (_session_state->local_ptr)
				return *static_cast<SESSION *>(_session_state->local_ptr);

			/*
			 * The session is provided remotely. So return a client stub for
			 * interacting with the session. We construct the client object if
			 * we have a valid session capability.
			 */
			if (!_client.constructed() && _session_state->cap.valid())
				_client.construct(cap());

			if (_client.constructed())
				return *_client;

			/*
			 * This error is printed if the session could not be
			 * established or the session is provided by a child service.
			 */
			error(SESSION::service_name(), " session (", _session_state->args(), ") "
			      "unavailable");
			throw Service_denied();
		}

		SESSION const &session() const
		{
			return const_cast<Local_connection *>(this)->session();
		}

		Local_connection(Service &service, Id_space<Parent::Client> &id_space,
		                 Parent::Client::Id id, Args const &args,
		                 Affinity const &affinity,
		                 Session::Label const &label = Session_label(),
		                 Session::Diag diag = { false })
		:
			Local_connection_base(service, id_space, id, args, affinity,
			                      label.valid() ? label : label_from_args(args.string()),
			                      diag,
			                      Session::Resources { Ram_quota { CONNECTION::RAM_QUOTA },
			                                           Cap_quota { CONNECTION::CAP_QUOTA } })
		{
			service.wakeup();
		}
};

#endif /* _INCLUDE__BASE__LOCAL_CONNECTION_H_ */
