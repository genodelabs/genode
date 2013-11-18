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

/* Genode includes */
#include <base/process.h>
#include <rom_session/connection.h>
#include <cap_session/connection.h>
#include <input/component.h>
#include <base/service.h>

/* Qt includes */
#include <QtGui>
#include <QApplication>

/* Local includes */
#include "main_window.h"
#include "gui_session.h"

using namespace Genode;

Framebuffer_widget::Framebuffer_widget()
{
	_widget->setParent(this);

	_layout->addWidget(_widget);
	setTitle("VM Framebuffer");
	_layout->setContentsMargins(3, 3, 3, 3);
}

Register_widget::Register_widget()
 : _l_r0("r0:"),
   _l_r1("r1:"),
   _l_r2("r2:"),
   _l_r3("r3:"),
   _l_r4("r4:"),
   _l_r5("r5:"),
   _l_r6("r6:"),
   _l_r7("r7:"),
   _l_r8("r8:"),
   _l_r9("r9:"),
   _l_r10("r10:"),
   _l_r11("r11:"),
   _l_r12("r12:"),
   _l_r13("sp:"),
   _l_r14("lr:"),
   _l_r15("ip:"),
   _l_cpsr("cpsr:"),
   _l_sp_svc("sp_svc:"),
   _l_lr_svc("lr_svc:"),
   _l_spsr_svc("spsr_svc:"),
   _l_sp_abt("sp_abt:"),
   _l_lr_abt("lr_abt:"),
   _l_spsr_abt("spsr_abt:"),
   _l_sp_irq("sp_irq:"),
   _l_lr_irq("lr_irq:"),
   _l_spsr_irq("spsr_irq:"),
   _l_exc_lab("Exception type:")
{
	_l_r0->setParent(this);
	_l_r1->setParent(this);
	_l_r2->setParent(this);
	_l_r3->setParent(this);
	_l_r4->setParent(this);
	_l_r5->setParent(this);
	_l_r6->setParent(this);
	_l_r7->setParent(this);
	_l_r8->setParent(this);
	_l_r9->setParent(this);
	_l_r10->setParent(this);
	_l_r11->setParent(this);
	_l_r12->setParent(this);
	_l_exc_lab->setParent(this);
	_le_r0->setParent(this);
	_le_r1->setParent(this);
	_le_r2->setParent(this);
	_le_r3->setParent(this);
	_le_r4->setParent(this);
	_le_r5->setParent(this);
	_le_r6->setParent(this);
	_le_r7->setParent(this);
	_le_r8->setParent(this);
	_le_r9->setParent(this);
	_le_r10->setParent(this);
	_le_r11->setParent(this);
	_le_r12->setParent(this);
	_l_r13->setParent(this);
	_l_r14->setParent(this);
	_l_r15->setParent(this);
	_l_cpsr->setParent(this);
	_l_sp_svc->setParent(this);
	_l_lr_svc->setParent(this);
	_l_spsr_svc->setParent(this);
	_l_sp_abt->setParent(this);
	_l_lr_abt->setParent(this);
	_l_spsr_abt->setParent(this);
	_l_sp_irq->setParent(this);
	_l_lr_irq->setParent(this);
	_l_spsr_irq->setParent(this);
	_l_exc_type->setParent(this);
	_le_r13->setParent(this);
	_le_r14->setParent(this);
	_le_r15->setParent(this);
	_le_cpsr->setParent(this);
	_le_sp_svc->setParent(this);
	_le_lr_svc->setParent(this);
	_le_spsr_svc->setParent(this);
	_le_sp_abt->setParent(this);
	_le_lr_abt->setParent(this);
	_le_spsr_abt->setParent(this);
	_le_sp_irq->setParent(this);
	_le_lr_irq->setParent(this);
	_le_spsr_irq->setParent(this);

	_layout->addWidget(_l_r0,   0, 0);
	_layout->addWidget(_l_r1,   1, 0);
	_layout->addWidget(_l_r2,   2, 0);
	_layout->addWidget(_l_r3,   3, 0);
	_layout->addWidget(_l_r4,   4, 0);
	_layout->addWidget(_l_r5,   5, 0);
	_layout->addWidget(_l_r6,   6, 0);
	_layout->addWidget(_l_r7,   7, 0);
	_layout->addWidget(_l_r8,   8, 0);
	_layout->addWidget(_l_r9,   9, 0);
	_layout->addWidget(_l_r10, 10, 0);
	_layout->addWidget(_l_r11, 11, 0);
	_layout->addWidget(_l_r12, 12, 0);
	_layout->addWidget(_l_exc_lab, 13, 0, 1, 2);

	_layout->addWidget(_le_r0,  0,  1);
	_layout->addWidget(_le_r1,  1,  1);
	_layout->addWidget(_le_r2,  2,  1);
	_layout->addWidget(_le_r3,  3,  1);
	_layout->addWidget(_le_r4,  4,  1);
	_layout->addWidget(_le_r5,  5,  1);
	_layout->addWidget(_le_r6,  6,  1);
	_layout->addWidget(_le_r7,  7,  1);
	_layout->addWidget(_le_r8,  8,  1);
	_layout->addWidget(_le_r9,  9,  1);
	_layout->addWidget(_le_r10, 10, 1);
	_layout->addWidget(_le_r11, 11, 1);
	_layout->addWidget(_le_r12, 12, 1);

	_layout->addWidget(_l_r13,      0,  2);
	_layout->addWidget(_l_r14,      1,  2);
	_layout->addWidget(_l_r15,      2,  2);
	_layout->addWidget(_l_cpsr,     3,  2);
	_layout->addWidget(_l_sp_svc,   4,  2);
	_layout->addWidget(_l_lr_svc,   5,  2);
	_layout->addWidget(_l_spsr_svc, 6,  2);
	_layout->addWidget(_l_sp_abt,   7,  2);
	_layout->addWidget(_l_lr_abt,   8,  2);
	_layout->addWidget(_l_spsr_abt, 9,  2);
	_layout->addWidget(_l_sp_irq,   10, 2);
	_layout->addWidget(_l_lr_irq,   11, 2);
	_layout->addWidget(_l_spsr_irq, 12, 2);
	_layout->addWidget(_l_exc_type, 13, 2, 1, 2);

	_layout->addWidget(_le_r13,      0,  3);
	_layout->addWidget(_le_r14,      1,  3);
	_layout->addWidget(_le_r15,      2,  3);
	_layout->addWidget(_le_cpsr,     3,  3);
	_layout->addWidget(_le_sp_svc,   4,  3);
	_layout->addWidget(_le_lr_svc,   5,  3);
	_layout->addWidget(_le_spsr_svc, 6,  3);
	_layout->addWidget(_le_sp_abt,   7,  3);
	_layout->addWidget(_le_lr_abt,   8,  3);
	_layout->addWidget(_le_spsr_abt, 9,  3);
	_layout->addWidget(_le_sp_irq,   10, 3);
	_layout->addWidget(_le_lr_irq,   11, 3);
	_layout->addWidget(_le_spsr_irq, 12, 3);

	setTitle("VM Registers");
	_layout->setContentsMargins(3, 3, 3, 3);
	_layout->setHorizontalSpacing(3);
}


void Register_widget::set_state(Genode::Cpu_state_modes *state) {
	using namespace Genode;

	QMetaObject::invokeMethod(_le_r0, "setText", Qt::QueuedConnection,
							  Q_ARG(QString, "0x" + QString::number(state->r0, 16)));
	QMetaObject::invokeMethod(_le_r1, "setText", Qt::QueuedConnection,
							  Q_ARG(QString, "0x" + QString::number(state->r1, 16)));
	QMetaObject::invokeMethod(_le_r2, "setText", Qt::QueuedConnection,
							  Q_ARG(QString, "0x" + QString::number(state->r2, 16)));
	QMetaObject::invokeMethod(_le_r3, "setText", Qt::QueuedConnection,
							  Q_ARG(QString, "0x" + QString::number(state->r3, 16)));
	QMetaObject::invokeMethod(_le_r4, "setText", Qt::QueuedConnection,
							  Q_ARG(QString, "0x" + QString::number(state->r4, 16)));
	QMetaObject::invokeMethod(_le_r5, "setText", Qt::QueuedConnection,
							  Q_ARG(QString, "0x" + QString::number(state->r5, 16)));
	QMetaObject::invokeMethod(_le_r6, "setText", Qt::QueuedConnection,
							  Q_ARG(QString, "0x" + QString::number(state->r6, 16)));
	QMetaObject::invokeMethod(_le_r7, "setText", Qt::QueuedConnection,
							  Q_ARG(QString, "0x" + QString::number(state->r7, 16)));
	QMetaObject::invokeMethod(_le_r8, "setText", Qt::QueuedConnection,
							  Q_ARG(QString, "0x" + QString::number(state->r8, 16)));
	QMetaObject::invokeMethod(_le_r9, "setText", Qt::QueuedConnection,
							  Q_ARG(QString, "0x" + QString::number(state->r9, 16)));
	QMetaObject::invokeMethod(_le_r10, "setText", Qt::QueuedConnection,
							  Q_ARG(QString, "0x" + QString::number(state->r10, 16)));
	QMetaObject::invokeMethod(_le_r11, "setText", Qt::QueuedConnection,
							  Q_ARG(QString, "0x" + QString::number(state->r11, 16)));
	QMetaObject::invokeMethod(_le_r12, "setText", Qt::QueuedConnection,
							  Q_ARG(QString, "0x" + QString::number(state->r12, 16)));
	QMetaObject::invokeMethod(_le_r13, "setText", Qt::QueuedConnection,
							  Q_ARG(QString, "0x" + QString::number(state->sp, 16)));
	QMetaObject::invokeMethod(_le_r14, "setText", Qt::QueuedConnection,
							  Q_ARG(QString, "0x" + QString::number(state->lr, 16)));
	QMetaObject::invokeMethod(_le_r15, "setText", Qt::QueuedConnection,
							  Q_ARG(QString, "0x" + QString::number(state->ip, 16)));
	QMetaObject::invokeMethod(_le_cpsr, "setText", Qt::QueuedConnection,
							  Q_ARG(QString, "0x" + QString::number(state->cpsr, 16)));
	QMetaObject::invokeMethod(_le_sp_svc, "setText", Qt::QueuedConnection,
							  Q_ARG(QString, "0x" + QString::number(state->mode[Cpu_state_modes::Mode_state::SVC].sp, 16)));
	QMetaObject::invokeMethod(_le_lr_svc, "setText", Qt::QueuedConnection,
							  Q_ARG(QString, "0x" + QString::number(state->mode[Cpu_state_modes::Mode_state::SVC].lr, 16)));
	QMetaObject::invokeMethod(_le_spsr_svc, "setText", Qt::QueuedConnection,
							  Q_ARG(QString, "0x" + QString::number(state->mode[Cpu_state_modes::Mode_state::SVC].spsr, 16)));
	QMetaObject::invokeMethod(_le_sp_abt, "setText", Qt::QueuedConnection,
							  Q_ARG(QString, "0x" + QString::number(state->mode[Cpu_state_modes::Mode_state::ABORT].sp, 16)));
	QMetaObject::invokeMethod(_le_lr_abt, "setText", Qt::QueuedConnection,
							  Q_ARG(QString, "0x" + QString::number(state->mode[Cpu_state_modes::Mode_state::ABORT].lr, 16)));
	QMetaObject::invokeMethod(_le_spsr_abt, "setText", Qt::QueuedConnection,
							  Q_ARG(QString, "0x" + QString::number(state->mode[Cpu_state_modes::Mode_state::ABORT].spsr, 16)));
	QMetaObject::invokeMethod(_le_sp_irq, "setText", Qt::QueuedConnection,
							  Q_ARG(QString, "0x" + QString::number(state->mode[Cpu_state_modes::Mode_state::IRQ].sp, 16)));
	QMetaObject::invokeMethod(_le_lr_irq, "setText", Qt::QueuedConnection,
							  Q_ARG(QString, "0x" + QString::number(state->mode[Cpu_state_modes::Mode_state::IRQ].lr, 16)));
	QMetaObject::invokeMethod(_le_spsr_irq, "setText", Qt::QueuedConnection,
							  Q_ARG(QString, "0x" + QString::number(state->mode[Cpu_state_modes::Mode_state::IRQ].spsr, 16)));

	switch (state->cpu_exception) {
	case Genode::Cpu_state::SUPERVISOR_CALL:
		QMetaObject::invokeMethod(_l_exc_type, "setText", Qt::QueuedConnection,
								  Q_ARG(QString, "Hypervisor Call"));
		break;
	case Genode::Cpu_state::DATA_ABORT:
		QMetaObject::invokeMethod(_l_exc_type, "setText", Qt::QueuedConnection,
								  Q_ARG(QString, "<b><FONT COLOR='#a00000'>Data Abort!</b>"));
		break;
	case Genode::Cpu_state::FAST_INTERRUPT_REQUEST:
		QMetaObject::invokeMethod(_l_exc_type, "setText", Qt::QueuedConnection,
								  Q_ARG(QString, "Fast Interrupt"));
	};
}


Main_window::Main_window()
{
	_fb_widget->setParent(this);
	_reg_widget->setParent(this);
	_control_bar->setParent(this);

	/* create local services */
	enum { STACK_SIZE = 2*sizeof(addr_t)*1024 };
	static Cap_connection cap;
	static Rpc_entrypoint gui_ep(&cap, STACK_SIZE, "vmm_gui_ep");
	static Vmm_gui::Root gui_root(&gui_ep, env()->heap(), _fb_widget->my_widget(),
	                              *_control_bar, *_reg_widget);
	env()->parent()->announce(gui_ep.manage(&gui_root));

	_layout->addStretch();
	_layout->addWidget(_fb_widget);
	_layout->addStretch();
	_layout->addWidget(_reg_widget);
	_layout->addStretch();
	_layout->addWidget(_control_bar);
	_layout->setContentsMargins(5, 5, 5, 5);
}


Background_window::Background_window()
{
	/* look for dynamic linker */
	try {
		static Rom_connection ldso_rom("ld.lib.so");
		Process::dynamic_linker(ldso_rom.dataspace());
	} catch (...) {
		PERR("ld.lib.so not found");
	}

	_main->setParent(this);
	_layout->addWidget(_main);
}
