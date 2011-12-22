/*
 * \brief  Launcher entry widget implementation
 * \author Christian Prochaska
 * \date   2008-04-06
 */

#include "launch_entry.h"

Launch_entry::Launch_entry(const char *filename, unsigned long default_quota,
                           unsigned long max_quota, Launchpad *launchpad,
                           QWidget *parent)
    : QWidget(parent), _filename(filename), _launchpad(launchpad)
{
	ui.setupUi(this);
	
	ui.launchButton->setText(filename);
	
	ui.quotaDial->setMaximum(max_quota);
	ui.quotaDial->setSingleStep(max_quota / 100);
	ui.quotaDial->setValue(default_quota);
}

Launch_entry::~Launch_entry()
{

}

void Launch_entry::on_launchButton_clicked()
{
	_launchpad->start_child(_filename, 1024 * ui.quotaDial->value(), Genode::Dataspace_capability());
}
