/*
 * Copyright (c) 2009, 2010 Intel, Inc.
 * Copyright (c) 2010 Red Hat, Inc.
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

#include <string.h>

#include <gio/gdesktopappinfo.h>

#include "cc-shell-model.h"
#include "cc-util.h"

#define GNOME_SETTINGS_PANEL_ID_KEY "X-GNOME-Settings-Panel"
#define GNOME_SETTINGS_PANEL_CATEGORY GNOME_SETTINGS_PANEL_ID_KEY
#define GNOME_SETTINGS_PANEL_ID_KEYWORDS "Keywords"


G_DEFINE_TYPE (CcShellModel, cc_shell_model, GTK_TYPE_LIST_STORE)

static void
cc_shell_model_class_init (CcShellModelClass *klass)
{
}

static void
cc_shell_model_init (CcShellModel *self)
{
  GType types[] = {G_TYPE_STRING, G_TYPE_STRING, G_TYPE_APP_INFO, G_TYPE_STRING,
                   G_TYPE_UINT, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_ICON, G_TYPE_STRV};

  gtk_list_store_set_column_types (GTK_LIST_STORE (self),
                                   N_COLS, types);

  gtk_tree_sortable_set_sort_column_id (GTK_TREE_SORTABLE (self), COL_NAME,
                                        GTK_SORT_ASCENDING);
}

CcShellModel *
cc_shell_model_new (void)
{
  return g_object_new (CC_TYPE_SHELL_MODEL, NULL);
}

static char **
get_casefolded_keywords (GAppInfo *appinfo)
{
  const char * const * keywords;
  char **casefolded_keywords;
  int i, n;

  keywords = g_desktop_app_info_get_keywords (G_DESKTOP_APP_INFO (appinfo));
  n = keywords ? g_strv_length ((char**) keywords) : 0;
  casefolded_keywords = g_new (char*, n+1);

  for (i = 0; i < n; i++)
    casefolded_keywords[i] = cc_util_normalize_casefold_and_unaccent (keywords[i]);
  casefolded_keywords[n] = NULL;

  return casefolded_keywords;
}

void
cc_shell_model_add_item (CcShellModel    *model,
                         CcPanelCategory  category,
                         GAppInfo        *appinfo,
                         const char      *id)
{
  GIcon       *icon = g_app_info_get_icon (appinfo);
  const gchar *name = g_app_info_get_name (appinfo);
  const gchar *comment = g_app_info_get_description (appinfo);
  char **keywords;
  char *casefolded_name, *casefolded_description;

  casefolded_name = cc_util_normalize_casefold_and_unaccent (name);
  casefolded_description = cc_util_normalize_casefold_and_unaccent (comment);
  keywords = get_casefolded_keywords (appinfo);

  gtk_list_store_insert_with_values (GTK_LIST_STORE (model), NULL, 0,
                                     COL_NAME, name,
                                     COL_CASEFOLDED_NAME, casefolded_name,
                                     COL_APP, appinfo,
                                     COL_ID, id,
                                     COL_CATEGORY, category,
                                     COL_DESCRIPTION, comment,
                                     COL_CASEFOLDED_DESCRIPTION, casefolded_description,
                                     COL_GICON, icon,
                                     COL_KEYWORDS, keywords,
                                     -1);

  g_free (casefolded_name);
  g_free (casefolded_description);
  g_strfreev (keywords);
}

gboolean
cc_shell_model_iter_matches_search (CcShellModel *model,
                                    GtkTreeIter  *iter,
                                    const char   *term)
{
  gchar *name, *description;
  gboolean result;
  gchar **keywords;

  gtk_tree_model_get (GTK_TREE_MODEL (model), iter,
                      COL_CASEFOLDED_NAME, &name,
                      COL_CASEFOLDED_DESCRIPTION, &description,
                      COL_KEYWORDS, &keywords,
                      -1);

  result = (strstr (name, term) != NULL);

  if (!result && description)
    result = (strstr (description, term) != NULL);

  if (!result && keywords)
    {
      gint i;

      for (i = 0; !result && keywords[i]; i++)
        result = (strstr (keywords[i], term) == keywords[i]);
    }

  g_free (name);
  g_free (description);
  g_strfreev (keywords);

  return result;
}
