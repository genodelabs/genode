/*
 * \brief  Intel IOMMU invalidation interfaces
 * \author Johannes Schlatow
 * \date   2024-03-21
 */

/*
 * Copyright (C) 2024 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _SRC__DRIVERS__PLATFORM__INTEL__INVALIDATOR_H_
#define _SRC__DRIVERS__PLATFORM__INTEL__INVALIDATOR_H_

/* Genode includes */
#include <util/mmio.h>
#include <base/attached_ram_dataspace.h>
#include <pci/types.h>

/* local includes */
#include <intel/domain_allocator.h>

namespace Intel {
	using namespace Genode;

	class Io_mmu; /* forward declaration */
	class Invalidator;
	class Register_invalidator;
	class Queued_invalidator;
}


class Intel::Invalidator
{
	public:

		virtual ~Invalidator() { }

		virtual void invalidate_irq(unsigned, bool) { };
		virtual void invalidate_iotlb(Domain_id) = 0;
		virtual void invalidate_context(Domain_id domain, Pci::rid_t) = 0;
		virtual void invalidate_all(Domain_id domain = Domain_id { Domain_id::INVALID },
		                            Pci::rid_t = 0) = 0;
};


class Intel::Register_invalidator : public Invalidator
{
	private:

		struct Context_mmio : Mmio<8>
		{
			struct Context_command : Register<0x0, 64>
			{
				struct Invalidate : Bitfield<63,1> { };

				/* invalidation request granularity */
				struct Cirg       : Bitfield<61,2>
				{
					enum {
						GLOBAL = 0x1,
						DOMAIN = 0x2,
						DEVICE = 0x3
					};
				};

				/* actual invalidation granularity */
				struct Caig      : Bitfield<59,2> { };

				/* source id */
				struct Sid       : Bitfield<16,16> { };

				/* domain id */
				struct Did       : Bitfield<0,16> { };
			};

			Context_mmio(Byte_range_ptr const &range)
			: Mmio<8>(range)
			{ }
		} _context_mmio;

		struct Iotlb_mmio : Mmio<16>
		{
			struct Iotlb : Register<0x8, 64>
			{
				struct Invalidate : Bitfield<63,1> { };

				/* IOTLB invalidation request granularity */
				struct Iirg : Bitfield<60,2>
				{
					enum {
						GLOBAL = 0x1,
						DOMAIN = 0x2,
						DEVICE = 0x3
					};
				};

				/* IOTLB actual invalidation granularity */
				struct Iaig : Bitfield<57,2> { };

				/* drain reads/writes */
				struct Dr   : Bitfield<49,1> { };
				struct Dw   : Bitfield<48,1> { };

				/* domain id */
				struct Did  : Bitfield<32,16> { };
			};

			Iotlb_mmio(Byte_range_ptr const &range)
			: Mmio<16>(range)
			{ }
		} _iotlb_mmio;

		bool            _verbose;

	public:

		void invalidate_iotlb(Domain_id) override;
		void invalidate_context(Domain_id domain, Pci::rid_t) override;
		void invalidate_all(Domain_id domain = Domain_id { Domain_id::INVALID },
		                    Pci::rid_t = 0) override;

		Register_invalidator(addr_t context_reg_base, addr_t iotlb_reg_base, bool verbose)
		: _context_mmio({(char*)context_reg_base, 8}),
		  _iotlb_mmio  ({(char*)iotlb_reg_base,  16}),
		  _verbose(verbose)
		{ }
};


class Intel::Queued_invalidator : public Invalidator
{
	private:

		struct Queue_mmio : Mmio<56>
		{
			struct Head : Register<0x0, 64>
			{
			};

			struct Tail : Register<0x8, 64>
			{
			};

			struct Address : Register<0x10, 64>
			{
				struct Size  : Bitfield< 0,3>  { enum { SIZE_4K   = 0}; };
				struct Width : Bitfield<11,1>  { enum { WIDTH_128 = 0}; };
				struct Base  : Bitfield<12,52> { };
			};

			Queue_mmio(Byte_range_ptr const &range)
			: Mmio<56>(range)
			{ }

		} _queue_mmio;

		Attached_ram_dataspace _queue;

		/*
		 * descriptor definitions
		 */

		struct Descriptor : Genode::Register<64>
		{
			struct Type_0_3 : Bitfield<0,4> { };
			struct Type_4_6 : Bitfield<9,3> { };
			struct Type     : Bitset_2<Type_0_3, Type_4_6>
			{
				enum {
					CONTEXT = 1,
					IOTLB   = 2,
					IEC     = 4
				};
			};

			struct Granularity : Bitfield<4,2> {
				enum {
					GLOBAL = 0x1,
					DOMAIN = 0x2,
					DEVICE_OR_PAGE = 0x3
				};
			};

			struct Domain_id : Bitfield<16,16> { };
		};

		struct Context : Descriptor
		{
			struct Source_id : Bitfield<32,16> { };
		};

		struct Iotlb : Descriptor
		{
			/* drain reads/writes */
			struct Dw   : Bitfield<6,1> { };
			struct Dr   : Bitfield<7,1> { };
		};

		struct Iec : Descriptor
		{
			struct Global : Bitfield<4,1> {
				enum {
					GLOBAL = 0,
					INDEX  = 1
				};
			};
			struct Index  : Bitfield<32,16> { };
		};

		bool _empty() {
			return _queue_mmio.read<Queue_mmio::Head>() == _queue_mmio.read<Queue_mmio::Tail>(); }

		Descriptor::access_t *_tail() {
			return (Descriptor::access_t*)(_queue_mmio.read<Queue_mmio::Tail>() + (addr_t)_queue.local_addr<void>()); }

		void _next()
		{
			addr_t tail_offset = _queue_mmio.read<Queue_mmio::Tail>();

			tail_offset += 16;
			if (tail_offset >= 0x1000)
				tail_offset = 0;

			_queue_mmio.write<Queue_mmio::Tail>(tail_offset);
		}

	public:

		void invalidate_irq(unsigned, bool) override;
		void invalidate_iotlb(Domain_id) override;
		void invalidate_context(Domain_id domain, Pci::rid_t) override;
		void invalidate_all(Domain_id domain = Domain_id { Domain_id::INVALID },
		                    Pci::rid_t = 0) override;

		Queued_invalidator(Genode::Env & env, addr_t queue_reg_base)
		: _queue_mmio({(char*)queue_reg_base, 56}),
		  _queue(env.ram(), env.rm(), 4096, Cache::CACHED)
		{
			/* set tail register to zero*/
			_queue_mmio.write<Queue_mmio::Tail>(0);

			/* write physical address of invalidation queue into address register */
			using Address = Queue_mmio::Address;
			addr_t queue_paddr = env.pd().dma_addr(_queue.cap());
			_queue_mmio.write<Address>(Address::Base::masked(queue_paddr) |
			                           Address::Size::bits(Address::Size::SIZE_4K) |
			                           Address::Width::bits(Address::Width::WIDTH_128));
		}
};


#endif /* _SRC__DRIVERS__PLATFORM__INTEL__INVALIDATOR_H_ */
