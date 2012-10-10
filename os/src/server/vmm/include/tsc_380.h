/*
 * \brief  Driver for the CoreLink Trustzone Address Space Controller TSC-380
 * \author Stefan Kalkowski
 * \date   2012-07-04
 */

/*
 * Copyright (C) 2012 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _BASE_HW__SRC__SERVER__VMM__TSC_380_H_
#define _BASE_HW__SRC__SERVER__VMM__TSC_380_H_

/* Genode includes */
#include <util/mmio.h>

namespace Genode
{

	class Tsc_380 : Mmio
	{
		private:

			enum {
				REGION0_REG_OFF  = 0x100,
				REGION1_REG_OFF  = 0x110,
				REGION2_REG_OFF  = 0x120,
				REGION3_REG_OFF  = 0x130,
				REGION4_REG_OFF  = 0x140,
				REGION5_REG_OFF  = 0x150,
				REGION6_REG_OFF  = 0x160,
				REGION7_REG_OFF  = 0x170,
				REGION8_REG_OFF  = 0x180,
				REGION9_REG_OFF  = 0x190,
				REGION10_REG_OFF = 0x1a0,
				REGION11_REG_OFF = 0x1b0,
				REGION12_REG_OFF = 0x1c0,
				REGION13_REG_OFF = 0x1d0,
				REGION14_REG_OFF = 0x1e0,
				REGION15_REG_OFF = 0x1f0,

				REGION_LOW_OFF   = 0x0,
				REGION_HIGH_OFF  = 0x4,
				REGION_ATTR_OFF  = 0x8,
			};

			/**
			 * Configuration register
			 */
			struct Config : public Register<0, 32>
			{
				struct Region_number : Bitfield<0,4>  { };
				struct Address_width : Bitfield<8,6>  { };
			};

			struct Irq_status : public Register<0x10, 32>
			{
				struct Status  : Bitfield<0,1> {};
				struct Overrun : Bitfield<1,1> {};
			};

			struct Irq_clear  : public Register<0x14, 32>
			{
				struct Status  : Bitfield<0,1> {};
				struct Overrun : Bitfield<1,1> {};
			};

			/**
			 * Fail address low register
			 */
			struct Fail_low : public Register<0x20, 32> { };

			template <off_t OFF>
			struct Region_low : public Register<OFF + REGION_LOW_OFF, 32>
			{
				enum { MASK = ~0UL << 15 };
			};

			template <off_t OFF>
			struct Region_high : public Register<OFF + REGION_HIGH_OFF, 32> { };

			template <off_t OFF>
			struct Region_attr : public Register<OFF + REGION_ATTR_OFF, 32>
			{
				struct Enable :
					Register<OFF + REGION_ATTR_OFF, 32>::template Bitfield<0,  1> { };
				struct Size :
					Register<OFF + REGION_ATTR_OFF, 32>::template Bitfield<1,  6>
				{
					enum {
						SZ_32K = 14,
						SZ_64K,
						SZ_128K,
						SZ_256K,
						SZ_512K,
						SZ_1M,
						SZ_2M,
						SZ_4M,
						SZ_8M,
						SZ_16M,
						SZ_32M,
						SZ_64M,
						SZ_128M,
						SZ_256M,
						SZ_512M,
						SZ_1G,
					};
				};
				struct Subreg0 :
					Register<OFF + REGION_ATTR_OFF, 32>::template Bitfield<8,  1> { };
				struct Subreg1 :
					Register<OFF + REGION_ATTR_OFF, 32>::template Bitfield<9,  1> { };
				struct Subreg2 :
					Register<OFF + REGION_ATTR_OFF, 32>::template Bitfield<10, 1> { };
				struct Subreg3 :
					Register<OFF + REGION_ATTR_OFF, 32>::template Bitfield<11, 1> { };
				struct Subreg4 :
					Register<OFF + REGION_ATTR_OFF, 32>::template Bitfield<12, 1> { };
				struct Subreg5 :
					Register<OFF + REGION_ATTR_OFF, 32>::template Bitfield<13, 1> { };
				struct Subreg6 :
					Register<OFF + REGION_ATTR_OFF, 32>::template Bitfield<14, 1> { };
				struct Subreg7 :
					Register<OFF + REGION_ATTR_OFF, 32>::template Bitfield<15, 1> { };
				struct Normal_write :
					Register<OFF + REGION_ATTR_OFF, 32>::template Bitfield<28, 1> { };
				struct Normal_read :
					Register<OFF + REGION_ATTR_OFF, 32>::template Bitfield<29, 1> { };
				struct Secure_write :
					Register<OFF + REGION_ATTR_OFF, 32>::template Bitfield<30, 1> { };
				struct Secure_read :
					Register<OFF + REGION_ATTR_OFF, 32>::template Bitfield<31, 1> { };
			};

			typedef Region_low<0x100> Region0_low;

		public:

			Tsc_380(addr_t const base) : Mmio(base)
			{
				/* Access to AACI, MMCI, KMI0/1 */
				write<Region_low<REGION15_REG_OFF> >(0x10000000);
				write<Region_high<REGION15_REG_OFF> >(0x10008000);
				write<Region_attr<REGION15_REG_OFF>::Enable>(1);
				write<Region_attr<REGION15_REG_OFF>::Size>(Region_attr<REGION15_REG_OFF>::Size::SZ_32K);
				write<Region_attr<REGION15_REG_OFF>::Normal_read>(1);
				write<Region_attr<REGION15_REG_OFF>::Normal_write>(1);
				write<Region_attr<REGION15_REG_OFF>::Secure_read>(1);
				write<Region_attr<REGION15_REG_OFF>::Secure_write>(1);
				write<Region_attr<REGION15_REG_OFF>::Subreg0>(1);
				write<Region_attr<REGION15_REG_OFF>::Subreg1>(1);
				write<Region_attr<REGION15_REG_OFF>::Subreg2>(1);
				write<Region_attr<REGION15_REG_OFF>::Subreg3>(1);

				/* Access to UART3, and WDT */
				write<Region_low<REGION14_REG_OFF> >(0x10008000);
				write<Region_high<REGION14_REG_OFF> >(0x10010000);
				write<Region_attr<REGION14_REG_OFF>::Enable>(1);
				write<Region_attr<REGION14_REG_OFF>::Size>(Region_attr<REGION14_REG_OFF>::Size::SZ_32K);
				write<Region_attr<REGION14_REG_OFF>::Normal_read>(1);
				write<Region_attr<REGION14_REG_OFF>::Normal_write>(1);
				write<Region_attr<REGION14_REG_OFF>::Secure_read>(1);
				write<Region_attr<REGION14_REG_OFF>::Secure_write>(1);
				write<Region_attr<REGION14_REG_OFF>::Subreg0>(1);
				write<Region_attr<REGION14_REG_OFF>::Subreg1>(1);
				write<Region_attr<REGION14_REG_OFF>::Subreg2>(1);
				write<Region_attr<REGION14_REG_OFF>::Subreg3>(1);
				write<Region_attr<REGION14_REG_OFF>::Subreg5>(1);
				write<Region_attr<REGION14_REG_OFF>::Subreg6>(1);

				/* Access to SP804, and RTC */
				write<Region_low<REGION13_REG_OFF> >(0x10010000);
				write<Region_high<REGION13_REG_OFF> >(0x10018000);
				write<Region_attr<REGION13_REG_OFF>::Enable>(1);
				write<Region_attr<REGION13_REG_OFF>::Size>(Region_attr<REGION13_REG_OFF>::Size::SZ_32K);
				write<Region_attr<REGION13_REG_OFF>::Normal_read>(1);
				write<Region_attr<REGION13_REG_OFF>::Normal_write>(1);
				write<Region_attr<REGION13_REG_OFF>::Secure_read>(1);
				write<Region_attr<REGION13_REG_OFF>::Secure_write>(1);
				write<Region_attr<REGION13_REG_OFF>::Subreg0>(1);
				write<Region_attr<REGION13_REG_OFF>::Subreg3>(1);
				write<Region_attr<REGION13_REG_OFF>::Subreg4>(1);
				write<Region_attr<REGION13_REG_OFF>::Subreg5>(1);
				write<Region_attr<REGION13_REG_OFF>::Subreg6>(1);

				/* Access to Ethernet and USB */
				write<Region_low<REGION12_REG_OFF> >(0x4e000000);
				write<Region_high<REGION12_REG_OFF> >(0x50000000);
				write<Region_attr<REGION12_REG_OFF>::Enable>(1);
				write<Region_attr<REGION12_REG_OFF>::Size>(Region_attr<REGION12_REG_OFF>::Size::SZ_32M);
				write<Region_attr<REGION12_REG_OFF>::Normal_read>(1);
				write<Region_attr<REGION12_REG_OFF>::Normal_write>(1);
				write<Region_attr<REGION12_REG_OFF>::Secure_read>(1);
				write<Region_attr<REGION12_REG_OFF>::Secure_write>(1);

				/* clear interrupts */
				write<Irq_clear>(0x3);
			}

			void* last_failed_access() {
				void *ret = (void*) read<Fail_low>();
				write<Irq_clear>(0x3);
				return ret;
			}
	};

}

#endif /* _BASE_HW__SRC__SERVER__VMM__TSC_380_H_ */
