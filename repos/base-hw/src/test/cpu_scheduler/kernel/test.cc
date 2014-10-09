/*
 * \brief  Test CPU-scheduler implementation of the kernel
 * \author Martin Stein
 * \date   2014-09-30
 */

/*
 * Copyright (C) 2014 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* core includes */
#include <kernel/cpu_scheduler.h>


/*
 * Utilities
 */

using Genode::size_t;
using Genode::addr_t;
using Kernel::Cpu_scheduler;
using Kernel::Cpu_share;

namespace Kernel { void test(); }

void * operator new(size_t s, void * p) { return p; }

struct Data
{
	Cpu_share idle;
	Cpu_scheduler scheduler;
	char shares[9][sizeof(Cpu_share)];

	Data() : idle(0, 0), scheduler(&idle, 1000, 100) { }
};

Data * data()
{
	static Data d;
	return &d;
}

void done()
{
	Genode::printf("[test] done\n");
	while (1) ;
}

unsigned share_id(void * const pointer)
{
	addr_t const address = (addr_t)pointer;
	addr_t const base = (addr_t)data()->shares;
	if (address < base || address >= base + sizeof(data()->shares)) {
		return 0; }
	return (address - base) / sizeof(Cpu_share) + 1;
}

Cpu_share * share(unsigned const id)
{
	if (!id) { return &data()->idle; }
	return reinterpret_cast<Cpu_share *>(&data()->shares[id - 1]);
}

void create(unsigned const id)
{
	Cpu_share * const s = share(id);
	void * const p = (void *)s;
	switch (id) {
	case 1: new (p) Cpu_share(2, 230); break;
	case 2: new (p) Cpu_share(0, 170); break;
	case 3: new (p) Cpu_share(3, 110); break;
	case 4: new (p) Cpu_share(1,  90); break;
	case 5: new (p) Cpu_share(3, 120); break;
	case 6: new (p) Cpu_share(0,   0); break;
	case 7: new (p) Cpu_share(2, 180); break;
	case 8: new (p) Cpu_share(2, 100); break;
	case 9: new (p) Cpu_share(0,   0); break;
	default: return;
	}
	data()->scheduler.insert(s);
}

void destroy(unsigned const id)
{
	Cpu_share * const s = share(id);
	data()->scheduler.remove(s);
	s->~Cpu_share();
}

void update_check(unsigned const l, unsigned const c,
                  unsigned const s, unsigned const q)
{
	data()->scheduler.update(c);
	Cpu_share * const hs = data()->scheduler.head();
	unsigned const hq = data()->scheduler.head_quota();
	if (hs != share(s)) {
		unsigned const hi = share_id(hs);
		Genode::printf("[test] wrong share %u in line %u\n", hi, l);
		done();
	}
	if (hq != q) {
		Genode::printf("[test] wrong quota %u in line %u\n", hq, l);
		done();
	}
}

void ready_check(unsigned const l, unsigned const s, bool const x)
{
	bool const y = data()->scheduler.ready_check(share(s));
	if (y != x) {
		Genode::printf("[test] wrong check result %u in line %u\n", y, l);
		done();
	}
}


/*
 * Shortcuts for all basic operations that the test consists of
 */

#define C(s)       create(s);
#define D(s)       destroy(s);
#define A(s)       data()->scheduler.ready(share(s));
#define I(s)       data()->scheduler.unready(share(s));
#define Y          data()->scheduler.yield();
#define U(c, s, q) update_check(__LINE__, c, s, q);
#define O(s)       ready_check(__LINE__, s, true);
#define N(s)       ready_check(__LINE__, s, false);


/**
 * Main routine
 */
void Kernel::test()
{
	/*
	 * Step-by-step testing
	 *
	 * Every line in this test is structured according to the scheme
	 * '<ops> U(t,c,s,q) <doc>' where the symbols are defined as follows:
	 *
	 * ops  Operations that affect the schedule but not the head of the
	 *      scheduler (which is a buffer to remember the last scheduling
	 *      choice). These operations are:
	 *
	 *      C(s)  construct the context with ID 's' and insert it
	 *      D(s)  remove the context with ID 's' and destruct it
	 *      A(s)  set the context with ID 's' active
	 *      I(s)  set the context with ID 's' inactive
	 *      O(s)  do 'A(s)' and check that this will outdate the head
	 *      N(s)  do 'A(s)' and check that this won't outdate the head
	 *      Y     annotate that the current head wants to yield
	 *
	 * U(c,s,q)  First update the head and time of the scheduler according to
	 *           the new schedule and the fact that the head consumed a
	 *           quantum of 'c'. Then check if the new head is the context
	 *           with ID 's' that has a quota of 'q'.
	 *
	 * doc  Documents the expected schedule for the point after the head
	 *      update in the corresponding line. First it shows the time left for
	 *      the actual round followd by a comma. Then it lists all claims
	 *      via their ID followed by a ' (active) or a ° (inactive) and the
	 *      corresponding quota. So 5°120 is the inactive context 5 that
	 *      has a quota of 120 left for the current round. They are sorted
	 *      decurrent by their priorities and the priority bands are
	 *      separated by dashes. After the lowest priority band there is
	 *      an additional dash followed by the current state of the round-
	 *      robin scheduling that has no quota or priority. Only active
	 *      contexts are listed here and only the head is listed together
	 *      with its remaining quota. So 4'50 1 7 means that the active
	 *      context 4 is the round-robin head with quota 50 remaining and
	 *      that he is followed by the active contexts 1 and 7 both with a
	 *      fresh time-slice.
	 *
	 * The order of operations is the same as in the operative kernel so each
	 * line can be seen as one "kernel pass". If any check in a line fails,
	 * the test prematurely stops and prints out where and why it has stopped.
	 */

	/* first round - idle */
	U( 10, 0, 100) /*   0, - */
	U( 90, 0, 100) /*  10, - */
	U(120, 0, 100) /* 100, - */
	U(130, 0, 100) /* 200, - */
	U(140, 0, 100) /* 300, - */
	U(150, 0, 100) /* 400, - */
	U(160, 0, 100) /* 500, - */
	U(170, 0, 100) /* 600, - */
	U(180, 0, 100) /* 700, - */
	U(190, 0, 100) /* 800, - */
	U(200, 0, 100) /* 900, - */

	/* second round - one claim, one filler */
	C(1) U(111, 0, 100) /*   0, 1°230 - */
	A(1) U(123, 1, 230) /* 100, 1'230 - 1'100 */
	I(1) U(200, 0, 100) /* 200, 1°30 - */
	A(1) U( 10, 1,  30) /* 400, 1'30 - 1'100 */
	     U(100, 1, 100) /* 410, 1'0 - 1'100 */
	     U(200, 1, 100) /* 440, 1'0 - 1'100 */
	I(1) U(200, 0, 100) /* 540, 1°0 - */
	     U(200, 0, 100) /* 640, 1°0 - */
	A(1) U( 10, 1, 100) /* 740, 1'0 - 1'100 */
	     U( 50, 1,  50) /* 750, 1'0 - 1'50 */
	     U( 20, 1,  30) /* 800, 1'0 - 1'30 */
	     U(100, 1, 100) /* 820, 1'0 - 1'100 */
	     U(200, 1,  50) /* 850, 1'0 - 1'100 */
	     U(200, 1, 230) /* 950, 1'230 - 1'100 */

	/* third round - one claim per priority */
	C(2) A(2) U( 50, 1, 180) /*   0, 1'180 - 2'170 - 1'100 2 */
	     I(1) U( 70, 2, 170) /*  50, 1°110 - 2'170 - 2'100 */
	A(1) I(2) U(110, 1, 110) /* 120, 1'110 - 2°60 - 1'100 */
	          U( 90, 1,  20) /* 230, 1'20 - 2°60 - 1'100 */
	A(2) I(1) U( 10, 2,  60) /* 320, 1°10 - 2'60 - 2'100 */
	     C(3) U( 40, 2,  20) /* 330, 3°110 - 1°10 - 2'10 - 2'100 */
	     A(3) U( 10, 3, 110) /* 370, 3'110 - 1°10 - 2'10 - 2'100 3 */
	          U(150, 2,  10) /* 380, 3'0 - 1°10 - 2'10 - 2'100 3 */
	          U( 10, 2, 100) /* 490, 3'0 - 1°10 - 2'0 - 2'100 3 */
	          U( 60, 2,  40) /* 500, 3'0 - 1°10 - 2'0 - 2'40 3 */
	     C(4) U( 60, 3, 100) /* 560, 3'0 - 1°10 - 4°90 - 2'0 - 3'100 2 */
	C(6) A(6) U(120, 2, 100) /* 600, 3'0 - 1°10 - 4°90 - 2'0 - 2'100 6 3*/
	     A(4) U( 80, 4,  90) /* 700, 3'0 - 1°10 - 4'90 - 2'0 - 2'20 6 3 4 */
	I(4) A(1) U( 50, 1,  10) /* 780, 3'0 - 1'10 - 4°40 - 2'0 - 2'20 6 3 1 */
	          U( 50, 2,  20) /* 830, 3'0 - 1'0 - 4°40 - 2'0 - 2'20 6 3 1 */
	          U( 50, 6, 100) /* 840, 3'0 - 1'0 - 4°40 - 2'0 - 6'100 3 1 2 */
	          U(100, 3,  40) /* 860, 3'0 - 1'0 - 4°40 - 2'0 - 3'100 1 2 6 */
	          U( 60, 3, 110) /* 960, 3'110 - 1'230 - 4°40 - 2'170 - 3'60 1 2 6 */

	/* fourth round - multiple claims per priority */
	     C(5) U( 60, 3,  50) /*   0, 3'50 5°120 - 1'230 - 4°90 - 2'170 - 3'60 1 2 6 */
	A(4) I(3) U( 40, 1, 230) /*  60, 3°10 5°120 - 1'230 - 4'90 - 2'170 - 1'100 2 6 4 */
	C(7) A(7) U(200, 7, 180) /* 100, 3°10 5°120 - 7'180 1'30 - 4'90 - 2'170 - 1'100 2 6 4 7 */
	C(8) A(5) U(100, 5, 120) /* 300, 5'120 3°10 - 7'80 1'30 8°100 - 4'90 - 2'170 - 1'100 2 6 4 7 5 */
	     A(3) U(100, 3,  10) /* 400, 3'10 5'20 - 7'80 1'30 8°100 - 4'90 - 2'170 - 1'100 2 6 4 7 5 3 */
	          U( 30, 5,  20) /* 500, 5'20 3'0 - 7'80 1'30 8°100 - 4'90 - 2'170 - 1'100 2 6 4 7 5 3 */
	C(9) A(9) U( 10, 5,  10) /* 510, 5'10 3'0 - 7'80 1'30 8°100 - 4'90 - 2'170 - 1'100 2 6 4 7 5 3 9 */
	          U( 50, 7,  80) /* 520, 5'0 3'0 - 7'80 1'30 8°100 - 4'90 - 2'170 - 1'100 2 6 4 7 5 3 9 */
	A(8) I(7) U( 10, 8, 100) /* 530, 5'0 3'0 - 8'100 1'30 7°70 - 4'90 - 2'170 - 1'100 2 6 4 5 3 9 8 */
	     I(8) U( 80, 1,  30) /* 540, 5'0 3'0 - 1'30 7°70 8°20 - 4'90 - 2'170 - 1'100 2 6 4 5 3 9 */
	          U(200, 4,  90) /* 620, 5'0 3'0 - 1'0 7°70 8°20 - 4'90 - 2'170 - 1'100 2 6 4 5 3 9 */
	          U(100, 2, 170) /* 650, 5'0 3'0 - 1'0 7°70 8°20 - 4'0 - 2'170 - 1'100 2 6 4 5 3 9 */
	A(8) A(7) U( 10, 7,  70) /* 740, 5'0 3'0 - 7'70 8'20 1'0 - 4'0 - 2'160 - 1'100 2 6 4 5 3 9 8 7 */
	I(7) I(3) U( 10, 8,  20) /* 750, 5'0 3°0 - 8'20 1'0 7°60 - 4'0 - 2'160 - 1'100 2 6 4 5 9 8 */
	     I(8) U( 10, 2, 160) /* 760, 5'0 3°0 - 1'0 7°60 8°10 - 4'0 - 2'160 - 1'100 2 6 4 5 9 */
	     I(2) U( 40, 1, 100) /* 770, 5'0 3°0 - 1'0 7°60 8°10 - 4'0 - 2°120 - 1'100 6 4 5 9 */
	     A(3) U( 30, 1,  70) /* 810, 5'0 3'0 - 1'0 7°60 8°10 - 4'0 - 2°120 - 1'100 6 4 5 9 3 */
	          U( 80, 6,  90) /* 840, 5'0 3'0 - 1'0 7°60 8°10 - 4'0 - 2°120 - 6'100 4 5 9 3 1 */
	A(7) A(8) U( 10, 8,  10) /* 910, 5'0 3'0 - 8'10 7'60 1'0 - 4'0 - 2°120 - 6'90 4 5 9 3 1 7 8 */
	          U( 30, 7,  60) /* 920, 5'0 3'0 - 7'60 1'0 8'0 - 4°0 - 2°120 - 6'90 4 5 9 3 1 7 8 */
	A(2) I(7) U( 10, 2,  60) /* 930, 5'0 3'0 - 1'0 8'0 7°50 - 4'0 - 2'120 - 6'90 4 5 9 3 1 8 2 */
	I(3) I(5) U( 40, 2,  20) /* 940, 5°0 3°0 - 1'0 8'0 7°50 - 4'0 - 2'80 - 6'90 4 9 1 8 2 */
	I(9) I(4) U( 10, 2,  10) /* 980, 5°0 3°0 - 1'0 8'0 7°50 - 4°0 - 2'70 - 6'90 1 8 2 */
	          U( 40, 1, 230) /* 990, 5°120 3°110 - 1'230 8'100 7°180 - 4°90 - 2'170 - 6'90 1 8 2 */

	/* fifth round - yield, ready & check*/
	     I(6) U( 30, 1, 200) /*   0, 5°120 3°110 - 1'200 8'100 7°180 - 4°90 - 2'170 - 1'100 8 2 */
	        Y U( 20, 8, 100) /*  30, 5°120 3°110 - 8'100 1'0 7°180 - 4°90 - 2'170 - 8'100 2 1 */
	          U(200, 2, 170) /*  50, 5°120 3°110 - 1'0 8'0 7°180 - 4°90 - 2'170 - 8'100 2 1 */
	        Y U( 70, 8, 100) /* 150, 5°120 3°110 - 1'0 8'0 7°180 - 4°90 - 2'0 - 8'100 2 1 */
	     I(8) U( 40, 2, 100) /* 220, 5°120 3°110 - 1'0 7°180 8°0 - 4°90 - 2'0 - 2'100 1 */
	     I(1) U( 50, 2,  50) /* 260, 5°120 3°110 - 1°0 7°180 8°0 - 4°90 - 2'0 - 2'50 */
	          U( 10, 2,  40) /* 310, 5°120 3°110 - 1°0 7°180 8°0 - 4°90 - 2'0 - 2'40 */
	     N(1) U(200, 1, 100) /* 320, 5°120 3°110 - 1°0 7°180 8°0 - 4°90 - 2'0 - 1'100 2 */
	          U( 10, 1,  90) /* 360, 5°120 3°110 - 1'0 7°180 8°0 - 4°90 - 2'0 - 1'90 2 */
	     I(1) U( 10, 2, 100) /* 370, 5°120 3°110 - 1°0 7°180 8°0 - 4°90 - 2'0 - 2'100 */
	     O(5) U( 10, 5, 120) /* 380, 5'120 3°110 - 1°0 7°180 8°0 - 4°90 - 2'0 - 2'90 5 */
	        Y U( 90, 2,  90) /* 390, 5'0 3°110 - 1°0 7°180 8°0 - 4°90 - 2'0 - 2'90 5 */
	        Y U( 10, 5, 100) /* 480, 5'0 3°110 - 1°0 7°180 8°0 - 4°90 - 2'0 - 5'100 2 */
	     O(7) U( 10, 7, 180) /* 490, 5'0 3°110 - 7'180 1°0 8°0 - 4°90 - 2'0 - 5'90 2 7 */
	        Y U( 10, 5,  90) /* 500, 5'0 3°110 - 7'0 1°0 8°0 - 4°90 - 2'0 - 5'90 2 7 */
	        Y U( 10, 2, 100) /* 510, 5'0 3°110 - 7'0 1°0 8°0 - 4°90 - 2'0 - 2'100 7 5 */
	        Y U( 10, 7, 100) /* 520, 5'0 3°110 - 7'0 1°0 8°0 - 4°90 - 2'0 - 7'100 5 2 */
	     I(5) U( 10, 7,  90) /* 530, 5°0 3°110 - 7'0 1°0 8°0 - 4°90 - 2'0 - 7'90 2 */
	I(7) N(5) U( 10, 2, 100) /* 540, 5'0 3°110 - 7°0 1°0 8°0 - 4°90 - 2'0 - 2'100 5 */
	     N(7) U(200, 5, 100) /* 550, 5'0 3°110 - 7'0 1°0 8°0 - 4°90 - 2'0 - 5'100 7 2 */
	I(5) I(7) U( 10, 2, 100) /* 650, 5°0 3°110 - 7°0 1°0 8°0 - 4°90 - 2'0 - 2'100 */
	     I(2) U( 10, 0, 100) /* 660, 5°0 3°110 - 7°0 1°0 8°0 - 4°90 - 2°0 - */
	          U( 10, 0, 100) /* 670, 5°0 3°110 - 7°0 1°0 8°0 - 4°90 - 2°0 - */
	          U(100, 0, 100) /* 680, 5°0 3°110 - 7°0 1°0 8°0 - 4°90 - 2°0 - */
	     O(9) U( 10, 9, 100) /* 780, 5°0 3°110 - 7°0 1°0 8°0 - 4°90 - 2°0 - 9'100 */
	     N(6) U( 20, 9,  80) /* 790, 5°0 3°110 - 7°0 1°0 8°0 - 4°90 - 2°0 - 9'80 6 */
	     N(8) U( 10, 9,  70) /* 810, 5°0 3°110 - 8'0 7°0 1°0 - 4°90 - 2°0 - 9'70 6 8 */
	        Y U( 10, 6, 100) /* 820, 5°0 3°110 - 8'0 7°0 1°0 - 4°90 - 2°0 - 6'100 8 9 */
	        Y U( 10, 8, 100) /* 830, 5°0 3°110 - 8'0 7°0 1°0 - 4°90 - 2°0 - 8'100 9 6 */
	   N(7) Y U( 20, 9, 100) /* 840, 5°0 3°110 - 8'0 7'0 1°0 - 4°90 - 2°0 - 9'100 6 7 8 */
	I(8) I(9) U( 10, 6, 100) /* 860, 5°0 3°110 - 7'0 8°0 1°0 - 4°90 - 2°0 - 6'100 7 */
	I(6) I(7) U( 10, 0, 100) /* 870, 5°0 3°110 - 7°0 8°0 1°0 - 4°90 - 2°0 - */
	     O(4) U( 20, 4,  90) /* 880, 5°0 3°110 - 7°0 8°0 1°0 - 4'90 - 2°0 - 4'100 */
	O(3) N(1) U( 10, 3,  90) /* 900, 3'110 5°0 - 1'0 7°0 8°0 - 4'80 - 2°0 - 4'100 3 1 */
	N(5) I(4) U( 10, 3,  80) /* 910, 3'100 5'0 - 1'0 7°0 8°0 - 4°80 - 2°0 - 3 1 5 */
	     I(3) U( 10, 1,  70) /* 920, 5'0 3°90 - 1'0 7°0 8°0 - 4°80 - 2°0 - 1'100 5 */
	     O(3) U( 10, 3,  60) /* 930, 3'90 5'0 - 1'0 7°0 8°0 - 4°80 - 2°0 - 1'90 5 3 */
	     N(4) U( 10, 3,  50) /* 940, 3'80 5'0 - 1'0 7°0 8°0 - 4'80 - 2°0 - 1'90 5 3 4 */
	     I(4) U( 10, 3,  40) /* 950, 3'70 5'0 - 1'0 7°0 8°0 - 4°80 - 2°0 - 1'90 5 3 */
	I(3) N(4) U( 10, 4,  30) /* 960, 5'0 3°60 - 1'0 7°0 8°0 - 4'80 - 2°0 - 1'90 5 4 */
	     I(4) U( 10, 1,  20) /* 970, 5'0 3°60 - 1'0 7°0 8°0 - 4°70 - 2°0 - 1'90 5 */
	O(3) O(4) U( 10, 3,  10) /* 980, 3'60 5'0 - 1'0 7°0 8°0 - 4'70 - 2°0 - 1'80 5 3 4 */
	        Y U( 10, 5, 120) /* 990, 5'120 3'110 - 1'230 7°180 8°100 - 4'90 - 2°170 - 1'80 5 3 4 */

	/* sixth round - destroy and re-create */
	     D(3) U( 30, 5,  90) /*   0, 5'90 - 1'230 7°180 8°100 - 4'90 - 2°170 - 1'80 5 4 */
	     I(5) U( 30, 1, 230) /*  30, 5°60 - 1'230 7°180 8°100 - 4'90 - 2°170 - 1'80 4 */
	D(4) D(7) U( 20, 1, 210) /*  60, 5°60 - 1'210 8°100 - 2°170 - 1'80 4 */
	I(1) N(9) U( 40, 9, 100) /*  80, 5°60 - 1°170 8°100 - 2°170 - 9'100 */
	A(5) O(8) U( 70, 5,  60) /* 120, 5'60 - 1°170 8'100 - 2°170 - 9'30 5 8 */
	D(8) I(5) U( 10, 9,  30) /* 190, 5°60 - 1°170 - 2°170 - 9'30 */
	N(6) C(4) U( 10, 9,  20) /* 200, 5°60 - 1°170 - 4°90 - 2°170 - 9'20 6 */
	D(5) O(4) U( 10, 4,  90) /* 210, 1°170 - 4'90 - 2°170 - 9'10 6 4 */
	          U(100, 9,  10) /* 220, 1°170 - 4'0 - 2°170 - 9'10 6 4 */
	          U( 10, 6, 100) /* 310, 1°170 - 4'0 - 2°170 - 6'100 4 9 */
	     D(4) U(200, 9, 100) /* 320, 1°170 - 2°170 - 9'100 6 */
	C(5) A(5) U( 10, 5, 120) /* 420, 5'120 - 1°210 - 2°170 - 9'90 6 5 */
	   C(4) Y U( 10, 9,  90) /* 430, 5'0 - 1°170 - 4°90 - 2°170 - 9'90 6 5 */
	   O(4) Y U( 50, 4,  90) /* 440, 5'0 - 1°170 - 4'90 - 2°170 - 6'100 5 4 9 */
	   D(6) Y U( 10, 5, 100) /* 490, 5'0 - 1°170 - 4'0 - 2°170 - 5'100 4 9 */
	     D(9) U(200, 4, 100) /* 500, 5'0 - 1°170 - 4'0 - 2°170 - 4'100 5 */
	C(7) C(8) U(200, 5, 100) /* 600, 5'0 - 1°170 7°180 8°100 - 4'0 - 2°170 - 5'100 4 */
	O(1) O(7) U( 10, 7, 180) /* 700, 5'0 - 7'180 1'170 8°100 - 4'0 - 2°170 - 5'90 4 1 7 */
	     O(8) U( 40, 8, 100) /* 710, 5'0 - 8'100 7'140 1'170 - 4'0 - 2°170 - 5'90 4 1 7 8 */
	     D(7) U(200, 1, 150) /* 750, 5'0 - 1'170 8'0 - 4'0 - 2°170 - 5'90 4 1 8 */
	        Y U( 60, 5,  90) /* 850, 5'0 - 8'0 1'0 - 4'0 - 2°170 - 5'90 4 1 8 */
	          U(100, 5, 120) /* 910, 5'120 - 8'100 1'230 - 4'90 - 2°170 - 5'90 4 1 8 */

	done();
}
