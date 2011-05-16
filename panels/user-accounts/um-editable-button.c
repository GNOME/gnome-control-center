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
#include "um-editable-button.h"

#define EMPTY_TEXT "\xe2\x80\x94"

struct _UmEditableButtonPrivate {
        GtkNotebook *notebook;
        GtkLabel    *label;
        GtkButton   *button;

        gchar *text;
        gboolean editable;
        gint weight;
        gboolean weight_set;
        gdouble scale;
        gboolean scale_set;
};

#define UM_EDITABLE_BUTTON_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE ((obj), UM_TYPE_EDITABLE_BUTTON, UmEditableButtonPrivate))

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
        START_EDITING,
        ACTIVATE,
        LAST_SIGNAL
};

static guint signals [LAST_SIGNAL] = { 0, };

G_DEFINE_TYPE (UmEditableButton, um_editable_button, GTK_TYPE_ALIGNMENT);

void
um_editable_button_set_text (UmEditableButton *button,
                             const gchar      *text)
{
        UmEditableButtonPrivate *priv;
        gchar *tmp;
        GtkWidget *label;

        priv = button->priv;

        tmp = g_strdup (text);
        g_free (priv->text);
        priv->text = tmp;

        if (tmp == NULL || tmp[0] == '\0')
                tmp = EMPTY_TEXT;

        gtk_label_set_text (priv->label, tmp);
        label = gtk_bin_get_child (GTK_BIN (priv->button));
        gtk_label_set_text (GTK_LABEL (label), tmp);

        g_object_notify (G_OBJECT (button), "text");
}

const gchar *
um_editable_button_get_text (UmEditableButton *button)
{
        return button->priv->text;
}

void
um_editable_button_set_editable (UmEditableButton *button,
                                 gboolean          editable)
{
        UmEditableButtonPrivate *priv;

        priv = button->priv;

        if (priv->editable != editable) {
                priv->editable = editable;

                gtk_notebook_set_current_page (priv->notebook, editable ? 1 : 0);

                g_object_notify (G_OBJECT (button), "editable");
        }
}

gboolean
um_editable_button_get_editable (UmEditableButton *button)
{
        return button->priv->editable;
}

static void
update_fonts (UmEditableButton *button)
{
        PangoAttrList *attrs;
        PangoAttribute *attr;
        GtkWidget *label;

        UmEditableButtonPrivate *priv = button->priv;

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
}

void
um_editable_button_set_weight (UmEditableButton *button,
                               gint              weight)
{
        UmEditableButtonPrivate *priv = button->priv;

        if (priv->weight == weight && priv->weight_set)
                return;

        priv->weight = weight;
        priv->weight_set = TRUE;

        update_fonts (button);

        g_object_notify (G_OBJECT (button), "weight");
        g_object_notify (G_OBJECT (button), "weight-set");
}

gint
um_editable_button_get_weight (UmEditableButton *button)
{
        return button->priv->weight;
}

void
um_editable_button_set_scale (UmEditableButton *button,
                              gdouble           scale)
{
        UmEditableButtonPrivate *priv = button->priv;

        if (priv->scale == scale && priv->scale_set)
                return;

        priv->scale = scale;
        priv->scale_set = TRUE;

        update_fonts (button);

        g_object_notify (G_OBJECT (button), "scale");
        g_object_notify (G_OBJECT (button), "scale-set");
}

gdouble
um_editable_button_get_scale (UmEditableButton *button)
{
        return button->priv->scale;
}

static void
um_editable_button_set_property (GObject      *object,
                                 guint         prop_id,
                                 const GValue *value,
                                 GParamSpec   *pspec)
{
        UmEditableButton *button = UM_EDITABLE_BUTTON (object);

        switch (prop_id) {
        case PROP_TEXT:
                um_editable_button_set_text (button, g_value_get_string (value));
                break;
        case PROP_EDITABLE:
                um_editable_button_set_editable (button, g_value_get_boolean (value));
                break;
        case PROP_WEIGHT:
                um_editable_button_set_weight (button, g_value_get_int (value));
                break;
        case PROP_WEIGHT_SET:
                button->priv->weight_set = g_value_get_boolean (value);
                break;
        case PROP_SCALE:
                um_editable_button_set_scale (button, g_value_get_double (value));
                break;
        case PROP_SCALE_SET:
                button->priv->scale_set = g_value_get_boolean (value);
                break;
        default:
                G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
                break;
        }
}

static void
um_editable_button_get_property (GObject    *object,
                                 guint       prop_id,
                                 GValue     *value,
                                 GParamSpec *pspec)
{
        UmEditableButton *button = UM_EDITABLE_BUTTON (object);

        switch (prop_id) {
        case PROP_TEXT:
                g_value_set_string (value,
                                    um_editable_button_get_text (button));
                break;
        case PROP_EDITABLE:
                g_value_set_boolean (value,
                                     um_editable_button_get_editable (button));
                break;
        case PROP_WEIGHT:
                g_value_set_int (value,
                                 um_editable_button_get_weight (button));
                break;
        case PROP_WEIGHT_SET:
                g_value_set_boolean (value,
                                     button->priv->weight_set);
                break;
        case PROP_SCALE:
                g_value_set_double (value,
                                    um_editable_button_get_scale (button));
                break;
        case PROP_SCALE_SET:
                g_value_set_boolean (value,
                                     button->priv->scale_set);
                break;
        default:
                G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
                break;
        }
}

static void
um_editable_button_finalize (GObject *object)
{
        UmEditableButton *button = (UmEditableButton*)object;

        g_free (button->priv->text);

        G_OBJECT_CLASS (um_editable_button_parent_class)->finalize (object);
}

static void um_editable_button_activate (UmEditableButton *button);

static void
um_editable_button_class_init (UmEditableButtonClass *class)
{
        GObjectClass *object_class;
        GtkWidgetClass *widget_class;

        object_class = G_OBJECT_CLASS (class);
        widget_class = GTK_WIDGET_CLASS (class);

        object_class->set_property = um_editable_button_set_property;
        object_class->get_property = um_editable_button_get_property;
        object_class->finalize = um_editable_button_finalize;

        signals[START_EDITING] =
                g_signal_new ("start-editing",
                              G_TYPE_FROM_CLASS (class),
                              G_SIGNAL_RUN_LAST,
                              G_STRUCT_OFFSET (UmEditableButtonClass, start_editing),
                              NULL, NULL,
                              g_cclosure_marshal_VOID__VOID,
                              G_TYPE_NONE, 0);
        signals[ACTIVATE] =
                g_signal_new ("activate",
                              G_TYPE_FROM_CLASS (class),
                              G_SIGNAL_RUN_FIRST | G_SIGNAL_ACTION,
                              G_STRUCT_OFFSET (UmEditableButtonClass, activate),
                              NULL, NULL,
                              g_cclosure_marshal_VOID__VOID,
                              G_TYPE_NONE, 0);
        widget_class->activate_signal = signals[ACTIVATE];
        class->activate = um_editable_button_activate;


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

        g_type_class_add_private (class, sizeof (UmEditableButtonPrivate));
}

static void
start_editing (UmEditableButton *button)
{
        g_signal_emit (button, signals[START_EDITING], 0);
}

static void
um_editable_button_activate (UmEditableButton *button)
{
        UmEditableButtonPrivate *priv = button->priv;

        if (priv->editable) {
                gtk_widget_grab_focus (GTK_WIDGET (button->priv->button));
        }
}

static void
button_clicked (GtkWidget        *widget,
                UmEditableButton *button)
{
        start_editing (button);
}

static void
update_button_padding (GtkWidget        *widget,
                       GtkAllocation    *allocation,
                       UmEditableButton *button)
{
        UmEditableButtonPrivate *priv = button->priv;
        GtkAllocation parent_allocation;
        gint offset;
        gint pad;

        gtk_widget_get_allocation (gtk_widget_get_parent (widget), &parent_allocation);

        offset = allocation->x - parent_allocation.x;

        gtk_misc_get_padding  (GTK_MISC (priv->label), &pad, NULL);
        if (offset != pad)
                gtk_misc_set_padding (GTK_MISC (priv->label), offset, 0);
}

static void
um_editable_button_init (UmEditableButton *button)
{
        UmEditableButtonPrivate *priv;

        priv = button->priv = UM_EDITABLE_BUTTON_GET_PRIVATE (button);

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
        g_signal_connect (priv->button, "clicked", G_CALLBACK (button_clicked), button);
        g_signal_connect (gtk_bin_get_child (GTK_BIN (priv->button)), "size-allocate", G_CALLBACK (update_button_padding), button);

        gtk_container_add (GTK_CONTAINER (button), (GtkWidget*)priv->notebook);

        gtk_widget_show ((GtkWidget*)priv->notebook);
        gtk_widget_show ((GtkWidget*)priv->label);
        gtk_widget_show ((GtkWidget*)priv->button);

        gtk_notebook_set_current_page (priv->notebook, 0);
}

GtkWidget *
um_editable_button_new (void)
{
        return (GtkWidget *) g_object_new (UM_TYPE_EDITABLE_BUTTON, NULL);
}
