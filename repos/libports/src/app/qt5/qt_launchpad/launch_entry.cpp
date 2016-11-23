/*
 * \brief  Launcher entry widget implementation
 * \author Christian Prochaska
 * \date   2008-04-06
 */

#include "launch_entry.h"

Launch_entry::Launch_entry(Launchpad_child::Name const &prg_name,
                           unsigned long default_quota,
                           unsigned long max_quota,
                           Launchpad *launchpad,
                           Genode::Dataspace_capability config_ds,
                           QWidget *parent)
: QWidget(parent),
  _prg_name(prg_name),
  _launchpad(launchpad),
  _config_ds(config_ds)
{
	ui.setupUi(this);

	ui.launchButton->setText(prg_name.string());

	ui.quotaDial->setMaximum(max_quota);
	ui.quotaDial->setSingleStep(max_quota / 100);
	ui.quotaDial->setValue(default_quota);
}


void Launch_entry::on_launchButton_clicked()
{
	_launchpad->start_child(_prg_name,
	                        1024 * ui.quotaDial->value(),
	                        _config_ds);
}
