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

#include "gconf-property-editor.h"
#include "gconf-property-editor-marshal.h"

enum {
	VALUE_CHANGED,
	LAST_SIGNAL
};

enum {
	PROP_0,
	PROP_KEY,
	PROP_CALLBACK,
	PROP_CHANGESET,
	PROP_OBJECT
};

struct _GConfPropertyEditorPrivate 
{
	gchar          *key;
	guint           handler_id;
	GConfChangeSet *changeset;
};

static guint peditor_signals[LAST_SIGNAL];

static GObjectClass *parent_class;

static void gconf_property_editor_init        (GConfPropertyEditor *gconf_property_editor, GConfPropertyEditorClass *class);
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

static void
gconf_property_editor_init (GConfPropertyEditor *gconf_property_editor, GConfPropertyEditorClass *class)
{
	gconf_property_editor->p = g_new0 (GConfPropertyEditorPrivate, 1);
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
		(object_class, PROP_OBJECT,
		 g_param_spec_object ("object",
				      _("Destroy notify object"),
				      _("Destroy the property editor when this object gets destroyed"),
				      G_TYPE_OBJECT,
				      G_PARAM_WRITABLE));
	g_object_class_install_property
		(object_class, PROP_CHANGESET,
		 g_param_spec_pointer ("changeset",
				       _("Change set"),
				       _("GConf change set containing data to be forwarded to the gconf client on apply"),
				       G_PARAM_READWRITE));

	peditor_signals[VALUE_CHANGED] =
		g_signal_new ("value-changed",
			      G_TYPE_FROM_CLASS (object_class), 0,
			      G_STRUCT_OFFSET (GConfPropertyEditorClass, value_changed),
			      NULL, NULL,
			      (GSignalCMarshaller) gconf_property_editor_marshal_VOID__STRING_POINTER,
			      G_TYPE_NONE, 2, G_TYPE_STRING, G_TYPE_POINTER);

	parent_class = G_OBJECT_CLASS
		(g_type_class_ref (G_TYPE_OBJECT));
}

static void
gconf_property_editor_set_prop (GObject *object, guint prop_id, const GValue *value, GParamSpec *pspec) 
{
	GConfPropertyEditor *peditor;
	GConfClient *client;
	GConfNotifyFunc cb;
	GObject *det_obj;

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
		peditor->p->handler_id =
			gconf_client_notify_add (client, peditor->p->key,
						 (GConfClientNotifyFunc) cb,
						 peditor, NULL, NULL);
		break;

	case PROP_CHANGESET:
		peditor->p->changeset = g_value_get_pointer (value);
		break;

	case PROP_OBJECT:
		det_obj = g_value_get_object (value);
		g_object_weak_ref (det_obj, (GWeakNotify) g_object_unref, object);
		break;

	default:
		g_warning ("Bad argument set");
		break;
	}
}

static void
gconf_property_editor_get_prop (GObject *object, guint prop_id, GValue *value, GParamSpec *pspec) 
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

	g_free (gconf_property_editor->p);

	G_OBJECT_CLASS (parent_class)->finalize (object);
}

const gchar *
gconf_property_editor_get_key (GConfPropertyEditor *peditor)
{
	return peditor->p->key;
}

static void
peditor_boolean_value_changed (GConfClient *client, guint cnxn_id, GConfEntry *entry, GConfPropertyEditor *peditor) 
{
	GtkToggleButton *tb;
	GConfValue *value;

	tb = g_object_get_data (G_OBJECT (peditor), "toggle-button");

	value = gconf_entry_get_value (entry);

	if (value != NULL && gtk_toggle_button_get_active (tb) != gconf_value_get_bool (value))
		gtk_toggle_button_set_active (tb, gconf_value_get_bool (value));
}

static void
peditor_boolean_widget_changed (GConfPropertyEditor *peditor, GtkToggleButton *tb)
{
	GConfValue *value;

	gconf_change_set_set_bool (peditor->p->changeset, peditor->p->key, gtk_toggle_button_get_active (tb));
	gconf_change_set_check_value (peditor->p->changeset, peditor->p->key, &value);
	g_signal_emit (peditor, peditor_signals[VALUE_CHANGED], 0, peditor->p->key, value);
}

GObject *
gconf_peditor_new_boolean (GConfChangeSet *changeset, gchar *key, GtkWidget *checkbox)
{
	GObject *peditor;
	GConfClient *client;
	GConfEntry *gconf_entry;

	peditor = g_object_new (gconf_property_editor_get_type (),
				"key", key,
				"callback", peditor_boolean_value_changed,
				"changeset", changeset,
				"object", checkbox,
				NULL);

	g_signal_connect_swapped (G_OBJECT (checkbox), "toggled",
				  (GCallback) peditor_boolean_widget_changed, peditor);
	g_object_set_data (peditor, "toggle-button", checkbox);

	client = gconf_client_get_default ();
	gconf_entry = gconf_client_get_entry (client, key, NULL, TRUE, NULL);
	peditor_boolean_value_changed (client, 0, gconf_entry, GCONF_PROPERTY_EDITOR (peditor));

	return peditor;
}

static void
peditor_string_value_changed (GConfClient *client, guint cnxn_id, GConfEntry *entry, GConfPropertyEditor *peditor) 
{
	GtkEntry *gtk_entry;
	GConfValue *value;

	gtk_entry = g_object_get_data (G_OBJECT (peditor), "entry");

	value = gconf_entry_get_value (entry);

	if (value != NULL && strcmp (gtk_entry_get_text (gtk_entry), gconf_value_get_string (value))) {
		gconf_change_set_remove (peditor->p->changeset, peditor->p->key);
		gtk_entry_set_text (gtk_entry, gconf_value_get_string (value));
	}
}

static void
peditor_string_widget_changed (GConfPropertyEditor *peditor, GtkEntry *entry)
{
	GConfValue *value;

	gconf_change_set_set_string (peditor->p->changeset, peditor->p->key, gtk_entry_get_text (entry));
	gconf_change_set_check_value (peditor->p->changeset, peditor->p->key, &value);
	g_signal_emit (peditor, peditor_signals[VALUE_CHANGED], 0, peditor->p->key, value);
}

GObject *
gconf_peditor_new_string (GConfChangeSet *changeset, gchar *key, GtkWidget *entry)
{
	GObject *peditor;
	GConfClient *client;
	GConfEntry *gconf_entry;

	peditor = g_object_new (gconf_property_editor_get_type (),
				"key", key,
				"callback", peditor_string_value_changed,
				"changeset", changeset,
				"object", entry,
				NULL);

	g_signal_connect_swapped (G_OBJECT (entry), "changed",
				  (GCallback) peditor_string_widget_changed, peditor);
	g_object_set_data (peditor, "entry", entry);

	client = gconf_client_get_default ();
	gconf_entry = gconf_client_get_entry (client, key, NULL, TRUE, NULL);
	peditor_string_value_changed (client, 0, gconf_entry, GCONF_PROPERTY_EDITOR (peditor));

	return peditor;
}

GObject *
gconf_peditor_new_filename (GConfChangeSet *changeset, gchar *key, GtkWidget *file_entry)
{
	gconf_peditor_new_string (changeset, key, gnome_file_entry_gtk_entry (GNOME_FILE_ENTRY (file_entry)));
}

static void
peditor_color_value_changed (GConfClient *client, guint cnxn_id, GConfEntry *entry, GConfPropertyEditor *peditor) 
{
	GnomeColorPicker *cp;
	GConfValue *value;
	GdkColor color;
	guint16 r, g, b, a;

	cp = g_object_get_data (G_OBJECT (peditor), "cp");
	value = gconf_entry_get_value (entry);

	if (value != NULL) {
		gdk_color_parse (gconf_value_get_string (value), &color);
		gnome_color_picker_get_i16 (cp, &r, &g, &b, &a);

		if (r != color.red || g != color.green || b != color.blue) {
			gconf_change_set_remove (peditor->p->changeset, peditor->p->key);
			gnome_color_picker_set_i16 (cp, color.red, color.green, color.blue, 65535);
		}
	}
}

static void
peditor_color_widget_changed (GConfPropertyEditor *peditor, guint r, guint g, guint b, guint a, GnomeColorPicker *cp)
{
	gchar *str;
	GConfValue *value;

	str = g_strdup_printf ("#%02x%02x%02x", r >> 8, g >> 8, b >> 8);
	gconf_change_set_set_string (peditor->p->changeset, peditor->p->key, str);

	gconf_change_set_check_value (peditor->p->changeset, peditor->p->key, &value);
	g_signal_emit (peditor, peditor_signals[VALUE_CHANGED], 0, peditor->p->key, value);
}

GObject *
gconf_peditor_new_color (GConfChangeSet *changeset, gchar *key, GtkWidget *cp)
{
	GObject *peditor;
	GConfClient *client;
	GConfEntry *gconf_entry;

	peditor = g_object_new (gconf_property_editor_get_type (),
				"key", key,
				"callback", peditor_color_value_changed,
				"changeset", changeset,
				"object", cp,
				NULL);

	g_signal_connect_swapped (G_OBJECT (cp), "color_set",
				  (GCallback) peditor_color_widget_changed, peditor);
	g_object_set_data (peditor, "cp", cp);

	client = gconf_client_get_default ();
	gconf_entry = gconf_client_get_entry (client, key, NULL, TRUE, NULL);
	peditor_color_value_changed (client, 0, gconf_entry, GCONF_PROPERTY_EDITOR (peditor));

	return peditor;
}

static void
peditor_select_menu_value_changed (GConfClient *client, guint cnxn_id, GConfEntry *entry, GConfPropertyEditor *peditor) 
{
	GtkOptionMenu *option_menu;
	GtkMenu *menu;
	GList *item;
	GConfValue *value;

	gconf_change_set_remove (peditor->p->changeset, peditor->p->key);
	value = gconf_entry_get_value (entry);

	if (value != NULL) {
		option_menu = g_object_get_data (G_OBJECT (peditor), "option-menu");
		gtk_option_menu_set_history (option_menu, gconf_value_get_int (value));
	}
}

static void
peditor_select_menu_widget_changed (GConfPropertyEditor *peditor, GtkOptionMenu *option_menu)
{
	gint idx;
	GConfValue *value;

	idx = gtk_option_menu_get_history (option_menu);

	gconf_change_set_set_int (peditor->p->changeset, peditor->p->key, idx);

	gconf_change_set_check_value (peditor->p->changeset, peditor->p->key, &value);
	g_signal_emit (peditor, peditor_signals[VALUE_CHANGED], 0, peditor->p->key, value);
}

GObject *
gconf_peditor_new_select_menu (GConfChangeSet *changeset, gchar *key, GtkWidget *option_menu)
{
	GObject *peditor;
	GConfClient *client;
	GConfEntry *gconf_entry;
	GList *item;

	peditor = g_object_new (gconf_property_editor_get_type (),
				"key", key,
				"callback", peditor_select_menu_value_changed,
				"changeset", changeset,
				"object", option_menu,
				NULL);

	g_signal_connect_swapped (G_OBJECT (option_menu), "changed",
				  (GCallback) peditor_select_menu_widget_changed, peditor);

	g_object_set_data (peditor, "option-menu", option_menu);

	client = gconf_client_get_default ();
	gconf_entry = gconf_client_get_entry (client, key, NULL, TRUE, NULL);
	peditor_select_menu_value_changed (client, 0, gconf_entry, GCONF_PROPERTY_EDITOR (peditor));

	return peditor;
}

static void
peditor_select_radio_value_changed (GConfClient *client, guint cnxn_id, GConfEntry *entry, GConfPropertyEditor *peditor) 
{
	GSList *group;
	GConfValue *value;

	gconf_change_set_remove (peditor->p->changeset, peditor->p->key);
	value = gconf_entry_get_value (entry);

	if (value != NULL) {
		group = g_object_get_data (G_OBJECT (peditor), "group");
		group = g_slist_nth (group, gconf_value_get_int (value));
		gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (group->data), TRUE);
	}
}

static void
peditor_select_radio_widget_changed (GConfPropertyEditor *peditor, GtkToggleButton *tb)
{
	GSList *group;
	gint idx;
	GConfValue *value;

	group = g_object_get_data (G_OBJECT (peditor), "group");
	idx = g_slist_index (group, tb);

	gconf_change_set_set_int (peditor->p->changeset, peditor->p->key, idx);

	gconf_change_set_check_value (peditor->p->changeset, peditor->p->key, &value);
	g_signal_emit (peditor, peditor_signals[VALUE_CHANGED], 0, peditor->p->key, value);
}

GObject *
gconf_peditor_new_select_radio (GConfChangeSet *changeset, gchar *key, GSList *radio_group)
{
	GObject *peditor;
	GConfClient *client;
	GConfEntry *gconf_entry;
	GSList *item;

	peditor = g_object_new (gconf_property_editor_get_type (),
				"key", key,
				"callback", peditor_select_radio_value_changed,
				"changeset", changeset,
				"object", radio_group,
				NULL);

	for (item = radio_group; item != NULL; item = item->next)
		g_signal_connect_swapped (G_OBJECT (item->data), "toggled",
					  (GCallback) peditor_select_radio_widget_changed, peditor);

	g_object_set_data (peditor, "group", radio_group);

	client = gconf_client_get_default ();
	gconf_entry = gconf_client_get_entry (client, key, NULL, TRUE, NULL);
	peditor_select_radio_value_changed (client, 0, gconf_entry, GCONF_PROPERTY_EDITOR (peditor));

	return peditor;
}

static void
peditor_int_range_value_changed (GConfClient *client, guint cnxn_id, GConfEntry *entry, GConfPropertyEditor *peditor) 
{
	GConfValue *value;
	GtkAdjustment *adjustment;

	gconf_change_set_remove (peditor->p->changeset, peditor->p->key);
	value = gconf_entry_get_value (entry);

	if (value != NULL) {
		adjustment = g_object_get_data (G_OBJECT (peditor), "adjustment");
		gtk_adjustment_set_value (adjustment, gconf_value_get_int (value));
	}
}

static void
peditor_int_range_widget_changed (GConfPropertyEditor *peditor, GtkAdjustment *adjustment)
{
	GConfValue *value;

	gconf_change_set_set_int (peditor->p->changeset, peditor->p->key,
				  gtk_adjustment_get_value (adjustment));

	gconf_change_set_check_value (peditor->p->changeset, peditor->p->key, &value);
	g_signal_emit (peditor, peditor_signals[VALUE_CHANGED], 0, peditor->p->key, value);
}

GObject *
gconf_peditor_new_int_range (GConfChangeSet *changeset, gchar *key, GtkWidget *range)
{
	GObject *peditor;
	GConfClient *client;
	GConfEntry *gconf_entry;
	GSList *item;
	GtkAdjustment *adjustment;

	peditor = g_object_new (gconf_property_editor_get_type (),
				"key", key,
				"callback", peditor_int_range_value_changed,
				"changeset", changeset,
				"object", range,
				NULL);

	adjustment = gtk_range_get_adjustment (GTK_RANGE (range));
	g_object_set_data (peditor, "adjustment", adjustment);

	g_signal_connect_swapped (G_OBJECT (adjustment), "changed",
				  (GCallback) peditor_int_range_widget_changed, peditor);

	client = gconf_client_get_default ();
	gconf_entry = gconf_client_get_entry (client, key, NULL, TRUE, NULL);
	peditor_int_range_value_changed (client, 0, gconf_entry, GCONF_PROPERTY_EDITOR (peditor));

	return peditor;
}

static void
guard_value_changed (GConfPropertyEditor *peditor, const gchar *key, const GConfValue *value, GtkWidget *widget) 
{
	gtk_widget_set_sensitive (widget, gconf_value_get_bool (value));
}

void
gconf_peditor_widget_set_guard (GConfPropertyEditor *peditor, GtkWidget *widget)
{
	GConfClient *client;

	client = gconf_client_get_default ();
	gtk_widget_set_sensitive (widget, gconf_client_get_bool (client, peditor->p->key, NULL));

	g_signal_connect (G_OBJECT (peditor), "value-changed", (GCallback) guard_value_changed, widget);
}

