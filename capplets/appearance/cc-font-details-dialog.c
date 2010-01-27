/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2002 Jonathan Blandford <jrb@gnome.org>
 * Copyright (C) 2007 Jens Granseuer <jensgr@gmx.net>
 * Copyright (C) 2010 William Jon McCann
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
 */

#include "config.h"

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>

#include <glib/gi18n.h>
#include <gtk/gtk.h>
#include <glib/gstdio.h>
#include <gconf/gconf-client.h>

#include "cc-font-details-dialog.h"
#include "cc-font-common.h"

#define CC_FONT_DETAILS_DIALOG_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), CC_TYPE_FONT_DETAILS_DIALOG, CcFontDetailsDialogPrivate))

#define WID(s) GTK_WIDGET (gtk_builder_get_object (builder, s))

struct CcFontDetailsDialogPrivate
{
        GSList    *font_groups;
        GtkWidget *dpi_spin_button;

        GtkWidget *antialias_none_sample;
        GtkWidget *antialias_grayscale_sample;
        GtkWidget *antialias_subpixel_sample;

        GtkWidget *antialias_none_radiobutton;
        GtkWidget *antialias_grayscale_radiobutton;
        GtkWidget *antialias_subpixel_radiobutton;

        GtkWidget *hint_none_sample;
        GtkWidget *hint_slight_sample;
        GtkWidget *hint_medium_sample;
        GtkWidget *hint_full_sample;

        GtkWidget *hint_none_radiobutton;
        GtkWidget *hint_slight_radiobutton;
        GtkWidget *hint_medium_radiobutton;
        GtkWidget *hint_full_radiobutton;

        GtkWidget *subpixel_rgb_image;
        GtkWidget *subpixel_bgr_image;
        GtkWidget *subpixel_vrgb_image;
        GtkWidget *subpixel_vbgr_image;

        GtkWidget *subpixel_rgb_radiobutton;
        GtkWidget *subpixel_bgr_radiobutton;
        GtkWidget *subpixel_vrgb_radiobutton;
        GtkWidget *subpixel_vbgr_radiobutton;

        gboolean   in_change;

        guint      dpi_notify_id;
};

enum {
        PROP_0,
};

static void     cc_font_details_dialog_class_init  (CcFontDetailsDialogClass *klass);
static void     cc_font_details_dialog_init        (CcFontDetailsDialog      *font_details_dialog);
static void     cc_font_details_dialog_finalize    (GObject                   *object);

G_DEFINE_TYPE (CcFontDetailsDialog, cc_font_details_dialog, GTK_TYPE_DIALOG)

static GConfEnumStringPair rgba_order_enums[] = {
        { RGBA_RGB,  "rgb" },
        { RGBA_BGR,  "bgr" },
        { RGBA_VRGB, "vrgb" },
        { RGBA_VBGR, "vbgr" },
        { -1,         NULL }
};

static GConfEnumStringPair antialias_enums[] = {
        { ANTIALIAS_NONE,      "none" },
        { ANTIALIAS_GRAYSCALE, "grayscale" },
        { ANTIALIAS_RGBA,      "rgba" },
        { -1,                  NULL }
};

static GConfEnumStringPair hint_enums[] = {
        { HINT_NONE,   "none" },
        { HINT_SLIGHT, "slight" },
        { HINT_MEDIUM, "medium" },
        { HINT_FULL,   "full" },
        { -1,          NULL }
};

static void
cc_font_details_dialog_set_property (GObject        *object,
                                     guint           prop_id,
                                     const GValue   *value,
                                     GParamSpec     *pspec)
{
        CcFontDetailsDialog *self;

        self = CC_FONT_DETAILS_DIALOG (object);

        switch (prop_id) {
        default:
                G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
                break;
        }
}

static void
cc_font_details_dialog_get_property (GObject        *object,
                                     guint           prop_id,
                                     GValue         *value,
                                     GParamSpec     *pspec)
{
        switch (prop_id) {
        default:
                G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
                break;
        }
}

static double
dpi_from_pixels_and_mm (int pixels, int mm)
{
        double dpi;

        if (mm >= 1)
                dpi = pixels / (mm / 25.4);
        else
                dpi = 0;

        return dpi;
}

static double
get_dpi_from_x_server (void)
{
        GdkScreen *screen;
        double dpi;

        screen = gdk_screen_get_default ();
        if (screen) {
                double width_dpi, height_dpi;

                width_dpi = dpi_from_pixels_and_mm (gdk_screen_get_width (screen),
                                                    gdk_screen_get_width_mm (screen));
                height_dpi = dpi_from_pixels_and_mm (gdk_screen_get_height (screen),
                                                     gdk_screen_get_height_mm (screen));

                if (width_dpi < DPI_LOW_REASONABLE_VALUE || width_dpi > DPI_HIGH_REASONABLE_VALUE ||
                    height_dpi < DPI_LOW_REASONABLE_VALUE || height_dpi > DPI_HIGH_REASONABLE_VALUE)
                        dpi = DPI_FALLBACK;
                else
                        dpi = (width_dpi + height_dpi) / 2.0;
        } else {
                /* Huh!?  No screen? */
                dpi = DPI_FALLBACK;
        }

        return dpi;
}

static void
dpi_load (CcFontDetailsDialog *dialog)
{
        GConfClient *client;
        GConfValue  *value;
        gdouble      dpi;

        client = gconf_client_get_default ();
        value = gconf_client_get_without_default (client, FONT_DPI_KEY, NULL);
        g_object_unref (client);

        if (value) {
                dpi = gconf_value_get_float (value);
                gconf_value_free (value);
        } else
                dpi = get_dpi_from_x_server ();

        if (dpi < DPI_LOW_REASONABLE_VALUE)
                dpi = DPI_LOW_REASONABLE_VALUE;

        dialog->priv->in_change = TRUE;
        gtk_spin_button_set_value (GTK_SPIN_BUTTON (dialog->priv->dpi_spin_button), dpi);
        dialog->priv->in_change = FALSE;
}

static void
on_dpi_changed (GConfClient         *client,
                guint                cnxn_id,
                GConfEntry          *entry,
                CcFontDetailsDialog *dialog)
{
        dpi_load (dialog);
}

static void
on_dpi_value_changed (GtkSpinButton       *spinner,
                      CcFontDetailsDialog *dialog)
{
        /* Like any time when using a spin button with GConf, there is
         * a race condition here. When we change, we send the new
         * value to GConf, then restore to the old value until
         * we get a response to emulate the proper model/view behavior.
         *
         * If the user changes the value faster than responses are
         * received from GConf, this may cause mildly strange effects.
         */
        if (!dialog->priv->in_change) {
                gdouble      new_dpi;
                GConfClient *client;

                client = gconf_client_get_default ();
                new_dpi = gtk_spin_button_get_value (GTK_SPIN_BUTTON (dialog->priv->dpi_spin_button));
                gconf_client_set_float (client, FONT_DPI_KEY, new_dpi, NULL);
                g_object_unref (client);

                dpi_load (dialog);
        }
}

/*
 * EnumGroup - a group of radio buttons tied to a string enumeration
 *             value. We add this here because the gconf peditor
 *             equivalent of this is both painful to use (you have
 *             to supply functions to convert from enums to indices)
 *             and conceptually broken (the order of radio buttons
 *             in a group when using Glade is not predictable.
 */
typedef struct
{
        GConfClient         *client;
        CcFontDetailsDialog *dialog;
        GSList              *items;
        char                *gconf_key;
        GConfEnumStringPair *enums;
        int                  default_value;
        guint                notify_id;
} EnumGroup;

typedef struct
{
        EnumGroup       *group;
        GtkToggleButton *widget;
        int              value;
} EnumItem;

static void
enum_group_load (EnumGroup *group)
{
        char   *str = gconf_client_get_string (group->client, group->gconf_key, NULL);
        int     val = group->default_value;
        GSList *tmp_list;

        if (str)
                gconf_string_to_enum (group->enums, str, &val);

        g_free (str);

        group->dialog->priv->in_change = TRUE;

        for (tmp_list = group->items; tmp_list; tmp_list = tmp_list->next) {
                EnumItem *item = tmp_list->data;

                if (val == item->value)
                        gtk_toggle_button_set_active (item->widget, TRUE);
        }

        group->dialog->priv->in_change = FALSE;
}

static void
on_enum_group_changed (GConfClient *client,
                       guint        cnxn_id,
                       GConfEntry  *entry,
                       EnumGroup   *group)
{
        enum_group_load (group);
}

static void
enum_item_toggled (GtkToggleButton *toggle_button,
                   EnumItem        *item)
{
        EnumGroup *group = item->group;

        if (!group->dialog->priv->in_change) {
                gconf_client_set_string (group->client,
                                         group->gconf_key,
                                         gconf_enum_to_string (group->enums, item->value),
                                         NULL);
        }

        /* Restore back to the previous state until we get notification */
        enum_group_load (group);
}

static EnumGroup *
enum_group_create (CcFontDetailsDialog *dialog,
                   const char          *gconf_key,
                   GConfEnumStringPair *enums,
                   int                  default_value,
                   GtkWidget           *first_widget,
                   ...)
{
        EnumGroup *group;
        GtkWidget *widget;
        va_list    args;

        group = g_new (EnumGroup, 1);

        group->dialog = dialog;
        group->client = gconf_client_get_default ();
        group->gconf_key = g_strdup (gconf_key);
        group->enums = enums;
        group->default_value = default_value;
        group->items = NULL;

        va_start (args, first_widget);

        widget = first_widget;
        while (widget) {
                EnumItem *item;

                item = g_new (EnumItem, 1);
                item->group = group;
                item->widget = GTK_TOGGLE_BUTTON (widget);
                item->value = va_arg (args, int);

                g_signal_connect (item->widget,
                                  "toggled",
                                  G_CALLBACK (enum_item_toggled),
                                  item);

                group->items = g_slist_prepend (group->items, item);

                widget = va_arg (args, GtkWidget *);
        }

        va_end (args);

        enum_group_load (group);

        group->notify_id = gconf_client_notify_add (group->client,
                                                    gconf_key,
                                                    (GConfClientNotifyFunc) on_enum_group_changed,
                                                    group,
                                                    NULL,
                                                    NULL);

        return group;
}

static void
enum_group_destroy (EnumGroup *group)
{
        g_free (group->gconf_key);

        g_slist_foreach (group->items, (GFunc) g_free, NULL);
        g_slist_free (group->items);

        gconf_client_notify_remove (group->client, group->notify_id);
        g_object_unref (group->client);

        g_free (group);
}

static void
setup_dialog (CcFontDetailsDialog *dialog)
{
        GtkBuilder        *builder;
        GtkWidget         *widget;
        GtkWidget         *box;
        GtkAdjustment     *adjustment;
        EnumGroup         *group;
        GConfClient       *client;
        GError            *error;

        builder = gtk_builder_new ();

        error = NULL;
        gtk_builder_add_from_file (builder,
                                   GNOMECC_UI_DIR
                                   "/appearance.ui",
                                   &error);
        if (error != NULL) {
                g_error (_("Could not load user interface file: %s"),
                         error->message);
                g_error_free (error);
                return;
        }

        dialog->priv->dpi_spin_button = WID ("dpi_spinner");

        /* pick a sensible maximum dpi */
        adjustment = gtk_spin_button_get_adjustment (GTK_SPIN_BUTTON (dialog->priv->dpi_spin_button));
        adjustment->upper = DPI_HIGH_REASONABLE_VALUE;
        adjustment->lower = DPI_LOW_REASONABLE_VALUE;
        adjustment->step_increment = 1;

        dpi_load (dialog);
        g_signal_connect (dialog->priv->dpi_spin_button,
                          "value_changed",
                          G_CALLBACK (on_dpi_value_changed),
                          dialog);

        client = gconf_client_get_default ();
        dialog->priv->dpi_notify_id = gconf_client_notify_add (client,
                                                               FONT_DPI_KEY,
                                                               (GConfClientNotifyFunc) on_dpi_changed,
                                                               dialog,
                                                               NULL,
                                                               NULL);

        dialog->priv->antialias_none_sample = WID ("antialias_none_sample");
        dialog->priv->antialias_grayscale_sample = WID ("antialias_grayscale_sample");
        dialog->priv->antialias_subpixel_sample = WID ("antialias_subpixel_sample");

        dialog->priv->antialias_none_radiobutton = WID ("antialias_none_radio");
        dialog->priv->antialias_grayscale_radiobutton = WID ("antialias_grayscale_radio");
        dialog->priv->antialias_subpixel_radiobutton = WID ("antialias_subpixel_radio");

        dialog->priv->hint_none_sample = WID ("hint_none_sample");
        dialog->priv->hint_slight_sample = WID ("hint_slight_sample");
        dialog->priv->hint_medium_sample = WID ("hint_medium_sample");
        dialog->priv->hint_full_sample = WID ("hint_full_sample");

        dialog->priv->hint_none_radiobutton = WID ("hint_none_radio");
        dialog->priv->hint_slight_radiobutton = WID ("hint_slight_radio");
        dialog->priv->hint_medium_radiobutton = WID ("hint_medium_radio");
        dialog->priv->hint_full_radiobutton = WID ("hint_full_radio");

        dialog->priv->subpixel_rgb_image = WID ("subpixel_rgb_image");
        dialog->priv->subpixel_bgr_image = WID ("subpixel_bgr_image");
        dialog->priv->subpixel_vrgb_image = WID ("subpixel_vrgb_image");
        dialog->priv->subpixel_vbgr_image = WID ("subpixel_vbgr_image");

        dialog->priv->subpixel_rgb_radiobutton = WID ("subpixel_rgb_radio");
        dialog->priv->subpixel_bgr_radiobutton = WID ("subpixel_bgr_radio");
        dialog->priv->subpixel_vrgb_radiobutton = WID ("subpixel_vrgb_radio");
        dialog->priv->subpixel_vbgr_radiobutton = WID ("subpixel_vbgr_radio");

        setup_font_sample (dialog->priv->antialias_none_sample,
                           ANTIALIAS_NONE,
                           HINT_FULL);
        setup_font_sample (dialog->priv->antialias_grayscale_sample,
                           ANTIALIAS_GRAYSCALE,
                           HINT_FULL);
        setup_font_sample (dialog->priv->antialias_subpixel_sample,
                           ANTIALIAS_RGBA,
                           HINT_FULL);

        group = enum_group_create (dialog,
                                   FONT_ANTIALIASING_KEY,
                                   antialias_enums,
                                   ANTIALIAS_GRAYSCALE,
                                   dialog->priv->antialias_none_radiobutton,
                                   ANTIALIAS_NONE,
                                   dialog->priv->antialias_grayscale_radiobutton,
                                   ANTIALIAS_GRAYSCALE,
                                   dialog->priv->antialias_subpixel_radiobutton,
                                   ANTIALIAS_RGBA,
                                   NULL);
        dialog->priv->font_groups = g_slist_prepend (dialog->priv->font_groups, group);

        setup_font_sample (dialog->priv->hint_none_sample,   ANTIALIAS_GRAYSCALE, HINT_NONE);
        setup_font_sample (dialog->priv->hint_slight_sample, ANTIALIAS_GRAYSCALE, HINT_SLIGHT);
        setup_font_sample (dialog->priv->hint_medium_sample, ANTIALIAS_GRAYSCALE, HINT_MEDIUM);
        setup_font_sample (dialog->priv->hint_full_sample,   ANTIALIAS_GRAYSCALE, HINT_FULL);

        group = enum_group_create (dialog,
                                   FONT_HINTING_KEY,
                                   hint_enums,
                                   HINT_FULL,
                                   dialog->priv->hint_none_radiobutton, HINT_NONE,
                                   dialog->priv->hint_slight_radiobutton, HINT_SLIGHT,
                                   dialog->priv->hint_medium_radiobutton, HINT_MEDIUM,
                                   dialog->priv->hint_full_radiobutton, HINT_FULL,
                                   NULL);
        dialog->priv->font_groups = g_slist_prepend (dialog->priv->font_groups, group);

        gtk_image_set_from_file (GTK_IMAGE (dialog->priv->subpixel_rgb_image),
                                 GNOMECC_PIXMAP_DIR "/subpixel-rgb.png");
        gtk_image_set_from_file (GTK_IMAGE (dialog->priv->subpixel_bgr_image),
                                 GNOMECC_PIXMAP_DIR "/subpixel-bgr.png");
        gtk_image_set_from_file (GTK_IMAGE (dialog->priv->subpixel_vrgb_image),
                                 GNOMECC_PIXMAP_DIR "/subpixel-vrgb.png");
        gtk_image_set_from_file (GTK_IMAGE (dialog->priv->subpixel_vbgr_image),
                                 GNOMECC_PIXMAP_DIR "/subpixel-vbgr.png");

        group = enum_group_create (dialog,
                                   FONT_RGBA_ORDER_KEY, rgba_order_enums, RGBA_RGB,
                                   dialog->priv->subpixel_rgb_radiobutton,  RGBA_RGB,
                                   dialog->priv->subpixel_bgr_radiobutton,  RGBA_BGR,
                                   dialog->priv->subpixel_vrgb_radiobutton, RGBA_VRGB,
                                   dialog->priv->subpixel_vbgr_radiobutton, RGBA_VBGR,
                                   NULL);
        dialog->priv->font_groups = g_slist_prepend (dialog->priv->font_groups, group);

        gtk_dialog_add_button (GTK_DIALOG (dialog), GTK_STOCK_CLOSE, GTK_RESPONSE_CLOSE);

        widget = WID ("render_details_vbox");
        box = gtk_dialog_get_content_area (GTK_DIALOG (dialog));
        gtk_widget_reparent (widget, box);
        gtk_widget_show (widget);

        g_object_unref (builder);
        g_object_unref (client);
}

static GObject *
cc_font_details_dialog_constructor (GType                  type,
                                    guint                  n_construct_properties,
                                    GObjectConstructParam *construct_properties)
{
        CcFontDetailsDialog *dialog;

        dialog = CC_FONT_DETAILS_DIALOG (G_OBJECT_CLASS (cc_font_details_dialog_parent_class)->constructor (type,
                                                                                                            n_construct_properties,
                                                                                                            construct_properties));

        setup_dialog (dialog);

        return G_OBJECT (dialog);
}

static void
cc_font_details_dialog_class_init (CcFontDetailsDialogClass *klass)
{
        GObjectClass   *object_class = G_OBJECT_CLASS (klass);

        object_class->get_property = cc_font_details_dialog_get_property;
        object_class->set_property = cc_font_details_dialog_set_property;
        object_class->constructor = cc_font_details_dialog_constructor;
        object_class->finalize = cc_font_details_dialog_finalize;

        g_type_class_add_private (klass, sizeof (CcFontDetailsDialogPrivate));
}

static void
cc_font_details_dialog_init (CcFontDetailsDialog *dialog)
{
        dialog->priv = CC_FONT_DETAILS_DIALOG_GET_PRIVATE (dialog);
}

static void
cc_font_details_dialog_finalize (GObject *object)
{
        CcFontDetailsDialog *dialog;
        GConfClient         *client;

        g_return_if_fail (object != NULL);
        g_return_if_fail (CC_IS_FONT_DETAILS_DIALOG (object));

        dialog = CC_FONT_DETAILS_DIALOG (object);

        g_return_if_fail (dialog->priv != NULL);

        client = gconf_client_get_default ();
        gconf_client_notify_remove (client, dialog->priv->dpi_notify_id);
        g_object_unref (client);

        g_slist_foreach (dialog->priv->font_groups,
                         (GFunc) enum_group_destroy,
                         NULL);
        g_slist_free (dialog->priv->font_groups);

        G_OBJECT_CLASS (cc_font_details_dialog_parent_class)->finalize (object);
}

GtkWidget *
cc_font_details_dialog_new (void)
{
        GObject *object;

        object = g_object_new (CC_TYPE_FONT_DETAILS_DIALOG,
                               "title", _("Font Rendering Details"),
                               "has-separator", FALSE,
                               NULL);

        return GTK_WIDGET (object);
}
