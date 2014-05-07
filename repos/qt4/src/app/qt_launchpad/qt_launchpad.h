/*
 * \brief   Qt Launchpad window interface
 * \author  Christian Prochaska
 * \date    2008-04-05
 */

/*
 * Copyright (C) 2008-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef QT_LAUNCHPAD_H
#define QT_LAUNCHPAD_H

#include <launchpad/launchpad.h>

#include <QtGui>
#include "ui_qt_launchpad.h"

class Qt_launchpad : public QMainWindow, public Launchpad, private Ui::Qt_launchpadClass
{
    Q_OBJECT

public:
    Qt_launchpad(unsigned long initial_quota, QWidget *parent = 0);
    ~Qt_launchpad();

		virtual void quota(unsigned long quota);

		virtual	void add_launcher(const char *filename,
		                          unsigned long default_quota);

		virtual void add_child(const char *unique_name,
		                       unsigned long quota,
													 Launchpad_child *launchpad_child,
		                       Genode::Allocator *alloc);
														 
		virtual void remove_child(const char *name, Genode::Allocator *alloc);
									
private slots:
		void avail_quota_update();
};

#endif // QT_LAUNCHPAD_H
