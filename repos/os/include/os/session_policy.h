/*
 * \brief  Utilities for handling server-side session policies
 * \author Norman Feske
 * \date   2011-09-13
 */

/*
 * Copyright (C) 2011-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__OS__SESSION_POLICY_H_
#define _INCLUDE__OS__SESSION_POLICY_H_

#include <base/session_label.h>
#include <base/log.h>
#include <session/session.h>
#include <util/arg_string.h>
#include <util/xml_node.h>

namespace Genode {

	struct Xml_node_label_score;
	class  Session_policy;
}


/**
 * Score for matching an Xml_node against a label
 *
 * The score is based on the attributes 'label', 'label_prefix', and
 * 'label_suffix'.
 */
struct Genode::Xml_node_label_score
{
	bool  label_present = true;
	bool prefix_present = true;
	bool suffix_present = true;

	bool label_match = false;

	/*
	 * The match values contain the number of matching characters + 1.
	 * If 0, there is a conflict. If 1, an empty string matched.
	 */
	enum { CONFLICT = 0 };
	unsigned prefix_match = CONFLICT;
	unsigned suffix_match = CONFLICT;

	Xml_node_label_score() { }

	template <size_t N>
	Xml_node_label_score(Xml_node node, String<N> const &label)
	:
		label_present (node.has_attribute("label")),
		prefix_present(node.has_attribute("label_prefix")),
		suffix_present(node.has_attribute("label_suffix"))
	{
		if (label_present)
			label_match = node.attribute_value("label", String<N>()) == label;

		if (prefix_present) {
			typedef String<N> Prefix;
			Prefix const prefix = node.attribute_value("label_prefix", Prefix());

			if (!strcmp(label.string(), prefix.string(), prefix.length() - 1))
				prefix_match = prefix.length();
		}

		if (suffix_present) {
			typedef String<N> Suffix;
			Suffix const suffix = node.attribute_value("label_suffix", Suffix());

			if (label.length() >= suffix.length()) {
				unsigned const offset = label.length() - suffix.length();

				if (!strcmp(label.string() + offset, suffix.string()))
					suffix_match = suffix.length();
			}
		}
	}

	bool conflict() const
	{
		return (label_present  && !label_match)
		    || (prefix_present && !prefix_match)
		    || (suffix_present && !suffix_match);
	}

	/**
	 * Return true if this node's score is higher than 'other'
	 */
	bool stronger(Xml_node_label_score const &other) const
	{
		/* something must match */
		if (!(label_present || prefix_present || suffix_present))
			return false;

		/* if we are in conflict, we have a lower score than any other node */
		if (conflict())
			return false;

		/* there are no conflicts */

		/* we have a higher score than another conflicting node */
		if (other.conflict())
			return true;

		if (label_present && !other.label_present)
			return true;

		if (other.label_present)
			return false;

		/* labels are equally good */

		if (prefix_present && !other.prefix_present)
			return true;

		if (!prefix_present && other.prefix_present)
			return false;

		if (prefix_present && other.prefix_present) {

			if (prefix_match > other.prefix_match)
				return true;

			if (prefix_match < other.prefix_match)
				return false;
		}

		/* prefixes are equally good */

		if (suffix_present && !other.suffix_present)
			return true;

		if (!suffix_present && other.suffix_present)
			return false;

		if (suffix_present && other.suffix_present) {

			if (suffix_match > other.suffix_match)
				return true;

			if (suffix_match < other.suffix_match)
				return false;
		}

		/* nodes are equally good */

		return false;
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
		class No_policy_defined : public Service_denied { };

	private:

		/**
		 * Query session policy from session label
		 */
		template <size_t N>
		static Xml_node _query_policy(String<N> const &label, Xml_node config)
		{
			/*
			 * Find policy node that matches best
			 */
			Xml_node             best_match("<none/>");
			Xml_node_label_score best_score;

			/*
			 * Functor to be applied to each policy node
			 */
			auto lambda = [&] (Xml_node policy) {
				Xml_node_label_score const score(policy, label);
				if (score.stronger(best_score)) {
					best_match = policy;
					best_score = score;
				}
			};

			config.for_each_sub_node("policy", lambda);

			if (!best_match.has_type("none"))
				return best_match;

			try { return config.sub_node("default-policy"); }
			catch (...) { }

			warning("no policy defined for label '", label, "'");
			throw No_policy_defined();
		}

	public:

		/**
		 * Constructor
		 *
		 * \param label   label used as the selector of a policy
		 * \param config  XML node that contains the policies as sub nodes
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
		Session_policy(String<N> const &label, Xml_node config)
		:
			Xml_node(_query_policy(label, config))
		{ }
};

#endif /* _INCLUDE__OS__SESSION_POLICY_H_ */
