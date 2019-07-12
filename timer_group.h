/* $Id$
 * $URL$
 *
 * generic grouping of widget timers that have been set to the same
 * update interval, thus allowing synchronized updates
 *
 * Copyright (C) 2010 Martin Zuther <code@mzuther.de>
 * Copyright (C) 2010 The LCD4Linux Team <lcd4linux-devel@users.sourceforge.net>
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


#ifndef _TIMER_GROUP_H_
#define _TIMER_GROUP_H_

/* structure for storing all relevant timer data of a single widget */
typedef struct TIMER_GROUP_WIDGET {
    /* pointer to function of type void func(void *data) that will be
       called when the timer is processed; it will also be used to
       identify a specific widget */
    void (*callback) (void *data);

    /* pointer to data which will be passed to the callback function;
       it will also be used to identify a specific widget */
    void *data;

    /* specifies the timer's triggering interval in milliseconds; it
       will also be used to identify a specific widget */
    int interval;

    /* specifies whether the timer should trigger indefinitely until
       it is deleted (value of 0) or only once (all other values) */
    int one_shot;

    /* marks timer as being active (so it will get processed) or
       inactive (which means the timer has been deleted and its
       allocated memory may be re-used) */
    int active;
} TIMER_GROUP_WIDGET;

void timer_process_group(void *data);

void timer_exit_group(void);

int timer_add_widget(void (*callback) (void *data), void *data, const int interval, const int one_shot);

int timer_remove_widget(void (*callback) (void *data), void *data);

#endif
