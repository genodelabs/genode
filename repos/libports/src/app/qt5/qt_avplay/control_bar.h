/*
 * \brief   Control bar
 * \author  Christian Prochaska
 * \date    2012-03-30
 */

/*
 * Copyright (C) 2012-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _CONTROL_BAR_H_
#define _CONTROL_BAR_H_

/* Qt includes */
#include <QtGui>
#include <QtWidgets>

/* Qoost includes */
#include <qoost/compound_widget.h>
#include <qoost/qmember.h>

/* Genode includes */
#include <input/component.h>

struct Play_pause_button : QPushButton { Q_OBJECT };
struct Stop_button : QPushButton { Q_OBJECT };
struct Volume_label : QLabel { Q_OBJECT };
struct Volume_slider : QSlider { Q_OBJECT };

class Control_bar : public Compound_widget<QWidget, QHBoxLayout>
{
	Q_OBJECT

	private:

		Input::Session_component  &_input;

		QMember<Play_pause_button> _play_pause_button;
		QMember<Stop_button>       _stop_button;
		QMember<Volume_label>      _volume_label;
		QMember<Volume_slider>     _volume_slider;

		bool _playing;

		void _rewind();

	private Q_SLOTS:

		void _pause_resume();
		void _stop();

	public:

		Control_bar(Input::Session_component &input);

	Q_SIGNALS:

		void volume_changed(int value);
};

#endif /* _CONTROL_BAR_H_ */
