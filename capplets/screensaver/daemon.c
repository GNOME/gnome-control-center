/* -*- mode: c; style: linux -*- */

/* daemon.c
 * Copyright (C) 2000 Helix Code, Inc.
 *
 * Written by Bradford Hovinen (hovinen@helixcode.com)
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
 * 02111-1307, USA.
 */

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <stdlib.h>
#include <gnome.h>

#include "daemon.h"

void 
start_xscreensaver (void) 
{
	gnome_execute_shell (".", "xscreensaver -no-splash &");
}

void 
stop_xscreensaver (void) 
{
	gnome_execute_shell (".", "xscreensaver-command -exit");
}

void 
restart_xscreensaver (void)
{
	gnome_execute_shell (".", "xscreensaver-command -restart &");
}

void 
setup_dpms (Preferences *prefs) 
{
	char *command_line;
	guint standby_time, suspend_time, power_down_time;

	if (!prefs->power_management) return;

	standby_time = prefs->standby_time ? 
		(prefs->timeout + prefs->standby_time) * 60 : 0;
	suspend_time = prefs->suspend_time ? 
		(prefs->timeout + prefs->suspend_time) * 60 : 0;
	power_down_time = prefs->power_down_time ? 
		(prefs->timeout + prefs->power_down_time) * 60 : 0;

	/* Sanitize values */
	if (standby_time > suspend_time) standby_time = 0;
	if (standby_time > power_down_time) standby_time = 0;
	if (suspend_time > power_down_time) suspend_time = 0;

	command_line = g_strdup_printf ("xset dpms %u %u %u",
					standby_time,
					suspend_time,
					power_down_time);
	gnome_execute_shell (".", command_line);
	g_free (command_line);
}
