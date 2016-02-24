// license:BSD-3-Clause
// copyright-holders:Olivier Galibert, R. Belmont
//============================================================
//
//  sdlsync.c - SDL core synchronization functions
//
//  SDLMAME by Olivier Galibert and R. Belmont
//
//============================================================

#include "sdlinc.h"

// standard C headers
#include <unistd.h>
#include <sys/types.h>
#include <signal.h>

// MAME headers
#include "osdcore.h"
#include "osdsync.h"

#include "eminline.h"

#define VERBOSE     (0)

#if VERBOSE
#define LOG( x ) do { printf x; printf("\n"); } while (0)
#else
#define LOG( x )
#endif
struct hidden_mutex_t {
	SDL_mutex *         id;
	volatile INT32      locked;
	volatile INT32      threadid;
};

struct osd_event {
	SDL_mutex *         mutex;
	SDL_cond *          cond;
	volatile INT32      autoreset;
	volatile INT32      signalled;
};

//============================================================
//  TYPE DEFINITIONS
//============================================================

struct osd_thread {
	SDL_Thread *        thread;
	osd_thread_callback callback;
	void *param;
};


//============================================================
//  osd_event_alloc
//============================================================

osd_event *osd_event_alloc(int manualreset, int initialstate)
{
	osd_event *ev;

	ev = (osd_event *)calloc(1, sizeof(osd_event));
	if (ev == NULL)
		return NULL;

	ev->mutex = SDL_CreateMutex();
	ev->cond = SDL_CreateCond();
	ev->signalled = initialstate;
	ev->autoreset = !manualreset;

	return ev;
}

//============================================================
//  osd_event_free
//============================================================

void osd_event_free(osd_event *event)
{
	SDL_DestroyMutex(event->mutex);
	SDL_DestroyCond(event->cond);
	free(event);
}

//============================================================
//  osd_event_set
//============================================================

void osd_event_set(osd_event *event)
{
	LOG(("osd_event_set"));
	SDL_mutexP(event->mutex);
	if (event->signalled == FALSE)
	{
		event->signalled = TRUE;
		if (event->autoreset)
			SDL_CondSignal(event->cond);
		else
			SDL_CondBroadcast(event->cond);
	}
	SDL_mutexV(event->mutex);
}

//============================================================
//  osd_event_reset
//============================================================

void osd_event_reset(osd_event *event)
{
	LOG(("osd_event_reset"));
	SDL_mutexP(event->mutex);
	event->signalled = FALSE;
	SDL_mutexV(event->mutex);
}

//============================================================
//  osd_event_wait
//============================================================

int osd_event_wait(osd_event *event, osd_ticks_t timeout)
{
	LOG(("osd_event_wait"));
	if (timeout == OSD_EVENT_WAIT_INFINITE)
		timeout = osd_ticks_per_second() * (osd_ticks_t)10000;
	SDL_mutexP(event->mutex);
	if (!timeout)
	{
		if (!event->signalled)
		{
			SDL_mutexV(event->mutex);
				return FALSE;
		}
	}
	else
	{
		if (!event->signalled)
		{
			UINT64 msec = (timeout * 1000) / osd_ticks_per_second();

			do {
				int ret = SDL_CondWaitTimeout(event->cond, event->mutex, msec);
				if ( ret == SDL_MUTEX_TIMEDOUT )
				{
					if (!event->signalled)
					{
						SDL_mutexV(event->mutex);
						return FALSE;
					}
					else
						break;
				}
				if (ret == 0)
					break;
				printf("Error %d while waiting for pthread_cond_timedwait:  %s\n", ret, strerror(ret));
			} while (TRUE);
		}
	}

	if (event->autoreset)
		event->signalled = 0;

	SDL_mutexV(event->mutex);

	return TRUE;
}

//============================================================
//  osd_thread_create
//============================================================

static int worker_thread_entry(void *param)
{
	osd_thread *thread = (osd_thread *) param;
	void *res;

	res = thread->callback(thread->param);
#ifdef PTR64
	return (int) (INT64) res;
#else
	return (int) res;
#endif
}

osd_thread *osd_thread_create(osd_thread_callback callback, void *cbparam)
{
	osd_thread *thread;

	thread = (osd_thread *)calloc(1, sizeof(osd_thread));
	if (thread == NULL)
		return NULL;
	thread->callback = callback;
	thread->param = cbparam;
	thread->thread = SDL_CreateThread(worker_thread_entry, "Thread", thread);
	if ( thread->thread == NULL )
	{
		free(thread);
		return NULL;
	}
	return thread;
}

//============================================================
//  osd_thread_adjust_priority
//============================================================

int osd_thread_adjust_priority(osd_thread *thread, int adjust)
{
	return TRUE;
}

//============================================================
//  osd_thread_cpu_affinity
//============================================================

int osd_thread_cpu_affinity(osd_thread *thread, UINT32 mask)
{
	return TRUE;
}

//============================================================
//  osd_thread_wait_free
//============================================================

void osd_thread_wait_free(osd_thread *thread)
{
	int status;
	SDL_WaitThread(thread->thread, &status);
	free(thread);
}
