/*
 * \brief  Usb session to Block session translator
 * \author Josef Soentgen
 * \date   2016-02-08
 */

/*
 * Copyright (C) 2016-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Genode includes */
#include <base/allocator_avl.h>
#include <base/attached_rom_dataspace.h>
#include <base/component.h>
#include <base/log.h>
#include <base/heap.h>
#include <block/component.h>
#include <block/driver.h>
#include <block_session/connection.h>
#include <os/reporter.h>
#include <timer_session/connection.h>
#include <usb/usb.h>

/* used by cbw_csw.h so declare it here */
static bool verbose_scsi = false;

/* local includes */
#include <cbw_csw.h>


namespace Usb {
	using namespace Genode;

	struct Block_driver;
	struct Main;
}


/*********************************************************
 ** USB Mass Storage (BBB) Block::Driver implementation **
 *********************************************************/

struct Usb::Block_driver : Usb::Completion,
                           Block::Driver
{
	Env        &env;
	Entrypoint &ep;

	Signal_context_capability announce_sigh;

	/*
	 * Pending block request
	 */
	struct Block_request
	{
		Block::Packet_descriptor  packet;
		Block::sector_t           lba;
		char                     *buffer;
		size_t                    size;
		bool                      read;
		bool                      pending = false;
	} req;

	bool initialized     = false;
	bool device_plugged  = false;

	/**
	 * Handle stage change signal
	 */
	void handle_state_change()
	{
		if (!usb.plugged()) {
			Genode::log("Device unplugged");
			device_plugged = false;
			return;
		}

		if (initialized) {
			Genode::error("Device was already initialized");
			return;
		}

		Genode::log("Device plugged");

		if (!initialize()) {
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
	static char const *get_label(Xml_node node)
	{
		static Genode::String<256> usb_label;
		try {
			node.attribute("label").value(&usb_label);
			return usb_label.string();
		} catch (...) { }

		return "usb_storage";
	}

	/*
	 * USB session
	 */
	Allocator_avl   alloc;
	Usb::Connection usb { env, &alloc, get_label(config.xml()), 2 * (1<<20), state_change_dispatcher };
	Usb::Device     device;

	/*
	 * Reporter
	 */
	Reporter reporter { env, "devices" };
	bool _report_device = false;

	/*
	 * Block session
	 */
	Block::Session::Operations _block_ops;
	Block::sector_t            _block_count;
	size_t                     _block_size;

	bool _writeable = false;

	bool force_cmd_10 = false;

	uint8_t active_interface = 0;
	uint8_t active_lun       = 0;

	uint32_t active_tag = 0;
	uint32_t new_tag() { return ++active_tag % 0xffffffu; }

	enum Tags {
		INQ_TAG = 0x01, RDY_TAG = 0x02, CAP_TAG = 0x04,
		REQ_TAG = 0x08, SS_TAG = 0x10
	};
	enum Endpoints  { IN = 0, OUT = 1 };

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

		Usb::Device &device;
		uint8_t      interface;

		Block::sector_t block_count;
		size_t          block_size;

		char vendor[Scsi::Inquiry_response::Vid::ITEMS+1];
		char product[Scsi::Inquiry_response::Pid::ITEMS+1];

		Init_completion(Usb::Device &device, uint8_t interface)
		: device(device), interface(interface) { }

		void complete(Packet_descriptor &p)
		{
			Interface iface = device.interface(interface);

			if (p.type != Packet_descriptor::BULK) {
				Genode::error("Can only handle BULK packets");
				iface.release(p);
				return;
			}

			if (!p.succeded) {
				Genode::error("init complete error: packet not succeded");
				iface.release(p);
				return;
			}

			/* OUT transfer finished */
			if (!p.read_transfer()) {
				iface.release(p);
				return;
			}

			int const actual_size = p.transfer.actual_size;
			char * const data     = reinterpret_cast<char*>(iface.content(p));

			using namespace Scsi;

			switch (actual_size) {
			case 36: /* min INQUIRY data size */
			case Inquiry_response::LENGTH:
			{
				Inquiry_response r((addr_t)data);
				if (verbose_scsi) r.dump();

				if (!r.sbc()) {
					Genode::warning("Device does not use SCSI Block Commands and may not work");
				}

				r.get_id<Inquiry_response::Vid>(vendor, sizeof(vendor));
				r.get_id<Inquiry_response::Pid>(product, sizeof(product));
				break;
			}
			case Capacity_response_10::LENGTH:
			{
				Capacity_response_10 r((addr_t)data);
				if (verbose_scsi) r.dump();

				block_count = r.block_count();
				block_size  = r.block_size();
				break;
			}
			case Capacity_response_16::LENGTH:
			{
				Capacity_response_16 r((addr_t)data);
				if (verbose_scsi) r.dump();

				block_count = r.block_count();
				block_size  = r.block_size();
				break;
			}
			case Request_sense_response::LENGTH:
			{
				Request_sense_response r((addr_t)data);
				if (verbose_scsi) r.dump();

				uint8_t const asc = r.read<Request_sense_response::Asc>();
				uint8_t const asq = r.read<Request_sense_response::Asq>();

				enum { MEDIUM_NOT_PRESENT = 0x3a, NOT_READY_TO_READY_CHANGE = 0x28 };
				switch (asc) {
				case MEDIUM_NOT_PRESENT:
					Genode::error("Not ready - medium not present");
					no_medium = true;
					break;
				case NOT_READY_TO_READY_CHANGE: /* asq == 0x00 */
					Genode::warning("Not ready - try again");
					try_again = true;
					break;
				default:
					Genode::error("Request_sense_response asc: ",
					              Hex(asc, Hex::PREFIX, Hex::PAD),
					              " asq: ", Hex(asq, Hex::PREFIX, Hex::PAD));
					break;
				}
				break;
			}
			case Csw::LENGTH:
			{
				Csw csw((addr_t)data);

				uint32_t const sig = csw.sig();
				if (sig != Csw::SIG) {
					Genode::error("CSW signature does not match: ",
					              Hex(sig, Hex::PREFIX, Hex::PAD));
					break;
				}

				uint32_t const   tag = csw.tag();
				uint8_t const status = csw.sts();
				if (status != Csw::PASSED) {
					Genode::error("CSW failed: ", Hex(status, Hex::PREFIX, Hex::PAD),
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
		Usb::Endpoint         &ep = iface.endpoint(OUT);
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
		Usb::Endpoint         &ep = iface.endpoint(IN);
		Usb::Packet_descriptor  p = iface.alloc(CSW_VALID_SIZE);
		iface.bulk_transfer(p, ep, block, &c);
	}

	/**
	 * Receive response
	 */
	void resp(size_t size, Completion &c, bool block = false)
	{
		Usb::Interface     &iface = device.interface(active_interface);
		Usb::Endpoint         &ep = iface.endpoint(IN);
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
			Genode::Reporter::Xml_generator xml(reporter, [&] () {
				xml.node("device", [&] () {
					xml.attribute("vendor", vendor);
					xml.attribute("product", product);
					xml.attribute("block_count", count);
					xml.attribute("block_size", size);
					xml.attribute("writeable", _writeable);
				});
			});
		} catch (...) { Genode::warning("Could not report block device"); }
	}

	/**
	 * Initialize device
	 *
	 * All USB transfers in this method are done synchronously. First we reset
	 * the device, than we query the max LUN. Afterwards we start sending CBWs.
	 *
	 * Since it might take some time for the device to get ready to use, we
	 * have to check the SCSI logical unit several times.
	 */
	bool initialize()
	{
		device.update_config();

		Interface &iface = device.interface(active_interface);
		try { iface.claim(); }
		catch (Usb::Session::Interface_already_claimed) {
			Genode::error("Device already claimed");
			return false;
		} catch (Usb::Session::Interface_not_found) {
			Genode::error("Interface not found");
			return false;
		}

		enum {
			ICLASS_MASS_STORAGE = 8,
			ISUBCLASS_SCSI      = 6,
			IPROTO_BULK_ONLY    = 80
		};
		try {
			Alternate_interface &alt_iface = iface.alternate_interface(0);
			iface.set_alternate_interface(alt_iface);

			if (alt_iface.iclass != ICLASS_MASS_STORAGE
				|| alt_iface.isubclass != ISUBCLASS_SCSI
				|| alt_iface.iprotocol != IPROTO_BULK_ONLY) {
				Genode::error("No mass storage SCSI bulk-only device");
				return false;
			}
		} catch (Usb::Session::Interface_not_found) {
			Genode::error("Interface not found");
			return false;
		}

		try {
			/* reset */
			Usb::Packet_descriptor p = iface.alloc(0);
			iface.control_transfer(p, 0x21, 0xff, 0, active_interface, 100);
			if (!p.succeded) {
				Genode::error("Could not reset device");
				throw -1;
			}

			/*
			 * Let us do GetMaxLUN and simply ignore the return value because none
			 * of the devices that were tested did infact report another value than 0.
			 */
			p = iface.alloc(1);
			iface.control_transfer(p, 0xa1, 0xfe, 0, active_interface, 100);
			uint8_t max_lun = *(uint8_t*)iface.content(p);
			if (p.succeded && max_lun == 0) { max_lun = 1; }
			iface.release(p);

			/*
			 * Query device
			 */

			char cbw_buffer[Cbw::LENGTH];

			/*
			 * We should probably execute the SCSI REPORT_LUNS command first
			 * but we will receive LOGICAL UNIT NOT SUPPORTED if we try to
			 * access an invalid unit. The user has to specify the LUN in
			 * the configuration anyway.
			 */

			/* Scsi::Opcode::INQUIRY */
			Inquiry inq((addr_t)cbw_buffer, INQ_TAG, active_lun);

			cbw(cbw_buffer, init, true);
			resp(Scsi::Inquiry_response::LENGTH, init, true);
			csw(init, true);

			if (!init.inquiry) {
				Genode::warning("Inquiry_cmd failed");
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
					Test_unit_ready unit_ready((addr_t)cbw_buffer, RDY_TAG, active_lun);

					cbw(cbw_buffer, init, true);
					csw(init, true);

					if (!init.unit_ready) {
						Request_sense sense((addr_t)cbw_buffer, REQ_TAG, active_lun);

						cbw(cbw_buffer, init, true);
						resp(Scsi::Request_sense_response::LENGTH, init, true);
						csw(init, true);
						if (!init.request_sense) {
							Genode::warning("Request_sense failed");
							throw -1;
						}

						if (init.no_medium) {
							/* do nothing for now */
						} else if (init.try_again) {
							init.try_again = false;
						} else break;
					} else break;

					timer.msleep(1000);
				}
				if (retries == MAX_RETRIES) {
					Genode::warning("Test_unit_ready_cmd failed");
					throw -1;
				}
			}

			/* Scsi::Opcode::READ_CAPACITY_16 */
			Read_capacity_16 read_cap((addr_t)cbw_buffer, CAP_TAG, active_lun);

			cbw(cbw_buffer, init, true);
			resp(Scsi::Capacity_response_16::LENGTH, init, true);
			csw(init, true);

			if (!init.read_capacity) {
				/* try Scsi::Opcode::READ_CAPACITY_10 next */
				Read_capacity_10 read_cap((addr_t)cbw_buffer, CAP_TAG, active_lun);

				cbw(cbw_buffer, init, true);
				resp(Scsi::Capacity_response_10::LENGTH, init, true);
				csw(init, true);

				if (!init.read_capacity) {
					Genode::warning("Read_capacity_cmd failed");
					throw -1;
				}

				Genode::warning("Device does not support CDB 16-byte commands, force 10-byte commands");
				force_cmd_10 = true;
			}

			_block_size  = init.block_size;
			_block_count = init.block_count;

			initialized     = true;
			device_plugged  = true;

			char vendor[32];
			char product[32];

			device.manufactorer_string.to_char(vendor, sizeof(vendor));
			device.product_string.to_char(product, sizeof(product));

			Genode::log("Found USB device: ", (char const*)vendor, " (",
			            (char const*)product, ") block size: ", _block_size,
			            " count: ", _block_count);

			if (_report_device)
				report_device(init.vendor, init.product,
				              init.block_count, init.block_size);
			return true;
		} catch (int) {
			/* handle command failures */
			Genode::error("Could not initialize storage device");
			return false;
		} catch (...) {
			/* handle Usb::Session failures */
			Genode::error("Could not initialize storage device");
			throw;
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
		Usb::Endpoint          ep = iface.endpoint(req.read ? IN : OUT);
		Usb::Packet_descriptor  p = iface.alloc(req.size);

		if (!req.read) memcpy(iface.content(p), req.buffer, req.size);

		iface.bulk_transfer(p, ep, false, this);

		return true;
	}

	/**
	 * Acknowledge currently pending request
	 *
	 * After receiving the CSW ack the request at the Block session.
	 */
	void ack_pending_request(bool success = true)
	{
		/*
		 * Needs to be reset bevor calling ack_packet to prevent getting a new
		 * request imediately and throwing Request_congestion() in io() again.
		 */
		req.pending = false;

		Block::Packet_descriptor p = req.packet;
		ack_packet(p, success);
	}

	/**
	 * Handle packet completion
	 *
	 * This method is called several times while doing one transaction. First
	 * the CWB is sent, than the payload read or written. At the end, the CSW
	 * is requested.
	 */
	void complete(Packet_descriptor &p)
	{
		Interface iface = device.interface(active_interface);

		if (p.type != Packet_descriptor::BULK) {
			Genode::error("No BULK packet");
			iface.release(p);
			return;
		}

		if (!p.succeded) {
			Genode::error("complete error: packet not succeded");
			if (req.pending) {
				Genode::error("request pending: tag: ", active_tag, " read: ",
				              (int)req.read, " buffer: ", (void *)req.buffer, " lba: ",
				              req.lba, " size: ", req.size);
				ack_pending_request(false);
			}
			iface.release(p);
			return;
		}

		static bool request_executed = false;
		if (!p.read_transfer()) {
			/* send read/write request */
			if (req.pending) {

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
			Genode::error("Transfer actual size: ", actual_size);
			actual_size = 0;
		}

		/* the size indicates an IN I/O packet */
		if ((uint32_t)actual_size >= _block_size) {
			if (req.pending) {

				/* the content was successfully read, get the CSW */
				memcpy(req.buffer, iface.content(p), actual_size);
				csw(*this);
			}

			iface.release(p);
			return;
		}

		/* when ending up here, we should have gotten an CSW packet */
		if (actual_size != Csw::LENGTH)
			Genode::warning("This is not the actual size you are looking for");

		do {
			Csw csw((addr_t)iface.content(p));

			uint32_t const sig = csw.sig();
			if (sig != Csw::SIG) {
				Genode::error("CSW signature does not match: ",
				              Hex(sig, Hex::PREFIX, Hex::PAD));
				break;
			}

			uint32_t const tag = csw.tag();
			if (tag != active_tag) {
				Genode::error("CSW tag mismatch. Got ", tag, " expected: ",
				              active_tag);
				break;
			}

			uint8_t const status = csw.sts();
			if (status != Csw::PASSED) {
				Genode::error("CSW failed: ", Hex(status, Hex::PREFIX, Hex::PAD),
				              " read: ", (int)req.read, " buffer: ", (void *)req.buffer,
				              " lba: ", req.lba, " size: ", req.size);
				break;
			}

			uint32_t const dr = csw.dr();
			if (dr) {
				Genode::warning("CSW data residue: ", dr, " not considered");
			}

			/* ack Block::Packet_descriptor */
			request_executed = false;
			ack_pending_request();
		} while (0);

		iface.release(p);
	}

	/**
	 * Parse configuration
	 */
	void parse_config(Xml_node node)
	{
		_block_ops.set_operation(Block::Packet_descriptor::READ);

		_writeable = node.attribute_value<bool>("writeable", false);
		if (_writeable)
			_block_ops.set_operation(Block::Packet_descriptor::WRITE);

		_report_device = node.attribute_value<bool>("report", false);

		active_interface = node.attribute_value<unsigned long>("interface", 0);
		active_lun       = node.attribute_value<unsigned long>("lun", 0);

		verbose_scsi = node.attribute_value<bool>("verbose_scsi", false);
	}

	/**
	 * Constructor
	 *
	 * \param alloc allocator used by Usb::Connection
	 * \param ep    Server::Endpoint
	 * \param sigh  signal context used for annoucing Block service
	 */
	Block_driver(Env &env, Genode::Allocator &alloc,
	             Genode::Signal_context_capability sigh)
	:
		Block::Driver(env.ram()),
		env(env), ep(env.ep()), announce_sigh(sigh), alloc(&alloc),
		device(&alloc, usb, ep)
	{
		parse_config(config.xml());
		reporter.enabled(true);

		/* USB device gets initialized by handle_state_change() */
	}

	/**
	 * Send CBW
	 */
	void send_cbw(Block::sector_t lba, size_t len, bool read)
	{
		uint32_t const t = new_tag();

		char cb[Cbw::LENGTH];
		if (read) {
			if (!force_cmd_10) Read_16 r((addr_t)cb, t, active_lun, lba, len, _block_size);
			else               Read_10 r((addr_t)cb, t, active_lun, lba, len, _block_size);
		} else {
			if (!force_cmd_10) Write_16 w((addr_t)cb, t, active_lun, lba, len, _block_size);
			else               Write_10 w((addr_t)cb, t, active_lun, lba, len, _block_size);
		}

		cbw(cb, *this);
	}

	/**
	 * Perform IO/ request
	 *
	 * \param read    set to true when reading, false when writting
	 * \param lba     address of the starting block
	 * \param buffer  source/destination buffer
	 * \param p       Block::Packet_descriptor
	 */
	void io(bool read, Block::sector_t lba, size_t count,
	        char *buffer, Block::Packet_descriptor &p)
	{
		if (!device_plugged)          throw Io_error();
		if (lba+count > _block_count) throw Io_error();
		if (req.pending)              throw Request_congestion();

		req.pending = true;
		req.packet  = p;
		req.lba     = lba;
		req.size    = count * _block_size;
		req.buffer  = buffer;
		req.read    = read;

		send_cbw(lba, count, read);
	}

	/*******************************
	 **  Block::Driver interface  **
	 *******************************/

	size_t              block_size() override { return _block_size;  }
	Block::sector_t    block_count() override { return _block_count; }
	Block::Session::Operations ops() override { return _block_ops;   }

	void read(Block::sector_t lba, size_t count,
	          char *buffer, Block::Packet_descriptor &p) override {
		io(true, lba, count, buffer, p); }

	void write(Block::sector_t lba, size_t count,
	           char const *buffer, Block::Packet_descriptor &p) override {
		io(false, lba, count, const_cast<char*>(buffer), p); }

	void sync() override { /* maybe implement SYNCHRONIZE_CACHE_10/16? */ }
};


struct Usb::Main
{
	Env  &env;
	Heap  heap { env.ram(), env.rm() };

	void announce()
	{
		env.parent().announce(env.ep().manage(root));
	}

	Signal_handler<Main> announce_dispatcher {
		env.ep(), *this, &Usb::Main::announce };

	struct Factory : Block::Driver_factory
	{
		Env                       &env;
		Allocator                 &alloc;
		Signal_context_capability  sigh;

		Usb::Block_driver *driver = nullptr;

		Factory(Env &env, Allocator &alloc,
		        Signal_context_capability sigh)
		: env(env), alloc(alloc), sigh(sigh)
		{
			driver = new (&alloc) Usb::Block_driver(env, alloc, sigh);
		}

		Block::Driver *create() { return driver; }

		void destroy(Block::Driver *driver) { }
	};

	Factory     factory { env, heap, announce_dispatcher };
	Block::Root root    { env.ep(), heap, env.rm(), factory };

	Main(Env &env) : env(env) { }
};


void Component::construct(Genode::Env &env) { static Usb::Main main(env); }
