/**
 * \brief  Block driver session creation
 * \author Sebastian Sumpf
 * \date   2015-09-29
 */

/*
 * Copyright (C) 2016-2024 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Genode includes */
#include <base/attached_rom_dataspace.h>
#include <base/component.h>
#include <base/log.h>
#include <block/request_stream.h>
#include <os/session_policy.h>
#include <timer_session/connection.h>
#include <util/xml_node.h>
#include <os/reporter.h>

/* local includes */
#include <ahci.h>
#include <ata_protocol.h>
#include <atapi_protocol.h>

namespace Ahci {
	struct Dispatch;
	class  Driver;
	struct Main;
	struct Block_session_handler;
	struct Block_session_component;
}


struct Ahci::Dispatch : Interface
{
	virtual void session(unsigned index) = 0;
};


class Ahci::Driver : Noncopyable
{
	public:

		enum { MAX_PORTS = 32 };

	private:

		struct Timer_delayer : Mmio<0>::Delayer, Timer::Connection
		{
			using Timer::Connection::Connection;

			void usleep(uint64_t us) override { Timer::Connection::usleep(us); }
		};

		Env                   & _env;
		Dispatch              & _dispatch;
		Timer_delayer           _delayer   { _env };
		Signal_handler<Driver>  _handler   { _env.ep(), *this, &Driver::handle_irq };
		Resources               _resources { _env, _handler };

		Constructible<Attached_rom_dataspace> _system_rom { };
		Signal_handler<Driver>  _system_rom_sigh {
			_env.ep(), *this, &Driver::_system_update };

		Constructible<Ata::Protocol>   _ata  [MAX_PORTS];
		Constructible<Atapi::Protocol> _atapi[MAX_PORTS];
		Constructible<Port>            _ports[MAX_PORTS];

		bool _enable_atapi;
		bool _schedule_stop { };

		unsigned _scan_ports(Region_map &rm, Platform::Connection &plat, Hba &hba)
		{
			log("port scan:");

			unsigned port_count = 0;

			for (unsigned index = 0; index < MAX_PORTS; index++) {

				Port_base port(index, plat, hba, _delayer);

				if (port.implemented() == false)
					continue;

				bool enabled = false;
				if (port.ata()) {
					try {
						_ata[index].construct();
						_ports[index].construct(*_ata[index], rm, plat,
						                        hba, _delayer, index);
						enabled = true;
					} catch (...) { }

					log("\t\t#", index, ":", enabled ? " ATA" : " off (ATA)");
				} else if (port.atapi() && _enable_atapi) {
					try {
						_atapi[index].construct();
						_ports[index].construct(*_atapi[index], rm, plat,
						                        hba, _delayer, index);
						enabled = true;
					} catch (...) { }

					log("\t\t#", index, ":", enabled ? " ATAPI" : " off (ATAPI)");
				} else {
						log("\t\t#", index, ":", port.atapi() ? " off (ATAPI)"
						    : "  off (unknown device signature)");
				}

				port_count++;
			}

			return port_count;
		}

		void _system_update()
		{
			if (!_system_rom.constructed())
				return;

			_system_rom->update();

			if (!_system_rom->valid())
				return;

			auto state = _system_rom->xml().attribute_value("state",
			                                                String<32>(""));

			if (state == "driver_stop") {
				_schedule_stop = true;

				for_each_port([&](auto &port, auto, auto) {
					port.stop_processing = true;
				});

				device_release_if_stopped_and_idle();

				return;
			}

			if (state == "driver_reinit") {
				_resources.acquire_device();

				_schedule_stop = false;

				/* re-start request handling of client sessions */
				for_each_port([&](auto &port, auto const index, auto) {
					port.stop_processing = false;
					port.reinit();
					_dispatch.session(index);
				});

				return;
			}
		}

	public:

		Driver(Env &env, Dispatch &dispatch, bool support_atapi, bool use_system_rom)
		: _env(env), _dispatch(dispatch), _enable_atapi(support_atapi)
		{
			/* search for devices */
			_resources.with_platform([&](auto &platform) {
				_resources.with_hba([&](auto &hba) {
					unsigned port_count = _scan_ports(env.rm(), platform, hba);

					if (port_count != hba.port_count())
						log("controller port count differs from detected ports (CAP.NP=",
						    Hex(hba.cap_np_value()), ",PI=", Hex(hba.pi_value()), ")");
				});
			});

			if (use_system_rom) {
				_system_rom.construct(_env, "system");
				_system_rom->sigh(_system_rom_sigh);
			}
		}

		/**
		 * Forward IRQs to ports/block sessions
		 */
		void handle_irq()
		{
			_resources.with_hba([&](auto &hba) {
				hba.handle_irq([&] (unsigned port) {
					if (_ports[port].constructed())
						_ports[port]->handle_irq();

					/* handle (pending) requests */
					_dispatch.session(port);
				}, [&]() { error("hba handle_irq failed"); });
			});

			device_release_if_stopped_and_idle();
		}

		void device_release_if_stopped_and_idle()
		{
			if (!_schedule_stop)
				return;

			/* check for outstanding requests */
			bool pending = false;

			for_each_port([&](auto const &port, auto, auto) {
				if (port.protocol.pending_requests())
					pending = true;
			});

			/* avoid disabling device if we have outstanding requests */
			if (pending)
				return;

			_resources.release_device();
		}

		Port &port(Session_label const &label, Session_policy const &policy)
		{
			/* try read device port number attribute */
			long device = policy.attribute_value("device", -1L);

			/* try read device model and serial number attributes */
			auto const model  = policy.attribute_value("model",  String<64>());
			auto const serial = policy.attribute_value("serial", String<64>());

			/* check for model/device */
			if (model != "" && serial != "") {
				for (long index = 0; index < MAX_PORTS; index++) {
					if (!_ata[index].constructed()) continue;

					Ata::Protocol &protocol = *_ata[index];
					if (*protocol.model == model.string() && *protocol.serial == serial.string())
						return *_ports[index];
				}
				warning("No device with model ", model, " and serial ", serial, " found for \"", label, "\"");
			}

			/* check for device number */
			if (device >= 0 && device < MAX_PORTS && _ports[device].constructed())
				return *_ports[device];

			warning("No device found on port ", device, " for \"", label, "\"");
			throw Service_denied();
		}

		template <typename FN> void for_each_port(FN const &fn)
		{
			for (unsigned index = 0; index < MAX_PORTS; index++) {
				if (!_ports[index].constructed()) continue;
				fn(*_ports[index], index, !_ata[index].constructed());
			}
		}

		void report_ports(Reporter &reporter)
		{
			Reporter::Xml_generator xml(reporter, [&] () {

				auto report = [&](Port const &port, unsigned index, bool atapi) {

					Block::Session::Info info = port.info();
					xml.node("port", [&] () {
						xml.attribute("num", index);
						xml.attribute("type", atapi ? "ATAPI" : "ATA");
						xml.attribute("block_count", info.block_count);
						xml.attribute("block_size", info.block_size);
						if (!atapi) {
							xml.attribute("model", _ata[index]->model->cstring());
							xml.attribute("serial", _ata[index]->serial->cstring());
						}
					});
				};

				for_each_port(report);
			});
		}
};


struct Ahci::Block_session_handler : Interface
{
	Env                 &env;
	Port                &port;
	Dataspace_capability ds;

	Signal_handler<Block_session_handler> request_handler
	  { env.ep(), *this, &Block_session_handler::handle};

	Block_session_handler(Env &env, Port &port, size_t buffer_size)
	: env(env), port(port), ds(port.alloc_buffer(buffer_size))
	{ }

	~Block_session_handler()
	{
		port.free_buffer();
	}

	virtual void handle_requests() = 0;

	void handle()
	{
		handle_requests();
	}
};

struct Ahci::Block_session_component : Rpc_object<Block::Session>,
                                       Block_session_handler,
                                       Block::Request_stream
{
	Block_session_component(Env &env, Port &port, size_t buffer_size)
	:
	  Block_session_handler(env, port, buffer_size),
	  Request_stream(env.rm(), ds, env.ep(), request_handler, port.info())
	{
		env.ep().manage(*this);
	}

	~Block_session_component()
	{
		env.ep().dissolve(*this);
	}

	Info info() const override { return Request_stream::info(); }

	Capability<Tx> tx_cap() override { return Request_stream::tx_cap(); }

	void handle_requests() override
	{
		while (true) {

			bool progress = false;

			/*
			 * Acknowledge any pending packets before sending new request to the
			 * controller.
			 */
			try_acknowledge([&](Ack &ack) {
				port.for_one_completed_request([&] (Block::Request request) {
					progress = true;
					ack.submit(request);
				});
			});

			/* all completed packets are handled, but no further processing */
			if (port.stop_processing)
				break;

			with_requests([&] (Block::Request request) {

				Response response = Response::RETRY;

				/* ignored operations */
				if (request.operation.type == Block::Operation::Type::TRIM ||
				    request.operation.type == Block::Operation::Type::INVALID) {
					request.success = true;
					progress = true;
					return Response::REJECTED;
				}

				if ((response = port.submit(request)) != Response::RETRY)
					progress = true;

				return response;
			});

			if (progress == false) break;
		}

		/* poke */
		wakeup_client_if_needed();
	}
};


struct Ahci::Main : Rpc_object<Typed_root<Block::Session>>, Dispatch
{
	Env                                  & env;
	Attached_rom_dataspace                 config   { env, "config" };
	Constructible<Ahci::Driver>            driver   { };
	Constructible<Reporter>                reporter { };
	Constructible<Block_session_component> block_session[Driver::MAX_PORTS];

	Main(Env &env) : env(env)
	{
		log("--- Starting AHCI driver ---");
		bool support_atapi  = config.xml().attribute_value("atapi", false);
		bool use_system_rom = config.xml().attribute_value("system", false);
		try {
			driver.construct(env, *this, support_atapi, use_system_rom);
			report_ports();
		} catch (Ahci::Missing_controller) {
			error("no AHCI controller found");
			env.parent().exit(~0);
		} catch (Service_denied) {
			error("hardware access denied");
			env.parent().exit(~0);
		}

		env.parent().announce(env.ep().manage(*this));
	}

	void session(unsigned index) override
	{
		if (index > Driver::MAX_PORTS || !block_session[index].constructed()) return;
		block_session[index]->handle_requests();
	}

	Session_capability session(Root::Session_args const &args,
	                            Affinity const &) override
	{
		Session_label const label = label_from_args(args.string());
		Session_policy const policy(label, config.xml());

		Ram_quota const ram_quota = ram_quota_from_args(args.string());
		size_t const tx_buf_size =
			Arg_string::find_arg(args.string(), "tx_buf_size").ulong_value(0);

		if (!tx_buf_size)
			throw Service_denied();

		if (tx_buf_size > ram_quota.value) {
			error("insufficient 'ram_quota' from '", label, "',"
			      " got ", ram_quota, ", need ", tx_buf_size);
			throw Insufficient_ram_quota();
		}

		Port &port = driver->port(label, policy);

		if (block_session[port.index].constructed()) {
			error("Device with number=", port.index, " is already in use");
			throw Service_denied();
		}

		port.writeable(policy.attribute_value("writeable", false));
		block_session[port.index].construct(env, port, tx_buf_size);
		return block_session[port.index]->cap();
	}

	void upgrade(Session_capability, Root::Upgrade_args const&) override { }

	void close(Session_capability cap) override
	{
		for (int index = 0; index < Driver::MAX_PORTS; index++) {
			if (!block_session[index].constructed() || !(cap == block_session[index]->cap()))
				continue;

			block_session[index].destruct();
		}
	}

	void report_ports()
	{
		try {
			Xml_node report = config.xml().sub_node("report");
			if (report.attribute_value("ports", false)) {
				reporter.construct(env, "ports");
				reporter->enabled(true);
				driver->report_ports(*reporter);
			}
		} catch (Xml_node::Nonexistent_sub_node) { }
	}
};

void Component::construct(Genode::Env &env) { static Ahci::Main server(env); }
