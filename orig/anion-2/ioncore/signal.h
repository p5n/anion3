/*
 * ion/ioncore/signal.h
 *
 * Copyright (c) Tuomo Valkonen 1999-2004. 
 *
 * Ion is free software; you can redistribute it and/or modify it under
 * the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 2.1 of the License, or
 * (at your option) any later version.
 */

#ifndef ION_IONCORE_SIGNAL_H
#define ION_IONCORE_SIGNAL_H

#include <sys/time.h>
#include <time.h>
#include <unistd.h>
#include "common.h"

INTRSTRUCT(WTimer);

DECLSTRUCT(WTimer){
    struct timeval when;
    void (*handler)();
    WTimer *next;
    WWatch paramwatch;
};

#define INIT_TIMER(FUN) {{0, 0}, FUN, NULL, WWATCH_INIT}

extern void check_signals();
extern void trap_signals();
extern void set_timer(WTimer *timer, uint msecs);
extern void set_timer_param(WTimer *timer, uint msecs, WObj *param);
extern void reset_timer(WTimer *timer);
extern bool timer_is_set(WTimer *timer);

#endif /* ION_IONCORE_SIGNAL_H */
