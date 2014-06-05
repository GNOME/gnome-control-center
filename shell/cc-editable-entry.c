/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*-
 *
 * Copyright 2009-2010  Red Hat, Inc,
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
 *
 * Written by: Matthias Clasen <mclasen@redhat.com>
 */

#include <gdk/gdkkeysyms.h>
#include "cc-editable-entry.h"

#define EMPTY_TEXT "\xe2\x80\x94"

struct _CcEditableEntryPrivate {
        GtkStack    *stack;
        GtkLabel    *label;
        GtkButton   *button;
        GtkEntry    *entry;

        gchar *text;
        gboolean editable;
        gboolean selectable;
        gint weight;
        gboolean weight_set;
        gdouble scale;
        gboolean scale_set;
        gint width_chars;
        gint max_width_chars;
        PangoEllipsizeMode ellipsize;

        gboolean in_stop_editing;
};

#define CC_EDITABLE_ENTRY_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE ((obj), CC_TYPE_EDITABLE_ENTRY, CcEditableEntryPrivate))

enum {
        PROP_0,
        PROP_TEXT,
        PROP_EDITABLE,
        PROP_SELECTABLE,
        PROP_SCALE,
        PROP_SCALE_SET,
        PROP_WEIGHT,
        PROP_WEIGHT_SET,
        PROP_WIDTH_CHARS,
        PROP_MAX_WIDTH_CHARS,
        PROP_ELLIPSIZE
};

enum {
        EDITING_DONE,
        LAST_SIGNAL
};

#define PAGE_LABEL "_label"
#define PAGE_BUTTON "_button"
#define PAGE_ENTRY "_entry"

static guint signals [LAST_SIGNAL] = { 0, };

G_DEFINE_TYPE (CcEditableEntry, cc_editable_entry, GTK_TYPE_ALIGNMENT);

void
cc_editable_entry_set_text (CcEditableEntry *e,
                             const gchar    *text)
{
        CcEditableEntryPrivate *priv;
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
cc_editable_entry_get_text (CcEditableEntry *e)
{
        return e->priv->text;
}

void
cc_editable_entry_set_editable (CcEditableEntry *e,
                                 gboolean        editable)
{
        CcEditableEntryPrivate *priv;

        priv = e->priv;

        if (priv->editable != editable) {
                priv->editable = editable;

                gtk_stack_set_visible_child_name (e->priv->stack, editable ? PAGE_BUTTON : PAGE_LABEL);

                g_object_notify (G_OBJECT (e), "editable");
        }
}

gboolean
cc_editable_entry_get_editable (CcEditableEntry *e)
{
        return e->priv->editable;
}

void
cc_editable_entry_set_selectable (CcEditableEntry *e,
                                  gboolean         selectable)
{
        CcEditableEntryPrivate *priv;

        priv = e->priv;

        if (priv->selectable != selectable) {
                priv->selectable = selectable;

                gtk_label_set_selectable (priv->label, selectable);

                g_object_notify (G_OBJECT (e), "selectable");
        }
}

gboolean
cc_editable_entry_get_selectable (CcEditableEntry *e)
{
        return e->priv->selectable;
}


static void
update_fonts (CcEditableEntry *e)
{
        PangoAttrList *attrs;
        PangoAttribute *attr;
        GtkWidget *label;

        CcEditableEntryPrivate *priv = e->priv;

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
        gtk_entry_set_attributes (priv->entry, attrs);

        pango_attr_list_unref (attrs);
}

void
cc_editable_entry_set_weight (CcEditableEntry *e,
                               gint            weight)
{
        CcEditableEntryPrivate *priv = e->priv;

        if (priv->weight == weight && priv->weight_set)
                return;

        priv->weight = weight;
        priv->weight_set = TRUE;

        update_fonts (e);

        g_object_notify (G_OBJECT (e), "weight");
        g_object_notify (G_OBJECT (e), "weight-set");
}

gint
cc_editable_entry_get_weight (CcEditableEntry *e)
{
        return e->priv->weight;
}

void
cc_editable_entry_set_scale (CcEditableEntry *e,
                              gdouble         scale)
{
        CcEditableEntryPrivate *priv = e->priv;

        if (priv->scale == scale && priv->scale_set)
                return;

        priv->scale = scale;
        priv->scale_set = TRUE;

        update_fonts (e);

        g_object_notify (G_OBJECT (e), "scale");
        g_object_notify (G_OBJECT (e), "scale-set");
}

gdouble
cc_editable_entry_get_scale (CcEditableEntry *e)
{
        return e->priv->scale;
}

void
cc_editable_entry_set_width_chars (CcEditableEntry *e,
                                   gint             n_chars)
{
        CcEditableEntryPrivate *priv = e->priv;
        GtkWidget *label;

        if (priv->width_chars != n_chars) {
                label = gtk_bin_get_child (GTK_BIN (priv->button));
                gtk_entry_set_width_chars (priv->entry, n_chars);
                gtk_label_set_width_chars (priv->label, n_chars);
                gtk_label_set_width_chars (GTK_LABEL (label), n_chars);

                priv->width_chars = n_chars;
                g_object_notify (G_OBJECT (e), "width-chars");
                gtk_widget_queue_resize (GTK_WIDGET (priv->entry));
                gtk_widget_queue_resize (GTK_WIDGET (priv->label));
                gtk_widget_queue_resize (GTK_WIDGET (label));
        }
}

gint
cc_editable_entry_get_width_chars (CcEditableEntry *e)
{
        return e->priv->width_chars;
}

void
cc_editable_entry_set_max_width_chars (CcEditableEntry *e,
                                       gint             n_chars)
{
        CcEditableEntryPrivate *priv = e->priv;
        GtkWidget *label;

        if (priv->max_width_chars != n_chars) {
                label = gtk_bin_get_child (GTK_BIN (priv->button));
                gtk_label_set_max_width_chars (priv->label, n_chars);
                gtk_label_set_max_width_chars (GTK_LABEL (label), n_chars);

                priv->max_width_chars = n_chars;
                g_object_notify (G_OBJECT (e), "max-width-chars");
                gtk_widget_queue_resize (GTK_WIDGET (priv->entry));
                gtk_widget_queue_resize (GTK_WIDGET (priv->label));
                gtk_widget_queue_resize (GTK_WIDGET (label));
        }
}

gint
cc_editable_entry_get_max_width_chars (CcEditableEntry *e)
{
        return e->priv->max_width_chars;
}

void
cc_editable_entry_set_ellipsize (CcEditableEntry   *e,
                                 PangoEllipsizeMode mode)
{
        CcEditableEntryPrivate *priv = e->priv;
        GtkWidget *label;

        if ((PangoEllipsizeMode) priv->ellipsize != mode) {
                label = gtk_bin_get_child (GTK_BIN (priv->button));
                gtk_label_set_ellipsize (priv->label, mode);
                gtk_label_set_ellipsize (GTK_LABEL (label), mode);

                priv->ellipsize = mode;
                g_object_notify (G_OBJECT (e), "ellipsize");
                gtk_widget_queue_resize (GTK_WIDGET (priv->label));
                gtk_widget_queue_resize (GTK_WIDGET (label));
        }
}

PangoEllipsizeMode
cc_editable_entry_get_ellipsize (CcEditableEntry *e)
{
        return e->priv->ellipsize;
}

static void
cc_editable_entry_set_property (GObject      *object,
                                guint         prop_id,
                                const GValue *value,
                                GParamSpec   *pspec)
{
        CcEditableEntry *e = CC_EDITABLE_ENTRY (object);

        switch (prop_id) {
        case PROP_TEXT:
                cc_editable_entry_set_text (e, g_value_get_string (value));
                break;
        case PROP_EDITABLE:
                cc_editable_entry_set_editable (e, g_value_get_boolean (value));
                break;
        case PROP_SELECTABLE:
                cc_editable_entry_set_selectable (e, g_value_get_boolean (value));
                break;
        case PROP_WEIGHT:
                cc_editable_entry_set_weight (e, g_value_get_int (value));
                break;
        case PROP_WEIGHT_SET:
                e->priv->weight_set = g_value_get_boolean (value);
                break;
        case PROP_SCALE:
                cc_editable_entry_set_scale (e, g_value_get_double (value));
                break;
        case PROP_SCALE_SET:
                e->priv->scale_set = g_value_get_boolean (value);
                break;
        case PROP_WIDTH_CHARS:
                cc_editable_entry_set_width_chars (e, g_value_get_int (value));
                break;
        case PROP_MAX_WIDTH_CHARS:
                cc_editable_entry_set_max_width_chars (e, g_value_get_int (value));
                break;
        case PROP_ELLIPSIZE:
                cc_editable_entry_set_ellipsize (e, g_value_get_enum (value));
                break;
        default:
                G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
                break;
        }
}

static void
cc_editable_entry_get_property (GObject    *object,
                                guint       prop_id,
                                GValue     *value,
                                GParamSpec *pspec)
{
        CcEditableEntry *e = CC_EDITABLE_ENTRY (object);

        switch (prop_id) {
        case PROP_TEXT:
                g_value_set_string (value,
                                    cc_editable_entry_get_text (e));
                break;
        case PROP_EDITABLE:
                g_value_set_boolean (value,
                                     cc_editable_entry_get_editable (e));
                break;
        case PROP_SELECTABLE:
                g_value_set_boolean (value,
                                     cc_editable_entry_get_selectable (e));
                break;
        case PROP_WEIGHT:
                g_value_set_int (value,
                                 cc_editable_entry_get_weight (e));
                break;
        case PROP_WEIGHT_SET:
                g_value_set_boolean (value, e->priv->weight_set);
                break;
        case PROP_SCALE:
                g_value_set_double (value,
                                    cc_editable_entry_get_scale (e));
                break;
        case PROP_SCALE_SET:
                g_value_set_boolean (value, e->priv->scale_set);
                break;
        case PROP_WIDTH_CHARS:
                g_value_set_int (value,
                                 cc_editable_entry_get_width_chars (e));
                break;
        case PROP_MAX_WIDTH_CHARS:
                g_value_set_int (value,
                                 cc_editable_entry_get_max_width_chars (e));
                break;
        case PROP_ELLIPSIZE:
                g_value_set_enum (value,
                                  cc_editable_entry_get_ellipsize (e));
                break;
        default:
                G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
                break;
        }
}

static void
cc_editable_entry_finalize (GObject *object)
{
        CcEditableEntry *e = (CcEditableEntry*)object;

        g_free (e->priv->text);

        G_OBJECT_CLASS (cc_editable_entry_parent_class)->finalize (object);
}

static void
cc_editable_entry_class_init (CcEditableEntryClass *class)
{
        GObjectClass *object_class;

        object_class = G_OBJECT_CLASS (class);

        object_class->set_property = cc_editable_entry_set_property;
        object_class->get_property = cc_editable_entry_get_property;
        object_class->finalize = cc_editable_entry_finalize;

        signals[EDITING_DONE] =
                g_signal_new ("editing-done",
                              G_TYPE_FROM_CLASS (class),
                              G_SIGNAL_RUN_LAST,
                              G_STRUCT_OFFSET (CcEditableEntryClass, editing_done),
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

        g_object_class_install_property (object_class, PROP_SELECTABLE,
                g_param_spec_boolean ("selectable",
                                      "Selectable", "Whether the text can be selected by mouse",
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

        g_object_class_install_property (object_class, PROP_WIDTH_CHARS,
                g_param_spec_int ("width-chars",
                                  "Width In Characters", "The desired width of the editable entry, in characters",
                                  -1, G_MAXINT, -1,
                                  G_PARAM_READWRITE));

        g_object_class_install_property (object_class, PROP_MAX_WIDTH_CHARS,
                g_param_spec_int ("max-width-chars",
                                  "Maximum Width In Characters","The desired maximum width of the editable entry, in characters",
                                  -1, G_MAXINT, -1,
                                  G_PARAM_READWRITE));

        g_object_class_install_property (object_class, PROP_ELLIPSIZE,
                g_param_spec_enum ("ellipsize",
                                   "Ellipsize", "The preferred place to ellipsize the string, if the editable entry does not have enough room to display the entire string",
                                   PANGO_TYPE_ELLIPSIZE_MODE, PANGO_ELLIPSIZE_NONE,
                                   G_PARAM_READWRITE));

        g_type_class_add_private (class, sizeof (CcEditableEntryPrivate));
}

static void
start_editing (CcEditableEntry *e)
{
        gtk_stack_set_visible_child_name (e->priv->stack, PAGE_ENTRY);
        gtk_widget_grab_focus (GTK_WIDGET (e->priv->entry));
}

static void
stop_editing (CcEditableEntry *e)
{
        gboolean has_focus;

        /* Avoid launching another "editing-done" signal
         * caused by the notebook page change */
        if (e->priv->in_stop_editing)
                return;

        e->priv->in_stop_editing = TRUE;
        has_focus = gtk_widget_has_focus (GTK_WIDGET (e->priv->entry));
        gtk_stack_set_visible_child_name (e->priv->stack, PAGE_BUTTON);
        if (has_focus)
                gtk_widget_grab_focus (GTK_WIDGET (e->priv->button));

        cc_editable_entry_set_text (e, gtk_entry_get_text (e->priv->entry));
        g_signal_emit (e, signals[EDITING_DONE], 0);
        e->priv->in_stop_editing = FALSE;
}

static void
cancel_editing (CcEditableEntry *e)
{
        gtk_entry_set_text (e->priv->entry, cc_editable_entry_get_text (e));
        gtk_stack_set_visible_child_name (e->priv->stack, PAGE_BUTTON);
        gtk_widget_grab_focus (GTK_WIDGET (e->priv->button));
}

static void
button_clicked (GtkWidget       *widget,
                CcEditableEntry *e)
{
        start_editing (e);
}

static void
entry_activated (GtkWidget       *widget,
                 CcEditableEntry *e)
{
        stop_editing (e);
}

static gboolean
entry_focus_out (GtkWidget       *widget,
                 GdkEventFocus   *event,
                 CcEditableEntry *e)
{
        stop_editing (e);
        return FALSE;
}

static gboolean
entry_key_press (GtkWidget       *widget,
                 GdkEventKey     *event,
                 CcEditableEntry *e)
{
        if (event->keyval == GDK_KEY_Escape) {
                cancel_editing (e);
        }
        return FALSE;
}

static void
update_button_padding (CcEditableEntry *e)
{
        CcEditableEntryPrivate *priv = e->priv;
        GtkStyleContext *context;
        GtkStateFlags state;
        GtkBorder padding, border;

        context = gtk_widget_get_style_context (GTK_WIDGET (priv->button));
        state = gtk_style_context_get_state (context);

        gtk_style_context_get_padding (context, state, &padding);
        gtk_style_context_get_border (context, state, &border);

        gtk_misc_set_padding (GTK_MISC (priv->label), padding.left + border.left, 0);
}

static void
cc_editable_entry_init (CcEditableEntry *e)
{
        CcEditableEntryPrivate *priv;

        priv = e->priv = CC_EDITABLE_ENTRY_GET_PRIVATE (e);

        priv->weight = PANGO_WEIGHT_NORMAL;
        priv->weight_set = FALSE;
        priv->scale = 1.0;
        priv->scale_set = FALSE;
        priv->width_chars = -1;
        priv->max_width_chars = -1;
        priv->ellipsize = PANGO_ELLIPSIZE_NONE;
        priv->stack = GTK_STACK (gtk_stack_new ());

        /* Label */
        priv->label = (GtkLabel*)gtk_label_new (EMPTY_TEXT);
        gtk_misc_set_alignment (GTK_MISC (priv->label), 0.0, 0.5);
        gtk_stack_add_named (priv->stack, GTK_WIDGET (priv->label), PAGE_LABEL);

        /* Button */
        priv->button = (GtkButton*)gtk_button_new_with_label (EMPTY_TEXT);
        gtk_widget_set_receives_default ((GtkWidget*)priv->button, TRUE);
        gtk_button_set_relief (priv->button, GTK_RELIEF_NONE);
        gtk_button_set_alignment (priv->button, 0.0, 0.5);
        gtk_stack_add_named (priv->stack, GTK_WIDGET (priv->button), PAGE_BUTTON);
        g_signal_connect (priv->button, "clicked", G_CALLBACK (button_clicked), e);

        /* Entry */
        priv->entry = (GtkEntry*)gtk_entry_new ();
        gtk_stack_add_named (priv->stack, GTK_WIDGET (priv->entry), PAGE_ENTRY);

        g_signal_connect (priv->entry, "activate", G_CALLBACK (entry_activated), e);
        g_signal_connect (priv->entry, "focus-out-event", G_CALLBACK (entry_focus_out), e);
        g_signal_connect (priv->entry, "key-press-event", G_CALLBACK (entry_key_press), e);

        update_button_padding (e);

        gtk_container_add (GTK_CONTAINER (e), (GtkWidget*)priv->stack);

        gtk_widget_show ((GtkWidget*)priv->stack);
        gtk_widget_show ((GtkWidget*)priv->label);
        gtk_widget_show ((GtkWidget*)priv->button);
        gtk_widget_show ((GtkWidget*)priv->entry);

        gtk_stack_set_visible_child_name (e->priv->stack, PAGE_LABEL);
}

GtkWidget *
cc_editable_entry_new (void)
{
        return (GtkWidget *) g_object_new (CC_TYPE_EDITABLE_ENTRY, NULL);
}
