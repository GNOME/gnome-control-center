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

#include "shell/list-box-helper.h"
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

CC_PANEL_REGISTER (CcRegionPanel, cc_region_panel)

#define WID(s) GTK_WIDGET (gtk_builder_get_object (self->priv->builder, s))

#define REGION_PANEL_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), CC_TYPE_REGION_PANEL, CcRegionPanelPrivate))

typedef enum {
        CHOOSE_LANGUAGE,
        CHOOSE_REGION,
        ADD_INPUT,
        REMOVE_INPUT,
        MOVE_UP_INPUT,
        MOVE_DOWN_INPUT,
} SystemOp;

struct _CcRegionPanelPrivate {
	GtkBuilder *builder;

        GtkWidget   *login_button;
        GtkWidget   *login_label;
        gboolean     login;
        gboolean     login_auto_apply;
        GPermission *permission;
        SystemOp     op;
        GDBusProxy  *localed;
        GDBusProxy  *session;
        GCancellable *cancellable;

        GtkWidget *overlay;
        GtkWidget *notification;

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
	GtkWidget *chooser;

        g_cancellable_cancel (priv->cancellable);
        g_clear_object (&priv->cancellable);

        if (priv->user_manager) {
                g_signal_handlers_disconnect_by_data (priv->user_manager, self);
                priv->user_manager = NULL;
        }

        if (priv->user) {
                g_signal_handlers_disconnect_by_data (priv->user, self);
                priv->user = NULL;
        }

        g_clear_object (&priv->permission);
        g_clear_object (&priv->localed);
        g_clear_object (&priv->session);
        g_clear_object (&priv->builder);
        g_clear_object (&priv->locale_settings);
        g_clear_object (&priv->input_settings);
        g_clear_object (&priv->xkb_info);
#ifdef HAVE_IBUS
        g_clear_object (&priv->ibus);
        if (priv->ibus_cancellable)
                g_cancellable_cancel (priv->ibus_cancellable);
        g_clear_object (&priv->ibus_cancellable);
        g_clear_pointer (&priv->ibus_engines, g_hash_table_destroy);
#endif
        g_free (priv->language);
        g_free (priv->region);
        g_free (priv->system_language);
        g_free (priv->system_region);

        chooser = g_object_get_data (G_OBJECT (self), "input-chooser");
        if (chooser)
                gtk_widget_destroy (chooser);

	G_OBJECT_CLASS (cc_region_panel_parent_class)->finalize (object);
}

static void
cc_region_panel_constructed (GObject *object)
{
        CcRegionPanel *self = CC_REGION_PANEL (object);
	CcRegionPanelPrivate *priv = self->priv;

        G_OBJECT_CLASS (cc_region_panel_parent_class)->constructed (object);

        if (priv->permission)
                cc_shell_embed_widget_in_header (cc_panel_get_shell (CC_PANEL (object)),
                                                 priv->login_button);
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
        CcRegionPanelPrivate *priv = self->priv;

        gtk_revealer_set_reveal_child (GTK_REVEALER (self->priv->notification), FALSE);

        g_dbus_proxy_call (priv->session,
                           "Logout",
                           g_variant_new ("(u)", 0),
                           G_DBUS_CALL_FLAGS_NONE,
                           -1, NULL, NULL, NULL);
}

static void
show_restart_notification (CcRegionPanel *self,
                           const gchar   *locale)
{
	CcRegionPanelPrivate *priv = self->priv;
        gchar *current_locale = NULL;

        if (locale) {
                current_locale = g_strdup (setlocale (LC_MESSAGES, NULL));
                setlocale (LC_MESSAGES, locale);
        }

        gtk_revealer_set_reveal_child (GTK_REVEALER (priv->notification), TRUE);

        if (locale) {
                setlocale (LC_MESSAGES, current_locale);
                g_free (current_locale);
        }
}

static void
dismiss_notification (CcRegionPanel *self)
{
        CcRegionPanelPrivate *priv = self->priv;

        gtk_revealer_set_reveal_child (GTK_REVEALER (priv->notification), FALSE);
}

typedef struct {
        CcRegionPanel *self;
        int category;
        gchar *target_locale;
} MaybeNotifyData;

static void
maybe_notify_finish (GObject      *source,
                     GAsyncResult *res,
                     gpointer      data)
{
        MaybeNotifyData *mnd = data;
        CcRegionPanel *self = mnd->self;
        GError *error = NULL;
        GVariant *retval = NULL;
        gchar *current_lang_code = NULL;
        gchar *current_country_code = NULL;
        gchar *target_lang_code = NULL;
        gchar *target_country_code = NULL;
        const gchar *current_locale = NULL;

        retval = g_dbus_proxy_call_finish (G_DBUS_PROXY (source), res, &error);
        if (!retval) {
                if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
                        g_warning ("Failed to get locale: %s\n", error->message);
                goto out;
        }

        g_variant_get (retval, "(&s)", &current_locale);

        if (!gnome_parse_locale (current_locale,
                                 &current_lang_code,
                                 &current_country_code,
                                 NULL,
                                 NULL))
                goto out;

        if (!gnome_parse_locale (mnd->target_locale,
                                 &target_lang_code,
                                 &target_country_code,
                                 NULL,
                                 NULL))
                goto out;

        if (g_str_equal (current_lang_code, target_lang_code) == FALSE ||
            g_str_equal (current_country_code, target_country_code) == FALSE)
                show_restart_notification (self,
                                           mnd->category == LC_MESSAGES ? mnd->target_locale : NULL);
out:
        g_free (target_country_code);
        g_free (target_lang_code);
        g_free (current_country_code);
        g_free (current_lang_code);
        g_clear_pointer (&retval, g_variant_unref);
        g_clear_error (&error);
        g_free (mnd->target_locale);
        g_free (mnd);
}

static void
maybe_notify (CcRegionPanel *self,
              int            category,
              const gchar   *target_locale)
{
        CcRegionPanelPrivate *priv = self->priv;
        MaybeNotifyData *mnd;

        mnd = g_new0 (MaybeNotifyData, 1);
        mnd->self = self;
        mnd->category = category;
        mnd->target_locale = g_strdup (target_locale);

        g_dbus_proxy_call (priv->session,
                           "GetLocale",
                           g_variant_new ("(i)", category),
                           G_DBUS_CALL_FLAGS_NONE,
                           -1,
                           priv->cancellable,
                           maybe_notify_finish,
                           mnd);
}

static void set_localed_locale (CcRegionPanel *self);

static void
set_system_language (CcRegionPanel *self,
                     const gchar   *language)
{
        CcRegionPanelPrivate *priv = self->priv;

        if (g_strcmp0 (language, priv->system_language) == 0)
                return;

        g_free (priv->system_language);
        priv->system_language = g_strdup (language);

        set_localed_locale (self);
}

static void
update_language (CcRegionPanel *self,
                 const gchar   *language)
{
	CcRegionPanelPrivate *priv = self->priv;

        if (priv->login) {
                set_system_language (self, language);
        } else {
                if (g_strcmp0 (language, priv->language) == 0)
                        return;
                act_user_set_language (priv->user, language);
                if (priv->login_auto_apply)
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
        CcRegionPanelPrivate *priv = self->priv;

        if (g_strcmp0 (region, priv->system_region) == 0)
                return;

        g_free (priv->system_region);
        priv->system_region = g_strdup (region);

        set_localed_locale (self);
}

static void
update_region (CcRegionPanel *self,
               const gchar   *region)
{
	CcRegionPanelPrivate *priv = self->priv;

        if (priv->login) {
                set_system_region (self, region);
        } else {
                if (g_strcmp0 (region, priv->region) == 0)
                        return;
                g_settings_set_string (priv->locale_settings, KEY_REGION, region);
                if (priv->login_auto_apply)
                        set_system_region (self, region);
                maybe_notify (self, LC_TIME, region);
        }
}

static void
format_response (GtkDialog *chooser,
                 gint       response_id,
                 CcRegionPanel *self)
{
        const gchar *region;

        if (response_id == GTK_RESPONSE_OK) {
                region = cc_format_chooser_get_region (GTK_WIDGET (chooser));
                update_region (self, region);
        }

        gtk_widget_destroy (GTK_WIDGET (chooser));
}

static const gchar *
get_effective_language (CcRegionPanel *self)
{
        CcRegionPanelPrivate *priv = self->priv;
        if (priv->login)
                return priv->system_language;
        else
                return priv->language;
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
        CcRegionPanelPrivate *priv = self->priv;
        const gchar *region;

        if (priv->login)
                region = priv->system_region;
        else
                region = priv->region;

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
        GtkWidget *toplevel;
        GtkWidget *chooser;

        toplevel = gtk_widget_get_toplevel (GTK_WIDGET (self));
        chooser = cc_format_chooser_new (toplevel);
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
	CcRegionPanelPrivate *priv = self->priv;
        GError *error = NULL;
        gboolean allowed;

        allowed = g_permission_acquire_finish (priv->permission, res, &error);
        if (error) {
                if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
                        g_warning ("Failed to acquire permission: %s\n", error->message);
                g_error_free (error);
                return;
        }

        if (allowed) {
                switch (priv->op) {
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
                        move_selected_input (self, priv->op);
                        break;
                default:
                        g_warning ("Unknown privileged operation: %d\n", priv->op);
                        break;
                }
        }
}

static void
activate_language_row (CcRegionPanel *self,
                       GtkListBoxRow *row)
{
	CcRegionPanelPrivate *priv = self->priv;

        if (row == priv->language_row) {
                if (!priv->login || g_permission_get_allowed (priv->permission)) {
                        show_language_chooser (self);
                } else if (g_permission_get_can_acquire (priv->permission)) {
                        priv->op = CHOOSE_LANGUAGE;
                        g_permission_acquire_async (priv->permission,
                                                    NULL,
                                                    permission_acquired,
                                                    self);
                }
        } else if (row == priv->formats_row) {
                if (!priv->login || g_permission_get_allowed (priv->permission)) {
                        show_region_chooser (self);
                } else if (g_permission_get_can_acquire (priv->permission)) {
                        priv->op = CHOOSE_REGION;
                        g_permission_acquire_async (priv->permission,
                                                    NULL,
                                                    permission_acquired,
                                                    self);
                }
        }
}

static void
update_region_label (CcRegionPanel *self)
{
        CcRegionPanelPrivate *priv = self->priv;
        const gchar *region = get_effective_region (self);
        gchar *name = NULL;

        if (region)
                name = gnome_get_country_from_locale (region, region);

        if (!name)
                name = gnome_get_country_from_locale (DEFAULT_LOCALE, DEFAULT_LOCALE);

        gtk_label_set_label (GTK_LABEL (priv->formats_label), name);
        g_free (name);
}

static void
update_region_from_setting (CcRegionPanel *self)
{
        CcRegionPanelPrivate *priv = self->priv;

        g_free (priv->region);
        priv->region = g_settings_get_string (priv->locale_settings, KEY_REGION);
        update_region_label (self);
}

static void
update_language_label (CcRegionPanel *self)
{
	CcRegionPanelPrivate *priv = self->priv;
        const gchar *language = get_effective_language (self);
        gchar *name = NULL;

        if (language)
                name = gnome_get_language_from_locale (language, language);

        if (!name)
                name = gnome_get_language_from_locale (DEFAULT_LOCALE, DEFAULT_LOCALE);

        gtk_label_set_label (GTK_LABEL (priv->language_label), name);
        g_free (name);

        /* Formats will change too if not explicitly set. */
        update_region_label (self);
}

static void
update_language_from_user (CcRegionPanel *self)
{
	CcRegionPanelPrivate *priv = self->priv;
        const gchar *language = NULL;

        if (act_user_is_loaded (priv->user))
                language = act_user_get_language (priv->user);

        if (language == NULL || *language == '\0')
                language = setlocale (LC_MESSAGES, NULL);

        g_free (priv->language);
        priv->language = g_strdup (language);
        update_language_label (self);
}

static void
setup_language_section (CcRegionPanel *self)
{
	CcRegionPanelPrivate *priv = self->priv;
        GtkWidget *widget;

        priv->user = act_user_manager_get_user_by_id (priv->user_manager, getuid ());
        g_signal_connect_swapped (priv->user, "notify::language",
                                  G_CALLBACK (update_language_from_user), self);
        g_signal_connect_swapped (priv->user, "notify::is-loaded",
                                  G_CALLBACK (update_language_from_user), self);

        priv->locale_settings = g_settings_new (GNOME_SYSTEM_LOCALE_DIR);
        g_signal_connect_swapped (priv->locale_settings, "changed::" KEY_REGION,
                                  G_CALLBACK (update_region_from_setting), self);

        priv->language_section = WID ("language_section");
        priv->language_row = GTK_LIST_BOX_ROW (WID ("language_row"));
        priv->language_label = WID ("language_label");
        priv->formats_row = GTK_LIST_BOX_ROW (WID ("formats_row"));
        priv->formats_label = WID ("formats_label");

        widget = WID ("language_list");
        gtk_list_box_set_selection_mode (GTK_LIST_BOX (widget),
                                         GTK_SELECTION_NONE);
        gtk_list_box_set_header_func (GTK_LIST_BOX (widget),
                                      cc_list_box_update_header_func,
                                      NULL, NULL);
        g_signal_connect_swapped (widget, "row-activated",
                                  G_CALLBACK (activate_language_row), self);

        update_language_from_user (self);
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
update_input_chooser (CcRegionPanel *self)
{
        CcRegionPanelPrivate *priv = self->priv;
        GtkWidget *chooser;

        chooser = g_object_get_data (G_OBJECT (self), "input-chooser");
        if (!chooser)
                return;

        cc_input_chooser_set_ibus_engines (chooser, priv->ibus_engines);
}

static void
fetch_ibus_engines_result (GObject       *object,
                           GAsyncResult  *result,
                           CcRegionPanel *self)
{
        CcRegionPanelPrivate *priv;
        GList *list, *l;
        GError *error;

        error = NULL;
        list = ibus_bus_list_engines_async_finish (IBUS_BUS (object), result, &error);
        if (!list && error) {
                if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
                        g_warning ("Couldn't finish IBus request: %s", error->message);
                g_error_free (error);
                return;
        }

        priv = self->priv;
        g_clear_object (&priv->ibus_cancellable);

        /* Maps engine ids to engine description objects */
        priv->ibus_engines = g_hash_table_new_full (g_str_hash, g_str_equal, NULL, g_object_unref);

        for (l = list; l; l = l->next) {
                IBusEngineDesc *engine = l->data;
                const gchar *engine_id = ibus_engine_desc_get_name (engine);

                if (g_str_has_prefix (engine_id, "xkb:"))
                        g_object_unref (engine);
                else
                        g_hash_table_replace (priv->ibus_engines, (gpointer)engine_id, engine);
        }
        g_list_free (list);

        update_ibus_active_sources (self);
        update_input_chooser (self);
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

static void
remove_no_input_row (GtkContainer *list)
{
        GList *l;

        l = gtk_container_get_children (list);
        if (!l)
                return;
        if (l->next != NULL)
                goto out;
        if (g_strcmp0 (g_object_get_data (G_OBJECT (l->data), "type"), "none") == 0)
                gtk_container_remove (list, GTK_WIDGET (l->data));
out:
        g_list_free (l);
}

static GtkWidget *
add_input_row (CcRegionPanel   *self,
               const gchar     *type,
               const gchar     *id,
               const gchar     *name,
               GDesktopAppInfo *app_info)
{
        CcRegionPanelPrivate *priv = self->priv;
        GtkWidget *row;
        GtkWidget *box;
        GtkWidget *label;
        GtkWidget *image;

        remove_no_input_row (GTK_CONTAINER (priv->input_list));

        row = gtk_list_box_row_new ();
        box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
        gtk_container_add (GTK_CONTAINER (row), box);
        label = gtk_label_new (name);
        gtk_widget_set_halign (label, GTK_ALIGN_START);
        gtk_widget_set_margin_start (label, 20);
        gtk_widget_set_margin_end (label, 20);
        gtk_widget_set_margin_top (label, 12);
        gtk_widget_set_margin_bottom (label, 12);
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
        gtk_container_add (GTK_CONTAINER (self->priv->input_list), row);

        g_object_set_data (G_OBJECT (row), "label", label);
        g_object_set_data (G_OBJECT (row), "type", (gpointer)type);
        g_object_set_data_full (G_OBJECT (row), "id", g_strdup (id), g_free);
        if (app_info) {
                g_object_set_data_full (G_OBJECT (row), "app-info", g_object_ref (app_info), g_object_unref);
        }

        cc_list_box_adjust_scrolling (GTK_LIST_BOX (priv->input_list));

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
        CcRegionPanelPrivate *priv = self->priv;
        GVariantIter iter;
        const gchar *type;
        const gchar *id;
        const gchar *name;
        gchar *display_name;
        GDesktopAppInfo *app_info;

        if (g_variant_n_children (sources) < 1) {
                add_no_input_row (self);
                return;
        }

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
}

static void
add_input_sources_from_settings (CcRegionPanel *self)
{
	CcRegionPanelPrivate *priv = self->priv;
        GVariant *sources;
        sources = g_settings_get_value (priv->input_settings, "sources");
        add_input_sources (self, sources);
        g_variant_unref (sources);
}

static void
clear_input_sources (CcRegionPanel *self)
{
	CcRegionPanelPrivate *priv = self->priv;
        GList *list, *l;
        list = gtk_container_get_children (GTK_CONTAINER (priv->input_list));
        for (l = list; l; l = l->next) {
                gtk_container_remove (GTK_CONTAINER (priv->input_list), GTK_WIDGET (l->data));
        }
        g_list_free (list);

        cc_list_box_adjust_scrolling (GTK_LIST_BOX (priv->input_list));
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
        gtk_container_foreach (GTK_CONTAINER (self->priv->input_list),
                               select_by_id, (gpointer)id);
}

static void
input_sources_changed (GSettings     *settings,
                       const gchar   *key,
                       CcRegionPanel *self)
{
	CcRegionPanelPrivate *priv = self->priv;
        GtkListBoxRow *selected;
        gchar *id = NULL;

        selected = gtk_list_box_get_selected_row (GTK_LIST_BOX (priv->input_list));
        if (selected)
                id = g_strdup (g_object_get_data (G_OBJECT (selected), "id"));
        clear_input_sources (self);
        add_input_sources_from_settings (self);
        if (id) {
                select_input (self, id);
                g_free (id);
        }
}


static void
update_buttons (CcRegionPanel *self)
{
	CcRegionPanelPrivate *priv = self->priv;
        GtkListBoxRow *selected;
        GList *children;
        guint n_rows;

        children = gtk_container_get_children (GTK_CONTAINER (priv->input_list));
        n_rows = g_list_length (children);
        g_list_free (children);

        selected = gtk_list_box_get_selected_row (GTK_LIST_BOX (priv->input_list));
        if (selected == NULL) {
                gtk_widget_set_visible (priv->show_config, FALSE);
                gtk_widget_set_sensitive (priv->remove_input, FALSE);
                gtk_widget_set_sensitive (priv->show_layout, FALSE);
                gtk_widget_set_sensitive (priv->move_up_input, FALSE);
                gtk_widget_set_sensitive (priv->move_down_input, FALSE);
        } else {
                GDesktopAppInfo *app_info;

                app_info = (GDesktopAppInfo *)g_object_get_data (G_OBJECT (selected), "app-info");

                gtk_widget_set_visible (priv->show_config, app_info != NULL);
                gtk_widget_set_sensitive (priv->show_layout, TRUE);
                gtk_widget_set_sensitive (priv->remove_input, n_rows > 1);
                gtk_widget_set_sensitive (priv->move_up_input, gtk_list_box_row_get_index (selected) > 0);
                gtk_widget_set_sensitive (priv->move_down_input, gtk_list_box_row_get_index (selected) < n_rows - 1);
        }

        gtk_widget_set_visible (priv->options_button,
                                n_rows > 1 && !priv->login);
}

static void
set_input_settings (CcRegionPanel *self)
{
	CcRegionPanelPrivate *priv = self->priv;
        const gchar *type;
        const gchar *id;
        GVariantBuilder builder;
        GList *list, *l;

        g_variant_builder_init (&builder, G_VARIANT_TYPE ("a(ss)"));
        list = gtk_container_get_children (GTK_CONTAINER (priv->input_list));
        for (l = list; l; l = l->next) {
                type = (const gchar *)g_object_get_data (G_OBJECT (l->data), "type");
                id = (const gchar *)g_object_get_data (G_OBJECT (l->data), "id");
                g_variant_builder_add (&builder, "(ss)", type, id);
        }
        g_list_free (list);

        g_settings_set_value (priv->input_settings, KEY_INPUT_SOURCES, g_variant_builder_end (&builder));
        g_settings_apply (priv->input_settings);
}

static void set_localed_input (CcRegionPanel *self);

static void
update_input (CcRegionPanel *self)
{
	CcRegionPanelPrivate *priv = self->priv;

        if (priv->login) {
                set_localed_input (self);
        } else {
                set_input_settings (self);
                if (priv->login_auto_apply)
                        set_localed_input (self);
        }
}

static gboolean
input_source_already_added (CcRegionPanel *self,
                            const gchar   *id)
{
        CcRegionPanelPrivate *priv = self->priv;
        GList *list, *l;
        gboolean retval = FALSE;

        list = gtk_container_get_children (GTK_CONTAINER (priv->input_list));
        for (l = list; l; l = l->next)
                if (g_str_equal (id, (const gchar *) g_object_get_data (G_OBJECT (l->data), "id"))) {
                        retval = TRUE;
                        break;
                }
        g_list_free (list);

        return retval;
}

static void
run_input_chooser (CcRegionPanel *self, GtkWidget *chooser)
{
        gchar *type;
        gchar *id;
        gchar *name;
        GDesktopAppInfo *app_info = NULL;

        if (gtk_dialog_run (GTK_DIALOG (chooser)) == GTK_RESPONSE_OK) {
                if (cc_input_chooser_get_selected (chooser, &type, &id, &name) &&
                    !input_source_already_added (self, id)) {
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
                        update_buttons (self);
                        update_input (self);

                        g_free (id);
                        g_free (name);
                        g_clear_object (&app_info);
                }
        }
        gtk_widget_hide(chooser);
}

static void
show_input_chooser (CcRegionPanel *self)
{
	CcRegionPanelPrivate *priv = self->priv;
        GtkWidget *chooser;
        GtkWidget *toplevel;

        chooser = g_object_get_data (G_OBJECT (self), "input-chooser");

        if (!chooser) {
                toplevel = gtk_widget_get_toplevel (GTK_WIDGET (self));
                chooser = cc_input_chooser_new (GTK_WINDOW (toplevel),
                                                priv->login,
                                                priv->xkb_info,
#ifdef HAVE_IBUS
                                                priv->ibus_engines
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
	CcRegionPanelPrivate *priv = self->priv;

        if (!priv->login) {
                show_input_chooser (self);
        } else if (g_permission_get_allowed (priv->permission)) {
                show_input_chooser (self);
        } else if (g_permission_get_can_acquire (priv->permission)) {
                priv->op = ADD_INPUT;
                g_permission_acquire_async (priv->permission,
                                            NULL,
                                            permission_acquired,
                                            self);
        }
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
do_remove_selected_input (CcRegionPanel *self)
{
	CcRegionPanelPrivate *priv = self->priv;
        GtkListBoxRow *selected;
        GtkWidget *sibling;

        selected = gtk_list_box_get_selected_row (GTK_LIST_BOX (priv->input_list));
        if (selected == NULL)
                return;

        sibling = find_sibling (GTK_CONTAINER (priv->input_list), GTK_WIDGET (selected));
        gtk_container_remove (GTK_CONTAINER (priv->input_list), GTK_WIDGET (selected));
        gtk_list_box_select_row (GTK_LIST_BOX (priv->input_list), GTK_LIST_BOX_ROW (sibling));

        cc_list_box_adjust_scrolling (GTK_LIST_BOX (priv->input_list));

        update_buttons (self);
        update_input (self);
}

static void
remove_selected_input (CcRegionPanel *self)
{
	CcRegionPanelPrivate *priv = self->priv;

        if (!priv->login) {
                do_remove_selected_input (self);
        } else if (g_permission_get_allowed (priv->permission)) {
                do_remove_selected_input (self);
        } else if (g_permission_get_can_acquire (priv->permission)) {
                priv->op = REMOVE_INPUT;
                g_permission_acquire_async (priv->permission,
                                            NULL,
                                            permission_acquired,
                                            self);
        }
}

static void
do_move_selected_input (CcRegionPanel *self,
                        SystemOp       op)
{
	CcRegionPanelPrivate *priv = self->priv;
        GtkListBoxRow *selected;
        gint idx;

        g_assert (op == MOVE_UP_INPUT || op == MOVE_DOWN_INPUT);

        selected = gtk_list_box_get_selected_row (GTK_LIST_BOX (priv->input_list));
        g_assert (selected);

        idx = gtk_list_box_row_get_index (selected);
        if (op == MOVE_UP_INPUT)
                idx -= 1;
        else
                idx += 1;

        gtk_list_box_unselect_row (GTK_LIST_BOX (priv->input_list), selected);

        g_object_ref (selected);
        gtk_container_remove (GTK_CONTAINER (priv->input_list), GTK_WIDGET (selected));
        gtk_list_box_insert (GTK_LIST_BOX (priv->input_list), GTK_WIDGET (selected), idx);
        g_object_unref (selected);

        gtk_list_box_select_row (GTK_LIST_BOX (priv->input_list), selected);

        cc_list_box_adjust_scrolling (GTK_LIST_BOX (priv->input_list));

        update_buttons (self);
        update_input (self);
}

static void
move_selected_input (CcRegionPanel *self,
                     SystemOp       op)
{
	CcRegionPanelPrivate *priv = self->priv;

        if (!priv->login) {
                do_move_selected_input (self, op);
        } else if (g_permission_get_allowed (priv->permission)) {
                do_move_selected_input (self, op);
        } else if (g_permission_get_can_acquire (priv->permission)) {
                priv->op = op;
                g_permission_acquire_async (priv->permission,
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
	CcRegionPanelPrivate *priv = self->priv;
        GtkListBoxRow *selected;
        GdkAppLaunchContext *ctx;
        GDesktopAppInfo *app_info;
        const gchar *id;
        GError *error = NULL;

        selected = gtk_list_box_get_selected_row (GTK_LIST_BOX (priv->input_list));
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
        GtkListBoxRow *selected;
        const gchar *type;
        const gchar *id;
        const gchar *layout;
        const gchar *variant;
        gchar *commandline;

        selected = gtk_list_box_get_selected_row (GTK_LIST_BOX (priv->input_list));
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

        priv->input_section = WID ("input_section");
        priv->options_button = WID ("input_options");
        priv->input_list = WID ("input_list");
        priv->add_input = WID ("input_source_add");
        priv->remove_input = WID ("input_source_remove");
        priv->move_up_input = WID ("input_source_up");
        priv->move_down_input = WID ("input_source_down");
        priv->show_config = WID ("input_source_config");
        priv->show_layout = WID ("input_source_layout");

        g_signal_connect_swapped (priv->options_button, "clicked",
                                  G_CALLBACK (show_input_options), self);
        g_signal_connect_swapped (priv->add_input, "clicked",
                                  G_CALLBACK (add_input), self);
        g_signal_connect_swapped (priv->remove_input, "clicked",
                                  G_CALLBACK (remove_selected_input), self);
        g_signal_connect_swapped (priv->move_up_input, "clicked",
                                  G_CALLBACK (move_selected_input_up), self);
        g_signal_connect_swapped (priv->move_down_input, "clicked",
                                  G_CALLBACK (move_selected_input_down), self);
        g_signal_connect_swapped (priv->show_config, "clicked",
                                  G_CALLBACK (show_selected_settings), self);
        g_signal_connect_swapped (priv->show_layout, "clicked",
                                  G_CALLBACK (show_selected_layout), self);

        cc_list_box_setup_scrolling (GTK_LIST_BOX (priv->input_list), 5);

        gtk_list_box_set_selection_mode (GTK_LIST_BOX (priv->input_list),
                                         GTK_SELECTION_SINGLE);
        gtk_list_box_set_header_func (GTK_LIST_BOX (priv->input_list),
                                      cc_list_box_update_header_func,
                                      NULL, NULL);
        g_signal_connect_object (priv->input_list, "row-selected",
                                 G_CALLBACK (update_buttons), self, G_CONNECT_SWAPPED);

        g_signal_connect (priv->input_settings, "changed::" KEY_INPUT_SOURCES,
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
	CcRegionPanelPrivate *priv = self->priv;
        GVariant *v;

        v = g_dbus_proxy_get_cached_property (proxy, "Locale");
        if (v) {
                const gchar **strv;
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
                g_free (priv->system_language);
                priv->system_language = g_strdup (messages);
                g_free (priv->system_region);
                priv->system_region = g_strdup (time);
                g_variant_unref (v);
                g_free (strv);

                update_language_label (self);
        }
}

static void
add_input_sources_from_localed (CcRegionPanel *self)
{
	CcRegionPanelPrivate *priv = self->priv;
        GVariant *v;
        const gchar *s;
        gchar **layouts = NULL;
        gchar **variants = NULL;
        gint i, n;

        if (!priv->localed)
                return;

        v = g_dbus_proxy_get_cached_property (priv->localed, "X11Layout");
        if (v) {
                s = g_variant_get_string (v, NULL);
                layouts = g_strsplit (s, ",", -1);
                g_variant_unref (v);
        }

        v = g_dbus_proxy_get_cached_property (priv->localed, "X11Variant");
        if (v) {
                s = g_variant_get_string (v, NULL);
                if (s && *s)
                        variants = g_strsplit (s, ",", -1);
                g_variant_unref (v);
        }

        if (variants && variants[0])
                n = MIN (g_strv_length (layouts), g_strv_length (variants));
        else if (layouts && layouts[0])
                n = g_strv_length (layouts);
        else
                n = 0;

        for (i = 0; i < n && layouts[i][0]; i++) {
                const gchar *name;
                gchar *id;

                if (variants && variants[i] && variants[i][0])
                        id = g_strdup_printf ("%s+%s", layouts[i], variants[i]);
                else
                        id = g_strdup (layouts[i]);

                gnome_xkb_info_get_layout_info (priv->xkb_info, id, &name, NULL, NULL, NULL);

                add_input_row (self, INPUT_SOURCE_TYPE_XKB, id, name ? name : id, NULL);

                g_free (id);
        }
        if (n == 0) {
                add_no_input_row (self);
        }

        g_strfreev (variants);
        g_strfreev (layouts);
}

static void
set_localed_locale (CcRegionPanel *self)
{
	CcRegionPanelPrivate *priv = self->priv;
        GVariantBuilder *b;
        gchar *s;

        b = g_variant_builder_new (G_VARIANT_TYPE ("as"));
        s = g_strconcat ("LANG=", priv->system_language, NULL);
        g_variant_builder_add (b, "s", s);
        g_free (s);

        if (priv->system_region != NULL &&
            g_strcmp0 (priv->system_language, priv->system_region) != 0) {
                s = g_strconcat ("LC_TIME=", priv->system_region, NULL);
                g_variant_builder_add (b, "s", s);
                g_free (s);
                s = g_strconcat ("LC_NUMERIC=", priv->system_region, NULL);
                g_variant_builder_add (b, "s", s);
                g_free (s);
                s = g_strconcat ("LC_MONETARY=", priv->system_region, NULL);
                g_variant_builder_add (b, "s", s);
                g_free (s);
                s = g_strconcat ("LC_MEASUREMENT=", priv->system_region, NULL);
                g_variant_builder_add (b, "s", s);
                g_free (s);
                s = g_strconcat ("LC_PAPER=", priv->system_region, NULL);
                g_variant_builder_add (b, "s", s);
                g_free (s);
        }
        g_dbus_proxy_call (priv->localed,
                           "SetLocale",
                           g_variant_new ("(asb)", b, TRUE),
                           G_DBUS_CALL_FLAGS_NONE,
                           -1, NULL, NULL, NULL);
        g_variant_builder_unref (b);
}

static void
set_localed_input (CcRegionPanel *self)
{
	CcRegionPanelPrivate *priv = self->priv;
        GString *layouts;
        GString *variants;
        const gchar *type, *id;
        GList *list, *li;
        const gchar *l, *v;

        layouts = g_string_new ("");
        variants = g_string_new ("");

        list = gtk_container_get_children (GTK_CONTAINER (priv->input_list));
        for (li = list; li; li = li->next) {
                type = (const gchar *)g_object_get_data (G_OBJECT (li->data), "type");
                id = (const gchar *)g_object_get_data (G_OBJECT (li->data), "id");
                if (g_str_equal (type, INPUT_SOURCE_TYPE_IBUS))
                        continue;

                if (gnome_xkb_info_get_layout_info (priv->xkb_info, id, NULL, NULL, &l, &v)) {
                        if (layouts->str[0]) {
                                g_string_append_c (layouts, ',');
                                g_string_append_c (variants, ',');
                        }
                        g_string_append (layouts, l);
                        g_string_append (variants, v);
                }
        }
        g_list_free (list);

        g_dbus_proxy_call (priv->localed,
                           "SetX11Keyboard",
                           g_variant_new ("(ssssbb)", layouts->str, "", variants->str, "", TRUE, TRUE),
                           G_DBUS_CALL_FLAGS_NONE,
                           -1, NULL, NULL, NULL);

        g_string_free (layouts, TRUE);
        g_string_free (variants, TRUE);
}

static void
localed_proxy_ready (GObject      *source,
                     GAsyncResult *res,
                     gpointer      data)
{
        CcRegionPanel *self = data;
        CcRegionPanelPrivate *priv;
        GDBusProxy *proxy;
        GError *error = NULL;

        proxy = g_dbus_proxy_new_finish (res, &error);

        if (!proxy) {
                if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
                        g_warning ("Failed to contact localed: %s\n", error->message);
                g_error_free (error);
                return;
        }

        priv = self->priv;
        priv->localed = proxy;

        gtk_widget_set_sensitive (priv->login_button, TRUE);

        g_signal_connect (priv->localed, "g-properties-changed",
                          G_CALLBACK (on_localed_properties_changed), self);
        on_localed_properties_changed (priv->localed, NULL, NULL, self);
}

static void
login_changed (CcRegionPanel *self)
{
	CcRegionPanelPrivate *priv = self->priv;
        gboolean can_acquire;

        priv->login = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (priv->login_button));
        gtk_widget_set_visible (priv->login_label, priv->login);

        can_acquire = priv->permission &&
                (g_permission_get_allowed (priv->permission) ||
                 g_permission_get_can_acquire (priv->permission));
        /* FIXME: insensitive doesn't look quite right for this */
        gtk_widget_set_sensitive (priv->language_section, !priv->login || can_acquire);
        gtk_widget_set_sensitive (priv->input_section, !priv->login || can_acquire);

        clear_input_sources (self);
        if (priv->login)
                add_input_sources_from_localed (self);
        else
                add_input_sources_from_settings (self);

        update_language_label (self);
        update_buttons (self);
}

static void
set_login_button_visibility (CcRegionPanel *self)
{
        CcRegionPanelPrivate *priv = self->priv;
        gboolean has_multiple_users;
        gboolean loaded;

        g_object_get (priv->user_manager, "is-loaded", &loaded, NULL);
        if (!loaded)
          return;

        g_object_get (priv->user_manager, "has-multiple-users", &has_multiple_users, NULL);

        priv->login_auto_apply = !has_multiple_users && g_permission_get_allowed (priv->permission);
        gtk_widget_set_visible (priv->login_button, !priv->login_auto_apply);

        g_signal_handlers_disconnect_by_func (priv->user_manager, set_login_button_visibility, self);
}

static void
setup_login_button (CcRegionPanel *self)
{
	CcRegionPanelPrivate *priv = self->priv;
        GDBusConnection *bus;
        gboolean loaded;
        GError *error = NULL;

        priv->permission = polkit_permission_new_sync ("org.freedesktop.locale1.set-locale", NULL, NULL, &error);
        if (priv->permission == NULL) {
                g_warning ("Could not get 'org.freedesktop.locale1.set-locale' permission: %s",
                           error->message);
                g_error_free (error);
                return;
        }

        bus = g_bus_get_sync (G_BUS_TYPE_SYSTEM, NULL, NULL);
        g_dbus_proxy_new (bus,
                          G_DBUS_PROXY_FLAGS_GET_INVALIDATED_PROPERTIES,
                          NULL,
                          "org.freedesktop.locale1",
                          "/org/freedesktop/locale1",
                          "org.freedesktop.locale1",
                          priv->cancellable,
                          (GAsyncReadyCallback) localed_proxy_ready,
                          self);
        g_object_unref (bus);

        priv->login_label = WID ("login-label");
        priv->login_button = gtk_toggle_button_new_with_mnemonic (_("Login _Screen"));
        gtk_style_context_add_class (gtk_widget_get_style_context (priv->login_button),
                                     "text-button");
        gtk_widget_set_valign (priv->login_button, GTK_ALIGN_CENTER);
        gtk_widget_set_visible (priv->login_button, FALSE);
        gtk_widget_set_sensitive (priv->login_button, FALSE);
        g_signal_connect_swapped (priv->login_button, "notify::active",
                                  G_CALLBACK (login_changed), self);

        g_object_get (priv->user_manager, "is-loaded", &loaded, NULL);
        if (loaded)
                set_login_button_visibility (self);
        else
                g_signal_connect_swapped (priv->user_manager, "notify::is-loaded",
                                          G_CALLBACK (set_login_button_visibility), self);
}

static void
session_proxy_ready (GObject      *source,
                     GAsyncResult *res,
                     gpointer      data)
{
        CcRegionPanel *self = data;
        GDBusProxy *proxy;
        GError *error = NULL;

        proxy = g_dbus_proxy_new_for_bus_finish (res, &error);

        if (!proxy) {
                if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
                        g_warning ("Failed to contact gnome-session: %s\n", error->message);
                g_error_free (error);
                return;
        }

        self->priv->session = proxy;
}

static void
cc_region_panel_init (CcRegionPanel *self)
{
	CcRegionPanelPrivate *priv;
        GtkWidget *button;
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

        priv->user_manager = act_user_manager_get_default ();

        priv->cancellable = g_cancellable_new ();

        g_dbus_proxy_new_for_bus (G_BUS_TYPE_SESSION,
                                  G_DBUS_PROXY_FLAGS_NONE,
                                  NULL,
                                  "org.gnome.SessionManager",
                                  "/org/gnome/SessionManager",
                                  "org.gnome.SessionManager",
                                  priv->cancellable,
                                  session_proxy_ready,
                                  self);

        priv->notification = GTK_WIDGET (gtk_builder_get_object (priv->builder, "notification"));

        button = GTK_WIDGET (gtk_builder_get_object (priv->builder, "restart-button"));
        g_signal_connect_swapped (button, "clicked", G_CALLBACK (restart_now), self);

        button = GTK_WIDGET (gtk_builder_get_object (priv->builder, "dismiss-button"));
        g_signal_connect_swapped (button, "clicked", G_CALLBACK (dismiss_notification), self);

        setup_login_button (self);
        setup_language_section (self);
        setup_input_section (self);

        priv->overlay = GTK_WIDGET (gtk_builder_get_object (priv->builder, "overlay"));
	gtk_container_add (GTK_CONTAINER (self), priv->overlay);
}
