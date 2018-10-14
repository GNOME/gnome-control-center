/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2009 Bastien Nocera
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
 */

#include "config.h"

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>

#include <glib.h>
#include <glib/gi18n-lib.h>
#include <gtk/gtk.h>
#include <canberra-gtk.h>
#include <pulse/pulseaudio.h>

#include "gvc-combo-box.h"
#include "gvc-mixer-stream.h"
#include "gvc-mixer-card.h"

struct _GvcComboBox
{
        GtkBox         parent_instance;

        GtkWidget     *drop_box;
        GtkWidget     *start_box;
        GtkWidget     *end_box;
        GtkWidget     *label;
        GtkWidget     *button;
        GtkTreeModel  *model;
        GtkWidget     *combobox;
        gboolean       set_called;
        GtkSizeGroup  *size_group;
        gboolean       symmetric;
};

enum {
        COL_NAME,
        COL_HUMAN_NAME,
        NUM_COLS
};

enum {
        CHANGED,
        BUTTON_CLICKED,
        LAST_SIGNAL
};

enum {
        PROP_0,
        PROP_LABEL,
        PROP_SHOW_BUTTON,
        PROP_BUTTON_LABEL
};

static guint signals [LAST_SIGNAL] = { 0, };

static void     gvc_combo_box_class_init (GvcComboBoxClass *klass);
static void     gvc_combo_box_init       (GvcComboBox      *combo_box);
static void     gvc_combo_box_finalize   (GObject            *object);

G_DEFINE_TYPE (GvcComboBox, gvc_combo_box, GTK_TYPE_BOX)

void
gvc_combo_box_set_size_group (GvcComboBox *combo_box,
                              GtkSizeGroup  *group,
                              gboolean       symmetric)
{
        g_return_if_fail (GVC_IS_COMBO_BOX (combo_box));

        combo_box->size_group = group;
        combo_box->symmetric = symmetric;

        if (combo_box->size_group != NULL) {
                gtk_size_group_add_widget (combo_box->size_group,
                                           combo_box->start_box);

                if (combo_box->symmetric) {
                        gtk_size_group_add_widget (combo_box->size_group,
                                                   combo_box->end_box);
                }
        }
        gtk_widget_queue_draw (GTK_WIDGET (combo_box));
}

static void
gvc_combo_box_set_property (GObject       *object,
                            guint          prop_id,
                            const GValue  *value,
                            GParamSpec    *pspec)
{
        GvcComboBox *self = GVC_COMBO_BOX (object);

        switch (prop_id) {
        case PROP_LABEL:
                gtk_label_set_text_with_mnemonic (GTK_LABEL (self->label), g_value_get_string (value));
                break;
        case PROP_BUTTON_LABEL:
                gtk_button_set_label (GTK_BUTTON (self->button), g_value_get_string (value));
                break;
        case PROP_SHOW_BUTTON:
                gtk_widget_set_visible (self->button, g_value_get_boolean (value));
                break;
        default:
                G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
                break;
        }
}

static void
gvc_combo_box_get_property (GObject     *object,
                            guint        prop_id,
                            GValue      *value,
                            GParamSpec  *pspec)
{
        GvcComboBox *self = GVC_COMBO_BOX (object);

        switch (prop_id) {
        case PROP_LABEL:
                g_value_set_string (value,
                                    gtk_label_get_text (GTK_LABEL (self->label)));
                break;
        case PROP_BUTTON_LABEL:
                g_value_set_string (value,
                                    gtk_button_get_label (GTK_BUTTON (self->button)));
                break;
        case PROP_SHOW_BUTTON:
                g_value_set_boolean (value,
                                     gtk_widget_get_visible (self->button));
                break;
        default:
                G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
                break;
        }
}

static void
gvc_combo_box_class_init (GvcComboBoxClass *klass)
{
        GObjectClass   *object_class = G_OBJECT_CLASS (klass);

        object_class->finalize = gvc_combo_box_finalize;
        object_class->set_property = gvc_combo_box_set_property;
        object_class->get_property = gvc_combo_box_get_property;

        g_object_class_install_property (object_class,
                                         PROP_LABEL,
                                         g_param_spec_string ("label",
                                                              "label",
                                                              "The combo box label",
                                                              _("_Profile:"),
                                                              G_PARAM_READWRITE|G_PARAM_CONSTRUCT));
        g_object_class_install_property (object_class,
                                         PROP_SHOW_BUTTON,
                                         g_param_spec_boolean ("show-button",
                                                              "show-button",
                                                              "Whether to show the button",
                                                              FALSE,
                                                              G_PARAM_READWRITE|G_PARAM_CONSTRUCT));
        g_object_class_install_property (object_class,
                                         PROP_BUTTON_LABEL,
                                         g_param_spec_string ("button-label",
                                                              "button-label",
                                                              "The button's label",
                                                              "APPLICATION BUG",
                                                              G_PARAM_READWRITE|G_PARAM_CONSTRUCT));
        signals [CHANGED] =
                g_signal_new ("changed",
                              G_TYPE_FROM_CLASS (klass),
                              G_SIGNAL_RUN_LAST,
                              0,
                              NULL, NULL,
                              g_cclosure_marshal_VOID__STRING,
                              G_TYPE_NONE, 1, G_TYPE_STRING);
        signals [BUTTON_CLICKED] =
                g_signal_new ("button-clicked",
                              G_TYPE_FROM_CLASS (klass),
                              G_SIGNAL_RUN_LAST,
                              0,
                              NULL, NULL,
                              g_cclosure_marshal_VOID__VOID,
                              G_TYPE_NONE, 0, G_TYPE_NONE);
}

void
gvc_combo_box_set_profiles (GvcComboBox *combo_box,
                            const GList       *profiles)
{
        const GList *l;

        g_return_if_fail (GVC_IS_COMBO_BOX (combo_box));
        g_return_if_fail (combo_box->set_called == FALSE);

        for (l = profiles; l != NULL; l = l->next) {
                GvcMixerCardProfile *p = l->data;

                gtk_list_store_insert_with_values (GTK_LIST_STORE (combo_box->model),
                                                   NULL,
                                                   G_MAXINT,
                                                   COL_NAME, p->profile,
                                                   COL_HUMAN_NAME, p->human_profile,
                                                   -1);
        }
        combo_box->set_called = TRUE;
}

void
gvc_combo_box_set_ports (GvcComboBox *combo_box,
                         const GList       *ports)
{
        const GList *l;

        g_return_if_fail (GVC_IS_COMBO_BOX (combo_box));
        g_return_if_fail (combo_box->set_called == FALSE);

        for (l = ports; l != NULL; l = l->next) {
                GvcMixerStreamPort *p = l->data;

                gtk_list_store_insert_with_values (GTK_LIST_STORE (combo_box->model),
                                                   NULL,
                                                   G_MAXINT,
                                                   COL_NAME, p->port,
                                                   COL_HUMAN_NAME, p->human_port,
                                                   -1);
        }
        combo_box->set_called = TRUE;
}

void
gvc_combo_box_set_active (GvcComboBox *combo_box,
                          const char  *id)
{
        GtkTreeIter iter;
        gboolean cont;

        cont = gtk_tree_model_get_iter_first (combo_box->model, &iter);
        while (cont != FALSE) {
                char *name;

                gtk_tree_model_get (combo_box->model, &iter,
                                    COL_NAME, &name,
                                    -1);
                if (g_strcmp0 (name, id) == 0) {
                        gtk_combo_box_set_active_iter (GTK_COMBO_BOX (combo_box->combobox), &iter);
                        return;
                }
                cont = gtk_tree_model_iter_next (combo_box->model, &iter);
        }
        g_warning ("Could not find id '%s' in combo box", id);
}

static void
on_combo_box_changed (GtkComboBox *widget,
                      GvcComboBox *combo_box)
{
        GtkTreeIter          iter;
        g_autofree gchar    *profile = NULL;

        if (gtk_combo_box_get_active_iter (GTK_COMBO_BOX (widget), &iter) == FALSE) {
                g_warning ("Could not find an active profile or port");
                return;
        }

        gtk_tree_model_get (combo_box->model, &iter,
                            COL_NAME, &profile,
                            -1);
        g_signal_emit (combo_box, signals[CHANGED], 0, profile);
}

static void
on_combo_box_button_clicked (GtkButton   *button,
                             GvcComboBox *combo_box)
{
        g_signal_emit (combo_box, signals[BUTTON_CLICKED], 0);
}

static void
gvc_combo_box_init (GvcComboBox *combo_box)
{
        GtkWidget *frame;
        GtkWidget            *box;
        GtkWidget            *sbox;
        GtkWidget            *ebox;
        GtkCellRenderer      *renderer;

        combo_box->model = GTK_TREE_MODEL (gtk_list_store_new (NUM_COLS,
                                                               G_TYPE_STRING,
                                                               G_TYPE_STRING));

        combo_box->label = gtk_label_new (NULL);
        gtk_widget_show (combo_box->label);
        gtk_widget_set_halign (combo_box->label, GTK_ALIGN_START);

        /* frame */
        frame = gtk_frame_new (NULL);
        gtk_widget_show (frame);
        gtk_frame_set_shadow_type (GTK_FRAME (frame), GTK_SHADOW_NONE);
        gtk_box_pack_start (GTK_BOX (combo_box), frame, TRUE, TRUE, 0);

        combo_box->drop_box = box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 6);
        gtk_widget_show (box);
        combo_box->combobox = gtk_combo_box_new_with_model (combo_box->model);
        gtk_widget_show (combo_box->combobox);
        renderer = gtk_cell_renderer_text_new ();
        gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (combo_box->combobox),
                                    renderer, TRUE);
        gtk_cell_layout_add_attribute (GTK_CELL_LAYOUT (combo_box->combobox),
                                       renderer,
                                       "text", COL_HUMAN_NAME);

        /* Make sure that the combo box isn't too wide when human names are overly long,
         * but that we can still read the full length of it */
        g_object_set (G_OBJECT (renderer), "ellipsize", PANGO_ELLIPSIZE_END, NULL);
        g_object_set (G_OBJECT (combo_box->combobox), "popup-fixed-width", FALSE, NULL);

        combo_box->start_box = sbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 6);
        gtk_widget_show (sbox);
        gtk_box_pack_start (GTK_BOX (box), sbox, FALSE, FALSE, 0);

        gtk_box_pack_start (GTK_BOX (sbox), combo_box->label, FALSE, FALSE, 0);

        gtk_box_pack_start (GTK_BOX (box), combo_box->combobox, TRUE, TRUE, 0);

        combo_box->button = gtk_button_new_with_label ("APPLICATION BUG");
        gtk_widget_hide (combo_box->button);
        gtk_button_set_use_underline (GTK_BUTTON (combo_box->button), TRUE);
        gtk_box_pack_start (GTK_BOX (box), combo_box->button, FALSE, FALSE, 0);


        combo_box->end_box = ebox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 6);
        gtk_widget_show (ebox);
        gtk_box_pack_start (GTK_BOX (box), ebox, FALSE, FALSE, 0);

        if (combo_box->size_group != NULL) {
                gtk_size_group_add_widget (combo_box->size_group, sbox);

                if (combo_box->symmetric) {
                        gtk_size_group_add_widget (combo_box->size_group, ebox);
                }
        }

        gtk_container_add (GTK_CONTAINER (frame), combo_box->drop_box);

        gtk_label_set_mnemonic_widget (GTK_LABEL (combo_box->label),
                                       combo_box->combobox);

        g_signal_connect (G_OBJECT (combo_box->combobox), "changed",
                          G_CALLBACK (on_combo_box_changed), combo_box);
        g_signal_connect (G_OBJECT (combo_box->button), "clicked",
                          G_CALLBACK (on_combo_box_button_clicked), combo_box);
}

static void
gvc_combo_box_finalize (GObject *object)
{
        GvcComboBox *combo_box;

        g_return_if_fail (object != NULL);
        g_return_if_fail (GVC_IS_COMBO_BOX (object));

        combo_box = GVC_COMBO_BOX (object);

        g_return_if_fail (combo_box != NULL);

        g_object_unref (combo_box->model);
        combo_box->model = NULL;

        G_OBJECT_CLASS (gvc_combo_box_parent_class)->finalize (object);
}

GtkWidget *
gvc_combo_box_new (const char *label)
{
        GObject *combo_box;
        combo_box = g_object_new (GVC_TYPE_COMBO_BOX,
                                  "label", label,
                                  "orientation", GTK_ORIENTATION_HORIZONTAL,
                                  NULL);
        return GTK_WIDGET (combo_box);
}

