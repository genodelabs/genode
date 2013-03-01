/*
 * \brief  TWL6030 definitions
 * \author Alexander Tarasikov <tarasikov@ksyslabs.org>
 * \date   2012-02-15
 */

/*
 * Copyright (C) 2012 Ksys Labs LLC
 * Copyright (C) 2012 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef __OS__INCLUDE__DRIVERS__MFD__TWL6030__REGULATOR_H__
#define __OS__INCLUDE__DRIVERS__MFD__TWL6030__REGULATOR_H__

namespace Regulator {
	namespace TWL6030 {

		enum regulator_id {
			/* adjustable LDOs */
			VAUX1,
			VAUX2,
			VAUX3,
			VMMC,
			VPP,
			VUSIM,

			/* fixed LDOs */
			VANA,
			VCXIO,
			VDAC,
			VUSB,

			/* fixed resources */
			CLK32KG,
			CLK32KAUDIO,

			/* adjustable SMPSs */
			VDD3,
			VMEM,
			V2V1,

			MAX_REGULATOR_COUNT,
		};

		enum twl_module {
			PM_RECEIVER,
		};

		enum twl_register_bits {
			VREG_GRP = 0,
			VREG_TRANS = 1,
			VREG_STATE = 2,
			VREG_VOLTAGE = 3,
			VREG_VOLTAGE_SMPS = 4,

			/* regulator GRP (ownership) */
			GRP_P3	= (1 << 2), /* modem */
			GRP_P2	= (1 << 1), /* peripherals */
			GRP_P1 = (1 << 0), /* omap4 */
			
			/* CFG_TRANS register */
			CFG_TRANS_STATE_MASK = 0x3,
			CFG_TRANS_STATE_OFF = 0,
			CFG_TRANS_STATE_AUTO = 1,
			CFG_TRANS_SLEEP_SHIFT = 2,

			/* CFG_STATE register */
			CFG_STATE_OFF = 0x00,
			CFG_STATE_ON  = 0x01,
			CFG_STATE_OFF2 = 0x02,
			CFG_STATE_SLEEP = 0x03,
			CFG_STATE_GRP_SHIFT = 5,
			CFG_STATE_APP_SHIFT = 2,
			CFG_STATE_APP_MASK = (0x03 << CFG_STATE_APP_SHIFT),
		};

		static inline Genode::uint8_t CFG_STATE_APP(Genode::uint8_t v) {
			return (v & CFG_STATE_APP_MASK) >> CFG_STATE_APP_SHIFT;
		}

		enum {
			LDO_MIN_VOLTAGE_MV = 1000,
			LDO_MAX_VOLTAGE_MV = 3300,
			LDO_VOLTAGE_STEP = 100,
			LDO_NUM_VOLTAGE_STEPS = 33,

			SMPS_NUM_VOLTAGE_STEPS = 63,
		};

	} //namespace TWL6030
} //namespace Regulator

#endif //__OS__INCLUDE__DRIVERS__MFD__TWL6030__REGULATOR_H__
