/*
 * \brief  Launcher entry widget interface
 * \author Christian Prochaska
 * \date   2008-04-06
 */

/*
 * Copyright (C) 2008-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef LAUNCH_ENTRY_H
#define LAUNCH_ENTRY_H

#include <launchpad/launchpad.h>

#include <QtGui/QWidget>

#include "ui_launch_entry.h"

class Launch_entry : public QWidget
{
    Q_OBJECT

public:
    Launch_entry(const char *filename, unsigned long default_quota,
                 unsigned long max_quota, Launchpad *launchpad,
                 QWidget *parent = 0);
    ~Launch_entry();

private:
    Ui::Launch_entryClass ui;

		const char *_filename;
		Launchpad *_launchpad;

private slots:
		void on_launchButton_clicked();
};

#endif // LAUNCH_ENTRY_H
