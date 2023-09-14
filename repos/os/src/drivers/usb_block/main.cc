/*
 * \brief  Usb session to Block session translator
 * \author Josef Soentgen
 * \author Sebastian Sumpf
 * \author Stefan Kalkowski
 * \date   2016-02-08
 */

/*
 * Copyright (C) 2016-2024 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Genode includes */
#include <base/allocator_avl.h>
#include <base/attached_ram_dataspace.h>
#include <base/attached_rom_dataspace.h>
#include <base/component.h>
#include <base/log.h>
#include <base/heap.h>
#include <base/sleep.h>
#include <block/request_stream.h>
#include <os/reporter.h>
#include <timer_session/connection.h>
#include <usb_session/device.h>

/* local includes */
#include <cbw_csw.h>


namespace Usb {
	using namespace Block;
	using Response = Block::Request_stream::Response;

	struct No_device {};
	struct Io_error  {};
	class  Block_driver;
	struct Block_session_component;
	struct Main;
}


/*********************************************************
 ** USB Mass Storage (BBB) Block::Driver implementation **
 *********************************************************/

class Usb::Block_driver
{
	private:

		enum State {
			ALT_SETTING,
			RESET,
			INQUIRY,
			CHECK_MEDIUM,
			READ_CAPACITY,
			REPORT,
			READY
		} _state { ALT_SETTING };

		enum Usb_request : uint8_t {
			BULK_GET_MAX_LUN = 0xfe,
			BULK_RESET       = 0xff
		};

		struct Reset : Device::Urb
		{
			using P  = Device::Packet_descriptor;
			using Rt = P::Request_type;

			Reset(Device &dev, Interface &iface)
			:
				Device::Urb(dev, BULK_RESET,
				            Rt::value(P::Recipient::IFACE, P::Type::CLASS,
				                      P::Direction::IN),
				            iface.index().number, 0) {}
		};

		Env        &_env;
		Entrypoint &_ep;
		Allocator  &_alloc;

		Connection _session { _env };
		Device     _device  { _session, _alloc, _env.rm() };

		/*
		 * We have to decide which contructor to use
		 * for the Usb::Interface dependent on configuration values.
		 * Therefore, we use a pattern of constructible + reference here
		 */
		Constructible<Interface> _iface_memory_holder {};
		Interface               &_interface;

		Endpoint _ep_in  { _interface, Endpoint::Direction::IN,
		                   Endpoint::Type::BULK };
		Endpoint _ep_out { _interface, Endpoint::Direction::OUT,
		                   Endpoint::Type::BULK };

		Interface::Alt_setting _alt_setting { _device, _interface };
		Constructible<Reset> _reset {};

		using String = Genode::String<64>;

		Block::sector_t _block_count  { 0 };
		uint32_t        _block_size   { 0 };

		String _vendor  {};
		String _product {};

		Reporter _reporter { _env, "devices" };

		bool     _writeable    { false };
		bool     _force_cmd_16 { false };
		uint8_t  _active_lun   { 0     };
		uint32_t _active_tag   { 0     };
		bool     _reset_device { false };
		bool     _verbose_scsi { false };

		uint32_t _new_tag() { return ++_active_tag % 0xffffffu; }

		enum Tag : uint32_t {
			INQ_TAG = 0x01,
			RDY_TAG = 0x02,
			CAP_TAG = 0x04,
			REQ_TAG = 0x08,
			SS_TAG  = 0x10
		};

		void _report_device()
		{
			if (!_reporter.enabled())
				return;

			try {
				Reporter::Xml_generator xml(_reporter, [&] () {
					xml.node("device", [&] () {
						xml.attribute("vendor",      _vendor);
						xml.attribute("product",     _product);
						xml.attribute("block_count", _block_count);
						xml.attribute("block_size",  _block_size);
						xml.attribute("writeable",   _writeable);
					});
				});
			} catch (...) { warning("Could not report block device"); }
		}

		Interface& _construct_interface(Xml_node & cfg)
		{
			using Type  = Interface::Type;
			using Index = Interface::Index;

			enum { INVALID = 256 };
			enum {
				ICLASS_MASS_STORAGE = 8,
				ISUBCLASS_SCSI      = 6,
				IPROTO_BULK_ONLY    = 80
			};

			static constexpr size_t PACKET_STREAM_BUF_SIZE = 2 * (1UL << 20);

			/* if interface is set by config, we assume user knows what to do */
			uint16_t _iface = cfg.attribute_value<uint16_t>("interface",
			                                                INVALID);
			uint8_t  _alt   = cfg.attribute_value<uint8_t>("alt_setting", 0);

			if (_iface < INVALID)
				_iface_memory_holder.construct(_device,
				                               Index{ (uint8_t)_iface, _alt },
				                               PACKET_STREAM_BUF_SIZE);
			else
				_iface_memory_holder.construct(_device,
				                               Type{ ICLASS_MASS_STORAGE,
				                                     ISUBCLASS_SCSI,
				                                     IPROTO_BULK_ONLY },
				                               PACKET_STREAM_BUF_SIZE);
			return *_iface_memory_holder;
		}

		void _no_write(Byte_range_ptr&) { }
		void _no_read(Const_byte_range_ptr const&) { }

		void _inquiry(Byte_range_ptr &dst) {
			Inquiry inq(dst, INQ_TAG, _active_lun, _verbose_scsi); }

		void _inquiry_result(Const_byte_range_ptr const &src)
		{
			using Response = Scsi::Inquiry_response;

			Response r(src, _verbose_scsi);
			char v[Response::Vid::ITEMS+1];
			char p[Response::Pid::ITEMS+1];

			if (!r.sbc())
				warning("Device does not use SCSI "
				        "Block Commands and may not work");

			r.get_id<Response::Vid>(v, sizeof(v));
			r.get_id<Response::Pid>(p, sizeof(p));
			_vendor  = String(v);
			_product = String(p);
		}

		void _unit_ready(Byte_range_ptr &dst) {
			Test_unit_ready r(dst, RDY_TAG, _active_lun, _verbose_scsi); }

		void _sense(Byte_range_ptr &dst) {
			Request_sense r(dst, REQ_TAG, _active_lun, _verbose_scsi); }

		void _sense_result(Const_byte_range_ptr const &src)
		{
			using namespace Scsi;

			Request_sense_response r(src, _verbose_scsi);

			uint8_t const asc = r.read<Request_sense_response::Asc>();
			uint8_t const asq = r.read<Request_sense_response::Asq>();

			enum { MEDIUM_NOT_PRESENT         = 0x3a,
			       NOT_READY_TO_READY_CHANGE  = 0x28,
			       POWER_ON_OR_RESET_OCCURRED = 0x29,
			       LOGICAL_UNIT_NOT_READY     = 0x04 };

			switch (asc) {
			case MEDIUM_NOT_PRESENT:
				warning("Medium not present!");
				_medium_state.state = Medium_state::WAIT;
				return;
			case NOT_READY_TO_READY_CHANGE:  /* asq == 0x00 */
			case POWER_ON_OR_RESET_OCCURRED: /* asq == 0x00 */
				warning("Medium not ready yet - try again");
				_medium_state.state = Medium_state::WAIT;
				return;
			case LOGICAL_UNIT_NOT_READY:
				/* initializing command required */
				if (asq == 2) {
					_medium_state.state = Medium_state::START_STOP;
					return;
				}
				/* initializing in progress */
				if (asq == 1) {
					_medium_state.state = Medium_state::WAIT;
					return;
				}
				[[fallthrough]];
			default:
				error("Request_sense_response asc: ",
				      Hex(asc), " asq: ", Hex(asq));
			};
		}

		void _start_stop(Byte_range_ptr &dst) {
			Start_stop r(dst, SS_TAG, _active_lun, _verbose_scsi); }

		void _capacity(Byte_range_ptr &dst)
		{
			if (_force_cmd_16)
				Read_capacity_16 r(dst, CAP_TAG, _active_lun, _verbose_scsi);
			else
				Read_capacity_10 r(dst, CAP_TAG, _active_lun, _verbose_scsi);
		}

		void _capacity_result(Const_byte_range_ptr const &src)
		{
			if (_force_cmd_16) {
				Scsi::Capacity_response_16 r(src, _verbose_scsi);
				_block_count = r.last_block() + 1;
				_block_size  = r.block_size();
			} else {
				Scsi::Capacity_response_10 r(src, _verbose_scsi);
				if (r.last_block() < ~(uint32_t)0U) {
					_block_count = r.last_block() + 1;
					_block_size  = r.block_size();
				}
			}
		}

		void _block_write(Byte_range_ptr &dst)
		{
			if (_block_cmd.constructed())
				memcpy(dst.start, _block_cmd->address, dst.num_bytes);
		}

		void _block_read(Const_byte_range_ptr const &src)
		{
			if (_block_cmd.constructed())
				memcpy(_block_cmd->address, src.start, src.num_bytes);
		}

		void _block_command(Byte_range_ptr &dst)
		{
			if (!_block_cmd.constructed())
				return;

			Operation const &op = _block_cmd->block_request.operation;
			uint64_t  const  n  = (uint32_t) op.block_number;
			uint16_t  const  c  = (uint16_t) op.count;

			if (op.type == Operation::Type::READ) {
				if (_force_cmd_16)
					Read_16 r(dst, _active_tag, _active_lun,
					          n, c, _block_size, _verbose_scsi);
				else
					Read_10 r(dst, _active_tag, _active_lun,
					          (uint32_t)n, c, _block_size, _verbose_scsi);
			} else {
				if (_force_cmd_16)
					Write_16 w(dst, _active_tag, _active_lun,
					           n, c, _block_size, _verbose_scsi);
				else
					Write_10 w(dst, _active_tag, _active_lun,
					           (uint32_t)n, c, _block_size, _verbose_scsi);
			}
		}

		struct Scsi_command
		{
			using Desc = Interface::Packet_descriptor;
			using Urb  = Interface::Urb;

			Block_driver &drv;

			void (Block_driver::*cmd)   (Byte_range_ptr&);
			void (Block_driver::*read)  (Const_byte_range_ptr const&);
			void (Block_driver::*write) (Byte_range_ptr&);

			uint32_t tag;
			size_t   size;
			bool     in;

			enum Cmdstate {
				CBW,
				DATA,
				CSW,
				DONE,
				PROTOCOL_ERROR
			} state { CBW };

			Constructible<Urb> urb { };

			Scsi_command(Block_driver & drv,
			             void (Block_driver::*cmd)(Byte_range_ptr&),
			             void (Block_driver::*read)(Const_byte_range_ptr const&),
			             void (Block_driver::*write)(Byte_range_ptr&),
			             uint32_t tag,
			             size_t   size,
			             bool     in = true)
			:
				drv(drv), cmd(cmd), read(read),
				write(write), tag(tag), size(size), in(in) {}

			void produce_out_content(Byte_range_ptr &dst)
			{
				if (state == CBW) {
					(drv.*cmd)(dst);
					return;
				}
				(drv.*write)(dst);
			}

			void consume_in_result(Const_byte_range_ptr const &src)
			{
				if (state == DATA) {
					(drv.*read)(src);
					return;
				}

				Csw csw(src);
				if (csw.sig() != Csw::SIG) {
					error("CSW signature does not match: ",
					      Hex(csw.sig(), Hex::PREFIX, Hex::PAD));
					state = PROTOCOL_ERROR;
					return;
				}

				if (!((csw.tag() & tag) && csw.sts() == Csw::PASSED)) {
					warning("SCSI command failure, expected tag=", tag,
					        ", got tag=", csw.tag(), " status=", csw.sts());
					state = PROTOCOL_ERROR;
					return;
				}
			}

			void completed(Desc::Return_value ret)
			{
				switch (ret) {
				case Desc::OK:        break;
				case Desc::NO_DEVICE: throw No_device();
				default:              throw Io_error();
				};

				switch (state) {
				case CBW:  state = (size) ? DATA : CSW; break;
				case DATA: state = CSW;  break;
				case CSW:  state = DONE; break;
				default: ;
				};
				urb.destruct();
			}

			bool process(State next_state)
			{
				if (state == PROTOCOL_ERROR)
					return false;

				Cmdstate s = state;

				if (!urb.constructed())
					switch (state) {
					case CBW:  urb.construct(drv._interface, drv._ep_out,
					                         Desc::BULK, Cbw::LENGTH);  break;
					case DATA: urb.construct(drv._interface,
					                         in ? drv._ep_in : drv._ep_out,
					                         Desc::BULK, size); break;
					case CSW:  urb.construct(drv._interface, drv._ep_in,
					                         Desc::BULK, Csw::LENGTH);  break;
					default: break;
					};

				drv._interface.update_urbs<Urb>(
					[this] (Urb &, Byte_range_ptr &dst) {
						produce_out_content(dst); },
					[this] (Urb &, Const_byte_range_ptr &src) {
						consume_in_result(src); },
					[this] (Urb&,
					        Interface::Packet_descriptor::Return_value r) {
						completed(r); });

				if (state == DONE) drv._state = next_state;
				return s != state;
			}

			bool done()    { return state == DONE; }
			bool failure() { return state == PROTOCOL_ERROR; }
		};

		struct Medium_state
		{
			enum State {
				TEST, SENSE, START_STOP, WAIT, READY
			} state { TEST };

			Constructible<Scsi_command> cmd {};

			Block_driver &drv;

			Timer::Connection timer { drv._env };

			Medium_state(Block_driver &drv) : drv(drv) {}

			bool process(Block_driver::State next)
			{
				State s = state;

				if (!cmd.constructed())
					switch (state) {
					case TEST:
						cmd.construct(drv,
						              &Block_driver::_unit_ready,
						              &Block_driver::_no_read,
						              &Block_driver::_no_write, RDY_TAG, 0);
						break;
					case SENSE:
						cmd.construct(drv,
						              &Block_driver::_sense,
						              &Block_driver::_sense_result,
						              &Block_driver::_no_write, REQ_TAG,
						              Scsi::Request_sense_response::LENGTH);
						break;
					case START_STOP:
						cmd.construct(drv,
						              &Block_driver::_start_stop,
						              &Block_driver::_no_read,
						              &Block_driver::_no_write, SS_TAG, 0);
						break;
					case WAIT:
						timer.msleep(1000);
						state = TEST;
						return true;
					default: return false;
					};

				bool ret = cmd->process(CHECK_MEDIUM);

				if (cmd->done() || cmd->failure()) {
					switch (state) {
					case TEST:
						state = (cmd->done()) ? READY : SENSE;
						if (cmd->done()) drv._state = next;
						break;
					case START_STOP:
						if (cmd->done()) state = WAIT;
						break;
					default: ;
					};
					cmd.destruct();
				}

				return ret || s != state;
			}

			bool done() { return state == READY; }
		};

		struct Block_command
		{
			Scsi_command cmd;
			Request      block_request;
			void * const address;
			size_t       size;

			Block_command(Block_driver &drv, Request request,
			              void *addr, size_t sz, uint32_t tag)
			:
				cmd(drv,
				    &Block_driver::_block_command,
				    &Block_driver::_block_read,
				    &Block_driver::_block_write,
				    tag, sz,
				    request.operation.type == Block::Operation::Type::READ),
				block_request(request), address(addr), size(sz)
			{
				if (!address && !size) cmd.state = Scsi_command::DONE;
			}

			Block_command(const Block_command&)  = delete;
			void operator=(const Block_command&) = delete;

			bool process(State next_state)
			{
				bool ret = cmd.process(next_state);
				if (cmd.done()) block_request.success = true;
				return ret;
			}
		};

		Scsi_command _inquiry_cmd { *this,
		                            &Block_driver::_inquiry,
		                            &Block_driver::_inquiry_result,
		                            &Block_driver::_no_write, INQ_TAG,
		                            Scsi::Inquiry_response::LENGTH };

		Medium_state _medium_state { *this };

		Scsi_command _capacity_10_cmd { *this,
		                                &Block_driver::_capacity,
		                                &Block_driver::_capacity_result,
		                                &Block_driver::_no_write, CAP_TAG,
		                                Scsi::Capacity_response_10::LENGTH };

		Scsi_command _capacity_16_cmd { *this,
		                                &Block_driver::_capacity,
		                                &Block_driver::_capacity_result,
		                                &Block_driver::_no_write, CAP_TAG,
		                                Scsi::Capacity_response_16::LENGTH };

		Constructible<Block_command> _block_cmd { };

		bool _capacity(State next_state)
		{
			if (!_force_cmd_16) {
				bool ret = _capacity_10_cmd.process(next_state);
				if (!_capacity_10_cmd.done() || _block_count)
					return ret;
				_force_cmd_16 = true;
				_state = READ_CAPACITY;
			}
			return _capacity_16_cmd.process(next_state);
		}

	public:

		void completed(Device::Packet_descriptor::Return_value ret)
		{
			using Desc = Device::Packet_descriptor;

			switch (ret) {
			case Desc::OK:        break;
			case Desc::NO_DEVICE: throw No_device();
			default:              throw Io_error();
			};

			switch (_state) {
			case ALT_SETTING:
				if (_reset_device) _reset.construct(_device, _interface);
				_state = _reset_device ? RESET : INQUIRY;
				return;
			case RESET:
				_reset.destruct();
				_state = INQUIRY;
				return;
			default:
				warning("Control URB received after initialization");
			};
		}

		void apply_config(Xml_node node)
		{
			_writeable    = node.attribute_value("writeable",    false);
			_active_lun   = node.attribute_value<uint8_t>("lun",     0);
			_reset_device = node.attribute_value("reset_device", false);
			_verbose_scsi = node.attribute_value("verbose_scsi", false);

			_reporter.enabled(node.attribute_value("report", false));
		}

		bool handle_io()
		{
			using Urb   = Device::Urb;
			using Value = Device::Packet_descriptor::Return_value;

			auto out = [] (Urb&, Byte_range_ptr&) { };
			auto in  = [] (Urb&, Const_byte_range_ptr&) { };
			auto cpl = [this] (Urb&, Value r) { completed(r); };

			switch (_state) {
			case ALT_SETTING: [[fallthrough]];
			case RESET:         return _device.update_urbs<Urb>(out, in, cpl);
			case INQUIRY:       return _inquiry_cmd.process(CHECK_MEDIUM);
			case CHECK_MEDIUM:  return _medium_state.process(READ_CAPACITY);
			case READ_CAPACITY: return _capacity(REPORT);
			case REPORT: _report_device(); _state = READY; [[fallthrough]];
			case READY:
				if (_block_cmd.constructed())
					return _block_cmd->process(READY);
			};

			return false;
		}

		bool device_ready() { return _state == READY; }

		Block_driver(Env &env, Allocator &alloc, Signal_context_capability sigh,
		             Xml_node config)
		:
			_env(env), _ep(env.ep()), _alloc(alloc),
			_interface(_construct_interface(config))
		{
			_device.sigh(sigh);
			_interface.sigh(sigh);

			apply_config(config);
			handle_io();
		}


		/*******************************************
		 ** interface to Block_session_component  **
		 *******************************************/

		Block::Session::Info info() const
		{
			return { .block_size  = _block_size,
				.block_count = _block_count,
				.align_log2  = log2(_block_size),
				.writeable   = _writeable };
		}

		Response submit(Request                 const &block_request,
		                Request_stream::Payload const &payload)
		{
			if (_state != READY)
				return Response::REJECTED;

			using Type = Operation::Type;

			/*
			 * Check if there is already a request pending and wait
			 * until it has finished. We do this check here to implement
			 * 'SYNC' as barrier that waits for out-standing requests.
			 */
			if (_block_cmd.constructed())
				return Response::RETRY;

			Operation const &op = block_request.operation;

			/* read only */
			if (_writeable == false && op.type == Type::WRITE)
				return Response::REJECTED;

			/* range check */
			block_number_t const last = op.block_number + op.count;
			if (last > _block_count)
				return Response::REJECTED;

			/* we only support 32-bit block numbers in 10-Cmd mode */
			if (!_force_cmd_16 && last >= ~0U)
				return Response::REJECTED;

			void * addr = nullptr;
			size_t size = 0;
			payload.with_content(block_request, [&](void *a, size_t sz) {
				addr = a; size = sz; });

			_block_cmd.construct(*this, block_request, addr, size, _new_tag());

			/* operations currently handled as successful NOP */
			if (_block_cmd.constructed() &&
			    (block_request.operation.type == Type::TRIM ||
			     block_request.operation.type == Type::SYNC)) {
				_block_cmd->block_request.success = true;
				return Response::ACCEPTED;
			}

			return Response::ACCEPTED;
		}

		template <typename FUNC>
		void with_completed(FUNC const &fn)
		{
			if (_block_cmd.constructed() &&
			    _block_cmd->block_request.success) {
				fn(_block_cmd->block_request);
				_block_cmd.destruct();
			}
		}
};


struct Usb::Block_session_component : Rpc_object<Block::Session>,
                                      Block::Request_stream
{
	Env &_env;

	Block_session_component(Env &env, Dataspace_capability ds,
	                        Signal_context_capability sigh,
	                        Block::Session::Info info)
	:
		Request_stream(env.rm(), ds, env.ep(), sigh, info),
		_env(env)
	{
		_env.ep().manage(*this);
	}

	~Block_session_component() { _env.ep().dissolve(*this); }

	Info info() const override { return Request_stream::info(); }

	Capability<Tx> tx_cap() override { return Request_stream::tx_cap(); }
};



struct Usb::Main : Rpc_object<Typed_root<Block::Session>>
{
	Env &env;
	Heap heap { env.ram(), env.rm() };

	Attached_rom_dataspace config { env, "config" };

	Constructible<Attached_ram_dataspace>  block_ds { }; 
	Constructible<Block_session_component> block_session { };

	Signal_handler<Main> config_handler { env.ep(), *this,
	                                      &Main::update_config };
	Signal_handler<Main> io_handler     { env.ep(), *this, &Main::handle};

	Block_driver driver { env, heap, io_handler, config.xml() };

	enum State { INIT, ANNOUNCED } state { INIT };

	void update_config()
	{
		config.update();
		driver.apply_config(config.xml());
	}

	void handle()
	{
		try {
			bool progress = true;

			while (progress) {
				while (driver.handle_io()) ;

				if (!driver.device_ready())
					return;

				if (state == INIT) {
					env.parent().announce(env.ep().manage(*this));
					state = ANNOUNCED;
				}

				if (!block_session.constructed())
					return;

				progress = false;

				/* ack and release possibly pending packet */
				block_session->try_acknowledge([&] (Block_session_component::Ack &ack) {
					driver.with_completed([&] (Block::Request const &request) {
						ack.submit(request);
						progress = true;
					});
				});

				block_session->with_requests([&] (Request const request) {

					Response response = Response::RETRY;

					block_session->with_payload([&] (Request_stream::Payload const &payload) {
						response = driver.submit(request, payload);
					});
					if (response != Response::RETRY)
						progress = true;

					return response;
				});
			}

			block_session->wakeup_client_if_needed();
		} catch (Io_error&) {
			error("An unrecoverable USB error occured, will halt!");
			sleep_forever();
		} catch (No_device&) {
			warning("The device has vanished, will halt.");
			sleep_forever();
		}
	}


	/*****************************
	 ** Block session interface **
	 *****************************/

	Genode::Session_capability session(Root::Session_args const &args,
	                                   Affinity const &) override
	{
		if (block_session.constructed()) {
			error("device is already in use");
			throw Service_denied();
		}

		size_t const ds_size =
			Arg_string::find_arg(args.string(), "tx_buf_size").ulong_value(0);

		Ram_quota const ram_quota = ram_quota_from_args(args.string());

		if (ds_size >= ram_quota.value) {
			warning("communication buffer size exceeds session quota");
			throw Insufficient_ram_quota();
		}

		block_ds.construct(env.ram(), env.rm(), ds_size);
		block_session.construct(env, block_ds->cap(), io_handler, driver.info());

		return block_session->cap();
	}

	void upgrade(Genode::Session_capability, Root::Upgrade_args const&) override { }

	void close(Genode::Session_capability cap) override
	{
		if (!block_session.constructed() || !(block_session->cap() == cap))
			return;

		block_session.destruct();
		block_ds.destruct();
	}

	Main(Env &env) : env(env) { config.sigh(config_handler); }
};


void Component::construct(Genode::Env &env) { static Usb::Main main(env); }
