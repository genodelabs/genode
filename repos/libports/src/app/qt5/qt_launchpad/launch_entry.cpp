/*
 * \brief  Launcher entry widget implementation
 * \author Christian Prochaska
 * \date   2008-04-06
 */

/*
 * Copyright (C) 2008 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#include "launch_entry.h"

Launch_entry::Launch_entry(Launchpad_child::Name const &prg_name,
                           Launchpad::Cap_quota caps,
                           unsigned long default_quota,
                           unsigned long max_quota,
                           Launchpad *launchpad,
                           Genode::Dataspace_capability config_ds,
                           QWidget *parent)
: QWidget(parent),
  _prg_name(prg_name),
  _launchpad(launchpad),
  _config_ds(config_ds),
  _caps(caps)
{
	ui.setupUi(this);

	ui.launchButton->setText(prg_name.string());

	ui.quotaDial->setMaximum(max_quota);
	ui.quotaDial->setSingleStep(max_quota / 100);
	ui.quotaDial->setValue(default_quota);
}


void Launch_entry::on_launchButton_clicked()
{
	Launchpad::Ram_quota ram_quota = { 1024UL * ui.quotaDial->value() };
	_launchpad->start_child(_prg_name, _caps, ram_quota, _config_ds);
}
