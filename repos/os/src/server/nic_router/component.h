/*
 * \brief  Downlink interface in form of a NIC session component
 * \author Martin Stein
 * \date   2016-08-23
 */

/*
 * Copyright (C) 2016-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _COMPONENT_H_
#define _COMPONENT_H_

/* Genode includes */
#include <base/heap.h>
#include <root/component.h>
#include <nic/packet_allocator.h>
#include <nic_session/rpc_object.h>

/* local includes */
#include <mac_allocator.h>
#include <interface.h>
#include <reference.h>
#include <report.h>

namespace Genode { class Session_env; }

namespace Net {

	class Communication_buffer;
	class Session_component_base;
	class Session_component;
	class Root;
}


class Genode::Session_env : public Ram_allocator,
                            public Region_map
{
	private:

		Env                    &_env;
		Net::Quota             &_shared_quota;
		Ram_quota_guard         _ram_guard;
		Cap_quota_guard         _cap_guard;

		template <typename FUNC>
		void _consume(size_t  own_ram,
		              size_t  max_shared_ram,
		              size_t  own_cap,
		              size_t  max_shared_cap,
		              FUNC && functor)
		{
			size_t const max_ram_consumpt { own_ram + max_shared_ram };
			size_t const max_cap_consumpt { own_cap + max_shared_cap };
			size_t ram_consumpt { _env.pd().used_ram().value };
			size_t cap_consumpt { _env.pd().used_caps().value };
			{
				Ram_quota_guard::Reservation ram_reserv { _ram_guard, Ram_quota { max_ram_consumpt } };
				Cap_quota_guard::Reservation cap_reserv { _cap_guard, Cap_quota { max_cap_consumpt } };

				functor();

				ram_reserv.acknowledge();
				cap_reserv.acknowledge();
			}
			ram_consumpt = _env.pd().used_ram().value  - ram_consumpt;
			cap_consumpt = _env.pd().used_caps().value - cap_consumpt;

			if (ram_consumpt > max_ram_consumpt) {
				error("Session_env: more RAM quota consumed than expected"); }
			if (cap_consumpt > max_cap_consumpt) {
				error("Session_env: more CAP quota consumed than expected"); }
			if (ram_consumpt < own_ram) {
				error("Session_env: less RAM quota consumed than expected"); }
			if (cap_consumpt < own_cap) {
				error("Session_env: less CAP quota consumed than expected"); }

			_shared_quota.ram += ram_consumpt - own_ram;
			_shared_quota.cap += cap_consumpt - own_cap;

			_ram_guard.replenish( Ram_quota { max_shared_ram } );
			_cap_guard.replenish( Cap_quota { max_shared_cap } );
		}

		template <typename FUNC>
		void _replenish(size_t  accounted_ram,
		                size_t  accounted_cap,
		                FUNC && functor)
		{
			size_t ram_replenish { _env.pd().used_ram().value };
			size_t cap_replenish { _env.pd().used_caps().value };
			functor();
			ram_replenish = ram_replenish - _env.pd().used_ram().value;
			cap_replenish = cap_replenish - _env.pd().used_caps().value;

			if (ram_replenish < accounted_ram) {
				error("Session_env: less RAM quota replenished than expected"); }
			if (cap_replenish < accounted_cap) {
				error("Session_env: less CAP quota replenished than expected"); }

			_shared_quota.ram -= ram_replenish - accounted_ram;
			_shared_quota.cap -= cap_replenish - accounted_cap;

			_ram_guard.replenish( Ram_quota { accounted_ram } );
			_cap_guard.replenish( Cap_quota { accounted_cap } );

		}

	public:

		Session_env(Env             &env,
		            Net::Quota      &shared_quota,
		            Ram_quota const &ram_quota,
		            Cap_quota const &cap_quota)
		:
			_env          { env },
			_shared_quota { shared_quota },
			_ram_guard    { ram_quota },
			_cap_guard    { cap_quota }
		{ }

		Entrypoint &ep() { return _env.ep(); }


		/*******************
		 ** Ram_allocator **
		 *******************/

		Ram_dataspace_capability alloc(size_t size, Cache_attribute cached) override
		{
			enum { MAX_SHARED_CAP           = 1 };
			enum { MAX_SHARED_RAM           = 4096 };
			enum { DS_SIZE_GRANULARITY_LOG2 = 12 };

			size_t const ds_size = align_addr(size, DS_SIZE_GRANULARITY_LOG2);
			Ram_dataspace_capability ds;
			_consume(ds_size, MAX_SHARED_RAM, 1, MAX_SHARED_CAP, [&] () {
				ds = _env.pd().alloc(ds_size, cached);
			});
			return ds;
		}


		void free(Ram_dataspace_capability ds) override
		{
			_replenish(_env.pd().dataspace_size(ds), 1, [&] () {
				_env.pd().free(ds);
			});
		}

		size_t dataspace_size(Ram_dataspace_capability ds) const override { return _env.pd().dataspace_size(ds); }


		/****************
		 ** Region_map **
		 ****************/

		Local_addr attach(Dataspace_capability ds,
		                  size_t               size = 0,
		                  off_t                offset = 0,
		                  bool                 use_local_addr = false,
		                  Local_addr           local_addr = (void *)0,
		                  bool                 executable = false,
		                  bool                 writeable = true) override
		{
			enum { MAX_SHARED_CAP = 2 };
			enum { MAX_SHARED_RAM = 4 * 4096 };

			void *ptr;
			_consume(0, MAX_SHARED_RAM, 0, MAX_SHARED_CAP, [&] () {
				ptr = _env.rm().attach(ds, size, offset, use_local_addr,
				                       local_addr, executable, writeable);
			});
			return ptr;
		};

		void report(Genode::Xml_generator &xml) const
		{
			xml.node("ram-quota", [&] () {
				xml.attribute("used",  _ram_guard.used().value);
				xml.attribute("limit", _ram_guard.limit().value);
				xml.attribute("avail", _ram_guard.avail().value);
			});
			xml.node("cap-quota", [&] () {
				xml.attribute("used",  _cap_guard.used().value);
				xml.attribute("limit", _cap_guard.limit().value);
				xml.attribute("avail", _cap_guard.avail().value);
			});
		}

		void detach(Local_addr local_addr) override
		{
			_replenish(0, 0, [&] () { _env.rm().detach(local_addr); });
		}

		void fault_handler(Signal_context_capability handler) override { _env.rm().fault_handler(handler); }
		State state() override { return _env.rm().state(); }
		Dataspace_capability dataspace() override { return _env.rm().dataspace(); }


		/***************
		 ** Accessors **
		 ***************/

		Ram_quota_guard const &ram_guard() const { return _ram_guard; };
		Cap_quota_guard const &cap_guard() const { return _cap_guard; };
};


class Net::Communication_buffer
{
	private:

		Genode::Ram_allocator            &_ram_alloc;
		Genode::Ram_dataspace_capability  _ram_ds;

	public:

		Communication_buffer(Genode::Ram_allocator &ram_alloc,
		                     Genode::size_t const   size);

		~Communication_buffer() { _ram_alloc.free(_ram_ds); }


		/***************
		 ** Accessors **
		 ***************/

		Genode::Dataspace_capability ds() const { return _ram_ds; }
};


class Net::Session_component_base
{
	protected:

		Genode::Session_env  &_session_env;
		Genode::Heap          _alloc;
		Nic::Packet_allocator _packet_alloc;
		Communication_buffer  _tx_buf;
		Communication_buffer  _rx_buf;

	public:

		Session_component_base(Genode::Session_env       &session_env,
		                       Genode::size_t      const  tx_buf_size,
		                       Genode::size_t      const  rx_buf_size);
};


class Net::Session_component : private Session_component_base,
                               public  ::Nic::Session_rpc_object
{
	private:

		struct Interface_policy : Net::Interface_policy
		{
			private:

				Genode::Session_label    const  _label;
				Const_reference<Configuration>  _config;
				Genode::Session_env      const &_session_env;

			public:

				Interface_policy(Genode::Session_label const &label,
				                 Genode::Session_env   const &session_env,
				                 Configuration         const &config);


				/***************************
				 ** Net::Interface_policy **
				 ***************************/

				Domain_name determine_domain_name() const override;
				void handle_config(Configuration const &config) override { _config = config; }
				Genode::Session_label const &label() const override { return _label; }
				void report(Genode::Xml_generator &xml) const override { _session_env.report(xml); };
		};

		bool                                   _link_state { true };
		Interface_policy                       _interface_policy;
		Interface                              _interface;
		Genode::Ram_dataspace_capability const _ram_ds;

	public:

		Session_component(Genode::Session_env                    &session_env,
		                  Genode::size_t                   const  tx_buf_size,
		                  Genode::size_t                   const  rx_buf_size,
		                  Timer::Connection                      &timer,
		                  Mac_address                      const  mac,
		                  Mac_address                      const &router_mac,
		                  Genode::Session_label            const &label,
		                  Interface_list                         &interfaces,
		                  Configuration                          &config,
		                  Genode::Ram_dataspace_capability const  ram_ds);


		/******************
		 ** Nic::Session **
		 ******************/

		Mac_address mac_address() override { return _interface.mac(); }
		bool link_state() override { return _interface.link_state(); }
		void link_state_sigh(Genode::Signal_context_capability sigh) override {
			_interface.session_link_state_sigh(sigh); }


		/***************
		 ** Accessors **
		 ***************/

		Interface_policy           const &interface_policy() const { return _interface_policy; }
		Genode::Ram_dataspace_capability  ram_ds()           const { return _ram_ds; };
		Genode::Session_env        const &session_env()      const { return _session_env; };
};


class Net::Root : public Genode::Root_component<Session_component>
{
	private:

		enum { MAC_ALLOC_BASE = 0x02 };

		Genode::Env              &_env;
		Timer::Connection        &_timer;
		Mac_allocator             _mac_alloc;
		Mac_address        const  _router_mac;
		Reference<Configuration>  _config;
		Quota                    &_shared_quota;
		Interface_list           &_interfaces;

		void _invalid_downlink(char const *reason);


		/********************
		 ** Root_component **
		 ********************/

		Session_component *_create_session(char const *args) override;
		void _destroy_session(Session_component *session) override;

	public:

		Root(Genode::Env       &env,
		     Timer::Connection &timer,
		     Genode::Allocator &alloc,
		     Configuration     &config,
		     Quota             &shared_quota,
		     Interface_list    &interfaces);

		void handle_config(Configuration &config) { _config = Reference<Configuration>(config); }
};

#endif /* _COMPONENT_H_ */
