/*
 * \brief  ROM service
 * \author Norman Feske
 * \date   2014-01-11
 */

/*
 * Copyright (C) 2014-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__REPORT_ROM__ROM_SERVICE_H_
#define _INCLUDE__REPORT_ROM__ROM_SERVICE_H_

/* Genode includes */
#include <util/arg_string.h>
#include <util/xml_node.h>
#include <rom_session/rom_session.h>
#include <root/component.h>
#include <report_rom/rom_registry.h>

namespace Rom {
	class Session_component;
	class Module_name_fn;
	class Root;

	using Genode::Xml_node;
}


class Rom::Session_component : public Genode::Rpc_object<Genode::Rom_session>,
                               public Reader
{
	private:

		Genode::Ram_session &_ram;
		Genode::Region_map  &_rm;

		Registry_for_reader &_registry;

		Genode::Session_label const _label;

		Readable_module &_module;

		Readable_module &_init_module(Genode::Session_label const &label)
		{
			try { return _registry.lookup(*this, label.string()); }
			catch (Registry_for_reader::Lookup_failed) {
				throw Genode::Service_denied(); }
		}

		Constructible<Genode::Attached_ram_dataspace> _ds { };

		size_t _content_size = 0;

		/**
		 * Keep state of valid content to notify the client only once when
		 * the ROM module becomes invalid.
		 */
		bool _valid = false;

		Genode::Signal_context_capability _sigh { };

		void _notify_client()
		{
			if (_sigh.valid())
				Genode::Signal_transmitter(_sigh).submit();
		}

	public:

		Session_component(Genode::Ram_session &ram, Genode::Region_map &rm,
		                  Registry_for_reader &registry,
		                  Genode::Session_label const &label)
		:
			_ram(ram), _rm(rm),
			_registry(registry), _label(label), _module(_init_module(label))
		{ }

		/**
		 * Constructor
		 *
		 * \deprecated
		 * \noapi
		 */
		Session_component(Registry_for_reader &registry,
		                  Genode::Session_label const &label) __attribute__((deprecated))
		:
			_ram(*Genode::env_deprecated()->ram_session()),
			_rm(*Genode::env_deprecated()->rm_session()),
			_registry(registry), _label(label), _module(_init_module(label))
		{ }

		~Session_component()
		{
			_registry.release(*this, _module);
		}

		Genode::Session_label label() const { return _label; }

		Genode::Rom_dataspace_capability dataspace() override
		{
			using namespace Genode;

				/* replace dataspace by new one */
				/* XXX we could keep the old dataspace if the size fits */
				_ds.construct(_ram, _rm, _module.size());

				/* fill dataspace content with report contained in module */
				_content_size =
					_module.read_content(*this, _ds->local_addr<char>(), _ds->size());

				_valid = _content_size > 0;

				/* cast RAM into ROM dataspace capability */
				Dataspace_capability ds_cap = static_cap_cast<Dataspace>(_ds->cap());
				return static_cap_cast<Rom_dataspace>(ds_cap);
		}

		bool update() override
		{
			if (!_ds.constructed() || _module.size() > _ds->size())
				return false;

			size_t const new_content_size =
				_module.read_content(*this, _ds->local_addr<char>(), _ds->size());

			/* clear difference between old and new content */
			if (new_content_size < _content_size)
				Genode::memset(_ds->local_addr<char>() + new_content_size, 0,
				               _content_size - new_content_size);

			_content_size = new_content_size;

			_valid = _content_size > 0;

			return true;
		}

		void sigh(Genode::Signal_context_capability sigh) override
		{
			_sigh = sigh;

			/*
			 * Notify client initially to enforce a client-side ROM update.
			 * Otherwise, a server-side ROM update between session creation and
			 * signal-handler registration would go unnoticed.
			 */
			_notify_client();
		}

		/**
		 * Reader interface
		 */
		void notify_module_changed() override
		{
			_notify_client();
		}

		/**
		 * Reader interface
		 */
		void notify_module_invalidated() override
		{
			/* deliver a signal for an invalidated module only once */
			if (!_valid)
				return;

			_valid = false;
			_notify_client();
		}
};


class Rom::Root : public Genode::Root_component<Session_component>
{
	private:

		Genode::Env         &_env;
		Registry_for_reader &_registry;

	protected:

		Session_component *_create_session(const char *args) override
		{
			using namespace Genode;

			return new (md_alloc())
				Session_component(_env.ram(), _env.rm(), _registry, label_from_args(args));
		}

	public:

		Root(Genode::Env          &env,
		     Genode::Allocator    &md_alloc,
		     Registry_for_reader  &registry)
		:
			Genode::Root_component<Session_component>(&env.ep().rpc_ep(), &md_alloc),
			_env(env), _registry(registry)
		{ }
};

#endif /* _INCLUDE__REPORT_ROM__ROM_SERVICE_H_ */
