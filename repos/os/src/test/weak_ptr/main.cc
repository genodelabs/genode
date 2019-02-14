/*
 * \brief  Test for weak-pointer utilities
 * \author Norman Feske
 * \date   2013-03-12
 */

/*
 * Copyright (C) 2013-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Genode includes */
#include <base/component.h>
#include <base/log.h>
#include <base/heap.h>
#include <base/thread.h>
#include <base/weak_ptr.h>
#include <timer_session/connection.h>


/********************************************************************
 ** Hooks for obtaining internal information of the tested classes **
 ********************************************************************/

static int weak_ptr_cnt;


void Genode::Weak_object_base::debug_info() const
{
	/* count number of weak pointers pointing to the object */
	weak_ptr_cnt = 0;
	for (Weak_ptr_base const *curr = _list.first(); curr; curr = curr->next())
		weak_ptr_cnt++;
}


static bool weak_ptr_valid;


void Genode::Weak_ptr_base::debug_info() const
{
	weak_ptr_valid = (_obj != nullptr);
}


struct Fatal_error { };


static void assert_weak_ptr_cnt(Genode::Weak_object_base const *obj,
                                int expected_cnt)
{
	obj->debug_info();

	if (expected_cnt != weak_ptr_cnt) {
		Genode::error("unexpected count, expected ", expected_cnt, ", "
		              "got ", weak_ptr_cnt);
		throw Fatal_error();
	}
}


static void assert_weak_ptr_valid(Genode::Weak_ptr_base const &ptr, bool valid)
{
	ptr.debug_info();

	if (weak_ptr_valid == valid)
		return;

	Genode::error("weak pointer unexpectedly ", valid ? "valid" : "invalid");
	throw Fatal_error();
}


/********************************************
 ** Test for the tracking of weak pointers **
 ********************************************/

static bool object_constructed;


struct Object : Genode::Weak_object<Object>
{
	Object() { object_constructed = true; }

	~Object()
	{
		Weak_object<Object>::lock_for_destruction();
		object_constructed = false;
	}
};


static void test_weak_pointer_tracking(Genode::Heap & heap)
{
	using namespace Genode;

	log("construct invalid weak pointer");
	{
		Weak_ptr<Object> ptr;
		assert_weak_ptr_valid(ptr, false);
	}

	Object *obj = new (heap) Object;

	Weak_ptr<Object> ptr_1 = obj->weak_ptr();
	assert_weak_ptr_valid(ptr_1, true);

	Weak_ptr<Object> ptr_2 = obj->weak_ptr();
	assert_weak_ptr_valid(ptr_2, true);

	assert_weak_ptr_cnt(obj, 2);

	log("test: assign weak pointer to itself");
	ptr_2 = ptr_2;
	assert_weak_ptr_cnt(obj, 2);
	assert_weak_ptr_valid(ptr_2, true);

	{
		log("test: assign weak pointer to another");
		Weak_ptr<Object> ptr_3 = ptr_2;
		assert_weak_ptr_cnt(obj, 3);
		assert_weak_ptr_valid(ptr_3, true);

		log("test: destruct weak pointer");
		/* 'ptr_3' gets destructed when leaving the scope */
	}
	assert_weak_ptr_cnt(obj, 2);

	{
		log("test: assign invalid weak pointer to another");
		Weak_ptr<Object> ptr_3 = ptr_2;
		assert_weak_ptr_cnt(obj, 3);
		assert_weak_ptr_valid(ptr_3, true);

		ptr_3 = Weak_ptr<Object>();
		assert_weak_ptr_cnt(obj, 2);
		assert_weak_ptr_valid(ptr_3, false);

		log("test: destruct weak pointer");
		/* 'ptr_3' gets destructed when leaving the scope */
	}
	assert_weak_ptr_cnt(obj, 2);

	log("destruct object");
	destroy(heap, obj);

	/*
	 * The destruction of the object should have invalidated all weak pointers
	 * pointing to the object.
	 */
	assert_weak_ptr_valid(ptr_1, false);
	assert_weak_ptr_valid(ptr_2, false);
}


/*******************************************
 ** Test for deferring object destruction **
 *******************************************/

template <typename O>
struct Destruct_thread : Genode::Thread
{
	O &obj;
	Genode::Heap & heap;

	void entry() override
	{
		using namespace Genode;
		log("thread: going to destroy object");
		destroy(heap, &obj);
		log("thread: destruction completed, job done");
	}

	Destruct_thread(O *obj, Genode::Env & env, Genode::Heap & heap)
	: Thread(env, "object_destructor", 16*1024), obj(*obj), heap(heap) { }
};


static void assert_constructed(bool expect_constructed)
{
	if (object_constructed == expect_constructed)
		return;

	Genode::error("object unexpectedly ",
	              !object_constructed ? "not " : "", "constructed");
	throw Fatal_error();
}


static void test_deferred_destruction(Genode::Env & env, Genode::Heap &heap)
{
	using namespace Genode;

	static Timer::Connection timer(env);

	Object *obj = new (&heap) Object;

	Weak_ptr<Object> ptr = obj->weak_ptr();
	assert_weak_ptr_cnt(obj, 1);
	assert_weak_ptr_valid(ptr, true);
	assert_constructed(true);

	/* create thread that will be used to destruct the object */
	Destruct_thread<Object> destruct_thread(obj, env, heap);

	{
		/* acquire possession over the object */
		Locked_ptr<Object> locked_ptr(ptr);

		/* start destruction using dedicated thread */
		destruct_thread.start();

		/* yield some time to the other thread */
		timer.msleep(500);

		/* even after the time period, the object should still be alive */
		assert_constructed(true);

		/* now, we release the locked pointer, the destruction can begin */
	}

	/*
	 * Now that the thread is expected to be unblocked, yield some time
	 * to actually do the destruction.
	 */
	timer.msleep(100);

	assert_constructed(false);

	destruct_thread.join();
}


/*******************************************************
 ** Test the failed aquisition of a destructed object **
 *******************************************************/

static void test_acquisition_failure(Genode::Heap & heap)
{
	using namespace Genode;

	log("create object and weak pointer");
	Object *obj = new (&heap) Object;
	Weak_ptr<Object> ptr = obj->weak_ptr();

	log("try to acquire possession over the object");
	{
		Locked_ptr<Object> locked_ptr(ptr);

		if (!locked_ptr.valid()) {
			error("locked pointer unexpectedly invalid");
			throw Fatal_error();
		}

		/* release lock */
	}

	log("destroy object");
	destroy(&heap, obj);

	log("try again, this time we should get an invalid pointer");
	{
		Locked_ptr<Object> locked_ptr(ptr);

		if (locked_ptr.valid()) {
			error("locked pointer unexpectedly valid");
			throw Fatal_error();
		}
	}
}


/*******************************************************
 ** Test the failed aquisition during the destruction **
 *******************************************************/

struct Object_with_delayed_destruction
: Genode::Weak_object<Object_with_delayed_destruction>
{
	Timer::Connection timer;

	Object_with_delayed_destruction(Genode::Env & env) : timer(env)	{
		object_constructed = true; }

	~Object_with_delayed_destruction()
	{
		Weak_object<Object_with_delayed_destruction>::lock_for_destruction();
		timer.msleep(2000);
		object_constructed = false;
	}
};


static void test_acquisition_during_destruction(Genode::Env & env,
                                                Genode::Heap & heap)
{
	using namespace Genode;
	using Destruct_thread = Destruct_thread<Object_with_delayed_destruction>;

	static Timer::Connection timer(env);

	Object_with_delayed_destruction *obj =
		new (&heap) Object_with_delayed_destruction(env);

	Weak_ptr<Object_with_delayed_destruction> ptr = obj->weak_ptr();
	assert_weak_ptr_cnt(obj, 1);
	assert_weak_ptr_valid(ptr, true);
	assert_constructed(true);

	/* create and start thread that will be used to destruct the object */
	Destruct_thread destruct_thread(obj, env, heap);
	destruct_thread.start();

	/* wait so that the thread enters the destructor */
	timer.msleep(500);

	{
		/* acquire possession over the object */
		Locked_ptr<Object_with_delayed_destruction> locked_ptr(ptr);

		/* the object should be invalid */
		assert_weak_ptr_valid(ptr, false);
	}

	/* synchronize destruction of thread */
	destruct_thread.join();
}

/******************
 ** Main program **
 ******************/

void Component::construct(Genode::Env & env)
{
	using namespace Genode;

	Heap heap(env.ram(), env.rm());

	log("--- test-weak_ptr started ---");

	log("\n-- test tracking of weak pointers --");
	test_weak_pointer_tracking(heap);

	log("\n-- test deferred destruction --");
	test_deferred_destruction(env, heap);

	log("\n-- test acquisition failure --");
	test_acquisition_failure(heap);

	log("\n-- test acquisition during destruction --");
	test_acquisition_during_destruction(env, heap);

	log("\n--- finished test-weak_ptr ---");
}
