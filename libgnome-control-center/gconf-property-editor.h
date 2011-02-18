/* -*- mode: c; style: linux -*- */

/* gconf-property-editor.h
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

#ifndef __GCONF_PROPERTY_EDITOR_H
#define __GCONF_PROPERTY_EDITOR_H

#include <gtk/gtk.h>
#include <glib/gi18n.h>
#include <gconf/gconf-client.h>
#include <gconf/gconf-changeset.h>

G_BEGIN_DECLS

#define GCONF_PROPERTY_EDITOR(obj)          G_TYPE_CHECK_INSTANCE_CAST (obj, gconf_property_editor_get_type (), GConfPropertyEditor)
#define GCONF_PROPERTY_EDITOR_CLASS(klass)  G_TYPE_CHECK_CLASS_CAST (klass, gconf_property_editor_get_type (), GConfPropertyEditorClass)
#define IS_GCONF_PROPERTY_EDITOR(obj)       G_TYPE_CHECK_INSTANCE_TYPE (obj, gconf_property_editor_get_type ())

typedef struct _GConfPropertyEditor GConfPropertyEditor;
typedef struct _GConfPropertyEditorClass GConfPropertyEditorClass;
typedef struct _GConfPropertyEditorPrivate GConfPropertyEditorPrivate;

typedef GConfValue *(*GConfPEditorValueConvFn) (GConfPropertyEditor *peditor, const GConfValue *);
typedef int	    (*GConfPEditorGetValueFn)  (GConfPropertyEditor *peditor, gpointer data);

struct _GConfPropertyEditor 
{
	GObject parent;

	GConfPropertyEditorPrivate *p;
};

struct _GConfPropertyEditorClass 
{
	GObjectClass g_object_class;

	void (*value_changed) (GConfPropertyEditor *peditor, gchar *key, const GConfValue *value);
};

GType gconf_property_editor_get_type    (void);

const gchar *gconf_property_editor_get_key        (GConfPropertyEditor  *peditor);
GObject     *gconf_property_editor_get_ui_control (GConfPropertyEditor  *peditor);

GObject *gconf_peditor_new_boolean      (GConfChangeSet          *changeset,
					 const gchar             *key,
					 GtkWidget               *checkbox,
					 const gchar             *first_property_name,
					 ...);

GObject *gconf_peditor_new_switch      (GConfChangeSet          *changeset,
					 const gchar             *key,
					 GtkWidget               *sw,
					 const gchar             *first_property_name,
					 ...);

GObject *gconf_peditor_new_enum_toggle  (GConfChangeSet 	 *changeset,
					 const gchar		 *key,
					 GtkWidget		 *checkbox,
					 GType			 enum_type,
					 GConfPEditorGetValueFn  val_true_fn,
					 guint			 val_false,
					 gboolean	 	 use_nick,
					 gpointer		 data,
					 const gchar 		 *first_property_name,
					 ...);

GObject *gconf_peditor_new_integer      (GConfChangeSet          *changeset,
					 const gchar             *key,
					 GtkWidget               *entry,
					 const gchar             *first_property_name,
					 ...);
GObject *gconf_peditor_new_string       (GConfChangeSet          *changeset,
					 const gchar             *key,
					 GtkWidget               *entry,
					 const gchar             *first_property_name,
					 ...);
GObject *gconf_peditor_new_color        (GConfChangeSet          *changeset,
					 const gchar             *key,
					 GtkWidget               *color_entry,
					 const gchar             *first_property_name,
					 ...);

GObject *gconf_peditor_new_combo_box	(GConfChangeSet *changeset,
					 const gchar 	*key,
					 GtkWidget      *combo_box,
					 const gchar    *first_property_name,
					 ...);

GObject *gconf_peditor_new_combo_box_with_enum	(GConfChangeSet *changeset,
						 const gchar 	*key,
						 GtkWidget      *combo_box,
						 GType          enum_type,
						 gboolean  	use_nick,
						 const gchar    *first_property_name,
						 ...);

GObject *gconf_peditor_new_select_radio (GConfChangeSet          *changeset,
					 const gchar             *key,
					 GSList                  *radio_group,
					 const gchar             *first_property_name,
					 ...);

GObject *gconf_peditor_new_select_radio_with_enum	 (GConfChangeSet *changeset,
							  const gchar	 *key,
							  GSList 	 *radio_group,
							  GType 	 enum_type,
							  gboolean	 use_nick,
							  const gchar    *first_property_name,
							  ...);

GObject *gconf_peditor_new_numeric_range (GConfChangeSet          *changeset,
					  const gchar             *key,
					  GtkWidget               *range,
					  const gchar             *first_property_name,
					  ...);

GObject *gconf_peditor_new_font          (GConfChangeSet          *changeset,
					  const gchar             *key,
					  GtkWidget               *font_button,
					  const gchar             *first_property_name,
					  ...);

GObject *gconf_peditor_new_image	 (GConfChangeSet	  *changeset,
					  const gchar		  *key,
					  GtkWidget		  *button,
					  const gchar		  *first_property,
					  ...);

GObject *gconf_peditor_new_tree_view	(GConfChangeSet *changeset,
					 const gchar 	*key,
					 GtkWidget      *tree_view,
					 const gchar    *first_property_name,
					 ...);

void gconf_peditor_widget_set_guard     (GConfPropertyEditor     *peditor,
					 GtkWidget               *widget);

/* some convenience callbacks to map int <-> float */
GConfValue *gconf_value_int_to_float    (GConfPropertyEditor *ignored, GConfValue const *value);
GConfValue *gconf_value_float_to_int    (GConfPropertyEditor *ignored, GConfValue const *value);

G_END_DECLS

#endif /* __GCONF_PROPERTY_EDITOR_H */
