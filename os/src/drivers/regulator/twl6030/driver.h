/*
 * \brief  Driver for Twl6030 Level Regulator
 * \author Alexander Tarasikov <tarasikov@ksyslabs.org>
 * \date   2013-02-18
 */

/*
 * Copyright (C) 2013 Ksys Labs LLC
 * Copyright (C) 2011-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef __OS__DRIVERS__REGULATOR__TWL6030__DRIVER_H__
#define __OS__DRIVERS__REGULATOR__TWL6030__DRIVER_H__

/* Genode includes */
#include <base/printf.h>
#include <base/sleep.h>

#include <i2c_session/connection.h>
#include <drivers/mfd/twl6030/regulator.h>

#include "../regulator_component.h"
#include "../regulator_driver.h"

using namespace Genode;

namespace Regulator {

/******************************************************************************
 * TWL6030 Specific Code
 ******************************************************************************/
enum {
	TWL6030_ADDR_MOD0 = 0x48,
};

class Twl6030_Regulator : public AbstractRegulator {
protected:
	uint8_t mOffset;
	I2C::Connection &mi2c;
	
	bool write_u8(uint8_t reg, uint8_t val) {
		return mi2c.write(TWL6030_ADDR_MOD0, reg + mOffset, &val, 1);
	}

	bool read_u8(uint8_t reg, uint8_t *val) {
		return mi2c.read(TWL6030_ADDR_MOD0, reg + mOffset, val, 1);
	}

	bool set_trans_state(uint8_t shift, uint8_t value) {
		uint8_t trans;
		uint8_t mask;

		if (!read_u8(TWL6030::VREG_TRANS, &trans)) {
			return false;
		}

		mask = TWL6030::CFG_TRANS_STATE_MASK << shift;
		value = (value << shift) & mask;

		if (value == (trans & mask)) {
			return true;
		}

		trans &= ~mask;
		trans |= value;
		
		return write_u8(TWL6030::VREG_TRANS, trans);
	}

public:
	Twl6030_Regulator(unsigned id, const char *name,
		unsigned min_uV, unsigned max_uV, uint8_t offset,
		I2C::Connection &i2c)
		: AbstractRegulator(id, name), mOffset(offset), mi2c(i2c)
	{
		mMin = min_uV;
		mMax = max_uV;
	}

	~Twl6030_Regulator() {}

	virtual enum regulator_state get_state(void) {
		uint8_t state;
		if (!read_u8(TWL6030::VREG_STATE, &state)) {
			return STATE_ERROR;
		}
		
		switch (TWL6030::CFG_STATE_APP(state)) {
			case TWL6030::CFG_STATE_ON:
				return STATE_ON;
			case TWL6030::CFG_STATE_SLEEP:
				return STATE_SLEEP;
		}
		return STATE_OFF;
	}

	virtual bool set_state(enum regulator_state state) {
		uint8_t grp;
		uint8_t statev;

		if (!read_u8(TWL6030::VREG_GRP, &grp)) {
			return false;
		}
		statev = grp << TWL6030::CFG_STATE_GRP_SHIFT;

		switch (state) {
			case STATE_ON:
				statev |= TWL6030::CFG_STATE_ON;
				break;
			case STATE_SLEEP:
				statev |= TWL6030::CFG_STATE_SLEEP; 
			default:
				return false;
		}

		return write_u8(TWL6030::VREG_STATE, statev);
	}

	virtual bool is_enabled(void) {
		uint8_t grp;
		uint8_t val;

		if (!read_u8(TWL6030::VREG_GRP, &grp)) {
			return false;
		}

		read_u8(TWL6030::VREG_STATE, &val);
		val = TWL6030::CFG_STATE_APP(val);

		return (val == TWL6030::CFG_STATE_ON) && (grp & TWL6030::GRP_P1);
	}

	virtual bool raw_enable(void) {
		uint8_t grp;
		if (!read_u8(TWL6030::VREG_GRP, &grp)) {
			return false;
		}
		
		bool ret = write_u8(TWL6030::VREG_STATE,
			grp << TWL6030::CFG_STATE_GRP_SHIFT | TWL6030::CFG_STATE_ON);

		if (!ret) {
			return false;
		}

		ret = set_trans_state(TWL6030::CFG_TRANS_SLEEP_SHIFT,
			TWL6030::CFG_TRANS_STATE_AUTO);

		return ret;
	}

	virtual bool raw_disable(void) {
		uint8_t grp =
			TWL6030::GRP_P1|TWL6030::GRP_P2|TWL6030::GRP_P3;

		bool ret = write_u8(TWL6030::VREG_STATE,
			(grp << TWL6030::CFG_STATE_GRP_SHIFT) |	TWL6030::CFG_STATE_OFF);
	
		if (!ret) {
			return false;
		}

		ret = set_trans_state(TWL6030::CFG_TRANS_SLEEP_SHIFT,
			TWL6030::CFG_TRANS_STATE_OFF);

		return ret;
	}
};
	
class Twl6030_RegulatorLDO : public Twl6030_Regulator {
public:
	Twl6030_RegulatorLDO(unsigned id, const char *name, uint8_t offset,
		I2C::Connection &i2c)
		: Twl6030_Regulator(id, name,
			TWL6030::LDO_MIN_VOLTAGE_MV * 1000,
			TWL6030::LDO_MAX_VOLTAGE_MV * 1000, offset, i2c)
	{
			mNumLevelSteps = TWL6030::LDO_NUM_VOLTAGE_STEPS;
	}

	virtual int get_level(void) {
		uint8_t vsel;

		if (!read_u8(TWL6030::VREG_VOLTAGE, &vsel)) {
			return -1;
		}

		//1000mV + 100mV * (vsel - 1)
		return (TWL6030::LDO_MIN_VOLTAGE_MV +
			(TWL6030::LDO_VOLTAGE_STEP) * (vsel - 1)) * 1000;
	}

	virtual bool set_level(unsigned min_uV, unsigned max_uV) {
		if (min_uV < mMin || max_uV > mMax) {
			return false;
		}

		uint8_t vsel = 1 + ((min_uV / 1000) - TWL6030::LDO_MIN_VOLTAGE_MV)
			/ TWL6030::LDO_VOLTAGE_STEP;
		return write_u8(TWL6030::VREG_VOLTAGE, vsel);
	}
};

class Twl6030_RegulatorRES : public Twl6030_Regulator {
public:
	Twl6030_RegulatorRES(unsigned id, const char *name, uint8_t offset,
		I2C::Connection &i2c)
		: Twl6030_Regulator(id, name, 0, 0, offset, i2c)
	{}

	virtual bool set_state(enum regulator_state state) {
		//unsupported
		return false;
	}
};

class Twl6030_RegulatorSMPS : public Twl6030_Regulator {
public:
	Twl6030_RegulatorSMPS(unsigned id, const char *name, uint8_t offset,
		unsigned min_uV, unsigned max_uV, I2C::Connection &i2c)
		: Twl6030_Regulator(id, name, min_uV, max_uV, offset, i2c)
	{
		mNumLevelSteps = 63;
	}

	virtual int get_level(void) {
		uint8_t vsel;
		int level = 0;

		if (!read_u8(TWL6030::VREG_VOLTAGE_SMPS, &vsel)) {
			return -1;
		}
		
		switch (vsel) {
		case 0:
			level = 0;
			break;
		case 58:
			level = 1350 * 1000;
			break;
		case 59:
			level = 1500 * 1000;
			break;
		case 60:
			level = 1800 * 1000;
			break;
		case 61:
			level = 1900 * 1000;
			break;
		case 62:
			level = 2100 * 1000;
			break;
		default:
			level = (600000 + (12500 * (vsel - 1)));
		}

		return level;
	}

	virtual bool set_level(unsigned min_uV, unsigned max_uV) {
		uint8_t vsel;

		if (min_uV == 0)
			vsel = 0;
		else if ((min_uV >= 600000) && (max_uV <= 1300000)) {
			vsel = (min_uV - 600000) / 125;
			if (vsel % 100)
				vsel += 100;
			vsel /= 100;
			vsel++;
		}
		/* Values 1..57 for vsel are linear and can be calculated
		 * values 58..62 are non linear.
		 */
		else if ((min_uV > 1900000) && (max_uV >= 2100000))
			vsel = 62;
		else if ((min_uV > 1800000) && (max_uV >= 1900000))
			vsel = 61;
		else if ((min_uV > 1500000) && (max_uV >= 1800000))
			vsel = 60;
		else if ((min_uV > 1350000) && (max_uV >= 1500000))
			vsel = 59;
		else if ((min_uV > 1300000) && (max_uV >= 1350000))
			vsel = 58;
		else
			return false;

		return write_u8(TWL6030::VREG_VOLTAGE_SMPS, vsel);
	}
};

/******************************************************************************
 * The actual driver
 ******************************************************************************/
class Twl6030_driver_factory : public Regulator::Driver_factory {
protected:
	AbstractRegulator **mRegulators;
	unsigned mNumRegulators;

public:
	Twl6030_driver_factory(I2C::Connection &i2c) {
		mRegulators = new (env()->heap()) 
			AbstractRegulator*[TWL6030::MAX_REGULATOR_COUNT];
		mNumRegulators = TWL6030::MAX_REGULATOR_COUNT;
		Genode::memset(mRegulators, 0,
			TWL6030::MAX_REGULATOR_COUNT * sizeof(AbstractRegulator*));

		int idx = 0;

		#define TWL_ADJUSTABLE_LDO(ID, offset) \
			mRegulators[idx++] = new (env()->heap()) \
			Twl6030_RegulatorLDO(TWL6030::ID, #ID, offset, i2c);

		#define TWL_FIXED_LDO(ID, offset, uV) \
			mRegulators[idx++] = new (env()->heap()) \
			Twl6030_Regulator(TWL6030::ID, #ID, uV, uV, offset, i2c);

		#define TWL_RESOURCE(ID, offset) \
			mRegulators[idx++] = new (env()->heap()) \
			Twl6030_RegulatorRES(TWL6030::ID, #ID, offset, i2c);

		#define TWL_SMPS(ID, offset, min_uV, max_uV) \
			mRegulators[idx++] = new (env()->heap()) \
			Twl6030_RegulatorSMPS(TWL6030::ID, #ID, offset, min_uV, max_uV, i2c);

		TWL_ADJUSTABLE_LDO(VAUX1, 0x84);
		TWL_ADJUSTABLE_LDO(VAUX2, 0x88);
		TWL_ADJUSTABLE_LDO(VAUX3, 0x8c);
		TWL_ADJUSTABLE_LDO(VMMC, 0x98);
		TWL_ADJUSTABLE_LDO(VPP, 0x9c);
		TWL_ADJUSTABLE_LDO(VUSIM, 0xa4);
		TWL_FIXED_LDO(VANA, 0x80, 2100 * 1000);
		TWL_FIXED_LDO(VCXIO, 0x90, 1800 * 1000);
		TWL_FIXED_LDO(VDAC, 0x94, 1800 * 1000);
		TWL_FIXED_LDO(VUSB, 0xa0, 3300 * 1000);
		TWL_RESOURCE(CLK32KG, 0xbc);
		TWL_RESOURCE(CLK32KAUDIO, 0xbf);
		TWL_SMPS(VDD3, 0x5e, 600 * 1000, 4000 * 1000);
		TWL_SMPS(VMEM, 0x64, 600 * 1000, 4000 * 1000);
		TWL_SMPS(V2V1, 0x4c, 1800 * 1000, 2100 * 1000);
	}

	Driver* create(NameList *allowed_regulators) {
		return new (env()->heap())
			Driver(allowed_regulators, mRegulators, mNumRegulators);
	}

	void destroy(Regulator::Driver *driver) {
		::destroy(env()->heap(), driver);
	}
};

} //namespace Regulator

#endif //__OS__DRIVERS__REGULATOR__TWL6030__DRIVER_H__
