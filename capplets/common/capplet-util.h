/* -*- mode: c; style: linux -*- */

/* capplet-util.h
 * Copyright (C) 2001 Ximian, Inc.
 *
 * Written by Bradford Hovinen <hovinen@ximian.com>
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

#ifndef __CAPPLET_UTIL_H
#define __CAPPLET_UTIL_H

#include <gnome.h>
#include <gconf/gconf.h>
#include <gconf/gconf-changeset.h>

/* Macros to make certain repetitive tasks a bit easier */

/* Print a debugging message */

#define DEBUG_MSG(str, ...) \
              g_log (G_LOG_DOMAIN, G_LOG_LEVEL_DEBUG, "(%d:%s) " str, \
		     getpid (), G_GNUC_FUNCTION, __VA_ARGS__)

/* Retrieve a widget from the Glade object */

#define WID(s) glade_xml_get_widget (dialog, s)

/* Copy a setting from the legacy gnome-config settings to the ConfigDatabase */

#define COPY_FROM_LEGACY(type, key, legacy_key)                                 \
	val_##type = gnome_config_get_##type##_with_default (legacy_key, &def); \
                                                                                \
	if (!def)                                                               \
		gconf_client_set_##type (client, key, val_##type, NULL);

/* Callback to apply the settings in the given database */
typedef void (*ApplySettingsFn) (void);

/* Callback to set up the dialog proper */
typedef GtkWidget *(*CreateDialogFn) (void);

/* Callback to set up property editors for the dialog */
typedef void (*SetupPropertyEditorsFn) (GtkWidget *dialog, GConfChangeSet *changeset);

/* Callback to retrieve legacy settings and store them in the new configuration
 * database */
typedef void (*GetLegacySettingsFn) (void);

/* Set up the session management so that this capplet will apply its
 * settings on every startup
 */

void setup_session_mgmt (const gchar *binary_name);

/* Wrapper function for the entire capplet. This handles all initialization and
 * runs the capplet for you. Just supply the appropriate callbacks and your argc
 * and argv from main()
 *
 * This function makes several assumptions, requiring that all capplets follow a
 * particular convention. In particular, suppose the name of the capplet binary
 * is foo-properties-capplet. Then:
 *
 *   - The factory IID is Bonobo_Control_Capplet_foo_properties_Factory
 *   - The default configuration moniker is archiver:foo-properties
 *
 * Following this convention yields capplets that are more uniform and thus
 * easier to maintain, and simplifies the interfaces quite a bit. All capplet in
 * this package are required to follow this convention.
 */

void capplet_init (int                      argc,
		   gchar                  **argv,
		   ApplySettingsFn          apply_fn,
		   CreateDialogFn           create_dialog_fn,
		   SetupPropertyEditorsFn   setup_property_editors_fn,
		   GetLegacySettingsFn      get_legacy_settings_fn);

void capplet_error_dialog (GtkWindow *parent, char const *msg, GError *err);
void capplet_help (GtkWindow *parent, char const *helpfile, char const *section);
void capplet_set_icon (GtkWidget *window, char const *icon_file_name);

#endif /* __CAPPLET_UTIL_H */
