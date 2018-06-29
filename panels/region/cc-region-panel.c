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

#include "shell/cc-object-storage.h"
#include "list-box-helper.h"
#include "cc-region-panel.h"
#include "cc-region-resources.h"
#include "cc-language-chooser.h"
#include "cc-format-chooser.h"
#include "cc-input-chooser.h"
#include "cc-input-options.h"

#include "cc-common-language.h"

#define GNOME_DESKTOP_USE_UNSTABLE_API
#include <libgnome-desktop/gnome-languages.h>
#include <libgnome-desktop/gnome-xkb-info.h>

#ifdef HAVE_IBUS
#include <ibus.h>
#include "cc-ibus-utils.h"
#endif

#include <act/act.h>

#define GNOME_DESKTOP_INPUT_SOURCES_DIR "org.gnome.desktop.input-sources"
#define KEY_INPUT_SOURCES        "sources"

#define GNOME_SYSTEM_LOCALE_DIR "org.gnome.system.locale"
#define KEY_REGION "region"

#define INPUT_SOURCE_TYPE_XKB "xkb"
#define INPUT_SOURCE_TYPE_IBUS "ibus"

#define DEFAULT_LOCALE "en_US.utf-8"

typedef enum {
        CHOOSE_LANGUAGE,
        CHOOSE_REGION,
        ADD_INPUT,
        REMOVE_INPUT,
        MOVE_UP_INPUT,
        MOVE_DOWN_INPUT,
} SystemOp;

struct _CcRegionPanel {
	CcPanel      parent_instance;

        GtkWidget   *login_button;
        GtkWidget   *login_label;
        gboolean     login;
        gboolean     login_auto_apply;
        GPermission *permission;
        SystemOp     op;
        GDBusProxy  *localed;
        GDBusProxy  *session;
        GCancellable *cancellable;

        GtkWidget *restart_revealer;
        gchar *needs_restart_file_path;

        GtkWidget     *language_section;
        GtkListBoxRow *language_row;
        GtkWidget     *language_label;
        GtkListBoxRow *formats_row;
        GtkWidget     *formats_label;

        ActUserManager *user_manager;
        ActUser        *user;
        GSettings      *locale_settings;

        gchar *language;
        gchar *region;
        gchar *system_language;
        gchar *system_region;

        GtkWidget *input_section;
        GtkWidget *options_button;
        GtkWidget *input_list;
        GtkWidget *add_input;
        GtkWidget *remove_input;
        GtkWidget *move_up_input;
        GtkWidget *move_down_input;
        GtkWidget *show_config;
        GtkWidget *show_layout;
        GtkWidget *restart_button;
        GtkWidget *language_list;

        GSettings *input_settings;
        GnomeXkbInfo *xkb_info;
#ifdef HAVE_IBUS
        IBusBus *ibus;
        GHashTable *ibus_engines;
        GCancellable *ibus_cancellable;
#endif
};

CC_PANEL_REGISTER (CcRegionPanel, cc_region_panel)

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
        if (self->ibus_cancellable)
                g_cancellable_cancel (self->ibus_cancellable);
        g_clear_object (&self->ibus_cancellable);
        g_clear_pointer (&self->ibus_engines, g_hash_table_destroy);
#endif
        g_free (self->language);
        g_free (self->region);
        g_free (self->system_language);
        g_free (self->system_region);

        g_clear_pointer (&self->needs_restart_file_path, g_free);

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
                                                 self->login_button);
}

static const char *
cc_region_panel_get_help_uri (CcPanel *panel)
{
        return "help:gnome-help/prefs-language";
}

static void
restart_now (CcRegionPanel *self)
{
        g_file_delete (g_file_new_for_path (self->needs_restart_file_path),
                                            NULL, NULL);

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

        if (locale) {
                current_locale = g_strdup (setlocale (LC_MESSAGES, NULL));
                setlocale (LC_MESSAGES, locale);
        }

        gtk_revealer_set_reveal_child (GTK_REVEALER (self->restart_revealer), visible);

        if (locale)
                setlocale (LC_MESSAGES, current_locale);

        if (!visible) {
                g_file_delete (g_file_new_for_path (self->needs_restart_file_path),
                                                    NULL, NULL);

                return;
        }

        if (!g_file_set_contents (self->needs_restart_file_path, "", -1, NULL))
                g_warning ("Unable to create %s", self->needs_restart_file_path);
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
language_response (GtkDialog     *chooser,
                   gint           response_id,
                   CcRegionPanel *self)
{
        const gchar *language;

        if (response_id == GTK_RESPONSE_OK) {
                language = cc_language_chooser_get_language (GTK_WIDGET (chooser));
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
format_response (CcFormatChooser *chooser,
                 gint             response_id,
                 CcRegionPanel   *self)
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
        GtkWidget *toplevel;
        GtkWidget *chooser;

        toplevel = gtk_widget_get_toplevel (GTK_WIDGET (self));
        chooser = cc_language_chooser_new (toplevel);
        cc_language_chooser_set_language (chooser, get_effective_language (self));
        g_signal_connect (chooser, "response",
                          G_CALLBACK (language_response), self);
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
        g_signal_connect (chooser, "response",
                          G_CALLBACK (format_response), self);
        gtk_window_present (GTK_WINDOW (chooser));
}

static void show_input_chooser (CcRegionPanel *self);
static void remove_selected_input (CcRegionPanel *self);
static void move_selected_input (CcRegionPanel *self,
                                 SystemOp       op);

static void
permission_acquired (GObject      *source,
                     GAsyncResult *res,
                     gpointer      data)
{
        CcRegionPanel *self = data;
        g_autoptr(GError) error = NULL;
        gboolean allowed;

        allowed = g_permission_acquire_finish (self->permission, res, &error);
        if (error) {
                if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
                        g_warning ("Failed to acquire permission: %s\n", error->message);
                return;
        }

        if (allowed) {
                switch (self->op) {
                case CHOOSE_LANGUAGE:
                        show_language_chooser (self);
                        break;
                case CHOOSE_REGION:
                        show_region_chooser (self);
                        break;
                case ADD_INPUT:
                        show_input_chooser (self);
                        break;
                case REMOVE_INPUT:
                        remove_selected_input (self);
                        break;
                case MOVE_UP_INPUT:
                case MOVE_DOWN_INPUT:
                        move_selected_input (self, self->op);
                        break;
                default:
                        g_warning ("Unknown privileged operation: %d\n", self->op);
                        break;
                }
        }
}

static void
activate_language_row (CcRegionPanel *self,
                       GtkListBoxRow *row)
{
        if (row == self->language_row) {
                if (!self->login || g_permission_get_allowed (self->permission)) {
                        show_language_chooser (self);
                } else if (g_permission_get_can_acquire (self->permission)) {
                        self->op = CHOOSE_LANGUAGE;
                        g_permission_acquire_async (self->permission,
                                                    NULL,
                                                    permission_acquired,
                                                    self);
                }
        } else if (row == self->formats_row) {
                if (!self->login || g_permission_get_allowed (self->permission)) {
                        show_region_chooser (self);
                } else if (g_permission_get_can_acquire (self->permission)) {
                        self->op = CHOOSE_REGION;
                        g_permission_acquire_async (self->permission,
                                                    NULL,
                                                    permission_acquired,
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

        gtk_label_set_label (GTK_LABEL (self->formats_label), name);
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

        gtk_label_set_label (GTK_LABEL (self->language_label), name);

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
        g_signal_connect_swapped (self->user, "notify::language",
                                  G_CALLBACK (update_language_from_user), self);
        g_signal_connect_swapped (self->user, "notify::is-loaded",
                                  G_CALLBACK (update_language_from_user), self);

        self->locale_settings = g_settings_new (GNOME_SYSTEM_LOCALE_DIR);
        g_signal_connect_swapped (self->locale_settings, "changed::" KEY_REGION,
                                  G_CALLBACK (update_region_from_setting), self);

        gtk_list_box_set_selection_mode (GTK_LIST_BOX (self->language_list),
                                         GTK_SELECTION_NONE);
        gtk_list_box_set_header_func (GTK_LIST_BOX (self->language_list),
                                      cc_list_box_update_header_func,
                                      NULL, NULL);
        g_signal_connect_swapped (self->language_list, "row-activated",
                                  G_CALLBACK (activate_language_row), self);

        update_language_from_user (self);
        update_region_from_setting (self);
}

#ifdef HAVE_IBUS
static void
update_ibus_active_sources (CcRegionPanel *self)
{
        g_autoptr(GList) rows = NULL;
        GList *l;
        GtkWidget *row;
        const gchar *type;
        const gchar *id;
        IBusEngineDesc *engine_desc;
        GtkWidget *label;

        rows = gtk_container_get_children (GTK_CONTAINER (self->input_list));
        for (l = rows; l; l = l->next) {
                row = l->data;
                type = g_object_get_data (G_OBJECT (row), "type");
                id = g_object_get_data (G_OBJECT (row), "id");
                if (g_strcmp0 (type, INPUT_SOURCE_TYPE_IBUS) != 0)
                        continue;

                engine_desc = g_hash_table_lookup (self->ibus_engines, id);
                if (engine_desc) {
                        g_autofree gchar *display_name = engine_get_display_name (engine_desc);
                        label = GTK_WIDGET (g_object_get_data (G_OBJECT (row), "label"));
                        gtk_label_set_text (GTK_LABEL (label), display_name);
                }
        }
}

static void
update_input_chooser (CcRegionPanel *self)
{
        GtkWidget *chooser;

        chooser = g_object_get_data (G_OBJECT (self), "input-chooser");
        if (!chooser)
                return;

        cc_input_chooser_set_ibus_engines (chooser, self->ibus_engines);
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

        g_clear_object (&self->ibus_cancellable);

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
        update_input_chooser (self);
}

static void
fetch_ibus_engines (CcRegionPanel *self)
{
        self->ibus_cancellable = g_cancellable_new ();

        ibus_bus_list_engines_async (self->ibus,
                                     -1,
                                     self->ibus_cancellable,
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

static GDesktopAppInfo *
setup_app_info_for_id (const gchar *id)
{
        g_autofree gchar *desktop_file_name = NULL;
        g_auto(GStrv) strv = NULL;

        strv = g_strsplit (id, ":", 2);
        desktop_file_name = g_strdup_printf ("ibus-setup-%s.desktop", strv[0]);

        return g_desktop_app_info_new (desktop_file_name);
}
#endif

static void
remove_no_input_row (GtkContainer *list)
{
        g_autoptr(GList) l = NULL;

        l = gtk_container_get_children (list);
        if (!l)
                return;
        if (l->next != NULL)
                return;
        if (g_strcmp0 (g_object_get_data (G_OBJECT (l->data), "type"), "none") == 0)
                gtk_container_remove (list, GTK_WIDGET (l->data));
}

static GtkWidget *
add_input_row (CcRegionPanel   *self,
               const gchar     *type,
               const gchar     *id,
               const gchar     *name,
               GDesktopAppInfo *app_info)
{
        GtkWidget *row;
        GtkWidget *box;
        GtkWidget *label;
        GtkWidget *image;

        remove_no_input_row (GTK_CONTAINER (self->input_list));

        row = gtk_list_box_row_new ();
        box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
        gtk_container_add (GTK_CONTAINER (row), box);
        label = gtk_label_new (name);
        gtk_widget_set_halign (label, GTK_ALIGN_START);
        gtk_widget_set_margin_start (label, 20);
        gtk_widget_set_margin_end (label, 20);
        gtk_widget_set_margin_top (label, 18);
        gtk_widget_set_margin_bottom (label, 18);
        gtk_box_pack_start (GTK_BOX (box), label, TRUE, TRUE, 0);

        if (strcmp (type, INPUT_SOURCE_TYPE_IBUS) == 0) {
                image = gtk_image_new_from_icon_name ("system-run-symbolic", GTK_ICON_SIZE_BUTTON);
                gtk_widget_set_margin_start (image, 20);
                gtk_widget_set_margin_end (image, 20);
                gtk_widget_set_margin_top (image, 6);
                gtk_widget_set_margin_bottom (image, 6);
                gtk_style_context_add_class (gtk_widget_get_style_context (image), "dim-label");
                gtk_box_pack_start (GTK_BOX (box), image, FALSE, TRUE, 0);
        }

        gtk_widget_show_all (row);
        gtk_container_add (GTK_CONTAINER (self->input_list), row);

        g_object_set_data (G_OBJECT (row), "label", label);
        g_object_set_data (G_OBJECT (row), "type", (gpointer)type);
        g_object_set_data_full (G_OBJECT (row), "id", g_strdup (id), g_free);
        if (app_info) {
                g_object_set_data_full (G_OBJECT (row), "app-info", g_object_ref (app_info), g_object_unref);
        }

        cc_list_box_adjust_scrolling (GTK_LIST_BOX (self->input_list));

        return row;
}

static void
add_no_input_row (CcRegionPanel *self)
{
        add_input_row (self, "none", "none", _("No input source selected"), NULL);
}

static void
add_input_sources (CcRegionPanel *self,
                   GVariant      *sources)
{
        GVariantIter iter;
        const gchar *type, *id;

        if (g_variant_n_children (sources) < 1) {
                add_no_input_row (self);
                return;
        }

        g_variant_iter_init (&iter, sources);
        while (g_variant_iter_next (&iter, "(&s&s)", &type, &id)) {
                g_autoptr(GDesktopAppInfo) app_info = NULL;
                g_autofree gchar *display_name = NULL;

                if (g_str_equal (type, INPUT_SOURCE_TYPE_XKB)) {
                        const gchar *name;

                        gnome_xkb_info_get_layout_info (self->xkb_info, id, &name, NULL, NULL, NULL);
                        if (!name) {
                                g_warning ("Couldn't find XKB input source '%s'", id);
                                continue;
                        }
                        display_name = g_strdup (name);
                        type = INPUT_SOURCE_TYPE_XKB;
#ifdef HAVE_IBUS
                } else if (g_str_equal (type, INPUT_SOURCE_TYPE_IBUS)) {
                        IBusEngineDesc *engine_desc = NULL;

                        if (self->ibus_engines)
                                engine_desc = g_hash_table_lookup (self->ibus_engines, id);
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
                gtk_container_remove (GTK_CONTAINER (self->input_list), GTK_WIDGET (l->data));
        }

        cc_list_box_adjust_scrolling (GTK_LIST_BOX (self->input_list));
}

static void
select_by_id (GtkWidget   *row,
              gpointer     data)
{
        const gchar *id = data;
        const gchar *row_id;

        row_id = (const gchar *)g_object_get_data (G_OBJECT (row), "id");
        if (g_strcmp0 (row_id, id) == 0)
                gtk_list_box_select_row (GTK_LIST_BOX (gtk_widget_get_parent (row)), GTK_LIST_BOX_ROW (row));
}

static void
select_input (CcRegionPanel *self,
              const gchar   *id)
{
        gtk_container_foreach (GTK_CONTAINER (self->input_list),
                               select_by_id, (gpointer)id);
}

static void
input_sources_changed (GSettings     *settings,
                       const gchar   *key,
                       CcRegionPanel *self)
{
        GtkListBoxRow *selected;
        g_autofree gchar *id = NULL;

        selected = gtk_list_box_get_selected_row (GTK_LIST_BOX (self->input_list));
        if (selected)
                id = g_strdup (g_object_get_data (G_OBJECT (selected), "id"));
        clear_input_sources (self);
        add_input_sources_from_settings (self);
        if (id)
                select_input (self, id);
}


static void
update_buttons (CcRegionPanel *self)
{
        GtkListBoxRow *selected;
        g_autoptr(GList) children = NULL;
        guint n_rows;

        children = gtk_container_get_children (GTK_CONTAINER (self->input_list));
        n_rows = g_list_length (children);

        selected = gtk_list_box_get_selected_row (GTK_LIST_BOX (self->input_list));
        if (selected == NULL) {
                gtk_widget_set_visible (self->show_config, FALSE);
                gtk_widget_set_sensitive (self->remove_input, FALSE);
                gtk_widget_set_sensitive (self->show_layout, FALSE);
                gtk_widget_set_sensitive (self->move_up_input, FALSE);
                gtk_widget_set_sensitive (self->move_down_input, FALSE);
        } else {
                GDesktopAppInfo *app_info;

                app_info = (GDesktopAppInfo *)g_object_get_data (G_OBJECT (selected), "app-info");

                gtk_widget_set_visible (self->show_config, app_info != NULL);
                gtk_widget_set_sensitive (self->show_layout, TRUE);
                gtk_widget_set_sensitive (self->remove_input, n_rows > 1);
                gtk_widget_set_sensitive (self->move_up_input, gtk_list_box_row_get_index (selected) > 0);
                gtk_widget_set_sensitive (self->move_down_input, gtk_list_box_row_get_index (selected) < n_rows - 1);
        }

        gtk_widget_set_visible (self->options_button,
                                n_rows > 1 && !self->login);
}

static void
set_input_settings (CcRegionPanel *self)
{
        const gchar *type;
        const gchar *id;
        GVariantBuilder builder;
        g_autoptr(GList) list = NULL;
        GList *l;

        g_variant_builder_init (&builder, G_VARIANT_TYPE ("a(ss)"));
        list = gtk_container_get_children (GTK_CONTAINER (self->input_list));
        for (l = list; l; l = l->next) {
                type = (const gchar *)g_object_get_data (G_OBJECT (l->data), "type");
                id = (const gchar *)g_object_get_data (G_OBJECT (l->data), "id");
                g_variant_builder_add (&builder, "(ss)", type, id);
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

static gboolean
input_source_already_added (CcRegionPanel *self,
                            const gchar   *id)
{
        g_autoptr(GList) list = NULL;
        GList *l;

        list = gtk_container_get_children (GTK_CONTAINER (self->input_list));
        for (l = list; l; l = l->next)
                if (g_str_equal (id, (const gchar *) g_object_get_data (G_OBJECT (l->data), "id"))) {
                        return TRUE;
                }

        return FALSE;
}

static void
run_input_chooser (CcRegionPanel *self, GtkWidget *chooser)
{
        if (gtk_dialog_run (GTK_DIALOG (chooser)) == GTK_RESPONSE_OK) {
                g_autofree gchar *type = NULL;
                g_autofree gchar *id = NULL;
                g_autofree gchar *name = NULL;

                if (cc_input_chooser_get_selected (chooser, &type, &id, &name) &&
                    !input_source_already_added (self, id)) {
                        g_autoptr(GDesktopAppInfo) app_info = NULL;

                        if (g_str_equal (type, INPUT_SOURCE_TYPE_IBUS)) {
#ifdef HAVE_IBUS
                                app_info = setup_app_info_for_id (id);
#endif
                        } else {
                                g_free (type);
                                type = g_strdup (INPUT_SOURCE_TYPE_XKB);
                        }

                        add_input_row (self, type, id, name, app_info);
                        update_buttons (self);
                        update_input (self);
                }
        }
        gtk_widget_hide(chooser);
}

static void
show_input_chooser (CcRegionPanel *self)
{
        GtkWidget *chooser;
        GtkWidget *toplevel;

        chooser = g_object_get_data (G_OBJECT (self), "input-chooser");

        if (!chooser) {
                toplevel = gtk_widget_get_toplevel (GTK_WIDGET (self));
                chooser = cc_input_chooser_new (GTK_WINDOW (toplevel),
                                                self->login,
                                                self->xkb_info,
#ifdef HAVE_IBUS
                                                self->ibus_engines
#else
                                                NULL
#endif
                                                );
                g_object_ref (chooser);
                g_object_set_data_full (G_OBJECT (self), "input-chooser",
                                        chooser, g_object_unref);
        } else {
                cc_input_chooser_reset (chooser);
        }

        run_input_chooser (self, chooser);
}

static void
add_input (CcRegionPanel *self)
{
        if (!self->login) {
                show_input_chooser (self);
        } else if (g_permission_get_allowed (self->permission)) {
                show_input_chooser (self);
        } else if (g_permission_get_can_acquire (self->permission)) {
                self->op = ADD_INPUT;
                g_permission_acquire_async (self->permission,
                                            NULL,
                                            permission_acquired,
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
do_remove_selected_input (CcRegionPanel *self)
{
        GtkListBoxRow *selected;
        GtkWidget *sibling;

        selected = gtk_list_box_get_selected_row (GTK_LIST_BOX (self->input_list));
        if (selected == NULL)
                return;

        sibling = find_sibling (GTK_CONTAINER (self->input_list), GTK_WIDGET (selected));
        gtk_container_remove (GTK_CONTAINER (self->input_list), GTK_WIDGET (selected));
        gtk_list_box_select_row (GTK_LIST_BOX (self->input_list), GTK_LIST_BOX_ROW (sibling));

        cc_list_box_adjust_scrolling (GTK_LIST_BOX (self->input_list));

        update_buttons (self);
        update_input (self);
}

static void
remove_selected_input (CcRegionPanel *self)
{
        if (!self->login) {
                do_remove_selected_input (self);
        } else if (g_permission_get_allowed (self->permission)) {
                do_remove_selected_input (self);
        } else if (g_permission_get_can_acquire (self->permission)) {
                self->op = REMOVE_INPUT;
                g_permission_acquire_async (self->permission,
                                            NULL,
                                            permission_acquired,
                                            self);
        }
}

static void
do_move_selected_input (CcRegionPanel *self,
                        SystemOp       op)
{
        GtkListBoxRow *selected;
        gint idx;

        g_assert (op == MOVE_UP_INPUT || op == MOVE_DOWN_INPUT);

        selected = gtk_list_box_get_selected_row (GTK_LIST_BOX (self->input_list));
        g_assert (selected);

        idx = gtk_list_box_row_get_index (selected);
        if (op == MOVE_UP_INPUT)
                idx -= 1;
        else
                idx += 1;

        gtk_list_box_unselect_row (GTK_LIST_BOX (self->input_list), selected);

        g_object_ref (selected);
        gtk_container_remove (GTK_CONTAINER (self->input_list), GTK_WIDGET (selected));
        gtk_list_box_insert (GTK_LIST_BOX (self->input_list), GTK_WIDGET (selected), idx);
        g_object_unref (selected);

        gtk_list_box_select_row (GTK_LIST_BOX (self->input_list), selected);

        cc_list_box_adjust_scrolling (GTK_LIST_BOX (self->input_list));

        update_buttons (self);
        update_input (self);
}

static void
move_selected_input (CcRegionPanel *self,
                     SystemOp       op)
{
        if (!self->login) {
                do_move_selected_input (self, op);
        } else if (g_permission_get_allowed (self->permission)) {
                do_move_selected_input (self, op);
        } else if (g_permission_get_can_acquire (self->permission)) {
                self->op = op;
                g_permission_acquire_async (self->permission,
                                            NULL,
                                            permission_acquired,
                                            self);
        }
}

static void
move_selected_input_up (CcRegionPanel *self)
{
        move_selected_input (self, MOVE_UP_INPUT);
}

static void
move_selected_input_down (CcRegionPanel *self)
{
        move_selected_input (self, MOVE_DOWN_INPUT);
}

static void
show_selected_settings (CcRegionPanel *self)
{
        GtkListBoxRow *selected;
        g_autoptr(GdkAppLaunchContext) ctx = NULL;
        GDesktopAppInfo *app_info;
        const gchar *id;
        g_autoptr(GError) error = NULL;

        selected = gtk_list_box_get_selected_row (GTK_LIST_BOX (self->input_list));
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

        if (!g_app_info_launch (G_APP_INFO (app_info), NULL, G_APP_LAUNCH_CONTEXT (ctx), &error))
                g_warning ("Failed to launch input source setup: %s", error->message);
}

static void
show_selected_layout (CcRegionPanel *self)
{
        GtkListBoxRow *selected;
        const gchar *type;
        const gchar *id;
        const gchar *layout;
        const gchar *variant;
        g_autofree gchar *commandline = NULL;

        selected = gtk_list_box_get_selected_row (GTK_LIST_BOX (self->input_list));
        if (selected == NULL)
                return;

        type = (const gchar *)g_object_get_data (G_OBJECT (selected), "type");
        id = (const gchar *)g_object_get_data (G_OBJECT (selected), "id");

        if (g_str_equal (type, INPUT_SOURCE_TYPE_XKB)) {
                gnome_xkb_info_get_layout_info (self->xkb_info,
                                                id, NULL, NULL,
                                                &layout, &variant);

                if (!layout || !layout[0]) {
                        g_warning ("Couldn't find XKB input source '%s'", id);
                        return;
                }
#ifdef HAVE_IBUS
        } else if (g_str_equal (type, INPUT_SOURCE_TYPE_IBUS)) {
                IBusEngineDesc *engine_desc = NULL;

                if (self->ibus_engines)
                        engine_desc = g_hash_table_lookup (self->ibus_engines, id);

                if (engine_desc) {
                        layout = ibus_engine_desc_get_layout (engine_desc);
                        variant = ibus_engine_desc_get_layout_variant (engine_desc);
                } else {
                        g_warning ("Couldn't find IBus input source '%s'", id);
                        return;
                }
#endif
        } else {
                g_warning ("Unhandled input source type '%s'", type);
                return;
        }

        if (variant && variant[0])
                commandline = g_strdup_printf ("gkbd-keyboard-display -l \"%s\t%s\"",
                                               layout, variant);
        else
                commandline = g_strdup_printf ("gkbd-keyboard-display -l %s",
                                               layout);

        g_spawn_command_line_async (commandline, NULL);
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
                        g_signal_connect_swapped (self->ibus, "connected",
                                                  G_CALLBACK (fetch_ibus_engines), self);
        }
        maybe_start_ibus ();
#endif

        cc_list_box_setup_scrolling (GTK_LIST_BOX (self->input_list), 5);

        gtk_list_box_set_selection_mode (GTK_LIST_BOX (self->input_list),
                                         GTK_SELECTION_SINGLE);
        gtk_list_box_set_header_func (GTK_LIST_BOX (self->input_list),
                                      cc_list_box_update_header_func,
                                      NULL, NULL);
        g_signal_connect_object (self->input_list, "row-selected",
                                 G_CALLBACK (update_buttons), self, G_CONNECT_SWAPPED);

        g_signal_connect (self->input_settings, "changed::" KEY_INPUT_SOURCES,
                          G_CALLBACK (input_sources_changed), self);

        add_input_sources_from_settings (self);
        update_buttons (self);
}

static void
on_localed_properties_changed (GDBusProxy     *proxy,
                               GVariant       *changed_properties,
                               const gchar   **invalidated_properties,
                               CcRegionPanel  *self)
{
        g_autoptr(GVariant) v = NULL;

        v = g_dbus_proxy_get_cached_property (proxy, "Locale");
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
                const gchar *name;
                g_autofree gchar *id = NULL;

                if (variants && variants[i] && variants[i][0])
                        id = g_strdup_printf ("%s+%s", layouts[i], variants[i]);
                else
                        id = g_strdup (layouts[i]);

                gnome_xkb_info_get_layout_info (self->xkb_info, id, &name, NULL, NULL, NULL);

                add_input_row (self, INPUT_SOURCE_TYPE_XKB, id, name ? name : id, NULL);
        }
        if (n == 0) {
                add_no_input_row (self);
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
        const gchar *type, *id;
        g_autoptr(GList) list = NULL;
        GList *li;
        const gchar *l, *v;

        layouts = g_string_new ("");
        variants = g_string_new ("");

        list = gtk_container_get_children (GTK_CONTAINER (self->input_list));
        for (li = list; li; li = li->next) {
                type = (const gchar *)g_object_get_data (G_OBJECT (li->data), "type");
                id = (const gchar *)g_object_get_data (G_OBJECT (li->data), "id");
                if (g_str_equal (type, INPUT_SOURCE_TYPE_IBUS))
                        continue;

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

        gtk_widget_set_sensitive (self->login_button, TRUE);

        g_signal_connect (self->localed, "g-properties-changed",
                          G_CALLBACK (on_localed_properties_changed), self);
        on_localed_properties_changed (self->localed, NULL, NULL, self);
}

static void
login_changed (CcRegionPanel *self)
{
        gboolean can_acquire;

        self->login = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (self->login_button));
        gtk_widget_set_visible (self->login_label, self->login);

        can_acquire = self->permission &&
                (g_permission_get_allowed (self->permission) ||
                 g_permission_get_can_acquire (self->permission));
        /* FIXME: insensitive doesn't look quite right for this */
        gtk_widget_set_sensitive (self->language_section, !self->login || can_acquire);
        gtk_widget_set_sensitive (self->input_section, !self->login || can_acquire);

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
        gtk_widget_set_visible (self->login_button, !self->login_auto_apply);

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

        self->login_button = gtk_toggle_button_new_with_mnemonic (_("Login _Screen"));
        gtk_style_context_add_class (gtk_widget_get_style_context (self->login_button),
                                     "text-button");
        gtk_widget_set_valign (self->login_button, GTK_ALIGN_CENTER);
        gtk_widget_set_visible (self->login_button, FALSE);
        gtk_widget_set_sensitive (self->login_button, FALSE);
        g_signal_connect_swapped (self->login_button, "notify::active",
                                  G_CALLBACK (login_changed), self);

        g_object_get (self->user_manager, "is-loaded", &loaded, NULL);
        if (loaded)
                set_login_button_visibility (self);
        else
                g_signal_connect_swapped (self->user_manager, "notify::is-loaded",
                                          G_CALLBACK (set_login_button_visibility), self);
}

static void
session_proxy_ready (GObject      *source,
                     GAsyncResult *res,
                     gpointer      data)
{
        CcRegionPanel *self = data;
        GDBusProxy *proxy;
        g_autoptr(GError) error = NULL;

        proxy = cc_object_storage_create_dbus_proxy_finish (res, &error);

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

        gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/control-center/region/region.ui");

        gtk_widget_class_bind_template_child (widget_class, CcRegionPanel, language_row);
        gtk_widget_class_bind_template_child (widget_class, CcRegionPanel, language_label);
        gtk_widget_class_bind_template_child (widget_class, CcRegionPanel, formats_row);
        gtk_widget_class_bind_template_child (widget_class, CcRegionPanel, formats_label);
        gtk_widget_class_bind_template_child (widget_class, CcRegionPanel, restart_revealer);
        gtk_widget_class_bind_template_child (widget_class, CcRegionPanel, input_section);
        gtk_widget_class_bind_template_child (widget_class, CcRegionPanel, options_button);
        gtk_widget_class_bind_template_child (widget_class, CcRegionPanel, input_list);
        gtk_widget_class_bind_template_child (widget_class, CcRegionPanel, add_input);
        gtk_widget_class_bind_template_child (widget_class, CcRegionPanel, remove_input);
        gtk_widget_class_bind_template_child (widget_class, CcRegionPanel, move_up_input);
        gtk_widget_class_bind_template_child (widget_class, CcRegionPanel, move_down_input);
        gtk_widget_class_bind_template_child (widget_class, CcRegionPanel, show_config);
        gtk_widget_class_bind_template_child (widget_class, CcRegionPanel, show_layout);
        gtk_widget_class_bind_template_child (widget_class, CcRegionPanel, restart_button);
        gtk_widget_class_bind_template_child (widget_class, CcRegionPanel, login_label);
        gtk_widget_class_bind_template_child (widget_class, CcRegionPanel, language_list);

        gtk_widget_class_bind_template_callback (widget_class, restart_now);
        gtk_widget_class_bind_template_callback (widget_class, show_input_options);
        gtk_widget_class_bind_template_callback (widget_class, add_input);
        gtk_widget_class_bind_template_callback (widget_class, remove_selected_input);
        gtk_widget_class_bind_template_callback (widget_class, move_selected_input_up);
        gtk_widget_class_bind_template_callback (widget_class, move_selected_input_down);
        gtk_widget_class_bind_template_callback (widget_class, show_selected_settings);
        gtk_widget_class_bind_template_callback (widget_class, show_selected_layout);
}

static void
cc_region_panel_init (CcRegionPanel *self)
{
        g_resources_register (cc_region_get_resource ());

        gtk_widget_init_template (GTK_WIDGET (self));

        self->user_manager = act_user_manager_get_default ();

        self->cancellable = g_cancellable_new ();

        cc_object_storage_create_dbus_proxy (G_BUS_TYPE_SESSION,
                                             G_DBUS_PROXY_FLAGS_NONE,
                                             "org.gnome.SessionManager",
                                             "/org/gnome/SessionManager",
                                             "org.gnome.SessionManager",
                                             self->cancellable,
                                             session_proxy_ready,
                                             self);

        setup_login_button (self);
        setup_language_section (self);
        setup_input_section (self);

        self->needs_restart_file_path = g_build_filename (g_get_user_runtime_dir (),
                                                          "gnome-control-center-region-needs-restart",
                                                          NULL);
        if (g_file_query_exists (g_file_new_for_path (self->needs_restart_file_path), NULL))
                set_restart_notification_visible (self, NULL, TRUE);
}
