/*
 * \brief  Child entry widget implementation
 * \author Christian Prochaska
 * \date   2008-04-06
 */

/*
 * Copyright (C) 2008 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#include "child_entry.h"

Child_entry::Child_entry(Launchpad_child::Name const &name, int quota_kb,
                         int max_quota_kb, Launchpad &launchpad,
                         Launchpad_child &launchpad_child,
                         QWidget *parent)
: QWidget(parent), _launchpad(launchpad), _launchpad_child(launchpad_child)
{
	ui.setupUi(this);

	ui.nameLabel->setText(name.string());
	ui.quotaBar->setMaximum(max_quota_kb);
	ui.quotaBar->setValue(quota_kb);
}


void Child_entry::on_exitButton_clicked()
{
	_launchpad.exit_child(_launchpad_child);
}
