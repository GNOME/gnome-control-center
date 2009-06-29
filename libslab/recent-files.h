/*
 * This file is part of the Main Menu.
 *
 * Copyright (c) 2006 Novell, Inc.
 *
 * The Main Menu is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the Free
 * Software Foundation; either version 2 of the License, or (at your option)
 * any later version.
 *
 * The Main Menu is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * the Main Menu; if not, write to the Free Software Foundation, Inc., 51
 * Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 */

#ifndef __RECENT_FILES_H__
#define __RECENT_FILES_H__

#include <glib.h>
#include <glib-object.h>
#include <time.h>

G_BEGIN_DECLS

#define MAIN_MENU_RECENT_MONITOR_TYPE         (main_menu_recent_monitor_get_type ())
#define MAIN_MENU_RECENT_MONITOR(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), MAIN_MENU_RECENT_MONITOR_TYPE, MainMenuRecentMonitor))
#define MAIN_MENU_RECENT_MONITOR_CLASS(c)     (G_TYPE_CHECK_CLASS_CAST ((c), MAIN_MENU_RECENT_MONITOR_TYPE, MainMenuRecentMonitorClass))
#define IS_MAIN_MENU_RECENT_MONITOR(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), MAIN_MENU_RECENT_MONITOR_TYPE))
#define IS_MAIN_MENU_RECENT_MONITOR_CLASS(c)  (G_TYPE_CHECK_CLASS_TYPE ((c), MAIN_MENU_RECENT_MONITOR_TYPE))
#define MAIN_MENU_RECENT_MONITOR_GET_CLASS(o) (G_TYPE_CHECK_GET_CLASS ((o), MAIN_MENU_RECENT_MONITOR_TYPE, MainMenuRecentMonitorClass))

#define MAIN_MENU_RECENT_FILE_TYPE         (main_menu_recent_file_get_type ())
#define MAIN_MENU_RECENT_FILE(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), MAIN_MENU_RECENT_FILE_TYPE, MainMenuRecentFile))
#define MAIN_MENU_RECENT_FILE_CLASS(c)     (G_TYPE_CHECK_CLASS_CAST ((c), MAIN_MENU_RECENT_FILE_TYPE, MainMenuRecentFileClass))
#define IS_MAIN_MENU_RECENT_FILE(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), MAIN_MENU_RECENT_FILE_TYPE))
#define IS_MAIN_MENU_RECENT_FILE_CLASS(c)  (G_TYPE_CHECK_CLASS_TYPE ((c), MAIN_MENU_RECENT_FILE_TYPE))
#define MAIN_MENU_RECENT_FILE_GET_CLASS(o) (G_TYPE_CHECK_GET_CLASS ((o), MAIN_MENU_RECENT_FILE_TYPE, MainMenuRecentFileClass))

typedef struct {
	GObject g_object;
} MainMenuRecentMonitor;

typedef struct {
	GObjectClass g_object_class;

	void (* changed) (MainMenuRecentMonitor *);
} MainMenuRecentMonitorClass;

GType main_menu_recent_monitor_get_type (void);

MainMenuRecentMonitor *main_menu_recent_monitor_new (void);

typedef struct {
	GObject g_object;
} MainMenuRecentFile;

typedef struct {
	GObjectClass g_object_class;
} MainMenuRecentFileClass;

GType main_menu_recent_file_get_type (void);

GList *main_menu_get_recent_files (MainMenuRecentMonitor *mgr);
GList *main_menu_get_recent_apps  (MainMenuRecentMonitor *mgr);

const gchar *main_menu_recent_file_get_uri       (MainMenuRecentFile *this);
const gchar *main_menu_recent_file_get_mime_type (MainMenuRecentFile *this);
time_t       main_menu_recent_file_get_modified  (MainMenuRecentFile *this);

void main_menu_rename_recent_file (MainMenuRecentMonitor *mgr, const gchar *uri_0, const gchar *uri_1);
void main_menu_remove_recent_file (MainMenuRecentMonitor *mgr, const gchar *uri);

G_END_DECLS

#endif /* __RECENT_FILES_H__ */
