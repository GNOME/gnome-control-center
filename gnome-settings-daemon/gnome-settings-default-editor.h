/*
 * gnome-settings-default-editor.h: sync default editor changes to mime database
 *
 * Copyright 2002 Sun Microsystems, Inc.
 *
 * Author: jacob berkman  <jacob@ximian.com>
 *
 */

#ifndef GNOME_SETTINGS_DEFAULT_EDITOR_H
#define GNOME_SETTINGS_DEFAULT_EDITOR_H

#include <gconf/gconf-client.h>

void gnome_settings_default_editor_init (GConfClient *client);
void gnome_settings_default_editor_load (GConfClient *client);

#endif /* GNOME_SETTINGS_DEFAULT_EDITOR_H */
