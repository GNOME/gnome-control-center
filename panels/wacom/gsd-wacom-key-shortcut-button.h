/*
 * gsd-wacom-key-shortcut-button.h
 *
 * Copyright © 2013 Red Hat, Inc.
 *
 * Author: Joaquim Rocha <jrocha@redhat.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 */

#ifndef __GSD_WACOM_KEY_SHORTCUT_BUTTON_H__
#define __GSD_WACOM_KEY_SHORTCUT_BUTTON_H__

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define GSD_WACOM_TYPE_KEY_SHORTCUT_BUTTON            (gsd_wacom_key_shortcut_button_get_type ())
#define GSD_WACOM_KEY_SHORTCUT_BUTTON(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), GSD_WACOM_TYPE_KEY_SHORTCUT_BUTTON, GsdWacomKeyShortcutButton))
#define GSD_WACOM_IS_KEY_SHORTCUT_BUTTON(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GSD_WACOM_TYPE_KEY_SHORTCUT_BUTTON))
#define GSD_WACOM_KEY_SHORTCUT_BUTTON_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), GSD_WACOM_TYPE_KEY_SHORTCUT_BUTTON, GsdWacomKeyShortcutButtonClass))
#define GSD_WACOM_IS_KEY_SHORTCUT_BUTTON_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), GSD_WACOM_TYPE_KEY_SHORTCUT_BUTTON))
#define GSD_WACOM_KEY_SHORTCUT_BUTTON_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), GSD_WACOM_TYPE_KEY_SHORTCUT_BUTTON, GsdWacomKeyShortcutButtonClass))

typedef struct _GsdWacomKeyShortcutButton        GsdWacomKeyShortcutButton;
typedef struct _GsdWacomKeyShortcutButtonClass   GsdWacomKeyShortcutButtonClass;
typedef struct _GsdWacomKeyShortcutButtonPrivate GsdWacomKeyShortcutButtonPrivate;

GType gsd_wacom_key_shortcut_button_mode_type (void) G_GNUC_CONST;
#define GSD_WACOM_TYPE_KEY_SHORTCUT_BUTTON_MODE (gsd_wacom_key_shortcut_button_mode_type())

typedef enum
{
  GSD_WACOM_KEY_SHORTCUT_BUTTON_MODE_OTHER,
  GSD_WACOM_KEY_SHORTCUT_BUTTON_MODE_ALL
} GsdWacomKeyShortcutButtonMode;

struct _GsdWacomKeyShortcutButton
{
  GtkButton parent_instance;

  /*< private >*/
  GsdWacomKeyShortcutButtonPrivate *priv;
};

struct _GsdWacomKeyShortcutButtonClass
{
  GtkButtonClass parent_class;

  void (* key_shortcut_edited)  (GsdWacomKeyShortcutButton *key_shortcut_button);

  void (* key_shortcut_cleared) (GsdWacomKeyShortcutButton *key_shortcut_button);
};

GType          gsd_wacom_key_shortcut_button_get_type        (void) G_GNUC_CONST;
GtkWidget    * gsd_wacom_key_shortcut_button_new             (void);

#endif /* __GSD_WACOM_KEY_SHORTCUT_BUTTON_H__ */
