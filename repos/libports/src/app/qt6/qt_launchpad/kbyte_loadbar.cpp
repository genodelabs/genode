/*
 * \brief   KByte loadbar implementation
 * \author  Christian Prochaska
 * \date    2008-04-05
 */

/*
 * Copyright (C) 2008 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#include "kbyte_loadbar.h"


Kbyte_loadbar::Kbyte_loadbar(QWidget *parent)
: QProgressBar(parent) { }


QString Kbyte_loadbar::text() const
{
	return QString::number(value()) + " KByte / " + 
	                       QString::number(maximum()) + " KByte";
}
