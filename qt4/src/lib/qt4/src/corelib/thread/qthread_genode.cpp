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

#include "qthread.h"

#include "qplatformdefs.h"

#include <private/qcoreapplication_p.h>
#include <private/qeventdispatcher_genode_p.h>

#include "qthreadstorage.h"

#include "qthread_p.h"

#include "qdebug.h"

QT_BEGIN_NAMESPACE

#ifndef QT_NO_THREAD

#include <base/env.h>
#include <timer_session/connection.h>

QHash<Qt::HANDLE, struct QThreadPrivate::tls_struct> QThreadPrivate::tls;

QThreadData *QThreadData::current()
{
	// create an entry for the thread-specific data of the current thread
	if (!QThreadPrivate::tls.contains(QThread::currentThreadId())) {
		QThreadPrivate::tls.insert(QThread::currentThreadId(),
                               (struct QThreadPrivate::tls_struct){0, true});
	}
	
	QThreadData *data = QThreadPrivate::tls.value(QThread::currentThreadId()).data;
	if (!data) {
		void *a;
		if (QInternal::activateCallbacks(QInternal::AdoptCurrentThread, &a)) {
				QThread *adopted = static_cast<QThread*>(a);
				Q_ASSERT(adopted);
				data = QThreadData::get2(adopted);
			
				struct QThreadPrivate::tls_struct tls_elem = 
					 QThreadPrivate::tls.value(QThread::currentThreadId());
				tls_elem.data = data;
				QThreadPrivate::tls.insert(QThread::currentThreadId(), tls_elem);
			
				adopted->d_func()->running = true;
				adopted->d_func()->finished = false;
				static_cast<QAdoptedThread *>(adopted)->init();
		} else {
				data = new QThreadData;
			
				struct QThreadPrivate::tls_struct tls_elem = 
					 QThreadPrivate::tls.value(QThread::currentThreadId());
				tls_elem.data = data;
				QThreadPrivate::tls.insert(QThread::currentThreadId(), tls_elem);

				data->thread = new QAdoptedThread(data);
				data->deref();
		}
    if (!QCoreApplicationPrivate::theMainThread)
        QCoreApplicationPrivate::theMainThread = data->thread;
	}
	
	return data;
}


void QAdoptedThread::init()
{
    d_func()->thread_id = QThread::currentThreadId();
}

/*
   QThreadPrivate
*/

#if defined(Q_C_CALLBACKS)
extern "C" {
#endif

typedef void*(*QtThreadCallback)(void*);

#if defined(Q_C_CALLBACKS)
}
#endif

#endif // QT_NO_THREAD

void QThreadPrivate::createEventDispatcher(QThreadData *data)
{
    data->eventDispatcher = new QEventDispatcherGenode;
    data->eventDispatcher->startingUp();
}

#ifndef QT_NO_THREAD

void QThreadPrivate::start(QThread *thr)
{
		thr->d_func()->thread_id = QThread::currentThreadId();
	
		QThread::setTerminationEnabled(false);
		
    QThreadData *data = QThreadData::get2(thr);

		// create an entry for the thread-specific data of the current thread
		if (!QThreadPrivate::tls.contains(QThread::currentThreadId())) {
			QThreadPrivate::tls.insert(QThread::currentThreadId(),
																 (struct QThreadPrivate::tls_struct){0, true});
		}
	
		struct QThreadPrivate::tls_struct tls_elem = 
			 QThreadPrivate::tls.value(QThread::currentThreadId());
		tls_elem.data = data;
		QThreadPrivate::tls.insert(QThread::currentThreadId(), tls_elem);

    data->ref();
    data->quitNow = false;

    // ### TODO: allow the user to create a custom event dispatcher
    createEventDispatcher(data);

    emit thr->started();
		
		QThread::setTerminationEnabled(true);
		
    thr->run();
}

void QThreadPrivate::finish(QThread *thr)
{
    QThreadPrivate *d = thr->d_func();
    QMutexLocker locker(&d->mutex);

    d->priority = QThread::InheritPriority;
    d->running = false;
    d->finished = true;
    if (d->terminated)
        emit thr->terminated();
    d->terminated = false;
    emit thr->finished();

    if (d->data->eventDispatcher) {
        d->data->eventDispatcher->closingDown();
        QAbstractEventDispatcher *eventDispatcher = d->data->eventDispatcher;
        d->data->eventDispatcher = 0;
        delete eventDispatcher;
    }

    void *data = &d->data->tls;
    QThreadStorageData::finish((void **)data);

    QThreadPrivate::tls.remove(QThread::currentThreadId());

    d->thread_id = 0;
    d->thread_done.wakeAll();
}




/**************************************************************************
 ** QThread
 *************************************************************************/

/*!
    Returns the thread handle of the currently executing thread.

    \warning The handle returned by this function is used for internal
    purposes and should not be used in any application code. On
    Windows, the returned value is a pseudo-handle for the current
    thread that cannot be used for numerical comparison.
*/
Qt::HANDLE QThread::currentThreadId()
{
		return (Qt::HANDLE)QThreadPrivate::Genode_thread::myself();
}

/*!
    Returns the ideal number of threads that can be run on the system. This is done querying
    the number of processor cores, both real and logical, in the system. This function returns -1
    if the number of processor cores could not be detected.
*/
int QThread::idealThreadCount()
{
		return -1;
}

/*!
    Forces the current thread to sleep for \a secs seconds.

    \sa msleep(), usleep()
*/
void QThread::sleep(unsigned long secs)
{
	static Timer::Connection timer;
	timer.msleep(secs * 1000);
}

/*!
    Causes the current thread to sleep for \a msecs milliseconds.

    \sa sleep(), usleep()
*/
void QThread::msleep(unsigned long msecs)
{
	static Timer::Connection timer;
	timer.msleep(msecs);
}

/*!
    Causes the current thread to sleep for \a usecs microseconds.

    \sa sleep(), msleep()
*/
void QThread::usleep(unsigned long usecs)
{
	static Timer::Connection timer;
	timer.msleep(usecs / 1000);
}

/*!
    Begins execution of the thread by calling run(), which should be
    reimplemented in a QThread subclass to contain your code. The
    operating system will schedule the thread according to the \a
    priority parameter. If the thread is already running, this
    function does nothing.

    \sa run(), terminate()
*/
void QThread::start(Priority priority)
{
    Q_D(QThread);
    QMutexLocker locker(&d->mutex);
    if (d->running)
        return;

    d->running = true;
    d->finished = false;
    d->terminated = false;

    d->priority = priority;

		d->genode_thread = new QThreadPrivate::Genode_thread(this);

		if (d->genode_thread) {

			/* set stacksize */
	    if (d->stackSize > 0) {
	      if (!d->genode_thread->set_stack_size(d->stackSize)) {
	          qWarning("QThread::start: Thread stack size error");
	
	          // we failed to set the stacksize, and as the documentation states,
	          // the thread will fail to run...
	          d->running = false;
	          d->finished = false;
	          return;
	      }
	    }

			d->genode_thread->start();

		} else {
      qWarning("QThread::start: Thread creation error");

      d->running = false;
      d->finished = false;
      d->thread_id = 0;
    }
}

/*!
    Terminates the execution of the thread. The thread may or may not
    be terminated immediately, depending on the operating systems
    scheduling policies. Use QThread::wait() after terminate() for
    synchronous termination.

    When the thread is terminated, all threads waiting for the thread
    to finish will be woken up.

    \warning This function is dangerous and its use is discouraged.
    The thread can be terminate at any point in its code path.
    Threads can be terminated while modifying data. There is no
    chance for the thread to cleanup after itself, unlock any held
    mutexes, etc. In short, use this function only if absolutely
    necessary.

    Termination can be explicitly enabled or disabled by calling
    QThread::setTerminationEnabled(). Calling this function while
    termination is disabled results in the termination being
    deferred, until termination is re-enabled. See the documentation
    of QThread::setTerminationEnabled() for more information.

    \sa setTerminationEnabled()
*/
void QThread::terminate()
{
    Q_D(QThread);
    QMutexLocker locker(&d->mutex);

    if (QThreadPrivate::tls.value(QThread::currentThreadId()).termination_enabled) {

        if (d->genode_thread) {
            delete d->genode_thread;
            d->genode_thread = 0;
        }

        d->terminated = true;
        d->running = false;
    }
}


static inline void join_and_delete_genode_thread(QThreadPrivate *d)
{
    if (d->genode_thread) {
        d->genode_thread->join();
        delete d->genode_thread;
        d->genode_thread = 0;
    }
}


/*!
    Blocks the thread until either of these conditions is met:

    \list
    \o The thread associated with this QThread object has finished
       execution (i.e. when it returns from \l{run()}). This function
       will return true if the thread has finished. It also returns
       true if the thread has not been started yet.
    \o \a time milliseconds has elapsed. If \a time is ULONG_MAX (the
        default), then the wait will never timeout (the thread must
        return from \l{run()}). This function will return false if the
        wait timed out.
    \endlist

    This provides similar functionality to the POSIX \c
    pthread_join() function.

    \sa sleep(), terminate()
*/
bool QThread::wait(unsigned long time)
{
    Q_D(QThread);
    QMutexLocker locker(&d->mutex);

    if (d->thread_id == QThread::currentThreadId()) {
        qWarning("QThread::wait: Thread tried to wait on itself");
        return false;
    }

    if (d->finished || !d->running) {
        join_and_delete_genode_thread(d);
        return true;
    }

    while (d->running) {
        if (!d->thread_done.wait(locker.mutex(), time))
            return false;
    }

    join_and_delete_genode_thread(d);

    return true;
}

/*!
    Enables or disables termination of the current thread based on the
    \a enabled parameter. The thread must have been started by
    QThread.

    When \a enabled is false, termination is disabled.  Future calls
    to QThread::terminate() will return immediately without effect.
    Instead, the termination is deferred until termination is enabled.

    When \a enabled is true, termination is enabled.  Future calls to
    QThread::terminate() will terminate the thread normally.  If
    termination has been deferred (i.e. QThread::terminate() was
    called with termination disabled), this function will terminate
    the calling thread \e immediately.  Note that this function will
    not return in this case.

    \sa terminate()
*/
void QThread::setTerminationEnabled(bool enabled)
{
    Q_ASSERT_X(currentThread() != 0, "QThread::setTerminationEnabled()",
               "Current thread was not started with QThread.");
	
		struct QThreadPrivate::tls_struct tls_elem = 
			 QThreadPrivate::tls.value(QThread::currentThreadId());
		tls_elem.termination_enabled = enabled;
		QThreadPrivate::tls.insert(QThread::currentThreadId(), tls_elem);
}

void QThread::setPriority(Priority priority)
{
    Q_D(QThread);
    QMutexLocker locker(&d->mutex);
    if (!d->running) {
        qWarning("QThread::setPriority: Cannot set priority, thread is not running");
        return;
    }

    d->priority = priority;
}

#endif // QT_NO_THREAD

QT_END_NAMESPACE

