/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*-
 *
 * Copyright 2009-2010  Red Hat, Inc,
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
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
 * Written by: Matthias Clasen <mclasen@redhat.com>
 */

#include <gdk/gdkkeysyms.h>
#include "um-editable-entry.h"

#define EMPTY_TEXT "\xe2\x80\x94"

struct _UmEditableEntryPrivate {
        GtkNotebook *notebook;
        GtkLabel    *label;
        GtkButton   *button;
        GtkEntry    *entry;

        gchar *text;
        gboolean editable;
        gint weight;
        gboolean weight_set;
        gdouble scale;
        gboolean scale_set;
};

#define UM_EDITABLE_ENTRY_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE ((obj), UM_TYPE_EDITABLE_ENTRY, UmEditableEntryPrivate))

enum {
        PROP_0,
        PROP_TEXT,
        PROP_EDITABLE,
        PROP_SCALE,
        PROP_SCALE_SET,
        PROP_WEIGHT,
        PROP_WEIGHT_SET
};

enum {
        EDITING_DONE,
        LAST_SIGNAL
};

static guint signals [LAST_SIGNAL] = { 0, };

G_DEFINE_TYPE (UmEditableEntry, um_editable_entry, GTK_TYPE_ALIGNMENT);

void
um_editable_entry_set_text (UmEditableEntry *e,
                             const gchar    *text)
{
        UmEditableEntryPrivate *priv;
        gchar *tmp;
        GtkWidget *label;

        priv = e->priv;

        tmp = g_strdup (text);
        g_free (priv->text);
        priv->text = tmp;

        gtk_entry_set_text (priv->entry, tmp);

        if (tmp == NULL || tmp[0] == '\0')
                tmp = EMPTY_TEXT;

        gtk_label_set_text (priv->label, tmp);
        label = gtk_bin_get_child (GTK_BIN (priv->button));
        gtk_label_set_text (GTK_LABEL (label), tmp);

        g_object_notify (G_OBJECT (e), "text");
}

const gchar *
um_editable_entry_get_text (UmEditableEntry *e)
{
        return e->priv->text;
}

void
um_editable_entry_set_editable (UmEditableEntry *e,
                                 gboolean        editable)
{
        UmEditableEntryPrivate *priv;

        priv = e->priv;

        if (priv->editable != editable) {
                priv->editable = editable;

                gtk_notebook_set_current_page (priv->notebook, editable ? 1 : 0);

                g_object_notify (G_OBJECT (e), "editable");
        }
}

gboolean
um_editable_entry_get_editable (UmEditableEntry *e)
{
        return e->priv->editable;
}

static void
update_entry_font (GtkWidget        *widget,
                   GtkStyle         *previous_style,
                   UmEditableEntry *e)
{
        UmEditableEntryPrivate *priv = e->priv;
        PangoFontDescription *desc;
        GtkStyle *style;
        gint size;

        if (!priv->weight_set && !priv->scale_set)
                return;

        g_signal_handlers_block_by_func (widget, update_entry_font, e);

        gtk_widget_modify_font (widget, NULL);

        style = gtk_widget_get_style (widget);
        desc = pango_font_description_copy (style->font_desc);
        if (priv->weight_set)
                pango_font_description_set_weight (desc, priv->weight);
        if (priv->scale_set) {
                size = pango_font_description_get_size (desc);
                pango_font_description_set_size (desc, priv->scale * size);
        }
        gtk_widget_modify_font (widget, desc);

        pango_font_description_free (desc);

        g_signal_handlers_unblock_by_func (widget, update_entry_font, e);
}

static void
update_fonts (UmEditableEntry *e)
{
        PangoAttrList *attrs;
        PangoAttribute *attr;
        GtkWidget *label;

        UmEditableEntryPrivate *priv = e->priv;

        attrs = pango_attr_list_new ();
        if (priv->scale_set) {
                attr = pango_attr_scale_new (priv->scale);
                pango_attr_list_insert (attrs, attr);
        }
        if (priv->weight_set) {
                attr = pango_attr_weight_new (priv->weight);
                pango_attr_list_insert (attrs, attr);
        }

        gtk_label_set_attributes (priv->label, attrs);

        label = gtk_bin_get_child (GTK_BIN (priv->button));
        gtk_label_set_attributes (GTK_LABEL (label), attrs);

        pango_attr_list_unref (attrs);

        update_entry_font ((GtkWidget *)priv->entry, NULL, e);
}

void
um_editable_entry_set_weight (UmEditableEntry *e,
                               gint            weight)
{
        UmEditableEntryPrivate *priv = e->priv;

        if (priv->weight == weight && priv->weight_set)
                return;

        priv->weight = weight;
        priv->weight_set = TRUE;

        update_fonts (e);

        g_object_notify (G_OBJECT (e), "weight");
        g_object_notify (G_OBJECT (e), "weight-set");
}

gint
um_editable_entry_get_weight (UmEditableEntry *e)
{
        return e->priv->weight;
}

void
um_editable_entry_set_scale (UmEditableEntry *e,
                              gdouble         scale)
{
        UmEditableEntryPrivate *priv = e->priv;

        if (priv->scale == scale && priv->scale_set)
                return;

        priv->scale = scale;
        priv->scale_set = TRUE;

        update_fonts (e);

        g_object_notify (G_OBJECT (e), "scale");
        g_object_notify (G_OBJECT (e), "scale-set");
}

gdouble
um_editable_entry_get_scale (UmEditableEntry *e)
{
        return e->priv->scale;
}

static void
um_editable_entry_set_property (GObject      *object,
                                guint         prop_id,
                                const GValue *value,
                                GParamSpec   *pspec)
{
        UmEditableEntry *e = UM_EDITABLE_ENTRY (object);

        switch (prop_id) {
        case PROP_TEXT:
                um_editable_entry_set_text (e, g_value_get_string (value));
                break;
        case PROP_EDITABLE:
                um_editable_entry_set_editable (e, g_value_get_boolean (value));
                break;
        case PROP_WEIGHT:
                um_editable_entry_set_weight (e, g_value_get_int (value));
                break;
        case PROP_WEIGHT_SET:
                e->priv->weight_set = g_value_get_boolean (value);
                break;
        case PROP_SCALE:
                um_editable_entry_set_scale (e, g_value_get_double (value));
                break;
        case PROP_SCALE_SET:
                e->priv->scale_set = g_value_get_boolean (value);
                break;
        default:
                G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
                break;
        }
}

static void
um_editable_entry_get_property (GObject    *object,
                                guint       prop_id,
                                GValue     *value,
                                GParamSpec *pspec)
{
        UmEditableEntry *e = UM_EDITABLE_ENTRY (object);

        switch (prop_id) {
        case PROP_TEXT:
                g_value_set_string (value,
                                    um_editable_entry_get_text (e));
                break;
        case PROP_EDITABLE:
                g_value_set_boolean (value,
                                     um_editable_entry_get_editable (e));
                break;
        case PROP_WEIGHT:
                g_value_set_int (value,
                                 um_editable_entry_get_weight (e));
                break;
        case PROP_WEIGHT_SET:
                g_value_set_boolean (value, e->priv->weight_set);
                break;
        case PROP_SCALE:
                g_value_set_double (value,
                                    um_editable_entry_get_scale (e));
                break;
        case PROP_SCALE_SET:
                g_value_set_boolean (value, e->priv->scale_set);
                break;
        default:
                G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
                break;
        }
}

static void
um_editable_entry_finalize (GObject *object)
{
        UmEditableEntry *e = (UmEditableEntry*)object;

        g_free (e->priv->text);

        G_OBJECT_CLASS (um_editable_entry_parent_class)->finalize (object);
}

static void
um_editable_entry_class_init (UmEditableEntryClass *class)
{
        GObjectClass *object_class;

        object_class = G_OBJECT_CLASS (class);

        object_class->set_property = um_editable_entry_set_property;
        object_class->get_property = um_editable_entry_get_property;
        object_class->finalize = um_editable_entry_finalize;

        signals[EDITING_DONE] =
                g_signal_new ("editing-done",
                              G_TYPE_FROM_CLASS (class),
                              G_SIGNAL_RUN_LAST,
                              G_STRUCT_OFFSET (UmEditableEntryClass, editing_done),
                              NULL, NULL,
                              g_cclosure_marshal_VOID__VOID,
                              G_TYPE_NONE, 0);

        g_object_class_install_property (object_class, PROP_TEXT,
                g_param_spec_string ("text",
                                     "Text", "The text of the button",
                                     NULL,
                                     G_PARAM_READWRITE));

        g_object_class_install_property (object_class, PROP_EDITABLE,
                g_param_spec_boolean ("editable",
                                      "Editable", "Whether the text can be edited",
                                      FALSE,
                                      G_PARAM_READWRITE));

        g_object_class_install_property (object_class, PROP_WEIGHT,
                g_param_spec_int ("weight",
                                  "Font Weight", "The font weight to use",
                                  0, G_MAXINT, PANGO_WEIGHT_NORMAL,
                                  G_PARAM_READWRITE));

        g_object_class_install_property (object_class, PROP_WEIGHT_SET,
                g_param_spec_boolean ("weight-set",
                                      "Font Weight Set", "Whether a font weight is set",
                                      FALSE,
                                      G_PARAM_READWRITE));

        g_object_class_install_property (object_class, PROP_SCALE,
                g_param_spec_double ("scale",
                                     "Font Scale", "The font scale to use",
                                     0.0, G_MAXDOUBLE, 1.0,
                                     G_PARAM_READWRITE));

        g_object_class_install_property (object_class, PROP_SCALE_SET,
                g_param_spec_boolean ("scale-set",
                                      "Font Scale Set", "Whether a font scale is set",
                                      FALSE,
                                      G_PARAM_READWRITE));

        g_type_class_add_private (class, sizeof (UmEditableEntryPrivate));
}

static void
start_editing (UmEditableEntry *e)
{
        gtk_notebook_set_current_page (e->priv->notebook, 2);
}

static void
stop_editing (UmEditableEntry *e)
{
        um_editable_entry_set_text (e, gtk_entry_get_text (e->priv->entry));
        gtk_notebook_set_current_page (e->priv->notebook, 1);
        g_signal_emit (e, signals[EDITING_DONE], 0);
}

static void
cancel_editing (UmEditableEntry *e)
{
        gtk_entry_set_text (e->priv->entry, um_editable_entry_get_text (e));
        gtk_notebook_set_current_page (e->priv->notebook, 1);
}

static void
button_clicked (GtkWidget       *widget,
                UmEditableEntry *e)
{
        start_editing (e);
}

static void
entry_activated (GtkWidget       *widget,
                 UmEditableEntry *e)
{
        stop_editing (e);
}

static gboolean
entry_focus_out (GtkWidget       *widget,
                 GdkEventFocus   *event,
                 UmEditableEntry *e)
{
        stop_editing (e);
        return FALSE;
}

static gboolean
entry_key_press (GtkWidget       *widget,
                 GdkEventKey     *event,
                 UmEditableEntry *e)
{
        if (event->keyval == GDK_KEY_Escape) {
                cancel_editing (e);
        }
        return FALSE;
}

static void
update_button_padding (GtkWidget       *widget,
                       GtkAllocation   *allocation,
                       UmEditableEntry *e)
{
        UmEditableEntryPrivate *priv = e->priv;
        GtkAllocation alloc;
        gint offset;
        gint pad;

        gtk_widget_get_allocation (gtk_widget_get_parent (widget), &alloc);

        offset = allocation->x - alloc.x;

        gtk_misc_get_padding  (GTK_MISC (priv->label), &pad, NULL);
        if (offset != pad)
                gtk_misc_set_padding (GTK_MISC (priv->label), offset, 0);
}

static void
um_editable_entry_init (UmEditableEntry *e)
{
        UmEditableEntryPrivate *priv;

        priv = e->priv = UM_EDITABLE_ENTRY_GET_PRIVATE (e);

        priv->weight = PANGO_WEIGHT_NORMAL;
        priv->weight_set = FALSE;
        priv->scale = 1.0;
        priv->scale_set = FALSE;

        priv->notebook = (GtkNotebook*)gtk_notebook_new ();
        gtk_notebook_set_show_tabs (priv->notebook, FALSE);
        gtk_notebook_set_show_border (priv->notebook, FALSE);

        priv->label = (GtkLabel*)gtk_label_new (EMPTY_TEXT);
        gtk_misc_set_alignment (GTK_MISC (priv->label), 0.0, 0.5);
        gtk_notebook_append_page (priv->notebook, (GtkWidget*)priv->label, NULL);

        priv->button = (GtkButton*)gtk_button_new_with_label (EMPTY_TEXT);
        gtk_widget_set_receives_default ((GtkWidget*)priv->button, TRUE);
        gtk_button_set_relief (priv->button, GTK_RELIEF_NONE);
        gtk_button_set_alignment (priv->button, 0.0, 0.5);
        gtk_notebook_append_page (priv->notebook, (GtkWidget*)priv->button, NULL);
        g_signal_connect (priv->button, "clicked", G_CALLBACK (button_clicked), e);

        priv->entry = (GtkEntry*)gtk_entry_new ();
        gtk_notebook_append_page (priv->notebook, (GtkWidget*)priv->entry, NULL);

        g_signal_connect (priv->entry, "activate", G_CALLBACK (entry_activated), e);
        g_signal_connect (priv->entry, "focus-out-event", G_CALLBACK (entry_focus_out), e);
        g_signal_connect (priv->entry, "key-press-event", G_CALLBACK (entry_key_press), e);
        g_signal_connect (priv->entry, "style-set", G_CALLBACK (update_entry_font), e);
        g_signal_connect (gtk_bin_get_child (GTK_BIN (priv->button)), "size-allocate", G_CALLBACK (update_button_padding), e);

        gtk_container_add (GTK_CONTAINER (e), (GtkWidget*)priv->notebook);

        gtk_widget_show ((GtkWidget*)priv->notebook);
        gtk_widget_show ((GtkWidget*)priv->label);
        gtk_widget_show ((GtkWidget*)priv->button);
        gtk_widget_show ((GtkWidget*)priv->entry);

        gtk_notebook_set_current_page (priv->notebook, 0);
}

GtkWidget *
um_editable_entry_new (void)
{
        return (GtkWidget *) g_object_new (UM_TYPE_EDITABLE_ENTRY, NULL);
}
