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
#include <gio/gdesktopappinfo.h>

#include "cc-region-panel.h"
#include "cc-region-resources.h"
#include "cc-language-chooser.h"
#include "cc-format-chooser.h"
#include "cc-input-chooser.h"
#include "cc-input-options.h"

#include <gtk/gtk.h>

#include "cc-common-language.h"

#define GNOME_DESKTOP_USE_UNSTABLE_API
#include <libgnome-desktop/gnome-languages.h>
#include <libgnome-desktop/gnome-xkb-info.h>

#ifdef HAVE_IBUS
#include <ibus.h>
#include "supported-ibus-engines.h"
#include "cc-ibus-utils.h"
#endif

#include "egg-list-box/egg-list-box.h"
#include <libgd/gd-notification.h>

#define GNOME_DESKTOP_INPUT_SOURCES_DIR "org.gnome.desktop.input-sources"
#define KEY_CURRENT_INPUT_SOURCE "current"
#define KEY_INPUT_SOURCES        "sources"

#define GNOME_SYSTEM_LOCALE_DIR "org.gnome.system.locale"
#define KEY_REGION "region"

#define INPUT_SOURCE_TYPE_XKB "xkb"
#define INPUT_SOURCE_TYPE_IBUS "ibus"


static gboolean
strv_contains (const gchar * const *strv,
               const gchar         *str)
{
  const gchar * const *p = strv;
  for (p = strv; *p; p++)
    if (g_strcmp0 (*p, str) == 0)
      return TRUE;

  return FALSE;
}

CC_PANEL_REGISTER (CcRegionPanel, cc_region_panel)

#define WID(s) GTK_WIDGET (gtk_builder_get_object (self->priv->builder, s))

#define REGION_PANEL_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), CC_TYPE_REGION_PANEL, CcRegionPanelPrivate))

struct _CcRegionPanelPrivate {
	GtkBuilder *builder;

        GtkWidget *login_button;
        GtkWidget *overlay;
        GtkWidget *notification;

        GtkWidget *language_row;
        GtkWidget *language_label;
        GtkWidget *formats_row;
        GtkWidget *formats_label;

        GDBusProxy *user;
        GSettings  *locale_settings;

        gchar *language;
        gchar *region;

        GtkWidget *options_button;
        GtkWidget *input_list;
        GtkWidget *add_input;
        GtkWidget *remove_input;
        GtkWidget *show_config;
        GtkWidget *show_layout;

        GSettings *input_settings;
        GnomeXkbInfo *xkb_info;
#ifdef HAVE_IBUS
        IBusBus *ibus;
        GHashTable *ibus_engines;
        GCancellable *ibus_cancellable;
#endif
};

static void
cc_region_panel_finalize (GObject *object)
{
	CcRegionPanel *self = CC_REGION_PANEL (object);
	CcRegionPanelPrivate *priv = self->priv;

        g_clear_object (&priv->builder);
        g_clear_object (&priv->user);
        g_clear_object (&priv->locale_settings);
        g_clear_object (&priv->input_settings);
        g_clear_object (&priv->xkb_info);
        g_clear_object (&priv->ibus);
        if (priv->ibus_cancellable)
                g_cancellable_cancel (priv->ibus_cancellable);
        g_clear_object (&priv->ibus_cancellable);
        g_clear_pointer (&priv->ibus_engines, g_hash_table_destroy);

	G_OBJECT_CLASS (cc_region_panel_parent_class)->finalize (object);
}

static void
cc_region_panel_constructed (GObject *object)
{
        CcRegionPanel *self = CC_REGION_PANEL (object);
	CcRegionPanelPrivate *priv = self->priv;

        G_OBJECT_CLASS (cc_region_panel_parent_class)->constructed (object);

#if 0
        priv->login_button = gtk_button_new_with_label (_("Login Screen"));

        cc_shell_embed_widget_in_header (cc_panel_get_shell (CC_PANEL (object)),
                                         priv->login_button);
        gtk_widget_show_all (priv->login_button);
#endif
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
restart_now (CcRegionPanel *self)
{
        GDBusConnection *bus;

        gd_notification_dismiss (GD_NOTIFICATION (self->priv->notification));

        g_print ("Ta Da! Restarting...\n");
        bus = g_bus_get_sync (G_BUS_TYPE_SESSION, NULL, NULL);
        g_dbus_connection_call (bus,
                                "org.gnome.SessionManager",
                                "/org/gnome/SessionManager",
                                "org.gnome.SessionManager",
                                "Logout",
                                g_variant_new ("(u)", 0),
                                NULL, 0, G_MAXINT,
                                NULL, NULL, NULL);
}

static void
show_restart_notification (CcRegionPanel *self)
{
	CcRegionPanelPrivate *priv = self->priv;
        GtkWidget *box;
        GtkWidget *label;
        GtkWidget *button;

        if (priv->notification)
                return;

        priv->notification = gd_notification_new ();
        g_object_add_weak_pointer (G_OBJECT (priv->notification),
                                   (gpointer *)&priv->notification);
        box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 24);
        gtk_widget_set_margin_left (box, 12);
        gtk_widget_set_margin_right (box, 12);
        gtk_widget_set_margin_top (box, 6);
        gtk_widget_set_margin_bottom (box, 6);
        label = gtk_label_new (_("Your session needs to be restarted for changes to take effect"));
        button = gtk_button_new_with_label (_("Restart Now"));
        g_signal_connect_swapped (button, "clicked",
                                  G_CALLBACK (restart_now), self);
        gtk_box_pack_start (GTK_BOX (box), label, FALSE, FALSE, 0);
        gtk_box_pack_start (GTK_BOX (box), button, FALSE, FALSE, 0);
        gtk_widget_show_all (box);

        gtk_container_add (GTK_CONTAINER (priv->notification), box);
        gtk_overlay_add_overlay (GTK_OVERLAY (self->priv->overlay), priv->notification);
        gtk_widget_show (priv->notification);
}

static void
update_separator_func (GtkWidget **separator,
                       GtkWidget  *child,
                       GtkWidget  *before,
                       gpointer    user_data)
{
        if (before == NULL)
                return;

        if (*separator == NULL) {
                *separator = gtk_separator_new (GTK_ORIENTATION_HORIZONTAL);
                g_object_ref_sink (*separator);
                gtk_widget_show (*separator);
        }
}

static gboolean
update_language (CcRegionPanel *self,
                 const gchar   *language)
{
	CcRegionPanelPrivate *priv = self->priv;
        gchar *name;

        if (g_strcmp0 (language, priv->language) == 0)
                return FALSE;

        g_free (priv->language);
        priv->language = g_strdup (language);

        name = gnome_get_language_from_name (language, language);
        gtk_label_set_label (GTK_LABEL (priv->language_label), name);
        g_free (name);

        g_dbus_proxy_call (priv->user,
                           "SetLanguage",
                           g_variant_new ("(s)", priv->language),
                           0,
                           G_MAXINT,
                           NULL,
                           NULL,
                           NULL);

        return TRUE;
}

static void
language_response (GtkDialog     *chooser,
                   gint           response_id,
                   CcRegionPanel *self)
{
        const gchar *language;
        gboolean changed;

        changed = FALSE;

        if (response_id == GTK_RESPONSE_OK) {
                language = cc_language_chooser_get_language (GTK_WIDGET (chooser));
                changed = update_language (self, language);
        }

        gtk_widget_destroy (GTK_WIDGET (chooser));

        if (changed)
                show_restart_notification (self);
}

static gboolean
update_region (CcRegionPanel *self,
               const gchar   *region)
{
	CcRegionPanelPrivate *priv = self->priv;
        const gchar *old_region;
        old_region = g_settings_get_string (priv->locale_settings, KEY_REGION);
        if (g_strcmp0 (region, old_region) != 0) {
                g_settings_set_string (priv->locale_settings, KEY_REGION, region);
                return TRUE;
        }
        return FALSE;
}

static void
format_response (GtkDialog *chooser,
                 gint       response_id,
                 CcRegionPanel *self)
{
        const gchar *region;
        gboolean changed;

        changed = FALSE;

        if (response_id == GTK_RESPONSE_OK) {
                region = cc_format_chooser_get_region (GTK_WIDGET (chooser));
                changed = update_region (self, region);
        }

        gtk_widget_destroy (GTK_WIDGET (chooser));

        if (changed)
                show_restart_notification (self);
}

static void
activate_language_child (CcRegionPanel *self, GtkWidget *child)
{
	CcRegionPanelPrivate *priv = self->priv;
        GtkWidget *chooser;
        GtkWidget *toplevel;

        toplevel = gtk_widget_get_toplevel (GTK_WIDGET (self));
        if (child == priv->language_row) {
                chooser = cc_language_chooser_new (toplevel);
                cc_language_chooser_set_language (chooser, priv->language);
                g_signal_connect (chooser, "response",
                                  G_CALLBACK (language_response), self);
                gtk_window_present (GTK_WINDOW (chooser));
        } else if (child == priv->formats_row) {
                chooser = cc_format_chooser_new (toplevel);
                cc_format_chooser_set_region (chooser, priv->region);
                g_signal_connect (chooser, "response",
                                  G_CALLBACK (format_response), self);
                gtk_window_present (GTK_WINDOW (chooser));
        }
}

static void
set_initial_language (CcRegionPanel *self)
{
	CcRegionPanelPrivate *priv = self->priv;
        GVariant *p;
        gchar *lang;

        p = g_dbus_proxy_get_cached_property (priv->user, "Language");
        lang = g_variant_dup_string (p, NULL);
        update_language (self, lang);
        g_free (lang);
        g_variant_unref (p);
}

static void
update_region_from_setting (CcRegionPanel *self)
{
	CcRegionPanelPrivate *priv = self->priv;
        gchar *name;

        g_free (priv->region);
        priv->region = g_settings_get_string (priv->locale_settings, KEY_REGION);
        name = gnome_get_region_from_name (priv->region, priv->region);
        gtk_label_set_label (GTK_LABEL (priv->formats_label), name);
        g_free (name);
}

static void
setup_language_section (CcRegionPanel *self)
{
	CcRegionPanelPrivate *priv = self->priv;
        GtkWidget *widget;
        gchar *path;
        GError *error = NULL;

        path = g_strdup_printf ("/org/freedesktop/Accounts/User%d", getuid ());
        priv->user = g_dbus_proxy_new_for_bus_sync (G_BUS_TYPE_SYSTEM,
                                                    G_DBUS_PROXY_FLAGS_NONE,
                                                    NULL,
                                                    "org.freedesktop.Accounts",
                                                    path,
                                                    "org.freedesktop.Accounts.User",
                                                     NULL,
                                                     &error);
        if (priv->user == NULL) {
                g_warning ("Failed to get proxy for user '%s': %s",
                           path, error->message);
                g_error_free (error);
                g_free (path);
                return;
        }
        g_free (path);

        priv->locale_settings = g_settings_new (GNOME_SYSTEM_LOCALE_DIR);
        g_signal_connect_swapped (priv->locale_settings, "changed::" KEY_REGION,
                                  G_CALLBACK (update_region_from_setting), self);

        priv->language_row = WID ("language_row");
        priv->language_label = WID ("language_label");
        priv->formats_row = WID ("formats_row");
        priv->formats_label = WID ("formats_label");

        widget = WID ("language_list");
        egg_list_box_set_selection_mode (EGG_LIST_BOX (widget),
                                         GTK_SELECTION_NONE);
        egg_list_box_set_separator_funcs (EGG_LIST_BOX (widget),
                                          update_separator_func,
                                          NULL, NULL);
        g_signal_connect_swapped (widget, "child-activated",
                                  G_CALLBACK (activate_language_child), self);

        set_initial_language (self);
        update_region_from_setting (self);
}

#ifdef HAVE_IBUS
static void
update_ibus_active_sources (CcRegionPanel *self)
{
        CcRegionPanelPrivate *priv = self->priv;
        GList *rows, *l;
        GtkWidget *row;
        const gchar *type;
        const gchar *id;
        IBusEngineDesc *engine_desc;
        gchar *display_name;
        GtkWidget *label;

        rows = gtk_container_get_children (GTK_CONTAINER (priv->input_list));
        for (l = rows; l; l = l->next) {
                row = l->data;
                type = g_object_get_data (G_OBJECT (row), "type");
                id = g_object_get_data (G_OBJECT (row), "id");
                if (g_strcmp0 (type, INPUT_SOURCE_TYPE_IBUS) != 0)
                        continue;

                engine_desc = g_hash_table_lookup (priv->ibus_engines, id);
                if (engine_desc) {
                        display_name = engine_get_display_name (engine_desc);
                        label = GTK_WIDGET (g_object_get_data (G_OBJECT (row), "label"));
                        gtk_label_set_text (GTK_LABEL (label), display_name);
                        g_free (display_name);
                }
        }
        g_list_free (rows);
}

static void
fetch_ibus_engines_result (GObject       *object,
                           GAsyncResult  *result,
                           CcRegionPanel *self)
{
        CcRegionPanelPrivate *priv = self->priv;
        gboolean show_all_sources;
        GList *list, *l;
        GError *error;

        error = NULL;
        list = ibus_bus_list_engines_async_finish (priv->ibus, result, &error);
        g_clear_object (&priv->ibus_cancellable);
        if (!list && error) {
                g_warning ("Couldn't finish IBus request: %s", error->message);
                g_error_free (error);
                return;
        }

        show_all_sources = g_settings_get_boolean (priv->input_settings, "show-all-sources");

        /* Maps engine ids to engine description objects */
        priv->ibus_engines = g_hash_table_new_full (g_str_hash, g_str_equal, NULL, g_object_unref);

        for (l = list; l; l = l->next) {
                IBusEngineDesc *engine = l->data;
                const gchar *engine_id = ibus_engine_desc_get_name (engine);

                if (show_all_sources || strv_contains (supported_ibus_engines, engine_id))
                        g_hash_table_replace (priv->ibus_engines, (gpointer)engine_id, engine);
                else
                        g_object_unref (engine);
        }
        g_list_free (list);

        update_ibus_active_sources (self);
}

static void
fetch_ibus_engines (CcRegionPanel *self)
{
        CcRegionPanelPrivate *priv = self->priv;

        priv->ibus_cancellable = g_cancellable_new ();

        ibus_bus_list_engines_async (priv->ibus,
                                     -1,
                                     priv->ibus_cancellable,
                                     (GAsyncReadyCallback)fetch_ibus_engines_result,
                                     self);

  /* We've got everything we needed, don't want to be called again. */
  g_signal_handlers_disconnect_by_func (priv->ibus, fetch_ibus_engines, self);
}

static void
maybe_start_ibus (void)
{
        /* IBus doesn't export API in the session bus. The only thing
         * we have there is a well known name which we can use as a
         * sure-fire way to activate it.
         */
        g_bus_unwatch_name (g_bus_watch_name (G_BUS_TYPE_SESSION,
                                              IBUS_SERVICE_IBUS,
                                              G_BUS_NAME_WATCHER_FLAGS_AUTO_START,
                                              NULL,
                                              NULL,
                                              NULL,
                                              NULL));
}

static GDesktopAppInfo *
setup_app_info_for_id (const gchar *id)
{
  GDesktopAppInfo *app_info;
  gchar *desktop_file_name;
  gchar **strv;

  strv = g_strsplit (id, ":", 2);
  desktop_file_name = g_strdup_printf ("ibus-setup-%s.desktop", strv[0]);
  g_strfreev (strv);

  app_info = g_desktop_app_info_new (desktop_file_name);
  g_free (desktop_file_name);

  return app_info;
}
#endif

static GtkWidget *
add_input_row (CcRegionPanel   *self,
               const gchar     *type,
               const gchar     *id,
               const gchar     *name,
               GDesktopAppInfo *app_info)
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

        if (strcmp (type, INPUT_SOURCE_TYPE_IBUS) == 0) {
                image = gtk_image_new_from_icon_name ("system-run-symbolic", GTK_ICON_SIZE_BUTTON);
                gtk_widget_set_margin_left (image, 20);
                gtk_widget_set_margin_right (image, 20);
                gtk_widget_set_margin_top (image, 6);
                gtk_widget_set_margin_bottom (image, 6);
                gtk_box_pack_start (GTK_BOX (row), image, FALSE, TRUE, 0);
        }

        gtk_widget_show_all (row);
        gtk_container_add (GTK_CONTAINER (self->priv->input_list), row);

        g_object_set_data (G_OBJECT (row), "label", label);
        g_object_set_data (G_OBJECT (row), "type", (gpointer)type);
        g_object_set_data_full (G_OBJECT (row), "id", g_strdup (id), g_free);
        if (app_info) {
                g_object_set_data_full (G_OBJECT (row), "app-info", g_object_ref (app_info), g_object_unref);
        }

        return row;
}

static void
populate_with_active_sources (CcRegionPanel *self)
{
        CcRegionPanelPrivate *priv = self->priv;
        GVariant *sources;
        GVariantIter iter;
        const gchar *type;
        const gchar *id;
        const gchar *name;
        gchar *display_name;
        GDesktopAppInfo *app_info;

        sources = g_settings_get_value (priv->input_settings, "sources");
        g_variant_iter_init (&iter, sources);
        while (g_variant_iter_next (&iter, "(&s&s)", &type, &id)) {
                display_name = NULL;
                app_info = NULL;

                if (g_str_equal (type, INPUT_SOURCE_TYPE_XKB)) {
                        gnome_xkb_info_get_layout_info (priv->xkb_info, id, &name, NULL, NULL, NULL);
                        if (!name) {
                                g_warning ("Couldn't find XKB input source '%s'", id);
                                continue;
                        }
                        display_name = g_strdup (name);
                        type = INPUT_SOURCE_TYPE_XKB;
#ifdef HAVE_IBUS
                } else if (g_str_equal (type, INPUT_SOURCE_TYPE_IBUS)) {
                        IBusEngineDesc *engine_desc = NULL;

                        if (priv->ibus_engines)
                                engine_desc = g_hash_table_lookup (priv->ibus_engines, id);
                        if (engine_desc)
                                display_name = engine_get_display_name (engine_desc);

                        app_info = setup_app_info_for_id (id);
                        type = INPUT_SOURCE_TYPE_IBUS;
#endif
                } else {
                        g_warning ("Unhandled input source type '%s'", type);
                        continue;
                }

                add_input_row (self, type, id, display_name ? display_name : id, app_info);
                g_free (display_name);
                g_clear_object (&app_info);
        }
        g_variant_unref (sources);
}

static void
container_remove_all (GtkContainer *container)
{
        GList *list, *l;
        list = gtk_container_get_children (container);
        for (l = list; l; l = l->next) {
                gtk_container_remove (container, GTK_WIDGET (l->data));
        }
        g_list_free (list);
}

static void
select_by_id (GtkWidget   *row,
              gpointer     data)
{
        const gchar *id = data;
        const gchar *row_id;

        row_id = (const gchar *)g_object_get_data (G_OBJECT (row), "id");
        if (g_strcmp0 (row_id, id) == 0)
                egg_list_box_select_child (EGG_LIST_BOX (gtk_widget_get_parent (row)), row);
}

static void
select_input (CcRegionPanel *self,
              const gchar   *id)
{
        gtk_container_foreach (GTK_CONTAINER (self->priv->input_list),
                               select_by_id, (gpointer)id);
}

static void
input_sources_changed (GSettings     *settings,
                       const gchar   *key,
                       CcRegionPanel *self)
{
	CcRegionPanelPrivate *priv = self->priv;
        GtkWidget *selected;
        const gchar *id = NULL;

        selected = egg_list_box_get_selected_child (EGG_LIST_BOX (priv->input_list));
        if (selected)
                id = (const gchar *)g_object_get_data (G_OBJECT (selected), "id");
        container_remove_all (GTK_CONTAINER (priv->input_list));
        populate_with_active_sources (self);
        if (id)
                select_input (self, id);
}


static void
update_button_sensitivity (CcRegionPanel *self)
{
	CcRegionPanelPrivate *priv = self->priv;
        GtkWidget *selected;
        GList *children;
        gboolean multiple_sources;

        children = gtk_container_get_children (GTK_CONTAINER (priv->input_list));
        multiple_sources = g_list_next (children) != NULL;
        g_list_free (children);

        selected = egg_list_box_get_selected_child (EGG_LIST_BOX (priv->input_list));
        if (selected == NULL) {
                gtk_widget_set_sensitive (priv->remove_input, FALSE);
                gtk_widget_set_sensitive (priv->show_config, FALSE);
                gtk_widget_set_sensitive (priv->show_layout, FALSE);
        } else {
                GDesktopAppInfo *app_info;

                app_info = (GDesktopAppInfo *)g_object_get_data (G_OBJECT (selected), "app-info");

                gtk_widget_set_sensitive (priv->show_config, app_info != NULL);
                gtk_widget_set_sensitive (priv->show_layout, TRUE);
                gtk_widget_set_sensitive (priv->remove_input, multiple_sources);
        }
        gtk_widget_set_visible (priv->options_button, multiple_sources);
}

static void
update_configuration (CcRegionPanel *self)
{
	CcRegionPanelPrivate *priv = self->priv;
        const gchar *type;
        const gchar *id;
        GVariantBuilder builder;
        GVariant *old_sources;
        const gchar *old_current_type;
        const gchar *old_current_id;
        guint old_current;
        guint old_n_sources;
        guint index;
        GList *list, *l;

        old_sources = g_settings_get_value (priv->input_settings, KEY_INPUT_SOURCES);
        old_current = g_settings_get_uint (priv->input_settings, KEY_CURRENT_INPUT_SOURCE);
        old_n_sources = g_variant_n_children (old_sources);

        if (old_n_sources > 0 && old_current < old_n_sources) {
                g_variant_get_child (old_sources, old_current,
                                     "(&s&s)", &old_current_type, &old_current_id);
        } else {
                old_current_type = "";
                old_current_id = "";
        }

        g_variant_builder_init (&builder, G_VARIANT_TYPE ("a(ss)"));
        index = 0;
        list = gtk_container_get_children (GTK_CONTAINER (priv->input_list));
        for (l = list; l; l = l->next) {
                type = (const gchar *)g_object_get_data (G_OBJECT (l->data), "type");
                id = (const gchar *)g_object_get_data (G_OBJECT (l->data), "id");
                if (index != old_current &&
                    g_str_equal (type, old_current_type) &&
                    g_str_equal (id, old_current_id)) {
                        g_settings_set_uint (priv->input_settings, KEY_CURRENT_INPUT_SOURCE, index);
                }
                g_variant_builder_add (&builder, "(ss)", type, id);
                index += 1;
        }
        g_list_free (list);

        g_settings_set_value (priv->input_settings, KEY_INPUT_SOURCES, g_variant_builder_end (&builder));
        g_settings_apply (priv->input_settings);

        g_variant_unref (old_sources);
}

static void
select_input_child (CcRegionPanel *self, GtkWidget *child)
{
        update_button_sensitivity (self);
}

static void
chooser_response (GtkWidget *chooser, gint response_id, gpointer data)
{
	CcRegionPanel *self = data;
	CcRegionPanelPrivate *priv = self->priv;
        gchar *type;
        gchar *id;
        gchar *name;
        GDesktopAppInfo *app_info = NULL;

        if (cc_input_chooser_get_selected (chooser, &type, &id, &name)) {
                if (g_str_equal (type, INPUT_SOURCE_TYPE_IBUS)) {
                        g_free (type);
                        type = INPUT_SOURCE_TYPE_IBUS;
#ifdef HAVE_IBUS
                        app_info = setup_app_info_for_id (id);
#endif
                } else {
                        g_free (type);
                        type = INPUT_SOURCE_TYPE_XKB;
                }
                add_input_row (self, type, id, name, app_info);
                g_free (id);
                g_free (name);
                g_clear_object (&app_info);

                update_button_sensitivity (self);
                update_configuration (self);
        }

        gtk_widget_destroy (chooser);
}

static void
add_input (CcRegionPanel *self)
{
	CcRegionPanelPrivate *priv = self->priv;
        GtkWidget *chooser;
        GtkWidget *toplevel;

        toplevel = gtk_widget_get_toplevel (GTK_WIDGET (self));
        chooser = cc_input_chooser_new (GTK_WINDOW (toplevel),
                                        priv->xkb_info,
                                        priv->ibus_engines);
        g_signal_connect (chooser, "response",
                          G_CALLBACK (chooser_response), self);
}

static GtkWidget *
find_sibling (GtkContainer *container, GtkWidget *child)
{
        GList *list, *c;
        GList *l;
        GtkWidget *sibling;

        list = gtk_container_get_children (container);
        c = g_list_find (list, child);

        for (l = c->next; l; l = l->next) {
                sibling = l->data;
                if (gtk_widget_get_visible (sibling) && gtk_widget_get_child_visible (sibling))
                        goto out;
        }

        for (l = c->prev; l; l = l->prev) {
                sibling = l->data;
                if (gtk_widget_get_visible (sibling) && gtk_widget_get_child_visible (sibling))
                        goto out;
        }

        sibling = NULL;

out:
        g_list_free (list);

        return sibling;
}

static void
remove_selected_input (CcRegionPanel *self)
{
	CcRegionPanelPrivate *priv = self->priv;
        GtkWidget *selected;
        GtkWidget *sibling;

        selected = egg_list_box_get_selected_child (EGG_LIST_BOX (priv->input_list));
        if (selected == NULL)
                return;

        sibling = find_sibling (GTK_CONTAINER (priv->input_list), selected);
        gtk_container_remove (GTK_CONTAINER (priv->input_list), selected);
        egg_list_box_select_child (EGG_LIST_BOX (priv->input_list), sibling);

        update_button_sensitivity (self);
        update_configuration (self);
}

static void
show_selected_settings (CcRegionPanel *self)
{
	CcRegionPanelPrivate *priv = self->priv;
        GtkWidget *selected;
        GdkAppLaunchContext *ctx;
        GDesktopAppInfo *app_info;
        const gchar *id;
        GError *error = NULL;

        selected = egg_list_box_get_selected_child (EGG_LIST_BOX (priv->input_list));
        if (selected == NULL)
                return;

        app_info = (GDesktopAppInfo *)g_object_get_data (G_OBJECT (selected), "app-info");
        if  (app_info == NULL)
                return;

        ctx = gdk_display_get_app_launch_context (gdk_display_get_default ());
        gdk_app_launch_context_set_timestamp (ctx, gtk_get_current_event_time ());

        id = (const gchar *)g_object_get_data (G_OBJECT (selected), "id");
        g_app_launch_context_setenv (G_APP_LAUNCH_CONTEXT (ctx),
                                     "IBUS_ENGINE_NAME", id);

        if (!g_app_info_launch (G_APP_INFO (app_info), NULL, G_APP_LAUNCH_CONTEXT (ctx), &error)) {
                g_warning ("Failed to launch input source setup: %s", error->message);
                g_error_free (error);
        }

        g_object_unref (ctx);
}

static void
show_selected_layout (CcRegionPanel *self)
{
	CcRegionPanelPrivate *priv = self->priv;
        GtkWidget *selected;
        const gchar *type;
        const gchar *id;
        const gchar *layout;
        const gchar *variant;
        gchar *commandline;

        selected = egg_list_box_get_selected_child (EGG_LIST_BOX (priv->input_list));
        if (selected == NULL)
                return;

        type = (const gchar *)g_object_get_data (G_OBJECT (selected), "type");
        id = (const gchar *)g_object_get_data (G_OBJECT (selected), "id");

        if (g_str_equal (type, INPUT_SOURCE_TYPE_XKB)) {
                gnome_xkb_info_get_layout_info (priv->xkb_info,
                                                id, NULL, NULL,
                                                &layout, &variant);

                if (!layout || !layout[0]) {
                        g_warning ("Couldn't find XKB input source '%s'", id);
                        return;
                }
#ifdef HAVE_IBUS
        } else if (g_str_equal (type, INPUT_SOURCE_TYPE_IBUS)) {
                IBusEngineDesc *engine_desc = NULL;

                if (priv->ibus_engines)
                        engine_desc = g_hash_table_lookup (priv->ibus_engines, id);

                if (engine_desc) {
                        layout = ibus_engine_desc_get_layout (engine_desc);
                        variant = "";
                } else {
                        g_warning ("Couldn't find IBus input source '%s'", id);
                        return;
                }
#endif
        } else {
                g_warning ("Unhandled input source type '%s'", type);
                return;
        }

        if (variant[0])
                commandline = g_strdup_printf ("gkbd-keyboard-display -l \"%s\t%s\"",
                                               layout, variant);
        else
                commandline = g_strdup_printf ("gkbd-keyboard-display -l %s",
                                               layout);

        g_spawn_command_line_async (commandline, NULL);
        g_free (commandline);
}

static void
options_response (GtkDialog     *options,
                  gint           response_id,
                  CcRegionPanel *self)
{
        gtk_widget_destroy (GTK_WIDGET (options));
}


static void
show_input_options (CcRegionPanel *self)
{
        GtkWidget *toplevel;
        GtkWidget *options;

        toplevel = gtk_widget_get_toplevel (GTK_WIDGET (self));
        options = cc_input_options_new (toplevel);
        g_signal_connect (options, "response",
                          G_CALLBACK (options_response), self);
        gtk_window_present (GTK_WINDOW (options));
}

static void
setup_input_section (CcRegionPanel *self)
{
	CcRegionPanelPrivate *priv = self->priv;

        priv->input_settings = g_settings_new (GNOME_DESKTOP_INPUT_SOURCES_DIR);
        g_settings_delay (priv->input_settings);

        priv->xkb_info = gnome_xkb_info_new ();

#ifdef HAVE_IBUS
        ibus_init ();
        if (!priv->ibus) {
                priv->ibus = ibus_bus_new_async ();
                if (ibus_bus_is_connected (priv->ibus))
                        fetch_ibus_engines (self);
                else
                        g_signal_connect_swapped (priv->ibus, "connected",
                                                  G_CALLBACK (fetch_ibus_engines), self);
        }
        maybe_start_ibus ();
#endif

        priv->options_button = WID ("input_options");
        priv->input_list = WID ("input_list");
        priv->add_input = WID ("input_source_add");
        priv->remove_input = WID ("input_source_remove");
        priv->show_config = WID ("input_source_config");
        priv->show_layout = WID ("input_source_layout");

        g_signal_connect_swapped (priv->options_button, "clicked",
                                  G_CALLBACK (show_input_options), self);
        g_signal_connect_swapped (priv->add_input, "clicked",
                                  G_CALLBACK (add_input), self);
        g_signal_connect_swapped (priv->remove_input, "clicked",
                                  G_CALLBACK (remove_selected_input), self);
        g_signal_connect_swapped (priv->show_config, "clicked",
                                  G_CALLBACK (show_selected_settings), self);
        g_signal_connect_swapped (priv->show_layout, "clicked",
                                  G_CALLBACK (show_selected_layout), self);

        egg_list_box_set_selection_mode (EGG_LIST_BOX (priv->input_list),
                                         GTK_SELECTION_SINGLE);
        egg_list_box_set_separator_funcs (EGG_LIST_BOX (priv->input_list),
                                          update_separator_func,
                                          NULL, NULL);
        g_signal_connect_swapped (priv->input_list, "child-selected",
                                  G_CALLBACK (select_input_child), self);

        g_signal_connect (priv->input_settings, "changed::" KEY_INPUT_SOURCES,
                          G_CALLBACK (input_sources_changed), self);

        populate_with_active_sources (self);

        update_button_sensitivity (self);
}

static void
cc_region_panel_init (CcRegionPanel *self)
{
	CcRegionPanelPrivate *priv;
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

        setup_language_section (self);
        setup_input_section (self);

        priv->overlay = GTK_WIDGET (gtk_builder_get_object (priv->builder, "overlay"));
	gtk_widget_reparent (priv->overlay, GTK_WIDGET (self));
}
