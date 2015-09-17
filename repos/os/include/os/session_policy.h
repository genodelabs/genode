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

	struct Session_label;
	class  Session_policy;
}


struct Genode::Session_label : String<128>
{
	Session_label() { }

	/**
	 * Constructor
	 *
	 * \param args  session arguments as null-terminated string
	 *
	 * The constructor extracts the label from the supplied session-argument
	 * string.
	 */
	explicit Session_label(char const *args)
	{
		typedef String<128> String;

		char buf[String::capacity()];
		Arg_string::find_arg(args, "label").string(buf, sizeof(buf),
		                                           "<undefined>");
		*static_cast<String *>(this) = String(buf);
	}
};


/**
 * Query server-side policy for a session request
 */
class Genode::Session_policy : public Xml_node
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
		static bool _label_matches(char const *label, char const *match) {
			return strcmp(label, match, strlen(match)) == 0; }

		/**
		 * Query session policy from session label
		 */
		static Xml_node _query_policy(char const *label, Xml_node config)
		{
			/*
			 * Find policy node that matches best
			 */
			Xml_node best_match("<none/>");

			unsigned label_len = 0;

			/*
			 * Functor to be applied to each policy node
			 */
			auto lambda = [&] (Xml_node policy) {

				/* label attribute from policy node */
				char policy_label[Session_label::capacity()];
				policy.attribute("label").value(policy_label,
				                                sizeof(policy_label));

				if (!_label_matches(label, policy_label)
				 || strlen(policy_label) < label_len)
					return;

				label_len = strlen(policy_label);
				best_match = policy;
			};

			config.for_each_sub_node("policy", lambda);

			if (!best_match.has_type("none"))
				return best_match;

			throw No_policy_defined();
		}

	public:

		/**
		 * Constructor
		 *
		 * \param label   label used as the selector of a policy
		 * \param config  XML node that contains the policies as sub nodes,
		 *                using the component's top-level config node by
		 *                default
		 *
		 * \throw No_policy_defined  the server configuration has no
		 *                           policy defined for the specified label
		 *
		 * On construction, the 'Session_policy' looks up the 'policy' XML node
		 * that matches the label provided as argument. The server-side
		 * policies are defined in one or more policy subnodes of the server's
		 * 'config' node. Each policy node has a label attribute. If the policy
		 * label matches the first part of the label as delivered as session
		 * argument, the policy matches. If multiple policies match, the one
		 * with the longest label is selected.
		 */
		template <size_t N>
		explicit Session_policy(String<N> const &label,
		                        Xml_node config = Genode::config()->xml_node())
		:
			Xml_node(_query_policy(label.string(), config))
		{ }
};

#endif /* _INCLUDE__OS__SESSION_POLICY_H_ */
