/*
 * \brief  Genode-specific timer backend
 * \author Christian Prochaska
 * \date   2012-03-13
 *
 * based on the dummy SDL timer
 */

/*
 * Copyright (C) 2012-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#include <timer_session/connection.h>

extern "C" {

#include <unistd.h>

#include "SDL_config.h"

#include "SDL_timer.h"
#include "SDL_timer_c.h"


static unsigned long start_ms = 0;

static Timer::Connection _timer;


void SDL_StartTicks(void)
{
	start_ms = _timer.elapsed_ms();
}


Uint32 SDL_GetTicks (void)
{
	return _timer.elapsed_ms() - start_ms;
}


void SDL_Delay (Uint32 ms)
{
	_timer.msleep(ms);
}


#include "SDL_thread.h"


/* Data to handle a single periodic alarm */
static int timer_alive = 0;
static SDL_Thread *timer = NULL;

static int RunTimer(void *unused)
{
	while ( timer_alive ) {
		if ( SDL_timer_running ) {
			SDL_ThreadedTimerCheck();
		}
		SDL_Delay(1);
	}
	return(0);
}


/* This is only called if the event thread is not running */
int SDL_SYS_TimerInit(void)
{
	timer_alive = 1;
	timer = SDL_CreateThread(RunTimer, NULL);
	if ( timer == NULL )
		return(-1);
	return(SDL_SetTimerThreaded(1));
}


void SDL_SYS_TimerQuit(void)
{
	timer_alive = 0;
	if ( timer ) {
		SDL_WaitThread(timer, NULL);
		timer = NULL;
	}
}


int SDL_SYS_StartTimer(void)
{
	SDL_SetError("Internal logic error: threaded timer in use");
	return(-1);
}


void SDL_SYS_StopTimer(void)
{
	return;
}

}
