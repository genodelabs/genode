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

#include <QWidget>

#include "ui_launch_entry.h"

class Launch_entry : public QWidget
{
	Q_OBJECT

	private:

		Ui::Launch_entryClass ui;

		const char *_filename;
		Genode::Dataspace_capability _config_ds;
		Launchpad *_launchpad;

	private slots:

		void on_launchButton_clicked();

	public:

		Launch_entry(const char *filename,
		             unsigned long default_quota,
		             unsigned long max_quota,
		             Genode::Dataspace_capability config_ds,
		             Launchpad *launchpad,
		             QWidget *parent = 0);
};

#endif /* LAUNCH_ENTRY_H */
