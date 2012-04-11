/*
 * \brief  D3m block service
 * \author Norman Feske
 * \author Christian Helmuth
 * \date   2012-01-25
 */

/*
 * Copyright (C) 2012 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _BLOCK_SERVICE_H_
#define _BLOCK_SERVICE_H_

/* Genode includes */
#include <util/list.h>
#include <block_session/capability.h>
#include <root/component.h>


/**
 * Facility to probe a block session for the presence of a specific file
 *
 * The 'Iso9660_boot_probe' utility is used to select an iso9660 formatted
 * block device to boot from by checking for the presence of a magic file.
 */
class Iso9660_boot_probe
{
	/**
	 * Policy for iso9660 server when executed as slave service
	 *
	 * The policy supplies a predefined block root capability to the iso9660
	 * server.
	 */
	struct Iso9660_policy : Genode::Slave_policy
	{
		Genode::Lock                      &_annouce_lock;
		Genode::Capability<Block::Session> _block_session;
		Genode::Root_capability            _rom_root;

		/**
		 * Pseudo service, handing out a predefined session capability
		 */
		struct Proxy_service : Genode::Service
		{
			Genode::Session_capability _session;

			Proxy_service(Genode::Session_capability session)
			: Genode::Service("proxy"), _session(session) { }

			Genode::Session_capability session(const char *) { return _session; }

			void upgrade(Genode::Session_capability session, const char *) { }

		} _block_proxy_service;

		Iso9660_policy(Genode::Rpc_entrypoint            &entrypoint,
		               Genode::Lock                      &annouce_lock,
		               Genode::Capability<Block::Session> block_session)
		:
			Genode::Slave_policy("iso9660", entrypoint),
			_annouce_lock(annouce_lock),
			_block_session(block_session),
			_block_proxy_service(block_session)
		{ }

		Genode::Root_capability rom_root() { return _rom_root; }


		/****************************
		 ** Slave_policy interface **
		 ****************************/

		const char **_permitted_services() const
		{
			static const char *permitted_services[] = {
				"CAP", "RM", "LOG", "SIGNAL", 0 };
			return permitted_services;
		};

		Genode::Service *resolve_session_request(const char *service_name,
		                                         const char *args)
		{
			if (Genode::strcmp(service_name, "Block") == 0)
				return &_block_proxy_service;

			return Genode::Slave_policy::resolve_session_request(service_name, args);
		}

		bool announce_service(char const             *service_name,
		                      Genode::Root_capability root,
		                      Genode::Allocator      *alloc,
		                      Genode::Server         *server)
		{
			if (Genode::strcmp(service_name, "ROM") != 0)
				return false;

			_rom_root = root;
			_annouce_lock.unlock();
			return true;
		}
	};

	Genode::Root_capability   _block_root;
	Block::Session_capability _block_session;
	Genode::Lock              _rom_announce_lock;
	Genode::Cap_connection    _cap;

	enum { STACK_SIZE = 2*4096 };
	Genode::Rpc_entrypoint _entrypoint;

	Iso9660_policy _iso9660_policy;
	Genode::Slave  _iso9660_slave;

	/**
	 * RAM quota to assign to the iso9660 service
	 */
	enum { ISO9660_RAM_QUOTA = 8*1024*1024 };

	/**
	 * Obtain block session from specified root interface
	 */
	Block::Session_capability _init_session()
	{
		char const *args = "ram_quota=140K, tx_buf_size=128K";
		Genode::Root_client root(_block_root);
		return Genode::static_cap_cast<Block::Session>(root.session(args));
	}

	/**
	 * Private constructor
	 *
	 * This constructor is used by the 'probe' function below.
	 */
	Iso9660_boot_probe(Genode::Root_capability root, char const *boot_tag_name)
	:
		_block_root(root),
		_block_session(_init_session()),
		_rom_announce_lock(Genode::Lock::LOCKED),
		_entrypoint(&_cap, STACK_SIZE, "iso9660_slave"),
		_iso9660_policy(_entrypoint, _rom_announce_lock, _block_session),
		_iso9660_slave(_entrypoint, _iso9660_policy, ISO9660_RAM_QUOTA)
	{
		/* wait until the iso9660 server announces the ROM service */
		_rom_announce_lock.lock();

		/* try to open file with filename 'boot_tag_name' */
		Genode::Root_client rom_root(_iso9660_policy.rom_root());
		char args[Genode::Root::Session_args::MAX_SIZE];
		Genode::snprintf(args, sizeof(args), "ram_quota=4K, filename=\"%s\"",
		                 boot_tag_name);
		rom_root.session(args);
	}

	public:

		/**
		 * Probe block service for the presence of a boot tag file
		 *
		 * \param root           root capability of block service to probe
		 * \param boot_tag_name  name of the file to probe for
		 *
		 * \return  true if the specified tag file exists at the block session
		 */
		static bool probe(Genode::Root_capability root,
		                  char const *boot_tag_name)
		{
			/*
			 * In the process of creating an 'Iso9660_boot_probe', many steps
			 * can fail. For example, the binary of the iso9660 server may be
			 * missing, the block service may not contain an iso9660 file
			 * system, or the file may be missing. Only if all steps succeed,
			 * we report the probing to have succeeded.
			 */
			try {
				/*
				 * Do not allocate the boot probe at the stack because this
				 * object may too large for the stack size of the context
				 * calling the probe function. We use the heap instead to
				 * create and subsequently destroy the boot probe.
				 */
				Iso9660_boot_probe *boot_probe = new (Genode::env()->heap())
					Iso9660_boot_probe(root, boot_tag_name);

				Genode::destroy(Genode::env()->heap(), boot_probe);
				return true;
			} catch (...) { }

			PWRN("Could not probe file at iso9660 ROM service");
			return false;
		}

		~Iso9660_boot_probe()
		{
			/* close session at block service */
			Genode::Root_client(_block_root).close(_block_session);
		}
};


namespace Block {

	class Driver : public Genode::List<Driver>::Element
	{
		private:

			char const             *_name;
			Genode::Root_capability _root;

		public:

			Driver() : _name(0) { }

			void init(char const *name, Genode::Root_capability root)
			{
				_name = name;
				_root = root;
			}

			char const *name() const { return _name; }

			Genode::Root_capability root() const { return _root; }
	};


	class Driver_registry
	{
		private:

			Genode::Lock         _lock;
			Genode::List<Driver> _drivers;
			Genode::Semaphore    _retry_probing_sem;

		public:

			void add_driver(Driver *driver)
			{
				Genode::Lock::Guard guard(_lock);

				_drivers.insert(driver);

				PDBG("\ninsert new driver %s", driver->name());
				_retry_probing_sem.up();
			}

			/**
			 * Return root capability of boot device
			 *
			 * If the boot device is not available yet, block until a matching
			 * driver becomes available.
			 */
			Genode::Root_capability root()
			{
				for (;;) {
					{
						Genode::Lock::Guard guard(_lock);
						while (_drivers.first()) {

							Driver *driver = _drivers.first();
							PDBG("\nprobing driver %s", driver->name());

							if (!Iso9660_boot_probe::probe(driver->root(),
							                               "libc.lib.so")) {
								PWRN("probing failed, not using %s as boot device",
								     driver->name());

								_drivers.remove(driver);
								break;
							}

							PINF("found boot medium via %s", driver->name());
							return driver->root();
						}
					}

					/*
					 * Block until another driver becomes known via
					 * 'add_driver'
					 */
					_retry_probing_sem.down();
				};
			}
	};


	class Root : public Genode::Rpc_object<Genode::Typed_root<Session> >
	{
		private:

			Driver_registry &_driver_registry;

		public:

			Root(Driver_registry &driver_registry)
			: _driver_registry(driver_registry) { }

			Genode::Session_capability session(Genode::Root::Session_args const &args)
			{
				PDBG("\nsession requested args=\"%s\"", args.string());
				Genode::Root_capability root = _driver_registry.root();
				return Genode::Root_client(root).session(args);
			}

			void upgrade(Genode::Session_capability,
			             Genode::Root::Upgrade_args const &) { }

			void close(Genode::Session_capability session)
			{
				Genode::Root_capability root = _driver_registry.root();
				Genode::Root_client(root).close(session);
			}
	};
}

#endif /* _BLOCK_SERVICE_H_ */
