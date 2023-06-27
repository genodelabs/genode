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
#include <block/request_stream.h>
#include <dataspace/client.h>
#include <os/attached_mmio.h>
#include <os/reporter.h>
#include <os/session_policy.h>
#include <platform_session/device.h>
#include <platform_session/dma_buffer.h>
#include <root/root.h>
#include <timer_session/connection.h>
#include <util/bit_array.h>
#include <util/interface.h>
#include <util/misc_math.h>

/* local includes */
#include <util.h>


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

	struct Sqe;
	struct Sqe_create_cq;
	struct Sqe_create_sq;
	struct Sqe_identify;
	struct Sqe_get_feature;
	struct Sqe_set_feature;
	struct Sqe_io;

	struct Set_hmb;

	struct Queue;
	struct Sq;
	struct Cq;

	struct Controller;

	enum {
		CQE_LEN_LOG2           = 4u,
		CQE_LEN                = 1u << CQE_LEN_LOG2,
		SQE_LEN_LOG2           = 6u,
		SQE_LEN                = 1u << SQE_LEN_LOG2,
		MAX_IO_QUEUES          = 1,

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
		NUM_QUEUES = 1 + MAX_NS,
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
		 HMB  = 0x0d,
	};

	enum Feature_sel {
		CURRENT   = 0b000,
		DEFAULT   = 0b001,
		SAVED     = 0b010,
		SUPPORTED = 0b011,
	};

	struct Block_session_component;
	struct Driver;
	struct Main;
};


/*
 * Identify command data
 */
struct Nvme::Identify_data : Genode::Mmio
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

	Identify_data(addr_t const base)
	: Genode::Mmio(base)
	{
		char const *p = (char const*)base;

		sn = Sn(Util::extract_string(p, SN_OFFSET, SN_LEN+1));
		mn = Mn(Util::extract_string(p, MN_OFFSET, MN_LEN+1));
		fr = Fr(Util::extract_string(p, FR_OFFSET, FR_LEN+1));
	}
};


/*
 * Identify name space command data
 */
struct Nvme::Identify_ns_data : public Genode::Mmio
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

	Identify_ns_data(addr_t const base)
	: Genode::Mmio(base)
	{ }
};


/*
 * Queue doorbell register
 */
struct Nvme::Doorbell : public Genode::Mmio
{
	struct Sqtdbl : Register<0x00, 32>
	{
		struct Sqt : Bitfield< 0, 16> { }; /* submission queue tail */
	};

	struct Cqhdbl : Register<0x04, 32>
	{
		struct Cqh : Bitfield< 0, 16> { }; /* submission queue tail */
	};

	Doorbell(addr_t const base)
	: Genode::Mmio(base) { }
};


/*
 * Completion queue entry
 */
struct Nvme::Cqe : Genode::Mmio
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

	Cqe(addr_t const base) : Genode::Mmio(base) { }

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
struct Nvme::Sqe : Genode::Mmio
{
	struct Cdw0 : Register<0x00, 32>
	{
		struct Opc  : Bitfield< 0,  8> { }; /* opcode */
		struct Fuse : Bitfield< 9,  2> { }; /* fused operation */
		struct Psdt : Bitfield<14,  2> { }; /* PRP or SGL for data transfer */
		struct Cid  : Bitfield<16, 16> { }; /* command identifier */
	};
	struct Nsid : Register<0x04, 32> { };
	struct Mptr : Register<0x10, 64> { };
	struct Prp1 : Register<0x18, 64> { };
	struct Prp2 : Register<0x20, 64> { };

	/* SGL not supported */

	Sqe(addr_t const base) : Genode::Mmio(base) { }

	bool valid() const { return base() != 0ul; }
};


/*
 * Identify command
 */
struct Nvme::Sqe_identify : Nvme::Sqe
{
	struct Cdw10 : Register<0x28, 32>
	{
		struct Cns : Bitfield< 0, 8> { }; /* controller or namespace structure */
	};

	Sqe_identify(addr_t const base) : Sqe(base) { }
};


/*
 * Get feature command
 */
struct Nvme::Sqe_get_feature : Nvme::Sqe
{
	struct Cdw10 : Register<0x28, 32>
	{
		struct Fid : Bitfield< 0, 8> { }; /* feature identifier */
		struct Sel : Bitfield< 8, 2> { }; /* select which value is returned */
	};

	Sqe_get_feature(addr_t const base) : Sqe(base) { }
};


/*
 * Set feature command
 */
struct Nvme::Sqe_set_feature : Nvme::Sqe
{
	struct Cdw10 : Register<0x28, 32>
	{
		struct Fid : Bitfield< 0, 8> { }; /* feature identifier */
		struct Sv  : Bitfield<31, 1> { }; /* save */
	};

	Sqe_set_feature(addr_t const base) : Sqe(base) { }
};


struct Hmb_de : Genode::Mmio
{
	enum { SIZE = 16u };

	struct Badd  : Register<0x00, 64> { };
	struct Bsize : Register<0x08, 64> { };

	Hmb_de(addr_t const base, addr_t const buffer, size_t units) : Genode::Mmio(base)
	{
		write<Badd>(buffer);
		write<Bsize>(units);
	}
};


struct Nvme::Set_hmb : Nvme::Sqe_set_feature
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

	Set_hmb(addr_t const base, uint64_t const hmdl,
	        uint32_t const units, uint32_t const entries)
	:
		Sqe_set_feature(base)
	{
		write<Sqe_set_feature::Cdw10::Fid>(Feature_fid::HMB);
		write<Cdw11::Ehm>(1);
		write<Cdw12::Hsize>(units);
		write<Cdw13::Hmdlla>(uint32_t(hmdl));
		write<Cdw14::Hmdlua>(uint32_t(hmdl >> 32u));
		write<Cdw15::Hmdlec>(entries);
	}
};


/*
 *  Create completion queue command
 */
struct Nvme::Sqe_create_cq : Nvme::Sqe
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

	Sqe_create_cq(addr_t const base) : Sqe(base) { }
};


/*
 * Create submission queue command
 */
struct Nvme::Sqe_create_sq : Nvme::Sqe
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

	Sqe_create_sq(addr_t const base) : Sqe(base) { }
};


/*
 * I/O command
 */
struct Nvme::Sqe_io : Nvme::Sqe
{
	struct Slba_lower : Register<0x28, 32> { };
	struct Slba_upper : Register<0x2c, 32> { };

	struct Cdw12 : Register<0x30, 32>
	{
		struct Deac : Bitfield<25,  1> { }; /* for WRITE_ZEROS needed by TRIM */
		struct Nlb  : Bitfield< 0, 16> { };
	};

	Sqe_io(addr_t const base) : Sqe(base) { }
};


/*
 * Queue base structure
 */
struct Nvme::Queue : Platform::Dma_buffer
{
	size_t   len;
	uint32_t max_entries;

	Queue(Platform::Connection & platform,
	      uint32_t               max_entries,
	      size_t                 len)
	:
		Dma_buffer(platform, len * max_entries, UNCACHED),
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

	addr_t next()
	{
		addr_t a = (addr_t)local_addr<void>() + (tail * SQE_LEN);
		Genode::memset((void*)a, 0, SQE_LEN);
		tail = (tail + 1) % max_entries;
		return a;
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

	addr_t next() { return (addr_t)local_addr<void>() + (head * CQE_LEN); }

	void advance_head()
	{
		if (++head >= max_entries) {
			head   = 0;
			phase ^= 1;
		}
	}
};


/*
 * Controller
 */
class Nvme::Controller : Platform::Device,
                         Platform::Device::Mmio,
                         Platform::Device::Irq
{
	using Mmio = Genode::Mmio;

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

	/*
	 * I/O submission doorbell
	 */
	struct Io_sdb : Register<0x1008, 32>
	{
		struct Sqt : Bitfield< 0, 16> { }; /* submission queue tail */
	};

	/*
	 * I/O completion doorbell
	 */
	struct Io_cdb : Register<0x100C, 32>
	{
		struct Cqh : Bitfield< 0, 16> { }; /* completion queue tail */
	};

	struct Initialization_failed : Genode::Exception { };

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

	Constructible<Nvme::Cq> &_admin_cq = _cq[0];
	Constructible<Nvme::Sq> &_admin_sq = _sq[0];

	Platform::Dma_buffer _nvme_identify { _platform, IDENTIFY_LEN, UNCACHED };

	Genode::Constructible<Identify_data> _identify_data { };

	Platform::Dma_buffer _nvme_nslist { _platform, IDENTIFY_LEN, UNCACHED };
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
		SET_HMB_CID,
	};

	Constructible<Platform::Dma_buffer> _nvme_query_ns[MAX_NS] { };

	struct Hmb_chunk
	{
		Registry<Hmb_chunk>::Element _elem;

		Platform::Dma_buffer dma_buffer;

		Hmb_chunk(Registry<Hmb_chunk> &registry,
		          Platform::Connection &platform, size_t size)
		:
			_elem { registry, *this },
			dma_buffer { platform, size, UNCACHED }
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
	Constructible<Platform::Dma_buffer>  _hmb_descr_list_buffer { };

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
	addr_t _admin_command(Opcode opc, uint32_t nsid, uint32_t cid)
	{
		if (_queue_full(*_admin_sq, *_admin_cq)) { return 0ul; }

		Sqe b(_admin_sq->next());
		b.write<Nvme::Sqe::Cdw0::Opc>(opc);
		b.write<Nvme::Sqe::Cdw0::Cid>(cid);
		b.write<Nvme::Sqe::Nsid>(nsid);
		return b.base();
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

		b.write<Nvme::Sqe::Prp1>(_nvme_nslist.dma_addr());
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
			_nvme_query_ns[id].construct(_platform, IDENTIFY_LEN, UNCACHED);

		Sqe_identify b(_admin_command(Opcode::IDENTIFY, ns[id], QUERYNS_CID));
		b.write<Nvme::Sqe::Prp1>(_nvme_query_ns[id]->dma_addr());
		b.write<Nvme::Sqe_identify::Cdw10::Cns>(Cns::IDENTIFY_NS);

		write<Admin_sdb::Sqt>(_admin_sq->tail);

		if (!_wait_for_admin_cq(10, QUERYNS_CID)) {
			error("identify name space failed");
			throw Initialization_failed();
		}

		Identify_ns_data nsdata((addr_t)_nvme_query_ns[id]->local_addr<void>());
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
		b.write<Nvme::Sqe::Prp1>(_nvme_identify.dma_addr());
		b.write<Nvme::Sqe_identify::Cdw10::Cns>(Cns::IDENTIFY);

		write<Admin_sdb::Sqt>(_admin_sq->tail);

		if (!_wait_for_admin_cq(10, IDENTIFY_CID)) {
			error("identify failed");
			throw Initialization_failed();
		}

		_identify_data.construct((addr_t)_nvme_identify.local_addr<void>());

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
			_hmb_descr_list_buffer.construct(_platform, HMB_LIST_SIZE, UNCACHED);
		} catch (... /* intentional catch-all */) {
			warning("could not allocate HMB descriptor list page");
			return;
		}

		_hmb_chunk_registry.construct(_hmb_alloc);

		addr_t list_base =
			(addr_t)_hmb_descr_list_buffer->local_addr<addr_t>();
		for (uint32_t i = 0; i < num_entries; i++) {
			try {
				Hmb_chunk *c =
					new (_hmb_alloc) Hmb_chunk(*_hmb_chunk_registry,
					                           _platform, HMB_CHUNK_SIZE);

				Hmb_de e(list_base, c->dma_buffer.dma_addr(), HMB_CHUNK_UNITS);

				list_base += Hmb_de::SIZE;
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
	 * Setup I/O completion queue
	 *
	 * \param id  identifier of the completion queue
	 *
	 * \throw Initialization_failed() in case the queue could not be created
	 */
	void _setup_io_cq(uint16_t id)
	{
		if (!_cq[id].constructed())
			_cq[id].construct(_platform, _max_io_entries, CQE_LEN);

		Nvme::Cq &cq = *_cq[id];

		Sqe_create_cq b(_admin_command(Opcode::CREATE_IO_CQ, 0, CREATE_IO_CQ_CID));
		b.write<Nvme::Sqe::Prp1>(cq.dma_addr());
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
		b.write<Nvme::Sqe::Prp1>(sq.dma_addr());
		b.write<Nvme::Sqe_create_sq::Cdw10::Qid>(id);
		b.write<Nvme::Sqe_create_sq::Cdw10::Qsize>(_max_io_entries_mask);
		b.write<Nvme::Sqe_create_sq::Cdw11::Pc>(1);
		b.write<Nvme::Sqe_create_sq::Cdw11::Qprio>(0b00); /* urgent for now */
		b.write<Nvme::Sqe_create_sq::Cdw11::Cqid>(cqid);

		write<Admin_sdb::Sqt>(_admin_sq->tail);

		if (!_wait_for_admin_cq(10, CREATE_IO_SQ_CID)) {
			error("create I/O sq failed");
			throw Initialization_failed();
		}
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
		Platform::Device::Mmio((Platform::Device&)*this),
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
	 * Setup I/O queue
	 */
	void setup_io(uint16_t cid, uint16_t sid)
	{
		_setup_io_cq(cid);
		_setup_io_sq(sid, cid);
	}

	/**
	 * Get next free IO submission queue slot
	 *
	 * \param nsid  namespace identifier
	 *
	 * \return  returns virtual address of the I/O command
	 */
	addr_t io_command(uint16_t nsid, uint16_t cid)
	{
		Nvme::Sq &sq = *_sq[nsid];

		Sqe e(sq.next());
		e.write<Nvme::Sqe::Cdw0::Cid>(cid);
		e.write<Nvme::Sqe::Nsid>(nsid);
		return e.base();
	}

	/**
	 * Check if I/O queue is full
	 *
	 * \param nsid  namespace identifier
	 *
	 * \return  true if full, otherwise false
	 */
	bool io_queue_full(uint16_t nsid) const
	{
		Nvme::Sq const &sq = *_sq[nsid];
		Nvme::Cq const &cq = *_cq[nsid];
		return _queue_full(sq, cq);
	}

	/**
	 * Write current I/O submission queue tail
	 *
	 * \param nsid  namespace identifier
	 */
	void commit_io(uint16_t nsid)
	{
		Nvme::Sq &sq = *_sq[nsid];
		write<Io_sdb::Sqt>(sq.tail);
	}

	/**
	 * Process a pending I/O completion
	 *
	 * \param nsid  namespace identifier
	 * \param func  function that is called on each completion
	 */
	template <typename FUNC>
	void handle_io_completion(uint16_t nsid, FUNC const &func)
	{
		if (!_cq[nsid].constructed())
			return;

		Nvme::Cq &cq = *_cq[nsid];

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
	 * \param nsid  namespace identifier
	 */
	void ack_io_completions(uint16_t nsid)
	{
		Nvme::Cq &cq = *_cq[nsid];
		write<Io_cdb::Cqh>(cq.head);
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

	Block::Session::Info _info;

	Block_session_component(Env &env, Dataspace_capability ds,
	                        Signal_context_capability sigh,
	                        Block::Session::Info info)
	:
		Request_stream(env.rm(), ds, env.ep(), sigh, info), _env(env),
		_info(info)
	{
		_env.ep().manage(*this);
	}

	~Block_session_component() { _env.ep().dissolve(*this); }

	Info info() const override
	{
		return _info;
	}

	Capability<Tx> tx_cap() override { return Request_stream::tx_cap(); }
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
		bool _verbose_mem      { false };
		bool _verbose_regs     { false };

		size_t _hmb_size { 0 };

		struct Io_error           : Genode::Exception { };
		struct Request_congestion : Genode::Exception { };

	private:

		Driver(const Driver&) = delete;
		Driver& operator=(const Driver&) = delete;

		Genode::Env          &_env;
		Platform::Connection  _platform { _env };

		Genode::Attached_rom_dataspace &_config_rom;

		void _handle_config_update()
		{
			_config_rom.update();

			if (!_config_rom.valid()) { return; }

			Genode::Xml_node config = _config_rom.xml();
			_verbose_checks   = config.attribute_value("verbose_checks",   _verbose_checks);
			_verbose_identify = config.attribute_value("verbose_identify", _verbose_identify);
			_verbose_io       = config.attribute_value("verbose_io",       _verbose_io);
			_verbose_mem      = config.attribute_value("verbose_mem",      _verbose_mem);
			_verbose_regs     = config.attribute_value("verbose_regs",     _verbose_regs);

			_hmb_size = config.attribute_value("max_hmb_size", Genode::Number_of_bytes(0));
		}

		Genode::Signal_handler<Driver> _config_sigh {
			_env.ep(), *this, &Driver::_handle_config_update };

		/**************
		 ** Reporter **
		 **************/

		Genode::Reporter _namespace_reporter { _env, "controller" };

		void _report_namespaces()
		{
			try {
				Genode::Reporter::Xml_generator xml(_namespace_reporter, [&]() {
					Nvme::Controller::Info const &info = _nvme_ctrlr.info();

					xml.attribute("serial", info.sn);
					xml.attribute("model",  info.mn);

					Nvme::Controller::Nsinfo ns = _nvme_ctrlr.nsinfo(Nvme::IO_NSID);

					xml.node("namespace", [&]() {
						xml.attribute("id",          (uint16_t)Nvme::IO_NSID);
						xml.attribute("block_size",  ns.size);
						xml.attribute("block_count", ns.count);
					});
				});
			} catch (...) { }
		}

		/**************
		 ** Requests **
		 **************/

		struct Request
		{
			Block::Request block_request { };
			uint32_t       id            { 0 };
		};

		template <uint16_t ENTRIES>
		struct Command_id
		{
			using Bitmap = Genode::Bit_array<ENTRIES>;
			Bitmap _bitmap { };

			uint16_t _bitmap_find_free() const
			{
				for (uint16_t i = 0; i < ENTRIES; i++) {
					if (_bitmap.get(i, 1)) { continue; }
					return i;
				}
				return ENTRIES;
			}

			bool used(uint16_t const cid) const
			{
				return _bitmap.get(cid, 1);
			}

			uint16_t alloc()
			{
				uint16_t const id = _bitmap_find_free();
				_bitmap.set(id, 1);
				return id;
			}

			void free(uint16_t id)
			{
				_bitmap.clear(id, 1);
			}
		};

		Command_id<Nvme::MAX_IO_ENTRIES> _command_id_allocator { };
		Request                          _requests[Nvme::MAX_IO_ENTRIES] { };

		template <typename FUNC>
		bool _for_any_request(FUNC const &func) const
		{
			for (uint16_t i = 0; i < _nvme_ctrlr.max_io_entries(); i++) {
				if (_command_id_allocator.used(i) && func(_requests[i])) {
					return true;
				}
			}
			return false;
		}

		bool _submits_pending   { false };
		bool _completed_pending { false };

		/*********************
		 ** MMIO Controller **
		 *********************/

		struct Timer_delayer : Genode::Mmio::Delayer,
		                       Timer::Connection
		{
			Timer_delayer(Genode::Env &env)
			: Timer::Connection(env) { }

			void usleep(uint64_t us) override { Timer::Connection::usleep(us); }
		} _delayer { _env };

		Signal_context_capability _irq_sigh;
		Nvme::Controller _nvme_ctrlr { _env, _platform, _delayer, _irq_sigh };

		/*********
		 ** DMA **
		 *********/

		Constructible<Platform::Dma_buffer> _dma_buffer { };

		/*
		 * The PRP (Physical Region Pages) page is used to setup
		 * large requests.
		 */
		Platform::Dma_buffer _prp_list_helper { _platform, Nvme::PRP_DS_SIZE,
		                                        UNCACHED };

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
		       Genode::Signal_context_capability  request_sigh)
		: _env(env),
		  _config_rom(config_rom), _irq_sigh(request_sigh)
		{
			_config_rom.sigh(_config_sigh);
			_handle_config_update();

			/*
			 * Setup and identify NVMe PCI controller
			 */

			if (_verbose_regs) { _nvme_ctrlr.dump_cap(); }

			_nvme_ctrlr.init();
			_nvme_ctrlr.identify();

			if (_verbose_identify) {
				_nvme_ctrlr.dump_identify();
				_nvme_ctrlr.dump_nslist();
			}

			/*
			 * Setup HMB
			 */
			if (_nvme_ctrlr.info().hmpre)
				_nvme_ctrlr.setup_hmb(_hmb_size);

			/*
			 * Setup I/O
			 */

			if (_verbose_mem) {
				addr_t virt_addr = (addr_t)_prp_list_helper.local_addr<void>();
				addr_t phys_addr = _prp_list_helper.dma_addr();
				log("DMA", " virt: [", Hex(virt_addr), ",",
				           Hex(virt_addr + Nvme::PRP_DS_SIZE), "]",
				           " phys: [", Hex(phys_addr), ",",
				           Hex(phys_addr + Nvme::PRP_DS_SIZE), "]");
			}

			_nvme_ctrlr.setup_io(Nvme::IO_NSID, Nvme::IO_NSID);

			/*
			 * Setup Block session
			 */

			/* set Block session properties */
			Nvme::Controller::Nsinfo nsinfo = _nvme_ctrlr.nsinfo(Nvme::IO_NSID);
			if (!nsinfo.valid()) {
				error("could not query namespace information");
				throw Nvme::Controller::Initialization_failed();
			}

			_info = { .block_size  = nsinfo.size,
			          .block_count = nsinfo.count,
			          .align_log2  = Nvme::MPS_LOG2,
			          .writeable   = false };

			Nvme::Controller::Info const &info = _nvme_ctrlr.info();

			log("NVMe:",  info.version.string(),   " "
			    "serial:'", info.sn.string(), "'", " "
			    "model:'",  info.mn.string(), "'", " "
			    "frev:'",   info.fr.string(), "'");

			log("Block", " "
			    "size: ",  _info.block_size, " "
			    "count: ", _info.block_count, " "
			    "I/O entries: ", _nvme_ctrlr.max_io_entries());

			/* generate Report if requested */
			try {
				Genode::Xml_node report = _config_rom.xml().sub_node("report");
				if (report.attribute_value("namespaces", false)) {
					_namespace_reporter.enabled(true);
					_report_namespaces();
				}
			} catch (...) { }
		}

		~Driver() { /* free resources */ }

		Block::Session::Info info() const { return _info; }

		void writeable(bool writeable) { _info.writeable = writeable; }


		/******************************
		 ** Block request stream API **
		 ******************************/

		Response _check_acceptance(Block::Request request) const
		{
			/*
			 * All memory is dimensioned in a way that it will allow for
			 * MAX_IO_ENTRIES requests, so it is safe to only check the
			 * I/O queue.
			 */
			if (_nvme_ctrlr.io_queue_full(Nvme::IO_NSID)) {
				return Response::RETRY;
			}

			if (!Genode::aligned(request.offset, Nvme::MPS_LOG2))
				return Response::REJECTED;

			switch (request.operation.type) {
			case Block::Operation::Type::INVALID:
				return Response::REJECTED;

			case Block::Operation::Type::SYNC:
				return Response::ACCEPTED;

			case Block::Operation::Type::TRIM:
			[[fallthrough]];

			case Block::Operation::Type::WRITE:
				if (!_info.writeable) {
					return Response::REJECTED;
				}
			[[fallthrough]];

			case Block::Operation::Type::READ:
				/* limit request to what we can handle, needed for overlap check */
				if (request.operation.count > _nvme_ctrlr.max_count(Nvme::IO_NSID)) {
					request.operation.count = _nvme_ctrlr.max_count(Nvme::IO_NSID);
				}
			}

			size_t          const count   = request.operation.count;
			Block::sector_t const lba     = request.operation.block_number;
			Block::sector_t const lba_end = lba + count - 1;

			// XXX trigger overlap only in case of mixed read and write requests?
			auto overlap_check = [&] (Request const &req) {
				Block::sector_t const start = req.block_request.operation.block_number;
				Block::sector_t const end   = start + req.block_request.operation.count - 1;

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
			if (_for_any_request(overlap_check)) { return Response::RETRY; }

			return Response::ACCEPTED;
		}

		void _submit(Block::Request request)
		{
			if (!_dma_buffer.constructed())
				return;

			bool const write =
				request.operation.type == Block::Operation::Type::WRITE;

			/* limit request to what we can handle */
			if (request.operation.count > _nvme_ctrlr.max_count(Nvme::IO_NSID)) {
				request.operation.count = _nvme_ctrlr.max_count(Nvme::IO_NSID);
			}

			uint32_t        const count = (uint32_t)request.operation.count;
			Block::sector_t const lba   = request.operation.block_number;

			size_t const len        = request.operation.count * _info.block_size;
			bool   const need_list  = len > 2 * Nvme::MPS;
			addr_t const request_pa = _dma_buffer->dma_addr() + request.offset;

			if (_verbose_io) {
				log("Submit: ", write ? "WRITE" : "READ",
				    " len: ", len, " mps: ", (unsigned)Nvme::MPS,
				    " need_list: ", need_list,
				    " block count: ", count,
				    " lba: ", lba,
				    " dma_base: ", Hex(_dma_buffer->dma_addr()),
				    " offset: ", Hex(request.offset));
			}

			uint16_t const cid = _command_id_allocator.alloc();
			uint32_t const id  = cid | (Nvme::IO_NSID<<16);
			Request &r = _requests[cid];
			r = Request { .block_request = request,
			              .id            = id };

			Nvme::Sqe_io b(_nvme_ctrlr.io_command(Nvme::IO_NSID, cid));
			Nvme::Opcode const op = write ? Nvme::Opcode::WRITE : Nvme::Opcode::READ;
			b.write<Nvme::Sqe::Cdw0::Opc>(op);
			b.write<Nvme::Sqe::Prp1>(request_pa);

			/* payload will fit into 2 mps chunks */
			if (len > Nvme::MPS && !need_list) {
				b.write<Nvme::Sqe::Prp2>(request_pa + Nvme::MPS);
			} else if (need_list) {

				/* get page to store list of mps chunks */
				addr_t const offset = cid * Nvme::MPS;
				addr_t pa = _prp_list_helper.dma_addr() + offset;
				addr_t va = (addr_t)_prp_list_helper.local_addr<void>()
				            + offset;

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
				b.write<Nvme::Sqe::Prp2>(pa);
			}

			b.write<Nvme::Sqe_io::Slba_lower>(uint32_t(lba));
			b.write<Nvme::Sqe_io::Slba_upper>(uint32_t(lba >> 32u));
			b.write<Nvme::Sqe_io::Cdw12::Nlb>(count - 1); /* 0-base value */
		}

		void _submit_sync(Block::Request const request)
		{
			uint16_t const cid = _command_id_allocator.alloc();
			uint32_t const id  = cid | (Nvme::IO_NSID<<16);
			Request &r = _requests[cid];
			r = Request { .block_request = request,
			              .id            = id };

			Nvme::Sqe_io b(_nvme_ctrlr.io_command(Nvme::IO_NSID, cid));
			b.write<Nvme::Sqe::Cdw0::Opc>(Nvme::Opcode::FLUSH);
		}

		void _submit_trim(Block::Request const request)
		{
			uint16_t const cid = _command_id_allocator.alloc();
			uint32_t const id  = cid | (Nvme::IO_NSID<<16);
			Request &r = _requests[cid];
			r = Request { .block_request = request,
			              .id            = id };

			uint32_t        const count = (uint32_t)request.operation.count;
			Block::sector_t const lba   = request.operation.block_number;

			Nvme::Sqe_io b(_nvme_ctrlr.io_command(Nvme::IO_NSID, cid));
			b.write<Nvme::Sqe::Cdw0::Opc>(Nvme::Opcode::WRITE_ZEROS);
			b.write<Nvme::Sqe_io::Slba_lower>(uint32_t(lba));
			b.write<Nvme::Sqe_io::Slba_upper>(uint32_t(lba >> 32u));

			/*
			 * XXX For now let the device decide if it wants to deallocate
			 *     the blocks or not.
			 *
			 * b.write<Nvme::Sqe_io::Cdw12::Deac>(1);
			 */
			b.write<Nvme::Sqe_io::Cdw12::Nlb>(count - 1); /* 0-base value */
		}

		void _get_completed_request(Block::Request &out, uint16_t &out_cid)
		{
			_nvme_ctrlr.handle_io_completion(Nvme::IO_NSID, [&] (Nvme::Cqe const &b) {

				if (_verbose_io) { Nvme::Cqe::dump(b); }

				uint32_t const id  = Nvme::Cqe::request_id(b);
				uint16_t const cid = Nvme::Cqe::command_id(b);
				Request &r = _requests[cid];
				if (r.id != id) {
					error("no pending request found for CQ entry: id: ",
					      id, " != r.id: ", r.id);
					Nvme::Cqe::dump(b);
					return;
				}

				out_cid = cid;

				r.block_request.success = Nvme::Cqe::succeeded(b);
				out = r.block_request;

				_completed_pending = true;
			});
		}

		void _free_completed_request(uint16_t const cid)
		{
			_command_id_allocator.free(cid);
		}


		/**********************
		 ** driver interface **
		 **********************/

		Response acceptable(Block::Request const request) const
		{
			return _check_acceptance(request);
		}

		void submit(Block::Request const request)
		{
			switch (request.operation.type) {
			case Block::Operation::Type::READ:
			case Block::Operation::Type::WRITE:
				_submit(request);
				break;
			case Block::Operation::Type::SYNC:
				_submit_sync(request);
				break;
			case Block::Operation::Type::TRIM:
				_submit_trim(request);
				break;
			default:
				return;
			}

			_submits_pending = true;
		}

		void ack_irq()
		{
			_nvme_ctrlr.ack_irq();
		}

		bool execute()
		{
			if (!_submits_pending) { return false; }

			_nvme_ctrlr.commit_io(Nvme::IO_NSID);
			_submits_pending = false;
			return true;
		}

		template <typename FN>
		void with_any_completed_job(FN const &fn)
		{
			uint16_t cid { 0 };
			Block::Request request { };

			_get_completed_request(request, cid);

			if (request.operation.valid()) {
				fn(request);
				_free_completed_request(cid);
			}
		}

		void acknowledge_if_completed()
		{
			if (!_completed_pending) { return; }

			_nvme_ctrlr.ack_io_completions(Nvme::IO_NSID);
			_completed_pending = false;
		}

		Dataspace_capability dma_buffer_construct(size_t size)
		{
			_dma_buffer.construct(_platform, size, UNCACHED);
			return _dma_buffer->cap();
		}

		void dma_buffer_destruct() { _dma_buffer.destruct(); }
};


/**********
 ** Main **
 **********/

struct Nvme::Main : Rpc_object<Typed_root<Block::Session>>
{
	Genode::Env &_env;

	Genode::Attached_rom_dataspace _config_rom { _env, "config" };

	Genode::Ram_dataspace_capability       _block_ds_cap  { };
	Constructible<Block_session_component> _block_session { };

	Signal_handler<Main> _request_handler { _env.ep(), *this, &Main::_handle_requests };
	Signal_handler<Main> _irq_handler     { _env.ep(), *this, &Main::_handle_irq };

	Nvme::Driver _driver { _env, _config_rom, _irq_handler };

	void _handle_irq()
	{
		_handle_requests();
		_driver.ack_irq();
	}

	void _handle_requests()
	{
		if (!_block_session.constructed())
			return;

		Block_session_component &block_session = *_block_session;

		for (;;) {

			bool progress = false;

			/* import new requests */
			block_session.with_requests([&] (Block::Request request) {

				Response response = _driver.acceptable(request);

				switch (response) {
				case Response::ACCEPTED:
					_driver.submit(request);
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
			progress |= _driver.execute();

			/* acknowledge finished jobs */
			block_session.try_acknowledge([&] (Block_session_component::Ack &ack) {

				_driver.with_any_completed_job([&] (Block::Request request) {

					ack.submit(request);
					progress = true;
				});
			});

			/* defered acknowledge on the controller */
			_driver.acknowledge_if_completed();

			if (!progress) { break; }
		}

		block_session.wakeup_client_if_needed();
	}

	Capability<Session> session(Root::Session_args const &args,
	                            Affinity const &) override
	{
		if (_block_session.constructed()) {
			error("device is already in use");
			throw Service_denied();
		}

		Session_label  const label  { label_from_args(args.string()) };
		Session_policy const policy { label, _config_rom.xml() };

		size_t const min_tx_buf_size = 128 * 1024;
		size_t const tx_buf_size =
			Arg_string::find_arg(args.string(), "tx_buf_size")
			                     .ulong_value(min_tx_buf_size);

		Ram_quota const ram_quota = ram_quota_from_args(args.string());

		if (tx_buf_size > ram_quota.value) {
			error("insufficient 'ram_quota' from '", label, "',"
			      " got ", ram_quota, ", need ", tx_buf_size);
			throw Insufficient_ram_quota();
		}

		bool const writeable = policy.attribute_value("writeable", false);
		_driver.writeable(writeable);

		_block_session.construct(_env, _driver.dma_buffer_construct(tx_buf_size),
		                         _request_handler, _driver.info());
		return _block_session->cap();
	}

	void upgrade(Capability<Session>, Root::Upgrade_args const&) override { }

	void close(Capability<Session>) override
	{
		_block_session.destruct();
		/*
		 * XXX a malicious client could submit all its requests
		 *     and close the session...
		 */
		_driver.dma_buffer_destruct();
	}

	Main(Genode::Env &env) : _env(env) {
		_env.parent().announce(_env.ep().manage(*this)); }
};


void Component::construct(Genode::Env &env) { static Nvme::Main main(env); }
