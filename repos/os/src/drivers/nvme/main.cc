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
#include <base/attached_rom_dataspace.h>
#include <base/component.h>
#include <base/heap.h>
#include <base/log.h>
#include <block/component.h>
#include <dataspace/client.h>
#include <os/attached_mmio.h>
#include <os/reporter.h>
#include <timer_session/connection.h>
#include <util/bit_array.h>
#include <util/interface.h>
#include <util/misc_math.h>

/* local includes */
#include <util.h>
#include <pci.h>


namespace {

using uint16_t          = Genode::uint16_t;
using uint32_t          = Genode::uint32_t;
using uint64_t          = Genode::uint64_t;
using size_t            = Genode::size_t;
using addr_t            = Genode::addr_t;
using Packet_descriptor = Block::Packet_descriptor;

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
		CQE_LEN                = 16,
		SQE_LEN                = 64,
		MAX_IO_QUEUES          = 1,
		MAX_IO_ENTRIES         = 128,
		MAX_IO_ENTRIES_MASK    = MAX_IO_ENTRIES - 1,
		MAX_IO_PENDING         = MAX_IO_ENTRIES - 1, /* tail + 1 == head -> full */
		MAX_ADMIN_ENTRIES      = 128,
		MAX_ADMIN_ENTRIES_MASK = MAX_ADMIN_ENTRIES - 1,
	};

	enum {
		/*
		 * Limit max I/O requests size; we can map up to 2MiB with one list
		 * page (4K/8 = 512 * 4K) but 1MiB is plenty
		 */
		MAX_IO_LEN       =   1u << 20,
		DMA_DS_SIZE      =   4u << 20,
		DMA_LIST_DS_SIZE = 256u << 10,
		MPS              = 4096u,
	};

	enum {
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
	};
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
		struct Nlb : Bitfield<0, 16> { };
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

	size_t _mps { 0 };

	Nvme::Cq _cq[NUM_QUEUES] { };
	Nvme::Sq _sq[NUM_QUEUES] { };

	Nvme::Cq &_admin_cq = _cq[0];
	Nvme::Sq &_admin_sq = _sq[0];

	Mem_address _nvme_identify { };

	Genode::Constructible<Identify_data> _identify_data { };

	Mem_address _nvme_nslist { };
	uint32_t    _nvme_nslist_count { 0 };

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
	} _info { };

	struct Nsinfo
	{
		Block::sector_t count { 0 };
		size_t          size  { 0 };
		bool valid() const { return count && size; }
	} _nsinfo[MAX_NS] { };

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
		Microseconds const t((read<Cap::To>() * TO_UNIT) * (1000 / MAX));
		try {
			wait_for(a, t, _delayer, Csts::Rdy::Equal(val));
		} catch (Mmio::Polling_timeout) {
			Genode::error("Csts::Rdy(", val, ") failed");
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
		write<Intms>(~0u);
		write<Cc>(0);

		try { _wait_for_rdy(0); }
		catch (...) { throw Initialization_failed(); }

		/*
		 * For now we limit the memory page size to 4K because besides Qemu
		 * there are not that many consumer NVMe device that support larger
		 * page sizes and we do not want to align the DMA buffers to larger
		 * sizes.
		 */
		Cap::access_t const mpsmax = read<Cap::Mpsmax>();
		if (mpsmax > 0) { Genode::warning("ignore mpsmax:", mpsmax); }

		/* the value written to the register amounts to 2^(12 + v) bytes */
		Cap::access_t const v = Genode::log2((unsigned)Nvme::MPS) - 12;
		_mps = 1u << (12 + v);
		write<Cc::Mps>(v);

		write<Cc::Iocqes>(log2((unsigned)CQE_LEN));
		write<Cc::Iosqes>(log2((unsigned)SQE_LEN));
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
		return ((sq.tail + 1) & (MAX_IO_ENTRIES_MASK)) == cq.head;
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
			Genode::error("identify name space list failed");
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
		uint32_t const  id = 0;

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
			Genode::error("identify name space failed");
			throw Initialization_failed();
		}

		Identify_ns_data nsdata(_nvme_query_ns[id].va);
		uint32_t const flbas = nsdata.read<Nvme::Identify_ns_data::Flbas>();

		_nsinfo[id].count = nsdata.read<Nvme::Identify_ns_data::Nsze>();
		_nsinfo[id].size  = 1u << nsdata.read<Nvme::Identify_ns_data::Lbaf::Lbads>(flbas);
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
			Genode::error("identify failed");
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
		if (!cq.valid()) { _setup_queue(cq, MAX_IO_ENTRIES, CQE_LEN); }

		Sqe_create_cq b(_admin_command(Opcode::CREATE_IO_CQ, 0, CREATE_IO_CQ_CID));
		b.write<Nvme::Sqe::Prp1>(cq.pa);
		b.write<Nvme::Sqe_create_cq::Cdw10::Qid>(id);
		b.write<Nvme::Sqe_create_cq::Cdw10::Qsize>(MAX_IO_ENTRIES_MASK);
		b.write<Nvme::Sqe_create_cq::Cdw11::Pc>(1);
		b.write<Nvme::Sqe_create_cq::Cdw11::En>(1);

		write<Admin_sdb::Sqt>(_admin_sq.tail);

		if (!_wait_for_admin_cq(10, CREATE_IO_CQ_CID)) {
			Genode::error("create I/O cq failed");
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
		if (!sq.valid()) { _setup_queue(sq, MAX_IO_ENTRIES, SQE_LEN); }

		Sqe_create_sq b(_admin_command(Opcode::CREATE_IO_SQ, 0, CREATE_IO_SQ_CID));
		b.write<Nvme::Sqe::Prp1>(sq.pa);
		b.write<Nvme::Sqe_create_sq::Cdw10::Qid>(id);
		b.write<Nvme::Sqe_create_sq::Cdw10::Qsize>(MAX_IO_ENTRIES_MASK);
		b.write<Nvme::Sqe_create_sq::Cdw11::Pc>(1);
		b.write<Nvme::Sqe_create_sq::Cdw11::Qprio>(0b00); /* urgent for now */
		b.write<Nvme::Sqe_create_sq::Cdw11::Cqid>(cqid);

		write<Admin_sdb::Sqt>(_admin_sq.tail);

		if (!_wait_for_admin_cq(10, CREATE_IO_SQ_CID)) {
			Genode::error("create I/O sq failed");
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
				Genode::error("fatal controller status");
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
	 */
	addr_t io_command(uint16_t id)
	{
		Nvme::Sq &sq = _sq[id];
		Nvme::Cq &cq = _cq[id];

		if (_queue_full(sq, cq)) { return 0ul; }

		Sqe e(sq.next());
		e.write<Nvme::Sqe::Cdw0::Cid>(sq.id++);
		e.write<Nvme::Sqe::Nsid>(id);
		return e.base();
	}

	/**
	 * Write current I/O submission queue tail
	 */
	void commit_io(uint16_t id)
	{
		Nvme::Sq &sq = _sq[id];
		write<Io_sdb::Sqt>(sq.tail);
	}

	/**
	 * Flush cache
	 */
	void flush_cache(uint16_t id)
	{
		(void)id;
	}

	/**
	 * Process every pending I/O completion
	 *
	 * \param func  function that is called on each completion
	 */
	template <typename FUNC>
	void handle_io_completions(uint16_t id, FUNC const &func)
	{
		Nvme::Cq &cq = _cq[id];

		if (!cq.valid()) { return; }

		for (;;) {
			Cqe e(cq.next());

			/* process until old phase */
			if (e.read<Nvme::Cqe::Sf::P>() != cq.phase) { break; }

			func(e);

			cq.advance_head();

			/*
			 * Instead of acknowledging the completions here,
			 * we could handle them batch-wise after the loop.
			 */
			write<Io_cdb::Cqh>(cq.head);
		}
	}

	/**
	 * Get memory page size in bytes
	 */
	size_t mps() const { return _mps; }

	/**
	 * Get block metrics of namespace
	 *
	 * \param nsid  namespace identifier
	 *
	 * \return  returns information of the namespace
	 */
	Nsinfo nsinfo(uint32_t id)
	{
		id = id - 1;
		if (id >= MAX_NS) { return Nsinfo(); }
		return _nsinfo[id];
	}

	/**
	 * Get controller information
	 */
	Info const &info() const { return _info; }

	/***********
	 ** Debug **
	 ***********/

	void dump_cap()
	{
		Genode::log("CAP:", "  ",
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

		Genode::log("VS: ", "  ", read<Vs::Mjr>(), ".",
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
	}

	void dump_nslist()
	{
		uint32_t const *p = (uint32_t const*)_nvme_nslist.va;
		if (!p) { return; }

		for (size_t i = 0; i < 1024; i++) {
			if (p[i] == 0) { break; }
			Genode::log("ns:#", p[i], " found");
		}
	}
};


/******************
 ** Block driver **
 ******************/

class Driver : public Block::Driver
{
	public:

		bool _verbose_checks   { false };
		bool _verbose_identify { false };
		bool _verbose_io       { false };
		bool _verbose_mem      { false };
		bool _verbose_regs     { false };

	private:

		Genode::Env       &_env;
		Genode::Allocator &_alloc;

		Genode::Signal_context_capability _announce_sigh;

		Genode::Attached_rom_dataspace _config_rom { _env, "config" };

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

					for (int i = 1; i <= Nvme::MAX_NS; i++) {
						Nvme::Controller::Nsinfo ns = _nvme_ctrlr->nsinfo(i);

						xml.node("namespace", [&]() {
							xml.attribute("id", i);
							xml.attribute("block_size",  ns.size);
							xml.attribute("block_count", ns.count);
						});
					}
				});
			} catch (...) { }
		}

		/*********
		 ** DMA **
		 *********/

		Genode::Constructible<Nvme::Pci> _nvme_pci { };

		struct Io_buffer
		{
			addr_t pa   { 0 };
			addr_t va   { 0 };
			size_t size { 0 };

			bool valid() const { return size && pa && va; }
			void invalidate() { Genode::memset(this, 0, sizeof(*this)); }
		};

		template <unsigned ENTRIES, size_t MPS>
		struct Io_buffer_mapper
		{
			using Bitmap = Util::Bitmap<ENTRIES>;
			Bitmap _bitmap { };

			Util::Slots<Io_buffer, Nvme::MAX_IO_ENTRIES> _buffers { };

			Genode::Ram_dataspace_capability _ds { };
			addr_t _phys_addr { 0 };
			addr_t _virt_addr { 0 };

			Io_buffer_mapper(Genode::Ram_dataspace_capability ds,
			                 addr_t phys, addr_t virt)
			: _ds(ds), _phys_addr(phys), _virt_addr(virt) { }

			Io_buffer *alloc(size_t size)
			{
				Io_buffer *iob = _buffers.get();
				if (!iob) { return nullptr; }

				try {
					size_t const bits  = size / MPS;
					addr_t const start = _bitmap.alloc(bits);
					iob->pa   = (start * MPS) + _phys_addr;
					iob->va   = (start * MPS) + _virt_addr;
					iob->size = size;
				} catch (...) {
					iob->invalidate();
					return nullptr;
				}
				return iob;
			}

			void free(Io_buffer *iob)
			{
				if (iob) {
					size_t const size = iob->size;
					addr_t const start = (iob->pa - _phys_addr) / MPS;
					_bitmap.free(start, size / MPS);
					iob->invalidate();
				}
			}
		};

		Genode::Constructible<Io_buffer_mapper<Nvme::DMA_DS_SIZE / Nvme::MPS,
		                                       Nvme::MPS>> _io_mapper { };

		Genode::Constructible<Io_buffer_mapper<Nvme::DMA_LIST_DS_SIZE / Nvme::MPS,
		                                       Nvme::MPS>> _io_list_mapper { };

		void _setup_large_request(addr_t           va,
		                          Io_buffer const &iob,
		                          size_t    const  num,
		                          size_t    const  mps)
		{
			/* omit first page */
			addr_t    pa = iob.pa + mps;
			uint64_t *p  = (uint64_t*)va;

			for (size_t i = 0; i < num; i++) {
				p[i]  = pa;
				pa   += mps;
			}
		}

		/**************
		 ** Requests **
		 **************/

		struct Request
		{
			uint32_t           id     { 0 };
			Packet_descriptor  pd     {   };
			char              *buffer { nullptr };

			Io_buffer *iob           { nullptr };
			Io_buffer *large_request { nullptr };

			bool valid() const { return id != 0; }

			void invalidate()
			{
				id     = 0;
				buffer = nullptr;
				pd     = Packet_descriptor();

				iob           = nullptr;
				large_request = nullptr;
			}
		};

		Util::Slots<Request, Nvme::MAX_IO_ENTRIES> _requests         {   };
		size_t                                     _requests_pending { 0 };

		/*********************
		 ** MMIO Controller **
		 *********************/

		struct Timer_delayer : Genode::Mmio::Delayer,
		                       Timer::Connection
		{
			Timer_delayer(Genode::Env &env)
			: Timer::Connection(env) { }

			void usleep(unsigned us) { Timer::Connection::usleep(us); }
		} _delayer { _env };

		Genode::Constructible<Nvme::Controller> _nvme_ctrlr { };

		void _handle_completions()
		{
			_nvme_ctrlr->handle_io_completions(Nvme::IO_NSID, [&] (Nvme::Cqe const &b) {

				if (_verbose_io) { Nvme::Cqe::dump(b); }

				uint32_t const id = Nvme::Cqe::request_id(b);

				Request *r = _requests.lookup([&] (Request &r) {
					if (r.id == id) { return true; }
					return false;
				});
				if (!r) {
					Genode::error("no pending request found for CQ entry");
					Nvme::Cqe::dump(b);
					return;
				}

				bool const succeeded = Nvme::Cqe::succeeded(b);

				Packet_descriptor pd = r->pd;
				pd.succeeded(succeeded);

				Io_buffer *iob = r->iob;

				if (succeeded && pd.operation() == Packet_descriptor::READ) {
					size_t const len = pd.block_count() * _block_size;
					Genode::memcpy(r->buffer, (void*)iob->va, len);
				}
				_io_mapper->free(iob);

				if (r->large_request) {
					_io_list_mapper->free(r->large_request);
				}

				r->invalidate();
				--_requests_pending;
				ack_packet(pd, succeeded);
			});
		}

		void _handle_intr()
		{
			_nvme_ctrlr->mask_intr();
			_handle_completions();
			_nvme_ctrlr->clear_intr();
			_nvme_pci->ack_irq();
		}

		Genode::Signal_handler<Driver> _intr_sigh {
			_env.ep(), *this, &Driver::_handle_intr };

		/***********
		 ** Block **
		 ***********/

		size_t                     _block_size  { 0 };
		Block::sector_t            _block_count { 0 };
		Block::Session::Operations _block_ops   {   };

	public:

		/**
		 * Constructor
		 */
		Driver(Genode::Env &env, Genode::Allocator &alloc,
		       Genode::Signal_context_capability sigh)
		: Block::Driver(env.ram()), _env(env), _alloc(alloc), _announce_sigh(sigh)
		{
			_config_rom.sigh(_config_sigh);
			_handle_config_update();

			/*
			 * Setup and identify NVMe PCI controller
			 */

			try {
				_nvme_pci.construct(_env);
			} catch (Nvme::Pci::Missing_controller) {
				Genode::error("no NVMe PCIe controller found");
				throw;
			}

			try {
				_nvme_ctrlr.construct(_env, *_nvme_pci, _nvme_pci->base(),
				                      _nvme_pci->size(), _delayer);
			} catch (...) {
				Genode::error("could not access NVMe controller MMIO");
				throw;
			}

			if (_verbose_regs) { _nvme_ctrlr->dump_cap(); }

			_nvme_ctrlr->init();
			_nvme_ctrlr->identify();

			if (_verbose_identify) {
				Genode::warning(_requests_pending);
				_nvme_ctrlr->dump_identify();
				_nvme_ctrlr->dump_nslist();
			}

			/*
			 * Setup I/O
			 */

			{
				Genode::Ram_dataspace_capability ds = _nvme_pci->alloc(Nvme::DMA_DS_SIZE);
				if (!ds.valid()) {
					Genode::error("could not allocate DMA backing store");
					throw Nvme::Controller::Initialization_failed();
				}
				addr_t const phys_addr = Genode::Dataspace_client(ds).phys_addr();
				addr_t const virt_addr = (addr_t)_env.rm().attach(ds);
				_io_mapper.construct(ds, phys_addr, virt_addr);

				if (_verbose_mem) {
					Genode::log("DMA", " virt: [", Genode::Hex(virt_addr), ",",
					                   Genode::Hex(virt_addr + Nvme::DMA_DS_SIZE), "]",
					                   " phys: [", Genode::Hex(phys_addr), ",",
					                   Genode::Hex(phys_addr + Nvme::DMA_DS_SIZE), "]");
				}
			}

			{
				Genode::Ram_dataspace_capability ds = _nvme_pci->alloc(Nvme::DMA_LIST_DS_SIZE);
				if (!ds.valid()) {
					Genode::error("could not allocate DMA list-pages backing store");
					throw Nvme::Controller::Initialization_failed();
				}
				addr_t const phys_addr = Genode::Dataspace_client(ds).phys_addr();
				addr_t const virt_addr = (addr_t)_env.rm().attach(ds);
				_io_list_mapper.construct(ds, phys_addr, virt_addr);

				if (_verbose_mem) {
					Genode::log("DMA list-pages", " virt: [", Genode::Hex(virt_addr), ",",
					                   Genode::Hex(virt_addr + Nvme::DMA_DS_SIZE), "]",
					                   " phys: [", Genode::Hex(phys_addr), ",",
					                   Genode::Hex(phys_addr + Nvme::DMA_DS_SIZE), "]");
				}
			}

			_nvme_ctrlr->setup_io(Nvme::IO_NSID, Nvme::IO_NSID);

			/* from now on use interrupts */
			_nvme_pci->sigh_irq(_intr_sigh);
			_nvme_ctrlr->clear_intr();

			/*
			 * Setup Block session
			 */

			/* set Block session properties */
			Nvme::Controller::Nsinfo nsinfo = _nvme_ctrlr->nsinfo(Nvme::IO_NSID);
			if (!nsinfo.valid()) {
				Genode::error("could not query namespace information");
				throw Nvme::Controller::Initialization_failed();
			}

			_block_count = nsinfo.count;
			_block_size  = nsinfo.size;

			_block_ops.set_operation(Packet_descriptor::READ);
			_block_ops.set_operation(Packet_descriptor::WRITE);

			Nvme::Controller::Info const &info = _nvme_ctrlr->info();

			Genode::log("NVMe:",  info.version.string(),   " "
			            "serial:'", info.sn.string(), "'", " "
			            "model:'",  info.mn.string(), "'", " "
			            "frev:'",   info.fr.string(), "'");

			Genode::log("Block",                " "
			            "size:",  _block_size,  " "
			            "count:", _block_count);

			/* generate Report if requested */
			try {
				Genode::Xml_node report = _config_rom.xml().sub_node("report");
				if (report.attribute_value("namespaces", false)) {
					_namespace_reporter.enabled(true);
					_report_namespaces();
				}
			} catch (...) { }

			/* finally announce Block session */
			Genode::Signal_transmitter(_announce_sigh).submit();
		}

		~Driver() { }

		/*******************************
		 **  Block::Driver interface  **
		 *******************************/

		size_t              block_size() override { return _block_size;  }
		Block::sector_t    block_count() override { return _block_count; }
		Block::Session::Operations ops() override { return _block_ops;   }

		void _io(bool write, Block::sector_t lba, size_t count,
		         char *buffer, Packet_descriptor &pd)
		{
			using namespace Genode;

			size_t const len = count * _block_size;

			if (_verbose_io) {
				Genode::error(write ? "write" : "read", " "
				              "lba:",           lba,    " "
				              "count:",         count,  " "
				              "buffer:", (void*)buffer, " "
				              "len:",           len);
			}

			if (len > Nvme::MAX_IO_LEN) {
				error("request too large (max:", (size_t)Nvme::MAX_IO_LEN, " bytes)");
				throw Io_error();
			}

			if (_requests_pending == (Nvme::MAX_IO_PENDING)) {
				throw Request_congestion();
			}

			Block::sector_t const lba_end = lba + count - 1;
			auto overlap_check = [&] (Request &req) {
				Block::sector_t const start = req.pd.block_number();
				Block::sector_t const end   = start + req.pd.block_count() - 1;

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
			if (_requests.for_each(overlap_check)) { throw Request_congestion(); }

			Request *r = _requests.get();
			if (!r) { throw Request_congestion(); }

			size_t const mps       = _nvme_ctrlr->mps();
			size_t const mps_len   = Genode::align_addr(len, Genode::log2(mps));
			bool   const need_list = len > 2 * mps;

			Io_buffer *iob = _io_mapper->alloc(mps_len);
			if (!iob) { throw Request_congestion(); }

			if (need_list) {
				r->large_request = _io_list_mapper->alloc(mps);
				if (!r->large_request) {
					_io_mapper->free(iob);
					throw Request_congestion();
				}
			}

			if (write) { Genode::memcpy((void*)iob->va, buffer, len); }

			Nvme::Sqe_io b(_nvme_ctrlr->io_command(Nvme::IO_NSID));
			if (!b.valid()) {
				if (r->large_request) {
					_io_list_mapper->free(r->large_request);
				}
				_io_mapper->free(iob);
				throw Request_congestion();
			}

			addr_t const pa = iob->pa;

			Nvme::Opcode op = write ? Nvme::Opcode::WRITE : Nvme::Opcode::READ;
			b.write<Nvme::Sqe::Cdw0::Opc>(op);
			b.write<Nvme::Sqe::Prp1>(pa);

			/* payload will fit into 2 mps chunks */
			if (len > mps && !r->large_request) {
				b.write<Nvme::Sqe::Prp2>(pa + mps);
			} else if (r->large_request) {
				/* payload needs list of mps chunks */
				Io_buffer &lr = *r->large_request;
				_setup_large_request(lr.va,
				                     *iob, (mps_len - mps)/mps, mps);
				b.write<Nvme::Sqe::Prp2>(lr.pa);
			}

			b.write<Nvme::Sqe_io::Slba>(lba);
			b.write<Nvme::Sqe_io::Cdw12::Nlb>(count - 1); /* 0-base value */

			r->iob    = iob;
			r->pd     = pd; /* must be a copy */
			r->buffer = write ? nullptr : buffer;
			r->id     = b.read<Nvme::Sqe_io::Cdw0::Cid>() | (Nvme::IO_NSID<<16);

			++_requests_pending;
			_nvme_ctrlr->commit_io(Nvme::IO_NSID);
		}

		void read(Block::sector_t lba, size_t count,
		          char *buffer, Packet_descriptor &pd) override
		{
			if (!_block_ops.supported(Packet_descriptor::READ)) {
				throw Io_error();
			}
			_io(false, lba, count, buffer, pd);
		}

		void write(Block::sector_t lba, size_t count,
		           char const *buffer, Packet_descriptor &pd) override
		{
			if (!_block_ops.supported(Packet_descriptor::WRITE)) {
				throw Io_error();
			}
			_io(true, lba, count, const_cast<char*>(buffer), pd);
		}

		void sync() override { _nvme_ctrlr->flush_cache(Nvme::IO_NSID); }
};


/**********
 ** Main **
 **********/

struct Main
{
	Genode::Env  &_env;
	Genode::Heap  _heap { _env.ram(), _env.rm() };

	void _handle_announce()
	{
		_env.parent().announce(_env.ep().manage(_root));
	}

	Genode::Signal_handler<Main> _announce_sigh {
		_env.ep(), *this, &Main::_handle_announce };

	struct Factory : Block::Driver_factory
	{
		Genode::Env       &_env;
		Genode::Allocator &_alloc;
		Genode::Signal_context_capability _sigh;

		Genode::Constructible<::Driver> _driver { };

		Factory(Genode::Env &env, Genode::Allocator &alloc,
		        Genode::Signal_context_capability sigh)
		: _env(env), _alloc(alloc), _sigh(sigh)
		{
			_driver.construct(_env, _alloc, _sigh);
		}

		~Factory() { _driver.destruct(); }

		Block::Driver *create() { return &*_driver; }
		void destroy(Block::Driver *) { }
	};

	Factory     _factory { _env, _heap, _announce_sigh };
	Block::Root _root    { _env.ep(), _heap, _env.rm(), _factory, true };

	Main(Genode::Env &env) : _env(env) { }
};


void Component::construct(Genode::Env &env) { static Main main(env); }
