/*
 * \brief  RTC/CMOS clock driver
 * \author Christian Helmuth
 * \author Markus Partheymueller
 * \date   2007-04-18
 */

/*
 * Copyright (C) 2007-2019 Genode Labs GmbH
 * Copyright (C) 2012 Intel Corporation
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Genode includes */
#include <io_port_session/connection.h>

/* local includes */
#include "rtc.h"

using namespace Genode;


enum RTC
{
	RTC_SECONDS       = 0,
	RTC_SECONDS_ALARM = 1,
	RTC_MINUTES       = 2,
	RTC_MINUTES_ALARM = 3,
	RTC_HOURS         = 4,
	RTC_HOURS_ALARM   = 5,
	RTC_DAY_OF_WEEK   = 6,
	RTC_DAY_OF_MONTH  = 7,
	RTC_MONTH         = 8,
	RTC_YEAR          = 9,

	RTC_REG_A         = 10,
	RTC_REG_B         = 11,
	RTC_REG_C         = 12,
	RTC_REG_D         = 13,

	RTC_FREQ_SELECT   = RTC_REG_A,
		RTC_UIP = 0x80,
		RTC_DIV_CTL = 0x70,
			RTC_REF_CLCK_4MHZ  = 0x00,
			RTC_REF_CLCK_1MHZ  = 0x10,
			RTC_REF_CLCK_32KHZ = 0x20,
			RTC_DIV_RESET1     = 0x60,
			RTC_DIV_RESET2     = 0x70,
		RTC_RATE_SELECT = 0x0F,

	RTC_CONTROL       = RTC_REG_B,
		RTC_SET       = 0x80,
		RTC_PIE       = 0x40,
		RTC_AIE       = 0x20,
		RTC_UIE       = 0x10,
		RTC_SQWE      = 0x08,
		RTC_DM_BINARY = 0x04,
		RTC_24H       = 0x02,
		RTC_DST_EN    = 0x01,

	RTC_PORT_BASE     = 0x70,
		RTC_PORT_ADDR = RTC_PORT_BASE,
		RTC_PORT_DATA = RTC_PORT_BASE + 1,
	RTC_PORT_SIZE     = 2,
};


namespace Rtc { struct Driver; }


struct Rtc::Driver : Noncopyable
{
	Env &_env;

	Io_port_connection _ports { _env, RTC_PORT_BASE, RTC_PORT_SIZE };

	uint8_t _cmos_read(uint8_t addr)
	{
		_ports.outb(RTC_PORT_ADDR, addr);
		return _ports.inb(RTC_PORT_DATA);
	}

	void _cmos_write(uint8_t addr, uint8_t value)
	{
		_ports.outb(RTC_PORT_ADDR, addr);
		_ports.outb(RTC_PORT_DATA, value);
	}

	Driver(Env &env) : _env(env) { }

	static void init_singleton(Env &);

	Timestamp read_timestamp();

	void write_timestamp(Timestamp);
};


#define RTC_ALWAYS_BCD   1 /* RTC operates in binary mode */
#define BCD_TO_BIN(val)  ((val) = ((val) & 15) + ((val) >> 4) * 10)
#define BIN_TO_BCD(val)  ((val) = (((val)/10) << 4) + (val) % 10)


Rtc::Timestamp Rtc::Driver::read_timestamp()
{
	/* The Linux interpretation of the CMOS clock register contents:
	 * When the Update-In-Progress (UIP) flag goes from 1 to 0, the
	 * RTC registers show the second which has precisely just started.
	 * Let's hope other operating systems interpret the RTC the same way. */

	/* read RTC exactly on falling edge of update flag */

	unsigned const MAX_ITERATIONS = 1000000;
	unsigned i;
	for (i = 0; i < MAX_ITERATIONS; i++)
		if (_cmos_read(RTC_FREQ_SELECT) & RTC_UIP)
			break;

	if (i == MAX_ITERATIONS)
		warning("polling of RTC_UIP failed");

	for (i = 0; i < MAX_ITERATIONS; i++)
		if (!(_cmos_read(RTC_FREQ_SELECT) & RTC_UIP))
			break;

	if (i == MAX_ITERATIONS)
		warning("polling of !RTC_UIP failed");

	unsigned year, mon, day, hour, min, sec;

	do {
		sec  = _cmos_read(RTC_SECONDS);
		min  = _cmos_read(RTC_MINUTES);
		hour = _cmos_read(RTC_HOURS);
		day  = _cmos_read(RTC_DAY_OF_MONTH);
		mon  = _cmos_read(RTC_MONTH);
		year = _cmos_read(RTC_YEAR);
	} while (sec != _cmos_read(RTC_SECONDS));

	/* convert BCD to binary format if needed */
	if (!(_cmos_read(RTC_CONTROL) & RTC_DM_BINARY) || RTC_ALWAYS_BCD) {
		BCD_TO_BIN(sec);
		BCD_TO_BIN(min);
		BCD_TO_BIN(hour);
		BCD_TO_BIN(day);
		BCD_TO_BIN(mon);
		BCD_TO_BIN(year);
	}

	if ((year += 1900) < 1970) year += 100;

	return Timestamp { 0, sec, min, hour, day, mon, year };
}


void Rtc::Driver::write_timestamp(Timestamp ts)
{
	uint8_t const ctl  = _cmos_read(RTC_CONTROL);
	uint8_t const freq = _cmos_read(RTC_FREQ_SELECT);
	bool    const bcd  = (!(ctl & RTC_DM_BINARY) || RTC_ALWAYS_BCD);

	/* ignore century and hope for the best */
	ts.year %= 100;

	unsigned const sec  = bcd ? BIN_TO_BCD(ts.second) : ts.second;
	unsigned const min  = bcd ? BIN_TO_BCD(ts.minute) : ts.minute;
	unsigned const hour = bcd ? BIN_TO_BCD(ts.hour)   : ts.hour;
	unsigned const day  = bcd ? BIN_TO_BCD(ts.day)    : ts.day;
	unsigned const mon  = bcd ? BIN_TO_BCD(ts.month)  : ts.month;
	unsigned const year = bcd ? BIN_TO_BCD(ts.year)   : ts.year;

	/* disable updating */
	_cmos_write(RTC_CONTROL, ctl | RTC_SET);
	_cmos_write(RTC_FREQ_SELECT, freq | RTC_DIV_RESET2);

	_cmos_write(RTC_SECONDS,      (uint8_t)sec);
	_cmos_write(RTC_MINUTES,      (uint8_t)min);
	_cmos_write(RTC_HOURS,        (uint8_t)hour);
	_cmos_write(RTC_DAY_OF_MONTH, (uint8_t)day);
	_cmos_write(RTC_MONTH,        (uint8_t)mon);
	_cmos_write(RTC_YEAR,         (uint8_t)year);

	/* enable updating */
	_cmos_write(RTC_CONTROL, ctl);
	_cmos_write(RTC_FREQ_SELECT, freq);
}


static Rtc::Driver *_rtc_driver_ptr;


void Rtc::Driver::init_singleton(Env &env)
{
	static Rtc::Driver inst(env);

	_rtc_driver_ptr = &inst;
}


template <typename FN>
static void with_driver(FN const &fn)
{
	if (_rtc_driver_ptr)
		fn(*_rtc_driver_ptr);
	else
		error("missing call of 'Rtc::Driver::init_singleton");
}


Rtc::Timestamp Rtc::get_time(Env &env)
{
	Driver::init_singleton(env);

	Timestamp result { };
	with_driver([&] (Driver &driver) {
		result = driver.read_timestamp(); });

	return result;
}


void Rtc::set_time(Env &env, Timestamp ts)
{
	Driver::init_singleton(env);

	with_driver([&] (Driver &driver) {
		driver.write_timestamp(ts); });
}
