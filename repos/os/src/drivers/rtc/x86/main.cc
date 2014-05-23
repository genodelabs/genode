/*
 * \brief  Simple real-time-clock driver
 * \author Christian Helmuth
 * \author Markus Partheymueller
 * \date   2007-04-18
 */

/*
 * Copyright (C) 2007-2013 Genode Labs GmbH
 * Copyright (C) 2012 Intel Corporation
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* Genode */
#include <base/env.h>
#include <base/sleep.h>
#include <base/rpc_server.h>
#include <root/component.h>
#include <cap_session/connection.h>
#include <rtc_session/rtc_session.h>
#include <base/printf.h>
#include <io_port_session/connection.h>

using namespace Genode;

/**
 * Time helper
 */
bool is_leap_year(int year)
{
	if (((year & 3) || !((year % 100) != 0)) && (year % 400 != 0)) return false;
	return true;
}

/**
 * Return UNIX time from given date and time.
 */
uint64_t mktime(int day, int mon, int year, int hour, int minutes, int seconds)
{
	bool jan_mar = mon < 3;
	uint64_t ret = 0;
	ret += (367*(10+mon))/12;
	ret += jan_mar*2;
	ret -= 719866;
	ret += day;
	ret += jan_mar * is_leap_year(year);
	ret += 365*year;
	ret += year/4;
	ret -= year/100;
	ret += year/400;
	ret *= 24;
	ret += hour;
	ret *= 60;
	ret += minutes;
	ret *= 60;
	ret += seconds;

	return ret;
}

static uint64_t get_rtc_time();

namespace Rtc {

	class Session_component : public Genode::Rpc_object<Session>
	{
		public:
			uint64_t get_current_time()
			{
				uint64_t ret = get_rtc_time();
				PINF("Time is: %llx\n", ret);
				return ret;
			}

	};

	class Root_component : public Genode::Root_component<Session_component>
	{
		protected:

			Session_component *_create_session(const char *args)
			{
				PDBG("RTC: creating session\n");
				return new (md_alloc()) Session_component();
			}
		public:
			Root_component(Genode::Rpc_entrypoint *ep,
			               Genode::Allocator *allocator)
			: Genode::Root_component<Session_component>(ep, allocator)
			{
				PDBG("RTC: creating root component\n");
			}
        };
}

/*
 * Our RTC port session
 */
Io_port_connection *rtc_ports;


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


static inline unsigned cmos_read(unsigned char addr)
{
	unsigned char val;
	rtc_ports->outb(RTC_PORT_ADDR, addr);
//	iodelay();
	val = rtc_ports->inb(RTC_PORT_DATA);
//	iodelay();
	return val;
}


static inline void cmos_write(unsigned char val, unsigned char addr)
{
	rtc_ports->outb(RTC_PORT_ADDR, addr);
//	iodelay();
	rtc_ports->outb(RTC_PORT_DATA, val);
//	iodelay();
}


#define RTC_ALWAYS_BCD   1 /* RTC operates in binary mode */
#define BCD_TO_BIN(val)  ((val) = ((val) & 15) + ((val) >> 4) * 10)
#define BIN_TO_BCD(val)  ((val) = (((val)/10) << 4) + (val) % 10)


/**
 * Get current time from CMOS and initialize values.
 */
static uint64_t get_rtc_time(void)
{
	unsigned year, mon, day, hour, min, sec;
	int i;

	/* The Linux interpretation of the CMOS clock register contents:
	 * When the Update-In-Progress (UIP) flag goes from 1 to 0, the
	 * RTC registers show the second which has precisely just started.
	 * Let's hope other operating systems interpret the RTC the same way. */

	/* read RTC exactly on falling edge of update flag */
	for (i = 0 ; i < 1000000 ; i++)
		if (cmos_read(RTC_FREQ_SELECT) & RTC_UIP) break;

	for (i = 0 ; i < 1000000 ; i++)
		if (!(cmos_read(RTC_FREQ_SELECT) & RTC_UIP)) break;

	do {
		sec  = cmos_read(RTC_SECONDS);
		min  = cmos_read(RTC_MINUTES);
		hour = cmos_read(RTC_HOURS);
		day  = cmos_read(RTC_DAY_OF_MONTH);
		mon  = cmos_read(RTC_MONTH);
		year = cmos_read(RTC_YEAR);
	} while (sec != cmos_read(RTC_SECONDS));

	/* convert BCD to binary format if needed */
	if (!(cmos_read(RTC_CONTROL) & RTC_DM_BINARY) || RTC_ALWAYS_BCD) {
		BCD_TO_BIN(sec);
		BCD_TO_BIN(min);
		BCD_TO_BIN(hour);
		BCD_TO_BIN(day);
		BCD_TO_BIN(mon);
		BCD_TO_BIN(year);
	}

	if ((year += 1900) < 1970) year += 100;

	PDBG("Date:%02d.%02d.%04d Time:%02d:%02d:%02d\n", day, mon, year, hour, min, sec);

	/* return microseconds */
	return mktime(day, mon, year, hour, min, sec) * 1000000ULL; 
}


int main(int argc, char **argv)
{
	static Io_port_connection ports(RTC_PORT_BASE, RTC_PORT_SIZE);
	rtc_ports = &ports;

	Cap_connection cap;
	static Sliced_heap sliced_heap(env()->ram_session(), env()->rm_session());

	enum { STACK_SIZE = 1024*sizeof(size_t) };
	static Rpc_entrypoint ep(&cap, STACK_SIZE, "rtc_ep");
	static Rtc::Root_component rtc_root(&ep, &sliced_heap);
	env()->parent()->announce(ep.manage(&rtc_root));

	sleep_forever();

	return 0;
}
