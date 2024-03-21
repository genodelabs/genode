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
#include <pci/types.h>

/* local includes */
#include <intel/domain_allocator.h>

namespace Intel {
	using namespace Genode;

	class Io_mmu; /* forward declaration */
	class Invalidator;
	class Register_invalidator;
}


class Intel::Invalidator
{
	public:

		virtual ~Invalidator() { }

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


#endif /* _SRC__DRIVERS__PLATFORM__INTEL__INVALIDATOR_H_ */
