/*
 * Copyright 2026 Adrian Vovk
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#undef G_LOG_DOMAIN
#define G_LOG_DOMAIN "cc-factory-reset-page"

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <act/act.h>
#include <glib/gi18n.h>
#ifdef HAVE_MALCONTENT
#include <libmalcontent/malcontent.h>
#endif

#include "cc-factory-reset-page.h"
#include "user-utils.h"

struct _CcFactoryResetPage {
  AdwNavigationPage parent_instance;

  AdwPreferencesGroup *users_group;
  AdwPreferencesGroup *apps_group;

  AdwActionRow *users_row;
  AdwActionRow *factory_reset_row;

  GDBusProxy *logind_proxy;
};

G_DEFINE_TYPE (CcFactoryResetPage, cc_factory_reset_page, ADW_TYPE_NAVIGATION_PAGE)

static void
factory_reset_cb (GObject      *source,
                  GAsyncResult *result,
                  gpointer      user_data)
{
  g_autoptr (GError) error = NULL;
  g_autoptr (GVariant) result = NULL;

  result = g_dbus_proxy_call_finish(G_DBUS_PROXY (source), result, &error);
  if (result)
    return; /* We're done! Nothing else to do */

  // TODO: Parse error

  if (!result) {

  }
}

static void
on_confirm_clicked (CcFactoryResetPage *self)
{

  g_dbus_proxy_call (self->logind_proxy,
                     "FactoryReset",
                     g_variant_new ("(t)", 0),
                     G_DBUS_CALL_FLAGS_ALLOW_INTERACTIVE_AUTHORIZATION,
                     -1, NULL, NULL, NULL);

  // TODO: Notification in the event of an error (inhibited)? Or maybe, we should
  //       show a different dialog asking to try again later? IDK
}

static void
cc_factory_reset_page_dispose (GObject *object)
{
  CcFactoryResetPage *self = (CcFactoryResetPage *) object;

  g_clear_object (&self->logind_proxy);

  G_OBJECT_CLASS (cc_factory_reset_page_parent_class)->dispose (object);
}

static void
cc_factory_reset_page_class_init (CcFactoryResetPageClass * klass)
{
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);
  GObjectClass   *object_class = G_OBJECT_CLASS (klass);

  object_class->dispose = cc_factory_reset_page_dispose;

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/control-center/system/factory-reset/cc-factory-reset-page.ui");

  gtk_widget_class_bind_template_child (widget_class, CcFactoryResetPage, users_group);
  gtk_widget_class_bind_template_child (widget_class, CcFactoryResetPage, apps_group);

  gtk_widget_class_bind_template_callback (widget_class, on_confirm_clicked);
}

static gboolean
setup_logind_proxy (CcFactoryResetPage *self)
{
  g_autoptr (GError) error = NULL;

  self->logind_proxy = g_dbus_proxy_new_for_bus_sync(G_BUS_TYPE_SYSTEM,
                                                     G_DBUS_PROXY_FLAGS_DO_NOT_LOAD_PROPERTIES,
                                                     NULL,
                                                     "org.freedesktop.login1",
                                                     "/org/freedesktop/login1",
                                                     "org.freedesktop.login1.Manager",
                                                     NULL,
                                                     &error);
  if (!self->logind_proxy) {
    g_warning ("Failed to obtain logind proxy: %s", error->message);
    // TODO: Mark self as unavailable somehow so that it gets hidden
    return FALSE;
  }

  // TODO: Monitor inhibitors, and grey out button / maybe hide the option

  return TRUE;
}

static gint
compare_users (gconstpointer a,
               gconstpointer b,
               gpointer      user_data)
{
  ActUser *item1 = ACT_USER ((gpointer) a);
  ActUser *item2 = ACT_USER ((gpointer) b);
  g_autofree gchar *key1 = NULL;
  g_autofree gchar *key2 = NULL;

  /* Show the current user at the top of the list */
  if (act_user_get_uid (item1) == getuid ()) {
    return -G_MAXINT32;
  } else if (act_user_get_uid (item2) == getuid ()) {
    return G_MAXINT32;
  }

  key1 = g_utf8_collate_key(get_real_or_user_name (item1), -1);
  key2 = g_utf8_collate_key(get_real_or_user_name (item2), -1);

  return strcmp (key1, key2);
}

static GtkWidget *
create_user_row (gpointer item,
                 gpointer user_data)
{
  ActUser *user = ACT_USER (item);
  GtkWidget *row, *avatar;

  row = adw_action_row_new ();
  adw_preferences_row_set_title(ADW_PREFERENCES_ROW (row),
                                get_real_or_user_name (user));

  avatar = adw_avatar_new (32, NULL, TRUE);
  setup_avatar_for_user(ADW_AVATAR (avatar), user);
  adw_action_row_add_prefix(ADW_ACTION_ROW (row), avatar);

  return row;
}

static void
on_users_loaded (ActUserManager *manager,
                 GParamSpec     *pspec,
                 GListStore     *model)
{
  g_autoptr (GSList) user_list = NULL;
  GSList *l;

  /* Handle this only once */
  g_signal_handlers_disconnect_by_func (manager, on_users_loaded, model);
  g_object_unref (manager);

  user_list = act_user_manager_list_users (manager);
  for (l = user_list; l; l = l->next) {
    ActUser *user = ACT_USER (l->data);

    if (act_user_is_system_account (user))
      continue;

    g_list_store_insert_sorted (model, user, compare_users, NULL);
  }
}

static void
populate_users (CcFactoryResetPage *self)
{
  g_autoptr (GListStore) model = NULL;
  g_autoptr (ActUserManager) manager = NULL;
  gboolean already_loaded = FALSE;

  model = g_list_store_new (ACT_TYPE_USER);

  manager = act_user_manager_get_default ();
  g_signal_connect_object(manager,
                          "notify::is-loaded",
                          G_CALLBACK (on_users_loaded),
                          model,
                          G_CONNECT_DEFAULT);
  g_object_ref (manager); /* Keep the manager around until the callback */

  g_object_get (manager, "is-loaded", &already_loaded, NULL);
  if (already_loaded)
    on_users_loaded (manager, NULL, model);

  adw_preferences_group_bind_model (self->users_group,
                                    G_LIST_MODEL (model),
                                    create_user_row,
                                    NULL, NULL);
}

#ifdef HAVE_MALCONTENT
static MctAppFilter *
get_malcontent_app_filter (void)
{
  g_autoptr (GError) error = NULL;
  g_autoptr (GDBusConnection) bus = NULL;
  g_autoptr (MctManager) manager = NULL;
  g_autoptr (MctAppFilter) filter = NULL;

  bus = g_bus_get_sync (G_BUS_TYPE_SYSTEM, NULL, &error);
  if (!bus)
    {
      g_warning ("Error getting system bus: %s", error->message);
      return NULL;
    }

  manager = mct_manager_new (bus);

  filter = mct_manager_get_app_filter (manager, getuid (),
                                       MCT_MANAGER_GET_VALUE_FLAGS_NONE,
                                       NULL, &error);
  if (!filter)
    {
      g_warning ("Failed to get malcontent app filter: %s", error->message);
      return NULL;
    }

  return g_steal_pointer(&filter);
}
#endif

static gint
compare_apps (gconstpointer  a,
              gconstpointer  b,
              gpointer       data)
{
  GAppInfo *item1 = G_APP_INFO ((gpointer) a);
  GAppInfo *item2 = G_APP_INFO ((gpointer) b);

  g_autofree gchar *key1 = NULL;
  g_autofree gchar *key2 = NULL;

  key1 = g_utf8_casefold (g_app_info_get_display_name (item1), -1);
  key2 = g_utf8_casefold (g_app_info_get_display_name (item2), -1);

  return g_utf8_collate (key1, key2);
}

static GtkWidget *
create_app_row (gpointer item,
                gpointer user_data)
{
  GAppInfo *info = G_APP_INFO (item);
  g_autoptr (AdwActionRow) row = NULL;
  g_autoptr (GIcon) gicon = NULL;
  GtkImage *icon_widget;

  row = ADW_ACTION_ROW (adw_action_row_new ());

  adw_preferences_row_set_title (ADW_PREFERENCES_ROW (row),
                                 g_app_info_get_display_name (info));

  gicon = g_app_info_get_icon (info);
  if (gicon)
    g_object_ref (gicon);
  else
    gicon = g_themed_icon_new("application-x-executable");

  icon_widget = GTK_IMAGE (g_object_new (GTK_TYPE_IMAGE,
                                         "accessible-role", GTK_ACCESSIBLE_ROLE_PRESENTATION,
                                         "gicon", gicon,
                                         "icon-size", GTK_ICON_SIZE_LARGE,
                                         NULL));
  gtk_widget_add_css_class (GTK_WIDGET (icon_widget), "lowres-icon");
  adw_action_row_add_prefix(row, GTK_WIDGET (icon_widget));

  return GTK_WIDGET (g_steal_pointer (&row));
}

static void
populate_apps (CcFactoryResetPage *self)
{
#ifdef HAVE_MALCONTENT
  g_autoptr (MctAppFilter) app_filter = NULL;
#endif
  g_autoptr (GListStore) model = NULL;
  g_autolist (GAppInfo) infos = NULL;
  GList *l;

#ifdef HAVE_MALCONTENT
  app_filter = get_malcontent_app_filter ();
#endif

  model = g_list_store_new (G_TYPE_APP_INFO);

  infos = g_app_info_get_all ();
  for (l = infos; l; l = l->next)
    {
      GAppInfo *info = l->data;

      if (!g_app_info_should_show (info))
        continue;

#ifdef HAVE_MALCONTENT
      if (!mct_app_filter_is_appinfo_allowed (app_filter, info))
        continue;
#endif

      g_list_store_insert_sorted (model, info, compare_apps, NULL);
    }

  adw_preferences_group_bind_model(self->apps_group,
                                   G_LIST_MODEL (model),
                                   create_app_row,
                                   NULL, NULL);
}

static void
cc_factory_reset_page_init (CcFactoryResetPage *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));

  if (!setup_logind_proxy (self))
    return;

  populate_users (self);
  populate_apps (self);
}
