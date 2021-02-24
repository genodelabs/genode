/*
 * \brief  Report session provided by the CBE manager
 * \author Martin Stein
 * \author Norman Feske
 * \date   2021-02-25
 */

/*
 * Copyright (C) 2021 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _REPORT_SESSION_COMPONENT_H_
#define _REPORT_SESSION_COMPONENT_H_

/* Genode includes */
#include <base/attached_ram_dataspace.h>
#include <base/session_object.h>
#include <report_session/report_session.h>

namespace Report {

	using namespace Genode;

	class Session_component;
}


class Report::Session_component : public Session_object<Report::Session>
{
	public:

		struct Handler_base : Interface, Genode::Noncopyable
		{
			virtual void handle_report(char const *, size_t) = 0;
		};

		template <typename T>
		struct Xml_handler : Handler_base
		{
			T &_obj;
			void (T::*_member) (Xml_node const &);

			Xml_handler(T &obj, void (T::*member)(Xml_node const &))
			: _obj(obj), _member(member) { }

			void handle_report(char const *start, size_t length) override
			{
				(_obj.*_member)(Xml_node(start, length));
			}
		};

	private:

		Attached_ram_dataspace _ds;

		Handler_base &_handler;


		/*******************************
		 ** Report::Session interface **
		 *******************************/

		Dataspace_capability dataspace() override { return _ds.cap(); }

		void submit(size_t length) override
		{
			_handler.handle_report(_ds.local_addr<char const>(),
			                       min(_ds.size(), length));
		}

		void response_sigh(Signal_context_capability) override { }

		size_t obtain_response() override { return 0; }

	public:

		template <typename... ARGS>
		Session_component(Env &env, Handler_base &handler,
		                  Entrypoint &ep, Resources const &resources,
		                  ARGS &&... args)
		:
			Session_object(ep, resources, args...),
			_ds(env.ram(), env.rm(), resources.ram_quota.value),
			_handler(handler)
		{ }
};

#endif /* _REPORT_SESSION_COMPONENT_H_ */
