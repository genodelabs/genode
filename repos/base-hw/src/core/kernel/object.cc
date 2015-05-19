#include <kernel/object.h>
#include <kernel/pd.h>
#include <kernel/kernel.h>

#include <util/construct_at.h>

using namespace Kernel;

Object_identity::Object_identity(Object & object) : _object(object) { }


Object_identity::~Object_identity()
{
	for (Object_identity_reference * oir = first(); oir; oir = first())
		oir->invalidate();
}


Object_identity_reference *
Object_identity_reference::find(Pd * pd)
{
	if (!_identity) return nullptr;

	for (Object_identity_reference * oir = _identity->first();
	     oir; oir = oir->next())
		if (pd == &(oir->_pd)) return oir;
	return nullptr;
}


Object_identity_reference *
Object_identity_reference::find(capid_t capid)
{
	using Avl_node_base = Genode::Avl_node<Object_identity_reference>;

	if (capid == _capid) return this;
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


void Object_identity_reference::invalidate() {
	if (_identity) _identity->remove(this); }


Object_identity_reference::Object_identity_reference(Object_identity *oi,
                                                     Pd              &pd)
: _capid(pd.capid_alloc().alloc()), _identity(oi), _pd(pd)
{
	if (_identity) _identity->insert(this);
	_pd.cap_tree().insert(this);
}


Object_identity_reference::~Object_identity_reference()
{
	invalidate();
	_pd.cap_tree().remove(this);
	_pd.capid_alloc().free(_capid);
}


Object_identity_reference * Object_identity_reference_tree::find(capid_t id) {
	return (first()) ? first()->find(id) : nullptr; }
