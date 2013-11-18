/*
 * \brief   Control bar
 * \author  Stefan Kalkowski
 * \date    2013-04-17
 */

/*
 * Copyright (C) 2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _CONTROL_BAR_H_
#define _CONTROL_BAR_H_

/* Genode includes */
#include <base/signal.h>

/* Qt includes */
#include <QtGui>

/* Qoost includes */
#include <qoost/compound_widget.h>
#include <qoost/qmember.h>

struct Play_pause_button : QPushButton { Q_OBJECT };
struct Stop_button       : QPushButton { Q_OBJECT };
struct Bomb_button       : QPushButton { Q_OBJECT };
struct Power_button      : QPushButton { Q_OBJECT };

class Control_bar : public Compound_widget<QFrame, QHBoxLayout>
{
	Q_OBJECT

	private:

		QMember<Play_pause_button> _play_pause_button;
		QMember<Stop_button>       _stop_button;
		QMember<Bomb_button>       _bomb_button;
		QMember<Power_button>      _power_button;

		Genode::Signal_transmitter _st_play;
		Genode::Signal_transmitter _st_stop;
		Genode::Signal_transmitter _st_bomb;
		Genode::Signal_transmitter _st_power;

		bool _playing;


	private Q_SLOTS:

		void _pause_resume();
		void _stop();
		void _bomb();
		void _power();

	public:

		Control_bar();

		void play_sigh(Genode::Signal_context_capability cap)  {
			_st_play.context(cap);  }
		void stop_sigh(Genode::Signal_context_capability cap)  {
			_st_stop.context(cap);  }
		void bomb_sigh(Genode::Signal_context_capability cap)  {
			_st_bomb.context(cap);  }
		void power_sigh(Genode::Signal_context_capability cap) {
			_st_power.context(cap); }
};

#endif /* _CONTROL_BAR_H_ */
