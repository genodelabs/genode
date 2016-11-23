/*
 * \brief  Simple stdcxx regression tests
 * \author Christian Helmuth
 * \date   2015-05-28
 *
 */

/*
 * Copyright (C) 2015 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#include <base/printf.h>

#include <iostream>
#include <iomanip>
#include <sstream>
#include <string>

static void test_string(double year, float month, unsigned long day)
{
	std::cout << std::to_string(year);
	std::cout << " - ";
	std::cout << std::to_string(month);
	std::cout << " - ";
	std::cout << std::to_string(day);
	std::cout << std::endl;

	std::ostringstream buffer;

	buffer << std::fixed << std::setprecision(0);
	buffer << year;
	buffer << " - ";
	buffer << month;
	buffer << " - ";
	buffer << day;
	buffer << std::endl;

	std::cout << buffer.str();
}


#include <cstdlib>

static void test_cstdlib()
{
	static ::lldiv_t o __attribute__((used));
	std::cout << std::strtoul("123", 0, 10) << std::endl;
}


#include <stdexcept>

static void test_stdexcept()
{
	try {
		throw std::invalid_argument("INVALID");
	} catch (std::invalid_argument) {
		std::cout << "caught std::invalid_argument"<< std::endl;
	}
}


#include <mutex>

static void test_lock_guard()
{
#if 0
	typedef std::mutex Mutex;
#else
	struct Mutex
	{
		void lock() { }
		void unlock() { }
	};
#endif

	Mutex                  lock;
	std::lock_guard<Mutex> lock_guard(lock);
}


int main(int argc, char **argv)
{
	std::cout << "° °° °°° test-stdcxx started °°° °° °"  << std::endl;

	test_string(2015, 5, 4);
	test_cstdlib();
	test_stdexcept();
	test_lock_guard();

	std::cout << "° °° °°° test-stdcxx finished °°° °° °" << std::endl;
}
