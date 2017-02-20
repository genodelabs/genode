/*
 * \brief  ldso test program
 * \author Sebastian Sumpf
 * \author Martin Stein
 * \date   2009-11-05
 */

/*
 * Copyright (C) 2009-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _TEST_LDSO_H_
#define _TEST_LDSO_H_

void lib_1_test();
void lib_1_good();
void lib_1_exception();
void lib_2_exception();

struct Lib_1_local_3
{
	int x { 0x12345678 };
	Lib_1_local_3();
	void lib_1_local_3();
	~Lib_1_local_3();
};

Lib_1_local_3 * lib_1_local_3();

struct Lib_2_global
{
	int x { 0x11223344 };
	Lib_2_global();
	void lib_2_global();
	~Lib_2_global();
}

extern lib_2_global;

struct Lib_2_local
{
	int x { 0x55667788 };
	Lib_2_local();
	void lib_2_local();
	~Lib_2_local();
};

Lib_2_local * lib_2_local();

extern unsigned lib_1_pod_1;
extern unsigned lib_2_pod_1;

#endif /* _TEST_LDSO_H_ */
