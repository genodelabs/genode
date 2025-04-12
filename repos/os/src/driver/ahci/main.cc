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
#include <base/heap.h>
#include <base/log.h>
#include <block/request_stream.h>
#include <block/session_map.h>
#include <os/reporter.h>
#include <os/session_policy.h>
#include <root/root.h>
#include <timer_session/connection.h>
#include <util/bit_array.h>
#include <util/xml_node.h>

/* local includes */
#include <ahci.h>
#include <ata_protocol.h>
#include <atapi_protocol.h>


namespace Ahci {
	using namespace Block;

	struct Dispatch;
	class  Driver;
	struct Main;
	struct Port_dispatcher;
	struct Block_session_component;
	using Session_space = Id_space<Block_session_component>;
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

		Env                    &_env;
		Dispatch               &_dispatch;
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

		unsigned _scan_ports(Env::Local_rm &rm, Platform::Connection &plat, Hba &hba)
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

			auto state = _system_rom->node().attribute_value("state",
			                                                 String<32>(""));

			bool const resume_driver =  _schedule_stop && state == "";
			bool const stop_driver   = !_schedule_stop && state != "";

			if (stop_driver) {
				_schedule_stop = true;

				for_each_port([&](auto &port, auto, auto) {
					port.stop_processing = true;
				});

				device_release_if_stopped_and_idle();

				return;
			}

			if (resume_driver) {
				_schedule_stop = false;

				_resources.acquire_device();

				/* re-start request handling of client sessions */
				for_each_port([&](auto &port, auto const index, auto) {
					try {
						port.reinit();
						port.stop_processing = false;
						_dispatch.session(index);
					} catch (...) {
						error("port ", index, " failed to be resumed");
					}
				});

				log("driver resumed");

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

			log("driver halted");

			_resources.release_device();
		}

		Port &port(Session_label const &label, Node const &policy)
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
			Reporter::Result const result = reporter.generate([&] (Generator &g) {

				auto report = [&](Port const &port, unsigned index, bool atapi) {

					Block::Session::Info info = port.info();
					g.node("port", [&] () {
						g.attribute("num", index);
						g.attribute("type", atapi ? "ATAPI" : "ATA");
						g.attribute("block_count", info.block_count);
						g.attribute("block_size", info.block_size);
						if (!atapi) {
							g.attribute("model", _ata[index]->model->cstring());
							g.attribute("serial", _ata[index]->serial->cstring());
						}
					});
				};

				for_each_port(report);
			});

			if (result == Buffer_error::EXCEEDED)
				warning("report exceeds maximum size");
		}
};


struct Ahci::Block_session_component : Rpc_object<Block::Session>
{
	Env  &_env;
	Port &_port;

	Session_space::Element const _elem;

	Dataspace_capability  _dma_cap;
	Block::Request_stream _request_stream;

	Block_session_component(Session_space             &space,
	                        uint16_t                   session_id_value,
	                        Env                       &env,
	                        Port                      &port,
	                        Signal_context_capability  sigh,
	                        Block::Constrained_view    view,
	                        size_t                     buffer_size)
	:
		_env(env),
		_port(port),
		_elem(*this, space, Session_space::Id { .value = session_id_value }),
		_dma_cap(_port.alloc_buffer(_elem.id().value, buffer_size)),
		_request_stream(_env.rm(), _dma_cap, _env.ep(), sigh, _port.info(), view)
	{
		_env.ep().manage(*this);
	}

	~Block_session_component()
	{
		_env.ep().dissolve(*this);
		_port.free_buffer(_elem.id().value, _dma_cap);
	}

	Info info() const override { return _request_stream.info(); }

	Capability<Tx> tx_cap() override { return _request_stream.tx_cap(); }

	Session_space::Id session_id() const { return _elem.id(); }

	void with_request_stream(auto const &fn) {
		fn(_request_stream); }
};


struct Ahci::Port_dispatcher
{
	Env &_env;
	Port &_port;

	Sliced_heap _sliced_heap { _env.ram(), _env.rm() };

	using Session_space = Id_space<Block_session_component>;
	Session_space _sessions { };

	using Session_map = Block::Session_map<>;
	Session_map _session_map { };

	Signal_handler<Port_dispatcher> _request_handler {
		_env.ep(), *this, &Port_dispatcher::_handle};

	void _handle()
	{
		handle_requests();
	}

	Port_dispatcher(Env &env, Port &port)
	: _env  { env }, _port { port } { }

	void with_managed_session(Session_capability cap, auto const &fn) const
	{
		_sessions.for_each<Block_session_component>(
			[&] (Block_session_component &session) {
				if (cap == session.cap())
					fn(session.session_id());
			});
	}

	void close(Session_space::Id session_id)
	{
		_sessions.apply<Block_session_component>(session_id,
			[&] (Block_session_component &session) {
				Session_map::Index const index =
					Session_map::Index::from_id(session_id.value);
				_session_map.free(index);
				destroy(_sliced_heap, &session);
			});
	}

	bool active_sessions() const
	{
		bool active = false;
		_sessions.for_each<Block_session_component>(
			[&] (Block_session_component const &) {
				active = true;
			});
		return active;
	}

	Root::Result new_session(Block::Constrained_view const &view,
	                         size_t                         tx_buf_size)
	{
		Session_map::Index new_session_id { 0u };

		if (!_session_map.alloc().convert<bool>(
			[&] (Session_map::Alloc_ok ok) {
				new_session_id = ok.index;
				return true;
				},
			[&] (Session_map::Alloc_error) {
				return false; }))
			return Session_error::DENIED;

		try {
			Block_session_component *session =
				new (_sliced_heap) Block_session_component(_sessions, new_session_id.value,
				                                           _env, _port, _request_handler,
				                                           view, tx_buf_size);
			return { session->cap() };
		} catch (...) {
			_session_map.free(new_session_id);
			return Session_error::DENIED;
		}
	}

	void handle_requests()
	{
		for (;;) {
			bool progress = false;

			/* ack and release possibly pending packet */
			auto session_ack_fn = [&] (Block_session_component &block_session) {
				auto request_stream_ack_fn = [&] (Request_stream &request_stream) {
					auto ack_fn = [&] (Request_stream::Ack &ack) {
						auto completed_fn = [&] (Block::Request &request) {

							if (!request.operation.valid())
								return;

							ack.submit(request);
							progress = true;
						};
						_port.for_one_completed_request(block_session.session_id().value,
						                                completed_fn);
					};
					request_stream.try_acknowledge(ack_fn);
					request_stream.wakeup_client_if_needed();
				};
				block_session.with_request_stream(request_stream_ack_fn);
			};
			_sessions.for_each<Block_session_component>(session_ack_fn);

			/* all completed packets are handled, but no further processing */
			if (_port.stop_processing)
				break;

			auto index_fn = [&] (Session_map::Index index) {
				Session_space::Id const session_id { .value = index.value };
				auto block_session_submit_fn = [&] (Block_session_component &block_session) {
					auto request_stream_submit_fn = [&] (Request_stream &request_stream) {
						auto request_submit_fn = [&] (Request request) {

							Response response = Response::RETRY;

							/* ignored operations */

							if (request.operation.type == Block::Operation::Type::TRIM
							 || request.operation.type == Block::Operation::Type::INVALID) {
								request.success = true;
								progress = true;
								return Response::REJECTED;
							}

							response = _port.submit(block_session.session_id().value, request);

							if (response != Response::RETRY)
								progress = true;

							return response;
						};
						request_stream.with_requests(request_submit_fn);
					};
					block_session.with_request_stream(request_stream_submit_fn);
				};
				_sessions.apply<Block_session_component>(session_id,
				                                         block_session_submit_fn);
			};
			_session_map.for_each_index(index_fn);

			if (!progress)
				break;
		}
	}
};

struct Ahci::Main : Rpc_object<Typed_root<Block::Session>>, Dispatch
{
	Env                                   &env;
	Attached_rom_dataspace                 config   { env, "config" };
	Constructible<Ahci::Driver>            driver   { };
	Constructible<Reporter>                reporter { };
	Constructible<Port_dispatcher>         port_dispatcher[Driver::MAX_PORTS] { };

	Main(Env &env) : env(env)
	{
		log("--- Starting AHCI driver ---");
		bool support_atapi  = config.node().attribute_value("atapi", false);
		bool use_system_rom = config.node().attribute_value("system", false);
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
		if (index > Driver::MAX_PORTS || !port_dispatcher[index].constructed()) return;
		port_dispatcher[index]->handle_requests();
	}

	Root::Result session(Root::Session_args const &args, Affinity const &) override
	{
		Session_label const label = label_from_args(args.string());

		Ram_quota const ram_quota = ram_quota_from_args(args.string());
		size_t const tx_buf_size =
			Arg_string::find_arg(args.string(), "tx_buf_size").ulong_value(0);

		if (!tx_buf_size)
			return Session_error::DENIED;

		if (tx_buf_size > ram_quota.value) {
			error("insufficient 'ram_quota' from '", label, "',"
			      " got ", ram_quota, ", need ", tx_buf_size);
			return Session_error::INSUFFICIENT_RAM;
		}

		return with_matching_policy(label, config.node(),
			[&] (Node const &policy) -> Root::Result {
				try {
					/*
					 * Will throw Service_denied in case there is no device
					 * attached to the port.
					 */
					Port &port = driver->port(label, policy);

					if (!port_dispatcher[port.index].constructed())
						port_dispatcher[port.index].construct(env, port);

					bool const writeable_policy =
						policy.attribute_value("writeable", false);

					bool const writeable_arg =
						Arg_string::find_arg(args.string(), "writeable")
						                     .bool_value(true);

					Block::Constrained_view const view {
						.offset     = Block::Constrained_view::Offset {
							Arg_string::find_arg(args.string(), "offset")
							                     .ulonglong_value(0) },
						.num_blocks = Block::Constrained_view::Num_blocks {
							Arg_string::find_arg(args.string(), "num_blocks")
							                     .ulonglong_value(0) },
						.writeable  = writeable_policy && writeable_arg
					};

					return port_dispatcher[port.index]->new_session(view, tx_buf_size);
				} catch (...) { return Session_error::DENIED; }
			},
			[&] () -> Root::Result { return Session_error::DENIED; });
	}

	void upgrade(Session_capability, Root::Upgrade_args const&) override { }

	void close(Session_capability cap) override
	{
		for (int index = 0; index < Driver::MAX_PORTS; index++) {
			if (!port_dispatcher[index].constructed())
				continue;

			bool found = false;
			Session_space::Id session_id { .value = 0 };
			port_dispatcher[index]->with_managed_session(cap,
				[&] (Session_space::Id id) {
					session_id = id;
					found = true;
				});

			if (!found)
				continue;

			port_dispatcher[index]->close(session_id);

			if (!port_dispatcher[index]->active_sessions())
				port_dispatcher[index].destruct();
		}
	}

	void report_ports()
	{
		config.node().with_optional_sub_node("report", [&](auto const &report) {
			if (report.attribute_value("ports", false)) {
				reporter.construct(env, "ports");
				reporter->enabled(true);
				driver->report_ports(*reporter);
			}
		});
	}
};

void Component::construct(Genode::Env &env) { static Ahci::Main server(env); }
