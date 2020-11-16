/*
 * \brief  Wrapper for stdcxx output handling.
 * \author Pirmin Duss
 * \date   2020-11-11
 */

/*
 * Copyright (C) 2020 Genode Labs GmbH
 * Copyright (C) 2020 gapfruit AG
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef INTEGRATION_TEST_STDCXX_LOG_H
#define INTEGRATION_TEST_STDCXX_LOG_H


/* stdcxx includes */
#include <iostream>
#include <mutex>


namespace Integration_test
{
	using namespace std;

	extern mutex lock;

	template <typename... ARGS>
	void log(ARGS &&... args)
	{
		lock_guard<mutex> guard { lock };

		((cout << args), ...);
		cout << endl;
	}

	template <typename... ARGS>
	void error(ARGS &&... args)
	{
		lock_guard<mutex> guard { lock };

		cerr << "\033[31m";
		((cerr << args), ...);
		cerr << "\033[0m";
		cerr << endl;
	}
}

#endif /* INTEGRATION_TEST_STDCXX_LOG_H */
