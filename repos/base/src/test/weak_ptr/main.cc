/*
 * \brief  Test for weak-pointer utilities
 * \author Norman Feske
 * \date   2013-03-12
 */

/*
 * Copyright (C) 2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* Genode includes */
#include <base/printf.h>
#include <base/env.h>
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


static int weak_ptr_valid;


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
		PERR("unexpected count, expected %d, got %d",
		     expected_cnt, weak_ptr_cnt);
		throw Fatal_error();
	}
}


static void assert_weak_ptr_valid(Genode::Weak_ptr_base const &ptr, bool valid)
{
	ptr.debug_info();

	if (weak_ptr_valid == valid)
		return;

	PERR("weak pointer unexpectedly %s", valid ? "valid" : "invalid");
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


static void test_weak_pointer_tracking()
{
	using namespace Genode;

	PLOG("construct invalid weak pointer");
	{
		Weak_ptr<Object> ptr;
		assert_weak_ptr_valid(ptr, false);
	}

	Object *obj = new (env()->heap()) Object;

	Weak_ptr<Object> ptr_1 = obj->weak_ptr();
	assert_weak_ptr_valid(ptr_1, true);

	Weak_ptr<Object> ptr_2 = obj->weak_ptr();
	assert_weak_ptr_valid(ptr_2, true);

	assert_weak_ptr_cnt(obj, 2);

	PLOG("test: assign weak pointer to itself");
	ptr_2 = ptr_2;
	assert_weak_ptr_cnt(obj, 2);
	assert_weak_ptr_valid(ptr_2, true);

	{
		PLOG("test: assign weak pointer to another");
		Weak_ptr<Object> ptr_3 = ptr_2;
		assert_weak_ptr_cnt(obj, 3);

		PLOG("test: destruct weak pointer");
		/* 'ptr_3' gets destructed when leaving the scope */
	}
	assert_weak_ptr_cnt(obj, 2);

	PLOG("destruct object");
	destroy(env()->heap(), obj);

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
struct Destruct_thread : Genode::Thread_deprecated<4096>
{
	O *obj;

	void entry()
	{
		using namespace Genode;
		PLOG("thread: going to destroy object");
		destroy(env()->heap(), obj);
		PLOG("thread: destruction completed, job done");
	}

	Destruct_thread(O *obj) : Thread_deprecated("object_destructor"), obj(obj) { }
};


static void assert_constructed(bool expect_constructed)
{
	if (object_constructed == expect_constructed)
		return;

	PERR("object unexpectedly %sconstructed",
	     !object_constructed ? "not" : "");
	throw Fatal_error();
}


static void test_deferred_destruction()
{
	using namespace Genode;

	static Timer::Connection timer;

	Object *obj = new (env()->heap()) Object;

	Weak_ptr<Object> ptr = obj->weak_ptr();
	assert_weak_ptr_cnt(obj, 1);
	assert_weak_ptr_valid(ptr, true);
	assert_constructed(true);

	/* create thread that will be used to destruct the object */
	Destruct_thread<Object> destruct_thread(obj);

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

static void test_acquisition_failure()
{
	using namespace Genode;

	PLOG("create object and weak pointer");
	Object *obj = new (env()->heap()) Object;
	Weak_ptr<Object> ptr = obj->weak_ptr();

	PLOG("try to acquire possession over the object");
	{
		Locked_ptr<Object> locked_ptr(ptr);

		if (!locked_ptr.valid()) {
			PERR("locked pointer unexpectedly invalid");
			throw Fatal_error();
		}

		/* release lock */
	}

	PLOG("destroy object");
	destroy(env()->heap(), obj);

	PLOG("try again, this time we should get an invalid pointer");
	{
		Locked_ptr<Object> locked_ptr(ptr);

		if (locked_ptr.valid()) {
			PERR("locked pointer unexpectedly valid");
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

	Object_with_delayed_destruction() { object_constructed = true; }

	~Object_with_delayed_destruction()
	{
		Weak_object<Object_with_delayed_destruction>::lock_for_destruction();
		timer.msleep(2000);
		object_constructed = false;
	}
};


static void test_acquisition_during_destruction()
{
	using namespace Genode;

	static Timer::Connection timer;

	Object_with_delayed_destruction *obj =
		new (env()->heap()) Object_with_delayed_destruction();

	Weak_ptr<Object_with_delayed_destruction> ptr = obj->weak_ptr();
	assert_weak_ptr_cnt(obj, 1);
	assert_weak_ptr_valid(ptr, true);
	assert_constructed(true);

	/* create and start thread that will be used to destruct the object */
	Destruct_thread<Object_with_delayed_destruction> destruct_thread(obj);
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

int main(int argc, char **argv)
{
	using namespace Genode;

	printf("--- test-weak_ptr started ---\n");

	printf("\n-- test tracking of weak pointers --\n");
	test_weak_pointer_tracking();

	printf("\n-- test deferred destruction --\n");
	test_deferred_destruction();

	printf("\n-- test acquisition failure --\n");
	test_acquisition_failure();

	printf("\n-- test acquisition during destruction --\n");
	test_acquisition_during_destruction();

	printf("\n--- finished test-weak_ptr ---\n");
	return 0;
}
