/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */
/*
 * Copyright (c) 2012 Giovanni Campagna <scampa.giovanni@gmail.com>
 *
 * The Control Center is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 *
 * The Control Center is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with the Control Center; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 * Author: Thomas Wood <thos@gnome.org>
 */

#include <config.h>

#include <string.h>
#include <gio/gdesktopappinfo.h>

#include "cc-panel.h"
#include "cc-panel-loader.h"

#ifndef CC_PANEL_LOADER_NO_GTYPES

/* Extension points */
extern GType cc_background_panel_get_type (void);
#ifdef BUILD_BLUETOOTH
extern GType cc_bluetooth_panel_get_type (void);
#endif /* BUILD_BLUETOOTH */
extern GType cc_color_panel_get_type (void);
extern GType cc_date_time_panel_get_type (void);
extern GType cc_display_panel_get_type (void);
extern GType cc_info_overview_panel_get_type (void);
extern GType cc_info_default_apps_panel_get_type (void);
extern GType cc_info_removable_media_panel_get_type (void);
extern GType cc_keyboard_panel_get_type (void);
extern GType cc_mouse_panel_get_type (void);
#ifdef BUILD_NETWORK
extern GType cc_network_panel_get_type (void);
extern GType cc_wifi_panel_get_type (void);
#endif /* BUILD_NETWORK */
extern GType cc_notifications_panel_get_type (void);
extern GType cc_goa_panel_get_type (void);
extern GType cc_power_panel_get_type (void);
extern GType cc_printers_panel_get_type (void);
extern GType cc_privacy_panel_get_type (void);
extern GType cc_region_panel_get_type (void);
extern GType cc_search_panel_get_type (void);
extern GType cc_sharing_panel_get_type (void);
extern GType cc_sound_panel_get_type (void);
#ifdef BUILD_THUNDERBOLT
extern GType cc_bolt_panel_get_type (void);
#endif /* BUILD_THUNDERBOLT */
extern GType cc_ua_panel_get_type (void);
extern GType cc_user_panel_get_type (void);
#ifdef BUILD_WACOM
extern GType cc_wacom_panel_get_type (void);
#endif /* BUILD_WACOM */

#define PANEL_TYPE(name, get_type, init_func) { name, get_type, init_func }

#else /* CC_PANEL_LOADER_NO_GTYPES */

#define PANEL_TYPE(name, get_type, init_func) { name }

#endif

static struct {
  const char *name;
#ifndef CC_PANEL_LOADER_NO_GTYPES
  GType (*get_type)(void);
  CcPanelStaticInitFunc static_init_func;
#endif
} all_panels[] = {
  PANEL_TYPE("background",       cc_background_panel_get_type,           NULL),
#ifdef BUILD_BLUETOOTH
  PANEL_TYPE("bluetooth",        cc_bluetooth_panel_get_type,            NULL),
#endif
  PANEL_TYPE("color",            cc_color_panel_get_type,                NULL),
  PANEL_TYPE("datetime",         cc_date_time_panel_get_type,            NULL),
  PANEL_TYPE("display",          cc_display_panel_get_type,              NULL),
  PANEL_TYPE("info-overview",    cc_info_overview_panel_get_type,        NULL),
  PANEL_TYPE("default-apps",     cc_info_default_apps_panel_get_type,    NULL),
  PANEL_TYPE("removable-media",  cc_info_removable_media_panel_get_type, NULL),
  PANEL_TYPE("keyboard",         cc_keyboard_panel_get_type,             NULL),
  PANEL_TYPE("mouse",            cc_mouse_panel_get_type,                NULL),
#ifdef BUILD_NETWORK
  PANEL_TYPE("network",          cc_network_panel_get_type,              NULL),
  PANEL_TYPE("wifi",             cc_wifi_panel_get_type,                 NULL),
#endif
  PANEL_TYPE("notifications",    cc_notifications_panel_get_type,        NULL),
  PANEL_TYPE("online-accounts",  cc_goa_panel_get_type,                  NULL),
  PANEL_TYPE("power",            cc_power_panel_get_type,                NULL),
  PANEL_TYPE("printers",         cc_printers_panel_get_type,             NULL),
  PANEL_TYPE("privacy",          cc_privacy_panel_get_type,              NULL),
  PANEL_TYPE("region",           cc_region_panel_get_type,               NULL),
  PANEL_TYPE("search",           cc_search_panel_get_type,               NULL),
  PANEL_TYPE("sharing",          cc_sharing_panel_get_type,              NULL),
  PANEL_TYPE("sound",            cc_sound_panel_get_type,                NULL),
#ifdef BUILD_THUNDERBOLT
  PANEL_TYPE("thunderbolt",      cc_bolt_panel_get_type,                 NULL),
#endif
  PANEL_TYPE("universal-access", cc_ua_panel_get_type,                   NULL),
  PANEL_TYPE("user-accounts",    cc_user_panel_get_type,                 NULL),
#ifdef BUILD_WACOM
  PANEL_TYPE("wacom",            cc_wacom_panel_get_type,                NULL),
#endif
};

GList *
cc_panel_loader_get_panels (void)
{
  GList *l = NULL;
  guint i;

  for (i = 0; i < G_N_ELEMENTS (all_panels); i++)
    l = g_list_prepend (l, (gpointer) all_panels[i].name);

  return g_list_reverse (l);
}

static int
parse_categories (GDesktopAppInfo *app)
{
  g_auto(GStrv) split = NULL;
  const gchar *categories;
  gint retval;

  categories = g_desktop_app_info_get_categories (app);
  split = g_strsplit (categories, ";", -1);

  retval = -1;

#define const_strv(s) ((const gchar* const*) s)

  if (g_strv_contains (const_strv (split), "X-GNOME-ConnectivitySettings"))
    retval = CC_CATEGORY_CONNECTIVITY;
  else if (g_strv_contains (const_strv (split), "X-GNOME-PersonalizationSettings"))
    retval = CC_CATEGORY_PERSONALIZATION;
  else if (g_strv_contains (const_strv (split), "X-GNOME-AccountSettings"))
    retval = CC_CATEGORY_ACCOUNT;
  else if (g_strv_contains (const_strv (split), "X-GNOME-DevicesSettings"))
    retval = CC_CATEGORY_DEVICES;
  else if (g_strv_contains (const_strv (split), "X-GNOME-DetailsSettings"))
    retval = CC_CATEGORY_DETAILS;
  else if (g_strv_contains (const_strv (split), "HardwareSettings"))
    retval = CC_CATEGORY_HARDWARE;

#undef const_strv

  if (retval < 0)
    {
      g_warning ("Invalid categories %s for panel %s",
                 categories, g_app_info_get_id (G_APP_INFO (app)));
    }

  return retval;
}

void
cc_panel_loader_fill_model (CcShellModel *model)
{
  guint i;

  for (i = 0; i < G_N_ELEMENTS (all_panels); i++)
    {
      g_autoptr (GDesktopAppInfo) app;
      g_autofree gchar *desktop_name = NULL;
      gint category;

      desktop_name = g_strconcat ("gnome-", all_panels[i].name, "-panel.desktop", NULL);
      app = g_desktop_app_info_new (desktop_name);

      if (!app)
        {
          g_warning ("Ignoring broken panel %s (missing desktop file)", all_panels[i].name);
          continue;
        }

      category = parse_categories (app);
      if (G_UNLIKELY (category < 0))
        continue;

      /* Consult OnlyShowIn/NotShowIn for desktop environments */
      if (!g_desktop_app_info_get_show_in (app, NULL))
        continue;

      cc_shell_model_add_item (model, category, G_APP_INFO (app), all_panels[i].name);
    }

  /* If there's an static init function, execute it after adding all panels to
   * the model. This will allow the panels to show or hide themselves without
   * having an instance running.
   */
#ifndef CC_PANEL_LOADER_NO_GTYPES
  for (i = 0; i < G_N_ELEMENTS (all_panels); i++)
    {
      if (all_panels[i].static_init_func)
        all_panels[i].static_init_func ();
    }
#endif
}

#ifndef CC_PANEL_LOADER_NO_GTYPES

static GHashTable *panel_types;

static void
ensure_panel_types (void)
{
  int i;

  if (G_LIKELY (panel_types != NULL))
    return;

  panel_types = g_hash_table_new (g_str_hash, g_str_equal);
  for (i = 0; i < G_N_ELEMENTS (all_panels); i++)
    g_hash_table_insert (panel_types, (char*)all_panels[i].name, all_panels[i].get_type);
}

CcPanel *
cc_panel_loader_load_by_name (CcShell     *shell,
                              const char  *name,
                              GVariant    *parameters)
{
  GType (*get_type) (void);

  ensure_panel_types ();

  get_type = g_hash_table_lookup (panel_types, name);
  g_return_val_if_fail (get_type != NULL, NULL);

  return g_object_new (get_type (),
                       "shell", shell,
                       "parameters", parameters,
                       NULL);
}

#endif /* CC_PANEL_LOADER_NO_GTYPES */
