/*
 * \brief  ROM service
 * \author Norman Feske
 * \date   2014-01-11
 */

/*
 * Copyright (C) 2014 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _ROM_SERVICE_H_
#define _ROM_SERVICE_H_

/* Genode includes */
#include <util/arg_string.h>
#include <util/xml_node.h>
#include <rom_session/rom_session.h>
#include <root/component.h>

/* local includes */
#include <rom_registry.h>

namespace Rom {
	class Session_component;
	class Root;

	using Genode::Xml_node;
}


class Rom::Session_component : public Genode::Rpc_object<Genode::Rom_session>,
                               public Reader
{
	private:

		Registry_for_reader &_registry;
		Module        const &_module;

		Lazy_volatile_object<Genode::Attached_ram_dataspace> _ds;

		Genode::Signal_context_capability _sigh;

	public:

		Session_component(Registry_for_reader &registry,
		                  Rom::Module::Name const &name)
		:
			_registry(registry), _module(_registry.lookup(*this, name))
		{ }

		~Session_component()
		{
			_registry.release(*this, _module);
		}

		Genode::Rom_dataspace_capability dataspace() override
		{
			using namespace Genode;

			/* replace dataspace by new one */
			/* XXX we could keep the old dataspace if the size fits */
			_ds.construct(env()->ram_session(), _module.size());

			/* fill dataspace content with report contained in module */
			_module.read_content(_ds->local_addr<char>(), _ds->size());

			/* cast RAM into ROM dataspace capability */
			Dataspace_capability ds_cap = static_cap_cast<Dataspace>(_ds->cap());

			return static_cap_cast<Rom_dataspace>(ds_cap);
		}

		bool update() override
		{
			if (!_ds.is_constructed() || _module.size() > _ds->size())
				return false;

			_module.read_content(_ds->local_addr<char>(), _ds->size());
			return true;
		}

		void sigh(Genode::Signal_context_capability sigh) override
		{
			_sigh = sigh;
		}

		/**
		 * Implementation of 'Reader' interface
		 */
		void notify_module_changed() const override
		{
			if (_sigh.valid())
				Genode::Signal_transmitter(_sigh).submit();
		}
};


class Rom::Root : public Genode::Root_component<Session_component>
{
	private:

		Registry_for_reader &_registry;
		Xml_node      const &_config;

		typedef Rom::Module::Name Label;

		/**
		 * Determine module name for label according to the configured policy
		 */
		Rom::Module::Name _module_name(Label const &label) const
		{
			try {
				for (Xml_node node = _config.sub_node("policy");
				     true; node = node.next("policy")) {

					if (!node.has_attribute("label")
					 || !node.has_attribute("report")
					 || !node.attribute("label").has_value(label.string()))
					 	continue;

					char report[Rom::Module::Name::capacity()];
					node.attribute("report").value(report, sizeof(report));
					return Rom::Module::Name(report);
				}
			} catch (Xml_node::Nonexistent_sub_node) { }

			PWRN("no valid policy for label \"%s\"", label.string());
			throw Root::Invalid_args();
		}

	protected:

		Session_component *_create_session(const char *args) override
		{
			using namespace Genode;

			/* read label of ROM module from args */
			char label[Label::capacity()];
			Arg_string::find_arg(args, "label").string(label, sizeof(label), "");

			return new (md_alloc())
				Session_component(_registry, _module_name(Label(label)));
		}

	public:

		Root(Server::Entrypoint  &ep,
		     Genode::Allocator   &md_alloc,
		     Registry_for_reader &registry,
		     Xml_node      const &config)
		:
			Genode::Root_component<Session_component>(&ep.rpc_ep(), &md_alloc),
			_registry(registry),
			_config(config)
		{ }
};

#endif /* _ROM_SERVICE_H_ */
