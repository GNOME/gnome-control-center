/*
 * Copyright 2001 Red Hat, Inc.
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation, and that the name of Red Hat not be used in advertising or
 * publicity pertaining to distribution of the software without specific,
 * written prior permission.  Red Hat makes no representations about the
 * suitability of this software for any purpose.  It is provided "as is"
 * without express or implied warranty.
 *
 * RED HAT DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE, INCLUDING ALL
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO EVENT SHALL RED HAT
 * BE LIABLE FOR ANY SPECIAL, INDIRECT OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION
 * OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN 
 * CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 *
 * Authors:  Jonathan Blandford
 */

#ifndef __GNOME_SETTINGS_DAEMON_H
#define __GNOME_SETTINGS_DAEMON_H

#include <gconf/gconf.h>
#include <gconf/gconf-client.h>
#include <gtk/gtk.h>

#include <bonobo/bonobo-object.h>

#include "GNOME_SettingsDaemon.h"

typedef void (* KeyCallbackFunc) (GConfEntry *entry);

void       gnome_settings_daemon_register_callback (const char      *dir,
						    KeyCallbackFunc  func);
GtkWidget *gnome_settings_daemon_get_invisible     (void);


G_BEGIN_DECLS

#define GNOME_SETTINGS_DAEMON_TYPE (gnome_settings_daemon_get_type ())
#define GNOME_SETTINGS_DAEMON(obj) (G_TYPE_CHECK_INSTANCE_CAST ((obj), GNOME_SETTINGS_DAEMON_TYPE, GnomeSettingsDaemon))
#define GNOME_SETTINGS_DAEMON_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST ((klass), GNOME_SETTINGS_DAEMON_TYPE, GnomeSettingsDaemonClass))
#define IS_GNOME_SETTINGS_DAEMON(obj) (GTK_TYPE_CHECK_INSTANCE_TYPE ((obj), GNOME_SETTINGS_DAEMON_TYPE))
#define IS_GNOME_SETTINGS_DAEMON_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), GNOME_SETTINGS_DAEMON_TYPE))
#define GNOME_SETTINGS_DAEMON_GET_CLASS(obj) (G_TYPE_INSTANCE_GET_CLASS ((obj), GNOME_SETTINGS_DAEMON_TYPE, GnomeSettingsDaemonClass))

typedef struct _GnomeSettingsDaemon GnomeSettingsDaemon;
typedef struct _GnomeSettingsDaemonClass GnomeSettingsDaemonClass;
typedef struct _GnomeSettingsDaemonPrivate GnomeSettingsDaemonPrivate;

struct _GnomeSettingsDaemon
{
  BonoboObject parent_instance;
  GnomeSettingsDaemonPrivate *private;
};

struct _GnomeSettingsDaemonClass
{
  BonoboObjectClass parent_class;
  POA_GNOME_SettingsDaemon__epv epv;
};

GType gnome_settings_daemon_get_type          (void);
GObject *gnome_settings_daemon_new             (void);

G_END_DECLS

#endif /* __GNOME_SETTINGS_DAEMON_H */
