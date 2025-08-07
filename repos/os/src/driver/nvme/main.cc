/*
 * \brief  NVMe Block session component
 * \author Josef Soentgen
 * \date   2018-03-05
 *
 * Spec used: NVM-Express-1_3a-20171024_ratified.pdf
 */

/*
 * Copyright (C) 2018 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Genode includes */
#include <base/allocator_avl.h>
#include <base/attached_ram_dataspace.h>
#include <base/attached_rom_dataspace.h>
#include <base/component.h>
#include <base/heap.h>
#include <base/log.h>
#include <base/registry.h>
#include <base/tslab.h>
#include <block/request_stream.h>
#include <block/session_map.h>
#include <dataspace/client.h>
#include <os/attached_mmio.h>
#include <os/reporter.h>
#include <os/session_policy.h>
#include <platform_session/device.h>
#include <root/root.h>
#include <timer_session/connection.h>
#include <util/bit_allocator.h>
#include <util/interface.h>
#include <util/misc_math.h>

/* local includes */
#include <util.h>
#include <dma_buffer.h>


namespace {
	using namespace Genode;

	using Response = Block::Request_stream::Response;
} /* anonymous namespace */


/**********
 ** NVMe **
 **********/

namespace Nvme {
	using namespace Genode;

	struct Identify_data;
	struct Identify_ns_data;
	struct Doorbell;

	struct Cqe;

	template <size_t> struct Sqe;
	struct Sqe_header;
	struct Sqe_create_cq;
	struct Sqe_create_sq;
	struct Sqe_identify;
	struct Sqe_get_feature;
	template <size_t> struct Sqe_set_feature;
	struct Sqe_io;

	struct Set_hmb;
	struct Set_numq;

	struct Queue;
	struct Sq;
	struct Cq;

	struct Io_queue;

	using Io_queue_space = Id_space<Io_queue>;

	struct Controller;

	enum {
		CQE_LEN_LOG2           = 4u,
		CQE_LEN                = 1u << CQE_LEN_LOG2,
		SQE_LEN_LOG2           = 6u,
		SQE_LEN                = 1u << SQE_LEN_LOG2,
		MAX_IO_QUEUES          = 128u,
		NUM_QUEUES             = 1u + MAX_IO_QUEUES,

		/*
		 * Limit max number of I/O slots. By now most controllers
		 * should support >= 1024 but the current value is a trade-off
		 * as all data structures are allocated statically. However,
		 * the number of entries is rounded down to the number the
		 * controller actually supports in case it is smaller.
		 */
		MAX_IO_ENTRIES         = 512,
		MAX_IO_ENTRIES_MASK    = MAX_IO_ENTRIES - 1,
		MAX_ADMIN_ENTRIES      = 128,
		MAX_ADMIN_ENTRIES_MASK = MAX_ADMIN_ENTRIES - 1,
		MPS_LOG2               = 12u,
		MPS                    = 1u << MPS_LOG2,

		/*
		 * Setup the descriptor list in one page and use a chunk
		 * size that covers the common amount of HMB well and
		 * requires resonable sized mappings.
		 */
		HMB_LIST_SIZE          = 4096,
		HMB_LIST_ENTRY_SIZE    = 16,
		HMB_LIST_MAX_ENTRIES   = HMB_LIST_SIZE / HMB_LIST_ENTRY_SIZE,
		HMB_CHUNK_SIZE         = (2u << 20),
		HMB_CHUNK_UNITS        = HMB_CHUNK_SIZE / MPS,
	};

	enum {
		/*
		 * Limit max I/O requests size; we can map up to 2 MiB with
		 * one list page (4K/8 = 512 * 4K). However, the size is
		 * rounded down to the size the controller actually supports
		 * according to the MDTS register.
		 */
		MAX_IO_LEN  = 2u << 20,
		PRP_DS_SIZE = MAX_IO_ENTRIES * MPS,
	};

	enum {
		/*
		 * Limit namespace handling to the first namespace. Most
		 * if not all consumer NVMe devices only have one.
		 */
		IO_NSID    = 1u,
		MAX_NS     = 1u,
	};

	enum Opcode {
		/* Admin command set */
		DELETE_IO_SQ = 0x00,
		CREATE_IO_SQ = 0x01,
		DELETE_IO_CQ = 0x04,
		CREATE_IO_CQ = 0x05,
		IDENTIFY     = 0x06,
		SET_FEATURES = 0x09,
		GET_FEATURES = 0x0A,
		/* NVM command set */
		FLUSH        = 0x00,
		WRITE        = 0x01,
		READ         = 0x02,
		WRITE_ZEROS  = 0x08,
	};

	enum Feature_fid {
		NUMQ = 0x07,
		HMB  = 0x0d,
	};

	enum Feature_sel {
		CURRENT   = 0b000,
		DEFAULT   = 0b001,
		SAVED     = 0b010,
		SUPPORTED = 0b011,
	};

	struct Block_session_component;

	using Session_space = Id_space<Block_session_component>;

	struct Driver;
	struct Main;
};


/*
 * Identify command data
 */
struct Nvme::Identify_data : Genode::Mmio<0x208>
{
	enum {
		SN_OFFSET = 0x04, SN_LEN = 20,
		MN_OFFSET = 0x18, MN_LEN = 40,
		FR_OFFSET = 0x40, FR_LEN = 12,
	};

	using Sn = Genode::String<SN_LEN + 1>;
	using Mn = Genode::String<MN_LEN + 1>;
	using Fr = Genode::String<FR_LEN + 1>;

	Sn sn { }; /* serial number */
	Mn mn { }; /* model number */
	Fr fr { }; /* firmware revision */

	struct Vid   : Register<0x000, 16> { }; /* vendor id */
	struct Ssvid : Register<0x002, 16> { }; /* sub system vendor id */
	struct Mdts  : Register<0x04d,  8> { }; /* maximum data transfer size */

	/* optional admin command support */
	struct Oacs  : Register<0x100, 32>
	{
		struct Ssr  : Bitfield< 0, 1> { }; /* security send/receive */
		struct Nvmf : Bitfield< 1, 1> { }; /* NVM format */
		struct Fwcd : Bitfield< 2, 1> { }; /* firmware commit/download image */
		struct Nsm  : Bitfield< 3, 1> { }; /* namespace management */
		struct Vm   : Bitfield< 7, 1> { }; /* virtualization management */
	};

	/* optional host memory buffer */
	struct Hmpre : Register<0x110, 32> { }; /* preferred size */
	struct Hmmin : Register<0x114, 32> { }; /* minimum size */

	struct Nn    : Register<0x204, 32> { }; /* number of namespaces */
	struct Vwc   : Register<0x204,  8> { }; /* volatile write cache */

	Identify_data(Byte_range_ptr const &range) : Mmio(range)
	{
		char const *p = Mmio::range().start;

		sn = Sn(Util::extract_string(p, SN_OFFSET, SN_LEN+1));
		mn = Mn(Util::extract_string(p, MN_OFFSET, MN_LEN+1));
		fr = Fr(Util::extract_string(p, FR_OFFSET, FR_LEN+1));
	}
};


/*
 * Identify name space command data
 */
struct Nvme::Identify_ns_data : public Genode::Mmio<0xc0>
{
	struct Nsze   : Register<0x00, 64> { }; /* name space size */
	struct Ncap   : Register<0x08, 64> { }; /* name space capacity */
	struct Nuse   : Register<0x10, 64> { }; /* name space utilization */
	struct Nsfeat : Register<0x18,  8> { }; /* name space features */
	struct Nlbaf  : Register<0x19,  8> { }; /* number of LBA formats */
	/* formatted LBA size */
	struct Flbas  : Register<0x1a,  8>
	{
		struct Formats : Bitfield< 0,  3> { };
	};
	struct Mc     : Register<0x1b,  8> { }; /* metadata capabilities */
	struct Dpc    : Register<0x1c,  8> { }; /* end-to-end data protection capabilities */
	struct Dps    : Register<0x1d,  8> { }; /* end-to-end data protection settings */

	enum { MAX_LBAF = 16, };
	/* LBA format support */
	struct Lbaf   : Register_array<0x80, 32, MAX_LBAF, 32>
	{
		struct Ms    : Bitfield< 0, 16> { }; /* metadata size */
		struct Lbads : Bitfield<16,  8> { }; /* LBA data size (2^n) */
		struct Rp    : Bitfield<24,  2> { }; /* relative performance */
	};

	Identify_ns_data(Byte_range_ptr const &range) : Mmio(range) { }
};


/*
 * Queue doorbell register
 */
struct Nvme::Doorbell : public Genode::Mmio<0x8>
{
	struct Sqtdbl : Register<0x00, 32>
	{
		struct Sqt : Bitfield< 0, 16> { }; /* submission queue tail */
	};

	struct Cqhdbl : Register<0x04, 32>
	{
		struct Cqh : Bitfield< 0, 16> { }; /* submission queue tail */
	};

	Doorbell(Byte_range_ptr const &range) : Mmio(range) { }
};


/*
 * Completion queue entry
 */
struct Nvme::Cqe : Genode::Mmio<0x10>
{
	struct Dw0  : Register<0x00, 32> { }; /* command specific */
	struct Dw1  : Register<0x04, 32> { }; /* reserved */

	struct Sqhd : Register<0x08, 16> { };
	struct Sqid : Register<0x0a, 16> { };
	struct Cid  : Register<0x0c, 16> { };
	struct Sf   : Register<0x0e, 16>
	{
		struct P   : Bitfield< 0, 1> { };
		struct Sc  : Bitfield< 1, 8> { }; /* status code */
		struct Sct : Bitfield< 9, 3> { }; /* status code type */
		struct M   : Bitfield<14, 1> { }; /* more (get log) */
		struct Dnr : Bitfield<15, 1> { }; /* do not retry */
	};

	Cqe(Byte_range_ptr const &range) : Mmio(range) { }

	static uint32_t request_id(Nvme::Cqe const &b)
	{
		return (b.read<Sqid>() << 16)|b.read<Cid>();
	}

	static uint16_t command_id(Nvme::Cqe const &b)
	{
		return b.read<Cid>();
	}

	static bool succeeded(Nvme::Cqe const &b)
	{
		return !b.read<Sf::Sc>();
	}

	static void dump(Nvme::Cqe const &b)
	{
		using namespace Genode;
		log("sqhd:",        b.read<Sqhd>(),     " "
		    "sqid:",        b.read<Sqid>(),     " "
		    "cid:",         b.read<Cid>(),      " "
		    "p:",           b.read<Sf::P>(),    " "
		    "status: ", Hex(b.read<Sf>()),      " "
		    "sc:",      Hex(b.read<Sf::Sc>()),  " "
		    "sct:",     Hex(b.read<Sf::Sct>()), " "
		    "m:",           b.read<Sf::M>(),    " "
		    "dnr:",         b.read<Sf::Dnr>());
	}
};


/*
 * Submission queue entry base
 */
template <size_t SIZE>
struct Nvme::Sqe : Genode::Mmio<SIZE>
{
	using Base = Genode::Mmio<SIZE>;

	using Cdw0_base = Base::template Register<0x00, 32>;
	struct Cdw0 : Cdw0_base
	{
		struct Opc  : Cdw0_base::template Bitfield< 0,  8> { }; /* opcode */
		struct Fuse : Cdw0_base::template Bitfield< 9,  2> { }; /* fused operation */
		struct Psdt : Cdw0_base::template Bitfield<14,  2> { }; /* PRP or SGL for data transfer */
		struct Cid  : Cdw0_base::template Bitfield<16, 16> { }; /* command identifier */
	};
	struct Nsid : Base::template Register<0x04, 32> { };
	struct Mptr : Base::template Register<0x10, 64> { };
	struct Prp1 : Base::template Register<0x18, 64> { };
	struct Prp2 : Base::template Register<0x20, 64> { };

	/* SGL not supported */

	Sqe(Byte_range_ptr const &range) : Mmio<SIZE>(range) { }

	bool valid() const { return Base::base() != 0ul; }
};


struct Nvme::Sqe_header : Nvme::Sqe<0x28> { using Sqe::Sqe; };


/*
 * Identify command
 */
struct Nvme::Sqe_identify : Nvme::Sqe<0x2c>
{
	struct Cdw10 : Register<0x28, 32>
	{
		struct Cns : Bitfield< 0, 8> { }; /* controller or namespace structure */
	};

	Sqe_identify(Byte_range_ptr const &range) : Sqe(range) { }
};


/*
 * Get feature command
 */
struct Nvme::Sqe_get_feature : Nvme::Sqe<0x2c>
{
	struct Cdw10 : Register<0x28, 32>
	{
		struct Fid : Bitfield< 0, 8> { }; /* feature identifier */
		struct Sel : Bitfield< 8, 2> { }; /* select which value is returned */
	};

	Sqe_get_feature(Byte_range_ptr const &range) : Sqe(range) { }
};


/*
 * Set feature command
 */
template <size_t SIZE>
struct Nvme::Sqe_set_feature : Nvme::Sqe<SIZE>
{
	using Base = Genode::Mmio<SIZE>;

	using Cdw10_base = Base::template Register<0x28, 32>;
	struct Cdw10 : Cdw10_base
	{
		struct Fid : Cdw10_base::template Bitfield< 0, 8> { }; /* feature identifier */
		struct Sv  : Cdw10_base::template Bitfield<31, 1> { }; /* save */
	};

	Sqe_set_feature(Byte_range_ptr const &range) : Sqe<SIZE>(range) { }
};


struct Hmb_de : Genode::Mmio<0x10>
{
	enum { SIZE = 16u };

	struct Badd  : Register<0x00, 64> { };
	struct Bsize : Register<0x08, 64> { };

	Hmb_de(Byte_range_ptr const &range, addr_t const buffer, size_t units) : Mmio(range)
	{
		write<Badd>(buffer);
		write<Bsize>(units);
	}
};


struct Nvme::Set_hmb : Nvme::Sqe_set_feature<0x40>
{
	struct Cdw11 : Register<0x2c, 32>
	{
		struct Ehm : Bitfield< 0, 1> { }; /* enable host memory buffer */
		struct Mr  : Bitfield< 1, 1> { }; /* memory return */
	};

	struct Cdw12 : Register<0x30, 32>
	{
		struct Hsize : Bitfield<0, 32> { }; /* host memory buffer size (in MPS units) */
	};

	struct Cdw13 : Register<0x34, 32>
	{
		/* bits 3:0 should be zero */
		struct Hmdlla : Bitfield<0, 32> { }; /* host memory descriptor list lower address */
	};

	struct Cdw14 : Register<0x38, 32>
	{
		struct Hmdlua : Bitfield<0, 32> { }; /* host memory descriptor list upper address */
	};

	struct Cdw15 : Register<0x3c, 32>
	{
		struct Hmdlec : Bitfield<0, 32> { }; /* host memory descriptor list entry count */
	};

	Set_hmb(Byte_range_ptr const &range, uint64_t const hmdl,
	        uint32_t const units, uint32_t const entries)
	:
		Sqe_set_feature(range)
	{
		write<Sqe_set_feature::Cdw10::Fid>(Feature_fid::HMB);
		write<Cdw11::Ehm>(1);
		write<Cdw12::Hsize>(units);
		write<Cdw13::Hmdlla>(uint32_t(hmdl));
		write<Cdw14::Hmdlua>(uint32_t(hmdl >> 32u));
		write<Cdw15::Hmdlec>(entries);
	}
};


struct Nvme::Set_numq : Nvme::Sqe_set_feature<0x40>
{
	struct Cdw11 : Register<0x2c, 32>
	{
		struct Nsqr : Bitfield< 0, 16> { }; /* number of submission queues requested */
		struct Ncqr : Bitfield<16, 16> { }; /* number of completion queues requested */
	};

	Set_numq(Byte_range_ptr const &range, uint16_t num_queues)
	:
		Sqe_set_feature(range)
	{
		write<Sqe_set_feature::Cdw10::Fid>(Feature_fid::NUMQ);
		write<Cdw11::Nsqr>(num_queues-1);
		write<Cdw11::Ncqr>(num_queues-1);
	}
};


/*
 * Create completion queue command
 */
struct Nvme::Sqe_create_cq : Nvme::Sqe<0x30>
{
	struct Cdw10 : Register<0x28, 32>
	{
		struct Qid   : Bitfield< 0, 16> { }; /* queue identifier */
		struct Qsize : Bitfield<16, 16> { }; /* queue size 0-based vale */
	};

	struct Cdw11 : Register<0x2c, 32>
	{
		struct Pc : Bitfield< 0,  1> { }; /* physically contiguous */
		struct En : Bitfield< 1,  1> { }; /* interrupts enabled */
		struct Iv : Bitfield<16, 16> { }; /* interrupt vector */
	};

	Sqe_create_cq(Byte_range_ptr const &range) : Sqe(range) { }
};


/*
 * Create submission queue command
 */
struct Nvme::Sqe_create_sq : Nvme::Sqe<0x30>
{
	struct Cdw10 : Register<0x28, 32>
	{
		struct Qid   : Bitfield< 0, 16> { }; /* queue identifier */
		struct Qsize : Bitfield<16, 16> { }; /* queue size 0-based vale */
	};

	struct Cdw11 : Register<0x2c, 32>
	{
		struct Pc    : Bitfield< 0,  1> { }; /* physically contiguous */
		struct Qprio : Bitfield< 1,  2> { }; /* queue priority */
		struct Cqid  : Bitfield<16, 16> { }; /* completion queue identifier */
	};

	Sqe_create_sq(Byte_range_ptr const &range) : Sqe(range) { }
};


/*
 * I/O command
 */
struct Nvme::Sqe_io : Nvme::Sqe<0x34>
{
	struct Slba_lower : Register<0x28, 32> { };
	struct Slba_upper : Register<0x2c, 32> { };

	struct Cdw12 : Register<0x30, 32>
	{
		struct Deac : Bitfield<25,  1> { }; /* for WRITE_ZEROS needed by TRIM */
		struct Nlb  : Bitfield< 0, 16> { };
	};

	Sqe_io(Byte_range_ptr const &range) : Sqe(range) { }
};


/*
 * Queue base structure
 */
struct Nvme::Queue : Util::Dma_buffer
{
	size_t   len;
	uint32_t max_entries;

	Queue(Platform::Connection &platform,
	      uint32_t              max_entries,
	      size_t                len)
	:
		Dma_buffer(platform, len * max_entries),
		len(len), max_entries(max_entries) {};
};


/*
 * Submission queue
 */
struct Nvme::Sq : Nvme::Queue
{
	uint32_t tail { 0 };
	uint16_t id   { 0 };

	using Queue::Queue;

	Byte_range_ptr next()
	{
		char *a = local_addr<char>() + (tail * SQE_LEN);
		Genode::memset(a, 0, SQE_LEN);
		tail = (tail + 1) % max_entries;
		return {a, size()};
	}
};


/*
 * Completion queue
 */
struct Nvme::Cq : Nvme::Queue
{
	uint32_t head  { 0 };
	uint32_t phase { 1 };

	using Queue::Queue;

	Byte_range_ptr next()
	{
		off_t offset = head * CQE_LEN;
		return {local_addr<char>() + offset, size() - offset};
	}

	void advance_head()
	{
		if (++head >= max_entries) {
			head   = 0;
			phase ^= 1;
		}
	}
};


/*
 * I/O queue used by the Block_session_component
 */
struct Nvme::Io_queue : Noncopyable
{
	Io_queue_space::Element const _elem;

	struct Request
	{
		Block::Request block_request { };
		uint32_t       id            { 0 };
	};

	struct Command_id : Genode::Bit_allocator<Nvme::MAX_IO_ENTRIES>
	{
		bool used(uint16_t const cid) const
		{
			using BA = Bit_allocator<Nvme::MAX_IO_ENTRIES>;

			return BA::_array.get(cid, 1).template convert<bool>(
				[&] (bool used) { return used; },
				/* cannot happen as cid is capped to ENTRIES */
				[&] (BA::Error) { return false; });
		}
	};

	uint16_t _alloc_command_id()
	{
		return _command_id_allocator.alloc().convert<uint16_t>(
			[&] (addr_t cid) { return uint16_t(cid); },
			[&] (Command_id::Error) {
				/*
				 * Cannot happen because the acceptance check
				 * was successful and we are not called otherwise.
				*/
				return uint16_t(0);
			});
	}

	Command_id _command_id_allocator { };
	Request    _requests[Nvme::MAX_IO_ENTRIES] { };

	Util::Dma_buffer _dma_buffer;
	Util::Dma_buffer _prp_list_helper;

	Io_queue(Io_queue_space          &space,
	         Io_queue_space::Id       id,
	         Platform::Connection    &platform,
	         size_t                   tx_buf_size)
	:
		_elem            { *this, space, id },
		_dma_buffer      { platform, tx_buf_size },
		_prp_list_helper { platform, Nvme::PRP_DS_SIZE }
	{ }

	addr_t               dma_addr() const { return _dma_buffer.dma_addr(); }
	Dataspace_capability dma_cap()        { return _dma_buffer.cap(); }

	addr_t prp_dma_addr() const { return _prp_list_helper.dma_addr(); }
	addr_t prp_addr()     const { return (addr_t)_prp_list_helper.local_addr<void>(); }

	Io_queue_space::Id queue_id() const {  return _elem.id(); }

	uint16_t adopt_request(Block::Request const &request)
	{
		uint16_t const cid = _alloc_command_id();
		uint32_t const id  = cid | ((uint16_t)_elem.id().value<<16);

		_requests[cid] = Request {
			.block_request = request,
			.id            = id };

		return cid;
	}

	bool mark_completed_request(uint16_t cid, uint32_t id, bool success)
	{
		Request &r = _requests[cid];
		bool const valid = _command_id_allocator.used(cid) && r.id == id;
		if (valid)
			r.block_request.success = success;
		else
			error("Io_queue[", _elem.id().value, "]: ", cid,
			      "(", _command_id_allocator.used(cid), ") no pending request found "
			      "for CQ entry: id: ", id, " != r.id: ", r.id);

		return valid;
	}

	void with_completed_request(uint16_t cid, auto const &fn)
	{
		if (!_command_id_allocator.used(cid))
			return;

		fn(_requests[cid].block_request);

		_command_id_allocator.free(cid);
	}

	bool for_any_request(auto const &fn) const
	{
		for (uint16_t i = 0; i < Nvme::MAX_IO_ENTRIES; i++)
			if (_command_id_allocator.used(i) && fn(_requests[i].block_request))
				return true;

		return false;
	}
};


/*
 * Controller
 */
class Nvme::Controller : Platform::Device,
                         Platform::Device::Mmio<0x1010>,
                         Platform::Device::Irq
{
	using Mmio = Genode::Mmio<SIZE>;

	public:

	/**********
	 ** MMIO **
	 **********/

	/*
	 * Controller capabilities (p. 40 ff.)
	 */
	struct Cap : Register<0x0, 64>
	{
		struct Mqes   : Bitfield< 0, 15> { }; /* maximum queue entries supported 0-based */
		struct Cqr    : Bitfield<16,  1> { }; /* contiguous queues required */
		struct Ams    : Bitfield<17,  2> { }; /* arbitration mechanism supported */
		struct To     : Bitfield<24,  8> { }; /* timeout (csts.rdy) */
		struct Dstrd  : Bitfield<32,  4> { }; /* doorbell stride */
		struct Nssrs  : Bitfield<36,  1> { }; /* NVM subsystem reset supported */
		struct Css    : Bitfield<37,  8> { }; /* command sets supported */
		struct Bps    : Bitfield<45,  1> { }; /* boot partition support */
		struct Mpsmin : Bitfield<48,  4> { }; /* memory page size minimum */
		struct Mpsmax : Bitfield<52,  4> { }; /* memory page size maximum */
	};

	/*
	 * Version
	 */
	struct Vs : Register<0x8, 32>
	{
		struct Ter : Bitfield< 0,  8> { }; /* tertiary */
		struct Mnr : Bitfield< 8,  8> { }; /* minor */
		struct Mjr : Bitfield<16, 16> { }; /* major */
	};

	/*
	 * Interrupt mask set (for !MSI-X)
	 */
	struct Intms : Register<0x0c, 32>
	{
		struct Ivms : Bitfield<0, 32> { }; /* interrupt vector mask set */
	};

	/*
	 * Interrupt mask clear
	 */
	struct Intmc : Register<0x10, 32>
	{
		struct Ivmc : Bitfield<0, 32> { }; /* interrupt vector mask clear */
	};

	/*
	 * Controller configuration
	 */
	struct Cc : Register<0x14, 32>
	{
		struct En     : Bitfield< 0,  1> { }; /* enable */
		struct Css    : Bitfield< 4,  3> { }; /* I/O command set selected */
		struct Mps    : Bitfield< 7,  4> { }; /* memory page size */
		struct Ams    : Bitfield<11,  3> { }; /* arbitration mechanism selected */
		struct Shn    : Bitfield<14,  2> { }; /* shutdown notification */
		struct Iosqes : Bitfield<16,  4> { }; /* I/O submission queue entry size */
		struct Iocqes : Bitfield<20,  4> { }; /* I/O completion queue entry size */
	};

	/*
	 * Controller status
	 */
	struct Csts : Register<0x1c, 32>
	{
		struct Rdy   : Bitfield< 0,  1> { }; /* ready */
		struct Cfs   : Bitfield< 1,  1> { }; /* controller fatal status */
		struct Shst  : Bitfield< 2,  1> { }; /* shutdown status */
		struct Nssro : Bitfield< 4,  1> { }; /* NVM subsystem reset occurred */
		struct Pp    : Bitfield< 5,  1> { }; /* processing paused */
	};

	/*
	 * NVM subsystem reset
	 */
	struct Nssr : Register<0x20, 32>
	{
		struct Nssrc : Bitfield< 0, 32> { }; /* NVM subsystem reset control */
	};

	/*
	 * Admin queue attributes
	 */
	struct Aqa : Register<0x24, 32>
	{
		struct Asqs : Bitfield< 0, 12> { }; /* admin submission queue size 0-based */
		struct Acqs : Bitfield<16, 12> { }; /* admin completion queue size 0-based */
	};

	/*
	 * Admin submission queue base address
	 */
	struct Asq : Register<0x28, 64>
	{
		struct Asqb : Bitfield<12, 52> { }; /* admin submission queue base */
	};

	/*
	 * Admin completion queue base address
	 */
	struct Acq : Register<0x30, 64>
	{
		struct Acqb : Bitfield<12, 52> { }; /* admin completion queue base */
	};

	/*
	 * Controller memory buffer location
	 */
	struct Cmbloc : Register<0x38, 32>
	{
		struct Bir  : Bitfield< 0,  2> { }; /* base indicator register */
		struct Ofst : Bitfield<12, 24> { }; /* offset */
	};

	/*
	 * Controller memory buffer size
	 */
	struct Cmbsz : Register<0x3c, 32>
	{
		struct Sqs   : Bitfield< 0,  1> { }; /* submission queue support */
		struct Cqs   : Bitfield< 1,  1> { }; /* completion queue support */
		struct Lists : Bitfield< 2,  1> { }; /* PRP SGL list support */
		struct Rds   : Bitfield< 3,  1> { }; /* read data support */
		struct Wds   : Bitfield< 4,  1> { }; /* write data support */
		struct Szu   : Bitfield< 8,  4> { }; /* size units */
		struct Sz    : Bitfield<12, 24> { }; /* size */
	};

	/*
	 * Boot partition information
	 */
	struct Bpinfo : Register<0x40, 32>
	{
		struct Bpsz  : Bitfield< 0, 14> { }; /* boot partition size (in 128KiB) */
		struct Brs   : Bitfield<24,  2> { }; /* boot read status */
		struct Abpid : Bitfield<31,  1> { }; /* active boot partition id */
	};

	/*
	 * Boot partition read select
	 */
	struct Bprsel : Register<0x44, 32>
	{
		struct Bprsz : Bitfield< 0, 10> { }; /* boot partition read size (in 4KiB) */
		struct Bprof : Bitfield<10, 30> { }; /* boot partition read offset (in 4KiB) */
		struct Bpid  : Bitfield<31,  1> { }; /* boot partition identifier */
	};

	/*
	 * Boot partition memory buffer location
	 */
	struct Bpmbl : Register<0x48, 64>
	{
		struct Bmbba : Bitfield<12, 52> { }; /* boot partition memory buffer base address */
	};

	/*
	 * Admin submission doorbell
	 */
	struct Admin_sdb : Register<0x1000, 32>
	{
		struct Sqt : Bitfield< 0, 16> { }; /* submission queue tail */
	};

	/*
	 * Admin completion doorbell
	 */
	struct Admin_cdb : Register<0x1004, 32>
	{
		struct Cqh : Bitfield< 0, 16> { }; /* completion queue tail */
	};

	struct Initialization_failed : Genode::Exception { };
	struct Admin_command_failed  : Genode::Exception { };

	struct Info
	{
		Genode::String<8> version { };
		Identify_data::Sn sn { };
		Identify_data::Mn mn { };
		Identify_data::Fr fr { };
		size_t mdts { };

		uint32_t hmpre { };
		uint32_t hmmin { };
	};

	struct Nsinfo
	{
		uint64_t count { 0 };
		size_t   size  { 0 };
		uint64_t max_request_count { 0 };
		bool valid() const { return count && size; }
	};

	private:

	/**********
	 ** CODE **
	 **********/

	Genode::Env          &_env;
	Platform::Connection &_platform;
	Mmio::Delayer        &_delayer;

	/*
	 * There is a completion and submission queue for
	 * every namespace and one pair for the admin queues.
	 */
	Constructible<Nvme::Cq> _cq[NUM_QUEUES] { };
	Constructible<Nvme::Sq> _sq[NUM_QUEUES] { };

	Constructible<Nvme::Doorbell> _dbl[NUM_QUEUES] { };

	Constructible<Nvme::Cq> &_admin_cq = _cq[0];
	Constructible<Nvme::Sq> &_admin_sq = _sq[0];

	Util::Dma_buffer _nvme_identify { _platform, IDENTIFY_LEN };

	Genode::Constructible<Identify_data> _identify_data { };

	Util::Dma_buffer _nvme_nslist { _platform, IDENTIFY_LEN };
	uint32_t   _nvme_nslist_count { 0 };

	size_t _mdts_bytes { 0 };

	uint16_t _max_io_entries      { MAX_IO_ENTRIES };
	uint16_t _max_io_entries_mask { uint16_t(_max_io_entries - 1) };

	enum Cns {
		IDENTIFY_NS = 0x00,
		IDENTIFY    = 0x01,
		NSLIST      = 0x02,
	};

	enum {
		IDENTIFY_LEN = 4096,

		IDENTIFY_CID = 0x666,
		NSLIST_CID,
		QUERYNS_CID,
		CREATE_IO_CQ_CID,
		CREATE_IO_SQ_CID,
		DELETE_IO_CQ_CID,
		DELETE_IO_SQ_CID,
		SET_HMB_CID,
		SET_NUMQ_CID,
	};

	Constructible<Util::Dma_buffer> _nvme_query_ns[MAX_NS] { };

	struct Hmb_chunk
	{
		Registry<Hmb_chunk>::Element _elem;

		Util::Dma_buffer dma_buffer;

		Hmb_chunk(Registry<Hmb_chunk> &registry,
		          Platform::Connection &platform, size_t size)
		:
			_elem { registry, *this },
			dma_buffer { platform, size }
		{ }

		virtual ~Hmb_chunk() { }
	};

	struct Hmb_chunk_registry : Registry<Hmb_chunk>
	{
		Allocator &_alloc;

		Hmb_chunk_registry(Allocator &alloc)
		: _alloc { alloc } { }

		~Hmb_chunk_registry()
		{
			for_each([&] (Hmb_chunk &c) {
				destroy(_alloc, &c); });
		}
	};

	Heap                                 _hmb_alloc { _env.ram(), _env.rm() };
	Constructible<Hmb_chunk_registry>    _hmb_chunk_registry { };
	Constructible<Util::Dma_buffer>      _hmb_descr_list_buffer { };

	Info _info { };

	/* create larger array to use namespace id to as index */
	Nsinfo _nsinfo[MAX_NS+1] { };

	/**
	 * Wait for ready bit to change
	 *
	 * \param val  value of ready bit
	 *
	 * \throw Mmio::Polling_timeout
	 */
	void _wait_for_rdy(unsigned val)
	{
		enum { INTERVAL = 20'000u, TO_UNIT = 500'000u }; /* microseconds */

		unsigned     const to { (unsigned)read<Cap::To>() * TO_UNIT };
		Attempts     const a  { to / INTERVAL };
		Microseconds const t  { INTERVAL };

		try {
			wait_for(a, t, _delayer, Csts::Rdy::Equal(val));
		} catch (Mmio::Polling_timeout) {
			error("Csts::Rdy(", val, ") failed");
			throw;
		}
	}

	/**
	 * Reset controller
	 *
	 * \throw Initialization_failed
	 */
	void _reset()
	{
		/* disable intr and ctrlr */
		write<Intms>(1);
		write<Cc>(0);

		try { _wait_for_rdy(0); }
		catch (...) { throw Initialization_failed(); }

		/*
		 * For now we limit the memory page size to 4K because besides Qemu
		 * there are not that many consumer NVMe device that support larger
		 * page sizes and we do not want to align the DMA buffers to larger
		 * sizes. Essentially, we limit the memory page size to the statically
		 * defined Nvme::MPS.
		 */
		Cap::access_t const mpsmax = read<Cap::Mpsmax>();
		if (mpsmax > 0) { warning("ignore mpsmax:", mpsmax); }

		/* the value written to the register amounts to 2^(12 + v) bytes */
		Cap::access_t const v = Nvme::MPS_LOG2 - 12;
		write<Cc::Mps>(v);

		write<Cc::Iocqes>(CQE_LEN_LOG2);
		write<Cc::Iosqes>(SQE_LEN_LOG2);
	}

	/**
	 * Check if given queue tuple is full
	 *
	 * \param sq  reference to submission queue
	 * \param cq  reference to completion queue
	 *
	 * \return  returns true if queue is full and false otherwise
	 */
	bool _queue_full(Nvme::Sq const &sq, Nvme::Cq const &cq) const
	{
		return ((sq.tail + 1) & (_max_io_entries_mask)) == cq.head;
	}

	/**
	 * Setup admin queues
	 */
	void _setup_admin()
	{
		_admin_cq.construct(_platform, MAX_ADMIN_ENTRIES, CQE_LEN);
		write<Aqa::Acqs>(MAX_ADMIN_ENTRIES_MASK);
		write<Acq>(_admin_cq->dma_addr());

		_admin_sq.construct(_platform, MAX_ADMIN_ENTRIES, SQE_LEN);
		write<Aqa::Asqs>(MAX_ADMIN_ENTRIES_MASK);
		write<Asq>(_admin_sq->dma_addr());
	}

	/**
	 * Get address of the next free entry in the admin submission queue
	 *
	 * \param opc   entry opcode
	 * \param nsid  namespace identifier
	 * \param cid   command identifier
	 *
	 * \return  returns address of the next free entry or 0 if there is
	 *          no free entry
	 */
	Byte_range_ptr _admin_command(Opcode opc, uint32_t nsid, uint32_t cid)
	{
		if (_queue_full(*_admin_sq, *_admin_cq)) { return {nullptr, 0ul}; }

		Sqe_header b(_admin_sq->next());
		b.write<Nvme::Sqe_header::Cdw0::Opc>(opc);
		b.write<Nvme::Sqe_header::Cdw0::Cid>(cid);
		b.write<Nvme::Sqe_header::Nsid>(nsid);
		return b.range();
	}

	/**
	 * Wait until admin command has been finished
	 *
	 * In case the command was processed the 'fn' function is called
	 * and it is up to the caller to determine the result. Otherwise
	 * the 'to' function is called to denote the command was not
	 * processed in the given amount of time.
	 *
	 * This method should only be used in an synchronous fashion as
	 * batching admin commands could lead to out-of-order completions.
	 *
	 * \param num  number of attempts
	 * \param cid  command identifier
	 * \param fn   function called when the command has been finished
	 * \param to   function called when the command has not been finished
	 *             within the given number of attempts
	 */
	template <typename FN, typename TO>
	void _wait_for_admin_cq(uint32_t num, uint16_t cid,
	                        FN const &fn,
	                        TO const &timeout)
	{
		for (uint32_t i = 0; i < num; i++) {
			_delayer.usleep(50 * 1000);

			Cqe b(_admin_cq->next());

			if (b.read<Nvme::Cqe::Cid>() != cid)
				continue;

			_admin_cq->advance_head();

			/* do not spend too much time here */
			fn(b);

			write<Admin_cdb::Cqh>(_admin_cq->head);
			return;
		}

		timeout();
	}

	/**
	 * Wait until admin command has been finished
	 *
	 * A timed-out and an unsuccessful command are treated as the same.
	 *
	 * \param num  number of attempts
	 * \param cid  command identifier
	 *
	 * \return  returns true if the command was successfull, otherwise
	 *          false is returned
	 */
	bool _wait_for_admin_cq(uint32_t num, uint16_t cid)
	{
		bool success = false;

		_wait_for_admin_cq(num, cid,
			[&] (Cqe const &e) {
				success = Cqe::succeeded(e);
			},
			[&] () { /* already false */ }
		);

		return success;
	}

	/**
	 * Get list of namespaces
	 */
	void _query_nslist()
	{
		uint32_t *nslist = _nvme_nslist.local_addr<uint32_t>();

		bool const nsm = _identify_data->read<Identify_data::Oacs::Nsm>();
		if (!nsm) {
			nslist[0] = 1;
			_nvme_nslist_count = 1;
			return;
		}

		Sqe_identify b(_admin_command(Opcode::IDENTIFY, 0, NSLIST_CID));

		b.write<Nvme::Sqe_identify::Prp1>(_nvme_nslist.dma_addr());
		b.write<Nvme::Sqe_identify::Cdw10::Cns>(Cns::NSLIST);

		write<Admin_sdb::Sqt>(_admin_sq->tail);

		if (!_wait_for_admin_cq(10, NSLIST_CID)) {
			error("identify name space list failed");
			throw Initialization_failed();
		}

		for (size_t i = 0; i < 1024; i++) {
			if (nslist[i] == 0) { break; }
			++_nvme_nslist_count;
		}
	}

	/**
	 * Get information of namespaces
	 */
	void _query_ns()
	{
		uint32_t const max = _nvme_nslist_count > (uint32_t)MAX_NS ?
		                     (uint32_t)MAX_NS : _nvme_nslist_count;

		if (!max) {
			error("no name spaces found");
			throw Initialization_failed();
		}

		if (max > 1) { warning("only the first name space is used"); }

		uint32_t const *ns = _nvme_nslist.local_addr<uint32_t>();
		uint16_t const  id = 0;

		if (!_nvme_query_ns[id].constructed())
			_nvme_query_ns[id].construct(_platform, IDENTIFY_LEN);

		Sqe_identify b(_admin_command(Opcode::IDENTIFY, ns[id], QUERYNS_CID));
		b.write<Nvme::Sqe_identify::Prp1>(_nvme_query_ns[id]->dma_addr());
		b.write<Nvme::Sqe_identify::Cdw10::Cns>(Cns::IDENTIFY_NS);

		write<Admin_sdb::Sqt>(_admin_sq->tail);

		if (!_wait_for_admin_cq(10, QUERYNS_CID)) {
			error("identify name space failed");
			throw Initialization_failed();
		}

		Identify_ns_data nsdata({_nvme_query_ns[id]->local_addr<char>(), _nvme_query_ns[id]->size()});
		uint32_t const flbas = nsdata.read<Nvme::Identify_ns_data::Flbas::Formats>();

		/* use array subscription, omit first entry */
		uint16_t const ns_id = id + 1;

		_nsinfo[ns_id].count = nsdata.read<Nvme::Identify_ns_data::Nsze>();
		_nsinfo[ns_id].size  = 1u << nsdata.read<Nvme::Identify_ns_data::Lbaf::Lbads>(flbas);
		_nsinfo[ns_id].max_request_count = _mdts_bytes / _nsinfo[ns_id].size;
	}

	/**
	 * Query the controller information
	 */
	void _identify()
	{
		Sqe_identify b(_admin_command(Opcode::IDENTIFY, 0, IDENTIFY_CID));
		b.write<Nvme::Sqe_identify::Prp1>(_nvme_identify.dma_addr());
		b.write<Nvme::Sqe_identify::Cdw10::Cns>(Cns::IDENTIFY);

		write<Admin_sdb::Sqt>(_admin_sq->tail);

		if (!_wait_for_admin_cq(10, IDENTIFY_CID)) {
			error("identify failed");
			throw Initialization_failed();
		}

		_identify_data.construct(
			Byte_range_ptr(_nvme_identify.local_addr<char>(), _nvme_identify.size()));

		/* store information */
		_info.version = Genode::String<8>(read<Vs::Mjr>(), ".",
		                                  read<Vs::Mnr>(), ".",
		                                  read<Vs::Ter>());
		_info.sn = _identify_data->sn;
		_info.mn = _identify_data->mn;
		_info.fr = _identify_data->fr;

		_info.hmpre = _identify_data->read<Identify_data::Hmpre>();
		_info.hmmin = _identify_data->read<Identify_data::Hmmin>();

		/* limit maximum I/O request length */
		uint8_t const mdts = _identify_data->read<Identify_data::Mdts>();
		_mdts_bytes = !mdts ? (size_t)Nvme::MAX_IO_LEN
		                    : Genode::min((size_t)(1u << mdts) * Nvme::MPS,
		                                  (size_t)Nvme::MAX_IO_LEN);

		/* limit maximum queue length */
		uint16_t const mqes = read<Cap::Mqes>() + 1;
		_max_io_entries      = Genode::min((uint16_t)Nvme::MAX_IO_ENTRIES,
		                                   mqes);
		_max_io_entries_mask = _max_io_entries - 1;
	}

	/*
	 * Check units match at least hmmin and limit to hmpre or the
	 * amount of memory we can cover with our list and chunk size.
	 */
	uint32_t _check_hmb_units(uint32_t units)
	{
		if (!units) {
			if (_info.hmpre)
				warning("HMB support available but not configured");
			return 0;
		}

		units = align_addr(units, log2((unsigned)HMB_CHUNK_UNITS));

		if (units < _info.hmmin) {
			warning("HMB will not be enabled as configured size of ",
			         Number_of_bytes(units * Nvme::MPS),
			         " is less than minimal required amount of ",
			         Number_of_bytes(_info.hmmin * Nvme::MPS));
			return 0;
		}

		if (units > _info.hmpre)
			units = _info.hmpre;

		uint32_t const max_units = HMB_LIST_MAX_ENTRIES * HMB_CHUNK_UNITS;
		if (units > max_units)
			units = max_units;

		if (units < _info.hmpre)
			warning("HMB size of ",
			         Number_of_bytes(units * Nvme::MPS),
			         " is less than preferred amount of ",
			         Number_of_bytes(_info.hmpre * Nvme::MPS));

		return units;
	}

	/**
	 * Setup host-memory-buffer
	 *
	 * \param size  size of the HMB in bytes
	 */
	void _setup_hmb(size_t size)
	{
		uint32_t units = _check_hmb_units(uint32_t(size / Nvme::MPS));
		if (!units)
			return;

		uint32_t const bytes       = units * Nvme::MPS;
		uint32_t const num_entries = bytes / HMB_CHUNK_SIZE;

		try {
			_hmb_descr_list_buffer.construct(_platform, HMB_LIST_SIZE);
		} catch (... /* intentional catch-all */) {
			warning("could not allocate HMB descriptor list page");
			return;
		}

		_hmb_chunk_registry.construct(_hmb_alloc);

		Reconstructible<Byte_range_ptr> list
			{_hmb_descr_list_buffer->local_addr<char>(), _hmb_descr_list_buffer->size()};

		for (uint32_t i = 0; i < num_entries; i++) {
			try {
				Hmb_chunk *c =
					new (_hmb_alloc) Hmb_chunk(*_hmb_chunk_registry,
					                           _platform, HMB_CHUNK_SIZE);

				Hmb_de e(*list, c->dma_buffer.dma_addr(), HMB_CHUNK_UNITS);
				list.construct(list->start + Hmb_de::SIZE, list->num_bytes - Hmb_de::SIZE);

			} catch (... /* intentional catch-all */) {
				warning("could not allocate HMB chunk");

				/* if one allocation fails we bail entirely */
				_hmb_chunk_registry.destruct();
				_hmb_descr_list_buffer.destruct();
				return;
			}

		}

		Set_hmb b(_admin_command(Opcode::SET_FEATURES, 0, SET_HMB_CID),
		          _hmb_descr_list_buffer->dma_addr(), units, num_entries);

		write<Admin_sdb::Sqt>(_admin_sq->tail);

		if (!_wait_for_admin_cq(10, SET_HMB_CID)) {
			warning("could not enable HMB");

			_hmb_chunk_registry.destruct();
			_hmb_descr_list_buffer.destruct();
			return;
		}

		log("HMB enabled with ", Number_of_bytes(bytes), " in ",
		    num_entries, " chunks of ", Number_of_bytes(HMB_CHUNK_SIZE));
	}


	/**
	 * Set number of I/O submission and completion queues
	 */
	void _setup_numq(uint16_t num_queues)
	{
		Set_numq b(_admin_command(Opcode::SET_FEATURES, 0, SET_NUMQ_CID),
		           num_queues);

		write<Admin_sdb::Sqt>(_admin_sq->tail);

		_wait_for_admin_cq(10, SET_NUMQ_CID,
			[&] (Cqe const &e) {
				if (!Cqe::succeeded(e)) {
					warning("could not set number of queues");
					return;
				}

				uint32_t const dw0  = e.read<Cqe::Dw0>();
				uint16_t const nsqa = 1u + (dw0 & 0xffffu);
				uint16_t const ncqa = 1u + (dw0 & 0xffff0000u >> 16);
				log("Allocated number of submission: ",
				     nsqa, " and completion: ", ncqa, " queues");
				Cqe::dump(e);
			},
			[&] () { /* already false */ }
		);
	}


	/**
	 * Setup I/O completion queue
	 *
	 * \param id  identifier of the completion queue
	 *
	 * \throw Initialization_failed() in case the queue could not be created
	 */
	void _setup_io_cq(uint16_t id)
	{
		if (!_cq[id].constructed()) {
			_cq[id].construct(_platform, _max_io_entries, CQE_LEN);
			char *mmio_start = local_addr<char>() + 0x1000 + (id * 8);
			_dbl[id].construct(Byte_range_ptr(mmio_start, 8));
		}

		Nvme::Cq &cq = *_cq[id];

		Sqe_create_cq b(_admin_command(Opcode::CREATE_IO_CQ, 0, CREATE_IO_CQ_CID));
		b.write<Nvme::Sqe_create_cq::Prp1>(cq.dma_addr());
		b.write<Nvme::Sqe_create_cq::Cdw10::Qid>(id);
		b.write<Nvme::Sqe_create_cq::Cdw10::Qsize>(_max_io_entries_mask);
		b.write<Nvme::Sqe_create_cq::Cdw11::Pc>(1);
		b.write<Nvme::Sqe_create_cq::Cdw11::En>(1);

		write<Admin_sdb::Sqt>(_admin_sq->tail);

		if (!_wait_for_admin_cq(10, CREATE_IO_CQ_CID)) {
			error("create I/O cq failed");
			throw Initialization_failed();
		}
	}

	void _delete_io_cq(uint16_t id)
	{
		if (!_cq[id].constructed())
			return;

		Sqe_create_cq b(_admin_command(Opcode::DELETE_IO_CQ, 0, DELETE_IO_CQ_CID));
		b.write<Nvme::Sqe_create_cq::Cdw10::Qid>(id);

		write<Admin_sdb::Sqt>(_admin_sq->tail);

		if (!_wait_for_admin_cq(10, DELETE_IO_CQ_CID)) {
			error("delete I/O cq failed");
			throw Admin_command_failed();
		}

		_cq[id].destruct();
		_dbl[id].destruct();
	}

	/**
	 * Setup I/O submission queue
	 *
	 * \param id    identifier of the submission queue
	 * \param cqid  identifier of the completion queue
	 *
	 * \throw Initialization_failed() in case the queue could not be created
	 */
	void _setup_io_sq(uint16_t id, uint16_t cqid)
	{
		if (!_sq[id].constructed())
			_sq[id].construct(_platform, _max_io_entries, SQE_LEN);

		Nvme::Sq &sq = *_sq[id];

		Sqe_create_sq b(_admin_command(Opcode::CREATE_IO_SQ, 0, CREATE_IO_SQ_CID));
		b.write<Nvme::Sqe_create_sq::Prp1>(sq.dma_addr());
		b.write<Nvme::Sqe_create_sq::Cdw10::Qid>(id);
		b.write<Nvme::Sqe_create_sq::Cdw10::Qsize>(_max_io_entries_mask);
		b.write<Nvme::Sqe_create_sq::Cdw11::Pc>(1);
		b.write<Nvme::Sqe_create_sq::Cdw11::Qprio>(0b00); /* urgent for now */
		b.write<Nvme::Sqe_create_sq::Cdw11::Cqid>(cqid);

		write<Admin_sdb::Sqt>(_admin_sq->tail);

		if (!_wait_for_admin_cq(10, CREATE_IO_SQ_CID)) {
			error("delete I/O sq failed");
			throw Initialization_failed();
		}
	}

	/**
	 * Free I/O submission queue
	 *
	 * \param id  identifier of the submission queue
	 *
	 * \throw Admin_command_failed() in case the queue could not be deleted
	 */
	void _delete_io_sq(uint16_t id)
	{
		if (!_sq[id].constructed())
			return;

		Sqe_create_sq b(_admin_command(Opcode::DELETE_IO_SQ, 0, DELETE_IO_SQ_CID));
		b.write<Nvme::Sqe_create_sq::Cdw10::Qid>(id);

		write<Admin_sdb::Sqt>(_admin_sq->tail);

		if (!_wait_for_admin_cq(10, DELETE_IO_SQ_CID)) {
			error("delete I/O sq failed");
			throw Admin_command_failed();
		}

		_sq[id].destruct();
	}

	public:

	/**
	 * Constructor
	 */
	Controller(Genode::Env              &env,
	           Platform::Connection     &platform,
	           Mmio::Delayer            &delayer,
	           Signal_context_capability irq_sigh)
	:
		Platform::Device(platform),
		Platform::Device::Mmio<SIZE>((Platform::Device&)*this),
		Platform::Device::Irq((Platform::Device&)*this),
		_env(env), _platform(platform), _delayer(delayer)
	{
		sigh(irq_sigh);
	}

	/**
	 * Initialize controller
	 *
	 * \throw Initialization_failed
	 */
	void init()
	{
		_reset();
		_setup_admin();

		write<Cc::En>(1);

		try { _wait_for_rdy(1); }
		catch (...) {
			if (read<Csts::Cfs>()) {
				error("fatal controller status");
			}
			throw Initialization_failed();
		}

		clear_intr();
	}

	/**
	 * Mask interrupts
	 */
	void mask_intr() { write<Intms>(1); }

	/**
	 * Clean interrupts
	 */
	void clear_intr() { write<Intmc>(1); }

	/**
	 * Acknowledge interrupt
	 */
	void ack_irq() { Platform::Device::Irq::ack(); }

	/*
	 * Identify NVM system
	 */
	void identify()
	{
		_identify();
		_query_nslist();
		_query_ns();
	}

	/**
	 * Setup HMB
	 */
	void setup_hmb(size_t bytes)
	{
		_setup_hmb(bytes);
	}

	/**
	 * Set NUMQ
	 */
	void setup_numq(uint16_t num_queues)
	{
		_setup_numq(num_queues);
	}

	/**
	 * Setup I/O queue
	 */
	void setup_io(Io_queue_space::Id cid, Io_queue_space::Id sid)
	{
		uint16_t const cid_value = uint16_t(cid.value);
		uint16_t const sid_value = uint16_t(sid.value);
		_setup_io_cq(cid_value);
		_setup_io_sq(sid_value, cid_value);
	}

	/**
	 * Delete I/O queue
	 */
	void delete_io(Io_queue_space::Id cid, Io_queue_space::Id sid)
	{
		uint16_t const cid_value = uint16_t(cid.value);
		uint16_t const sid_value = uint16_t(sid.value);
		_delete_io_sq(sid_value);
		_delete_io_cq(cid_value);
	}

	/**
	 * Query if I/O queue is used
	 */
	bool active_io(Io_queue_space::Id qid)
	{
		/* SQ implies working CQ */
		uint16_t const qid_value = uint16_t(qid.value);
		return _sq[qid_value].constructed();
	}

	/**
	 * Get next free IO submission queue slot
	 *
	 * \param qid  queue identifier
	 *
	 * \return  returns virtual address of the I/O command
	 */
	Byte_range_ptr io_command(Io_queue_space::Id qid, uint16_t cid)
	{
		uint16_t const qid_value = uint16_t(qid.value);
		Nvme::Sq &sq = *_sq[qid_value];

		Sqe_header e(sq.next());
		e.write<Nvme::Sqe_header::Cdw0::Cid>(cid);
		e.write<Nvme::Sqe_header::Nsid>(IO_NSID);
		return e.range();
	}

	/**
	 * Check if I/O queue is full
	 *
	 * \param qid  queue identifier
	 *
	 * \return  true if full, otherwise false
	 */
	bool io_queue_full(Io_queue_space::Id qid) const
	{
		uint16_t const qid_value = uint16_t(qid.value);
		Nvme::Sq const &sq = *_sq[qid_value];
		Nvme::Cq const &cq = *_cq[qid_value];
		return _queue_full(sq, cq);
	}

	/**
	 * Write current I/O submission queue tail
	 *
	 * \param qid  queue identifier
	 */
	void commit_io(Io_queue_space::Id qid)
	{
		uint16_t const qid_value = uint16_t(qid.value);
		Nvme::Sq       &sq  = *_sq [qid_value];
		Nvme::Doorbell &dbl = *_dbl[qid_value];

		dbl.write<Nvme::Doorbell::Sqtdbl::Sqt>(sq.tail);
	}

	/**
	 * Process a pending I/O completion
	 *
	 * \param qid   queue identifier
	 * \param func  function that is called on each completion
	 */
	template <typename FUNC>
	void handle_io_completion(Io_queue_space::Id qid, FUNC const &func)
	{
		uint16_t const qid_value = uint16_t(qid.value);
		if (!_cq[qid_value].constructed())
			return;

		Nvme::Cq &cq = *_cq[qid_value];

		do {
			Cqe e(cq.next());

			/* process until old phase */
			if (e.read<Nvme::Cqe::Sf::P>() != cq.phase) { break; }

			func(e);

			cq.advance_head();

			/*
			 * Acknowledging the completions is done below,
			 * so that we can handle them batch-wise.
			 */
		} while(0);
	}

	/**
	 * Acknowledge every pending I/O already handled
	 *
	 * \param qid  queue identifier
	 */
	void ack_io_completions(Io_queue_space::Id qid)
	{
		uint16_t const qid_value = uint16_t(qid.value);
		Nvme::Cq        &cq = *_cq [qid_value];
		Nvme::Doorbell &dbl = *_dbl[qid_value];

		dbl.write<Nvme::Doorbell::Cqhdbl::Cqh>(cq.head);
	}

	/**
	 * Get block metrics of namespace
	 *
	 * \param nsid  namespace identifier
	 *
	 * \return  returns information of the namespace
	 */
	Nsinfo nsinfo(uint16_t nsid)
	{
		return _nsinfo[nsid];
	}

	/**
	 * Get controller information
	 *
	 * \return  returns controller information
	 */
	Info const &info() const { return _info; }

	/**
	 * Get supported maximum number of blocks per request for namespace
	 *
	 * \param nsid  namespace identifier
	 *
	 * \return  returns maximal count of blocks in one request
	 */
	Block::block_count_t max_count(uint16_t nsid) const
	{
		/*
		 * Limit to block_count_t which differs between 32 and 64 bit
		 * systems.
		 */
		return (Block::block_count_t)_nsinfo[nsid].max_request_count;
	}

	/**
	 * Get number of slots in the I/O queue
	 *
	 * \return  returns maximal number of I/O requests
	 */
	uint16_t max_io_entries() const { return _max_io_entries; }

	/***********
	 ** Debug **
	 ***********/

	void dump_cap()
	{
		log("CAP:", "  ",
		    "Mqes:",   read<Cap::Mqes>()+1, " ",
		    "Cqr:",    read<Cap::Cqr>(),    " ",
		    "Ams:",    read<Cap::Ams>(),    " ",
		    "To:",     read<Cap::To>(),     " ",
		    "Dstrd:",  read<Cap::Dstrd>(),  " ",
		    "Nssrs:",  read<Cap::Nssrs>(),  " ",
		    "Css:",    read<Cap::Css>(),    " ",
		    "Bps:",    read<Cap::Bps>(),    " ",
		    "Mpsmin:", read<Cap::Mpsmin>(), " ",
		    "Mpsmax:", read<Cap::Mpsmax>());

		log("VS: ", "  ", read<Vs::Mjr>(), ".",
		    read<Vs::Mnr>(), ".", read<Vs::Ter>());
	}

	void dump_identify()
	{
		log("vid:", Hex(_identify_data->read<Identify_data::Vid>()));
		log("ssvid:", Hex(_identify_data->read<Identify_data::Ssvid>()));
		log("oacs:", Hex(_identify_data->read<Identify_data::Oacs>()));
		log("  nsm:", Hex(_identify_data->read<Identify_data::Oacs::Nsm>()));
		log("sn:'", _identify_data->sn.string(), "'");
		log("mn:'", _identify_data->mn.string(), "'");
		log("fr:'", _identify_data->fr.string(), "'");
		log("nn:",   _identify_data->read<Identify_data::Nn>());
		log("vwc:",  _identify_data->read<Identify_data::Vwc>());
		log("mdts:", _identify_data->read<Identify_data::Mdts>());
		log("hmpre:", _identify_data->read<Identify_data::Hmpre>());
		log("hmmin:", _identify_data->read<Identify_data::Hmmin>());
	}

	void dump_nslist()
	{
		uint32_t const *p = _nvme_nslist.local_addr<uint32_t>();
		if (!p) { return; }

		for (size_t i = 0; i < 1024; i++) {
			if (p[i] == 0) { break; }
			log("ns:#", p[i], " found");
		}
	}
};


struct Nvme::Block_session_component : Rpc_object<Block::Session>,
                                       Block::Request_stream
{
	Env &_env;

	Session_space::Element const _elem;

	Io_queue_space::Id const _queue_id;

	Block_session_component(Session_space              &space,
	                        Env                        &env,
	                        Io_queue_space::Id          queue_id,
	                        Dataspace_capability        queue_dma_cap,
	                        Signal_context_capability   sigh,
	                        Block::Session::Info        info,
	                        Block::Constrained_view     view,
	                        uint16_t                    session_id)
	:
		Request_stream { env.rm(), queue_dma_cap, env.ep(), sigh,
		                 info, view },
		_env         { env },
		_elem        { *this, space, { .value = session_id } },
		_queue_id    { queue_id }
	{
		_env.ep().manage(*this);
	}

	~Block_session_component() { _env.ep().dissolve(*this); }

	Info info() const override { return Request_stream::info(); }

	Capability<Tx> tx_cap() override { return Request_stream::tx_cap(); }

	Session_space::Id session_id() const { return _elem.id(); }

	Io_queue_space::Id queue_id() const { return _queue_id; }
};


/******************
 ** Block driver **
 ******************/

class Nvme::Driver : Genode::Noncopyable
{
	public:

		bool _verbose_checks   { false };
		bool _verbose_identify { false };
		bool _verbose_io       { false };
		bool _verbose_regs     { false };

		size_t _hmb_size { 0 };

		struct Io_error           : Genode::Exception { };
		struct Request_congestion : Genode::Exception { };

	private:

		Driver(const Driver&) = delete;
		Driver& operator=(const Driver&) = delete;

		Genode::Env          &_env;
		Platform::Connection  _platform { _env };
		Sliced_heap           _sliced_heap { _env.ram(), _env.rm() };

		Genode::Attached_rom_dataspace &_config_rom;

		Constructible<Attached_rom_dataspace> _system_rom { };
		Signal_handler<Driver>  _system_rom_sigh {
			_env.ep(), *this, &Driver::_system_update };

		void _handle_config_update()
		{
			_config_rom.update();

			if (!_config_rom.valid()) { return; }

			Genode::Node config = _config_rom.node();
			_verbose_checks   = config.attribute_value("verbose_checks",   _verbose_checks);
			_verbose_identify = config.attribute_value("verbose_identify", _verbose_identify);
			_verbose_io       = config.attribute_value("verbose_io",       _verbose_io);
			_verbose_regs     = config.attribute_value("verbose_regs",     _verbose_regs);

			_hmb_size = config.attribute_value("max_hmb_size", Genode::Number_of_bytes(0));

			if (config.attribute_value("system", false)) {
				_system_rom.construct(_env, "system");
				_system_rom->sigh(_system_rom_sigh);
			} else
				_system_rom.destruct();
		}

		Genode::Signal_handler<Driver> _config_sigh {
			_env.ep(), *this, &Driver::_handle_config_update };

		/**************
		 ** Reporter **
		 **************/

		Genode::Reporter _namespace_reporter { _env, "controller" };

		void _report_namespaces(Nvme::Controller &ctrlr)
		{
			try {
				(void)_namespace_reporter.generate([&] (Generator &g) {
					Nvme::Controller::Info const &info = ctrlr.info();

					g.attribute("serial", info.sn);
					g.attribute("model",  info.mn);

					Nvme::Controller::Nsinfo ns = ctrlr.nsinfo(Nvme::IO_NSID);

					g.node("namespace", [&]() {
						g.attribute("id",          (uint16_t)Nvme::IO_NSID);
						g.attribute("block_size",  ns.size);
						g.attribute("block_count", ns.count);
					});
				});
			} catch (...) { }
		}

		uint64_t _submits_in_flight { };

		bool _submits_pending   { false };
		bool _stop_processing   { false };

		/*********************
		 ** MMIO Controller **
		 *********************/

		struct Timer_delayer : Genode::Mmio<0>::Delayer,
		                       Timer::Connection
		{
			Timer_delayer(Genode::Env &env)
			: Timer::Connection(env) { }

			void usleep(uint64_t us) override { Timer::Connection::usleep(us); }
		} _delayer { _env };

		Signal_context_capability const _irq_sigh;
		Signal_context_capability const _restart_sigh;

		Reconstructible<Nvme::Controller> _nvme_ctrlr { _env, _platform,
		                                                _delayer, _irq_sigh };

		/***********
		 ** Block **
		 ***********/

		Block::Session::Info _info { };

	public:

		/**
		 * Constructor
		 */
		Driver(Genode::Env                       &env,
		       Genode::Attached_rom_dataspace    &config_rom,
		       Genode::Signal_context_capability  irq_sigh,
		       Genode::Signal_context_capability  restart_sigh)
		: _env(env),
		  _config_rom(config_rom),
		  _irq_sigh(irq_sigh),
		  _restart_sigh(restart_sigh)
		{
			_config_rom.sigh(_config_sigh);
			_handle_config_update();

			reinit(*_nvme_ctrlr);
		}

		void with_controller(auto const &fn)
		{
			if (_nvme_ctrlr.constructed())
				fn(*_nvme_ctrlr);
		}

		void reinit(Nvme::Controller &ctrlr)
		{
			/*
			 * Setup and identify NVMe PCI controller
			 */

			if (_verbose_regs) { ctrlr.dump_cap(); }

			ctrlr.init();
			ctrlr.identify();

			if (_verbose_identify) {
				ctrlr.dump_identify();
				ctrlr.dump_nslist();
			}

			/*
			 * Setup HMB
			 */
			if (ctrlr.info().hmpre)
				ctrlr.setup_hmb(_hmb_size);

			/*
			 * Setup maximal number of SQ/CQ
			 */
			ctrlr.setup_numq(64);

			/*
			 * Setup I/O
			 */

			/* skip the admin SQ/CQ and reinit all other used queues */
			for (uint16_t qid_value = 1; qid_value < NUM_QUEUES; qid_value++) {
				Io_queue_space::Id const qid { .value = qid_value };
				if (!ctrlr.active_io(qid))
					continue;

				ctrlr.setup_io(qid, qid);
			}

			/*
			 * Setup Block session
			 */

			/* set Block session properties */
			Nvme::Controller::Nsinfo nsinfo = ctrlr.nsinfo(Nvme::IO_NSID);
			if (!nsinfo.valid()) {
				error("could not query namespace information");
				throw Nvme::Controller::Initialization_failed();
			}

			_info = { .block_size  = nsinfo.size,
			          .block_count = nsinfo.count,
			          .align_log2  = Nvme::MPS_LOG2,
			          .writeable   = true };

			Nvme::Controller::Info const &info = ctrlr.info();

			log("NVMe:",  info.version.string(),   " "
			    "serial:'", info.sn.string(), "'", " "
			    "model:'",  info.mn.string(), "'", " "
			    "frev:'",   info.fr.string(), "'");

			log("Block", " "
			    "size: ",  _info.block_size, " "
			    "count: ", _info.block_count, " "
			    "I/O entries: ", ctrlr.max_io_entries());

			/* generate Report if requested */
			_config_rom.node().with_optional_sub_node("report",
				[&] (Node const &report) {
					if (report.attribute_value("namespaces", false)) {
						_namespace_reporter.enabled(true);
						_report_namespaces(ctrlr);
					}
				});
		}

		~Driver() { /* free resources */ }

		Block::Session::Info info() const { return _info; }

		void writeable(bool writeable) { _info.writeable = writeable; }

		void device_release_if_stopped_and_idle()
		{
			if (_stop_processing && _submits_in_flight == 0) {
				_nvme_ctrlr.destruct();
			}
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

			bool const resume_driver =  _stop_processing && state == "";
			bool const stop_driver   = !_stop_processing && state != "";

			if (stop_driver) {
				_stop_processing = true;
				device_release_if_stopped_and_idle();

				log("driver halted");

				return;
			}

			if (resume_driver) {
				_stop_processing = false;

				_nvme_ctrlr.construct(_env, _platform, _delayer, _irq_sigh);
				reinit(*_nvme_ctrlr);

				log("driver resumed");

				/* restart block session handling */
				Signal_transmitter(_restart_sigh).submit();

				return;
			}
		}

		/******************************
		 ** Block request stream API **
		 ******************************/

		Response _check_acceptance(Io_queue         const &io_queue,
		                           Block::Request          request,
		                           Nvme::Controller const &ctrlr) const
		{
			/*
			 * All memory is dimensioned in a way that it will allow for
			 * MAX_IO_ENTRIES requests, so it is safe to only check the
			 * I/O queue.
			 */
			if (ctrlr.io_queue_full(io_queue.queue_id()))
				return Response::RETRY;

			if (!Genode::aligned(request.offset, Nvme::MPS_LOG2))
				return Response::REJECTED;

			switch (request.operation.type) {
			case Block::Operation::Type::INVALID:
				return Response::REJECTED;

			case Block::Operation::Type::SYNC:
				return Response::ACCEPTED;

			case Block::Operation::Type::TRIM:  [[fallthrough]];
			case Block::Operation::Type::WRITE: [[fallthrough]];
			case Block::Operation::Type::READ:
				/* limit request to what we can handle, needed for overlap check */
				if (request.operation.count > ctrlr.max_count(Nvme::IO_NSID))
					request.operation.count = ctrlr.max_count(Nvme::IO_NSID);
			}

			size_t          const count   = request.operation.count;
			Block::sector_t const lba     = request.operation.block_number;
			Block::sector_t const lba_end = lba + count - 1;

			// XXX trigger overlap only in case of mixed read and write requests?
			auto overlap_check = [&] (Block::Request const &req) {
				Block::sector_t const start = req.operation.block_number;
				Block::sector_t const end   = start + req.operation.count - 1;

				bool const in_req    = (lba >= start && lba_end <= end);
				bool const over_req  = (lba <= start && lba_end <= end) &&
				                       (start >= lba && start <= lba_end);
				bool const cross_req = (lba <= start && lba_end >= end);
				bool const overlap   = (in_req || over_req || cross_req);

				if (_verbose_checks && overlap) {
					warning("overlap: ", "[", lba, ",", lba_end, ") with "
					        "[", start,   ",", end, ")",
					        " ", in_req, " ", over_req, " ", cross_req);
				}
				return overlap;
			};

			if (io_queue.for_any_request(overlap_check))
				return Response::RETRY;

			return Response::ACCEPTED;
		}

		uint16_t _submit(Io_queue           &io_queue,
		                 Io_queue           &block_io_queue,
		                 Block::Request      request,
		                 Nvme::Controller   &ctrlr)
		{
			bool const write =
				request.operation.type == Block::Operation::Type::WRITE;

			/* limit request to what we can handle */
			if (request.operation.count > ctrlr.max_count(Nvme::IO_NSID))
				request.operation.count = ctrlr.max_count(Nvme::IO_NSID);

			uint32_t        const count = (uint32_t)request.operation.count;
			Block::sector_t const lba   = request.operation.block_number;

			size_t const len        = request.operation.count * _info.block_size;
			bool   const need_list  = len > 2 * Nvme::MPS;
			addr_t const request_pa = block_io_queue.dma_addr() + request.offset;

			if (_verbose_io) {
				log("Submit: ", write ? "WRITE" : "READ",
				    " len: ", len, " mps: ", (unsigned)Nvme::MPS,
				    " need_list: ", need_list,
				    " block count: ", count,
				    " lba: ", lba,
				    " dma_base: ", Hex(block_io_queue.dma_addr()),
				    " offset: ", Hex(request.offset));
			}

			uint16_t const cid = io_queue.adopt_request(request);

			Nvme::Sqe_io b(ctrlr.io_command(io_queue.queue_id(), cid));
			Nvme::Opcode const op = write ? Nvme::Opcode::WRITE : Nvme::Opcode::READ;
			b.write<Nvme::Sqe_io::Cdw0::Opc>(op);
			b.write<Nvme::Sqe_io::Prp1>(request_pa);

			/* payload will fit into 2 mps chunks */
			if (len > Nvme::MPS && !need_list) {
				b.write<Nvme::Sqe_io::Prp2>(request_pa + Nvme::MPS);
			} else if (need_list) {

				/* get page to store list of mps chunks */
				addr_t const offset = cid * Nvme::MPS;
				addr_t pa = block_io_queue.prp_dma_addr() + offset;
				addr_t va = block_io_queue.prp_addr() + offset;

				/* omit first page and write remaining pages to iob */
				addr_t  npa = request_pa + Nvme::MPS;
				using Page_entry = uint64_t;
				Page_entry *pe  = (Page_entry*)va;

				size_t const mps_len = Genode::align_addr(len, Nvme::MPS_LOG2);
				size_t const num     = (mps_len - Nvme::MPS) / Nvme::MPS;
				if (_verbose_io) {
					log("  page.va: ", Hex(va), " page.pa: ",
					    Hex(pa), " num: ", num);
				}

				for (size_t i = 0; i < num; i++) {
					if (_verbose_io) {
						log("    [", i, "]: ", Hex(npa));
					}
					pe[i] = npa;
					npa  += Nvme::MPS;
				}
				b.write<Nvme::Sqe_io::Prp2>(pa);
			}

			b.write<Nvme::Sqe_io::Slba_lower>(uint32_t(lba));
			b.write<Nvme::Sqe_io::Slba_upper>(uint32_t(lba >> 32u));
			b.write<Nvme::Sqe_io::Cdw12::Nlb>(count - 1); /* 0-base value */

			return cid;
		}

		uint16_t _submit_sync(Io_queue             &io_queue,
		                      Block::Request const &request,
		                      Nvme::Controller     &ctrlr)
		{
			uint16_t const cid = io_queue.adopt_request(request);

			Nvme::Sqe_io b(ctrlr.io_command(io_queue.queue_id(), cid));
			b.write<Nvme::Sqe_io::Cdw0::Opc>(Nvme::Opcode::FLUSH);

			return cid;
		}

		uint16_t _submit_trim(Io_queue             &io_queue,
		                      Block::Request const &request,
		                      Nvme::Controller     &ctrlr)
		{
			uint16_t const cid = io_queue.adopt_request(request);

			uint32_t        const count = (uint32_t)request.operation.count;
			Block::sector_t const lba   = request.operation.block_number;

			Nvme::Sqe_io b(ctrlr.io_command(io_queue.queue_id(), cid));
			b.write<Nvme::Sqe_io::Cdw0::Opc>(Nvme::Opcode::WRITE_ZEROS);
			b.write<Nvme::Sqe_io::Slba_lower>(uint32_t(lba));
			b.write<Nvme::Sqe_io::Slba_upper>(uint32_t(lba >> 32u));

			/*
			 * XXX For now let the device decide if it wants to deallocate
			 *     the blocks or not.
			 *
			 * b.write<Nvme::Sqe_io::Cdw12::Deac>(1);
			 */
			b.write<Nvme::Sqe_io::Cdw12::Nlb>(count - 1); /* 0-base value */

			return cid;
		}

		/**********************
		 ** driver interface **
		 **********************/

		Response _submit_io(Nvme::Controller     &ctrlr,
		                    Io_queue             &io_queue,
		                    Io_queue             &block_io_queue,
		                    Block::Request const &request,
		                    uint16_t             &cid)
		{
			Response result = Response::RETRY;

			if (_stop_processing)
				return result;

			result = _check_acceptance(io_queue, request, ctrlr);

			if (result == Response::ACCEPTED) {
				switch (request.operation.type) {
				case Block::Operation::Type::READ: [[fallthrough]];
				case Block::Operation::Type::WRITE:
					cid = _submit(io_queue, block_io_queue, request, ctrlr);
					break;
				case Block::Operation::Type::SYNC:
					cid = _submit_sync(io_queue, request, ctrlr);
					break;
				case Block::Operation::Type::TRIM:
					cid = _submit_trim(io_queue, request, ctrlr);
					break;
				default:
					break;
				}

				_submits_in_flight ++;
				_submits_pending = true;
			}

			return result;
		}

		Response submit(Nvme::Controller     &ctrlr,
		                Io_queue             &io_queue,
		                Block::Request const &request)
		{
			uint16_t cid = 0;
			return _submit_io(ctrlr, io_queue, io_queue, request, cid);
		}

		Response submit_sq(Nvme::Controller     &ctrlr,
		                   Io_queue             &io_queue,
		                   Io_queue             &block_io_queue,
		                   Block::Request const &request,
		                   uint16_t             &cid)
		{
			return _submit_io(ctrlr, io_queue, block_io_queue, request, cid);
		}

		bool commit_pending_submits(Nvme::Controller &ctrlr, Io_queue &io_queue)
		{
			if (!_submits_pending) { return false; }

			ctrlr.commit_io(io_queue.queue_id());
			_submits_pending = false;
			return true;
		}

		void with_any_completed_job(Nvme::Controller &ctrlr,
		                            Io_queue         &io_queue,
		                            auto       const &fn)
		{
			ctrlr.handle_io_completion(io_queue.queue_id(), [&] (Nvme::Cqe const &b) {

				if (_verbose_io) { Nvme::Cqe::dump(b); }

				if (_stop_processing)
					error("_get_completed request and ", _submits_in_flight);

				uint32_t const id  = Nvme::Cqe::request_id(b);
				uint16_t const cid = Nvme::Cqe::command_id(b);

				// TODO move handling out of the driver
				bool const matching_request =
					io_queue.mark_completed_request(cid, id, Nvme::Cqe::succeeded(b));

				if (!matching_request) {
					Nvme::Cqe::dump(b);
					return;
				}

				fn(cid);

				if (_submits_in_flight)
					_submits_in_flight--;
			});
		}

		Nvme::Io_queue::Command_id _io_queue_map { };
		Io_queue_space             _io_queue_space { };

		struct Io_queue_creation_error { };
		using Io_queue_create_result = Attempt<Io_queue_space::Id,
		                                       Io_queue_creation_error>;

		Io_queue_create_result create_io_queue(Nvme::Controller &ctrlr,
		                                       size_t tx_buf_size)
		{
			return _io_queue_map.alloc().convert<Io_queue_create_result>(
				[&] (addr_t value) {

					Io_queue_space::Id const new_id { .value = uint16_t(value + 1) };
					try {
						ctrlr.setup_io(new_id, new_id);

						new (_sliced_heap) Io_queue(_io_queue_space, new_id,
						                            _platform, tx_buf_size);
						return Io_queue_create_result { new_id };
					} catch (Nvme::Controller::Initialization_failed) {
						_io_queue_map.free(new_id.value - 1); }

					return Io_queue_create_result { Io_queue_creation_error { } };
				},
				[&] (Nvme::Io_queue::Command_id::Error) {
					/* max I/O queues reached */
					error("max I/O queues reached");
					return Io_queue_creation_error();
				});
		}

		void free_io_queue(Nvme::Controller &ctrlr, Io_queue_space::Id id)
		{
			_io_queue_space.apply<Io_queue>(id, [&] (Io_queue &io_queue) {
				destroy(_sliced_heap, &io_queue);

				ctrlr.delete_io(id, id);
				_io_queue_map.free(uint16_t(id.value - 1));
			});
		}

		void with_io_queue(Io_queue_space::Id queue_id, auto const &fn)
		{
			_io_queue_space.apply<Io_queue>(queue_id,
				[&] (Io_queue &io_queue) { fn(io_queue); });
		}
};


/**********
 ** Main **
 **********/

struct Nvme::Main : Rpc_object<Typed_root<Block::Session>>
{
	Genode::Env &_env;

	Sliced_heap _sliced_heap { _env.ram(), _env.rm() };

	Genode::Attached_rom_dataspace _config_rom { _env, "config" };

	Signal_handler<Main> _request_handler { _env.ep(), *this, &Main::_handle_requests };
	Signal_handler<Main> _irq_handler     { _env.ep(), *this, &Main::_handle_irq };

	Nvme::Driver _driver { _env, _config_rom, _irq_handler, _request_handler };

	using Session_space = Id_space<Block_session_component>;
	Session_space _sessions { };

	using Session_map = Block::Session_map<>;
	Session_map _session_map { };

	struct Session_command;
	using Session_command_space = Id_space<Session_command>;
	struct Session_command : Id_space<Session_command>::Element
	{
		Session_space::Id const session_id;

		Session_command(Session_command_space &space,
		                Session_space::Id      session_id,
		                uint16_t               command_id)
		:
			Element { *this, space,
			          Session_command_space::Id { .value = command_id } },
			session_id { session_id }
		{ }

		void print(Genode::Output &out) const
		{
			Genode::print(out, " session_id: ", session_id.value, " command_id: ", id().value);
		}
	};
	Session_command_space _session_commands { };
	Tslab<Session_command,sizeof(Session_command)*32> _session_commands_slab { _sliced_heap };

	bool const _force_sq { _config_rom.xml().attribute_value("force_sq", false) };

	void _handle_irq()
	{
		if (!_force_sq) _handle_requests();
		else            _handle_requests_sq();

		_driver.with_controller([&] (auto & ctrlr) {
			ctrlr.ack_irq(); });
	}

	void _handle_requests_sq()
	{
		_driver.with_io_queue(Io_queue_space::Id{1u}, [&] (Io_queue &io_queue) {
			for (;;) {
				bool progress = false;

				/* acknowledge completed */
				auto completed_job = [&] (uint16_t cid) {
					Session_command_space::Id const id { .value = cid };
					auto get_session = [&] (Session_command &session_command) {
						auto get_block = [&] (Block_session_component &block_session) {
							bool command_handled = false;
							auto acknowledge_job = [&] (Block_session_component::Ack &ack) {
								auto complete_fn = [&] (Block::Request &request) {
									ack.submit(request);
									progress = true;
									command_handled = true;
								};
								io_queue.with_completed_request(cid, complete_fn);
							};
							block_session.try_acknowledge(acknowledge_job);

							if (!command_handled)
								error("command: ", cid, " from session: ",
								      block_session.session_id().value, " not acked");
							else
								destroy(_session_commands_slab, &session_command);
						};
						_sessions.apply<Block_session_component>(session_command.session_id,
						                                         get_block);
					};
					_session_commands.apply<Session_command>(id, get_session);
				};
				_driver.with_controller([&] (auto & ctrlr) {
					_driver.with_any_completed_job(ctrlr, io_queue, completed_job); });

				/* deferred acknowledge on the controller */
				_driver.with_controller([&] (auto & ctrlr) {
					ctrlr.ack_io_completions(io_queue.queue_id()); });

				_sessions.for_each<Block_session_component>([&] (Block_session_component &block_session) {

					_driver.with_io_queue(block_session.queue_id(), [&] (Io_queue &block_io_queue) {

						/* import new requests */
						block_session.with_requests([&] (Block::Request request) {

							Response response = Response::RETRY;

							uint16_t cid;
							_driver.with_controller([&] (auto & ctrlr) {
								response = _driver.submit_sq(ctrlr, io_queue, block_io_queue,
								                             request, cid); });

							switch (response) {
							case Response::ACCEPTED:
							new (_session_commands_slab)
								Session_command(_session_commands,
								                block_session.session_id(), cid);
							[[fallthrough]];
							case Response::REJECTED:
								progress = true;
							[[fallthrough]];
							case Response::RETRY:
								break;
							}

							return response;
						});

						/* process I/O */
						_driver.with_controller([&] (auto & ctrlr) {
							progress |= _driver.commit_pending_submits(ctrlr, io_queue); });

						block_session.wakeup_client_if_needed();
					});
				});

				_driver.device_release_if_stopped_and_idle();

				if (!progress) { break; }
			}
		});
	}

	void _handle_requests_mq()
	{
		_sessions.for_each<Block_session_component>([&] (Block_session_component &block_session) {

			_driver.with_io_queue(block_session.queue_id(), [&] (Io_queue &io_queue) {

				for (;;) {
					bool progress = false;

					bool completed_pending = false;

					/* acknowledge finished jobs */
					auto acknowledge_job = [&] (Block_session_component::Ack &ack) {
						auto completed_job = [&] (uint16_t cid) {
							auto complete_fn = [&] (Block::Request &request) {
								ack.submit(request);
								progress = true;

								completed_pending = true;
							};
							io_queue.with_completed_request(cid, complete_fn);
						};

						_driver.with_controller([&] (auto & ctrlr) {
							_driver.with_any_completed_job(ctrlr, io_queue, completed_job); });
					};
					block_session.try_acknowledge(acknowledge_job);

					/* deferred acknowledge on the controller */
					if (completed_pending)
						_driver.with_controller([&] (auto & ctrlr) {
							ctrlr.ack_io_completions(io_queue.queue_id()); });

					/* import new requests */
					block_session.with_requests([&] (Block::Request request) {

						Response response = Response::RETRY;

						_driver.with_controller([&] (auto & ctrlr) {
							response = _driver.submit(ctrlr, io_queue, request); });

						switch (response) {
						case Response::ACCEPTED:
						[[fallthrough]];
						case Response::REJECTED:
							progress = true;
						[[fallthrough]];
						case Response::RETRY:
							break;
						}

						return response;
					});

					/* process I/O */
					_driver.with_controller([&] (auto & ctrlr) {
						progress |= _driver.commit_pending_submits(ctrlr, io_queue); });

					_driver.device_release_if_stopped_and_idle();

					if (!progress) { break; }
				}

				block_session.wakeup_client_if_needed();
			});
		});
	}

	void _handle_requests()
	{
		if (!_force_sq) _handle_requests_mq();
		else            _handle_requests_sq();
	}

	Root::Result session(Root::Session_args const &args, Affinity const &) override
	{
		Session_label const label { label_from_args(args.string()) };

		size_t const min_tx_buf_size = 128 * 1024;
		size_t const tx_buf_size =
			Arg_string::find_arg(args.string(), "tx_buf_size")
			                     .ulong_value(min_tx_buf_size);

		Ram_quota const ram_quota = ram_quota_from_args(args.string());

		if (tx_buf_size > ram_quota.value) {
			error("insufficient 'ram_quota' from '", label, "',"
			      " got ", ram_quota, ", need ", tx_buf_size);
			return Session_error::INSUFFICIENT_RAM;
		}

		return with_matching_policy(label, _config_rom.node(),

			[&] (Node const &policy) -> Root::Result {
				bool const writeable_policy =
					policy.attribute_value("writeable", false);

				Block::Constrained_view view =
					Block::Constrained_view::from_args(args.string());
				view.writeable = writeable_policy && view.writeable;

				Block_session_component *session = nullptr;

				_driver.with_controller([&] (auto & ctrlr) {
					_driver.create_io_queue(ctrlr, tx_buf_size).with_result(
						[&] (Io_queue_space::Id queue_id) {

							_driver.with_io_queue(queue_id, [&] (Io_queue &io_queue) {
								Session_map::Index new_session_id { 0u };

								if (!_session_map.alloc().convert<bool>(
									[&] (Session_map::Alloc_ok ok) {
										new_session_id = ok.index;
										return true;
									},
									[&] (Session_map::Alloc_error) {
										_driver.free_io_queue(ctrlr, queue_id);
										return false; }))
									return;

								try {
									session = new (_sliced_heap)
										Block_session_component(_sessions, _env,
										                        queue_id, io_queue.dma_cap(),
										                        _request_handler, _driver.info(),
										                        view, new_session_id.value);
								} catch (...) {
									_session_map.free(new_session_id);
									_driver.free_io_queue(ctrlr, queue_id);
								}
							});
						},
						[&] (Driver::Io_queue_creation_error) { });
					});

				if (session)
					return { session->cap() };

				return Session_error::DENIED;
			},
			[&] () -> Root::Result { return Session_error::DENIED; });
	}

	void upgrade(Capability<Session>, Root::Upgrade_args const&) override { }

	void close(Capability<Session> cap) override
	{
		bool found = false;
		Session_space::Id session_id { .value = 0 };

		_sessions.for_each<Block_session_component>(
			[&] (Block_session_component &session) {
				if (!(cap == session.cap()))
					return;

				found = true;
				session_id = session.session_id();
			});

		if (!found)
			return;

		_sessions.apply<Block_session_component>(session_id,
			[&] (Block_session_component &session) {
				Io_queue_space::Id const queue_id = session.queue_id();
				destroy(_sliced_heap, &session);

				Session_map::Index const index =
					Session_map::Index::from_id(session_id.value);
				_session_map.free(index);

				_driver.with_controller([&] (auto & ctrlr) {
					_driver.free_io_queue(ctrlr, queue_id); });
			});
	}

	Main(Genode::Env &env) : _env(env)
	{
		/*
		 * Mark first id (0) as used so that it is never allocated
		 * automatically and use it to denote an unset session id.
		 */
		bool reserved = true;
		_session_map.alloc().with_error(
			[&] (Session_map::Alloc_error) { reserved = false; });
		if (!reserved) {
			error("could not reserve index for admin queue");
			_env.parent().exit(-1);
			return;
		}

		if (_force_sq)
			_driver.with_controller([&] (Nvme::Controller &ctrlr) {
				_driver.create_io_queue(ctrlr, 4u << 20).with_result(
					[&] (Io_queue_space::Id queue_id) {
						log("created I/O queue ", queue_id.value,
						    " for single-queue multiplexing");
					}, [&] (Driver::Io_queue_creation_error) { });
			});

		_env.parent().announce(_env.ep().manage(*this));
	}
};


void Component::construct(Genode::Env &env) { static Nvme::Main main(env); }
