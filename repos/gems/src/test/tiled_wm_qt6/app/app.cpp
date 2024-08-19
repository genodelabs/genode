/*
 * \brief  Tiled-WM test: example application widget
 * \author Christian Helmuth
 * \date   2018-09-28
 */

/*
 * Copyright (C) 2018 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Qt includes */
#include <QApplication>

/* local includes */
#include "app.h"


App::App(QString name)
{
	_name->setText("This is <b>" + name + "</b> an example application for the tiled-WM test.");
	_entry->setPlaceholderText("Placeholder text");

	_layout->addWidget(new Spacer(), 1);
	_layout->addWidget(_name);
	_layout->addWidget(new Spacer(), 1);
	_layout->addWidget(_entry);
	_layout->addWidget(new Spacer(), 1);

	for (int i = 0; i < 3; ++i) {
		QLabel *l = new QLabel(QString("QLabel No." + QString::number(i)));
		l->setToolTip(QString::number(i) + " is just a number.");
		_layout->addWidget(l);
	}

	_layout->addWidget(new Spacer(), 1);
}


App::~App()
{
}
