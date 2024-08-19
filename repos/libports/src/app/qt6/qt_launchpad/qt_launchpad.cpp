/*
 * \brief   Qt Launchpad window implementation
 * \author  Christian Prochaska
 * \date    2008-04-05
 */

/*
 * Copyright (C) 2008-2020 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#include <QScrollArea>

#include <base/log.h>

#include "qt_launchpad.h"

#include "launch_entry.h"
#include "child_entry.h"

Qt_launchpad::Qt_launchpad(Genode::Env &env, unsigned long initial_quota,
                           QWidget *parent)
: QMainWindow(parent), Launchpad(env, initial_quota), _env(env)
{
	setupUi(this);

	// disable minimize and maximize buttons
	Qt::WindowFlags flags = windowFlags();
	flags &= ~Qt::WindowMinMaxButtonsHint;
	setWindowFlags(flags);

	// put a QScrollArea into launcherDockWidget for scrolling of launcher entries
	QScrollArea *launcherScrollArea = new QScrollArea;
	launcherScrollArea->setFrameStyle(QFrame::NoFrame);
	launcherScrollArea->setWidget(launcherDockWidgetContents);

	launcherDockWidget->setWidget(launcherScrollArea);

	QVBoxLayout *launcherDockWidgetLayout = new QVBoxLayout;
	launcherDockWidgetLayout->setContentsMargins(2, 2, 2, 2);
	launcherDockWidgetLayout->setSpacing(2);
	launcherDockWidgetContents->setLayout(launcherDockWidgetLayout);

	// put a QScrollArea into childrenDockWidget for scrolling of child entries
	QScrollArea *childrenScrollArea = new QScrollArea;
	childrenScrollArea->setFrameStyle(QFrame::NoFrame);
	childrenScrollArea->setWidget(childrenDockWidgetContents);

	childrenDockWidget->setWidget(childrenScrollArea);

	QVBoxLayout *childrenDockWidgetLayout = new QVBoxLayout;
	childrenDockWidgetLayout->setContentsMargins(2, 2, 2, 2);
	childrenDockWidgetLayout->setSpacing(2);
	childrenDockWidgetContents->setLayout(childrenDockWidgetLayout);

	// update the available quota bar every 200ms
	QTimer *avail_quota_timer = new QTimer(this);
	connect(avail_quota_timer, SIGNAL(timeout()), this, SLOT(_avail_quota_update()));
	avail_quota_timer->start(200);
}


void Qt_launchpad::_avail_quota_update()
{
	static Genode::size_t _avail = 0;

	Genode::size_t new_avail = _env.pd().avail_ram().value;

	if (new_avail != _avail)
		quota(new_avail);

	_avail = new_avail;
}


void Qt_launchpad::quota(unsigned long quota)
{
	totalQuotaProgressBar->setMaximum(initial_quota() / 1024);
	totalQuotaProgressBar->setValue(quota / 1024);
}


void Qt_launchpad::add_launcher(Launchpad_child::Name const &binary_name,
                                Cap_quota caps, unsigned long default_quota,
                                Genode::Dataspace_capability config_ds)
{
	Launch_entry *launch_entry = new Launch_entry(binary_name,
	                                              caps,
	                                              default_quota / 1024,
	                                              initial_quota() / 1024,
	                                              this,
	                                              config_ds);
	                                              launcherDockWidgetContents->layout()->addWidget(launch_entry);
	                                              launch_entry->show();
	                                              launcherDockWidgetContents->adjustSize();
}


void Qt_launchpad::add_child(Launchpad_child::Name const &name,
                             unsigned long quota,
                             Launchpad_child &launchpad_child,
                             Genode::Allocator &)
{
	Child_entry *child_entry = new Child_entry(name, quota / 1024,
                                             initial_quota() / 1024,
                                             *this, launchpad_child);
	child_entry->setObjectName(QString(name.string()) + "_child_entry");
	childrenDockWidgetContents->layout()->addWidget(child_entry);
	child_entry->show();
	childrenDockWidgetContents->adjustSize();
}


void Qt_launchpad::remove_child(Launchpad_child::Name const &name, Genode::Allocator &)
{
	Child_entry *child_entry =
	  childrenDockWidgetContents->findChild<Child_entry*>(QString(name.string()) + "_child_entry");

	if (!child_entry) {
		Genode::warning("child entry lookup failed");
		return;
	}

	// still in "button clicked" event handler
	child_entry->deleteLater();

	childrenDockWidgetContents->adjustSize();
}
