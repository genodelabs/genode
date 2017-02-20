/*
 * \brief  RTC/CMOS clock driver
 * \author Christian Helmuth
 * \author Markus Partheymueller
 * \date   2007-04-18
 */

/*
 * Copyright (C) 2007-2017 Genode Labs GmbH
 * Copyright (C) 2012 Intel Corporation
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Genode */
#include <io_port_session/connection.h>

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


static inline unsigned cmos_read(Io_port_connection &rtc_ports, unsigned char addr)
{
	unsigned char val;
	rtc_ports.outb(RTC_PORT_ADDR, addr);
//	iodelay();
	val = rtc_ports.inb(RTC_PORT_DATA);
//	iodelay();
	return val;
}


#define RTC_ALWAYS_BCD   1 /* RTC operates in binary mode */
#define BCD_TO_BIN(val)  ((val) = ((val) & 15) + ((val) >> 4) * 10)
#define BIN_TO_BCD(val)  ((val) = (((val)/10) << 4) + (val) % 10)


Rtc::Timestamp Rtc::get_time(Env &env)
{
	/*
	 * Our RTC port session
	 */
	static Io_port_connection rtc_ports(env, RTC_PORT_BASE, RTC_PORT_SIZE);

	unsigned year, mon, day, hour, min, sec;
	int i;

	/* The Linux interpretation of the CMOS clock register contents:
	 * When the Update-In-Progress (UIP) flag goes from 1 to 0, the
	 * RTC registers show the second which has precisely just started.
	 * Let's hope other operating systems interpret the RTC the same way. */

	/* read RTC exactly on falling edge of update flag */
	for (i = 0 ; i < 1000000 ; i++)
		if (cmos_read(rtc_ports, RTC_FREQ_SELECT) & RTC_UIP) break;

	for (i = 0 ; i < 1000000 ; i++)
		if (!(cmos_read(rtc_ports, RTC_FREQ_SELECT) & RTC_UIP)) break;

	do {
		sec  = cmos_read(rtc_ports, RTC_SECONDS);
		min  = cmos_read(rtc_ports, RTC_MINUTES);
		hour = cmos_read(rtc_ports, RTC_HOURS);
		day  = cmos_read(rtc_ports, RTC_DAY_OF_MONTH);
		mon  = cmos_read(rtc_ports, RTC_MONTH);
		year = cmos_read(rtc_ports, RTC_YEAR);
	} while (sec != cmos_read(rtc_ports, RTC_SECONDS));

	/* convert BCD to binary format if needed */
	if (!(cmos_read(rtc_ports, RTC_CONTROL) & RTC_DM_BINARY) || RTC_ALWAYS_BCD) {
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
