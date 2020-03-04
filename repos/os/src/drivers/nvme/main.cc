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
#include <block/request_stream.h>
#include <dataspace/client.h>
#include <os/attached_mmio.h>
#include <os/reporter.h>
#include <os/session_policy.h>
#include <root/root.h>
#include <timer_session/connection.h>
#include <util/bit_array.h>
#include <util/interface.h>
#include <util/misc_math.h>

/* local includes */
#include <util.h>
#include <pci.h>


namespace {

using uint16_t = Genode::uint16_t;
using uint32_t = Genode::uint32_t;
using uint64_t = Genode::uint64_t;
using size_t   = Genode::size_t;
using addr_t   = Genode::addr_t;
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
	struct Sqe_io;

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
	struct Slba       : Genode::Bitset_2<Slba_lower, Slba_upper> { };

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
struct Nvme::Queue
{
	Genode::Ram_dataspace_capability ds { };
	addr_t    pa { 0 };
	addr_t    va { 0 };
	uint32_t  max_entries { 0 };

	bool valid() const { return pa != 0ul; }
};


/*
 * Submission queue
 */
struct Nvme::Sq : Nvme::Queue
{
	uint32_t tail { 0 };
	uint16_t id   { 0 };

	addr_t next()
	{
		addr_t a = va + (tail * SQE_LEN);
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

	addr_t next() { return va + (head * CQE_LEN); }

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
struct Nvme::Controller : public Genode::Attached_mmio
{
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

	/**********
	 ** CODE **
	 **********/

	struct Mem_address
	{
		addr_t va { 0 };
		addr_t pa { 0 };
	};

	struct Initialization_failed : Genode::Exception { };

	Genode::Env &_env;

	Util::Dma_allocator &_dma_alloc;
	Mmio::Delayer       &_delayer;

	/*
	 * There is a completion and submission queue for
	 * every namespace and one pair for the admin queues.
	 */
	Nvme::Cq _cq[NUM_QUEUES] { };
	Nvme::Sq _sq[NUM_QUEUES] { };

	Nvme::Cq &_admin_cq = _cq[0];
	Nvme::Sq &_admin_sq = _sq[0];

	Mem_address _nvme_identify { };

	Genode::Constructible<Identify_data> _identify_data { };

	Mem_address _nvme_nslist { };
	uint32_t    _nvme_nslist_count { 0 };

	size_t _mdts_bytes { 0 };

	size_t _max_io_entries      { MAX_IO_ENTRIES };
	size_t _max_io_entries_mask { _max_io_entries - 1 };

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
	};

	Mem_address _nvme_query_ns[MAX_NS] { };

	struct Info
	{
		Genode::String<8> version { };
		Identify_data::Sn sn { };
		Identify_data::Mn mn { };
		Identify_data::Fr fr { };
		size_t mdts { };
	};

	Info _info { };

	struct Nsinfo
	{
		Block::sector_t count { 0 };
		size_t          size  { 0 };
		Block::sector_t max_request_count { 0 };
		bool valid() const { return count && size; }
	};

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
		enum { MAX = 50u, TO_UNIT = 500u, };
		Attempts     const a(MAX);
		Microseconds const t(((uint64_t)read<Cap::To>() * TO_UNIT) * (1000 / MAX));
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
	 * Setup queue, i.e., fill out fields
	 *
	 * \param q    reference to queue
	 * \param num  number of entries
	 * \param len  size of one entry
	 */
	void _setup_queue(Queue &q, size_t const num, size_t const len)
	{
		size_t const size = num * len;
		q.ds          = _dma_alloc.alloc(size);
		q.pa          = Dataspace_client(q.ds).phys_addr();
		q.va          = (addr_t)_env.rm().attach(q.ds);
		q.max_entries = num;
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
		_setup_queue(_admin_cq, MAX_ADMIN_ENTRIES, CQE_LEN);
		write<Aqa::Acqs>(MAX_ADMIN_ENTRIES_MASK);
		write<Acq>(_admin_cq.pa);

		_setup_queue(_admin_sq, MAX_ADMIN_ENTRIES, SQE_LEN);
		write<Aqa::Asqs>(MAX_ADMIN_ENTRIES_MASK);
		write<Asq>(_admin_sq.pa);
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
		if (_queue_full(_admin_sq, _admin_cq)) { return 0ul; }

		Sqe b(_admin_sq.next());
		b.write<Nvme::Sqe::Cdw0::Opc>(opc);
		b.write<Nvme::Sqe::Cdw0::Cid>(cid);
		b.write<Nvme::Sqe::Nsid>(nsid);
		return b.base();
	}

	/**
	 * Wait until admin command has finished
	 *
	 * \param num  number of attempts
	 * \param cid  command identifier
	 *
	 * \return  returns true if attempt to wait was successfull, otherwise
	 *          false is returned
	 */
	bool _wait_for_admin_cq(uint32_t num, uint16_t cid)
	{
		bool success = false;

		for (uint32_t i = 0; i < num; i++) {
			_delayer.usleep(100 * 1000);

			Cqe b(_admin_cq.next());

			if (b.read<Nvme::Cqe::Cid>() != cid) {
				continue;
			}

			_admin_cq.advance_head();

			success = true;

			write<Admin_cdb::Cqh>(_admin_cq.head);
			break;
		}

		return success;
	}

	/**
	 * Get list of namespaces
	 */
	void _query_nslist()
	{
		if (!_nvme_nslist.va) {
			Ram_dataspace_capability ds = _dma_alloc.alloc(IDENTIFY_LEN);
			_nvme_nslist.va = (addr_t)_env.rm().attach(ds);
			_nvme_nslist.pa = Dataspace_client(ds).phys_addr();
		}

		uint32_t *nslist = (uint32_t*)_nvme_nslist.va;

		bool const nsm = _identify_data->read<Identify_data::Oacs::Nsm>();
		if (!nsm) {
			nslist[0] = 1;
			_nvme_nslist_count = 1;
			return;
		}

		Sqe_identify b(_admin_command(Opcode::IDENTIFY, 0, NSLIST_CID));

		b.write<Nvme::Sqe::Prp1>(_nvme_nslist.pa);
		b.write<Nvme::Sqe_identify::Cdw10::Cns>(Cns::NSLIST);

		write<Admin_sdb::Sqt>(_admin_sq.tail);

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

		uint32_t const *ns = (uint32_t const*)_nvme_nslist.va;
		uint16_t const  id = 0;

		if (!_nvme_query_ns[id].va) {
			Ram_dataspace_capability ds = _dma_alloc.alloc(IDENTIFY_LEN);
			_nvme_query_ns[id].va = (addr_t)_env.rm().attach(ds);
			_nvme_query_ns[id].pa = Dataspace_client(ds).phys_addr();
		}

		Sqe_identify b(_admin_command(Opcode::IDENTIFY, ns[id], QUERYNS_CID));
		b.write<Nvme::Sqe::Prp1>(_nvme_query_ns[id].pa);
		b.write<Nvme::Sqe_identify::Cdw10::Cns>(Cns::IDENTIFY_NS);

		write<Admin_sdb::Sqt>(_admin_sq.tail);

		if (!_wait_for_admin_cq(10, QUERYNS_CID)) {
			error("identify name space failed");
			throw Initialization_failed();
		}

		Identify_ns_data nsdata(_nvme_query_ns[id].va);
		uint32_t const flbas = nsdata.read<Nvme::Identify_ns_data::Flbas>();

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
		if (!_nvme_identify.va) {
			Ram_dataspace_capability ds = _dma_alloc.alloc(IDENTIFY_LEN);
			_nvme_identify.va = (addr_t)_env.rm().attach(ds);
			_nvme_identify.pa = Dataspace_client(ds).phys_addr();
		}

		Sqe_identify b(_admin_command(Opcode::IDENTIFY, 0, IDENTIFY_CID));
		b.write<Nvme::Sqe::Prp1>(_nvme_identify.pa);
		b.write<Nvme::Sqe_identify::Cdw10::Cns>(Cns::IDENTIFY);

		write<Admin_sdb::Sqt>(_admin_sq.tail);

		if (!_wait_for_admin_cq(10, IDENTIFY_CID)) {
			error("identify failed");
			throw Initialization_failed();
		}

		_identify_data.construct(_nvme_identify.va);

		/* store information */
		_info.version = Genode::String<8>(read<Vs::Mjr>(), ".",
		                                  read<Vs::Mnr>(), ".",
		                                  read<Vs::Ter>());
		_info.sn = _identify_data->sn;
		_info.mn = _identify_data->mn;
		_info.fr = _identify_data->fr;

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

	/**
	 * Setup I/O completion queue
	 *
	 * \param id  identifier of the completion queue
	 *
	 * \throw Initialization_failed() in case the queue could not be created
	 */
	void _setup_io_cq(uint16_t id)
	{
		Nvme::Cq &cq = _cq[id];
		if (!cq.valid()) { _setup_queue(cq, _max_io_entries, CQE_LEN); }

		Sqe_create_cq b(_admin_command(Opcode::CREATE_IO_CQ, 0, CREATE_IO_CQ_CID));
		b.write<Nvme::Sqe::Prp1>(cq.pa);
		b.write<Nvme::Sqe_create_cq::Cdw10::Qid>(id);
		b.write<Nvme::Sqe_create_cq::Cdw10::Qsize>(_max_io_entries_mask);
		b.write<Nvme::Sqe_create_cq::Cdw11::Pc>(1);
		b.write<Nvme::Sqe_create_cq::Cdw11::En>(1);

		write<Admin_sdb::Sqt>(_admin_sq.tail);

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
		Nvme::Sq &sq = _sq[id];
		if (!sq.valid()) { _setup_queue(sq, _max_io_entries, SQE_LEN); }

		Sqe_create_sq b(_admin_command(Opcode::CREATE_IO_SQ, 0, CREATE_IO_SQ_CID));
		b.write<Nvme::Sqe::Prp1>(sq.pa);
		b.write<Nvme::Sqe_create_sq::Cdw10::Qid>(id);
		b.write<Nvme::Sqe_create_sq::Cdw10::Qsize>(_max_io_entries_mask);
		b.write<Nvme::Sqe_create_sq::Cdw11::Pc>(1);
		b.write<Nvme::Sqe_create_sq::Cdw11::Qprio>(0b00); /* urgent for now */
		b.write<Nvme::Sqe_create_sq::Cdw11::Cqid>(cqid);

		write<Admin_sdb::Sqt>(_admin_sq.tail);

		if (!_wait_for_admin_cq(10, CREATE_IO_SQ_CID)) {
			error("create I/O sq failed");
			throw Initialization_failed();
		}
	}

	/**
	 * Constructor
	 */
	Controller(Genode::Env &env, Util::Dma_allocator &dma_alloc,
	           addr_t const base, size_t const size,
	           Mmio::Delayer &delayer)
	:
		Genode::Attached_mmio(env, base, size),
		_env(env), _dma_alloc(dma_alloc), _delayer(delayer)
	{ }

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
	}

	/**
	 * Mask interrupts
	 */
	void mask_intr() { write<Intms>(1); }

	/**
	 * Clean interrupts
	 */
	void clear_intr() { write<Intmc>(1); }

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
		Nvme::Sq &sq = _sq[nsid];

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
		Nvme::Sq const &sq = _sq[nsid];
		Nvme::Cq const &cq = _cq[nsid];
		return _queue_full(sq, cq);
	}

	/**
	 * Write current I/O submission queue tail
	 *
	 * \param nsid  namespace identifier
	 */
	void commit_io(uint16_t nsid)
	{
		Nvme::Sq &sq = _sq[nsid];
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
		Nvme::Cq &cq = _cq[nsid];

		if (!cq.valid()) { return; }

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
		Nvme::Cq &cq = _cq[nsid];
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
	Block::sector_t max_count(uint16_t nsid) const { return _nsinfo[nsid].max_request_count; }

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
	}

	void dump_nslist()
	{
		uint32_t const *p = (uint32_t const*)_nvme_nslist.va;
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

		struct Io_error           : Genode::Exception { };
		struct Request_congestion : Genode::Exception { };

	private:

		Driver(const Driver&) = delete;
		Driver& operator=(const Driver&) = delete;

		Genode::Env       &_env;
		Genode::Allocator &_alloc;

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
					Nvme::Controller::Info const &info = _nvme_ctrlr->info();

					xml.attribute("serial", info.sn);
					xml.attribute("model",  info.mn);

					Nvme::Controller::Nsinfo ns = _nvme_ctrlr->nsinfo(Nvme::IO_NSID);

					xml.node("namespace", [&]() {
						xml.attribute("id",          (uint16_t)Nvme::IO_NSID);
						xml.attribute("block_size",  ns.size);
						xml.attribute("block_count", ns.count);
					});
				});
			} catch (...) { }
		}

		/*********
		 ** DMA **
		 *********/

		addr_t _dma_base { 0 };

		Genode::Constructible<Nvme::Pci> _nvme_pci { };

		/*
 		 * The PRP (Physical Region Pages) page is used to setup
		 * large requests.
		 */

		struct Prp_list_helper
		{
			struct Page
			{
				addr_t pa;
				addr_t va;
			};

			Genode::Ram_dataspace_capability _ds;
			addr_t                           _phys_addr;
			addr_t                           _virt_addr;

			Prp_list_helper(Genode::Ram_dataspace_capability ds,
			                addr_t phys, addr_t virt)
			: _ds(ds), _phys_addr(phys), _virt_addr(virt) { }

			Genode::Ram_dataspace_capability dataspace() { return _ds; }

			Page page(uint16_t cid)
			{
				addr_t const offset = cid * Nvme::MPS;

				return Page { .pa = offset + _phys_addr,
				              .va = offset + _virt_addr };
			}
		};

		Genode::Constructible<Prp_list_helper> _prp_list_helper { };

		/**************
		 ** Requests **
		 **************/

		struct Request
		{
			Block::Request block_request { };
			uint32_t       id            { 0 };
		};

		template <unsigned ENTRIES>
		struct Command_id
		{
			using Bitmap = Genode::Bit_array<ENTRIES>;
			Bitmap _bitmap { };

			uint16_t _bitmap_find_free() const
			{
				for (size_t i = 0; i < ENTRIES; i++) {
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
			for (uint16_t i = 0; i < _nvme_ctrlr->max_io_entries(); i++) {
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

		Genode::Constructible<Nvme::Controller> _nvme_ctrlr { };

		/***********
		 ** Block **
		 ***********/

		Block::Session::Info _info { };

	public:

		/**
		 * Constructor
		 */
		Driver(Genode::Env                       &env,
		       Genode::Allocator                 &alloc,
		       Genode::Attached_rom_dataspace    &config_rom,
		       Genode::Signal_context_capability  request_sigh)
		: _env(env), _alloc(alloc), _config_rom(config_rom)
		{
			_config_rom.sigh(_config_sigh);
			_handle_config_update();

			/*
			 * Setup and identify NVMe PCI controller
			 */

			try {
				_nvme_pci.construct(_env);
			} catch (Nvme::Pci::Missing_controller) {
				error("no NVMe PCIe controller found");
				throw;
			}

			try {
				_nvme_ctrlr.construct(_env, *_nvme_pci, _nvme_pci->base(),
				                      _nvme_pci->size(), _delayer);
			} catch (...) {
				error("could not access NVMe controller MMIO");
				throw;
			}

			if (_verbose_regs) { _nvme_ctrlr->dump_cap(); }

			_nvme_ctrlr->init();
			_nvme_ctrlr->identify();

			if (_verbose_identify) {
				_nvme_ctrlr->dump_identify();
				_nvme_ctrlr->dump_nslist();
			}

			/*
			 * Setup I/O
			 */

			{
				Genode::Ram_dataspace_capability ds = _nvme_pci->alloc(Nvme::PRP_DS_SIZE);
				if (!ds.valid()) {
					error("could not allocate DMA backing store");
					throw Nvme::Controller::Initialization_failed();
				}
				addr_t const phys_addr = Genode::Dataspace_client(ds).phys_addr();
				addr_t const virt_addr = (addr_t)_env.rm().attach(ds);
				_prp_list_helper.construct(ds, phys_addr, virt_addr);

				if (_verbose_mem) {
					log("DMA", " virt: [", Hex(virt_addr), ",",
					           Hex(virt_addr + Nvme::PRP_DS_SIZE), "]",
					           " phys: [", Hex(phys_addr), ",",
					           Hex(phys_addr + Nvme::PRP_DS_SIZE), "]");
				}
			}

			_nvme_ctrlr->setup_io(Nvme::IO_NSID, Nvme::IO_NSID);

			/*
			 * Setup Block session
			 */

			/* set Block session properties */
			Nvme::Controller::Nsinfo nsinfo = _nvme_ctrlr->nsinfo(Nvme::IO_NSID);
			if (!nsinfo.valid()) {
				error("could not query namespace information");
				throw Nvme::Controller::Initialization_failed();
			}

			_info = { .block_size  = nsinfo.size,
			          .block_count = nsinfo.count,
			          .align_log2  = Nvme::MPS_LOG2,
			          .writeable   = false };

			Nvme::Controller::Info const &info = _nvme_ctrlr->info();

			log("NVMe:",  info.version.string(),   " "
			    "serial:'", info.sn.string(), "'", " "
			    "model:'",  info.mn.string(), "'", " "
			    "frev:'",   info.fr.string(), "'");

			log("Block", " "
			    "size: ",  _info.block_size, " "
			    "count: ", _info.block_count, " "
			    "I/O entries: ", _nvme_ctrlr->max_io_entries());

			/* generate Report if requested */
			try {
				Genode::Xml_node report = _config_rom.xml().sub_node("report");
				if (report.attribute_value("namespaces", false)) {
					_namespace_reporter.enabled(true);
					_report_namespaces();
				}
			} catch (...) { }

			_nvme_pci->sigh_irq(request_sigh);
			_nvme_ctrlr->clear_intr();
			_nvme_pci->ack_irq();
		}

		~Driver() { /* free resources */ }

		Block::Session::Info info() const { return _info; }

		Genode::Ram_dataspace_capability dma_alloc(size_t size)
		{
			Genode::Ram_dataspace_capability cap = _nvme_pci->alloc(size);
			_dma_base = Dataspace_client(cap).phys_addr();
			return cap;
		}

		void dma_free(Genode::Ram_dataspace_capability cap)
		{
			_dma_base = 0;
			_nvme_pci->free(cap);
		}

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
			if (_nvme_ctrlr->io_queue_full(Nvme::IO_NSID)) {
				return Response::RETRY;
			}

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
				if (request.operation.count > _nvme_ctrlr->max_count(Nvme::IO_NSID)) {
					request.operation.count = _nvme_ctrlr->max_count(Nvme::IO_NSID);
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
			bool const write =
				request.operation.type == Block::Operation::Type::WRITE;

			/* limit request to what we can handle */
			if (request.operation.count > _nvme_ctrlr->max_count(Nvme::IO_NSID)) {
				request.operation.count = _nvme_ctrlr->max_count(Nvme::IO_NSID);
			}

			size_t          const count = request.operation.count;
			Block::sector_t const lba   = request.operation.block_number;

			size_t const len        = request.operation.count * _info.block_size;
			bool   const need_list  = len > 2 * Nvme::MPS;
			addr_t const request_pa = _dma_base + request.offset;

			if (_verbose_io) {
				log("Submit: ", write ? "WRITE" : "READ",
				    " len: ", len, " mps: ", (unsigned)Nvme::MPS,
				    " need_list: ", need_list,
				    " block count: ", count,
				    " lba: ", lba,
				    " dma_base: ", Hex(_dma_base),
				    " offset: ", Hex(request.offset));
			}

			uint16_t const cid = _command_id_allocator.alloc();
			uint32_t const id  = cid | (Nvme::IO_NSID<<16);
			Request &r = _requests[cid];
			r = Request { .block_request = request,
			              .id            = id };

			Nvme::Sqe_io b(_nvme_ctrlr->io_command(Nvme::IO_NSID, cid));
			Nvme::Opcode const op = write ? Nvme::Opcode::WRITE : Nvme::Opcode::READ;
			b.write<Nvme::Sqe::Cdw0::Opc>(op);
			b.write<Nvme::Sqe::Prp1>(request_pa);

			/* payload will fit into 2 mps chunks */
			if (len > Nvme::MPS && !need_list) {
				b.write<Nvme::Sqe::Prp2>(request_pa + Nvme::MPS);
			} else if (need_list) {

				/* get page to store list of mps chunks */
				Prp_list_helper::Page page = _prp_list_helper->page(cid);

				/* omit first page and write remaining pages to iob */
				addr_t  npa = request_pa + Nvme::MPS;
				using Page_entry = uint64_t;
				Page_entry *pe  = (Page_entry*)page.va;

				size_t const mps_len = Genode::align_addr(len, Nvme::MPS_LOG2);
				size_t const num     = (mps_len - Nvme::MPS) / Nvme::MPS;
				if (_verbose_io) {
					log("  page.va: ", Hex(page.va), " page.pa: ",
					    Hex(page.pa), " num: ", num);
				}

				for (size_t i = 0; i < num; i++) {
					if (_verbose_io) {
						log("    [", i, "]: ", Hex(npa));
					}
					pe[i] = npa;
					npa  += Nvme::MPS;
				}
				b.write<Nvme::Sqe::Prp2>(page.pa);
			}

			b.write<Nvme::Sqe_io::Slba>(lba);
			b.write<Nvme::Sqe_io::Cdw12::Nlb>(count - 1); /* 0-base value */
		}

		void _submit_sync(Block::Request const request)
		{
			uint16_t const cid = _command_id_allocator.alloc();
			uint32_t const id  = cid | (Nvme::IO_NSID<<16);
			Request &r = _requests[cid];
			r = Request { .block_request = request,
			              .id            = id };

			Nvme::Sqe_io b(_nvme_ctrlr->io_command(Nvme::IO_NSID, cid));
			b.write<Nvme::Sqe::Cdw0::Opc>(Nvme::Opcode::FLUSH);
		}

		void _submit_trim(Block::Request const request)
		{
			uint16_t const cid = _command_id_allocator.alloc();
			uint32_t const id  = cid | (Nvme::IO_NSID<<16);
			Request &r = _requests[cid];
			r = Request { .block_request = request,
			              .id            = id };

			size_t          const count = request.operation.count;
			Block::sector_t const lba   = request.operation.block_number;

			Nvme::Sqe_io b(_nvme_ctrlr->io_command(Nvme::IO_NSID, cid));
			b.write<Nvme::Sqe::Cdw0::Opc>(Nvme::Opcode::WRITE_ZEROS);
			b.write<Nvme::Sqe_io::Slba>(lba);

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
			_nvme_ctrlr->handle_io_completion(Nvme::IO_NSID, [&] (Nvme::Cqe const &b) {

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

		void mask_irq()
		{
			_nvme_ctrlr->mask_intr();
		}

		void ack_irq()
		{
			_nvme_ctrlr->clear_intr();
			_nvme_pci->ack_irq();
		}

		bool execute()
		{
			if (!_submits_pending) { return false; }

			_nvme_ctrlr->commit_io(Nvme::IO_NSID);
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

			_nvme_ctrlr->ack_io_completions(Nvme::IO_NSID);
			_completed_pending = false;
		}
};


/**********
 ** Main **
 **********/

struct Nvme::Main : Rpc_object<Typed_root<Block::Session>>
{
	Genode::Env  &_env;
	Genode::Heap  _heap { _env.ram(), _env.rm() };

	Genode::Attached_rom_dataspace _config_rom { _env, "config" };

	Genode::Ram_dataspace_capability       _block_ds_cap  { };
	Constructible<Block_session_component> _block_session { };
	Constructible<Nvme::Driver>            _driver        { };

	Signal_handler<Main> _request_handler { _env.ep(), *this, &Main::_handle_requests };
	Signal_handler<Main> _irq_handler     { _env.ep(), *this, &Main::_handle_irq };

	void _handle_irq()
	{
		_driver->mask_irq();
		_handle_requests();
		_driver->ack_irq();
	}

	void _handle_requests()
	{
		if (!_block_session.constructed() || !_driver.constructed())
			return;

		Block_session_component &block_session = *_block_session;

		for (;;) {

			bool progress = false;

			/* import new requests */
			block_session.with_requests([&] (Block::Request request) {

				Response response = _driver->acceptable(request);

				switch (response) {
				case Response::ACCEPTED:
					_driver->submit(request);
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
			progress |= _driver->execute();

			/* acknowledge finished jobs */
			block_session.try_acknowledge([&] (Block_session_component::Ack &ack) {

				_driver->with_any_completed_job([&] (Block::Request request) {

					ack.submit(request);
					progress = true;
				});
			});

			/* defered acknowledge on the controller */
			_driver->acknowledge_if_completed();

			if (!progress) { break; }
		}

		block_session.wakeup_client_if_needed();
	}

	Capability<Session> session(Root::Session_args const &args,
	                            Affinity const &) override
	{
		log("new block session: ", args.string());

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
		_driver->writeable(writeable);

		_block_ds_cap = _driver->dma_alloc(tx_buf_size);
		_block_session.construct(_env, _block_ds_cap, _request_handler,
		                         _driver->info());
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
		_driver->dma_free(_block_ds_cap);
	}

	Main(Genode::Env &env) : _env(env)
	{
		_driver.construct(_env, _heap, _config_rom, _irq_handler);

		_env.parent().announce(_env.ep().manage(*this));
	}
};


void Component::construct(Genode::Env &env) { static Nvme::Main main(env); }
