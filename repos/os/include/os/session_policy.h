/*
 * \brief  Utilities for handling server-side session policies
 * \author Norman Feske
 * \date   2011-09-13
 */

/*
 * Copyright (C) 2011-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _INCLUDE__OS__SESSION_POLICY_H_
#define _INCLUDE__OS__SESSION_POLICY_H_

#include <util/arg_string.h>
#include <os/config.h>

namespace Genode {

	class Session_label
	{
		public:

			enum { MAX_LEN = 128 };

		private:

			char _buf[MAX_LEN];

		public:

			Session_label() { _buf[0] = 0; }

			/**
			 * Constructor
			 *
			 * \param args  session arguments as null-terminated string
			 */
			explicit Session_label(char const *args)
			{
				Arg_string::find_arg(args, "label").string(_buf, sizeof(_buf),
				                                           "<undefined>");
			}

			char const *string() const { return _buf; }
	};

	/**
	 * Query server-side policy for a session request
	 */
	class Session_policy : public Xml_node
	{
		public:

			/**
			 * Exception type
			 */
			class No_policy_defined { };

		private:

			/**
			 * Returns true if the start of the label matches the specified
			 * match string
			 */
			static bool _label_matches(Session_label const &label, char const *match) {
				return strcmp(label.string(), match, strlen(match)) == 0; }

			/**
			 * Query session policy from session label
			 */
			static Xml_node _query_policy(Session_label const &label)
			{
				/* find index of policy node that matches best */
				int best_match = -1;
				try {
					unsigned label_len = 0;
					Xml_node policy = config()->xml_node().sub_node();

					for (int i = 0;; i++, policy = policy.next()) {

						if (!policy.has_type("policy"))
							continue;

						/* label attribute from policy node */
						char policy_label[Session_label::MAX_LEN];
						policy.attribute("label").value(policy_label,
						                                sizeof(policy_label));

						if (!_label_matches(label, policy_label)
						 || strlen(policy_label) < label_len)
							continue;

						label_len = strlen(policy_label);
						best_match = i;
					}
				} catch (...) { }

				if (best_match != -1)
					return config()->xml_node().sub_node(best_match);

				throw No_policy_defined();
			}

		public:

			/**
			 * Constructor
			 *
			 * \param args  session arguments
			 *
			 * \throw No_policy_defined  if the server configuration has no
			 *                           policy defined for the session
			 *                           request
			 *
			 * On construction, the 'Session_policy' looks up the 'policy' XML
			 * node that matches the label provided as argument. The
			 * server-side policies are defined in one or more policy subnodes
			 * of the server's 'config' node. Each policy node has a label
			 * attribute. If the policy label matches the first part of the
			 * label delivered as session argument, the policy matches. If
			 * multiple policies match, the one with the largest label is
			 * selected.
			 */
			explicit Session_policy(Session_label const &label)
			: Xml_node(_query_policy(label)) { }
	};
}

#endif /* _INCLUDE__OS__SESSION_POLICY_H_ */
