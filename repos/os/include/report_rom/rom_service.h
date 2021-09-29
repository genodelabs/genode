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

		Genode::Ram_allocator &_ram;
		Genode::Region_map    &_rm;

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

		/**
		 * Size of content delivered to the client
		 *
		 * Zero-sized content means invalidated ROM state was delivered to the
		 * client.
		 */
		size_t _content_size = 0;

		Genode::Signal_context_capability _sigh { };

		/*
		 * Keep track of the last version handed out to the client (at the
		 * time of the last 'Rom_session::update' RPC call, and the newest
		 * version that is available. If the client version is out of date
		 * when the client registers a signal handler, submit a signal
		 * immediately.
		 */
		unsigned _current_version = 0, _client_version = 0;

		void _notify_client()
		{
			if (_sigh.valid() && (_current_version != _client_version))
				Genode::Signal_transmitter(_sigh).submit();
		}

	public:

		Session_component(Genode::Ram_allocator &ram, Genode::Region_map &rm,
		                  Registry_for_reader &registry,
		                  Genode::Session_label const &label)
		:
			_ram(ram), _rm(rm),
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

			_client_version = _current_version;

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

			_client_version = _current_version;

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
		void mark_as_outdated() override { _current_version++; }

		/**
		 * Reader interface
		 */
		void mark_as_invalidated() override
		{
			/* increase version only if we delivered valid content last */
			if (_content_size > 0)
				_current_version++;
		}

		/**
		 * Reader interface
		 */
		void notify_client() override { _notify_client(); }
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
