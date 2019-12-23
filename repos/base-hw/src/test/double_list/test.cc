/*
 * \brief  Test double-list implementation of the kernel
 * \author Martin Stein
 * \date   2014-09-30
 */

/*
 * Copyright (C) 2014-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* base includes */
#include <base/component.h>
#include <base/log.h>

/* core includes */
#include <kernel/double_list.h>


/*
 * Utilities
 */

using Genode::size_t;
using Kernel::Double_list;
using Kernel::Double_list_item;

void * operator new(__SIZE_TYPE__, void * p) { return p; }

struct Item_load { char volatile x = 0, y = 0, z = 0; };

struct Item : Item_load, Double_list_item<Item>
{
	unsigned _id;

	Item(unsigned const id) : Double_list_item<Item>(*this), _id(id) { x = 1; y = 2; z = 3; }

	void iteration() { Genode::log(_id); }
};

struct Data
{
	static constexpr unsigned nr_of_items = 9;

	Double_list<Item> list { };
	char items[nr_of_items][sizeof(Item)];

	Data()
	{
		for (unsigned i = 0; i < nr_of_items; i++) {
			new (&items[i]) Item(i + 1); }
	}
};

Data * data()
{
	static Data d;
	return &d;
}

void done()
{
	Genode::log("done");
	while (1) ;
}

void check(unsigned i1, unsigned l)
{
	Double_list_item<Item> * const li2 = data()->list.head();
	if (li2) {
		Item * const i2 = &li2->payload();
		if (i1) {
			if(i1 == i2->_id) { return; }
			Genode::log("head ", i2->_id, " in line ", l);
			done();
		} else {
			Genode::log("non-empty ", i2->_id, " in line ", l);
			done();
		}
	} else if (i1) {
		Genode::log("empty in line ", l);
		done();
	}
}

void print_each()
{
	Genode::log("print each");
	data()->list.for_each([] (Item &i) { i.iteration(); });
}

Item * item(unsigned const i) {
	return reinterpret_cast<Item *>(&data()->items[i - 1]); }


/*
 * Shortcuts for all basic operations that the test consists of
 */

#define C(i) check(i, __LINE__);
#define T(i) data()->list.insert_tail(item(i));
#define H(i) data()->list.insert_head(item(i));
#define R(i) data()->list.remove(item(i));
#define B(i) data()->list.to_tail(item(i));
#define P    print_each();
#define N    data()->list.head_to_tail();


/**
 * Main routine
 */
void Component::construct(Genode::Env &)
{
	/*
	 * Step-by-step testing
	 *
	 * Every line in this test is structured according to the scheme
	 * '<ops> C(i) <doc>' where the symbols are defined as follows:
	 *
	 * ops    Operations that affect the list structure. These are:
	 *
	 *        T(i)  insert the item with ID 'i' as tail
	 *        H(i)  insert the item with ID 'i' as head
	 *        R(i)  remove the item with ID 'i'
	 *        B(i)  move the item with ID 'i' to the tail
	 *        N     move the head to the tail
	 *        P     print IDs of the items in the list in their list order
	 *
	 * C(i)   check if the item with ID 'i' is head
	 *
	 * doc    Documents the expected list content for the point after the
	 *        operations in the corresponding line.
	 *
	 * If any check in a line fails, the test prematurely stops and prints out
	 * where and why it has stopped.
	 */

	          C(0) /* */
	        N C(0) /* */
	        P C(0) /* */
	     T(1) C(1) /* 1 */
	        N C(1) /* 1 */
	      P N C(1) /* 1 */
	     B(1) C(1) /* 1 */
	        N C(1) /* 1 */
	     R(1) C(0) /* */
	        N C(0) /* */
	        N C(0) /* */
	     H(2) C(2) /* 2 */
	        N C(2) /* 2 */
	        N C(2) /* 2 */
	     T(3) C(2) /* 2 3 */
	        N C(3) /* 3 2 */
	     B(2) C(3) /* 3 2 */
	        N C(2) /* 2 3 */
	     H(4) C(4) /* 4 2 3 */
	        N C(2) /* 2 3 4 */
	        N C(3) /* 3 4 2 */
	        N C(4) /* 4 2 3 */
	   R(4) N C(3) /* 3 2 */
	        N C(2) /* 2 3 */
	     T(1) C(2) /* 2 3 1 */
	        N C(3) /* 3 1 2 */
	        N C(1) /* 1 2 3 */
	        N C(2) /* 2 3 1 */
	        N C(3) /* 3 1 2 */
	     R(1) C(3) /* 3 2 */
	        N C(2) /* 2 3 */
	        N C(3) /* 3 2 */
	     B(3) C(2) /* 2 3 */
	T(4) T(1) C(2) /* 2 3 4 1 */
	      N N C(4) /* 4 1 2 3 */
	        N C(1) /* 1 2 3 4 */
	      N N C(3) /* 3 4 1 2 */
	     R(2) C(3) /* 3 4 1 */
	     R(3) C(4) /* 4 1 */
	        N C(1) /* 1 4 */
	      N N C(1) /* 1 4 */
	T(3) T(2) C(1) /* 1 4 3 2 */
	   T(5) N C(4) /* 4 3 2 5 1 */
	T(7) H(6) C(6) /* 6 4 3 2 5 1 7 */
	        N C(4) /* 4 3 2 5 1 7 6 */
	     B(4) C(3) /* 3 2 5 1 7 6 4 */
	 B(4) N N C(5) /* 5 1 7 6 4 3 2 */
	 N B(7) N C(6) /* 6 4 3 2 5 7 1*/
	 N N B(1) C(3) /* 3 2 5 7 6 4 1 */
	        P C(3) /* 3 2 5 7 6 4 1 */
	R(4) H(4) C(4) /* 4 3 2 5 7 6 1 */
	B(7) B(6) C(4) /* 4 3 2 5 1 7 6 */
	    N N N C(5) /* 5 1 7 6 4 3 2 */
	    N N N C(6) /* 6 4 3 2 5 1 7 */
	    N N N C(2) /* 2 5 1 7 6 4 3 */
	 T(9) N N C(1) /* 1 7 6 4 3 9 2 5 */
	  N N N N C(3) /* 3 9 2 5 1 7 6 4 */
	  N N N N C(1) /* 1 7 6 4 3 9 2 5 */
	    N N N C(4) /* 4 3 9 2 5 1 7 6 */
	      N N C(9) /* 9 2 5 1 7 6 4 3 */
	   H(8) P C(8) /* 8 9 2 5 1 7 6 4 3 */
	     R(8) C(9) /* 9 2 5 1 7 6 4 3 */
	     R(9) C(2) /* 2 5 1 7 6 4 3 */
	 R(1) N N C(7) /* 7 6 4 3 2 5 */
	 N R(6) N C(3) /* 3 2 5 7 4 */
	T(8) R(3) C(2) /* 2 5 7 4 8 */
	 N N R(5) C(7) /* 7 4 8 2 */
	R(2) R(4) C(7) /* 7 8 */
	        N C(8) /* 8 7 */
	      N P C(7) /* 7 8 */
	     R(7) C(8) /* 7 8 */
	     R(8) C(0) /* */
	          C(0) /* */

	done();
}
