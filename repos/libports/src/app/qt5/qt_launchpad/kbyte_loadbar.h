/*
 * \brief   KByte loadbar interface
 * \author  Christian Prochaska
 * \date    2008-04-05
 */

/*
 * Copyright (C) 2008-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef KBYTE_LOADBAR_H
#define KBYTE_LOADBAR_H

#include <QProgressBar>

class Kbyte_loadbar : public QProgressBar
{
	Q_OBJECT

	public:

		Kbyte_loadbar(QWidget *parent = 0);

		virtual QString text() const;
};

#endif /* KBYTE_LOADBAR_H */
