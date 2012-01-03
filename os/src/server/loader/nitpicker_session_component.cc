/*
 * \brief  Nitpicker session component
 * \author Christian Prochaska
 * \date   2010-09-02
 */

/*
 * Copyright (C) 2010-2012 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* local includes */
#include <nitpicker_session_component.h>
#include <input_root.h>


using namespace Nitpicker;
using namespace Genode;


static long session_arg(const char *arg, const char *key)
{
	return Arg_string::find_arg(arg, key).long_value(0);
}


Session_component::Session_component(Rpc_entrypoint  *ep,
                                     Timed_semaphore *ready_sem,
                                     const char      *args)
:
	/* connect to the "real" Nitpicker service */
	_nitpicker(session_arg(args, "fb_width"),
	           session_arg(args, "fb_height")),
	_ep(ep), _ready_sem(ready_sem), _input_root(_ep, env()->heap(), this)
{
	_proxy_input_session = static_cap_cast<Input::Session>(_input_root.session("ram_quota=256K"));
}


Session_component::~Session_component()
{
	if (_loader_view) {
		_ep->dissolve(_loader_view);
		destroy(env()->heap(), _loader_view);
	}

	if (_proxy_view) {
		_ep->dissolve(_proxy_view);
		destroy(env()->heap(), _proxy_view);
	}

	if (_proxy_input_session.valid())
		_input_root.close(_proxy_input_session);

	if (_nitpicker_view.valid())
		_nitpicker.destroy_view(_nitpicker_view);
}


Framebuffer::Session_capability Session_component::framebuffer_session()
{
	return _nitpicker.framebuffer_session();
}


Input::Session_capability Session_component::input_session()
{
	return _proxy_input_session;
}


View_capability Nitpicker::Session_component::create_view()
{
	/* only one view is allowed */
	if (_proxy_view_cap.valid())
		return View_capability();

	/* create Nitpicker view */
	_nitpicker_view = _nitpicker.create_view();

	/* create proxy view component for the child */
	_proxy_view = new (env()->heap())
		View_component(_nitpicker_view, _ready_sem);

	/* create proxy view capability for the child */
	_proxy_view_cap = View_capability(_ep->manage(_proxy_view));

	/* create view component accessed by the client */
	_loader_view = new (env()->heap())
		Loader_view_component(_nitpicker_view);

	/* create proxy view capability for the application */
	_loader_view_cap = View_capability(_ep->manage(_loader_view));

	/* return proxy view capability */
	return _proxy_view_cap;
}


void Session_component::destroy_view(View_capability view)
{
	if (!_loader_view_cap.valid()) return;

	/* destroy proxy views */
	_ep->dissolve(_proxy_view);
	destroy(env()->heap(), _proxy_view);
	_proxy_view = 0;

	_ep->dissolve(_loader_view);
	destroy(env()->heap(), _loader_view);
	_loader_view = 0;

	/* destroy nitpicker view */
	_nitpicker.destroy_view(_nitpicker_view);
}


int Session_component::background(View_capability view)
{
	/* not forwarding to real nitpicker session */
	return 0;
}


View_capability Session_component::loader_view(int *w, int *h, int *buf_x, int *buf_y)
{
	/* return the view geometry as set by the child */
	if (_proxy_view) {
		_proxy_view->get_viewport(0, 0, w, h, buf_x, buf_y);
	}
	return _loader_view_cap;
}


Input::Session_capability Session_component::real_input_session()
{
	return _nitpicker.input_session();
}


Loader_view_component *Session_component::loader_view_component()
{
	return _loader_view;
}


View_component *Session_component::proxy_view_component()
{
	return _proxy_view;
}
