/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2002 Jonathan Blandford <jrb@gnome.org>
 * Copyright (C) 2007 Jens Granseuer <jensgr@gmx.net>
 * Copyright (C) 2010 Red Hat, Inc.
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

#include <gtk/gtk.h>
#include <gio/gio.h>
#include <glib/gi18n-lib.h>
#include <gconf/gconf-client.h>

#include "gconf-property-editor.h"

#include "cc-font-page.h"
#include "cc-font-details-dialog.h"
#include "cc-font-common.h"

#define CC_FONT_PAGE_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), CC_TYPE_FONT_PAGE, CcFontPagePrivate))

#define WID(s) GTK_WIDGET (gtk_builder_get_object (builder, s))

struct CcFontPagePrivate
{
        GtkWidget *window_title_font;
        GtkWidget *desktop_font;
        GtkWidget *document_font;
        GtkWidget *monospace_font;
        GtkWidget *application_font;

        GtkWidget *font_details_dialog;
        GSList    *font_pairs;

        gboolean   in_change;
        char      *old_font;
        guint      metacity_font_notify_id;
        guint      font_render_notify_id;
};

enum {
        PROP_0,
};

static void     cc_font_page_class_init     (CcFontPageClass *klass);
static void     cc_font_page_init           (CcFontPage      *font_page);
static void     cc_font_page_finalize       (GObject             *object);

G_DEFINE_TYPE (CcFontPage, cc_font_page, CC_TYPE_PAGE)

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
cc_font_page_set_property (GObject      *object,
                            guint         prop_id,
                            const GValue *value,
                            GParamSpec   *pspec)
{
        switch (prop_id) {
        default:
                G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
                break;
        }
}

static void
cc_font_page_get_property (GObject    *object,
                            guint       prop_id,
                            GValue     *value,
                            GParamSpec *pspec)
{
        switch (prop_id) {
        default:
                G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
                break;
        }
}

/*
 * Code implementing a group of radio buttons with different Xft option combinations.
 * If one of the buttons is matched by the GConf key, we pick it. Otherwise we
 * show the group as inconsistent.
 */
static void
font_render_get_gconf (GConfClient  *client,
                       Antialiasing *antialiasing,
                       Hinting      *hinting)
{
        char *antialias_str = gconf_client_get_string (client, FONT_ANTIALIASING_KEY, NULL);
        char *hint_str = gconf_client_get_string (client, FONT_HINTING_KEY, NULL);
        int   val;

        val = ANTIALIAS_GRAYSCALE;
        if (antialias_str) {
                gconf_string_to_enum (antialias_enums, antialias_str, &val);
                g_free (antialias_str);
        }
        *antialiasing = val;

        val = HINT_FULL;
        if (hint_str) {
                gconf_string_to_enum (hint_enums, hint_str, &val);
                g_free (hint_str);
        }
        *hinting = val;
}

typedef struct {
        Antialiasing     antialiasing;
        Hinting          hinting;
        GtkToggleButton *radio;
} FontPair;

static void
font_render_load (CcFontPage *page)
{
        Antialiasing antialiasing;
        Hinting      hinting;
        gboolean     inconsistent = TRUE;
        GSList      *tmp_list;
        GConfClient *client;

        client = gconf_client_get_default ();
        font_render_get_gconf (client, &antialiasing, &hinting);
        g_object_unref (client);

        page->priv->in_change = TRUE;

        for (tmp_list = page->priv->font_pairs; tmp_list; tmp_list = tmp_list->next) {
                FontPair *pair = tmp_list->data;

                if (antialiasing == pair->antialiasing && hinting == pair->hinting) {
                        gtk_toggle_button_set_active (pair->radio, TRUE);
                        inconsistent = FALSE;
                        break;
                }
        }

        for (tmp_list = page->priv->font_pairs; tmp_list; tmp_list = tmp_list->next) {
                FontPair *pair = tmp_list->data;

                gtk_toggle_button_set_inconsistent (pair->radio, inconsistent);
        }

        page->priv->in_change = FALSE;
}

static void
on_font_render_changed (GConfClient *client,
                        guint        cnxn_id,
                        GConfEntry  *entry,
                        CcFontPage  *page)
{
        font_render_load (page);
}

static void
on_font_radio_toggled (GtkToggleButton *toggle_button,
                       CcFontPage      *page)
{
        GSList *l;

        if (!page->priv->in_change) {
                GConfClient *client = gconf_client_get_default ();
                FontPair    *pair;

                pair = NULL;
                for (l = page->priv->font_pairs; l != NULL; l = l->next) {
                        FontPair *p;
                        p = l->data;
                        if (p->radio == toggle_button) {
                                pair = p;
                                break;
                        }
                }

                g_assert (pair != NULL);

                gconf_client_set_string (client,
                                         FONT_ANTIALIASING_KEY,
                                         gconf_enum_to_string (antialias_enums, pair->antialiasing),
                                         NULL);
                gconf_client_set_string (client,
                                         FONT_HINTING_KEY,
                                         gconf_enum_to_string (hint_enums, pair->hinting),
                                         NULL);

                /* Restore back to the previous state until we get notification */
                font_render_load (page);
                g_object_unref (client);
        }
}

static void
setup_font_pair (CcFontPage   *page,
                 GtkWidget    *radio,
                 GtkWidget    *darea,
                 Antialiasing  antialiasing,
                 Hinting       hinting)
{
        FontPair *pair = g_new (FontPair, 1);

        pair->antialiasing = antialiasing;
        pair->hinting = hinting;
        pair->radio = GTK_TOGGLE_BUTTON (radio);

        setup_font_sample (darea, antialiasing, hinting);
        page->priv->font_pairs = g_slist_prepend (page->priv->font_pairs, pair);

        g_signal_connect (radio,
                          "toggled",
                          G_CALLBACK (on_font_radio_toggled),
                          page);
}

static void
metacity_titlebar_load_sensitivity (CcFontPage *page)
{
        GConfClient *client;

        client = gconf_client_get_default ();
        gtk_widget_set_sensitive (page->priv->window_title_font,
                                  !gconf_client_get_bool (client,
                                                          WINDOW_TITLE_USES_SYSTEM_KEY,
                                                          NULL));
        g_object_unref (client);
}

static void
on_metacity_font_changed (GConfClient *client,
                          guint        cnxn_id,
                          GConfEntry  *entry,
                          CcFontPage  *page)
{
        metacity_titlebar_load_sensitivity (page);
}

/* returns 0 if the font is safe, otherwise returns the size in points. */
static gint
font_dangerous (const char *font)
{
        PangoFontDescription *pfd;
        gboolean              retval = 0;

        pfd = pango_font_description_from_string (font);
        if (pfd == NULL)
                /* an invalid font was passed in.  This isn't our problem. */
                return 0;

        if ((pango_font_description_get_set_fields (pfd) & PANGO_FONT_MASK_SIZE) &&
            (pango_font_description_get_size (pfd) >= MAX_FONT_SIZE_WITHOUT_WARNING)) {
                retval = pango_font_description_get_size (pfd)/1024;
        }
        pango_font_description_free (pfd);

        return retval;
}

static GConfValue *
application_font_to_gconf (GConfPropertyEditor *peditor,
                           GConfValue          *value)
{
        GConfValue *new_value;
        const char *new_font;
        GtkWidget  *font_button;
        gint        danger_level;
        CcFontPage *page;

        page = g_object_get_data (G_OBJECT (peditor), "page");

        font_button = GTK_WIDGET (gconf_property_editor_get_ui_control (peditor));
        g_return_val_if_fail (font_button != NULL, NULL);

        new_value = gconf_value_new (GCONF_VALUE_STRING);
        new_font = gconf_value_get_string (value);
        if (font_dangerous (page->priv->old_font)) {
                /* If we're already too large, we don't warn again. */
                gconf_value_set_string (new_value, new_font);
                return new_value;
        }

        danger_level = font_dangerous (new_font);
        if (danger_level) {
                GtkWidget  *warning_dialog, *apply_button;
                const char *warning_label;
                char       *warning_label2;

                warning_label = _("Font may be too large");

                if (danger_level > MAX_FONT_POINT_WITHOUT_WARNING) {
                        warning_label2 = g_strdup_printf (ngettext (
                                                                    "The font selected is %d point large, "
                                                                    "and may make it difficult to effectively "
                                                                    "use the computer.  It is recommended that "
                                                                    "you select a size smaller than %d.",
                                                                    "The font selected is %d points large, "
                                                                    "and may make it difficult to effectively "
                                                                    "use the computer. It is recommended that "
                                                                    "you select a size smaller than %d.",
                                                                    danger_level),
                                                          danger_level,
                                                          MAX_FONT_POINT_WITHOUT_WARNING);
                } else {
                        warning_label2 = g_strdup_printf (ngettext (
                                                                    "The font selected is %d point large, "
                                                                    "and may make it difficult to effectively "
                                                                    "use the computer.  It is recommended that "
                                                                    "you select a smaller sized font.",
                                                                    "The font selected is %d points large, "
                                                                    "and may make it difficult to effectively "
                                                                    "use the computer. It is recommended that "
                                                                    "you select a smaller sized font.",
                                                                    danger_level),
                                                          danger_level);
                }

                warning_dialog = gtk_message_dialog_new (NULL,
                                                         GTK_DIALOG_MODAL,
                                                         GTK_MESSAGE_WARNING,
                                                         GTK_BUTTONS_NONE,
                                                         "%s",
                                                         warning_label);

                gtk_message_dialog_format_secondary_text (GTK_MESSAGE_DIALOG (warning_dialog),
                                                          "%s", warning_label2);

                gtk_dialog_add_button (GTK_DIALOG (warning_dialog),
                                       _("Use previous font"), GTK_RESPONSE_CLOSE);

                apply_button = gtk_button_new_with_label (_("Use selected font"));

                gtk_button_set_image (GTK_BUTTON (apply_button), gtk_image_new_from_stock (GTK_STOCK_APPLY, GTK_ICON_SIZE_BUTTON));
                gtk_dialog_add_action_widget (GTK_DIALOG (warning_dialog), apply_button, GTK_RESPONSE_APPLY);
                GTK_WIDGET_SET_FLAGS (apply_button, GTK_CAN_DEFAULT);
                gtk_widget_show (apply_button);

                gtk_dialog_set_default_response (GTK_DIALOG (warning_dialog), GTK_RESPONSE_CLOSE);

                g_free (warning_label2);

                if (gtk_dialog_run (GTK_DIALOG (warning_dialog)) == GTK_RESPONSE_APPLY) {
                        gconf_value_set_string (new_value, new_font);
                } else {
                        gconf_value_set_string (new_value, page->priv->old_font);
                        gtk_font_button_set_font_name (GTK_FONT_BUTTON (font_button), page->priv->old_font);
                }

                gtk_widget_destroy (warning_dialog);
        } else {
                gconf_value_set_string (new_value, new_font);
        }

        return new_value;
}

static void
on_application_font_changed (CcFontPage *page)
{
        const char *font;

        font = gtk_font_button_get_font_name (GTK_FONT_BUTTON (page->priv->application_font));
        g_free (page->priv->old_font);
        page->priv->old_font = g_strdup (font);
}

#ifdef HAVE_XFT2

static void
on_font_details_response (GtkDialog  *dialog,
                          gint        response_id,
                          CcFontPage *page)
{
        if (page->priv->font_details_dialog != NULL) {
                gtk_widget_destroy (page->priv->font_details_dialog);
                page->priv->font_details_dialog = NULL;
        }
}

static void
on_details_button_clicked (GtkWidget  *button,
                           CcFontPage *page)
{
        if (page->priv->font_details_dialog == NULL) {
                GtkWidget *toplevel;

                toplevel = gtk_widget_get_toplevel (GTK_WIDGET (page));
                if (!GTK_WIDGET_TOPLEVEL (toplevel)) {
                        toplevel = NULL;
                }

                page->priv->font_details_dialog = cc_font_details_dialog_new ();

                g_signal_connect (page->priv->font_details_dialog,
                                  "response",
                                  G_CALLBACK (on_font_details_response),
                                  page);

                gtk_window_set_transient_for (GTK_WINDOW (page->priv->font_details_dialog),
                                              GTK_WINDOW (toplevel));

        }

        gtk_window_present (GTK_WINDOW (page->priv->font_details_dialog));
}
#endif /* HAVE_XFT2 */

static void
setup_page (CcFontPage *page)
{
        GtkBuilder         *builder;
        GtkWidget          *widget;
        GError             *error;
        GConfClient        *client;
        GObject            *peditor;

        client = gconf_client_get_default ();

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

        page->priv->font_details_dialog = NULL;

        gconf_client_add_dir (client,
                              "/desktop/gnome/interface",
                              GCONF_CLIENT_PRELOAD_ONELEVEL,
                              NULL);
        gconf_client_add_dir (client,
                              "/apps/nautilus/preferences",
                              GCONF_CLIENT_PRELOAD_ONELEVEL,
                              NULL);
        gconf_client_add_dir (client,
                              METACITY_DIR,
                              GCONF_CLIENT_PRELOAD_ONELEVEL,
                              NULL);
#ifdef HAVE_XFT2
        gconf_client_add_dir (client,
                              FONT_RENDER_DIR,
                              GCONF_CLIENT_PRELOAD_ONELEVEL,
                              NULL);
#endif  /* HAVE_XFT2 */

        page->priv->window_title_font = WID ("window_title_font");
        page->priv->application_font = WID ("application_font");
        page->priv->desktop_font = WID ("desktop_font");
        page->priv->document_font = WID ("document_font");
        page->priv->monospace_font = WID ("monospace_font");

        peditor = gconf_peditor_new_font (NULL,
                                          GTK_FONT_KEY,
                                          page->priv->application_font,
                                          "conv-from-widget-cb",
                                          application_font_to_gconf,
                                          NULL);
        g_object_set_data (peditor, "page", page);

        g_signal_connect_swapped (peditor,
                                  "value-changed",
                                  G_CALLBACK (on_application_font_changed),
                                  page);
        on_application_font_changed (page);

        peditor = gconf_peditor_new_font (NULL,
                                          DOCUMENT_FONT_KEY,
                                          page->priv->document_font,
                                          NULL);

        peditor = gconf_peditor_new_font (NULL,
                                          DESKTOP_FONT_KEY,
                                          page->priv->desktop_font,
                                          NULL);

        peditor = gconf_peditor_new_font (NULL,
                                          WINDOW_TITLE_FONT_KEY,
                                          page->priv->window_title_font,
                                          NULL);

        peditor = gconf_peditor_new_font (NULL,
                                          MONOSPACE_FONT_KEY,
                                          page->priv->monospace_font,
                                          NULL);

        page->priv->metacity_font_notify_id =
                gconf_client_notify_add (client,
                                         WINDOW_TITLE_USES_SYSTEM_KEY,
                                         (GConfClientNotifyFunc) on_metacity_font_changed,
                                         page,
                                         NULL,
                                         NULL);

        metacity_titlebar_load_sensitivity (page);

#ifdef HAVE_XFT2
        setup_font_pair (page,
                         WID ("monochrome_radio"),
                         WID ("monochrome_sample"),
                         ANTIALIAS_NONE, HINT_FULL);
        setup_font_pair (page,
                         WID ("best_shapes_radio"),
                         WID ("best_shapes_sample"),
                         ANTIALIAS_GRAYSCALE, HINT_MEDIUM);
        setup_font_pair (page,
                         WID ("best_contrast_radio"),
                         WID ("best_contrast_sample"),
                         ANTIALIAS_GRAYSCALE, HINT_FULL);
        setup_font_pair (page,
                         WID ("subpixel_radio"),
                         WID ("subpixel_sample"),
                         ANTIALIAS_RGBA, HINT_FULL);

        font_render_load (page);

        page->priv->font_render_notify_id =
                gconf_client_notify_add (client,
                                         FONT_RENDER_DIR,
                                         (GConfClientNotifyFunc) on_font_render_changed,
                                         page,
                                         NULL,
                                         NULL);

        g_signal_connect (WID ("details_button"),
                          "clicked",
                          G_CALLBACK (on_details_button_clicked),
                          page);
#else /* !HAVE_XFT2 */
        gtk_widget_hide (WID ("font_render_frame"));
#endif /* HAVE_XFT2 */


        widget = WID ("font_vbox");
        gtk_widget_reparent (widget, GTK_WIDGET (page));
        gtk_widget_show (widget);

        g_object_unref (client);
        g_object_unref (builder);
}

static GObject *
cc_font_page_constructor (GType                  type,
                          guint                  n_construct_properties,
                          GObjectConstructParam *construct_properties)
{
        CcFontPage      *font_page;

        font_page = CC_FONT_PAGE (G_OBJECT_CLASS (cc_font_page_parent_class)->constructor (type,
                                                                                           n_construct_properties,
                                                                                           construct_properties));

        g_object_set (font_page,
                      "display-name", _("Font"),
                      "id", "font",
                      NULL);

        setup_page (font_page);

        return G_OBJECT (font_page);
}

static void
start_working (CcFontPage *page)
{
        static gboolean once = FALSE;

        if (!once) {

                once = TRUE;
        }
}

static void
stop_working (CcFontPage *page)
{

}

static void
cc_font_page_active_changed (CcPage  *base_page,
                             gboolean is_active)
{
        CcFontPage *page = CC_FONT_PAGE (base_page);

        if (is_active)
                start_working (page);
        else
                stop_working (page);

        CC_PAGE_CLASS (cc_font_page_parent_class)->active_changed (base_page, is_active);

}

static void
cc_font_page_class_init (CcFontPageClass *klass)
{
        GObjectClass  *object_class = G_OBJECT_CLASS (klass);
        CcPageClass   *page_class = CC_PAGE_CLASS (klass);

        object_class->get_property = cc_font_page_get_property;
        object_class->set_property = cc_font_page_set_property;
        object_class->constructor = cc_font_page_constructor;
        object_class->finalize = cc_font_page_finalize;

        page_class->active_changed = cc_font_page_active_changed;

        g_type_class_add_private (klass, sizeof (CcFontPagePrivate));
}

static void
cc_font_page_init (CcFontPage *page)
{
        page->priv = CC_FONT_PAGE_GET_PRIVATE (page);
}

static void
cc_font_page_finalize (GObject *object)
{
        CcFontPage  *page;
        GConfClient *client;

        g_return_if_fail (object != NULL);
        g_return_if_fail (CC_IS_FONT_PAGE (object));

        page = CC_FONT_PAGE (object);

        g_return_if_fail (page->priv != NULL);

        client = gconf_client_get_default ();
        gconf_client_notify_remove (client, page->priv->metacity_font_notify_id);
        gconf_client_notify_remove (client, page->priv->font_render_notify_id);
        g_object_unref (client);


        g_slist_foreach (page->priv->font_pairs,
                         (GFunc) g_free,
                         NULL);
        g_slist_free (page->priv->font_pairs);
        g_free (page->priv->old_font);

        G_OBJECT_CLASS (cc_font_page_parent_class)->finalize (object);
}

CcPage *
cc_font_page_new (void)
{
        GObject *object;

        object = g_object_new (CC_TYPE_FONT_PAGE, NULL);

        return CC_PAGE (object);
}
