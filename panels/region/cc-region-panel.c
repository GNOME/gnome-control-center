/*
 * Copyright (C) 2010 Intel, Inc
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
 * Author: Sergey Udaltsov <svu@gnome.org>
 *
 */

#include <config.h>
#include <glib/gi18n.h>

#include "cc-region-panel.h"
#include "cc-region-resources.h"

#include <gtk/gtk.h>

#include "gnome-region-panel-input.h"
#include "gnome-region-panel-lang.h"
#include "gnome-region-panel-formats.h"
#include "gnome-region-panel-system.h"

#include "egg-list-box/egg-list-box.h"

CC_PANEL_REGISTER (CcRegionPanel, cc_region_panel)

#define REGION_PANEL_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), CC_TYPE_REGION_PANEL, CcRegionPanelPrivate))

struct _CcRegionPanelPrivate {
	GtkBuilder *builder;

        GtkWidget *login_button;
        GtkWidget *language_row;
        GtkWidget *formats_row;
        GtkWidget *options_button;
        GtkWidget *input_source_list;
};

static void
cc_region_panel_finalize (GObject * object)
{
	CcRegionPanel *panel;

	panel = CC_REGION_PANEL (object);

	if (panel->priv && panel->priv->builder)
		g_object_unref (panel->priv->builder);

	G_OBJECT_CLASS (cc_region_panel_parent_class)->finalize (object);
}

static void
cc_region_panel_constructed (GObject *object)
{
        CcRegionPanel *self = CC_REGION_PANEL (object);

        G_OBJECT_CLASS (cc_region_panel_parent_class)->constructed (object);

        self->priv->login_button = gtk_button_new_with_label (_("Login Screen"));

        cc_shell_embed_widget_in_header (cc_panel_get_shell (CC_PANEL (object)),
                                         self->priv->login_button);
        gtk_widget_show_all (self->priv->login_button);
}

static const char *
cc_region_panel_get_help_uri (CcPanel *panel)
{
  return "help:gnome-help/prefs-language";
}

static void
cc_region_panel_class_init (CcRegionPanelClass * klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	CcPanelClass * panel_class = CC_PANEL_CLASS (klass);

	g_type_class_add_private (klass, sizeof (CcRegionPanelPrivate));

	panel_class->get_help_uri = cc_region_panel_get_help_uri;

        object_class->constructed = cc_region_panel_constructed;
	object_class->finalize = cc_region_panel_finalize;
}

static void
update_separator_func (GtkWidget **separator,
                       GtkWidget  *child,
                       GtkWidget  *before,
                       gpointer    user_data)
{
  if (before == NULL)
    return;

  if (*separator == NULL)
    {
      *separator = gtk_separator_new (GTK_ORIENTATION_HORIZONTAL);
      g_object_ref_sink (*separator);
      gtk_widget_show (*separator);
    }
}

static void
activate_input_child (CcRegionPanel *self, GtkWidget child)
{
}

static void
add_language_section (CcRegionPanel *self)
{
	CcRegionPanelPrivate *priv = self->priv;
        GtkWidget *vbox;
        GtkWidget *frame;
        GtkWidget *widget;
        GtkWidget *row;
        GtkWidget *label;

        vbox = GTK_WIDGET (gtk_builder_get_object (priv->builder, "vbox_region"));

        widget = GTK_WIDGET (egg_list_box_new ());
        egg_list_box_set_selection_mode (EGG_LIST_BOX (widget),
                                         GTK_SELECTION_NONE);
        egg_list_box_set_separator_funcs (EGG_LIST_BOX (widget),
                                          update_separator_func,
                                          NULL, NULL);
        g_signal_connect_swapped (widget, "child-activated",
                                  G_CALLBACK (activate_input_child), self);


        frame = gtk_frame_new (NULL);
        gtk_frame_set_shadow_type (GTK_FRAME (frame), GTK_SHADOW_IN);
        gtk_widget_set_margin_left (frame, 134);
        gtk_widget_set_margin_right (frame, 134);
        gtk_widget_set_margin_bottom (frame, 22);

        gtk_container_add (GTK_CONTAINER (frame), widget);
        gtk_box_pack_start (GTK_BOX (vbox), frame, FALSE, TRUE, 0);

        priv->language_row = row = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
        gtk_container_add (GTK_CONTAINER (widget), priv->language_row);
        label = gtk_label_new (_("Language"));
        gtk_misc_set_alignment (GTK_MISC (label), 0, 0.5);
        gtk_widget_set_margin_left (label, 20);
        gtk_widget_set_margin_right (label, 20);
        gtk_widget_set_margin_top (label, 6);
        gtk_widget_set_margin_bottom (label, 6);
        gtk_box_pack_start (GTK_BOX (row), label, TRUE, TRUE, 0);

        label = gtk_label_new ("English (United Kingdom)");
        gtk_misc_set_alignment (GTK_MISC (label), 1, 0.5);
        gtk_widget_set_margin_left (label, 20);
        gtk_widget_set_margin_right (label, 20);
        gtk_widget_set_margin_top (label, 6);
        gtk_widget_set_margin_bottom (label, 6);
        gtk_box_pack_start (GTK_BOX (row), label, FALSE, TRUE, 0);

        priv->formats_row = row = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
        gtk_container_add (GTK_CONTAINER (widget), priv->formats_row);
        label = gtk_label_new (_("Formats"));
        gtk_misc_set_alignment (GTK_MISC (label), 0, 0.5);
        gtk_widget_set_margin_left (label, 20);
        gtk_widget_set_margin_right (label, 20);
        gtk_widget_set_margin_top (label, 6);
        gtk_widget_set_margin_bottom (label, 6);
        gtk_box_pack_start (GTK_BOX (row), label, TRUE, TRUE, 0);

        label = gtk_label_new ("United Kingdom");
        gtk_misc_set_alignment (GTK_MISC (label), 1, 0.5);
        gtk_widget_set_margin_left (label, 20);
        gtk_widget_set_margin_right (label, 20);
        gtk_widget_set_margin_top (label, 6);
        gtk_widget_set_margin_bottom (label, 6);
        gtk_box_pack_start (GTK_BOX (row), label, FALSE, TRUE, 0);

        gtk_widget_show_all (frame);
}

static void
add_keyboard_layout_row (CcRegionPanel *self, const gchar *name)
{
        GtkWidget *row;
        GtkWidget *label;

        row = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
        label = gtk_label_new (name);
        gtk_misc_set_alignment (GTK_MISC (label), 0, 0.5);
        gtk_widget_set_margin_left (label, 20);
        gtk_widget_set_margin_right (label, 20);
        gtk_widget_set_margin_top (label, 6);
        gtk_widget_set_margin_bottom (label, 6);
        gtk_box_pack_start (GTK_BOX (row), label, TRUE, TRUE, 0);
        gtk_widget_show_all (row);
        gtk_container_add (GTK_CONTAINER (self->priv->input_source_list), row);
}

static void
add_input_method_row (CcRegionPanel *self, const gchar *name)
{
        GtkWidget *row;
        GtkWidget *label;
        GtkWidget *image;

        row = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
        label = gtk_label_new (name);
        gtk_misc_set_alignment (GTK_MISC (label), 0, 0.5);
        gtk_widget_set_margin_left (label, 20);
        gtk_widget_set_margin_right (label, 20);
        gtk_widget_set_margin_top (label, 6);
        gtk_widget_set_margin_bottom (label, 6);
        gtk_box_pack_start (GTK_BOX (row), label, TRUE, TRUE, 0);

        image = gtk_image_new_from_icon_name ("system-run-symbolic", GTK_ICON_SIZE_BUTTON);
        gtk_widget_set_margin_left (image, 20);
        gtk_widget_set_margin_right (image, 20);
        gtk_widget_set_margin_top (image, 6);
        gtk_widget_set_margin_bottom (image, 6);
        gtk_box_pack_start (GTK_BOX (row), image, FALSE, TRUE, 0);

        gtk_widget_show_all (row);
        gtk_container_add (GTK_CONTAINER (self->priv->input_source_list), row);
}

static void
add_input_section (CcRegionPanel *self)
{
	CcRegionPanelPrivate *priv = self->priv;
        GtkWidget *vbox;
        GtkWidget *box;
        GtkWidget *widget;
        GtkWidget *row;
        GtkWidget *label;
        GtkWidget *frame;
        gchar *s;

        vbox = GTK_WIDGET (gtk_builder_get_object (priv->builder, "vbox_region"));

        box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
        gtk_widget_set_margin_left (box, 134);
        gtk_widget_set_margin_right (box, 134);
        gtk_widget_set_margin_top (box, 0);
        gtk_widget_set_margin_bottom (box, 22);
        gtk_box_pack_start (GTK_BOX (vbox), box, TRUE, TRUE, 0);

        s = g_strdup_printf ("<b>%s</b>", _("Input Sources"));
        label = gtk_label_new (s);
        g_free (s);
        gtk_label_set_use_markup (GTK_LABEL (label), TRUE);
        gtk_misc_set_alignment (GTK_MISC (label), 0, 0.5);
        gtk_widget_set_margin_left (label, 6);
        gtk_widget_set_margin_right (label, 6);
        gtk_widget_set_margin_bottom (label, 6);
        row = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
        gtk_box_pack_start (GTK_BOX (row), label, TRUE, TRUE, 0);

        self->priv->options_button = gtk_button_new_with_label (_("Options"));
        gtk_widget_set_margin_bottom (label, 6);
        gtk_box_pack_start (GTK_BOX (row), self->priv->options_button, FALSE, TRUE, 0);
        gtk_box_pack_start (GTK_BOX (box), row, FALSE, TRUE, 0);

        priv->input_source_list = widget = GTK_WIDGET (egg_list_box_new ());
        egg_list_box_set_selection_mode (EGG_LIST_BOX (widget),
                                         GTK_SELECTION_SINGLE);
        egg_list_box_set_separator_funcs (EGG_LIST_BOX (widget),
                                          update_separator_func,
                                          NULL, NULL);

        frame = gtk_frame_new (NULL);
        gtk_frame_set_shadow_type (GTK_FRAME (frame), GTK_SHADOW_IN);

        gtk_container_add (GTK_CONTAINER (frame), widget);
        gtk_box_pack_start (GTK_BOX (box), frame, FALSE, TRUE, 0);

        add_keyboard_layout_row (self, _("English (UK)"));
        add_input_method_row (self, _("Japanese (Anthy)"));
        add_input_method_row (self, _("Chinese (Pinyin)"));

        gtk_widget_show_all (box);
}

static void
cc_region_panel_init (CcRegionPanel *self)
{
	CcRegionPanelPrivate *priv;
	GtkWidget *vbox;
	GError *error = NULL;

	priv = self->priv = REGION_PANEL_PRIVATE (self);
        g_resources_register (cc_region_get_resource ());

	priv->builder = gtk_builder_new ();

	gtk_builder_add_from_resource (priv->builder,
                                       "/org/gnome/control-center/region/region.ui",
                                       &error);
	if (error != NULL) {
		g_warning ("Error loading UI file: %s", error->message);
		g_error_free (error);
		return;
	}

        add_language_section (self);
        add_input_section (self);

        vbox = GTK_WIDGET (gtk_builder_get_object (priv->builder, "vbox_region"));
	gtk_widget_reparent (vbox, GTK_WIDGET (self));
}
