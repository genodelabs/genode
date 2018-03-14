/*
 * \brief  Simple stdcxx regression tests
 * \author Christian Helmuth
 * \date   2015-05-28
 *
 */

/*
 * Copyright (C) 2015-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
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
	std::cout << std::stoi("456") << std::endl;
	std::cout << std::stod("7.8") << std::endl;
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


#include <iostream>
#include <sstream>
#include <limits>

static void test_ignore()
{
	std::istringstream input("1\n"
	                         "some non-numeric input\n"
	                         "2\n");
	for (;;) {
		int n;
		input >> n;

		if (input.eof() || input.bad()) {
			break;
		} else if (input.fail()) {
			input.clear(); /* unset failbit */
			input.ignore(std::numeric_limits<std::streamsize>::max(), '\n'); /* skip bad input */
		} else {
			std::cout << n << '\n';
		}
	}
}


int main(int argc, char **argv)
{
	std::cout << "° °° °°° test-stdcxx started °°° °° °"  << std::endl;

	test_string(2015, 5, 4);
	test_cstdlib();
	test_stdexcept();
	test_lock_guard();
	test_ignore();

	std::cout << "° °° °°° test-stdcxx finished °°° °° °" << std::endl;
}
