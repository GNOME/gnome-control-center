/* -*- mode: c; style: linux -*- */

/* gconf-property-editor.c
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

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <string.h>
#include <stdarg.h>

#include "gconf-property-editor.h"
#include "gconf-property-editor-marshal.h"

#include "preview-file-selection.h"

enum {
	VALUE_CHANGED,
	LAST_SIGNAL
};

enum {
	PROP_0,
	PROP_KEY,
	PROP_CALLBACK,
	PROP_CHANGESET,
	PROP_CONV_TO_WIDGET_CB,
	PROP_CONV_FROM_WIDGET_CB,
	PROP_UI_CONTROL,
	PROP_DATA,
	PROP_DATA_FREE_CB
};

typedef void (*GConfPropertyEditorDataFreeCb) (gpointer data);

struct _GConfPropertyEditorPrivate 
{
	gchar                   *key;
	guint                    handler_id;
	GConfChangeSet          *changeset;
	GObject                 *ui_control;
	GConfPEditorValueConvFn  conv_to_widget_cb;
	GConfPEditorValueConvFn  conv_from_widget_cb;
	GConfClientNotifyFunc    callback;
	gboolean                 inited;

	gpointer 		      data;
	GConfPropertyEditorDataFreeCb data_free_cb;
};

typedef struct
{
	GType			 enum_type;
	GConfPEditorGetValueFn   enum_val_true_fn;
	gpointer		 enum_val_true_fn_data;
	guint			 enum_val_false;
	gboolean		 use_nick;
} GConfPropertyEditorEnumData;

static guint peditor_signals[LAST_SIGNAL];

static GObjectClass *parent_class;

static void gconf_property_editor_init        (GConfPropertyEditor      *gconf_property_editor,
					       GConfPropertyEditorClass *class);
static void gconf_property_editor_class_init  (GConfPropertyEditorClass *class);
static void gconf_property_editor_base_init   (GConfPropertyEditorClass *class);

static void gconf_property_editor_set_prop    (GObject      *object, 
					       guint         prop_id,
					       const GValue *value, 
					       GParamSpec   *pspec);
static void gconf_property_editor_get_prop    (GObject      *object,
					       guint         prop_id,
					       GValue       *value,
					       GParamSpec   *pspec);

static void gconf_property_editor_finalize    (GObject      *object);

static GObject *gconf_peditor_new             (gchar                 *key,
					       GConfClientNotifyFunc  cb,
					       GConfChangeSet        *changeset,
					       GObject               *ui_control,
					       const gchar           *first_prop_name,
					       va_list                var_args,
					       const gchar	     *first_custom,
					       ...);

GType
gconf_property_editor_get_type (void)
{
	static GType gconf_property_editor_type = 0;

	if (!gconf_property_editor_type) {
		GTypeInfo gconf_property_editor_info = {
			sizeof (GConfPropertyEditorClass),
			(GBaseInitFunc) gconf_property_editor_base_init,
			NULL, /* GBaseFinalizeFunc */
			(GClassInitFunc) gconf_property_editor_class_init,
			NULL, /* GClassFinalizeFunc */
			NULL, /* user-supplied data */
			sizeof (GConfPropertyEditor),
			0, /* n_preallocs */
			(GInstanceInitFunc) gconf_property_editor_init,
			NULL
		};

		gconf_property_editor_type = 
			g_type_register_static (G_TYPE_OBJECT, 
						"GConfPropertyEditor",
						&gconf_property_editor_info, 0);
	}

	return gconf_property_editor_type;
}

static GConfValue* 
gconf_property_editor_conv_default (GConfPropertyEditor *peditor,
				    const GConfValue *value)
{
	return gconf_value_copy (value);
}

static void
gconf_property_editor_init (GConfPropertyEditor      *gconf_property_editor,
			    GConfPropertyEditorClass *class)
{
	gconf_property_editor->p = g_new0 (GConfPropertyEditorPrivate, 1);
	gconf_property_editor->p->conv_to_widget_cb = gconf_property_editor_conv_default;
	gconf_property_editor->p->conv_from_widget_cb = gconf_property_editor_conv_default;
	gconf_property_editor->p->inited = FALSE;
}

static void
gconf_property_editor_base_init (GConfPropertyEditorClass *class) 
{
}

static void
gconf_property_editor_class_init (GConfPropertyEditorClass *class) 
{
	GObjectClass *object_class;

	object_class = G_OBJECT_CLASS (class);

	object_class->finalize = gconf_property_editor_finalize;
	object_class->set_property = gconf_property_editor_set_prop;
	object_class->get_property = gconf_property_editor_get_prop;

	g_object_class_install_property
		(object_class, PROP_KEY,
		 g_param_spec_string ("key",
				      _("Key"),
				      _("GConf key to which this property editor is attached"),
				      NULL,
				      G_PARAM_READWRITE));
	g_object_class_install_property
		(object_class, PROP_CALLBACK,
		 g_param_spec_pointer ("callback",
				       _("Callback"),
				       _("Issue this callback when the value associated with key gets changed"),
				       G_PARAM_WRITABLE));
	g_object_class_install_property
		(object_class, PROP_CHANGESET,
		 g_param_spec_pointer ("changeset",
				       _("Change set"),
				       _("GConf change set containing data to be forwarded to the gconf client on apply"),
				       G_PARAM_READWRITE));
	g_object_class_install_property
		(object_class, PROP_CONV_TO_WIDGET_CB,
		 g_param_spec_pointer ("conv-to-widget-cb",
				       _("Conversion to widget callback"),
				       _("Callback to be issued when data are to be converted from GConf to the widget"),
				       G_PARAM_WRITABLE));
	g_object_class_install_property
		(object_class, PROP_CONV_FROM_WIDGET_CB,
		 g_param_spec_pointer ("conv-from-widget-cb",
				       _("Conversion from widget callback"),
				       _("Callback to be issued when data are to be converted to GConf from the widget"),
				       G_PARAM_WRITABLE));
	g_object_class_install_property
		(object_class, PROP_UI_CONTROL,
		 g_param_spec_object ("ui-control",
				      _("UI Control"),
				      _("Object that controls the property (normally a widget)"),
				      G_TYPE_OBJECT,
				      G_PARAM_WRITABLE));

	peditor_signals[VALUE_CHANGED] =
		g_signal_new ("value-changed",
			      G_TYPE_FROM_CLASS (object_class), 0,
			      G_STRUCT_OFFSET (GConfPropertyEditorClass, value_changed),
			      NULL, NULL,
			      (GSignalCMarshaller) gconf_property_editor_marshal_VOID__STRING_POINTER,
			      G_TYPE_NONE, 2, G_TYPE_STRING, G_TYPE_POINTER);

	g_object_class_install_property
		(object_class, PROP_DATA,
		 g_param_spec_pointer ("data",
				       _("Property editor object data"),
				       _("Custom data required by the specific property editor"),
				       G_PARAM_WRITABLE));

	g_object_class_install_property
		(object_class, PROP_DATA_FREE_CB,
		 g_param_spec_pointer ("data-free-cb",
				       _("Property editor data freeing callback"),
				       _("Callback to be issued when property editor object data is to be freed"),
				       G_PARAM_WRITABLE));

	parent_class = G_OBJECT_CLASS
		(g_type_class_ref (G_TYPE_OBJECT));
}

static void
gconf_property_editor_set_prop (GObject      *object,
				guint         prop_id,
				const GValue *value,
				GParamSpec   *pspec) 
{
	GConfPropertyEditor *peditor;
	GConfClient         *client;
	GConfNotifyFunc      cb;

	g_return_if_fail (object != NULL);
	g_return_if_fail (IS_GCONF_PROPERTY_EDITOR (object));

	peditor = GCONF_PROPERTY_EDITOR (object);

	switch (prop_id) {
	case PROP_KEY:
		peditor->p->key = g_value_dup_string (value);
		break;

	case PROP_CALLBACK:
		client = gconf_client_get_default ();
		cb = g_value_get_pointer (value);
		peditor->p->callback = (GConfClientNotifyFunc) cb;
		peditor->p->handler_id =
			gconf_client_notify_add (client, peditor->p->key,
						 peditor->p->callback,
						 peditor, NULL, NULL);
		break;

	case PROP_CHANGESET:
		peditor->p->changeset = g_value_get_pointer (value);
		break;

	case PROP_CONV_TO_WIDGET_CB:
		peditor->p->conv_to_widget_cb = g_value_get_pointer (value);
		break;

	case PROP_CONV_FROM_WIDGET_CB:
		peditor->p->conv_from_widget_cb = g_value_get_pointer (value);
		break;

	case PROP_UI_CONTROL:
		peditor->p->ui_control = g_value_get_object (value);
		g_object_weak_ref (peditor->p->ui_control, (GWeakNotify) g_object_unref, object);
		break;
	case PROP_DATA:
		peditor->p->data = g_value_get_pointer (value);
		break;
	case PROP_DATA_FREE_CB:
		peditor->p->data_free_cb = g_value_get_pointer (value);
		break;
	default:
		g_warning ("Bad argument set");
		break;
	}
}

static void
gconf_property_editor_get_prop (GObject    *object,
				guint       prop_id,
				GValue     *value,
				GParamSpec *pspec) 
{
	GConfPropertyEditor *peditor;

	g_return_if_fail (object != NULL);
	g_return_if_fail (IS_GCONF_PROPERTY_EDITOR (object));

	peditor = GCONF_PROPERTY_EDITOR (object);

	switch (prop_id) {
	case PROP_KEY:
		g_value_set_string (value, peditor->p->key);
		break;

	case PROP_CHANGESET:
		g_value_set_pointer (value, peditor->p->changeset);
		break;

	default:
		g_warning ("Bad argument get");
		break;
	}
}

static void
gconf_property_editor_finalize (GObject *object) 
{
	GConfPropertyEditor *gconf_property_editor;

	g_return_if_fail (object != NULL);
	g_return_if_fail (IS_GCONF_PROPERTY_EDITOR (object));

	gconf_property_editor = GCONF_PROPERTY_EDITOR (object);

	if (gconf_property_editor->p->data_free_cb)
		gconf_property_editor->p->data_free_cb (gconf_property_editor->p->data);
	
	g_free (gconf_property_editor->p);

	G_OBJECT_CLASS (parent_class)->finalize (object);
}

static GObject *
gconf_peditor_new (gchar                 *key,
		   GConfClientNotifyFunc  cb,
		   GConfChangeSet        *changeset,
		   GObject               *ui_control,
		   const gchar           *first_prop_name,
		   va_list                var_args,
		   const gchar		 *first_custom,
		   ...) 
{
	GObject     *obj;
	GConfClient *client;
	GConfEntry  *gconf_entry;

	g_return_val_if_fail (key != NULL, NULL);
	g_return_val_if_fail (cb != NULL, NULL);

	obj = g_object_new (gconf_property_editor_get_type (),
			    "key",        key,
			    "callback",   cb,
			    "changeset",  changeset,
			    "ui-control", ui_control,
			    NULL);

	g_object_set_valist (obj, first_prop_name, var_args);

	if (first_custom)
	{
		va_list custom_args;
		va_start (custom_args, first_custom);
		g_object_set_valist (obj, first_custom, custom_args);
		va_end (custom_args);
	}

	client = gconf_client_get_default ();
	gconf_entry = gconf_client_get_entry (client, GCONF_PROPERTY_EDITOR (obj)->p->key, NULL, TRUE, NULL);
	GCONF_PROPERTY_EDITOR (obj)->p->callback (client, 0, gconf_entry, obj);
	GCONF_PROPERTY_EDITOR (obj)->p->inited = TRUE;

	return obj;
}

const gchar *
gconf_property_editor_get_key (GConfPropertyEditor *peditor)
{
	return peditor->p->key;
}

static void
peditor_set_gconf_value (GConfPropertyEditor *peditor,
			 const gchar         *key,
			 GConfValue          *value) 
{
	if (peditor->p->changeset != NULL)
		gconf_change_set_set (peditor->p->changeset, peditor->p->key, value);
	else
		gconf_client_set (gconf_client_get_default (), peditor->p->key, value, NULL);
}

static void
peditor_boolean_value_changed (GConfClient         *client,
			       guint                cnxn_id,
			       GConfEntry          *entry,
			       GConfPropertyEditor *peditor) 
{
	GConfValue *value, *value_wid;

	if (peditor->p->changeset != NULL)
		gconf_change_set_remove (peditor->p->changeset, peditor->p->key);

	value = gconf_entry_get_value (entry);

	if (value != NULL) {
		value_wid = peditor->p->conv_to_widget_cb (peditor, value);
		gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (peditor->p->ui_control), gconf_value_get_bool (value_wid));
		gconf_value_free (value_wid);
	}
}

static void
peditor_boolean_widget_changed (GConfPropertyEditor *peditor,
				GtkToggleButton     *tb)
{
	GConfValue *value, *value_wid;

	if (!peditor->p->inited) return;
	value_wid = gconf_value_new (GCONF_VALUE_BOOL);
	gconf_value_set_bool (value_wid, gtk_toggle_button_get_active (tb));
	value = peditor->p->conv_from_widget_cb (peditor, value_wid);
	peditor_set_gconf_value (peditor, peditor->p->key, value);
	g_signal_emit (peditor, peditor_signals[VALUE_CHANGED], 0, peditor->p->key, value);
	gconf_value_free (value_wid);
	gconf_value_free (value);
}

GObject *
gconf_peditor_new_boolean (GConfChangeSet *changeset,
			   gchar          *key,
			   GtkWidget      *checkbox,
			   gchar          *first_property_name,
			   ...)
{
	GObject *peditor;
	va_list var_args;

	g_return_val_if_fail (key != NULL, NULL);
	g_return_val_if_fail (checkbox != NULL, NULL);
	g_return_val_if_fail (GTK_IS_TOGGLE_BUTTON (checkbox), NULL);

	va_start (var_args, first_property_name);

	peditor = gconf_peditor_new
		(key,
		 (GConfClientNotifyFunc) peditor_boolean_value_changed,
		 changeset,
		 G_OBJECT (checkbox),
		 first_property_name,
		 var_args,
		 NULL);

	va_end (var_args);

	g_signal_connect_swapped (G_OBJECT (checkbox), "toggled",
				  (GCallback) peditor_boolean_widget_changed, peditor);

	return peditor;
}

static void
peditor_string_value_changed (GConfClient         *client,
			      guint                cnxn_id,
			      GConfEntry          *entry,
			      GConfPropertyEditor *peditor) 
{
	GConfValue *value, *value_wid;
	const char *entry_current_text;

	if (peditor->p->changeset != NULL)
		gconf_change_set_remove (peditor->p->changeset, peditor->p->key);

	value = gconf_entry_get_value (entry);

	if (value != NULL) {
		value_wid = peditor->p->conv_to_widget_cb (peditor, value);
		entry_current_text = gtk_entry_get_text (GTK_ENTRY (peditor->p->ui_control));
		if (strcmp (entry_current_text, gconf_value_get_string (value)) != 0) {
		  gtk_entry_set_text (GTK_ENTRY (peditor->p->ui_control), gconf_value_get_string (value_wid));
		}
		gconf_value_free (value_wid);
	}
}

static void
peditor_string_widget_changed (GConfPropertyEditor *peditor,
			       GtkEntry            *entry)
{
	  GConfValue *value, *value_wid;
	  
	if (!peditor->p->inited) return;

	value_wid = gconf_value_new (GCONF_VALUE_STRING);

	gconf_value_set_string (value_wid, gtk_entry_get_text (entry));
	value = peditor->p->conv_from_widget_cb (peditor, value_wid);

	peditor_set_gconf_value (peditor, peditor->p->key, value);

	g_signal_emit (peditor, peditor_signals[VALUE_CHANGED], 0, peditor->p->key, value);
	gconf_value_free (value_wid);
	gconf_value_free (value);
}

static GObject *
gconf_peditor_new_string_valist (GConfChangeSet *changeset,
				 gchar          *key,
				 GtkWidget      *entry,
				 gchar          *first_property_name,
				 va_list         var_args)
{
	GObject *peditor;

	peditor = gconf_peditor_new
		(key,
		 (GConfClientNotifyFunc) peditor_string_value_changed,
		 changeset,
		 G_OBJECT (entry),
		 first_property_name,
		 var_args, NULL);

	g_signal_connect_swapped (G_OBJECT (entry), "changed",
				  (GCallback) peditor_string_widget_changed, peditor);

	return peditor;
}

GObject *
gconf_peditor_new_string (GConfChangeSet *changeset,
			  gchar          *key,
			  GtkWidget      *entry,
			  gchar          *first_property_name,
			  ...)
{
	GObject *peditor;
	va_list var_args;

	g_return_val_if_fail (key != NULL, NULL);
	g_return_val_if_fail (entry != NULL, NULL);
	g_return_val_if_fail (GTK_IS_ENTRY (entry), NULL);

	va_start (var_args, first_property_name);

	peditor = gconf_peditor_new_string_valist
		(changeset, key, entry,
		 first_property_name, var_args);

	va_end (var_args);

	return peditor;
}

GObject *
gconf_peditor_new_filename (GConfChangeSet *changeset,
			    gchar          *key,
			    GtkWidget      *file_entry,
			    gchar          *first_property_name,
			    ...)
{
	GObject *peditor;
	va_list var_args;

	g_return_val_if_fail (key != NULL, NULL);
	g_return_val_if_fail (file_entry != NULL, NULL);
	g_return_val_if_fail (GNOME_IS_FILE_ENTRY (file_entry), NULL);

	va_start (var_args, first_property_name);

	peditor = gconf_peditor_new_string_valist
		(changeset, key,
		 gnome_file_entry_gtk_entry (GNOME_FILE_ENTRY (file_entry)),
		 first_property_name, var_args);

	va_end (var_args);

	return peditor;
}

static void
peditor_color_value_changed (GConfClient         *client,
			     guint                cnxn_id,
			     GConfEntry          *entry,
			     GConfPropertyEditor *peditor) 
{
	GConfValue *value, *value_wid;
	GdkColor color;

	if (peditor->p->changeset != NULL)
		gconf_change_set_remove (peditor->p->changeset, peditor->p->key);

	value = gconf_entry_get_value (entry);

	if (value != NULL) {
		value_wid = peditor->p->conv_to_widget_cb (peditor, value);
		gdk_color_parse (gconf_value_get_string (value_wid), &color);
		gnome_color_picker_set_i16 (GNOME_COLOR_PICKER (peditor->p->ui_control), color.red, color.green, color.blue, 65535);
		gconf_value_free (value_wid);
	}
}

static void
peditor_color_widget_changed (GConfPropertyEditor *peditor,
			      guint                r,
			      guint                g,
			      guint                b,
			      guint                a,
			      GnomeColorPicker    *cp)
{
	gchar *str;
	GConfValue *value, *value_wid;

	if (!peditor->p->inited) return;

	value_wid = gconf_value_new (GCONF_VALUE_STRING);
	str = g_strdup_printf ("#%02x%02x%02x", r >> 8, g >> 8, b >> 8);
	gconf_value_set_string (value_wid, str);
	g_free (str);

	value = peditor->p->conv_from_widget_cb (peditor, value_wid);

	peditor_set_gconf_value (peditor, peditor->p->key, value);
	g_signal_emit (peditor, peditor_signals[VALUE_CHANGED], 0, peditor->p->key, value);

	gconf_value_free (value_wid);
	gconf_value_free (value);
}

GObject *
gconf_peditor_new_color (GConfChangeSet *changeset,
			 gchar          *key,
			 GtkWidget      *cp,
			 gchar          *first_property_name,
			 ...)
{
	GObject *peditor;
	va_list var_args;

	g_return_val_if_fail (key != NULL, NULL);
	g_return_val_if_fail (cp != NULL, NULL);
	g_return_val_if_fail (GNOME_IS_COLOR_PICKER (cp), NULL);

	va_start (var_args, first_property_name);

	peditor = gconf_peditor_new
		(key,
		 (GConfClientNotifyFunc) peditor_color_value_changed,
		 changeset,
		 G_OBJECT (cp),
		 first_property_name,
		 var_args, NULL);

	va_end (var_args);

	g_signal_connect_swapped (G_OBJECT (cp), "color_set",
				  (GCallback) peditor_color_widget_changed, peditor);

	return peditor;
}

static int
peditor_enum_int_from_string (GType type, const gchar *str, gboolean *use_nick)
{
	GEnumClass *klass;
	GEnumValue *val;
	int ret = -1;

	if (use_nick)
		*use_nick = FALSE;
	
	klass = g_type_class_ref (type);
	val = g_enum_get_value_by_name (klass, str);
	if (!val)
	{
		val = g_enum_get_value_by_nick (klass, str);
		if (use_nick)
			*use_nick = TRUE;
	}
	if (val)
		ret = val->value;

	g_type_class_unref (klass);

	return ret;
}

static gchar*
peditor_enum_string_from_int (GType type, const int index, gboolean use_nick)
{
	GEnumClass *klass;
	GEnumValue *val;
	gchar *ret = NULL;

	klass = g_type_class_ref (type);
	val = g_enum_get_value (klass, index);
	if (val)
	{
		if (val->value_nick && use_nick)
			ret = g_strdup (val->value_nick);
		else
			ret = g_strdup (val->value_name);
	}
	
	g_type_class_unref (klass);

	return ret;
}

static GConfValue*
peditor_enum_conv_to_widget (GConfPropertyEditor *peditor,
			     const GConfValue *value)
{
	GConfValue *ret;
	GConfPropertyEditorEnumData *data = peditor->p->data;
	int index;
	
	if (value->type == GCONF_VALUE_INT)
		return gconf_value_copy (value);
	
	ret = gconf_value_new (GCONF_VALUE_INT);
	
	index = peditor_enum_int_from_string (data->enum_type,
		    		    	      gconf_value_get_string (value),
					      &data->use_nick);
	
	gconf_value_set_int (ret, index);

	return ret;
}

static GConfValue*
peditor_enum_conv_from_widget (GConfPropertyEditor *peditor,
			       const GConfValue *value)
{
	GConfValue *ret;
	GConfPropertyEditorEnumData *data = peditor->p->data;
	gchar *str;

	if (value->type == GCONF_VALUE_STRING)
		return gconf_value_copy (value);

	ret = gconf_value_new (GCONF_VALUE_STRING);
	str = peditor_enum_string_from_int (data->enum_type,
					    gconf_value_get_int (value),
					    data->use_nick);
	gconf_value_set_string (ret, str);
	g_free (str);

	return ret;
}

static void
peditor_select_menu_value_changed (GConfClient         *client,
				   guint                cnxn_id,
				   GConfEntry          *entry,
				   GConfPropertyEditor *peditor) 
{
	GConfValue *value, *value_wid;

	if (peditor->p->changeset != NULL)
		gconf_change_set_remove (peditor->p->changeset, peditor->p->key);

	value = gconf_entry_get_value (entry);

	if (value != NULL) {
		value_wid = peditor->p->conv_to_widget_cb (peditor, value);
		gtk_option_menu_set_history (GTK_OPTION_MENU (peditor->p->ui_control), gconf_value_get_int (value_wid));
		gconf_value_free (value_wid);
	}
}

static void
peditor_select_menu_widget_changed (GConfPropertyEditor *peditor,
				    GtkOptionMenu       *option_menu)
{
	GConfValue *value, *value_wid;

	if (!peditor->p->inited) return;
	value_wid = gconf_value_new (GCONF_VALUE_INT);
	gconf_value_set_int (value_wid, gtk_option_menu_get_history (option_menu));
	value = peditor->p->conv_from_widget_cb (peditor, value_wid);
	peditor_set_gconf_value (peditor, peditor->p->key, value);
	g_signal_emit (peditor, peditor_signals[VALUE_CHANGED], 0, peditor->p->key, value);
	gconf_value_free (value_wid);
	gconf_value_free (value);
}

GObject *
gconf_peditor_new_select_menu (GConfChangeSet *changeset,
			       gchar          *key,
			       GtkWidget      *option_menu,
			       gchar          *first_property_name,
			       ...)
{
	GObject *peditor;
	va_list var_args;

	g_return_val_if_fail (key != NULL, NULL);
	g_return_val_if_fail (option_menu != NULL, NULL);
	g_return_val_if_fail (GTK_IS_OPTION_MENU (option_menu), NULL);

	va_start (var_args, first_property_name);

	peditor = gconf_peditor_new
		(key,
		 (GConfClientNotifyFunc) peditor_select_menu_value_changed,
		 changeset,
		 G_OBJECT (option_menu),
		 first_property_name,
		 var_args, NULL);

	va_end (var_args);

	g_signal_connect_swapped (G_OBJECT (option_menu), "changed",
				  (GCallback) peditor_select_menu_widget_changed, peditor);

	return peditor;
}

GObject *
gconf_peditor_new_select_menu_with_enum	(GConfChangeSet *changeset,
					 gchar 	        *key,
					 GtkWidget      *option_menu,
					 GType          enum_type,
					 gchar          *first_property_name,
					 ...)
{
	GConfPropertyEditor *peditor;
	GConfPropertyEditorEnumData *data;
	va_list var_args;

	g_return_val_if_fail (key != NULL, NULL);
	g_return_val_if_fail (option_menu != NULL, NULL);
	g_return_val_if_fail (GTK_IS_OPTION_MENU (option_menu), NULL);
	g_return_val_if_fail (enum_type != G_TYPE_NONE, NULL);

	data = g_new0 (GConfPropertyEditorEnumData, 1);
	data->enum_type = enum_type;

	va_start (var_args, first_property_name);

	peditor = GCONF_PROPERTY_EDITOR (
		gconf_peditor_new
		(key,
		 (GConfClientNotifyFunc) peditor_select_menu_value_changed,
		 changeset,
		 G_OBJECT (option_menu),
		 first_property_name,
		 var_args,
		 "conv-to-widget-cb",
		 peditor_enum_conv_to_widget,
		 "conv-from-widget-cb",
		 peditor_enum_conv_from_widget,
		 "data",
		 data,
		 "data-free-cb",
		 g_free,
		 NULL
		 ));

	va_end (var_args);
	
	g_signal_connect_swapped (G_OBJECT (option_menu), "changed",
				  (GCallback) peditor_select_menu_widget_changed, peditor);

	return G_OBJECT (peditor);
}

static void
peditor_select_radio_value_changed (GConfClient         *client,
				    guint                cnxn_id,
				    GConfEntry          *entry,
				    GConfPropertyEditor *peditor) 
{
	GSList *group, *link;
	GConfValue *value, *value_wid;

	if (peditor->p->changeset != NULL)
		gconf_change_set_remove (peditor->p->changeset, peditor->p->key);

	value = gconf_entry_get_value (entry);

	if (value != NULL) {
		value_wid = peditor->p->conv_to_widget_cb (peditor, value);
		group = g_slist_copy (gtk_radio_button_get_group (GTK_RADIO_BUTTON (peditor->p->ui_control)));
		group = g_slist_reverse (group);
		link = g_slist_nth (group, gconf_value_get_int (value_wid));
		if (link && link->data)
			gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (link->data), TRUE);
		gconf_value_free (value_wid);
		g_slist_free (group);
	}
}

static void
peditor_select_radio_widget_changed (GConfPropertyEditor *peditor,
				     GtkToggleButton     *tb)
{
	GSList *group;
	GConfValue *value, *value_wid;

	if (!peditor->p->inited) return;
	if (!tb->active) return;

	value_wid = gconf_value_new (GCONF_VALUE_INT);
	group = g_slist_copy (gtk_radio_button_get_group (GTK_RADIO_BUTTON (peditor->p->ui_control)));
	group = g_slist_reverse (group);
	
	gconf_value_set_int (value_wid, g_slist_index (group, tb));
	value = peditor->p->conv_from_widget_cb (peditor, value_wid);

	peditor_set_gconf_value (peditor, peditor->p->key, value);
	g_signal_emit (peditor, peditor_signals[VALUE_CHANGED], 0, peditor->p->key, value);

	gconf_value_free (value_wid);
	gconf_value_free (value);
	g_slist_free (group);
}

GObject *
gconf_peditor_new_select_radio (GConfChangeSet *changeset,
				gchar          *key,
				GSList         *radio_group,
				gchar          *first_property_name,
				...)
{
	GObject *peditor;
	GtkRadioButton *first_button;
	GSList *item;
	va_list var_args;

	g_return_val_if_fail (key != NULL, NULL);
	g_return_val_if_fail (radio_group != NULL, NULL);
	g_return_val_if_fail (radio_group->data != NULL, NULL);
	g_return_val_if_fail (GTK_IS_RADIO_BUTTON (radio_group->data), NULL);

	first_button = GTK_RADIO_BUTTON (radio_group->data);

	va_start (var_args, first_property_name);

	peditor = gconf_peditor_new
		(key,
		 (GConfClientNotifyFunc) peditor_select_radio_value_changed,
		 changeset,
		 G_OBJECT (first_button),
		 first_property_name,
		 var_args, NULL);

	va_end (var_args);

	for (item = radio_group; item != NULL; item = item->next)
		g_signal_connect_swapped (G_OBJECT (item->data), "toggled",
					  (GCallback) peditor_select_radio_widget_changed, peditor);

	return peditor;
}

static void
peditor_numeric_range_value_changed (GConfClient         *client,
				     guint                cnxn_id,
				     GConfEntry          *entry,
				     GConfPropertyEditor *peditor) 
{
	GConfValue *value, *value_wid;

	if (peditor->p->changeset != NULL)
		gconf_change_set_remove (peditor->p->changeset, peditor->p->key);

	value = gconf_entry_get_value (entry);

	if (value != NULL) {
		value_wid = peditor->p->conv_to_widget_cb (peditor, value);

		g_return_if_fail  (value_wid->type == GCONF_VALUE_FLOAT);

		gtk_adjustment_set_value (GTK_ADJUSTMENT (peditor->p->ui_control), gconf_value_get_float (value_wid));
		gconf_value_free (value_wid);
	}
}

static void
peditor_numeric_range_widget_changed (GConfPropertyEditor *peditor,
				      GtkAdjustment       *adjustment)
{
	GConfValue *value, *value_wid;

	if (!peditor->p->inited) return;
	value_wid = gconf_value_new (GCONF_VALUE_FLOAT);
	gconf_value_set_float (value_wid, gtk_adjustment_get_value (adjustment));
	value = peditor->p->conv_from_widget_cb (peditor, value_wid);
	peditor_set_gconf_value (peditor, peditor->p->key, value);
	g_signal_emit (peditor, peditor_signals[VALUE_CHANGED], 0, peditor->p->key, value);
	gconf_value_free (value_wid);
	gconf_value_free (value);
}

GObject *
gconf_peditor_new_numeric_range (GConfChangeSet *changeset,
				 gchar          *key,
				 GtkWidget      *range,
				 gchar          *first_property_name,
				 ...)
{
	GObject *peditor;
	GObject *adjustment;
	va_list var_args;

	g_return_val_if_fail (key != NULL, NULL);
	g_return_val_if_fail (range != NULL, NULL);
	g_return_val_if_fail (GTK_IS_RANGE (range), NULL);

	adjustment = G_OBJECT (gtk_range_get_adjustment (GTK_RANGE (range)));

	va_start (var_args, first_property_name);

	peditor = gconf_peditor_new
		(key,
		 (GConfClientNotifyFunc) peditor_numeric_range_value_changed,
		 changeset,
		 G_OBJECT (adjustment),
		 first_property_name,
		 var_args, NULL);

	va_end (var_args);

	g_signal_connect_swapped (adjustment, "value_changed",
				  (GCallback) peditor_numeric_range_widget_changed, peditor);

	return peditor;
}

static gboolean
guard_get_bool (GConfPropertyEditor *peditor, const GConfValue *value)
{
	if (value->type == GCONF_VALUE_BOOL)
		return gconf_value_get_bool (value);
	else
	{
		GConfPropertyEditorEnumData *data = peditor->p->data;
		int index = peditor_enum_int_from_string (data->enum_type, gconf_value_get_string (value), &data->use_nick);
		return (index != data->enum_val_false);
	}
}

static void
guard_value_changed (GConfPropertyEditor *peditor,
		     const gchar         *key,
		     const GConfValue    *value,
		     GtkWidget           *widget) 
{
	gtk_widget_set_sensitive (widget, guard_get_bool (peditor, value));
}

void
gconf_peditor_widget_set_guard (GConfPropertyEditor *peditor,
				GtkWidget           *widget)
{
	GConfClient *client;
	GConfValue *value;

	g_return_if_fail (peditor != NULL);
	g_return_if_fail (IS_GCONF_PROPERTY_EDITOR (peditor));
	g_return_if_fail (widget != NULL);
	g_return_if_fail (GTK_IS_WIDGET (widget));

	client = gconf_client_get_default ();
	
	value = gconf_client_get (client, peditor->p->key, NULL);
	gtk_widget_set_sensitive (widget, guard_get_bool (peditor, value));
	gconf_value_free (value);

	g_signal_connect (G_OBJECT (peditor), "value-changed", (GCallback) guard_value_changed, widget);
}

GConfValue *
gconf_value_int_to_float (const GConfValue *value)
{
	GConfValue *new_value;

	new_value = gconf_value_new (GCONF_VALUE_FLOAT);
	gconf_value_set_float (new_value, gconf_value_get_int (value));
	return new_value;
}

GConfValue *
gconf_value_float_to_int (const GConfValue *value)
{
	GConfValue *new_value;

	new_value = gconf_value_new (GCONF_VALUE_INT);
	gconf_value_set_int (new_value, gconf_value_get_float (value));
	return new_value;
}

static gint
peditor_font_get_size (const gchar *font_name)
{
	PangoFontDescription *desc;
	int size;
	
	g_return_val_if_fail (font_name != NULL, -1);
	
	desc = pango_font_description_from_string (font_name);
	size = pango_font_description_get_size (desc);
	pango_font_description_free (desc);

	return size / PANGO_SCALE;
}

static void
peditor_font_value_changed (GConfClient         *client,
			     guint                cnxn_id,
			     GConfEntry          *entry,
			     GConfPropertyEditor *peditor) 
{
	GConfValue *value, *value_wid;

	if (peditor->p->changeset != NULL)
		gconf_change_set_remove (peditor->p->changeset, peditor->p->key);

	value = gconf_entry_get_value (entry);

	if (value != NULL) {
		const gchar *font_name;

		value_wid = peditor->p->conv_to_widget_cb (peditor, value);
		font_name = gconf_value_get_string (value_wid);
		g_object_set (G_OBJECT (peditor->p->ui_control),
			      "font_name", font_name,
			      "label-font-size", peditor_font_get_size (font_name),
			      NULL);
		gconf_value_free (value_wid);
	}
}

static void
peditor_font_widget_changed (GConfPropertyEditor *peditor,
			      gchar              *font_name,
			      GnomeFontPicker    *font_picker)
{
	GConfValue *value, *value_wid;

	if (!peditor->p->inited) return;

	g_object_set (G_OBJECT (peditor->p->ui_control),
		      "label-font-size", peditor_font_get_size (font_name),
		      NULL);
	
	value_wid = gconf_value_new (GCONF_VALUE_STRING);
	gconf_value_set_string (value_wid, font_name);
	value = peditor->p->conv_from_widget_cb (peditor, value_wid);

	peditor_set_gconf_value (peditor, peditor->p->key, value);
	g_signal_emit (peditor, peditor_signals[VALUE_CHANGED], 0, peditor->p->key, value);

	gconf_value_free (value_wid);
	gconf_value_free (value);
}

GObject *
gconf_peditor_new_font (GConfChangeSet *changeset,
			gchar *key,
			GtkWidget *font_picker,
			gchar *first_property_name,
			...)
{
	GObject *peditor;
	va_list var_args;

	g_return_val_if_fail (key != NULL, NULL);
	g_return_val_if_fail (font_picker != NULL, NULL);
	g_return_val_if_fail (GNOME_IS_FONT_PICKER (font_picker), NULL);

	va_start (var_args, first_property_name);
	
	peditor = gconf_peditor_new
		(key,
		 (GConfClientNotifyFunc) peditor_font_value_changed,
		 changeset,
		 G_OBJECT (font_picker),
		 first_property_name,
		 var_args, NULL);

	va_end (var_args);

	g_signal_connect_swapped (G_OBJECT (font_picker), "font_set",
				  (GCallback) peditor_font_widget_changed, peditor);

	return peditor;
}

static GConfValue*
peditor_enum_toggle_conv_to_widget (GConfPropertyEditor *peditor,
				    const GConfValue *value)
{
	GConfValue *ret;
	GConfPropertyEditorEnumData *data = peditor->p->data;
	int index;
	
	if (value->type == GCONF_VALUE_BOOL)
		return gconf_value_copy (value);
	
	ret = gconf_value_new (GCONF_VALUE_BOOL);
	
	index = peditor_enum_int_from_string (data->enum_type,
		    		    	      gconf_value_get_string (value),
					      &data->use_nick);
	gconf_value_set_bool (ret, (index != data->enum_val_false));

	return ret;
}

static GConfValue*
peditor_enum_toggle_conv_from_widget (GConfPropertyEditor *peditor,
				      const GConfValue *value)
{
	GConfValue *ret;
	GConfPropertyEditorEnumData *data = peditor->p->data;
	gchar *str;
	int index;

	if (value->type == GCONF_VALUE_STRING)
		return gconf_value_copy (value);

	ret = gconf_value_new (GCONF_VALUE_STRING);
	if (gconf_value_get_bool (value))
		index = data->enum_val_true_fn (peditor, data->enum_val_true_fn_data);
	else
		index = data->enum_val_false;

	str = peditor_enum_string_from_int (data->enum_type, index, data->use_nick);
	gconf_value_set_string (ret, str);
	g_free (str);

	return ret;
}

GObject *
gconf_peditor_new_enum_toggle  (GConfChangeSet 	 *changeset,
				gchar			 *key,
				GtkWidget		 *checkbox,
				GType			 enum_type,
				GConfPEditorGetValueFn   val_true_fn,
				guint			 val_false,
				gpointer		 data,
				gchar 			 *first_property_name,
				...)
{
	GConfPropertyEditor *peditor;
	GConfPropertyEditorEnumData *enum_data;
	va_list var_args;

	g_return_val_if_fail (key != NULL, NULL);
	g_return_val_if_fail (checkbox != NULL, NULL);
	g_return_val_if_fail (GTK_IS_TOGGLE_BUTTON (checkbox), NULL);

	enum_data = g_new0 (GConfPropertyEditorEnumData, 1);
	enum_data->enum_type = enum_type;
	enum_data->enum_val_true_fn = val_true_fn;
	enum_data->enum_val_true_fn_data = data;
	enum_data->enum_val_false = val_false;

	va_start (var_args, first_property_name);

	peditor = GCONF_PROPERTY_EDITOR (
		gconf_peditor_new
		(key,
		 (GConfClientNotifyFunc) peditor_boolean_value_changed,
		 changeset,
		 G_OBJECT (checkbox),
		 first_property_name,
		 var_args,
		 "conv-to-widget-cb",
		 peditor_enum_toggle_conv_to_widget,
		 "conv-from-widget-cb",
		 peditor_enum_toggle_conv_from_widget,
		 "data",
		 enum_data,
		 "data-free-cb",
		 g_free,
		 NULL));

	va_end (var_args);

	g_signal_connect_swapped (G_OBJECT (checkbox), "toggled",
				  (GCallback) peditor_boolean_widget_changed, peditor);

	return G_OBJECT (peditor);
}

gboolean
peditor_image_set_filename (GConfPropertyEditor *peditor, const gchar *filename)
{
	GdkPixbuf *pixbuf = NULL;
	GdkPixbuf *scaled;
	GtkImage *image = NULL;
	const int scale = 100;
	gchar *message = NULL;
	GList *l;
	
	if (!g_file_test (filename, G_FILE_TEST_EXISTS))
	{
		message = g_strdup_printf (_("Couldn't find the file '%s'.\n\nPlease make "
					      "sure it exists and try again, " 
					      "or choose a different background picture."),
					   filename);

	}
	else if (!(pixbuf = gdk_pixbuf_new_from_file (filename, NULL)))
	{
		message = g_strdup_printf (_("I don't know how to open the file '%s'.\n"
					     "Perhaps it's "
					     "a kind of picture that is not yet supported.\n\n"
					     "Please select a different picture instead."),
					   filename);
	}

	if (message)
	{
		GtkWidget *box;

		box = gtk_message_dialog_new (NULL,
					      GTK_DIALOG_MODAL,
					      GTK_MESSAGE_ERROR,
					      GTK_BUTTONS_OK,
					      message);
		gtk_dialog_run (GTK_DIALOG (box));
		gtk_widget_destroy (box);
		g_free (message);

		return FALSE;
	}

	scaled = preview_file_selection_intelligent_scale (pixbuf,
							   scale);

	if (GTK_IS_IMAGE (GTK_BIN (peditor->p->ui_control)->child))
		image = GTK_IMAGE (GTK_BIN (peditor->p->ui_control)->child);
	else
	{
		for (l = gtk_container_get_children (GTK_CONTAINER (GTK_BIN (peditor->p->ui_control)->child)); l != NULL; l = l->next)
		{	
			if (GTK_IS_IMAGE (l->data))
				image = GTK_IMAGE (l->data);
			else if (GTK_IS_LABEL (l->data))
			{
				gchar *base = g_path_get_basename (filename);
				gtk_label_set_text (GTK_LABEL (l->data), base);
				g_free (base);
			}
		}
	}
	gtk_image_set_from_pixbuf (image, scaled);
	g_object_unref (G_OBJECT (pixbuf));
	g_object_unref (G_OBJECT (scaled));

	return TRUE;
}

void
peditor_image_fsel_ok_cb (GtkFileSelection *fsel, gpointer data)
{
	GConfValue *value, *value_wid;
	GConfPropertyEditor *peditor;
	const gchar *filename;

	peditor = g_object_get_data (G_OBJECT (fsel), "peditor");

	if (!peditor->p->inited)
		return;

	filename = gtk_file_selection_get_filename (fsel);
	if (!(filename && peditor_image_set_filename (peditor, filename)))
		return;

	value_wid = gconf_value_new (GCONF_VALUE_STRING);
	gconf_value_set_string (value_wid, gtk_file_selection_get_filename (fsel));
	value = peditor->p->conv_from_widget_cb (peditor, value_wid);

	peditor_set_gconf_value (peditor, peditor->p->key, value);
	g_signal_emit (peditor, peditor_signals[VALUE_CHANGED], 0, peditor->p->key, value);

	gconf_value_free (value_wid);
	gconf_value_free (value);
	gtk_widget_destroy (GTK_WIDGET (fsel));
}

void
peditor_image_clicked_cb (GConfPropertyEditor *peditor, GtkButton *button)
{
	GConfValue *value = NULL, *value_wid;
	const gchar *filename;
	GtkWidget *fsel;

	fsel = preview_file_selection_new (_("Please select an image."), TRUE);
	
	/* need the current filename */
	if (peditor->p->changeset)
		gconf_change_set_check_value (peditor->p->changeset, peditor->p->key, &value);

	if (value)
	{
		/* the one we got is not a copy */
		value = gconf_value_copy (value);
	}
	else
	{
		GConfClient *client = gconf_client_get_default ();
		value = gconf_client_get (client, peditor->p->key, NULL);
	}
	
	value_wid = peditor->p->conv_to_widget_cb (peditor, value);
	filename = gconf_value_get_string (value_wid);

	if (filename && strcmp (filename, ""))
		gtk_file_selection_set_filename (GTK_FILE_SELECTION (fsel), filename);

	g_object_set_data (G_OBJECT (fsel), "peditor", peditor);

	g_signal_connect_swapped  (G_OBJECT (GTK_FILE_SELECTION (fsel)->ok_button),
				   "clicked",
				   (GCallback) peditor_image_fsel_ok_cb,
				   fsel);
	
	g_signal_connect_swapped  (G_OBJECT (GTK_FILE_SELECTION (fsel)->cancel_button),
				   "clicked",
				   (GCallback) gtk_widget_destroy,
				   fsel);

	if (gtk_grab_get_current ())
		gtk_grab_add (fsel);
	
	gtk_widget_show (fsel);

	gconf_value_free (value);
	gconf_value_free (value_wid);
}

static void
peditor_image_value_changed (GConfClient         *client,
			     guint                cnxn_id,
			     GConfEntry          *entry,
			     GConfPropertyEditor *peditor) 
{
	GConfValue *value, *value_wid;

	if (peditor->p->changeset != NULL)
		gconf_change_set_remove (peditor->p->changeset, peditor->p->key);

	value = gconf_entry_get_value (entry);

	if (value != NULL) {
		const gchar *filename;

		value_wid = peditor->p->conv_to_widget_cb (peditor, value);
		filename = gconf_value_get_string (value_wid);
		peditor_image_set_filename (peditor, filename);
		gconf_value_free (value_wid);
	}
}

GObject *
gconf_peditor_new_image (GConfChangeSet	  *changeset,
			 gchar	          *key,
			 GtkWidget	  *button,
			 gchar		  *first_property_name,
			 ...)
{
	GObject *peditor;
	va_list var_args;

	g_return_val_if_fail (key != NULL, NULL);
	g_return_val_if_fail (button != NULL, NULL);
	g_return_val_if_fail (GTK_IS_BUTTON (button), NULL);

	va_start (var_args, first_property_name);
	
	peditor = gconf_peditor_new
		(key,
		 (GConfClientNotifyFunc) peditor_image_value_changed,
		 changeset,
		 G_OBJECT (button),
		 first_property_name,
		 var_args, NULL);

	va_end (var_args);

	g_signal_connect_swapped (G_OBJECT (button), "clicked",
				  (GCallback) peditor_image_clicked_cb, peditor);

	return peditor;
}

GObject *
gconf_peditor_new_select_radio_with_enum (GConfChangeSet *changeset,
					  gchar		 *key,
					  GSList 	 *radio_group,
					  GType 	 enum_type,
					  gchar          *first_property_name,
					  ...)
{
	GConfPropertyEditor *peditor;
	GConfPropertyEditorEnumData *enum_data;
	GtkRadioButton *first_button;
	GSList *item;
	va_list var_args;

	g_return_val_if_fail (key != NULL, NULL);
	g_return_val_if_fail (radio_group != NULL, NULL);
	g_return_val_if_fail (radio_group->data != NULL, NULL);
	g_return_val_if_fail (GTK_IS_RADIO_BUTTON (radio_group->data), NULL);

	enum_data = g_new0 (GConfPropertyEditorEnumData, 1);
	enum_data->enum_type = enum_type;

	first_button = GTK_RADIO_BUTTON (radio_group->data);

	va_start (var_args, first_property_name);

	peditor = GCONF_PROPERTY_EDITOR (
		gconf_peditor_new
		(key,
		 (GConfClientNotifyFunc) peditor_select_radio_value_changed,
		 changeset,
		 G_OBJECT (first_button),
		 first_property_name,
		 var_args,
		 "conv-to-widget-cb",
		 peditor_enum_conv_to_widget,
		 "conv-from-widget-cb",
		 peditor_enum_conv_from_widget,
		 "data",
		 enum_data,
		 "data-free-cb",
		 g_free,
		 NULL));

	va_end (var_args);

	for (item = radio_group; item != NULL; item = item->next)
		g_signal_connect_swapped (G_OBJECT (item->data), "toggled",
					  (GCallback) peditor_select_radio_widget_changed, peditor);

	return G_OBJECT (peditor);
}

