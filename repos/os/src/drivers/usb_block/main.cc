/*
 * \brief  Usb session to Block session translator
 * \author Josef Soentgen
 * \author Sebastian Sumpf
 * \date   2016-02-08
 */

/*
 * Copyright (C) 2016-2020 Genode Labs GmbH
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
#include <usb/usb.h>

/* used by cbw_csw.h so declare it here */
static bool verbose_scsi = false;

/* local includes */
#include <cbw_csw.h>


namespace Usb {
	using namespace Genode;
	using namespace Block;
	using Response = Block::Request_stream::Response;

	struct Block_driver;
	struct Block_session_component;
	struct Main;
}


/*********************************************************
 ** USB Mass Storage (BBB) Block::Driver implementation **
 *********************************************************/

struct Usb::Block_driver : Usb::Completion
{
	Env        &env;
	Entrypoint &ep;

	Signal_context_capability announce_sigh;

	/*
	 * Pending block request
	 */
	struct Block_request
	{
		Request        block_request;
		addr_t         address   { 0 };
		size_t         size      { 0 };
		bool           completed { false };

		Block_request(Request const &request, Request_stream::Payload const &payload)
		: block_request(request)
		{
			payload.with_content(request, [&](void *addr, size_t sz) {
				address = addr_t(addr);
				size    = sz;
			});
		}

		bool read() const
		{
			return block_request.operation.type == Operation::Type::READ;
		}

		bool write() const
		{
			return block_request.operation.type == Operation::Type::WRITE;
		}
	};

	Constructible<Block_request> request { };

	bool request_pending() const  { return request.constructed(); }

	bool initialized     = false;
	bool device_plugged  = false;

	/**
	 * Handle stage change signal
	 */
	void handle_state_change()
	{
		if (!usb.plugged()) {
			log("Device unplugged");
			device_plugged = false;
			return;
		}

		if (initialized) {
			error("Device was already initialized");
			return;
		}

		log("Device plugged");

		if (!initialize()) {
			env.parent().exit(-1);
			sleep_forever();
			return;
		}

		/* all is well, announce the device */
		Signal_transmitter(announce_sigh).submit();
	}

	Signal_handler<Block_driver> state_change_dispatcher = {
		ep, *this, &Block_driver::handle_state_change };

	/*
	 * Config ROM
	 */
	Attached_rom_dataspace config { env, "config" };

	/*
	 * Read Usb session label from the configuration
	 */
	template <unsigned SIZE>
	static Genode::String<SIZE> get_label(Xml_node node)
	{
		Genode::String<SIZE> usb_label { "usb_storage" };
		try {
			node.attribute("label").value(usb_label);
			return usb_label.string();
		} catch (...) { }

		return usb_label;
	}

	enum Sizes : size_t { PACKET_STREAM_BUF_SIZE = 2 * (1UL << 20) };

	/*
	 * USB session
	 */
	Allocator_avl         alloc;
	Usb::Connection       usb { env, &alloc,
		get_label<128>(config.xml()).string(), PACKET_STREAM_BUF_SIZE,
		state_change_dispatcher };
	Usb::Device           device;
	Signal_handler<Main> &block_request_handler;

	/*
	 * Reporter
	 */
	Reporter reporter { env, "devices" };
	bool _report_device = false;

	/*
	 * Block session
	 */
	Block::sector_t _block_count { 0 };
	uint32_t        _block_size  { 0 };

	bool _writeable = false;

	bool force_cmd_16 = false;

	uint8_t active_interface = 0;
	uint8_t active_lun       = 0;

	enum { INVALID_ALT_SETTING = 256 };
	uint16_t active_alt_setting = 0;

	uint32_t active_tag = 0;
	uint32_t new_tag() { return ++active_tag % 0xffffffu; }

	enum Tags {
		INQ_TAG = 0x01, RDY_TAG = 0x02, CAP_TAG = 0x04,
		REQ_TAG = 0x08, SS_TAG = 0x10
	};

	uint8_t ep_in = 0;
	uint8_t ep_out = 0;

	bool reset_device = false;

	/*
	 * Completion used while initializing the device
	 */
	struct Init_completion : Usb::Completion
	{
		bool inquiry       = false;
		bool unit_ready    = false;
		bool read_capacity = false;
		bool request_sense = false;

		bool no_medium     = false;
		bool try_again     = false;
		bool start_stop    = false;

		Usb::Device &device;
		uint8_t      interface;

		Block::sector_t block_count = 0;
		uint32_t        block_size  = 0;

		char vendor[Scsi::Inquiry_response::Vid::ITEMS+1];
		char product[Scsi::Inquiry_response::Pid::ITEMS+1];

		Init_completion(Usb::Device &device, uint8_t interface)
		: device(device), interface(interface) { }

		void complete(Packet_descriptor &p) override
		{
			Usb::Interface iface = device.interface(interface);

			if (p.type != Packet_descriptor::BULK) {
				error("Can only handle BULK packets");
				iface.release(p);
				return;
			}

			if (!p.succeded) {
				error("init complete error: packet not succeeded");
				iface.release(p);
				return;
			}

			/* OUT transfer finished */
			if (!p.read_transfer()) {
				iface.release(p);
				return;
			}

			int const actual_size = p.transfer.actual_size;
			Byte_range_ptr data {(char *)iface.content(p), p.size()};

			using namespace Scsi;

			switch (actual_size) {
			case Inquiry_response::LENGTH:
			{
				Inquiry_response r(data);
				if (verbose_scsi) r.dump();

				if (!r.sbc()) {
					warning("Device does not use SCSI Block Commands and may not work");
				}

				r.get_id<Inquiry_response::Vid>(vendor, sizeof(vendor));
				r.get_id<Inquiry_response::Pid>(product, sizeof(product));
				break;
			}
			case Capacity_response_10::LENGTH:
			{
				Capacity_response_10 r(data);
				if (verbose_scsi) r.dump();

				block_count = r.last_block() + 1;
				block_size  = r.block_size();
				break;
			}
			case Capacity_response_16::LENGTH:
			{
				Capacity_response_16 r(data);
				if (verbose_scsi) r.dump();

				block_count = r.last_block() + 1;
				block_size  = r.block_size();
				break;
			}
			case Request_sense_response::LENGTH:
			{
				Request_sense_response r(data);
				if (verbose_scsi) r.dump();

				uint8_t const asc = r.read<Request_sense_response::Asc>();
				uint8_t const asq = r.read<Request_sense_response::Asq>();

				bool error = false;

				enum { MEDIUM_NOT_PRESENT         = 0x3a,
				       NOT_READY_TO_READY_CHANGE  = 0x28,
				       POWER_ON_OR_RESET_OCCURRED = 0x29,
				       LOGICAL_UNIT_NOT_READY     = 0x04 };
				switch (asc) {

				case MEDIUM_NOT_PRESENT:
					Genode::error("Not ready - medium not present");
					no_medium = true;
					break;

				case NOT_READY_TO_READY_CHANGE:  /* asq == 0x00 */
				case POWER_ON_OR_RESET_OCCURRED: /* asq == 0x00 */
					warning("Not ready - try again");
					try_again = true;
					break;

				case LOGICAL_UNIT_NOT_READY:

					if      (asq == 2) start_stop = true; /* initializing command required */
					else if (asq == 1) try_again  = true; /* initializing in progress */
					else error = true;

					break;

				default:
					error = true;
					break;
				}

				if (error)
					Genode::error("Request_sense_response asc: ",
					              Hex(asc, Hex::PREFIX, Hex::PAD),
					              " asq: ", Hex(asq, Hex::PREFIX, Hex::PAD));
				break;
			}
			case Csw::LENGTH:
			{
				Csw csw(data);

				uint32_t const sig = csw.sig();
				if (sig != Csw::SIG) {
					error("CSW signature does not match: ",
					              Hex(sig, Hex::PREFIX, Hex::PAD));
					break;
				}

				uint32_t const   tag  = csw.tag();
				uint32_t const status = csw.sts();
				if (status != Csw::PASSED) {
					error("CSW failed: ", Hex(status, Hex::PREFIX, Hex::PAD),
					              " tag: ", tag);
					break;
				}

				inquiry       |= tag & INQ_TAG;
				unit_ready    |= tag & RDY_TAG;
				read_capacity |= tag & CAP_TAG;
				request_sense |= tag & REQ_TAG;
				break;
			}
			default: break;
			}

			iface.release(p);
		}
	} init { device, active_interface };

	/**
	 * Send CBW
	 */
	void cbw(void *cb, Completion &c, bool block = false)
	{
		enum { CBW_VALID_SIZE = Cbw::LENGTH };
		Usb::Interface     &iface = device.interface(active_interface);
		Usb::Endpoint         &ep = iface.endpoint(ep_out);
		Usb::Packet_descriptor  p = iface.alloc(CBW_VALID_SIZE);
		memcpy(iface.content(p), cb, CBW_VALID_SIZE);
		iface.bulk_transfer(p, ep, block, &c);
	}

	/**
	 * Receive CSW
	 */
	void csw(Completion &c, bool block = false)
	{
		enum { CSW_VALID_SIZE = Csw::LENGTH };
		Usb::Interface     &iface = device.interface(active_interface);
		Usb::Endpoint         &ep = iface.endpoint(ep_in);
		Usb::Packet_descriptor  p = iface.alloc(CSW_VALID_SIZE);
		iface.bulk_transfer(p, ep, block, &c);
	}

	/**
	 * Receive response
	 */
	void resp(size_t size, Completion &c, bool block = false)
	{
		Usb::Interface     &iface = device.interface(active_interface);
		Usb::Endpoint         &ep = iface.endpoint(ep_in);
		Usb::Packet_descriptor  p = iface.alloc(size);
		iface.bulk_transfer(p, ep, block, &c);
	}

	/**
	 * Report block device
	 */
	void report_device(char const *vendor, char const *product,
	                   Block::sector_t count, size_t size)
	{
		try {
			Reporter::Xml_generator xml(reporter, [&] () {
				xml.node("device", [&] () {
					xml.attribute("vendor", vendor);
					xml.attribute("product", product);
					xml.attribute("block_count", count);
					xml.attribute("block_size", size);
					xml.attribute("writeable", _writeable);
				});
			});
		} catch (...) { warning("Could not report block device"); }
	}

	/**
	 * Initialize device
	 *
	 * All USB transfers in this method are done synchronously. First we reset
	 * the device (optional), then we query the max LUN. Afterwards we start
	 * sending CBWs.
	 *
	 * Since it might take some time for the device to get ready to use, we
	 * have to check the SCSI logical unit several times.
	 */
	bool initialize()
	{
		device.update_config();

		Usb::Interface &iface = device.interface(active_interface);
		try { iface.claim(); }
		catch (Usb::Session::Interface_already_claimed) {
			error("Device already claimed");
			return false;
		} catch (Usb::Session::Interface_not_found) {
			error("Interface not found");
			return false;
		}

		enum {
			ICLASS_MASS_STORAGE = 8,
			ISUBCLASS_SCSI      = 6,
			IPROTO_BULK_ONLY    = 80
		};

		/*
		 * Devices following the USB Attached SCSI specification
		 * normally expose the bulk-only transport in interface 0 alt 0
		 * and the UAS endpoints in interface 0 alt 1.
		 *
		 * The default interface and thereby 'iface.current()', however,
		 * might point to the interface 0 alt 1 for such devices.
		 *
		 * In case the alternate setting was not explicitly configured
		 * we look for the first bulk-only setting.
		 */

		if (active_alt_setting == INVALID_ALT_SETTING) {

			/* cap value in case there is no bulk-only */
			active_alt_setting = 0;

			for (uint16_t i = 0; i < iface.alternate_count(); i++) {
				Alternate_interface &aif = iface.alternate_interface(i);
				if (aif.iclass == ICLASS_MASS_STORAGE
				    && aif.isubclass == ISUBCLASS_SCSI
				    && aif.iprotocol == IPROTO_BULK_ONLY) {

					active_alt_setting = i;

					Genode::log("Use probed alternate setting ",
					            active_alt_setting, " for interface ",
					            active_interface);
					break;
				}
			}
		}

		Alternate_interface &aif =
			iface.alternate_interface((uint8_t)active_alt_setting);
		iface.set_alternate_interface(aif);

		Alternate_interface &alt_iface = iface.current();

		if (alt_iface.iclass != ICLASS_MASS_STORAGE
			|| alt_iface.isubclass != ISUBCLASS_SCSI
			|| alt_iface.iprotocol != IPROTO_BULK_ONLY) {
			error("No mass storage SCSI bulk-only device");
			return false;
		}

		for (uint8_t i = 0; i < alt_iface.num_endpoints; i++) {
			Endpoint ep = alt_iface.endpoint(i);
			if (!ep.bulk())
				continue;
			if (ep.address & Usb::ENDPOINT_IN)
				ep_in = i;
			else
				ep_out = i;
		}

		try {

			/*
			 * reset
			 *
			 * This command caused write command errors on a
			 * 'SanDisk Cruzer Force' (0781:557d) USB stick, so it is
			 * omitted by default.
			 */
			if (reset_device) {
				Usb::Packet_descriptor p = iface.alloc(0);
				iface.control_transfer(p, 0x21, 0xff, 0, active_interface, 100);
				if (!p.succeded) {
					error("Could not reset device");
					iface.release(p);
					throw -1;
				}
				iface.release(p);
			}

			/*
			 * Let us do GetMaxLUN and simply ignore the return value because none
			 * of the devices that were tested did infact report another value than 0.
			 */
			Usb::Packet_descriptor p = iface.alloc(1);
			iface.control_transfer(p, 0xa1, 0xfe, 0, active_interface, 100);
			uint8_t max_lun = *(uint8_t*)iface.content(p);
			if (p.succeded && max_lun == 0) { max_lun = 1; }
			iface.release(p);

			/*
			 * Query device
			 */

			char cbw_buffer[Cbw::LENGTH];
			Byte_range_ptr cbw_buf_range {(char *)cbw_buffer, Cbw::LENGTH};

			/*
			 * We should probably execute the SCSI REPORT_LUNS command first
			 * but we will receive LOGICAL UNIT NOT SUPPORTED if we try to
			 * access an invalid unit. The user has to specify the LUN in
			 * the configuration anyway.
			 */

			/* Scsi::Opcode::INQUIRY */
			Inquiry inq(cbw_buf_range, INQ_TAG, active_lun);

			cbw(cbw_buffer, init, true);
			resp(Scsi::Inquiry_response::LENGTH, init, true);
			csw(init, true);

			if (!init.inquiry) {
				warning("Inquiry_cmd failed");
				throw -1;
			}

			/* Scsi::Opcode::TEST_UNIT_READY */
			{
				Timer::Connection timer { env };
				/*
				 * It might take some time for devices to get ready (e.g. the ZTE Open C
				 * takes 3 retries to actually present us a medium and another try to
				 * let us use the medium.
				 */
				enum { MAX_RETRIES = 10 };
				int retries;
				for (retries = 0; retries < MAX_RETRIES; retries++) {
					Test_unit_ready unit_ready(cbw_buf_range, RDY_TAG, active_lun);

					cbw(cbw_buffer, init, true);
					csw(init, true);

					if (!init.unit_ready) {
						Request_sense sense(cbw_buf_range, REQ_TAG, active_lun);

						cbw(cbw_buffer, init, true);
						resp(Scsi::Request_sense_response::LENGTH, init, true);
						csw(init, true);
						if (!init.request_sense) {
							warning("Request_sense failed");
							throw -1;
						}

						if (init.no_medium) {
							/* do nothing for now */
						} else if (init.try_again) {
							init.try_again = false;
						} else if (init.start_stop) {

							init.start_stop = false;
							Start_stop start_stop(cbw_buf_range, SS_TAG, active_lun);

							cbw(cbw_buffer, init, true);
							csw(init, true);

						} else break;
					} else break;

					timer.msleep(1000);
				}
				if (retries == MAX_RETRIES) {
					warning("Test_unit_ready_cmd failed");
					throw -1;
				}
			}

			/*
			 * Some devices (e.g. corsair voyager usb stick (1b1c:1a03)) failed
			 * when the 16-byte command was tried first and when the 10-byte
			 * command was tried afterwards as a fallback.
			 *
			 * For this reason, we use the 10-byte commands by default and the
			 * 16-byte commands only when the capacity of the device requires
			 * it.
			 */

			/* Scsi::Opcode::READ_CAPACITY_10 */
			Read_capacity_10 read_cap(cbw_buf_range, CAP_TAG, active_lun);

			cbw(cbw_buffer, init, true);
			resp(Scsi::Capacity_response_10::LENGTH, init, true);
			csw(init, true);

			if (!init.read_capacity) {
				warning("Read_capacity_cmd failed");
				throw -1;
			}

			/**
			 * The READ_CAPACITY_10 count is 32-bit last() block + 1.
			 * If the maximum value is reached READ_CAPACITY_16 has to be used.
			 */
			if (init.block_count > ~(uint32_t)0U) {

				Read_capacity_16 read_cap(cbw_buf_range, CAP_TAG, active_lun);

				init.read_capacity = false;

				cbw(cbw_buffer, init, true);
				resp(Scsi::Capacity_response_16::LENGTH, init, true);
				csw(init, true);

				if (!init.read_capacity) {
					warning("Read_capacity_cmd failed");
					throw -1;
				}

				force_cmd_16 = true;
			}

			_block_size  = init.block_size;
			_block_count = init.block_count;

			initialized     = true;
			device_plugged  = true;

			char vendor[32];
			char product[32];

			device.manufactorer_string.to_char(vendor, sizeof(vendor));
			device.product_string.to_char(product, sizeof(product));

			log("Found USB device: ", (char const*)vendor, " (",
			            (char const*)product, ") block size: ", _block_size,
			            " count: ", _block_count);

			if (_report_device)
				report_device(init.vendor, init.product,
				              init.block_count, init.block_size);
			return true;
		} catch (int) {
			/* handle command failures */
			error("Could not initialize storage device");
		} catch (...) {
			/* handle Usb::Session failures */
			error("Could not initialize storage device");
		}
		return false;
	}

	/**
	 * Execute pending request
	 *
	 * Called after the CBW has been successfully received by the device
	 * to initiate read/write transaction.
	 */
	bool execute_pending_request()
	{
		Usb::Interface     &iface = device.interface(active_interface);
		Usb::Endpoint          ep = iface.endpoint(request->read() ? ep_in : ep_out);
		Usb::Packet_descriptor  p = iface.alloc(request->size);

		if (request->write())
			memcpy(iface.content(p), (void *)request->address, request->size);

		iface.bulk_transfer(p, ep, false, this);

		return true;
	}

	void wakeup_client(bool success)
	{
		request->block_request.success = success;
		request->completed             = true;

		/*
		 * An I/O operation completed, thus trigger block-request handling on
		 * component service level.
		 */
		block_request_handler.local_submit();
	}

	/**
	 * Handle packet completion
	 *
	 * This method is called several times while doing one transaction. First
	 * the CWB is sent, than the payload read or written. At the end, the CSW
	 * is requested.
	 */
	void complete(Packet_descriptor &p) override
	{
		Usb::Interface iface = device.interface(active_interface);

		if (p.type != Packet_descriptor::BULK) {
			error("No BULK packet");
			iface.release(p);
			return;
		}

		if (!p.succeded) {
			error("complete error: packet not succeded");
			if (request_pending()) {
				error("request pending: tag: ", active_tag, " read: ",
				      request->read(), " buffer: ", Hex(request->address),
				      " lba: ", request->block_request.operation.block_number,
				      " size: ", request->size);
				wakeup_client(false);
			}
			iface.release(p);
			return;
		}

		static bool request_executed = false;
		if (!p.read_transfer()) {
			/* send read/write request */
			if (request_pending()) {

				/*
				 * The CBW was successfully sent to the device, now read/write the
				 * actual content.
				 */
				if (!request_executed) {
					request_executed = execute_pending_request();
				} else {
					/* the content was successfully written, get the CSW */
					csw(*this);
				}
			}

			iface.release(p);
			return;
		}

		int actual_size = p.transfer.actual_size;
		if (actual_size < 0) {
			error("Transfer actual size: ", actual_size);
			actual_size = 0;
		}

		/* the size indicates an IN I/O packet */
		if ((uint32_t)actual_size >= _block_size) {
			if (request_pending()) {

				/* the content was successfully read, get the CSW */
				memcpy((void *)request->address, iface.content(p), actual_size);
				csw(*this);
			}

			iface.release(p);
			return;
		}

		/* when ending up here, we should have gotten an CSW packet */
		if (actual_size != Csw::LENGTH)
			warning("This is not the actual size you are looking for");

		do {
			Csw csw({(char *)iface.content(p), p.size()});

			uint32_t const sig = csw.sig();
			if (sig != Csw::SIG) {
				error("CSW signature does not match: ",
				      Hex(sig, Hex::PREFIX, Hex::PAD));
				break;
			}

			uint32_t const tag = csw.tag();
			if (tag != active_tag) {
				error("CSW tag mismatch. Got ", tag, " expected: ",
				      active_tag);
				break;
			}

			uint32_t const status = csw.sts();
			if (status != Csw::PASSED) {
				error("CSW failed: ", Hex(status, Hex::PREFIX, Hex::PAD),
				      " read: ", request->read(), " buffer: ", (void *)request->address,
				      " lba: ", request->block_request.operation.block_number,
				      " size: ", request->size);
				break;
			}

			uint32_t const dr = csw.dr();
			if (dr) {
				warning("CSW data residue: ", dr, " not considered");
			}

			/* ack Block::Packet_descriptor */
			request_executed = false;
			wakeup_client(true);
		} while (0);

		iface.release(p);
	}

	/**
	 * Parse configuration
	 */
	void parse_config(Xml_node node)
	{
		_writeable       = node.attribute_value("writeable",    false);
		_report_device   = node.attribute_value("report",       false);
		active_interface = node.attribute_value<uint8_t>("interface", 0);
		active_lun       = node.attribute_value<uint8_t>("lun",       0);
		reset_device     = node.attribute_value("reset_device", false);
		verbose_scsi     = node.attribute_value("verbose_scsi", false);

		active_alt_setting =
			node.attribute_value<uint16_t>("alt_setting", INVALID_ALT_SETTING);
	}

	/**
	 * Constructor
	 *
	 * \param alloc allocator used by Usb::Connection
	 * \param ep    Server::Endpoint
	 * \param sigh  signal context used for annoucing Block service
	 */
	Block_driver(Env &env, Allocator &alloc,
	             Genode::Signal_context_capability sigh,
	             Signal_handler<Main> &block_request_handler)
	:
		env(env), ep(env.ep()), announce_sigh(sigh), alloc(&alloc),
		device(&alloc, usb, ep), block_request_handler(block_request_handler)
	{
		parse_config(config.xml());
		reporter.enabled(true);

		/* USB device gets initialized by handle_state_change() */
		handle_state_change();
	}

	~Block_driver()
	{
		Usb::Interface &iface = device.interface(active_interface);
		iface.release();
	}

	/**
	 * Send CBW
	 */
	void send_cbw(block_number_t lba, block_count_t count, bool read)
	{
		uint32_t const t = new_tag();

		/**
		 * Assuming a minimal packet size of 512. Total packet stream buffer
		 * should not exceed 16-bit block count value.
		 */
		static_assert((PACKET_STREAM_BUF_SIZE / 512UL) < (uint16_t)~0UL);
		uint16_t c = (uint16_t) count;

		/**
		 * We check for lba fitting 32-bit value for 10-Cmd mode
		 * before entering this function
		 */
		char cb[Cbw::LENGTH];
		Byte_range_ptr cb_range {(char *)cb, Cbw::LENGTH};
		if (read) {
			if (force_cmd_16) Read_16 r(cb_range, t, active_lun, lba, c, _block_size);
			else              Read_10 r(cb_range, t, active_lun, (uint32_t)lba, c, _block_size);
		} else {
			if (force_cmd_16) Write_16 w(cb_range, t, active_lun, lba, c, _block_size);
			else              Write_10 w(cb_range, t, active_lun, (uint32_t)lba, c, _block_size);
		}

		cbw(cb, *this);
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
		if (device_plugged == false)
			return Response::REJECTED;

		/*
		 * Check if there is already a request pending and wait
		 * until it has finished. We do this check here to implement
		 * 'SYNC' as barrier that waits for out-standing requests.
		 */
		if (request_pending())
			return Response::RETRY;

		Operation const &op = block_request.operation;

		/* read only */
		if (info().writeable == false &&
		    op.type == Operation::Type::WRITE)
			return Response::REJECTED;

		/* range check */
		block_number_t const last = op.block_number + op.count;
		if (last > info().block_count)
			return Response::REJECTED;

		/* we only support 32-bit block numbers in 10-Cmd mode */
		if (!force_cmd_16 && last >= ~0U)
			return Response::REJECTED;

		request.construct(block_request, payload);

		/* operations currently handled as successful NOP */
		if (request->block_request.operation.type == Operation::Type::TRIM
		 || request->block_request.operation.type == Operation::Type::SYNC) {
			request->block_request.success = true;
			request->completed             = true;
			return Response::ACCEPTED;
		}

		/* execute */
		send_cbw(op.block_number, op.count, request->read());

		return Response::ACCEPTED;
	}

	template <typename FUNC>
	void with_completed(FUNC const &fn)
	{
		if (request_pending() == false || request->completed == false) return;

		fn(request->block_request);

		request.destruct();
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
	Env  &env;
	Heap  heap { env.ram(), env.rm() };

	Constructible<Attached_ram_dataspace>  block_ds { }; 
	Constructible<Block_session_component> block_session { };

	Signal_handler<Main> announce_dispatcher {
		env.ep(), *this, &Usb::Main::announce };

	Signal_handler<Main>  request_handler {
		env.ep(), *this, &Main::handle_requests };

	Block_driver driver { env, heap, announce_dispatcher, request_handler };

	void announce()
	{
		env.parent().announce(env.ep().manage(*this));
	}

	/**
	 * There can only be one request in flight for this driver, so keep it simple
	 */
	void handle_requests()
	{
		if (!block_session.constructed()) return;

		bool progress;
		do {
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
		} while (progress);

		block_session->wakeup_client_if_needed();
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
		block_session.construct(env, block_ds->cap(), request_handler,
		                        driver.info());

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

	Main(Env &env) : env(env) { }
};


void Component::construct(Genode::Env &env) { static Usb::Main main(env); }
