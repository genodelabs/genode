/*
 * \brief   Main window of the VMM GUI
 * \author  Stefan Kalkowski
 * \date    2013-04-17
 */

/*
 * Copyright (C) 2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _MAIN_WINDOW_H_
#define _MAIN_WINDOW_H_

/* Genode includes */
#include <cpu/cpu_state.h>

/* Qt includes */
#include <QVBoxLayout>
#include <QWidget>
#include <qnitpickerviewwidget/qnitpickerviewwidget.h>

/* Qoost includes */
#include <qoost/compound_widget.h>
#include <qoost/qmember.h>

#include "control_bar.h"


class Framebuffer_widget : public Compound_widget<QGroupBox, QHBoxLayout>
{
	Q_OBJECT

	private:

		QMember<QNitpickerViewWidget> _widget;

	public:

		Framebuffer_widget();

		QNitpickerViewWidget &my_widget() { return *_widget; }
};


class Register_widget : public Compound_widget<QGroupBox, QGridLayout, 1>
{
	Q_OBJECT

	private:

		QMember<QLabel> _l_r0;
		QMember<QLabel> _l_r1;
		QMember<QLabel> _l_r2;
		QMember<QLabel> _l_r3;
		QMember<QLabel> _l_r4;
		QMember<QLabel> _l_r5;
		QMember<QLabel> _l_r6;
		QMember<QLabel> _l_r7;
		QMember<QLabel> _l_r8;
		QMember<QLabel> _l_r9;
		QMember<QLabel> _l_r10;
		QMember<QLabel> _l_r11;
		QMember<QLabel> _l_r12;
		QMember<QLabel> _l_r13;
		QMember<QLabel> _l_r14;
		QMember<QLabel> _l_r15;
		QMember<QLabel> _l_cpsr;
		QMember<QLabel> _l_sp_und;
		QMember<QLabel> _l_lr_und;
		QMember<QLabel> _l_spsr_und;
		QMember<QLabel> _l_sp_svc;
		QMember<QLabel> _l_lr_svc;
		QMember<QLabel> _l_spsr_svc;
		QMember<QLabel> _l_sp_abt;
		QMember<QLabel> _l_lr_abt;
		QMember<QLabel> _l_spsr_abt;
		QMember<QLabel> _l_sp_irq;
		QMember<QLabel> _l_lr_irq;
		QMember<QLabel> _l_spsr_irq;
		QMember<QLabel> _l_sp_fiq;
		QMember<QLabel> _l_lr_fiq;
		QMember<QLabel> _l_spsr_fiq;
		QMember<QLabel> _l_exc_lab;
		QMember<QLabel> _l_exc_type;
		QMember<QLineEdit> _le_r0;
		QMember<QLineEdit> _le_r1;
		QMember<QLineEdit> _le_r2;
		QMember<QLineEdit> _le_r3;
		QMember<QLineEdit> _le_r4;
		QMember<QLineEdit> _le_r5;
		QMember<QLineEdit> _le_r6;
		QMember<QLineEdit> _le_r7;
		QMember<QLineEdit> _le_r8;
		QMember<QLineEdit> _le_r9;
		QMember<QLineEdit> _le_r10;
		QMember<QLineEdit> _le_r11;
		QMember<QLineEdit> _le_r12;
		QMember<QLineEdit> _le_r13;
		QMember<QLineEdit> _le_r14;
		QMember<QLineEdit> _le_r15;
		QMember<QLineEdit> _le_cpsr;
		QMember<QLineEdit> _le_sp_und;
		QMember<QLineEdit> _le_lr_und;
		QMember<QLineEdit> _le_spsr_und;
		QMember<QLineEdit> _le_sp_svc;
		QMember<QLineEdit> _le_lr_svc;
		QMember<QLineEdit> _le_spsr_svc;
		QMember<QLineEdit> _le_sp_abt;
		QMember<QLineEdit> _le_lr_abt;
		QMember<QLineEdit> _le_spsr_abt;
		QMember<QLineEdit> _le_sp_irq;
		QMember<QLineEdit> _le_lr_irq;
		QMember<QLineEdit> _le_spsr_irq;
		QMember<QLineEdit> _le_sp_fiq;
		QMember<QLineEdit> _le_lr_fiq;
		QMember<QLineEdit> _le_spsr_fiq;

	public:

		Register_widget();

		void set_state(Genode::Cpu_state_modes *state);
};


class Main_window : public Compound_widget<QFrame, QVBoxLayout, 10>
{
	Q_OBJECT

	private:

		QMember<Framebuffer_widget>   _fb_widget;
		QMember<Register_widget>      _reg_widget;
		QMember<Control_bar>          _control_bar;

	public:

		Main_window();
};


class Background_window : public Compound_widget<QFrame, QVBoxLayout>
{
	Q_OBJECT

	private:

		QMember<Main_window> _main;

	public:

		Background_window();
};


#endif /* _MAIN_WINDOW_H_ */
