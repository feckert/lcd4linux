/* $Id$
 * $URL$
 *
 * Copyright (C) 2015 Marcus Menzel <codingmax@gmx-topmail.de>
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


#ifndef _ACTION_H_
#define _ACTION_H_


#include "property.h"

typedef struct Action Action;

typedef struct Action {
    char *name;
    PROPERTY *property;
    Action *next;
} Action;

void action_free_all(Action * firstAction);
void action_init_default(const char *confPrefix, Action ** firstActionPtr);
void action_init_multiple(const char *prefix, const char **names, Action ** firstActionPtr);
int action_trigger(Action * firstAction, const char *name);



#endif
