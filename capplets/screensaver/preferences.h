/* -*- mode: c; style: linux -*- */

/* preferences.h
 * Copyright (C) 2000 Helix Code, Inc.
 *
 * Written by Bradford Hovinen <hovinen@helixcode.com>
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

#ifndef __PREFERENCES_H
#define __PREFERENCES_H

#include <sys/time.h>
#include <gtk/gtk.h>
#include <libxml/tree.h>

enum _SelectionMode {
	SM_DISABLE_SCREENSAVER,
	SM_BLANK_SCREEN,
	SM_ONE_SCREENSAVER_ONLY,
	SM_CHOOSE_FROM_LIST,
	SM_CHOOSE_RANDOMLY
};

typedef enum _SelectionMode SelectionMode;

#define SCREENSAVER(obj) ((Screensaver *) obj)

typedef struct _Screensaver Screensaver;

struct _Screensaver 
{
	guint id;
	gchar *name;
	gchar *filename;
	gchar *label;
	gchar *description;
	gchar *command_line;
	gchar *compat_command_line;
	gchar *visual;
	gboolean enabled;
	GList *link;

	gchar *fakepreview;
	GList *fakes;
};

struct _Preferences 
{
	time_t    init_file_date;

	gboolean  verbose;
	gboolean  lock;

	gboolean  fade;
	gboolean  unfade;
	time_t    fade_seconds;
	guint     fade_ticks;

	gboolean  install_colormap;

	gint      nice;

	gdouble   timeout;
	gdouble   lock_timeout;
	gdouble   cycle;

	gchar    *programs_list;  /* Programs resource, in JWZ's format */

	SelectionMode selection_mode;

	GList    *screensavers;
	GHashTable *savers_hash;

	gint  frozen;         /* TRUE if we shouldn't reflect
			       * preference changes in capplet */
	GTree *config_db;     /* key-value database of config options */

	GList *invalidsavers;

	/* Settings that are not stored in .xscreensaver ... */

	gboolean  power_management;
	time_t    standby_time;
	time_t    suspend_time;
	time_t    power_down_time;
};

typedef struct _Preferences Preferences;

Preferences *preferences_new (void);
Preferences *preferences_clone (Preferences *prefs);
void preferences_destroy (Preferences *prefs);

void preferences_load (Preferences *prefs);
void preferences_save (Preferences *prefs);

Preferences *preferences_read_xml (xmlDocPtr xml_doc);
xmlDocPtr preferences_write_xml (Preferences *prefs);

Screensaver *screensaver_new (void);
Screensaver *screensaver_new_from_file (const gchar *filename);
void screensaver_destroy (Screensaver *saver);

GList *screensaver_add (Screensaver *saver, GList *screensavers);
GList *screensaver_remove (Screensaver *saver, GList *screensavers);

Screensaver *screensaver_read_xml (xmlNodePtr node);
xmlNodePtr screensaver_write_xml (Screensaver *saver);

char *screensaver_get_desc (Screensaver *saver);

char *screensaver_get_label (gchar *name);

#endif /* __PREFERENCES_H */
