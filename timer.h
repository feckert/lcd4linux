/* $Id$
 * $URL$
 *
 * generic timer handling
 *
 * Copyright (C) 2003, 2004 Michael Reinelt <michael@reinelt.co.at>
 * Copyright (C) 2004 The LCD4Linux Team <lcd4linux-devel@users.sourceforge.net>
 *
 * This file is part of LCD4Linux.
 *
 * LCD4Linux is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 *
 * LCD4Linux is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 */


#ifndef _TIMER_H_
#define _TIMER_H_

#define TIMER_ACTIVE  1
#define TIMER_INACTIVE  0

#include <time.h>

/* structure for storing all relevant data of a single timer */
typedef struct TIMER {
    /* pointer to function of type void func(void *data) that will be
       called when the timer is processed; it will also be used to
       identify a specific timer */
    void (*callback) (void *data);

    /* pointer to data which will be passed to the callback function;
       it will also be used to identify a specific timer */
    void *data;

    /* struct to hold the time (in seconds and milliseconds since the
       Epoch) when the timer will be processed for the next time */
    struct timeval when;

    /* specifies the timer's triggering interval in milliseconds */
    int interval;

    /* specifies whether the timer should trigger indefinitely until
       it is deleted (value of 0) or only once (all other values) */
    int one_shot;

    /* marks timer as being active (so it will get processed) or
       inactive (which means the timer has been deleted and its
       allocated memory may be re-used) */
    int active;
} TIMER;

int timer_add(void (*callback) (void *data), void *data, const int interval, const int one_shot);

int timer_add_late(void (*callback) (void *data), void *data, const int interval, const int one_shot);

int timer_process(struct timespec *delay);

int timer_remove(void (*callback) (void *data), void *data);

void timer_exit(void);

#endif
