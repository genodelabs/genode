/*
 * \brief   Kernel object identities and references
 * \author  Stefan Kalkowski
 * \date    2012-11-30
 */

/*
 * Copyright (C) 2012-2025 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#include <kernel/object.h>
#include <kernel/pd.h>

#include <util/construct_at.h>

using namespace Kernel;


/************
 ** Object **
 ************/

Object::Object(Thread &obj)
:
	_type { THREAD },
	_obj  { (void *)&obj }
{ }


Object::Object(Irq &obj)
:
	_type { IRQ },
	_obj  { (void *)&obj }
{ }


Object::Object(Signal_receiver &obj)
:
	_type { SIGNAL_RECEIVER },
	_obj  { (void *)&obj }
{ }


Object::Object(Signal_context &obj)
:
	_type { SIGNAL_CONTEXT },
	_obj  { (void *)&obj }
{ }


Object::Object(Pd &obj)
:
	_type { PD },
	_obj  { (void *)&obj }
{ }


Object::Object(Vcpu &obj)
:
	_type { VCPU },
	_obj  { (void *)&obj }
{ }


Object::~Object()
{
	for (Object_identity * oi = first(); oi; oi = first())
		oi->invalidate();
}


namespace Kernel {

	template <> Pd *Object::obj<Pd>() const
	{
		if (_type != PD)
			return nullptr;

		return reinterpret_cast<Pd *>(_obj);
	}

	template <> Irq *Object::obj<Irq>() const
	{
		if (_type != IRQ)
			return nullptr;

		return reinterpret_cast<Irq *>(_obj);
	}

	template <> Signal_receiver *Object::obj<Signal_receiver>() const
	{
		if (_type != SIGNAL_RECEIVER)
			return nullptr;

		return reinterpret_cast<Signal_receiver *>(_obj);
	}

	template <> Signal_context *Object::obj<Signal_context>() const
	{
		if (_type != SIGNAL_CONTEXT)
			return nullptr;

		return reinterpret_cast<Signal_context *>(_obj);
	}

	template <> Thread *Object::obj<Thread>() const
	{
		if (_type != THREAD)
			return nullptr;

		return reinterpret_cast<Thread *>(_obj);
	}

	template <> Vcpu *Object::obj<Vcpu>() const
	{
		if (_type != VCPU)
			return nullptr;

		return reinterpret_cast<Vcpu *>(_obj);
	}
}


/*********************
 ** Object_identity **
 *********************/

void Object_identity::invalidate()
{
	for (Object_identity_reference * oir = first(); oir; oir = first())
		oir->invalidate();

	if (_object) {
		_object->remove(this);
		_object = nullptr;
	}
}


Object_identity::Object_identity(Object & object)
:
	_object(&object)
{
	_object->insert(this);
}


Object_identity::~Object_identity() { invalidate(); }


/*******************************
 ** Object_identity_reference **
 *******************************/

Object_identity_reference *
Object_identity_reference::find(Pd &pd)
{
	if (!_identity)
		return nullptr;

	for (Object_identity_reference * oir = _identity->first();
	     oir; oir = oir->next()) {

		if (&pd == &(oir->_pd))
			return oir;
	}

	return nullptr;
}


Object_identity_reference *
Object_identity_reference::find(capid_t capid)
{
	using Avl_node_base = Genode::Avl_node<Object_identity_reference>;

	if (capid == _capid)
		return this;

	Object_identity_reference * subtree =
		Avl_node_base::child(capid > _capid);

	return (subtree) ? subtree->find(capid) : nullptr;
}


Object_identity_reference * Object_identity_reference::factory(void * dst,
                                                               Pd   & pd)
{
	using namespace Genode;
	return !_identity ?
		nullptr : construct_at<Object_identity_reference>(dst, _identity, pd);
}


void Object_identity_reference::invalidate()
{
	if (_identity)
		_identity->remove(this);

	_identity = nullptr;
}


Object_identity_reference::Object_identity_reference(Object_identity *oi,
                                                     Pd              &pd)
:
	_capid((capid_t)pd.capid_alloc().alloc()), _identity(oi), _pd(pd), _in_utcbs(0)
{
	if (_identity)
		_identity->insert(this);

	_pd.cap_tree().insert(this);
}


Object_identity_reference::~Object_identity_reference()
{
	invalidate();
	_pd.cap_tree().remove(this);
	_pd.capid_alloc().free(_capid);
}


Object_identity_reference * Object_identity_reference_tree::find(capid_t id)
{
	return (first()) ? first()->find(id) : nullptr;
}
