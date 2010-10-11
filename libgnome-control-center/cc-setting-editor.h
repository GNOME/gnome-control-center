/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*-
 *
 * Copyright (C) 2010 Rodrigo Moya
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
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 *
 * Authors: Rodrigo Moya <rodrigo@gnome.org>
 */

#ifndef __CC_SETTING_EDITOR_H
#define __CC_SETTING_EDITOR_H

#include <gio/gio.h>
#include <gtk/gtk.h>

G_BEGIN_DECLS

#define CC_TYPE_SETTING_EDITOR         (cc_setting_editor_get_type ())
#define CC_SETTING_EDITOR(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), CC_TYPE_SETTING_EDITOR, CcSettingEditor))
#define CC_SETTING_EDITOR_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST((k), CC_TYPE_SETTING_EDITOR, CcSettingEditorClass))
#define CC_IS_SETTING_EDITOR(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), CC_TYPE_SETTING_EDITOR))
#define CC_IS_SETTING_EDITOR_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), CC_TYPE_SETTING_EDITOR))
#define CC_SETTING_EDITOR_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), CC_TYPE_SETTING_EDITOR, CcSettingEditorClass))

typedef struct _CcSettingEditorPrivate CcSettingEditorPrivate;
typedef struct _CcSettingEditor        CcSettingEditor;
typedef struct _CcSettingEditorClass   CcSettingEditorClass;

/**
 * CcSettingEditor:
 *
 * The contents of this struct are private and should not be accessed directly.
 */
struct _CcSettingEditor
{
  /*< private >*/
  GtkVBox parent;
  CcSettingEditorPrivate *priv;
};

/**
 * CcSettingEditorClass:
 *
 * The contents of this struct are private and should not be accessed directly.
 */
struct _CcSettingEditorClass
{
  /*< private >*/
  GtkVBoxClass parent_class;

  void (* value_changed) (CcSettingEditor *seditor, const gchar *key, GVariant *value);
};

GType        cc_setting_editor_get_type (void);

GtkWidget   *cc_setting_editor_new_application (const gchar *mime_type);
GtkWidget   *cc_setting_editor_new_boolean (const gchar *label,
                                            const gchar *settings_prefix,
                                            const gchar *key);
GtkWidget   *cc_setting_editor_new_string (const gchar *settings_prefix,
                                           const gchar *key);

const gchar *cc_setting_editor_get_key (CcSettingEditor *editor);
GtkWidget   *cc_setting_editor_get_ui_control (CcSettingEditor *seditor);

G_END_DECLS

#endif
