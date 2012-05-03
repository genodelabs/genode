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

#ifndef QT_NO_THREAD

#include "qwaitcondition.h"
#include "qmutex.h"
#include "qatomic.h"
#include "qmutex_p.h"

#include <os/timed_semaphore.h>

struct QWaitConditionPrivate {
    Genode::Lock mutex;
    Genode::Timed_semaphore sem;
};


/*!
    \class QWaitCondition
    \brief The QWaitCondition class provides a condition variable for
    synchronizing threads.

    \threadsafe

    \ingroup thread
    \ingroup environment

    QWaitCondition allows a thread to tell other threads that some
    sort of condition has been met. One or many threads can block
    waiting for a QWaitCondition to set a condition with wakeOne() or
    wakeAll(). Use wakeOne() to wake one randomly selected condition or
    wakeAll() to wake them all.

    For example, let's suppose that we have three tasks that should
    be performed whenever the user presses a key. Each task could be
    split into a thread, each of which would have a
    \l{QThread::run()}{run()} body like this:

    \code
        forever {
            mutex.lock();
            keyPressed.wait(&mutex);
            do_something();
            mutex.unlock();
        }
    \endcode

    Here, the \c keyPressed variable is a global variable of type
    QWaitCondition.

    A fourth thread would read key presses and wake the other three
    threads up every time it receives one, like this:

    \code
        forever {
            getchar();
            keyPressed.wakeAll();
        }
    \endcode

    The order in which the three threads are woken up is undefined.
    Also, if some of the threads are still in \c do_something() when
    the key is pressed, they won't be woken up (since they're not
    waiting on the condition variable) and so the task will not be
    performed for that key press. This issue can be solved using a
    counter and a QMutex to guard it. For example, here's the new
    code for the worker threads:

    \code
        forever {
            mutex.lock();
            keyPressed.wait(&mutex);
            ++count;
            mutex.unlock();

            do_something();

            mutex.lock();
            --count;
            mutex.unlock();
        }
    \endcode

    Here's the code for the fourth thread:

    \code
        forever {
            getchar();

            mutex.lock();
            // Sleep until there are no busy worker threads
            while (count > 0) {
                mutex.unlock();
                sleep(1);
                mutex.lock();
            }
            keyPressed.wakeAll();
            mutex.unlock();
        }
    \endcode

    The mutex is necessary because the results of two threads
    attempting to change the value of the same variable
    simultaneously are unpredictable.

    Wait conditions are a powerful thread synchronization primitive.
    The \l{threads/waitconditions}{Wait Conditions} example shows how
    to use QWaitCondition as an alternative to QSemaphore for
    controlling access to a circular buffer shared by a producer
    thread and a consumer thread.

    \sa QMutex, QSemaphore, QThread, {Wait Conditions Example}
*/

/*!
    Constructs a new wait condition object.
*/
QWaitCondition::QWaitCondition()
{
    d = new QWaitConditionPrivate;
}


/*!
    Destroys the wait condition object.
*/
QWaitCondition::~QWaitCondition()
{
    delete d;
}

/*!
    Wakes one thread waiting on the wait condition. The thread that
    is woken up depends on the operating system's scheduling
    policies, and cannot be controlled or predicted.

    If you want to wake up a specific thread, the solution is
    typically to use different wait conditions and have different
    threads wait on different conditions.

    \sa wakeAll()
*/
void QWaitCondition::wakeOne()
{
    Genode::Lock::Guard lock_guard(d->mutex);

    if (d->sem.cnt() < 0) {
      d->sem.up();
    }
}

/*!
    Wakes all threads waiting on the wait condition. The order in
    which the threads are woken up depends on the operating system's
    scheduling policies and cannot be controlled or predicted.

    \sa wakeOne()
 */
void QWaitCondition::wakeAll()
{
  Genode::Lock::Guard lock_guard(d->mutex);

  while (d->sem.cnt() < 0) {
    d->sem.up();
  }
}

/*!
    Releases the locked \a mutex and wait on the wait condition.
    The \a mutex must be initially locked by the calling thread. If
    \a mutex is not in a locked state, this function returns
    immediately. If \a mutex is a recursive mutex, this function
    returns immediately. The \a mutex will be unlocked, and the
    calling thread will block until either of these conditions is
    met:

    \list
    \o Another thread signals it using wakeOne() or wakeAll(). This
       function will return true in this case.
    \o \a time milliseconds has elapsed. If \a time is \c ULONG_MAX
       (the default), then the wait will never timeout (the event
       must be signalled). This function will return false if the
       wait timed out.
    \endlist

    The mutex will be returned to the same locked state. This
    function is provided to allow the atomic transition from the
    locked state to the wait state.

    \sa wakeOne(), wakeAll()
 */
bool QWaitCondition::wait(QMutex *mutex, unsigned long time)
{
    bool result;

    if (! mutex)
        return false;

    if (mutex->d->recursive) {
        qWarning("QWaitCondition: cannot wait on recursive mutexes");
        return false;
    }
    
    mutex->unlock();
    
    if (time == ULONG_MAX) {
    	/* Timeout: never */
    	d->sem.down();
    	result = true;
    } else {
    	try {
    	  d->sem.down((Genode::Alarm::Time) time);
          result = true;
    	} catch (Genode::Timeout_exception) {
          result = false;
    	}
    }
    
    mutex->lock();

    return result;
}
#endif // QT_NO_THREAD
