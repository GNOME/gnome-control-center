/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2007 - 2010 Red Hat, Inc.
 * Copyright (C) 2010 Intel Corp
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

#include <glib/gi18n-lib.h>
#include <gio/gio.h>
#include <gtk/gtk.h>
#include <mx/mx-gtk.h>
#include <gconf/gconf-client.h>
#include <dbus/dbus-glib.h>

#define GNOME_DESKTOP_USE_UNSTABLE_API
#include <libgnomeui/gnome-rr.h>
#include <libgnomeui/gnome-rr-config.h>
#include <gdk/gdkx.h>
#include <X11/Xlib.h>

#include "cc-display-page.h"

#define CC_DISPLAY_PAGE_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), CC_TYPE_DISPLAY_PAGE, CcDisplayPagePrivate))

#define WID(s) GTK_WIDGET (gtk_builder_get_object (builder, s))

typedef enum {
        /* Internal display is on, no external */
        INTERNAL,
        /* Internal display is on, external is present */
        INTERNAL_EXTERNAL_PRESENT,
        /* Internal display off, external display is on */
        EXTERNAL,
} OutputMode;

enum {
        COL_MODE = 0,
        COL_NAME = 1,
        COL_END = -1
};

struct CcDisplayPagePrivate
{
        GnomeRRScreen   *screen;
        GnomeRRConfig   *current_configuration;
        GnomeOutputInfo *internal_output, *external_output;

        GtkWidget       *monitor_icon;
        GtkWidget       *toggle;
        GtkTreeModel    *resolution_store;
        GtkWidget       *resolution_combo;
        GtkWidget       *resolution_box;
        GtkWidget       *state_label;

        /* We store the event timestamp when the Apply button is clicked */
        GtkWidget       *apply_button;
        guint32          apply_button_clicked_timestamp;

        /* These are used while we are waiting for the ApplyConfiguration method to be executed over D-bus */
        DBusGConnection *connection;
        DBusGProxy      *proxy;
        DBusGProxyCall  *proxy_call;

        enum {
                APPLYING_VERSION_1,
                APPLYING_VERSION_2
        } apply_configuration_state;
};

enum {
        PROP_0,
};

static void     cc_display_page_class_init     (CcDisplayPageClass *klass);
static void     cc_display_page_init           (CcDisplayPage      *display_page);
static void     cc_display_page_finalize       (GObject             *object);

static void     apply_configuration_returned_cb (DBusGProxy *proxy, DBusGProxyCall *call_id, CcDisplayPage *page);

G_DEFINE_TYPE (CcDisplayPage, cc_display_page, CC_TYPE_PAGE)

static void
error_message (CcDisplayPage *page,
               const char    *primary_text,
               const char    *secondary_text)
{
        GtkWidget *dialog;
        GtkWidget *toplevel;

        toplevel = gtk_widget_get_toplevel (GTK_WIDGET (page));
        if (!GTK_WIDGET_TOPLEVEL (toplevel)) {
                toplevel = NULL;
        }

        dialog = gtk_message_dialog_new ((toplevel) ? GTK_WINDOW (toplevel) : NULL,
                                         GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
                                         GTK_MESSAGE_ERROR,
                                         GTK_BUTTONS_CLOSE,
                                         "%s", primary_text);

        if (secondary_text != NULL)
                gtk_message_dialog_format_secondary_text (GTK_MESSAGE_DIALOG (dialog),
                                                          "%s", secondary_text);

        gtk_dialog_run (GTK_DIALOG (dialog));
        gtk_widget_destroy (dialog);
}

static void
compute_virtual_size_for_configuration (GnomeRRConfig *config,
                                        int           *ret_width,
                                        int           *ret_height)
{
        int i;
        int width;
        int height;

        width = height = 0;

        for (i = 0; config->outputs[i] != NULL; i++) {
                GnomeOutputInfo *output;

                output = config->outputs[i];

                if (output->on) {
                        width = MAX (width, output->x + output->width);
                        height = MAX (height, output->y + output->height);
                }
        }

        *ret_width = width;
        *ret_height = height;
}

static void
check_required_virtual_size (CcDisplayPage *page)
{
        int req_width;
        int req_height;
        int min_width;
        int max_width;
        int min_height;
        int max_height;

        compute_virtual_size_for_configuration (page->priv->current_configuration,
                                                &req_width,
                                                &req_height);

        gnome_rr_screen_get_ranges (page->priv->screen,
                                    &min_width,
                                    &max_width,
                                    &min_height,
                                    &max_height);

#if 0
        g_debug ("X Server supports:");
        g_debug ("min_width = %d, max_width = %d", min_width, max_width);
        g_debug ("min_height = %d, max_height = %d", min_height, max_height);

        g_debug ("Requesting size of %dx%d", req_width, req_height);
#endif

        if (!(min_width <= req_width && req_width <= max_width
              && min_height <= req_height && req_height <= max_height)) {
                /* FIXME: present a useful dialog, maybe even before the user tries to Apply */
#if 0
                g_debug ("Your X server needs a larger Virtual size!");
#endif
        }
}

static void
begin_version2_apply_configuration (CcDisplayPage *page,
                                    GdkWindow     *parent_window,
                                    guint32        timestamp)
{
        XID parent_window_xid;

        parent_window_xid = GDK_WINDOW_XID (parent_window);

        page->priv->proxy = dbus_g_proxy_new_for_name (page->priv->connection,
                                                       "org.gnome.SettingsDaemon",
                                                       "/org/gnome/SettingsDaemon/XRANDR",
                                                       "org.gnome.SettingsDaemon.XRANDR_2");
        g_assert (page->priv->proxy != NULL); /* that call does not fail unless we pass bogus names */

        page->priv->apply_configuration_state = APPLYING_VERSION_2;
        page->priv->proxy_call = dbus_g_proxy_begin_call (page->priv->proxy,
                                                          "ApplyConfiguration",
                                                          (DBusGProxyCallNotify) apply_configuration_returned_cb,
                                                          page,
                                                          NULL,
                                                          G_TYPE_INT64,
                                                          (gint64) parent_window_xid,
                                                          G_TYPE_INT64,
                                                          (gint64) timestamp,
                                                          G_TYPE_INVALID,
                                                          G_TYPE_INVALID);
        /* FIXME: we don't check for page->priv->proxy_call == NULL, which could happen if
         * the connection was disconnected.  This is left as an exercise for the
         * reader.
         */
}

static void
begin_version1_apply_configuration (CcDisplayPage *page)
{
        page->priv->proxy = dbus_g_proxy_new_for_name (page->priv->connection,
                                                "org.gnome.SettingsDaemon",
                                                "/org/gnome/SettingsDaemon/XRANDR",
                                                "org.gnome.SettingsDaemon.XRANDR");
        g_assert (page->priv->proxy != NULL); /* that call does not fail unless we pass bogus names */

        page->priv->apply_configuration_state = APPLYING_VERSION_1;
        page->priv->proxy_call = dbus_g_proxy_begin_call (page->priv->proxy,
                                                          "ApplyConfiguration",
                                                          (DBusGProxyCallNotify) apply_configuration_returned_cb,
                                                          page,
                                                          NULL,
                                                          G_TYPE_INVALID,
                                                          G_TYPE_INVALID);
        /* FIXME: we don't check for page->priv->proxy_call == NULL, which could happen if
         * the connection was disconnected.  This is left as an exercise for the
         * reader.
         */
}

static void
ensure_current_configuration_is_saved (void)
{
        GnomeRRScreen *rr_screen;
        GnomeRRConfig *rr_config;

        /* Normally, gnome_rr_config_save() creates a backup file based on the
         * old monitors.xml.  However, if *that* file didn't exist, there is
         * nothing from which to create a backup.  So, here we'll save the
         * current/unchanged configuration and then let our caller call
         * gnome_rr_config_save() again with the new/changed configuration, so
         * that there *will* be a backup file in the end.
         */

        rr_screen = gnome_rr_screen_new (gdk_screen_get_default (),
                                         NULL,
                                         NULL,
                                         NULL); /* NULL-GError */
        if (!rr_screen)
                return;

        rr_config = gnome_rr_config_new_current (rr_screen);
        gnome_rr_config_save (rr_config, NULL); /* NULL-GError */

        gnome_rr_config_free (rr_config);
        gnome_rr_screen_destroy (rr_screen);
}

/* Callback for dbus_g_proxy_begin_call() */
static void
apply_configuration_returned_cb (DBusGProxy     *proxy,
                                 DBusGProxyCall *call_id,
                                 CcDisplayPage  *page)
{
        gboolean success;
        GError  *error;

        g_assert (call_id == page->priv->proxy_call);

        error = NULL;
        success = dbus_g_proxy_end_call (proxy, call_id, &error, G_TYPE_INVALID);

        if (!success) {
                if (page->priv->apply_configuration_state == APPLYING_VERSION_2
                    && g_error_matches (error, DBUS_GERROR, DBUS_GERROR_UNKNOWN_METHOD)) {
                        g_error_free (error);

                        g_object_unref (page->priv->proxy);
                        page->priv->proxy = NULL;

                        begin_version1_apply_configuration (page);
                        return;
                } else {
                        /* We don't pop up an error message; gnome-settings-daemon already does that
                         * in case the selected RANDR configuration could not be applied.
                         */
                        g_error_free (error);
                }
        }

        g_object_unref (page->priv->proxy);
        page->priv->proxy = NULL;

        dbus_g_connection_unref (page->priv->connection);
        page->priv->connection = NULL;
        page->priv->proxy_call = NULL;

        gtk_widget_set_sensitive (GTK_WIDGET (page), TRUE);
}

static void
apply (CcDisplayPage *page)
{
        GError    *error;
        GtkWidget *toplevel;


        gnome_rr_config_sanitize (page->priv->current_configuration);

        check_required_virtual_size (page);

        ensure_current_configuration_is_saved ();

        error = NULL;
        if (!gnome_rr_config_save (page->priv->current_configuration, &error)) {
                error_message (page,
                               _("Could not save the monitor configuration"),
                               error->message);
                g_error_free (error);
                return;
        }

        g_assert (page->priv->connection == NULL);
        g_assert (page->priv->proxy == NULL);
        g_assert (page->priv->proxy_call == NULL);

        page->priv->connection = dbus_g_bus_get (DBUS_BUS_SESSION, &error);
        if (page->priv->connection == NULL) {
                error_message (page,
                               _("Could not get session bus while applying display configuration"),
                               error->message);
                g_error_free (error);
                return;
        }

        gtk_widget_set_sensitive (GTK_WIDGET (page), FALSE);

        toplevel = gtk_widget_get_toplevel (GTK_WIDGET (page));
        if (!GTK_WIDGET_TOPLEVEL (toplevel)) {
                toplevel = GTK_WIDGET (page);
        }

        begin_version2_apply_configuration (page,
                                            gtk_widget_get_window (toplevel),
                                            page->priv->apply_button_clicked_timestamp);
}

static void
on_apply_button_clicked (GtkButton     *button,
                         CcDisplayPage *page)
{
        /* We simply store the timestamp at which the Apply button was clicked.
         * We'll just wait for the dialog to return from gtk_dialog_run(), and
         * *then* use the timestamp when applying the RANDR configuration.
         */

        page->priv->apply_button_clicked_timestamp = gtk_get_current_event_time ();

        apply (page);
}

typedef struct {
        GnomeRRMode *mode;
        gboolean found;
        GtkTreeIter iter;
        GnomeRRMode *found_mode;
} FindData;

/*
 * foreach helper to find the mode in @data->mode in @model.  If it exists,
 * @data->found is TRUE and @data->iter/@data->found_mode are set.
 */
static gboolean
find_mode (GtkTreeModel *model,
           GtkTreePath  *path,
           GtkTreeIter  *iter,
           gpointer      user_data)
{
        FindData *data = user_data;
        GnomeRRMode *mode = NULL;
        int width, height;

        g_assert (data->mode);
        g_assert (!data->found);

        width = gnome_rr_mode_get_width (data->mode);
        height = gnome_rr_mode_get_height (data->mode);

        gtk_tree_model_get (model, iter, COL_MODE, &mode, COL_END);
        g_assert (mode);

        if (gnome_rr_mode_get_width (mode) == width &&
            gnome_rr_mode_get_height (mode) == height) {
                data->found = TRUE;
                data->iter = *iter;
                data->found_mode = mode;
                return TRUE;
        } else {
                return FALSE;
        }
}

static void
update_resolutions (CcDisplayPage *page)
{
        GnomeRROutput *output;
        GnomeRRMode **modes, *current_mode;
        int i;

        gtk_list_store_clear (GTK_LIST_STORE (page->priv->resolution_store));

        output = gnome_rr_screen_get_output_by_name (page->priv->screen, page->priv->external_output->name);
        modes = gnome_rr_output_list_modes (output);
        current_mode = gnome_rr_output_get_current_mode (output);

        for (i = 0; modes[i] != NULL; i++) {
                GnomeRRMode *mode = modes[i];
                char *s;
                FindData data;

                /* Skip modes that are too small for our UX */
                if (gnome_rr_mode_get_width (mode) < 1024 ||
                    gnome_rr_mode_get_height (mode) < 576)
                        continue;

                data.mode = mode;
                data.found = FALSE;

                /* See if this have this resolution already in the store */
                gtk_tree_model_foreach (page->priv->resolution_store, find_mode, &data);
                if (data.found) {
                        /* This resolution is already in the store.  If this mode is a higher
                           refresh than the one in the store, replace it */
                        if (gnome_rr_mode_get_freq (data.found_mode) <
                            gnome_rr_mode_get_freq (mode)) {
                                gtk_list_store_set (GTK_LIST_STORE (page->priv->resolution_store), &data.iter,
                                                    COL_MODE, mode, COL_END);
                        }
                } else {
                        /* This resolution isn't in the store, add it */
                        s = g_strdup_printf ("%dx%d",
                                             gnome_rr_mode_get_width (mode),
                                             gnome_rr_mode_get_height (mode));

                        gtk_list_store_insert_with_values (GTK_LIST_STORE (page->priv->resolution_store), &data.iter, -1,
                                                           COL_MODE, mode, COL_NAME, s, COL_END);
                        g_free (s);
                }

                /* If this mode is the current mode, active it */
                if (mode == current_mode) {
                        gtk_combo_box_set_active_iter (GTK_COMBO_BOX (page->priv->resolution_combo), &data.iter);
                }
        }
}

static void
update_ui (CcDisplayPage *page, OutputMode mode)
{
        switch (mode) {
        case INTERNAL:
                gtk_widget_set_sensitive (page->priv->toggle, FALSE);
                mx_gtk_light_switch_set_active (MX_GTK_LIGHT_SWITCH (page->priv->toggle), FALSE);
                gtk_label_set_text (GTK_LABEL (page->priv->state_label),
                                    _("You are only showing your desktop on your computer's screen. "
                                      "Plug in another display to share your view and then turn display sharing on above."));
                gtk_widget_hide (page->priv->resolution_box);
                gtk_image_set_from_file (GTK_IMAGE (page->priv->monitor_icon), PIXMAPDIR "/display-netbook-only.png");
                break;
        case INTERNAL_EXTERNAL_PRESENT:
                gtk_widget_set_sensitive (page->priv->toggle, TRUE);
                mx_gtk_light_switch_set_active (MX_GTK_LIGHT_SWITCH (page->priv->toggle), FALSE);
                gtk_label_set_text (GTK_LABEL (page->priv->state_label),
                                    _("You are only showing your desktop on your computer's screen. "
                                      "Plug in another display to share your view and then turn display sharing on above."));
                gtk_widget_hide (page->priv->resolution_box);
                gtk_image_set_from_file (GTK_IMAGE (page->priv->monitor_icon), PIXMAPDIR "/display-netbook-only.png");
                break;
        case EXTERNAL:
                gtk_widget_set_sensitive (page->priv->toggle, TRUE);
                mx_gtk_light_switch_set_active (MX_GTK_LIGHT_SWITCH (page->priv->toggle), TRUE);
                gtk_label_set_text (GTK_LABEL (page->priv->state_label),
                                    _("You are showing your desktop on an external monitor or projector."));
                gtk_widget_show (page->priv->resolution_box);
                gtk_image_set_from_file (GTK_IMAGE (page->priv->monitor_icon), PIXMAPDIR "/display-netbook-and-external.png");

                update_resolutions (page);
                break;
  }
}

static gboolean
is_internal_screen (GnomeOutputInfo *output)
{
        g_return_val_if_fail (output, FALSE);

        /* Really hope this is good enough */
        return strncasecmp (output->name, "lvds", 4) == 0;
}

static void
on_screen_changed (GnomeRRScreen *scr,
                   CcDisplayPage *page)
{
        GnomeRRConfig *config;
        int i;

        if (page->priv->current_configuration)
                gnome_rr_config_free (page->priv->current_configuration);

        config = page->priv->current_configuration = gnome_rr_config_new_current (page->priv->screen);


        page->priv->internal_output = NULL;
        page->priv->external_output = NULL;

        for (i = 0; config->outputs[i] != NULL; i++) {
                GnomeOutputInfo *output = config->outputs[i];

                if (is_internal_screen (output)) {
                        page->priv->internal_output = output;
                } else {
                        if (output->connected)
                                page->priv->external_output = output;
                }
        }

        if (page->priv->internal_output->on &&
            !page->priv->external_output)
                update_ui (page, INTERNAL);
        else if (page->priv->internal_output->on &&
                 page->priv->external_output &&
                 !page->priv->external_output->on)
                update_ui (page, INTERNAL_EXTERNAL_PRESENT);
        else if (page->priv->external_output)
                update_ui (page, EXTERNAL);
}

static void
activate_output (GnomeOutputInfo *info, GnomeRRMode *mode)
{
        g_return_if_fail (info);
        g_return_if_fail (mode);

        info->primary = TRUE;
        info->on = TRUE;
        info->width = gnome_rr_mode_get_width (mode);
        info->height = gnome_rr_mode_get_height (mode);
        info->rate = gnome_rr_mode_get_freq (mode);
        info->rotation = GNOME_RR_ROTATION_0;
}

static void
on_toggled (MxGtkLightSwitch *toggle, gboolean state, gpointer user_data)
{
        CcDisplayPage *page = CC_DISPLAY_PAGE (user_data);
        GnomeRROutput *output;
        GnomeRRMode *mode = NULL;

        if (mx_gtk_light_switch_get_active (MX_GTK_LIGHT_SWITCH (toggle))) {
                output = gnome_rr_screen_get_output_by_name (page->priv->screen, page->priv->external_output->name);
                mode = gnome_rr_output_get_preferred_mode (output);
                if (!mode)
                        mode = gnome_rr_output_list_modes (output)[0];
                activate_output (page->priv->external_output, mode);

                page->priv->internal_output->on = FALSE;
        } else {
                output = gnome_rr_screen_get_output_by_name (page->priv->screen, page->priv->internal_output->name);
                mode = gnome_rr_output_get_preferred_mode (output);
                activate_output (page->priv->internal_output, mode);
                page->priv->external_output->on = FALSE;
        }

        page->priv->apply_button_clicked_timestamp = gtk_get_current_event_time ();

        apply (page);
}

static void
setup_page (CcDisplayPage *page)
{
        GtkBuilder      *builder;
        GtkWidget       *widget;
        GtkCellRenderer *renderer;
        GError          *error;

        builder = gtk_builder_new ();

        error = NULL;
        gtk_builder_add_from_file (builder,
                                   UIDIR
                                   "/display-minimal.ui",
                                   &error);
        if (error != NULL) {
                g_error (_("Could not load user interface file: %s"),
                         error->message);
                g_error_free (error);
                return;
        }

        page->priv->monitor_icon = WID ("monitor_icon");
        page->priv->state_label = WID ("state_label");
        page->priv->resolution_box = WID ("resolution_box");
        page->priv->resolution_combo = WID ("res_combo");
        page->priv->resolution_store = GTK_TREE_MODEL (gtk_builder_get_object (builder, "mode_store"));
        widget = WID ("apply_button");
        g_signal_connect (widget, "clicked", G_CALLBACK (on_apply_button_clicked), page);

        page->priv->toggle = mx_gtk_light_switch_new ();
        g_signal_connect (page->priv->toggle, "switch-flipped", G_CALLBACK (on_toggled), page);
        gtk_widget_show (page->priv->toggle);
        widget = WID ("hbox");
        gtk_box_pack_start (GTK_BOX (widget), page->priv->toggle, FALSE, FALSE, 0);

        renderer = gtk_cell_renderer_text_new ();
        gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (page->priv->resolution_combo), renderer, TRUE);
        gtk_cell_layout_add_attribute (GTK_CELL_LAYOUT (page->priv->resolution_combo), renderer, "text", COL_NAME);

        widget = WID ("topbox");
        gtk_widget_reparent (widget, GTK_WIDGET (page));
        gtk_widget_show (widget);

        g_object_unref (builder);
}

static GObject *
cc_display_page_constructor (GType                  type,
                             guint                  n_construct_properties,
                             GObjectConstructParam *construct_properties)
{
        CcDisplayPage      *display_page;

        display_page = CC_DISPLAY_PAGE (G_OBJECT_CLASS (cc_display_page_parent_class)->constructor (type,
                                                                                                    n_construct_properties,
                                                                                                    construct_properties));

        g_object_set (display_page,
                      "display-name", _("Displays and projectors"),
                      "id", "general",
                      NULL);

        setup_page (display_page);

        return G_OBJECT (display_page);
}

static void
start_working (CcDisplayPage *page)
{
        GError *error = NULL;
        g_debug (__FUNCTION__);
        page->priv->screen = gnome_rr_screen_new (gdk_screen_get_default (),
                                                  (GnomeRRScreenChanged) on_screen_changed,
                                                  page,
                                                  &error);
        if (page->priv->screen == NULL) {
                error_message (NULL,
                               _("Could not get screen information"),
                               error->message);
                g_error_free (error);
                return;
        }
        on_screen_changed (page->priv->screen, page);
}

static void
stop_working (CcDisplayPage *page)
{
        gnome_rr_screen_destroy (page->priv->screen);
        page->priv->screen = NULL;
}

static void
cc_display_page_active_changed (CcPage  *base_page,
                                gboolean is_active)
{
        CcDisplayPage *page = CC_DISPLAY_PAGE (base_page);
        g_debug (__FUNCTION__);

        if (is_active)
                start_working (page);
        else
                stop_working (page);

        CC_PAGE_CLASS (cc_display_page_parent_class)->active_changed (base_page, is_active);

}

static void
cc_display_page_class_init (CcDisplayPageClass *klass)
{
        GObjectClass  *object_class = G_OBJECT_CLASS (klass);
        CcPageClass   *page_class = CC_PAGE_CLASS (klass);

        page_class->active_changed = cc_display_page_active_changed;

        object_class->constructor = cc_display_page_constructor;
        object_class->finalize = cc_display_page_finalize;

        g_type_class_add_private (klass, sizeof (CcDisplayPagePrivate));
}

static void
cc_display_page_init (CcDisplayPage *page)
{
        page->priv = CC_DISPLAY_PAGE_GET_PRIVATE (page);
}

static void
cc_display_page_finalize (GObject *object)
{
        CcDisplayPage *page;

        g_return_if_fail (object != NULL);
        g_return_if_fail (CC_IS_DISPLAY_PAGE (object));

        page = CC_DISPLAY_PAGE (object);

        g_return_if_fail (page->priv != NULL);

        stop_working (page);

        G_OBJECT_CLASS (cc_display_page_parent_class)->finalize (object);
}

CcPage *
cc_display_page_new (void)
{
        GObject *object;

        object = g_object_new (CC_TYPE_DISPLAY_PAGE, NULL);

        return CC_PAGE (object);
}
