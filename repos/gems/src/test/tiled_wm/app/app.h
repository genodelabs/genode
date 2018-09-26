/*
 * \brief  TIled-WM test: example application widget
 * \author Christian Helmuth
 * \date   2018-09-28
 */

/*
 * Copyright (C) 2018 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _TEST__TILED_WM__APP__APP_H_
#define _TEST__TILED_WM__APP__APP_H_

/* Qt includes */
#include <QVBoxLayout>
#include <QLabel>
#include <QWidget>
#include <QLineEdit>

/* Qoost includes */
#include <qoost/compound_widget.h>
#include <qoost/qmember.h>

/* local includes */
#include <util.h>


class App : public Compound_widget<QWidget, QVBoxLayout>
{
	Q_OBJECT

	private:

		QMember<QLabel>    _name;
		QMember<QLineEdit> _entry;

	public:

		App(QString name);
		~App();
};

#endif /* _TEST__TILED_WM__APP__APP_H_ */
