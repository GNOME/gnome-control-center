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

#include <gnome.h>
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

typedef enum
{
	PEDITOR_FONT_NAME,
	PEDITOR_FONT_SIZE,
	PEDITOR_FONT_COMBINED
} GConfPEditorFontType;

struct _GConfPropertyEditor 
{
	GObject parent;

	GConfPropertyEditorPrivate *p;
};

struct _GConfPropertyEditorClass 
{
	GObjectClass g_object_class;

	void (*value_changed) (GConfPropertyEditor *peditor, gchar *key, GConfValue *value);
};

GType gconf_property_editor_get_type    (void);

const gchar *gconf_property_editor_get_key (GConfPropertyEditor  *peditor);

GObject *gconf_peditor_new_boolean      (GConfChangeSet          *changeset,
					 gchar                   *key,
					 GtkWidget               *checkbox,
					 gchar                   *first_property_name,
					 ...);

GObject *gconf_peditor_new_enum_toggle  (GConfChangeSet 	 *changeset,
					 gchar			 *key,
					 GtkWidget		 *checkbox,
					 GType			 enum_type,
					 GConfPEditorGetValueFn  val_true_fn,
					 guint			 val_false,
					 gpointer		 data,
					 gchar 			 *first_property_name,
					 ...);

GObject *gconf_peditor_new_string       (GConfChangeSet          *changeset,
					 gchar                   *key,
					 GtkWidget               *entry,
					 gchar                   *first_property_name,
					 ...);
GObject *gconf_peditor_new_filename     (GConfChangeSet          *changeset,
					 gchar                   *key,
					 GtkWidget               *file_entry,
					 gchar                   *first_property_name,
					 ...);
GObject *gconf_peditor_new_color        (GConfChangeSet          *changeset,
					 gchar                   *key,
					 GtkWidget               *color_entry,
					 gchar                   *first_property_name,
					 ...);

GObject *gconf_peditor_new_select_menu	(GConfChangeSet *changeset,
					 gchar 	        *key,
					 GtkWidget      *option_menu,
					 gchar          *first_property_name,
					 ...);


GObject *gconf_peditor_new_select_menu_with_enum	(GConfChangeSet *changeset,
							 gchar 	        *key,
							 GtkWidget      *option_menu,
							 GType          enum_type,
							 gchar          *first_property_name,
							 ...);

GObject *gconf_peditor_new_select_radio (GConfChangeSet          *changeset,
					 gchar                   *key,
					 GSList                  *radio_group,
					 gchar                   *first_property_name,
					 ...);

GObject *gconf_peditor_new_select_radio_with_enum	 (GConfChangeSet *changeset,
							  gchar		 *key,
							  GSList 	 *radio_group,
							  GType 	 enum_type,
							  gchar          *first_property_name,
							  ...);

GObject *gconf_peditor_new_numeric_range (GConfChangeSet          *changeset,
					  gchar                   *key,
					  GtkWidget               *range,
					  gchar                   *first_property_name,
					  ...);

GObject *gconf_peditor_new_font          (GConfChangeSet          *changeset,
					  gchar                   *key,
					  GtkWidget               *font_picker,
					  GConfPEditorFontType	   font_type,
					  gchar                   *first_property_name,
					  ...);

GObject *gconf_peditor_new_image	 (GConfChangeSet	  *changeset,
					  gchar			  *key,
					  GtkWidget		  *button,
					  gchar			  *first_property,
					  ...);

void gconf_peditor_widget_set_guard     (GConfPropertyEditor     *peditor,
					 GtkWidget               *widget);

GConfValue *gconf_value_int_to_float    (const GConfValue        *value);
GConfValue *gconf_value_float_to_int    (const GConfValue        *value);

G_END_DECLS

#endif /* __GCONF_PROPERTY_EDITOR_H */
