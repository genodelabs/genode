/*
 * \brief   Main window of the media player
 * \author  Christian Prochaska
 * \date    2012-03-29
 */

/*
 * Copyright (C) 2012 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _MAIN_WINDOW_H_
#define _MAIN_WINDOW_H_

/* Qt includes */
#include <QVBoxLayout>
#include <QWidget>
#include <qnitpickerviewwidget/qnitpickerviewwidget.h>

/* Qoost includes */
#include <qoost/compound_widget.h>
#include <qoost/qmember.h>

#include "control_bar.h"


class Main_window : public Compound_widget<QWidget, QVBoxLayout>
{
	Q_OBJECT

	private:

		QMember<QNitpickerViewWidget> _avplay_widget;
		QMember<Control_bar>          _control_bar;

	public:

		Main_window();
};

#endif /* _MAIN_WINDOW_H_ */
