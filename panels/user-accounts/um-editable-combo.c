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
#include "um-editable-combo.h"

#define EMPTY_TEXT "\xe2\x80\x94"

struct _UmEditableComboPrivate {
        GtkStack    *stack;
        GtkLabel    *label;
        GtkButton   *button;
        GtkComboBox *combo;
        GtkWidget   *toplevel;

        gint active;
        gint editable;
        gint text_column;
};

#define UM_EDITABLE_COMBO_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE ((obj), UM_TYPE_EDITABLE_COMBO, UmEditableComboPrivate))

enum {
        PROP_0,
        PROP_EDITABLE,
        PROP_MODEL,
        PROP_TEXT_COLUMN
};

enum {
        EDITING_DONE,
        ACTIVATE,
        LAST_SIGNAL
};

#define PAGE_LABEL "_label"
#define PAGE_BUTTON "_button"
#define PAGE_COMBO "_combo"

static guint signals [LAST_SIGNAL] = { 0, };

G_DEFINE_TYPE (UmEditableCombo, um_editable_combo, GTK_TYPE_ALIGNMENT);

void
um_editable_combo_set_editable (UmEditableCombo *combo,
                                gboolean         editable)
{
        UmEditableComboPrivate *priv;

        priv = combo->priv;

        if (priv->editable != editable) {
                priv->editable = editable;

                gtk_stack_set_visible_child_name (combo->priv->stack, editable ? PAGE_BUTTON : PAGE_LABEL);

                g_object_notify (G_OBJECT (combo), "editable");
        }
}

gboolean
um_editable_combo_get_editable (UmEditableCombo *combo)
{
        return combo->priv->editable;
}

void
um_editable_combo_set_model (UmEditableCombo *combo,
                             GtkTreeModel    *model)
{
        gtk_combo_box_set_model (combo->priv->combo, model);

        g_object_notify (G_OBJECT (combo), "model");
}

GtkTreeModel *
um_editable_combo_get_model (UmEditableCombo *combo)
{
        return gtk_combo_box_get_model (combo->priv->combo);
}

void
um_editable_combo_set_text_column (UmEditableCombo *combo,
                                   gint             text_column)
{
        UmEditableComboPrivate *priv = combo->priv;
        GList *cells;

        if (priv->text_column == text_column)
                return;

        priv->text_column = text_column;

        cells = gtk_cell_layout_get_cells (GTK_CELL_LAYOUT (priv->combo));
        gtk_cell_layout_set_attributes (GTK_CELL_LAYOUT (priv->combo),
                                        cells->data,
                                        "text", text_column,
                                        NULL);
        g_list_free (cells);

        g_object_notify (G_OBJECT (combo), "text-column");
}

gint
um_editable_combo_get_text_column (UmEditableCombo *combo)
{
        return combo->priv->text_column;
}

void
um_editable_combo_set_active (UmEditableCombo *combo,
                              gint             active)
{
        GtkTreeModel *model;
        GtkTreePath *path;
        GtkTreeIter iter;

        if (active == -1)
                um_editable_combo_set_active_iter (combo, NULL);
        else {
                model = gtk_combo_box_get_model (combo->priv->combo);
                path = gtk_tree_path_new_from_indices (active, -1);
                gtk_tree_model_get_iter (model, &iter, path);
                gtk_tree_path_free (path);
                um_editable_combo_set_active_iter (combo, &iter);
        }
}

void
um_editable_combo_set_active_iter (UmEditableCombo *combo,
                                   GtkTreeIter     *iter)
{
        UmEditableComboPrivate *priv = combo->priv;
        GtkWidget *label;
        gchar *text;
        GtkTreeModel *model;

        gtk_combo_box_set_active_iter (priv->combo, iter);
        priv->active = gtk_combo_box_get_active (priv->combo);

        if (priv->text_column == -1)
                return;

        if (iter) {
                model = gtk_combo_box_get_model (priv->combo);
                gtk_tree_model_get (model, iter, priv->text_column, &text, -1);
        }
        else {
                text = g_strdup (EMPTY_TEXT);
        }

        gtk_label_set_text (priv->label, text);
        label = gtk_bin_get_child ((GtkBin*)priv->button);
        gtk_label_set_text (GTK_LABEL (label), text);

        g_free (text);
}

gboolean
um_editable_combo_get_active_iter (UmEditableCombo *combo,
                                   GtkTreeIter     *iter)
{
        return gtk_combo_box_get_active_iter (combo->priv->combo, iter);
}

gint
um_editable_combo_get_active (UmEditableCombo *combo)
{
        return combo->priv->active;
}

static void
um_editable_combo_set_property (GObject      *object,
                                guint         prop_id,
                                const GValue *value,
                                GParamSpec   *pspec)
{
        UmEditableCombo *combo = UM_EDITABLE_COMBO (object);

        switch (prop_id) {
        case PROP_EDITABLE:
                um_editable_combo_set_editable (combo, g_value_get_boolean (value));
                break;
        case PROP_MODEL:
                um_editable_combo_set_model (combo, g_value_get_object (value));
                break;
        case PROP_TEXT_COLUMN:
                um_editable_combo_set_text_column (combo, g_value_get_int (value));
                break;
        default:
                G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
                break;
        }
}

static void
um_editable_combo_get_property (GObject    *object,
                                guint       prop_id,
                                GValue     *value,
                                GParamSpec *pspec)
{
        UmEditableCombo *combo = UM_EDITABLE_COMBO (object);

        switch (prop_id) {
        case PROP_EDITABLE:
                g_value_set_boolean (value,
                                     um_editable_combo_get_editable (combo));
                break;
        case PROP_MODEL:
                g_value_set_object (value,
                                    um_editable_combo_get_model (combo));
                break;
        case PROP_TEXT_COLUMN:
                g_value_set_int (value,
                                 um_editable_combo_get_text_column (combo));
                break;
        default:
                G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
                break;
        }
}

static void um_editable_combo_activate (UmEditableCombo *combo);

static void
um_editable_combo_class_init (UmEditableComboClass *class)
{
        GObjectClass *object_class;
        GtkWidgetClass *widget_class;

        object_class = G_OBJECT_CLASS (class);
        widget_class = GTK_WIDGET_CLASS (class);

        object_class->set_property = um_editable_combo_set_property;
        object_class->get_property = um_editable_combo_get_property;

        signals[EDITING_DONE] =
                g_signal_new ("editing-done",
                              G_TYPE_FROM_CLASS (class),
                              G_SIGNAL_RUN_LAST,
                              G_STRUCT_OFFSET (UmEditableComboClass, editing_done),
                              NULL, NULL,
                              g_cclosure_marshal_VOID__VOID,
                              G_TYPE_NONE, 0);
        signals[ACTIVATE] =
                g_signal_new ("activate",
                              G_TYPE_FROM_CLASS (class),
                              G_SIGNAL_RUN_FIRST | G_SIGNAL_ACTION,
                              G_STRUCT_OFFSET (UmEditableComboClass, activate),
                              NULL, NULL,
                              g_cclosure_marshal_VOID__VOID,
                              G_TYPE_NONE, 0);
        widget_class->activate_signal = signals[ACTIVATE];
        class->activate = um_editable_combo_activate;

        g_object_class_install_property (object_class, PROP_MODEL,
                g_param_spec_object ("model",
                                     "Model", "The options to present in the combobox",
                                     GTK_TYPE_TREE_MODEL,
                                     G_PARAM_READWRITE));

        g_object_class_install_property (object_class, PROP_TEXT_COLUMN,
                g_param_spec_int ("text-column",
                                  "Text Column", "The model column that contains the displayable text",
                                  -1, G_MAXINT, -1,
                                  G_PARAM_READWRITE));


        g_object_class_install_property (object_class, PROP_EDITABLE,
                g_param_spec_boolean ("editable",
                                      "Editable", "Whether the text can be edited",
                                      FALSE,
                                      G_PARAM_READWRITE));

        g_type_class_add_private (class, sizeof (UmEditableComboPrivate));
}

static void
start_editing (UmEditableCombo *combo)
{
        gtk_stack_set_visible_child_name (combo->priv->stack, PAGE_COMBO);
        gtk_widget_grab_focus (GTK_WIDGET (combo->priv->combo));
        gtk_combo_box_popup (combo->priv->combo);
}

static gboolean
gtk_combo_box_has_focus (GtkComboBox *combo)
{
        GtkWidget *widget;
        GtkWindow *window;

        window = GTK_WINDOW (gtk_widget_get_toplevel (GTK_WIDGET (combo)));
        widget = gtk_window_get_focus (window);
        if (widget) {
                return gtk_widget_is_ancestor (widget, GTK_WIDGET (combo));
        }

        return FALSE;
}

static void
stop_editing (UmEditableCombo *combo)
{
        gboolean has_focus;

        um_editable_combo_set_active (combo,
                                      gtk_combo_box_get_active (combo->priv->combo));
        has_focus = gtk_combo_box_has_focus (combo->priv->combo);
        gtk_stack_set_visible_child_name (combo->priv->stack, PAGE_BUTTON);
        if (has_focus)
                gtk_widget_grab_focus (GTK_WIDGET (combo->priv->button));

        g_signal_emit (combo, signals[EDITING_DONE], 0);
}

static void
cancel_editing (UmEditableCombo *combo)
{
        gboolean has_focus;

        gtk_combo_box_set_active (combo->priv->combo,
                                  um_editable_combo_get_active (combo));
        has_focus = gtk_combo_box_has_focus (combo->priv->combo);
        gtk_stack_set_visible_child_name (combo->priv->stack, PAGE_BUTTON);
        if (has_focus)
                gtk_widget_grab_focus (GTK_WIDGET (combo->priv->button));
}

static void
um_editable_combo_activate (UmEditableCombo *combo)
{
        if (combo->priv->editable) {
                gtk_stack_set_visible_child_name (combo->priv->stack, PAGE_COMBO);
                gtk_widget_grab_focus (GTK_WIDGET (combo->priv->combo));
        }
}

static void
button_clicked (GtkWidget       *widget,
                UmEditableCombo *combo)
{
        if (combo->priv->editable)
                start_editing (combo);
}

static void
combo_changed (GtkWidget       *widget,
               UmEditableCombo *combo)
{
        if (combo->priv->editable)
                stop_editing (combo);
}

static void
popup_shown (GtkWidget       *widget,
             GParamSpec      *pspec,
             UmEditableCombo *combo)
{
        gboolean is_shown;

        g_object_get (G_OBJECT (combo->priv->combo), "popup-shown", &is_shown, NULL);
        if (!is_shown)
                cancel_editing (combo);
}

static void
update_button_padding (UmEditableCombo *combo)
{
        UmEditableComboPrivate *priv = combo->priv;
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
um_editable_combo_init (UmEditableCombo *combo)
{
        UmEditableComboPrivate *priv;
        GtkCellRenderer *cell;

        priv = combo->priv = UM_EDITABLE_COMBO_GET_PRIVATE (combo);

        priv->active = -1;
        priv->text_column = -1;
        priv->stack = GTK_STACK (gtk_stack_new ());

        priv->label = (GtkLabel*)gtk_label_new ("");
        gtk_misc_set_alignment (GTK_MISC (priv->label), 0.0, 0.5);
        gtk_stack_add_named (priv->stack, GTK_WIDGET (priv->label), PAGE_LABEL);

        priv->button = (GtkButton*)gtk_button_new_with_label ("");
        gtk_widget_set_receives_default ((GtkWidget*)priv->button, TRUE);
        gtk_button_set_relief (priv->button, GTK_RELIEF_NONE);
        gtk_button_set_alignment (priv->button, 0.0, 0.5);
        gtk_stack_add_named (priv->stack, GTK_WIDGET (priv->button), PAGE_BUTTON);
        g_signal_connect (priv->button, "clicked", G_CALLBACK (button_clicked), combo);

        priv->combo = (GtkComboBox*)gtk_combo_box_new ();
        cell = gtk_cell_renderer_text_new ();
        gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (priv->combo), cell, TRUE);
        gtk_stack_add_named (priv->stack, GTK_WIDGET (priv->combo), PAGE_COMBO);

        g_signal_connect (priv->combo, "changed", G_CALLBACK (combo_changed), combo);
        g_signal_connect (priv->combo, "notify::popup-shown", G_CALLBACK (popup_shown), combo);

        update_button_padding (combo);

        gtk_container_add (GTK_CONTAINER (combo), GTK_WIDGET (priv->stack));

        gtk_widget_show ((GtkWidget*)priv->stack);
        gtk_widget_show ((GtkWidget*)priv->label);
        gtk_widget_show ((GtkWidget*)priv->button);
        gtk_widget_show ((GtkWidget*)priv->combo);

        gtk_stack_set_visible_child_name (priv->stack, PAGE_LABEL);
}

GtkWidget *
um_editable_combo_new (void)
{
        return (GtkWidget *) g_object_new (UM_TYPE_EDITABLE_COMBO, NULL);
}
