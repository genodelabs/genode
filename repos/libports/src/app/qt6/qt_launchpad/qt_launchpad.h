/*
 * \brief   Qt Launchpad window interface
 * \author  Christian Prochaska
 * \date    2008-04-05
 */

/*
 * Copyright (C) 2008-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef QT_LAUNCHPAD_H
#define QT_LAUNCHPAD_H

#include <launchpad/launchpad.h>

#include <QtGui>
#include "ui_qt_launchpad.h"

class Qt_launchpad : public QMainWindow, public Launchpad, private Ui::Qt_launchpadClass
{
	Q_OBJECT

	private:

		Genode::Env &_env;

	private slots:

		void _avail_quota_update();

	public:

		Qt_launchpad(Genode::Env &env, unsigned long initial_quota,
		             QWidget *parent = 0);

		virtual void quota(unsigned long quota) override;

		virtual	void add_launcher(Launchpad_child::Name const &binary_name,
		                          Cap_quota caps, unsigned long default_quota,
		                          Genode::Dataspace_capability config_ds) override;

		virtual void add_child(Launchpad_child::Name const &name,
		                       unsigned long quota,
		                       Launchpad_child &launchpad_child,
		                       Genode::Allocator &alloc) override;

		virtual void remove_child(Launchpad_child::Name const &name,
		                          Genode::Allocator &alloc) override;
};

#endif /* QT_LAUNCHPAD_H */
