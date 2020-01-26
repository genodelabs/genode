/*
 * \brief  Cached information of the current runtime configuration
 * \author Norman Feske
 * \date   2019-02-22
 */

/*
 * Copyright (C) 2019 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _MODEL__RUNTIME_CONFIG_H_
#define _MODEL__RUNTIME_CONFIG_H_

/* Genode includes */
#include <util/xml_node.h>
#include <util/list_model.h>
#include <base/registry.h>

/* local includes */
#include <types.h>
#include <model/service.h>

namespace Sculpt { class Runtime_config; }


class Sculpt::Runtime_config
{
	private:

		Allocator &_alloc;

		/**
		 * Return target name of route specified by <service> node
		 *
		 * For a route to another child, the target name is the child name.
		 * For a route to the parent, the target name expresses a role of
		 * the parent:
		 *
		 * - 'hardware' provides access to hardware
		 * - 'config' allows the change of the systems configuration
		 * - 'info' reveals system information
		 * - 'GUI' connects to the nitpicker GUI server
		 */
		static Start_name _to_name(Xml_node node)
		{
			Start_name result { };
			node.with_sub_node("child", [&] (Xml_node child) {
				result = child.attribute_value("name", Start_name()); });

			if (result.valid())
				return result;

			node.with_sub_node("parent", [&] (Xml_node parent) {

				Service::Type_name const service =
					node.attribute_value("name", Service::Type_name());

				Label const dst_label =
					parent.attribute_value("label", Label());

				bool const ignored_service = (service == "CPU")
				                          || (service == "PD")
				                          || (service == "Report")
				                          || (service == "Timer")
				                          || (service == "LOG");
				if (ignored_service)
					return;

				bool const hardware = (service == "Platform")
				                   || (service == "IO_PORT")
				                   || (service == "IO_MEM")
				                   || (service == "Rtc")
				                   || (service == "IRQ")
				                   || (service == "TRACE");
				if (hardware) {
					result = "hardware";
					return;
				}

				bool const usb = (service == "Usb");
				if (usb) {
					result = "usb";
					return;
				}

				bool const storage = (service == "Block");
				if (storage) {
					result = "storage";
					return;
				}

				if (service == "ROM") {

					/*
					 * ROM sessions for plain binaries (e.g, as requested by
					 * the sculpt-managed inspect or part_block instances) are
					 * not interesting for the graph. Non-sculpt-managed
					 * subsystems can only be connected to the few ROMs
					 * whitelisted in the 'Parent_services' definition below.
					 */
					bool const interesting_rom =
						dst_label.valid() &&
						(strcmp("config", dst_label.string(), 5) == 0 ||
						 dst_label == "platform_info" ||
						 dst_label == "capslock");

					if (interesting_rom) {
						result = "info";
						return;
					}
				}

				if (service == "File_system") {

					if (dst_label == "config") {
						result = "config";
						return;
					}

					if (dst_label == "fonts" || dst_label == "report") {
						result = "info";
						return;
					}
				}

				if (service == "Nitpicker") {
					result = "GUI";
					return;
				}
			});

			return result;
		}

		/**
		 * Return component name targeted by the first route of the start node
		 */
		static Start_name _primary_dependency(Xml_node const start)
		{
			Start_name result { };
			start.with_sub_node("route", [&] (Xml_node route) {
				route.with_sub_node("service", [&] (Xml_node service) {
					result = _to_name(service); }); });

			return result;
		}

		struct Child_service : Service, List_model<Child_service>::Element
		{
			static Service::Type type_from_xml(Xml_node service)
			{
				auto const name = service.attribute_value("name", Service::Type_name());
				for (unsigned i = 0; i < (unsigned)Type::UNDEFINED; i++) {
					Type const t = (Type)i;
					if (name == Service::name_attr(t))
						return t;
				}

				return Type::UNDEFINED;
			}

			Child_service(Start_name server, Xml_node provides)
			: Service(server, type_from_xml(provides), Label()) { }

			struct Update_policy
			{
				typedef Child_service Element;

				Start_name _server;
				Allocator &_alloc;

				Update_policy(Start_name const &server, Allocator &alloc)
				: _server(server), _alloc(alloc) { }

				void destroy_element(Element &elem) { destroy(_alloc, &elem); }

				Element &create_element(Xml_node node)
				{
					return *new (_alloc) Child_service(_server, node);
				}

				void update_element(Element &, Xml_node) { }

				static bool element_matches_xml_node(Element const &elem, Xml_node node)
				{
					return type_from_xml(node) == elem.type;
				}

				static bool node_is_element(Xml_node node)
				{
					return type_from_xml(node) != Service::Type::UNDEFINED;
				}
			};
		};

	public:

		struct Component : List_model<Component>::Element
		{
			Start_name const name;

			Start_name primary_dependency { };

			struct Dep : List_model<Dep>::Element
			{
				Start_name const to_name;

				Dep(Start_name to_name) : to_name(to_name) { }

				struct Update_policy
				{
					typedef Dep Element;

					Allocator &_alloc;

					Update_policy(Allocator &alloc) : _alloc(alloc) { }

					void destroy_element(Dep &elem) { destroy(_alloc, &elem); }

					Dep &create_element(Xml_node node)
					{
						return *new (_alloc) Dep(_to_name(node));
					}

					void update_element(Dep &, Xml_node) { }

					static bool element_matches_xml_node(Dep const &elem, Xml_node node)
					{
						return _to_name(node) == elem.to_name;
					}

					static bool node_is_element(Xml_node node)
					{
						return _to_name(node).valid();
					}
				};
			};

			/* dependencies on other child components */
			List_model<Dep> deps { };

			template <typename FN>
			void for_each_secondary_dep(FN const &fn) const
			{
				deps.for_each([&] (Dep const &dep) {
					if (dep.to_name != primary_dependency)
						fn(dep.to_name); });
			}

			List_model<Child_service> _child_services { };

			Component(Start_name const &name) : name(name) { }

			template <typename FN>
			void for_each_service(FN const &fn) const
			{
				_child_services.for_each(fn);
			}

			struct Update_policy
			{
				typedef Component Element;

				Allocator &_alloc;

				Update_policy(Allocator &alloc) : _alloc(alloc) { }

				void destroy_element(Component &elem)
				{
					/* flush list models */
					update_element(elem, Xml_node("<start> <route/> <provides/> </start>"));

					destroy(_alloc, &elem);
				}

				Component &create_element(Xml_node node)
				{
					return *new (_alloc)
						Component(node.attribute_value("name", Start_name()));
				}

				void update_element(Component &elem, Xml_node node)
				{
					elem.primary_dependency = _primary_dependency(node);

					{
						Dep::Update_policy policy { _alloc };

						node.with_sub_node("route", [&] (Xml_node route) {
							elem.deps.update_from_xml(policy, route); });
					}

					{
						Child_service::Update_policy policy { elem.name, _alloc };

						node.with_sub_node("provides", [&] (Xml_node provides) {
							elem._child_services.update_from_xml(policy,
							                                     provides); });
					}
				}

				static bool element_matches_xml_node(Component const &elem, Xml_node node)
				{
					return node.attribute_value("name", Start_name()) == elem.name;
				}

				static bool node_is_element(Xml_node node) { return node.has_type("start"); }
			};
		};

	private:

		List_model<Component> _components { };

		struct Parent_services
		{
			typedef Registered_no_delete<Service> Parent_service;
			typedef Service::Type Type;

			Registry<Parent_service> _r { };

			Parent_service const
				_focus     { _r, Type::NITPICKER,   "keyboard focus",                 "focus" },
				_backdrop  { _r, Type::NITPICKER,   "desktop background",             "backdrop" },
				_lockscreen{ _r, Type::NITPICKER,   "desktop lock screen",            "lock_screen" },
				_nitpicker { _r, Type::NITPICKER,   "system GUI server" },
				_config_fs { _r, Type::FILE_SYSTEM, "writeable system configuration", "config" },
				_report_fs { _r, Type::FILE_SYSTEM, "read-only system reports",       "report" },
				_capslock  { _r, Type::ROM,         "global capslock state",          "capslock" },
				_vimrc     { _r, Type::ROM,         "default vim configuration",      "config -> vimrc" },
				_fonts     { _r, Type::ROM,         "system font configuration",      "config -> managed/fonts" },
				_pf_info   { _r, Type::ROM,         "platform information",           "platform_info" },
				_system    { _r, Type::ROM,         "system status",                  "config -> system" },
				_report    { _r, Type::REPORT,      "system reports" },
				_shape     { _r, Type::REPORT,      "pointer shape",    "shape",     Service::Match_label::LAST },
				_copy      { _r, Type::REPORT,      "global clipboard", "clipboard", Service::Match_label::LAST },
				_paste     { _r, Type::ROM,         "global clipboard", "clipboard", Service::Match_label::LAST },
				_rm        { _r, Type::RM,          "custom virtual memory objects" },
				_io_mem    { _r, Type::IO_MEM,      "raw hardware access" },
				_io_port   { _r, Type::IO_PORT,     "raw hardware access" },
				_irq       { _r, Type::IRQ,         "raw hardware access" },
				_rtc       { _r, Type::RTC,         "system clock" },
				_block     { _r, Type::BLOCK,       "direct block-device access" },
				_usb       { _r, Type::USB,         "direct USB-device access" },
				_pci_wifi  { _r, Type::PLATFORM,    "wifi hardware",    "wifi" },
				_pci_net   { _r, Type::PLATFORM,    "network hardware", "nic" },
				_pci_audio { _r, Type::PLATFORM,    "audio hardware",   "audio" },
				_pci_acpi  { _r, Type::PLATFORM,    "ACPI",             "acpica" },
				_trace     { _r, Type::TRACE,       "system-global tracing" },
				_vm        { _r, Type::VM,          "virtualization hardware" };

			template <typename FN>
			void for_each(FN const &fn) const { _r.for_each(fn); }

		} _parent_services { };

		Service const _used_fs_service { "default_fs_rw",
		                                 Service::Type::FILE_SYSTEM,
		                                 Label(), "used file system" };

	public:

		Runtime_config(Allocator &alloc) : _alloc(alloc) { }

		void update_from_xml(Xml_node config)
		{
			Component::Update_policy policy(_alloc);
			_components.update_from_xml(policy, config);
		}

		template <typename FN>
		void for_each_component(FN const &fn) const { _components.for_each(fn); }

		/**
		 * Call 'fn' with the name of each dependency of component 'name'
		 */
		template <typename FN>
		void for_each_dependency(Start_name const &name, FN const &fn) const
		{
			_components.for_each([&] (Component const &component) {
				if (component.name == name) {
					component.deps.for_each([&] (Component::Dep const &dep) {
						fn(dep.to_name); }); } });
		}

		template <typename FN>
		void for_each_service(FN const &fn) const
		{
			_parent_services.for_each(fn);

			fn(_used_fs_service);

			_components.for_each([&] (Component const &component) {
				component.for_each_service(fn); });
		}
};

#endif /* _MODEL__RUNTIME_CONFIG_H_ */
