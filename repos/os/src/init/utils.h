/*
 * \brief  Utilities
 * \author Norman Feske
 * \date   2010-05-04
 */

/*
 * Copyright (C) 2010-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _SRC__INIT__UTIL_H_
#define _SRC__INIT__UTIL_H_

namespace Init {

	static inline void warn_insuff_quota(size_t const avail)
	{
		warning("specified quota exceeds available quota, "
		        "proceeding with a quota of ", avail);
	}


	/**
	 * Return sub string of label with the leading child name stripped out
	 *
	 * \return character pointer to the scoped part of the label,
	 *         or nullptr if the label is not correctly prefixed with the child's
	 *         name
	 */
	inline char const *skip_label_prefix(char const *child_name, char const *label)
	{
		size_t const child_name_len = strlen(child_name);

		if (strcmp(child_name, label, child_name_len) != 0)
			return nullptr;

		label += child_name_len;

		/*
		 * Skip label separator. This condition should be always satisfied.
		 */
		if (strcmp(" -> ", label, 4) != 0)
			return nullptr;

		return label + 4;
	}


	/**
	 * Return true if service XML node matches service request
	 *
	 * \param args          session arguments, inspected for the session label
	 * \param child_name    name of the originator of the session request
	 * \param service_name  name of the requested service
	 */
	inline bool service_node_matches(Xml_node           const  service_node,
	                                 Session_label      const &label,
	                                 Child_policy::Name const &child_name,
	                                 Service::Name      const &service_name)
	{
		bool const service_matches =
			service_node.has_type("any-service") ||
			(service_node.has_type("service") &&
			 service_node.attribute("name").has_value(service_name.string()));

		if (!service_matches)
			return false;

		typedef String<Session_label::capacity()> Label;

		char const *unscoped_attr   = "unscoped_label";
		char const *label_last_attr = "label_last";

		bool const route_depends_on_child_provided_label =
			service_node.has_attribute("label") ||
			service_node.has_attribute("label_prefix") ||
			service_node.has_attribute("label_suffix") ||
			service_node.has_attribute(label_last_attr);

		if (service_node.has_attribute(unscoped_attr)) {

			/*
			 * If an 'unscoped_label' attribute is provided, don't consider any
			 * scoped label attribute.
			 */
			if (route_depends_on_child_provided_label)
				warning("service node contains both scoped and unscoped label attributes");

			return label == service_node.attribute_value(unscoped_attr, Label());
		}

		if (service_node.has_attribute(label_last_attr))
			return service_node.attribute_value(label_last_attr, Label()) == label.last_element();

		if (!route_depends_on_child_provided_label)
			return true;

		char const * const scoped_label = skip_label_prefix(
			child_name.string(), label.string());

		if (!scoped_label)
			return false;

		Session_label const session_label(scoped_label);

		return !Xml_node_label_score(service_node, session_label).conflict();
	}


	/**
	 * Check if service name is ambiguous
	 *
	 * \return true  if the same service is provided multiple
	 *               times
	 *
	 * \deprecated
	 */
	template <typename T>
	inline bool is_ambiguous(Registry<T> const &services, Service::Name const &name)
	{
		/* count number of services with the specified name */
		unsigned cnt = 0;
		services.for_each([&] (T const &service) {
			cnt += (service.name() == name); });

		return cnt > 1;
	}

	/**
	 * Find service with certain values in given registry
	 *
	 * \param services   service registry
	 * \param name       name of wanted service
	 * \param filter_fn  function that applies additional filters
	 *
	 * \throw Service_denied
	 */
	template <typename T, typename FILTER_FN>
	inline T &find_service(Registry<T> &services,
	                       Service::Name const &name,
	                       FILTER_FN const &filter_fn)
	{
		T *service = nullptr;
		services.for_each([&] (T &s) {

			if (service || s.name() != name || filter_fn(s))
				return;

			service = &s;
		});

		if (!service)
			throw Service_denied();

		if (service->abandoned())
			throw Service_denied();

		return *service;
	}


	inline void generate_ram_info(Xml_generator &xml, Ram_session const &ram)
	{
		typedef String<32> Value;
		xml.attribute("quota", Value(ram.ram_quota()));
		xml.attribute("used",  Value(ram.used_ram()));
		xml.attribute("avail", Value(ram.avail_ram()));
	}

	inline void generate_caps_info(Xml_generator &xml, Pd_session const &pd)
	{
		typedef String<32> Value;
		xml.attribute("quota", Value(pd.cap_quota()));
		xml.attribute("used",  Value(pd.used_caps()));
		xml.attribute("avail", Value(pd.avail_caps()));
	}

	/**
	 * Read priority-levels declaration from config
	 */
	inline Prio_levels prio_levels_from_xml(Xml_node config)
	{
		long const prio_levels = config.attribute_value("prio_levels", 0UL);

		if (prio_levels && (prio_levels != (1 << log2(prio_levels)))) {
			warning("prio levels is not power of two, priorities are disabled");
			return Prio_levels { 0 };
		}
		return Prio_levels { prio_levels };
	}


	inline long priority_from_xml(Xml_node start_node, Prio_levels prio_levels)
	{
		long priority = Cpu_session::DEFAULT_PRIORITY;
		try { start_node.attribute("priority").value(&priority); }
		catch (...) { }

		/*
		 * All priority declarations in the config file are
		 * negative because child priorities can never be higher
		 * than parent priorities. To simplify priority
		 * calculations, we use inverted values. Lower values
		 * correspond to higher priorities.
		 */
		priority = -priority;

		if (priority && (priority >= prio_levels.value)) {
			long new_prio = prio_levels.value ? prio_levels.value - 1 : 0;
			char name[Service::Name::capacity()];
			start_node.attribute("name").value(name, sizeof(name));
			warning(Cstring(name), ": invalid priority, upgrading "
			        "from ", -priority, " to ", -new_prio);
			return new_prio;
		}

		return priority;
	}


	inline Affinity::Location
	affinity_location_from_xml(Affinity::Space const &space, Xml_node start_node)
	{
		typedef Affinity::Location Location;
		try {
			Xml_node node = start_node.sub_node("affinity");

			/* if no position value is specified, select the whole row/column */
			unsigned long const
				default_width  = node.has_attribute("xpos") ? 1 : space.width(),
				default_height = node.has_attribute("ypos") ? 1 : space.height();

			unsigned long const
				width  = node.attribute_value<unsigned long>("width",  default_width),
				height = node.attribute_value<unsigned long>("height", default_height);

			long const x1 = node.attribute_value<long>("xpos", 0),
			           y1 = node.attribute_value<long>("ypos", 0),
			           x2 = x1 + width  - 1,
			           y2 = y1 + height - 1;

			/* clip location to space boundary */
			return Location(max(x1, 0L), max(y1, 0L),
			                min((unsigned)(x2 - x1 + 1), space.width()),
			                min((unsigned)(y2 - y1 + 1), space.height()));
		}
		catch (...) { return Location(0, 0, space.width(), space.height()); }
	}


	/**
	 * Read affinity-space parameters from config
	 *
	 * If no affinity space is declared, construct a space with a single element,
	 * width and height being 1. If only one of both dimensions is specified, the
	 * other dimension is set to 1.
	 */
	inline Affinity::Space affinity_space_from_xml(Xml_node config)
	{
		try {
			Xml_node node = config.sub_node("affinity-space");
			return Affinity::Space(node.attribute_value<unsigned long>("width",  1),
			                       node.attribute_value<unsigned long>("height", 1));
		} catch (...) {
			return Affinity::Space(1, 1); }
	}
}

#endif /* _SRC__INIT__UTIL_H_ */
