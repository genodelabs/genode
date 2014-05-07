/**************************************************************************** 
** 
** Copyright (C) 2009 Nokia Corporation and/or its subsidiary(-ies). 
** Contact: Qt Software Information (qt-info@nokia.com) 
** 
** This file is part of the QtCore module of the Qt Toolkit. 
** 
** $QT_BEGIN_LICENSE:LGPL$ 
** Commercial Usage 
** Licensees holding valid Qt Commercial licenses may use this file in 
** accordance with the Qt Commercial License Agreement provided with the 
** Software or, alternatively, in accordance with the terms contained in 
** a written agreement between you and Nokia. 
** 
** GNU Lesser General Public License Usage 
** Alternatively, this file may be used under the terms of the GNU Lesser 
** General Public License version 2.1 as published by the Free Software 
** Foundation and appearing in the file LICENSE.LGPL included in the 
** packaging of this file.  Please review the following information to 
** ensure the GNU Lesser General Public License version 2.1 requirements 
** will be met: http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html. 
** 
** In addition, as a special exception, Nokia gives you certain 
** additional rights. These rights are described in the Nokia Qt LGPL 
** Exception version 1.0, included in the file LGPL_EXCEPTION.txt in this 
** package. 
** 
** GNU General Public License Usage 
** Alternatively, this file may be used under the terms of the GNU 
** General Public License version 3.0 as published by the Free Software 
** Foundation and appearing in the file LICENSE.GPL included in the 
** packaging of this file.  Please review the following information to 
** ensure the GNU General Public License version 3.0 requirements will be 
** met: http://www.gnu.org/copyleft/gpl.html. 
** 
** If you are unsure which license is appropriate for your use, please 
** contact the sales department at qt-sales@nokia.com. 
** $QT_END_LICENSE$ 
** 
****************************************************************************/

#include "qplatformdefs.h"
#include "qmutex.h"
#include "qstring.h"

#ifndef QT_NO_THREAD

#include "qmutex_p.h"
#include "qthread.h"

#include <base/env.h>
#include <timer_session/client.h>

QMutexPrivate::QMutexPrivate(QMutex::RecursionMode mode)
    : QMutexData(mode), maximumSpinTime(MaximumSpinTimeThreshold), averageWaitTime(0), owner(0), count(0)
{
}

QMutexPrivate::~QMutexPrivate()
{
}

bool QMutexPrivate::wait(int timeout)
{
	bool result;

    if (contenders.fetchAndAddAcquire(1) == 0) {
        // lock acquired without waiting
        return true;
    }

	if (timeout == 0) {
		result = false; /* timed out */
	} else if (timeout < 0) {
		sem.down();
		result = true; /* woken up */
	} else {
		try {
			sem.down(timeout);
			result = true;
		} catch(Genode::Timeout_exception) {
			result = false;
		}
	}

	contenders.deref();

	return result;
}

void QMutexPrivate::wakeUp()
{
	sem.up();
}

#endif // QT_NO_THREAD
