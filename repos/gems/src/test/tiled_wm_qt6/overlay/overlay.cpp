/*
 * \brief  Tiled-WM test: overlay widget
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
#include "overlay.h"


Overlay::Overlay()
{
	_label->setText("WiFi overlay");
	_entry->setEchoMode(QLineEdit::Password);

	_layout->addWidget(new Spacer(), 1);
	_layout->addWidget(_label);
	_layout->addWidget(new Spacer(), 1);
	_layout->addWidget(_entry);
	_layout->addWidget(new Spacer(), 1);
}


Overlay::~Overlay()
{
}
