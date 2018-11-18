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
#include "cc-input-chooser.h"
#include "cc-input-row.h"
#include "cc-input-source-ibus.h"
#include "cc-input-source-xkb.h"

#include "cc-common-language.h"

#define GNOME_DESKTOP_USE_UNSTABLE_API
#include <libgnome-desktop/gnome-languages.h>
#include <libgnome-desktop/gnome-xkb-info.h>

#ifdef HAVE_IBUS
#include <ibus.h>
#endif

#include <act/act.h>

#define GNOME_DESKTOP_INPUT_SOURCES_DIR "org.gnome.desktop.input-sources"
#define KEY_INPUT_SOURCES        "sources"

#define GNOME_SYSTEM_LOCALE_DIR "org.gnome.system.locale"
#define KEY_REGION "region"

#define DEFAULT_LOCALE "en_US.utf-8"

struct _CcRegionPanel {
        CcPanel          parent_instance;

        GtkButton       *add_input_button;
        GtkLabel        *alt_next_source;
        GtkLabel        *formats_label;
        GtkListBoxRow   *formats_row;
        GtkListBox      *input_list;
        GtkBox          *input_section_box;
        GtkToggleButton *login_button;
        GtkLabel        *login_label;
        GtkLabel        *language_label;
        GtkListBox      *language_list;
        GtkListBoxRow   *language_row;
        GtkFrame        *language_section_frame;
        GtkButton       *move_down_input_button;
        GtkButton       *move_up_input_button;
        GtkLabel        *next_source;
        GtkLabel        *next_source_label;
        GtkListBoxRow   *no_inputs_row;
        GtkButton       *options_button;
        GtkRadioButton  *per_window_source;
        GtkLabel        *previous_source;
        GtkLabel        *previous_source_label;
        GtkButton       *restart_button;
        GtkRevealer     *restart_revealer;
        GtkRadioButton  *same_source;
        GtkButton       *show_config_button;

        gboolean     login;
        gboolean     login_auto_apply;
        GPermission *permission;
        GDBusProxy  *localed;
        GDBusProxy  *session;
        GCancellable *cancellable;

        ActUserManager *user_manager;
        ActUser        *user;
        GSettings      *locale_settings;

        gchar *language;
        gchar *region;
        gchar *system_language;
        gchar *system_region;

        GSettings *input_settings;
        GnomeXkbInfo *xkb_info;
#ifdef HAVE_IBUS
        IBusBus *ibus;
        GHashTable *ibus_engines;
#endif
};

CC_PANEL_REGISTER (CcRegionPanel, cc_region_panel)

typedef struct
{
        CcRegionPanel *panel;
        CcInputRow    *row;
        gint           offset;
} RowData;

static RowData *
row_data_new (CcRegionPanel *panel, CcInputRow *row, gint offset)
{
        RowData *data = g_malloc0 (sizeof (RowData));
        data->panel = panel;
        data->row = g_object_ref (row);
        data->offset = offset;
        return data;
}

static void
row_data_free (RowData *data)
{
        g_object_unref (data->row);
        g_free (data);
}

G_DEFINE_AUTOPTR_CLEANUP_FUNC (RowData, row_data_free)

static void
cc_region_panel_finalize (GObject *object)
{
        CcRegionPanel *self = CC_REGION_PANEL (object);
        GtkWidget *chooser;

        g_cancellable_cancel (self->cancellable);
        g_clear_object (&self->cancellable);

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
        g_clear_object (&self->input_settings);
        g_clear_object (&self->xkb_info);
#ifdef HAVE_IBUS
        g_clear_object (&self->ibus);
        g_clear_pointer (&self->ibus_engines, g_hash_table_destroy);
#endif
        g_free (self->language);
        g_free (self->region);
        g_free (self->system_language);
        g_free (self->system_region);

        chooser = g_object_get_data (G_OBJECT (self), "input-chooser");
        if (chooser)
                gtk_widget_destroy (chooser);

        G_OBJECT_CLASS (cc_region_panel_parent_class)->finalize (object);
}

static void
cc_region_panel_constructed (GObject *object)
{
        CcRegionPanel *self = CC_REGION_PANEL (object);

        G_OBJECT_CLASS (cc_region_panel_parent_class)->constructed (object);

        if (self->permission)
                cc_shell_embed_widget_in_header (cc_panel_get_shell (CC_PANEL (object)),
                                                 GTK_WIDGET (self->login_button));
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
        g_autofree gchar *current_locale = NULL;
        g_autoptr(GFile) file = NULL;
        g_autoptr(GFileOutputStream) output_stream = NULL;
        g_autoptr(GError) error = NULL;

        if (locale) {
                current_locale = g_strdup (setlocale (LC_MESSAGES, NULL));
                setlocale (LC_MESSAGES, locale);
        }

        gtk_revealer_set_reveal_child (self->restart_revealer, visible);

        if (locale)
                setlocale (LC_MESSAGES, current_locale);

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
                           self->cancellable,
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

static void show_input_chooser (CcRegionPanel *self);

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
                                                    self->cancellable,
                                                    choose_language_permission_cb,
                                                    self);
                }
        } else if (row == self->formats_row) {
                if (!self->login || g_permission_get_allowed (self->permission)) {
                        show_region_chooser (self);
                } else if (g_permission_get_can_acquire (self->permission)) {
                        g_permission_acquire_async (self->permission,
                                                    self->cancellable,
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

        gtk_list_box_set_selection_mode (self->language_list,
                                         GTK_SELECTION_NONE);
        gtk_list_box_set_header_func (self->language_list,
                                      cc_list_box_update_header_func,
                                      NULL, NULL);
        g_signal_connect_object (self->language_list, "row-activated",
                                 G_CALLBACK (activate_language_row), self, G_CONNECT_SWAPPED);

        update_language_from_user (self);
        update_region_from_setting (self);
}

#ifdef HAVE_IBUS
static void
update_ibus_active_sources (CcRegionPanel *self)
{
        g_autoptr(GList) rows = NULL;
        GList *l;

        rows = gtk_container_get_children (GTK_CONTAINER (self->input_list));
        for (l = rows; l; l = l->next) {
                CcInputRow *row;
                CcInputSourceIBus *source;
                IBusEngineDesc *engine_desc;

                if (!CC_IS_INPUT_ROW (l->data))
                        continue;
                row = CC_INPUT_ROW (l->data);

                if (!CC_IS_INPUT_SOURCE_IBUS (cc_input_row_get_source (row)))
                        continue;
                source = CC_INPUT_SOURCE_IBUS (cc_input_row_get_source (row));

                engine_desc = g_hash_table_lookup (self->ibus_engines, cc_input_source_ibus_get_engine_name (source));
                if (engine_desc != NULL)
                        cc_input_source_ibus_set_engine_desc (source, engine_desc);
        }
}

static void
fetch_ibus_engines_result (GObject       *object,
                           GAsyncResult  *result,
                           CcRegionPanel *self)
{
        g_autoptr(GList) list = NULL;
        GList *l;
        g_autoptr(GError) error = NULL;

        list = ibus_bus_list_engines_async_finish (IBUS_BUS (object), result, &error);
        if (!list && error) {
                if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
                        g_warning ("Couldn't finish IBus request: %s", error->message);
                return;
        }

        /* Maps engine ids to engine description objects */
        self->ibus_engines = g_hash_table_new_full (g_str_hash, g_str_equal, NULL, g_object_unref);

        for (l = list; l; l = l->next) {
                IBusEngineDesc *engine = l->data;
                const gchar *engine_id = ibus_engine_desc_get_name (engine);

                if (g_str_has_prefix (engine_id, "xkb:"))
                        g_object_unref (engine);
                else
                        g_hash_table_replace (self->ibus_engines, (gpointer)engine_id, engine);
        }

        update_ibus_active_sources (self);
}

static void
fetch_ibus_engines (CcRegionPanel *self)
{
        ibus_bus_list_engines_async (self->ibus,
                                     -1,
                                     self->cancellable,
                                     (GAsyncReadyCallback)fetch_ibus_engines_result,
                                     self);

  /* We've got everything we needed, don't want to be called again. */
  g_signal_handlers_disconnect_by_func (self->ibus, fetch_ibus_engines, self);
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

#endif

static void
row_layout_cb (CcRegionPanel *self,
               CcInputRow    *row)
{
        CcInputSource *source;
        const gchar *layout, *layout_variant;
        g_autofree gchar *commandline = NULL;

        source = cc_input_row_get_source (row);

        layout = cc_input_source_get_layout (source);
        layout_variant = cc_input_source_get_layout_variant (source);

        if (layout_variant && layout_variant[0])
                commandline = g_strdup_printf ("gkbd-keyboard-display -l \"%s\t%s\"",
                                               layout, layout_variant);
        else
                commandline = g_strdup_printf ("gkbd-keyboard-display -l %s",
                                               layout);

        g_spawn_command_line_async (commandline, NULL);
}

static void remove_input (CcRegionPanel *self, CcInputRow *row);

static void
row_removed_cb (CcRegionPanel *self,
                CcInputRow    *row)
{
        remove_input (self, row);
}

static void
update_input_rows (CcRegionPanel *self)
{
        g_autoptr(GList) rows = NULL;
        GList *l;
        guint n_input_rows = 0;

        rows = gtk_container_get_children (GTK_CONTAINER (self->input_list));
        for (l = rows; l; l = l->next)
                if (CC_IS_INPUT_ROW (l->data))
                       n_input_rows++;
        for (l = rows; l; l = l->next) {
                CcInputRow *row;

                if (!CC_IS_INPUT_ROW (l->data))
                        continue;
                row = CC_INPUT_ROW (l->data);

                cc_input_row_set_removable (row, n_input_rows > 1);
        }
}

static void
add_input_row (CcRegionPanel *self, CcInputSource *source)
{
        CcInputRow *row;

        gtk_widget_set_visible (GTK_WIDGET (self->no_inputs_row), FALSE);

        row = cc_input_row_new (source);
        gtk_widget_show (GTK_WIDGET (row));
        g_signal_connect_object (row, "show-layout", G_CALLBACK (row_layout_cb), self, G_CONNECT_SWAPPED);
        g_signal_connect_object (row, "remove-row", G_CALLBACK (row_removed_cb), self, G_CONNECT_SWAPPED);
        gtk_container_add (GTK_CONTAINER (self->input_list), GTK_WIDGET (row));
        update_input_rows (self);

        cc_list_box_adjust_scrolling (self->input_list);
}

static void
add_input_sources (CcRegionPanel *self,
                   GVariant      *sources)
{
        GVariantIter iter;
        const gchar *type, *id;

        if (g_variant_n_children (sources) < 1) {
                gtk_widget_set_visible (GTK_WIDGET (self->no_inputs_row), TRUE);
                return;
        }

        g_variant_iter_init (&iter, sources);
        while (g_variant_iter_next (&iter, "(&s&s)", &type, &id)) {
                g_autoptr(CcInputSource) source = NULL;

                if (g_str_equal (type, "xkb")) {
                        source = CC_INPUT_SOURCE (cc_input_source_xkb_new_from_id (self->xkb_info, id));
                } else if (g_str_equal (type, "ibus")) {
                        source = CC_INPUT_SOURCE (cc_input_source_ibus_new (id));
#ifdef HAVE_IBUS
                        if (self->ibus_engines) {
                                IBusEngineDesc *engine_desc = g_hash_table_lookup (self->ibus_engines, id);
                                if (engine_desc != NULL)
                                        cc_input_source_ibus_set_engine_desc (CC_INPUT_SOURCE_IBUS (source), engine_desc);
                        }
#endif
                } else {
                        g_warning ("Unhandled input source type '%s'", type);
                        continue;
                }

                add_input_row (self, source);
        }
}

static void
add_input_sources_from_settings (CcRegionPanel *self)
{
        g_autoptr(GVariant) sources = NULL;
        sources = g_settings_get_value (self->input_settings, "sources");
        add_input_sources (self, sources);
}

static void
clear_input_sources (CcRegionPanel *self)
{
        g_autoptr(GList) list = NULL;
        GList *l;

        list = gtk_container_get_children (GTK_CONTAINER (self->input_list));
        for (l = list; l; l = l->next) {
                if (CC_IS_INPUT_ROW (l->data))
                        gtk_container_remove (GTK_CONTAINER (self->input_list), GTK_WIDGET (l->data));
        }

        cc_list_box_adjust_scrolling (self->input_list);
}

static CcInputRow *
get_row_by_source (CcRegionPanel *self, CcInputSource *source)
{
        g_autoptr(GList) list = NULL;
        GList *l;

        list = gtk_container_get_children (GTK_CONTAINER (self->input_list));
        for (l = list; l; l = l->next) {
                CcInputRow *row;

                if (!CC_IS_INPUT_ROW (l->data))
                        continue;
                row = CC_INPUT_ROW (l->data);

                if (cc_input_source_matches (source, cc_input_row_get_source (row)))
                        return row;
        }

        return NULL;
}

static void
input_sources_changed (CcRegionPanel *self,
                       const gchar   *key)
{
        CcInputRow *selected;
        g_autoptr(CcInputSource) source = NULL;

        selected = CC_INPUT_ROW (gtk_list_box_get_selected_row (self->input_list));
        if (selected)
                source = g_object_ref (cc_input_row_get_source (selected));
        clear_input_sources (self);
        add_input_sources_from_settings (self);
        if (source != NULL) {
                CcInputRow *row = get_row_by_source (self, source);
                if (row != NULL)
                        gtk_list_box_select_row (GTK_LIST_BOX (self->input_list), GTK_LIST_BOX_ROW (row));
        }
}

static void
update_buttons (CcRegionPanel *self)
{
        CcInputRow *selected;
        g_autoptr(GList) children = NULL;
        guint n_rows;

        children = gtk_container_get_children (GTK_CONTAINER (self->input_list));
        n_rows = g_list_length (children);

        selected = CC_INPUT_ROW (gtk_list_box_get_selected_row (self->input_list));
        if (selected == NULL) {
                gtk_widget_set_visible (GTK_WIDGET (self->show_config_button), FALSE);
                gtk_widget_set_sensitive (GTK_WIDGET (self->move_up_input_button), FALSE);
                gtk_widget_set_sensitive (GTK_WIDGET (self->move_down_input_button), FALSE);
        } else {
                gint index;

                index = gtk_list_box_row_get_index (GTK_LIST_BOX_ROW (selected));

                gtk_widget_set_visible (GTK_WIDGET (self->show_config_button), CC_IS_INPUT_SOURCE_IBUS (cc_input_row_get_source (selected)));
                gtk_widget_set_sensitive (GTK_WIDGET (self->move_up_input_button), index > 1);
                gtk_widget_set_sensitive (GTK_WIDGET (self->move_down_input_button), index < n_rows - 1);
        }

        gtk_widget_set_visible (GTK_WIDGET (self->options_button),
                                n_rows > 1 && !self->login);
}

static void
set_input_settings (CcRegionPanel *self)
{
        GVariantBuilder builder;
        g_autoptr(GList) list = NULL;
        GList *l;

        g_variant_builder_init (&builder, G_VARIANT_TYPE ("a(ss)"));
        list = gtk_container_get_children (GTK_CONTAINER (self->input_list));
        for (l = list; l; l = l->next) {
                CcInputRow *row;
                CcInputSource *source;

                if (!CC_IS_INPUT_ROW (l->data))
                        continue;
                row = CC_INPUT_ROW (l->data);
                source = cc_input_row_get_source (row);

                if (CC_IS_INPUT_SOURCE_XKB (source)) {
                        g_autofree gchar *id = cc_input_source_xkb_get_id (CC_INPUT_SOURCE_XKB (source));
                        g_variant_builder_add (&builder, "(ss)", "xkb", id);
                } else if (CC_IS_INPUT_SOURCE_IBUS (source)) {
                        g_variant_builder_add (&builder, "(ss)", "ibus",
                                               cc_input_source_ibus_get_engine_name (CC_INPUT_SOURCE_IBUS (source)));
                }
        }

        g_settings_set_value (self->input_settings, KEY_INPUT_SOURCES, g_variant_builder_end (&builder));
        g_settings_apply (self->input_settings);
}

static void set_localed_input (CcRegionPanel *self);

static void
update_input (CcRegionPanel *self)
{
        if (self->login) {
                set_localed_input (self);
        } else {
                set_input_settings (self);
                if (self->login_auto_apply)
                        set_localed_input (self);
        }
}

static void
show_input_chooser (CcRegionPanel *self)
{
        CcInputChooser *chooser;

        chooser = cc_input_chooser_new (self->login,
                                        self->xkb_info,
#ifdef HAVE_IBUS
                                        self->ibus_engines
#else
                                        NULL
#endif
                                        );
        gtk_window_set_transient_for (GTK_WINDOW (chooser), GTK_WINDOW (gtk_widget_get_toplevel (GTK_WIDGET (self))));

        if (gtk_dialog_run (GTK_DIALOG (chooser)) == GTK_RESPONSE_OK) {
                CcInputSource *source;

                source = cc_input_chooser_get_source (chooser);
                if (source != NULL && get_row_by_source (self, source) == NULL) {
                        add_input_row (self, source);
                        update_buttons (self);
                        update_input (self);
                }
        }
        gtk_widget_destroy (GTK_WIDGET (chooser));
}

static void
add_input_permission_cb (GObject *source, GAsyncResult *res, gpointer user_data)
{
        CcRegionPanel *self = user_data;
        if (permission_acquired (G_PERMISSION (source), res, "add input"))
                show_input_chooser (self);
}

static void
add_input (CcRegionPanel *self)
{
        if (!self->login) {
                show_input_chooser (self);
        } else if (g_permission_get_allowed (self->permission)) {
                show_input_chooser (self);
        } else if (g_permission_get_can_acquire (self->permission)) {
                g_permission_acquire_async (self->permission,
                                            self->cancellable,
                                            add_input_permission_cb,
                                            self);
        }
}

static GtkWidget *
find_sibling (GtkContainer *container, GtkWidget *child)
{
        g_autoptr(GList) list = NULL;
        GList *c, *l;
        GtkWidget *sibling;

        list = gtk_container_get_children (container);
        c = g_list_find (list, child);

        for (l = c->next; l; l = l->next) {
                sibling = l->data;
                if (gtk_widget_get_visible (sibling) && gtk_widget_get_child_visible (sibling))
                        return sibling;
        }

        for (l = c->prev; l; l = l->prev) {
                sibling = l->data;
                if (gtk_widget_get_visible (sibling) && gtk_widget_get_child_visible (sibling))
                        return sibling;
        }

        return NULL;
}

static void
do_remove_input (CcRegionPanel *self, CcInputRow *row)
{
        GtkWidget *sibling;

        sibling = find_sibling (GTK_CONTAINER (self->input_list), GTK_WIDGET (row));
        gtk_container_remove (GTK_CONTAINER (self->input_list), GTK_WIDGET (row));
        gtk_list_box_select_row (self->input_list, GTK_LIST_BOX_ROW (sibling));

        cc_list_box_adjust_scrolling (self->input_list);

        update_buttons (self);
        update_input (self);
        update_input_rows (self);
}

static void
remove_input_permission_cb (GObject *source, GAsyncResult *res, gpointer user_data)
{
        RowData *data = user_data;
        if (permission_acquired (G_PERMISSION (source), res, "remove input"))
                do_remove_input (data->panel, data->row);
}

static void
remove_input (CcRegionPanel *self, CcInputRow *row)
{
        if (!self->login) {
                do_remove_input (self, row);
        } else if (g_permission_get_allowed (self->permission)) {
                do_remove_input (self, row);
        } else if (g_permission_get_can_acquire (self->permission)) {
                g_permission_acquire_async (self->permission,
                                            self->cancellable,
                                            remove_input_permission_cb,
                                            row_data_new (self, row, -1));
        }
}

static void
do_move_input (CcRegionPanel *self,
               CcInputRow    *row,
               gint           offset)
{
        gint idx;

        idx = gtk_list_box_row_get_index (GTK_LIST_BOX_ROW (row)) + offset;

        gtk_list_box_unselect_row (self->input_list, GTK_LIST_BOX_ROW (row));

        g_object_ref (row);
        gtk_container_remove (GTK_CONTAINER (self->input_list), GTK_WIDGET (row));
        gtk_list_box_insert (self->input_list, GTK_WIDGET (row), idx);
        g_object_unref (row);

        gtk_list_box_select_row (self->input_list, GTK_LIST_BOX_ROW (row));

        cc_list_box_adjust_scrolling (self->input_list);

        update_buttons (self);
        update_input (self);
}

static void
move_input_permission_cb (GObject *source, GAsyncResult *res, gpointer user_data)
{
        RowData *data = user_data;
        if (permission_acquired (G_PERMISSION (source), res, "move input"))
                do_move_input (data->panel, data->row, data->offset);
}

static void
move_input (CcRegionPanel *self,
            CcInputRow    *row,
            gint           offset)
{
        if (!self->login) {
                do_move_input (self, row, offset);
        } else if (g_permission_get_allowed (self->permission)) {
                do_move_input (self, row, offset);
        } else if (g_permission_get_can_acquire (self->permission)) {
                g_permission_acquire_async (self->permission,
                                            self->cancellable,
                                            move_input_permission_cb,
                                            row_data_new (self, row, offset));
        }
}

static void
move_selected_input_up (CcRegionPanel *self)
{
        GtkListBoxRow *selected;

        selected = gtk_list_box_get_selected_row (GTK_LIST_BOX (self->input_list));
        if (selected == NULL)
                return;

        move_input (self, CC_INPUT_ROW (selected), -1);
}

static void
move_selected_input_down (CcRegionPanel *self)
{
        GtkListBoxRow *selected;

        selected = gtk_list_box_get_selected_row (GTK_LIST_BOX (self->input_list));
        if (selected == NULL)
                return;

        move_input (self, CC_INPUT_ROW (selected), 1);
}

static void
show_selected_settings (CcRegionPanel *self)
{
        CcInputRow *selected;
        CcInputSourceIBus *source;
        g_autoptr(GdkAppLaunchContext) ctx = NULL;
        g_autoptr(GDesktopAppInfo) app_info = NULL;
        g_autoptr(GError) error = NULL;

        selected = CC_INPUT_ROW (gtk_list_box_get_selected_row (self->input_list));
        if (selected == NULL)
                return;
        g_return_if_fail (CC_IS_INPUT_SOURCE_IBUS (cc_input_row_get_source (selected)));
        source = CC_INPUT_SOURCE_IBUS (cc_input_row_get_source (selected));

        app_info = cc_input_source_ibus_get_app_info (source);
        if (app_info == NULL)
                return;

        ctx = gdk_display_get_app_launch_context (gdk_display_get_default ());
        gdk_app_launch_context_set_timestamp (ctx, gtk_get_current_event_time ());

        g_app_launch_context_setenv (G_APP_LAUNCH_CONTEXT (ctx),
                                     "IBUS_ENGINE_NAME", cc_input_source_ibus_get_engine_name (source));

        if (!g_app_info_launch (G_APP_INFO (app_info), NULL, G_APP_LAUNCH_CONTEXT (ctx), &error))
                g_warning ("Failed to launch input source setup: %s", error->message);
}

static void
update_shortcut_label (GtkLabel    *label,
                       const gchar *value)
{
        g_autofree gchar *text = NULL;
        guint accel_key;
        g_autofree guint *keycode = NULL;
        GdkModifierType mods;

        if (value == NULL || *value == '\0') {
                gtk_widget_hide (GTK_WIDGET (label));
                return;
        }

        gtk_accelerator_parse_with_keycode (value, &accel_key, &keycode, &mods);
        if (accel_key == 0 && keycode == NULL && mods == 0) {
                g_warning ("Failed to parse keyboard shortcut: '%s'", value);
                gtk_widget_hide (GTK_WIDGET (label));
                return;
        }

        text = gtk_accelerator_get_label_with_keycode (gtk_widget_get_display (GTK_WIDGET (label)), accel_key, *keycode, mods);
        gtk_label_set_text (label, text);
}

static void
update_shortcuts (CcRegionPanel *self)
{
        g_auto(GStrv) previous = NULL;
        g_auto(GStrv) next = NULL;
        g_autofree gchar *previous_shortcut = NULL;
        g_autoptr(GSettings) settings = NULL;

        settings = g_settings_new ("org.gnome.desktop.wm.keybindings");

        previous = g_settings_get_strv (settings, "switch-input-source-backward");
        next = g_settings_get_strv (settings, "switch-input-source");

        previous_shortcut = g_strdup (previous[0]);

        update_shortcut_label (self->previous_source, previous_shortcut);
        update_shortcut_label (self->next_source, next[0]);
}

static void
update_modifiers_shortcut (CcRegionPanel *self)
{
        g_auto(GStrv) options = NULL;
        gchar **p;
        g_autoptr(GSettings) settings = NULL;
        g_autoptr(GnomeXkbInfo) xkb_info = NULL;
        const gchar *text;

        xkb_info = gnome_xkb_info_new ();
        settings = g_settings_new ("org.gnome.desktop.input-sources");
        options = g_settings_get_strv (settings, "xkb-options");

        for (p = options; p && *p; ++p)
                if (g_str_has_prefix (*p, "grp:"))
                        break;

        if (p && *p) {
                text = gnome_xkb_info_description_for_option (xkb_info, "grp", *p);
                gtk_label_set_text (self->alt_next_source, text);
        } else {
                gtk_widget_hide (GTK_WIDGET (self->alt_next_source));
        }
}

static void
setup_input_section (CcRegionPanel *self)
{
        self->input_settings = g_settings_new (GNOME_DESKTOP_INPUT_SOURCES_DIR);
        g_settings_delay (self->input_settings);

        self->xkb_info = gnome_xkb_info_new ();

#ifdef HAVE_IBUS
        ibus_init ();
        if (!self->ibus) {
                self->ibus = ibus_bus_new_async ();
                if (ibus_bus_is_connected (self->ibus))
                        fetch_ibus_engines (self);
                else
                        g_signal_connect_object (self->ibus, "connected",
                                                 G_CALLBACK (fetch_ibus_engines), self,
                                                 G_CONNECT_SWAPPED);
        }
        maybe_start_ibus ();
#endif

        cc_list_box_setup_scrolling (self->input_list, 5);

        gtk_list_box_set_selection_mode (self->input_list,
                                         GTK_SELECTION_SINGLE);
        gtk_list_box_set_header_func (self->input_list,
                                      cc_list_box_update_header_func,
                                      NULL, NULL);
        g_signal_connect_object (self->input_list, "row-selected",
                                 G_CALLBACK (update_buttons), self, G_CONNECT_SWAPPED);

        g_signal_connect_object (self->input_settings, "changed::" KEY_INPUT_SOURCES,
                                 G_CALLBACK (input_sources_changed), self, G_CONNECT_SWAPPED);

        add_input_sources_from_settings (self);
        update_buttons (self);

        g_object_bind_property (self->previous_source, "visible",
                                self->previous_source_label, "visible",
                                G_BINDING_DEFAULT);
        g_object_bind_property (self->next_source, "visible",
                                self->next_source_label, "visible",
                                G_BINDING_DEFAULT);

        g_settings_bind (self->input_settings, "per-window",
                         self->per_window_source, "active",
                         G_SETTINGS_BIND_DEFAULT);
        g_settings_bind (self->input_settings, "per-window",
                         self->same_source, "active",
                         G_SETTINGS_BIND_DEFAULT | G_SETTINGS_BIND_INVERT_BOOLEAN);

        update_shortcuts (self);
        update_modifiers_shortcut (self);
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
add_input_sources_from_localed (CcRegionPanel *self)
{
        g_autoptr(GVariant) layout_property = NULL;
        g_autoptr(GVariant) variant_property = NULL;
        const gchar *s;
        g_auto(GStrv) layouts = NULL;
        g_auto(GStrv) variants = NULL;
        gint i, n;

        if (!self->localed)
                return;

        layout_property = g_dbus_proxy_get_cached_property (self->localed, "X11Layout");
        if (layout_property) {
                s = g_variant_get_string (layout_property, NULL);
                layouts = g_strsplit (s, ",", -1);
        }

        variant_property = g_dbus_proxy_get_cached_property (self->localed, "X11Variant");
        if (variant_property) {
                s = g_variant_get_string (variant_property, NULL);
                if (s && *s)
                        variants = g_strsplit (s, ",", -1);
        }

        if (variants && variants[0])
                n = MIN (g_strv_length (layouts), g_strv_length (variants));
        else if (layouts && layouts[0])
                n = g_strv_length (layouts);
        else
                n = 0;

        for (i = 0; i < n && layouts[i][0]; i++) {
                g_autoptr(CcInputSourceXkb) source = cc_input_source_xkb_new (self->xkb_info, layouts[i], variants[i]);
                add_input_row (self, CC_INPUT_SOURCE (source));
        }
        gtk_widget_set_visible (GTK_WIDGET (self->no_inputs_row), n == 0);
}

static void
set_localed_locale (CcRegionPanel *self)
{
        g_autoptr(GVariantBuilder) b = NULL;
        g_autofree gchar *lang_value = NULL;

        b = g_variant_builder_new (G_VARIANT_TYPE ("as"));
        lang_value = g_strconcat ("LANG=", self->system_language, NULL);
        g_variant_builder_add (b, "s", lang_value);

        if (self->system_region != NULL &&
            g_strcmp0 (self->system_language, self->system_region) != 0) {
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
set_localed_input (CcRegionPanel *self)
{
        g_autoptr(GString) layouts = NULL;
        g_autoptr(GString) variants = NULL;
        g_autoptr(GList) list = NULL;
        GList *li;

        layouts = g_string_new ("");
        variants = g_string_new ("");

        list = gtk_container_get_children (GTK_CONTAINER (self->input_list));
        for (li = list; li; li = li->next) {
                CcInputRow *row;
                CcInputSourceXkb *source;
                g_autofree gchar *id = NULL;
                const gchar *l, *v;

                if (!CC_IS_INPUT_ROW (li->data))
                        continue;
                row = CC_INPUT_ROW (li->data);

                if (!CC_IS_INPUT_SOURCE_XKB (cc_input_row_get_source (row)))
                        continue;
                source = CC_INPUT_SOURCE_XKB (cc_input_row_get_source (row));

                id = cc_input_source_xkb_get_id (source);
                if (gnome_xkb_info_get_layout_info (self->xkb_info, id, NULL, NULL, &l, &v)) {
                        if (layouts->str[0]) {
                                g_string_append_c (layouts, ',');
                                g_string_append_c (variants, ',');
                        }
                        g_string_append (layouts, l);
                        g_string_append (variants, v);
                }
        }

        g_dbus_proxy_call (self->localed,
                           "SetX11Keyboard",
                           g_variant_new ("(ssssbb)", layouts->str, "", variants->str, "", TRUE, TRUE),
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

        gtk_widget_set_sensitive (GTK_WIDGET (self->login_button), TRUE);

        g_signal_connect_object (self->localed, "g-properties-changed",
                                 G_CALLBACK (on_localed_properties_changed), self, G_CONNECT_SWAPPED);
        on_localed_properties_changed (self, NULL, NULL);
}

static void
login_changed (CcRegionPanel *self)
{
        gboolean can_acquire;

        self->login = gtk_toggle_button_get_active (self->login_button);
        gtk_widget_set_visible (GTK_WIDGET (self->login_label), self->login);

        can_acquire = self->permission &&
                (g_permission_get_allowed (self->permission) ||
                 g_permission_get_can_acquire (self->permission));
        /* FIXME: insensitive doesn't look quite right for this */
        gtk_widget_set_sensitive (GTK_WIDGET (self->language_section_frame), !self->login || can_acquire);
        gtk_widget_set_sensitive (GTK_WIDGET (self->input_section_box), !self->login || can_acquire);

        clear_input_sources (self);
        if (self->login)
                add_input_sources_from_localed (self);
        else
                add_input_sources_from_settings (self);

        update_language_label (self);
        update_buttons (self);
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
        gtk_widget_set_visible (GTK_WIDGET (self->login_button), !self->login_auto_apply);

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
                          self->cancellable,
                          (GAsyncReadyCallback) localed_proxy_ready,
                          self);

        self->login_button = GTK_TOGGLE_BUTTON (gtk_toggle_button_new_with_mnemonic (_("Login _Screen")));
        gtk_style_context_add_class (gtk_widget_get_style_context (GTK_WIDGET (self->login_button)),
                                     "text-button");
        gtk_widget_set_valign (GTK_WIDGET (self->login_button), GTK_ALIGN_CENTER);
        gtk_widget_set_visible (GTK_WIDGET (self->login_button), FALSE);
        gtk_widget_set_sensitive (GTK_WIDGET (self->login_button), FALSE);
        g_signal_connect_object (self->login_button, "notify::active",
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

        object_class->constructed = cc_region_panel_constructed;
        object_class->finalize = cc_region_panel_finalize;

        gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/control-center/region/cc-region-panel.ui");

        gtk_widget_class_bind_template_child (widget_class, CcRegionPanel, add_input_button);
        gtk_widget_class_bind_template_child (widget_class, CcRegionPanel, alt_next_source);
        gtk_widget_class_bind_template_child (widget_class, CcRegionPanel, formats_label);
        gtk_widget_class_bind_template_child (widget_class, CcRegionPanel, formats_row);
        gtk_widget_class_bind_template_child (widget_class, CcRegionPanel, input_list);
        gtk_widget_class_bind_template_child (widget_class, CcRegionPanel, input_section_box);
        gtk_widget_class_bind_template_child (widget_class, CcRegionPanel, login_label);
        gtk_widget_class_bind_template_child (widget_class, CcRegionPanel, language_label);
        gtk_widget_class_bind_template_child (widget_class, CcRegionPanel, language_list);
        gtk_widget_class_bind_template_child (widget_class, CcRegionPanel, language_row);
        gtk_widget_class_bind_template_child (widget_class, CcRegionPanel, language_section_frame);
        gtk_widget_class_bind_template_child (widget_class, CcRegionPanel, move_down_input_button);
        gtk_widget_class_bind_template_child (widget_class, CcRegionPanel, move_up_input_button);
        gtk_widget_class_bind_template_child (widget_class, CcRegionPanel, next_source);
        gtk_widget_class_bind_template_child (widget_class, CcRegionPanel, next_source_label);
        gtk_widget_class_bind_template_child (widget_class, CcRegionPanel, no_inputs_row);
        gtk_widget_class_bind_template_child (widget_class, CcRegionPanel, options_button);
        gtk_widget_class_bind_template_child (widget_class, CcRegionPanel, per_window_source);
        gtk_widget_class_bind_template_child (widget_class, CcRegionPanel, previous_source);
        gtk_widget_class_bind_template_child (widget_class, CcRegionPanel, previous_source_label);
        gtk_widget_class_bind_template_child (widget_class, CcRegionPanel, restart_button);
        gtk_widget_class_bind_template_child (widget_class, CcRegionPanel, restart_revealer);
        gtk_widget_class_bind_template_child (widget_class, CcRegionPanel, same_source);
        gtk_widget_class_bind_template_child (widget_class, CcRegionPanel, show_config_button);

        gtk_widget_class_bind_template_callback (widget_class, restart_now);
        gtk_widget_class_bind_template_callback (widget_class, add_input);
        gtk_widget_class_bind_template_callback (widget_class, move_selected_input_up);
        gtk_widget_class_bind_template_callback (widget_class, move_selected_input_down);
        gtk_widget_class_bind_template_callback (widget_class, show_selected_settings);
}

static void
cc_region_panel_init (CcRegionPanel *self)
{
        g_autoptr(GFile) needs_restart_file = NULL;

        g_resources_register (cc_region_get_resource ());

        gtk_widget_init_template (GTK_WIDGET (self));

        self->user_manager = act_user_manager_get_default ();

        self->cancellable = g_cancellable_new ();

        g_dbus_proxy_new_for_bus (G_BUS_TYPE_SESSION,
                                  G_DBUS_PROXY_FLAGS_NONE,
                                  NULL,
                                  "org.gnome.SessionManager",
                                  "/org/gnome/SessionManager",
                                  "org.gnome.SessionManager",
                                  self->cancellable,
                                  session_proxy_ready,
                                  self);

        setup_login_button (self);
        setup_language_section (self);
        setup_input_section (self);

        needs_restart_file = get_needs_restart_file ();
        if (g_file_query_exists (needs_restart_file, NULL))
                set_restart_notification_visible (self, NULL, TRUE);
}
