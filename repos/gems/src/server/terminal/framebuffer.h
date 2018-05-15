/*
 * \brief  Terminal framebuffer output backend
 * \author Norman Feske
 * \date   2018-02-06
 */

/*
 * Copyright (C) 2018 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _FRAMEBUFFER_H_
#define _FRAMEBUFFER_H_

/* Genode includes */
#include <framebuffer_session/connection.h>

/* local includes */
#include "types.h"

namespace Terminal { class Framebuffer; }

class Terminal::Framebuffer
{
	private:

		Env &_env;

		::Framebuffer::Connection _fb { _env, ::Framebuffer::Mode() };

		Constructible<Attached_dataspace> _ds { };

		::Framebuffer::Mode _mode { };

	public:

		/**
		 * Constructor
		 *
		 * \param mode_sigh  signal handler to be triggered on mode changes
		 */
		Framebuffer(Env &env, Signal_context_capability mode_sigh) : _env(env)
		{
			switch_to_new_mode();
			_fb.mode_sigh(mode_sigh);
		}

		unsigned w() const { return _mode.width(); }
		unsigned h() const { return _mode.height(); }

		template <typename PT>
		PT *pixel() { return _ds->local_addr<PT>(); }

		void refresh(Rect rect)
		{
			_fb.refresh(rect.x1(), rect.y1(), rect.w(), rect.h());
		}

		/**
		 * Return true if the framebuffer mode differs from the current
		 * terminal size.
		 */
		bool mode_changed() const
		{
			::Framebuffer::Mode _new_mode = _fb.mode();

			return _new_mode.width()  != _mode.width()
			    || _new_mode.height() != _mode.height();
		}

		void switch_to_new_mode()
		{
			/*
			 * The mode information must be obtained before updating the
			 * dataspace to ensure that the mode is consistent with the
			 * obtained version of the dataspace.
			 *
			 * Otherwise - if the server happens to change the mode just after
			 * the dataspace update - the mode information may correspond to
			 * the next pending mode at the server while we are operating on
			 * the old (possibly too small) dataspace.
			 */
			_mode = _fb.mode();
			if (_mode.width() && _mode.height())
				_ds.construct(_env.rm(), _fb.dataspace());
		}
};

#endif /* _FRAMEBUFFER_H_ */
