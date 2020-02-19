/*
 * \brief  TLS support ('emutls')
 * \author Christian Prochaska
 * \date   2019-06-13
 */

/*
 * Copyright (C) 2019 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Genode includes */
#include <base/heap.h>
#include <base/log.h>
#include <base/thread.h>

/* base-internal includes */
#include <base/internal/globals.h>

using namespace Genode;

/* implemented in 'malloc_free.cc' */
extern Heap &cxx_heap();

static constexpr bool _verbose = false;


/*
 * An emutls object describes the properties of a thread-local variable.
 * Structure layout as defined in libgcc's 'emutls.c'.
 */
struct __emutls_object
{
  size_t const size;  /* size of the variable */
  size_t const align; /* alignment of the variable */
  void *ptr;          /* used in this implementation for an AVL tree with
                         references to all the thread-local instances */
  void const *templ;  /* template for initializing a thread-local instance */
};


/*
 * AVL node referencing the thread-local variable instance of a specific thread
 */
class Tls_node : public Avl_node<Tls_node>
{
	private:

		void *_thread;  /* key */
		void *_address; /* value */

		/* Noncopyable */
		Tls_node(const Tls_node&);
		void operator=(const Tls_node&);

	public:

		Tls_node(void *thread, void *address)
		: _thread(thread), _address(address) { }

		void *address() { return _address; }

		bool higher(Tls_node *other)
		{
			return (other->_thread > _thread);
		}

		Tls_node *find_by_thread(void *thread)
		{
			if (thread == _thread) return this;

			Tls_node *c = child(thread > _thread);
			return c ? c->find_by_thread(thread) : nullptr;
		}
};


struct Tls_tree : Avl_tree<Tls_node>, List<Tls_tree>::Element { };


/*
 * List referencing all AVL trees.
 * Needed for freeing all allocated variable instances of a thread.
 */
static List<Tls_tree> &_tls_tree_list()
{
	static List<Tls_tree> instance;
	return instance;
}


static Mutex &_emutls_mutex()
{
	static Mutex instance;
	return instance;
}


/*
 * Free all thread-local variable instances of the given thread
 */
void Genode::cxx_free_tls(void *thread)
{
	Mutex::Guard lock_guard(_emutls_mutex());

	for (Tls_tree *tls_tree = _tls_tree_list().first();
		 tls_tree; tls_tree = tls_tree->next()) {

		if (tls_tree->first()) {

			Tls_node *tls_node = tls_tree->first()->find_by_thread(thread);

			if (tls_node) {
				tls_tree->remove(tls_node);
				cxx_heap().free(tls_node->address(), 0);
				destroy(cxx_heap(), tls_node);
			}
		}
	}
}


/*
 * This function is called when a thread-local variable is accessed.
 * It returns the address of the variable for the current thread and
 * allocates and initializes the variable when it is accessed for the
 * first time by this thread.
 */
extern "C" void *__emutls_get_address(void *obj)
{
	Mutex::Guard lock_guard(_emutls_mutex());

	__emutls_object *emutls_object = reinterpret_cast<__emutls_object*>(obj);

	if (_verbose)
		log(__func__, ": emutls_object: ", emutls_object,
		    ", size: ", emutls_object->size,
		    ", align: ", emutls_object->align,
		    ", ptr: ", emutls_object->ptr,
		    ", templ: ", emutls_object->templ);

	if (!emutls_object->ptr) {
		/*
		 * The variable is accessed for the first time by any thread.
		 * Create an AVL tree which keeps track of all instances of this
		 * variable.
		 */
		Tls_tree *tls_tree = new (cxx_heap()) Tls_tree;
		_tls_tree_list().insert(tls_tree);
		emutls_object->ptr = tls_tree;
	}

	Tls_tree *tls_tree = static_cast<Tls_tree*>(emutls_object->ptr);

	Thread *myself = Thread::myself();

	Tls_node *tls_node = nullptr;

	if (tls_tree->first())
		tls_node = tls_tree->first()->find_by_thread(myself);

	if (!tls_node) {

		/*
		 * The variable is accessed for the first time by this thread.
		 * Allocate and initialize a new variable instance and store a
		 * reference in the AVL tree.
		 */

		/* the heap allocates 16-byte aligned */
		if ((16 % emutls_object->align) != 0)
			Genode::warning(__func__, ": cannot ensure alignment of ",
			                emutls_object->align, " bytes");

		void *address = nullptr;

		if (!cxx_heap().alloc(emutls_object->size, &address)) {
			Genode::error(__func__,
			              ": could not allocate thread-local variable instance");
			return nullptr;
		}

		if (emutls_object->templ)
			memcpy(address, emutls_object->templ, emutls_object->size);
		else
			memset(address, 0, emutls_object->size);

		tls_node = new (cxx_heap()) Tls_node(myself, address);

		tls_tree->insert(tls_node);
	}

	return tls_node->address();
}
