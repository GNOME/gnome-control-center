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
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 *
 * Author: Sergey Udaltsov <svu@gnome.org>
 *
 */

#include <config.h>
#include <errno.h>
#include <locale.h>
#include <glib/gi18n.h>
#include <gio/gio.h>
#include <gio/gdesktopappinfo.h>
#include <gtk/gtk.h>
#include <polkit/polkit.h>

#include "list-box-helper.h"
#include "cc-region-panel.h"
#include "cc-region-resources.h"
#include "cc-language-chooser.h"
#include "cc-format-chooser.h"

#include "cc-common-language.h"

#define GNOME_DESKTOP_USE_UNSTABLE_API
#include <libgnome-desktop/gnome-languages.h>
#include <libgnome-desktop/gnome-xkb-info.h>

#include <act/act.h>

#define GNOME_SYSTEM_LOCALE_DIR "org.gnome.system.locale"
#define KEY_REGION "region"

#define DEFAULT_LOCALE "en_US.utf-8"

struct _CcRegionPanel {
        CcPanel          parent_instance;

        GtkLabel        *formats_label;
        GtkListBox      *formats_list;
        GtkListBoxRow   *formats_row;
        GtkSizeGroup    *input_size_group;
        GtkLabel        *login_label;
        GtkLabel        *language_label;
        GtkListBox      *language_list;
        GtkListBoxRow   *language_row;
        GtkFrame        *language_section_frame;
        GtkToggleButton *login_language_button;
        GtkButton       *restart_button;
        GtkRevealer     *restart_revealer;
        GtkBox          *session_or_login_box;

        gboolean     login;
        gboolean     login_auto_apply;
        GPermission *permission;
        GDBusProxy  *localed;
        GDBusProxy  *session;

        ActUserManager *user_manager;
        ActUser        *user;
        GSettings      *locale_settings;

        gchar *language;
        gchar *region;
        gchar *system_language;
        gchar *system_region;
};

CC_PANEL_REGISTER (CcRegionPanel, cc_region_panel)

static void update_region (CcRegionPanel *self, const gchar *region);

static void
cc_region_panel_finalize (GObject *object)
{
        CcRegionPanel *self = CC_REGION_PANEL (object);
        GtkWidget *chooser;

        if (self->user_manager) {
                g_signal_handlers_disconnect_by_data (self->user_manager, self);
                self->user_manager = NULL;
        }

        if (self->user) {
                g_signal_handlers_disconnect_by_data (self->user, self);
                self->user = NULL;
        }

        g_clear_object (&self->permission);
        g_clear_object (&self->localed);
        g_clear_object (&self->session);
        g_clear_object (&self->locale_settings);
        g_free (self->language);
        g_free (self->region);
        g_free (self->system_language);
        g_free (self->system_region);

        chooser = g_object_get_data (G_OBJECT (self), "input-chooser");
        if (chooser)
                gtk_widget_destroy (chooser);

        G_OBJECT_CLASS (cc_region_panel_parent_class)->finalize (object);
}

static const char *
cc_region_panel_get_help_uri (CcPanel *panel)
{
        return "help:gnome-help/prefs-language";
}

static GFile *
get_needs_restart_file (void)
{
        g_autofree gchar *path = NULL;

        path = g_build_filename (g_get_user_runtime_dir (),
                                 "gnome-control-center-region-needs-restart",
                                 NULL);
        return g_file_new_for_path (path);
}

static void
restart_now (CcRegionPanel *self)
{
        g_autoptr(GFile) file = NULL;

        file = get_needs_restart_file ();
        g_file_delete (file, NULL, NULL);

        g_dbus_proxy_call (self->session,
                           "Logout",
                           g_variant_new ("(u)", 0),
                           G_DBUS_CALL_FLAGS_NONE,
                           -1, NULL, NULL, NULL);
}

static void
set_restart_notification_visible (CcRegionPanel *self,
                                  const gchar   *locale,
                                  gboolean       visible)
{
        locale_t new_locale;
        locale_t current_locale;
        g_autoptr(GFile) file = NULL;
        g_autoptr(GFileOutputStream) output_stream = NULL;
        g_autoptr(GError) error = NULL;

        if (locale) {
                new_locale = newlocale (LC_MESSAGES_MASK, locale, (locale_t) 0);
                if (new_locale == (locale_t) 0)
                        g_warning ("Failed to create locale %s: %s", locale, g_strerror (errno));
                else
                        current_locale = uselocale (new_locale);
        }

        gtk_revealer_set_reveal_child (self->restart_revealer, visible);

        if (locale && new_locale != (locale_t) 0) {
                uselocale (current_locale);
                freelocale (new_locale);
        }

        file = get_needs_restart_file ();

        if (!visible) {
                g_file_delete (file, NULL, NULL);
                return;
        }

        output_stream = g_file_create (file, G_FILE_CREATE_NONE, NULL, &error);
        if (output_stream == NULL) {
                if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_EXISTS))
                        g_warning ("Unable to create %s: %s", g_file_get_path (file), error->message);
        }
}

typedef struct {
        CcRegionPanel *self;
        int category;
        gchar *target_locale;
} MaybeNotifyData;

static void
maybe_notify_data_free (MaybeNotifyData *data)
{
        g_free (data->target_locale);
        g_free (data);
}

G_DEFINE_AUTOPTR_CLEANUP_FUNC (MaybeNotifyData, maybe_notify_data_free)

static void
maybe_notify_finish (GObject      *source,
                     GAsyncResult *res,
                     gpointer      data)
{
        g_autoptr(MaybeNotifyData) mnd = data;
        CcRegionPanel *self = mnd->self;
        g_autoptr(GError) error = NULL;
        g_autoptr(GVariant) retval = NULL;
        g_autofree gchar *current_lang_code = NULL;
        g_autofree gchar *current_country_code = NULL;
        g_autofree gchar *target_lang_code = NULL;
        g_autofree gchar *target_country_code = NULL;
        const gchar *current_locale = NULL;

        retval = g_dbus_proxy_call_finish (G_DBUS_PROXY (source), res, &error);
        if (!retval) {
                if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
                        g_warning ("Failed to get locale: %s\n", error->message);
                return;
        }

        g_variant_get (retval, "(&s)", &current_locale);

        if (!gnome_parse_locale (current_locale,
                                 &current_lang_code,
                                 &current_country_code,
                                 NULL,
                                 NULL))
                return;

        if (!gnome_parse_locale (mnd->target_locale,
                                 &target_lang_code,
                                 &target_country_code,
                                 NULL,
                                 NULL))
                return;

        if (g_str_equal (current_lang_code, target_lang_code) == FALSE ||
            g_str_equal (current_country_code, target_country_code) == FALSE)
                set_restart_notification_visible (self,
                                                  mnd->category == LC_MESSAGES ? mnd->target_locale : NULL,
                                                  TRUE);
        else
                set_restart_notification_visible (self,
                                                  mnd->category == LC_MESSAGES ? mnd->target_locale : NULL,
                                                  FALSE);
}

static void
maybe_notify (CcRegionPanel *self,
              int            category,
              const gchar   *target_locale)
{
        MaybeNotifyData *mnd;

        mnd = g_new0 (MaybeNotifyData, 1);
        mnd->self = self;
        mnd->category = category;
        mnd->target_locale = g_strdup (target_locale);

        g_dbus_proxy_call (self->session,
                           "GetLocale",
                           g_variant_new ("(i)", category),
                           G_DBUS_CALL_FLAGS_NONE,
                           -1,
                           cc_panel_get_cancellable (CC_PANEL (self)),
                           maybe_notify_finish,
                           mnd);
}

static void set_localed_locale (CcRegionPanel *self);

static void
set_system_language (CcRegionPanel *self,
                     const gchar   *language)
{
        if (g_strcmp0 (language, self->system_language) == 0)
                return;

        g_free (self->system_language);
        self->system_language = g_strdup (language);

        set_localed_locale (self);
}

static void
update_language (CcRegionPanel *self,
                 const gchar   *language)
{
        if (self->login) {
                set_system_language (self, language);
        } else {
                if (g_strcmp0 (language, self->language) == 0)
                        return;
                act_user_set_language (self->user, language);
                if (self->login_auto_apply)
                        set_system_language (self, language);
                maybe_notify (self, LC_MESSAGES, language);
        }
}

static void
language_response (CcRegionPanel     *self,
                   gint               response_id,
                   CcLanguageChooser *chooser)
{
        const gchar *language;

        if (response_id == GTK_RESPONSE_OK) {
                language = cc_language_chooser_get_language (chooser);
                update_language (self, language);

                /* Keep format strings consistent with the user's language */
                update_region (self, NULL);
        }

        gtk_widget_destroy (GTK_WIDGET (chooser));
}

static void
set_system_region (CcRegionPanel *self,
                   const gchar   *region)
{
        if (g_strcmp0 (region, self->system_region) == 0)
                return;

        g_free (self->system_region);
        self->system_region = g_strdup (region);

        set_localed_locale (self);
}

static void
update_region (CcRegionPanel *self,
               const gchar   *region)
{
        if (self->login) {
                set_system_region (self, region);
        } else {
                if (g_strcmp0 (region, self->region) == 0)
                        return;
                if (region == NULL || region[0] == '\0')
                        g_settings_reset (self->locale_settings, KEY_REGION);
                else
                        g_settings_set_string (self->locale_settings, KEY_REGION, region);
                if (self->login_auto_apply)
                        set_system_region (self, region);
                maybe_notify (self, LC_TIME, region);
        }
}

static void
format_response (CcRegionPanel   *self,
                 gint             response_id,
                 CcFormatChooser *chooser)
{
        const gchar *region;

        if (response_id == GTK_RESPONSE_OK) {
                region = cc_format_chooser_get_region (chooser);
                update_region (self, region);
        }

        gtk_widget_destroy (GTK_WIDGET (chooser));
}

static const gchar *
get_effective_language (CcRegionPanel *self)
{
        if (self->login)
                return self->system_language;
        else
                return self->language;
}

static void
show_language_chooser (CcRegionPanel *self)
{
        CcLanguageChooser *chooser;

        chooser = cc_language_chooser_new ();
        gtk_window_set_transient_for (GTK_WINDOW (chooser), GTK_WINDOW (gtk_widget_get_toplevel (GTK_WIDGET (self))));
        cc_language_chooser_set_language (chooser, get_effective_language (self));
        g_signal_connect_object (chooser, "response",
                                 G_CALLBACK (language_response), self, G_CONNECT_SWAPPED);
        gtk_window_present (GTK_WINDOW (chooser));
}

static const gchar *
get_effective_region (CcRegionPanel *self)
{
        const gchar *region;

        if (self->login)
                region = self->system_region;
        else
                region = self->region;

        /* Region setting might be empty - show the language because
         * that's what LC_TIME and others will effectively be when the
         * user logs in again. */
        if (region == NULL || region[0] == '\0')
                region = get_effective_language (self);

        return region;
}

static void
show_region_chooser (CcRegionPanel *self)
{
        CcFormatChooser *chooser;

        chooser = cc_format_chooser_new ();
        gtk_window_set_transient_for (GTK_WINDOW (chooser), GTK_WINDOW (gtk_widget_get_toplevel (GTK_WIDGET (self))));
        cc_format_chooser_set_region (chooser, get_effective_region (self));
        g_signal_connect_object (chooser, "response",
                                 G_CALLBACK (format_response), self, G_CONNECT_SWAPPED);
        gtk_window_present (GTK_WINDOW (chooser));
}

static gboolean
permission_acquired (GPermission *permission, GAsyncResult *res, const gchar *action)
{
        g_autoptr(GError) error = NULL;

        if (!g_permission_acquire_finish (permission, res, &error)) {
                if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
                        g_warning ("Failed to acquire permission to %s: %s\n", error->message, action);
                return FALSE;
        }

        return FALSE;
}

static void
choose_language_permission_cb (GObject *source, GAsyncResult *res, gpointer user_data)
{
        CcRegionPanel *self = user_data;
        if (permission_acquired (G_PERMISSION (source), res, "choose language"))
                show_language_chooser (self);
}

static void
choose_region_permission_cb (GObject *source, GAsyncResult *res, gpointer user_data)
{
        CcRegionPanel *self = user_data;
        if (permission_acquired (G_PERMISSION (source), res, "choose region"))
                show_region_chooser (self);
}

static void
activate_language_row (CcRegionPanel *self,
                       GtkListBoxRow *row)
{
        if (row == self->language_row) {
                if (!self->login || g_permission_get_allowed (self->permission)) {
                        show_language_chooser (self);
                } else if (g_permission_get_can_acquire (self->permission)) {
                        g_permission_acquire_async (self->permission,
                                                    cc_panel_get_cancellable (CC_PANEL (self)),
                                                    choose_language_permission_cb,
                                                    self);
                }
        } else if (row == self->formats_row) {
                if (!self->login || g_permission_get_allowed (self->permission)) {
                        show_region_chooser (self);
                } else if (g_permission_get_can_acquire (self->permission)) {
                        g_permission_acquire_async (self->permission,
                                                    cc_panel_get_cancellable (CC_PANEL (self)),
                                                    choose_region_permission_cb,
                                                    self);
                }
        }
}

static void
update_region_label (CcRegionPanel *self)
{
        const gchar *region = get_effective_region (self);
        g_autofree gchar *name = NULL;

        if (region)
                name = gnome_get_country_from_locale (region, region);

        if (!name)
                name = gnome_get_country_from_locale (DEFAULT_LOCALE, DEFAULT_LOCALE);

        gtk_label_set_label (self->formats_label, name);
}

static void
update_region_from_setting (CcRegionPanel *self)
{
        g_free (self->region);
        self->region = g_settings_get_string (self->locale_settings, KEY_REGION);
        update_region_label (self);
}

static void
update_language_label (CcRegionPanel *self)
{
        const gchar *language = get_effective_language (self);
        g_autofree gchar *name = NULL;

        if (language)
                name = gnome_get_language_from_locale (language, language);

        if (!name)
                name = gnome_get_language_from_locale (DEFAULT_LOCALE, DEFAULT_LOCALE);

        gtk_label_set_label (self->language_label, name);

        /* Formats will change too if not explicitly set. */
        update_region_label (self);
}

static void
update_language_from_user (CcRegionPanel *self)
{
        const gchar *language = NULL;

        if (act_user_is_loaded (self->user))
                language = act_user_get_language (self->user);

        if (language == NULL || *language == '\0')
                language = setlocale (LC_MESSAGES, NULL);

        g_free (self->language);
        self->language = g_strdup (language);
        update_language_label (self);
}

static void
setup_language_section (CcRegionPanel *self)
{
        self->user = act_user_manager_get_user_by_id (self->user_manager, getuid ());
        g_signal_connect_object (self->user, "notify::language",
                                 G_CALLBACK (update_language_from_user), self, G_CONNECT_SWAPPED);
        g_signal_connect_object (self->user, "notify::is-loaded",
                                 G_CALLBACK (update_language_from_user), self, G_CONNECT_SWAPPED);

        self->locale_settings = g_settings_new (GNOME_SYSTEM_LOCALE_DIR);
        g_signal_connect_object (self->locale_settings, "changed::" KEY_REGION,
                                 G_CALLBACK (update_region_from_setting), self, G_CONNECT_SWAPPED);

        gtk_list_box_set_header_func (self->language_list,
                                      cc_list_box_update_header_func,
                                      NULL, NULL);
        g_signal_connect_object (self->language_list, "row-activated",
                                 G_CALLBACK (activate_language_row), self, G_CONNECT_SWAPPED);

        gtk_list_box_set_header_func (self->formats_list,
                                      cc_list_box_update_header_func,
                                      NULL, NULL);
        g_signal_connect_object (self->formats_list, "row-activated",
                                 G_CALLBACK (activate_language_row), self, G_CONNECT_SWAPPED);

        update_language_from_user (self);
        update_region_from_setting (self);
}

static void
on_localed_properties_changed (CcRegionPanel  *self,
                               GVariant       *changed_properties,
                               const gchar   **invalidated_properties)
{
        g_autoptr(GVariant) v = NULL;

        v = g_dbus_proxy_get_cached_property (G_DBUS_PROXY (self->localed), "Locale");
        if (v) {
                g_autofree const gchar **strv = NULL;
                gsize len;
                gint i;
                const gchar *lang, *messages, *time;

                strv = g_variant_get_strv (v, &len);

                lang = messages = time = NULL;
                for (i = 0; strv[i]; i++) {
                        if (g_str_has_prefix (strv[i], "LANG=")) {
                                lang = strv[i] + strlen ("LANG=");
                        } else if (g_str_has_prefix (strv[i], "LC_MESSAGES=")) {
                                messages = strv[i] + strlen ("LC_MESSAGES=");
                        } else if (g_str_has_prefix (strv[i], "LC_TIME=")) {
                                time = strv[i] + strlen ("LC_TIME=");
                        }
                }
                if (!lang) {
                        lang = setlocale (LC_MESSAGES, NULL);
                }
                if (!messages) {
                        messages = lang;
                }
                g_free (self->system_language);
                self->system_language = g_strdup (messages);
                g_free (self->system_region);
                self->system_region = g_strdup (time);

                update_language_label (self);
        }
}

static void
set_localed_locale (CcRegionPanel *self)
{
        g_autoptr(GVariantBuilder) b = NULL;
        g_autofree gchar *lang_value = NULL;

        b = g_variant_builder_new (G_VARIANT_TYPE ("as"));
        lang_value = g_strconcat ("LANG=", self->system_language, NULL);
        g_variant_builder_add (b, "s", lang_value);

        if (self->system_region != NULL) {
                g_autofree gchar *time_value = NULL;
                g_autofree gchar *numeric_value = NULL;
                g_autofree gchar *monetary_value = NULL;
                g_autofree gchar *measurement_value = NULL;
                g_autofree gchar *paper_value = NULL;
                time_value = g_strconcat ("LC_TIME=", self->system_region, NULL);
                g_variant_builder_add (b, "s", time_value);
                numeric_value = g_strconcat ("LC_NUMERIC=", self->system_region, NULL);
                g_variant_builder_add (b, "s", numeric_value);
                monetary_value = g_strconcat ("LC_MONETARY=", self->system_region, NULL);
                g_variant_builder_add (b, "s", monetary_value);
                measurement_value = g_strconcat ("LC_MEASUREMENT=", self->system_region, NULL);
                g_variant_builder_add (b, "s", measurement_value);
                paper_value = g_strconcat ("LC_PAPER=", self->system_region, NULL);
                g_variant_builder_add (b, "s", paper_value);
        }
        g_dbus_proxy_call (self->localed,
                           "SetLocale",
                           g_variant_new ("(asb)", b, TRUE),
                           G_DBUS_CALL_FLAGS_NONE,
                           -1, NULL, NULL, NULL);
}

static void
localed_proxy_ready (GObject      *source,
                     GAsyncResult *res,
                     gpointer      data)
{
        CcRegionPanel *self = data;
        GDBusProxy *proxy;
        g_autoptr(GError) error = NULL;

        proxy = g_dbus_proxy_new_finish (res, &error);

        if (!proxy) {
                if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
                        g_warning ("Failed to contact localed: %s\n", error->message);
                return;
        }

        self->localed = proxy;

        g_signal_connect_object (self->localed, "g-properties-changed",
                                 G_CALLBACK (on_localed_properties_changed), self, G_CONNECT_SWAPPED);
        on_localed_properties_changed (self, NULL, NULL);
}

static void
login_changed (CcRegionPanel *self)
{
        gboolean can_acquire;

        self->login = gtk_toggle_button_get_active (self->login_language_button);
        gtk_widget_set_visible (GTK_WIDGET (self->login_label), self->login);

        can_acquire = self->permission &&
                (g_permission_get_allowed (self->permission) ||
                 g_permission_get_can_acquire (self->permission));
        /* FIXME: insensitive doesn't look quite right for this */
        gtk_widget_set_sensitive (GTK_WIDGET (self->language_section_frame), !self->login || can_acquire);

        update_language_label (self);
}

static void
set_login_button_visibility (CcRegionPanel *self)
{
        gboolean has_multiple_users;
        gboolean loaded;

        g_object_get (self->user_manager, "is-loaded", &loaded, NULL);
        if (!loaded)
          return;

        g_object_get (self->user_manager, "has-multiple-users", &has_multiple_users, NULL);

        self->login_auto_apply = !has_multiple_users && g_permission_get_allowed (self->permission);
        gtk_widget_set_visible (GTK_WIDGET (self->session_or_login_box), !self->login_auto_apply);

        g_signal_handlers_disconnect_by_func (self->user_manager, set_login_button_visibility, self);
}

static void
setup_login_button (CcRegionPanel *self)
{
        g_autoptr(GDBusConnection) bus = NULL;
        gboolean loaded;
        g_autoptr(GError) error = NULL;

        self->permission = polkit_permission_new_sync ("org.freedesktop.locale1.set-locale", NULL, NULL, &error);
        if (self->permission == NULL) {
                g_warning ("Could not get 'org.freedesktop.locale1.set-locale' permission: %s",
                           error->message);
                return;
        }

        bus = g_bus_get_sync (G_BUS_TYPE_SYSTEM, NULL, NULL);
        g_dbus_proxy_new (bus,
                          G_DBUS_PROXY_FLAGS_GET_INVALIDATED_PROPERTIES,
                          NULL,
                          "org.freedesktop.locale1",
                          "/org/freedesktop/locale1",
                          "org.freedesktop.locale1",
                          cc_panel_get_cancellable (CC_PANEL (self)),
                          (GAsyncReadyCallback) localed_proxy_ready,
                          self);

        g_signal_connect_object (self->login_language_button, "notify::active",
                                 G_CALLBACK (login_changed), self, G_CONNECT_SWAPPED);

        g_object_get (self->user_manager, "is-loaded", &loaded, NULL);
        if (loaded)
                set_login_button_visibility (self);
        else
                g_signal_connect_object (self->user_manager, "notify::is-loaded",
                                         G_CALLBACK (set_login_button_visibility), self, G_CONNECT_SWAPPED);
}

static void
session_proxy_ready (GObject      *source,
                     GAsyncResult *res,
                     gpointer      data)
{
        CcRegionPanel *self = data;
        GDBusProxy *proxy;
        g_autoptr(GError) error = NULL;

        proxy = g_dbus_proxy_new_for_bus_finish (res, &error);

        if (!proxy) {
                if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
                        g_warning ("Failed to contact gnome-session: %s\n", error->message);
                return;
        }

        self->session = proxy;
}

static void
cc_region_panel_class_init (CcRegionPanelClass * klass)
{
        GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);
        GObjectClass *object_class = G_OBJECT_CLASS (klass);
        CcPanelClass *panel_class = CC_PANEL_CLASS (klass);

        panel_class->get_help_uri = cc_region_panel_get_help_uri;

        object_class->finalize = cc_region_panel_finalize;

        gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/control-center/region/cc-region-panel.ui");

        gtk_widget_class_bind_template_child (widget_class, CcRegionPanel, formats_label);
        gtk_widget_class_bind_template_child (widget_class, CcRegionPanel, formats_list);
        gtk_widget_class_bind_template_child (widget_class, CcRegionPanel, formats_row);
        gtk_widget_class_bind_template_child (widget_class, CcRegionPanel, login_label);
        gtk_widget_class_bind_template_child (widget_class, CcRegionPanel, language_label);
        gtk_widget_class_bind_template_child (widget_class, CcRegionPanel, language_list);
        gtk_widget_class_bind_template_child (widget_class, CcRegionPanel, language_row);
        gtk_widget_class_bind_template_child (widget_class, CcRegionPanel, language_section_frame);
        gtk_widget_class_bind_template_child (widget_class, CcRegionPanel, login_language_button);
        gtk_widget_class_bind_template_child (widget_class, CcRegionPanel, restart_button);
        gtk_widget_class_bind_template_child (widget_class, CcRegionPanel, restart_revealer);
        gtk_widget_class_bind_template_child (widget_class, CcRegionPanel, session_or_login_box);

        gtk_widget_class_bind_template_callback (widget_class, restart_now);
}

static void
cc_region_panel_init (CcRegionPanel *self)
{
        g_autoptr(GFile) needs_restart_file = NULL;

        g_resources_register (cc_region_get_resource ());

        gtk_widget_init_template (GTK_WIDGET (self));

        self->user_manager = act_user_manager_get_default ();

        g_dbus_proxy_new_for_bus (G_BUS_TYPE_SESSION,
                                  G_DBUS_PROXY_FLAGS_NONE,
                                  NULL,
                                  "org.gnome.SessionManager",
                                  "/org/gnome/SessionManager",
                                  "org.gnome.SessionManager",
                                  cc_panel_get_cancellable (CC_PANEL (self)),
                                  session_proxy_ready,
                                  self);

        setup_login_button (self);
        setup_language_section (self);

        needs_restart_file = get_needs_restart_file ();
        if (g_file_query_exists (needs_restart_file, NULL))
                set_restart_notification_visible (self, NULL, TRUE);
}
