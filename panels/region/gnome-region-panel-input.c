/*
 * Copyright (C) 2011 Red Hat, Inc.
 *
 * Written by: Matthias Clasen <mclasen@redhat.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
 * 02111-1307, USA.
 */

#include <config.h>

#include <string.h>

#include <glib.h>
#include <glib/gi18n.h>
#include <gio/gdesktopappinfo.h>

#define GNOME_DESKTOP_USE_UNSTABLE_API
#include <libgnome-desktop/gnome-xkb-info.h>

#ifdef HAVE_IBUS
#include <ibus.h>
#endif

#include "gdm-languages.h"
#include "gnome-region-panel-input.h"

#define WID(s) GTK_WIDGET(gtk_builder_get_object (builder, s))

#define GNOME_DESKTOP_INPUT_SOURCES_DIR "org.gnome.desktop.input-sources"

#define KEY_CURRENT_INPUT_SOURCE "current"
#define KEY_INPUT_SOURCES        "sources"

#define INPUT_SOURCE_TYPE_XKB  "xkb"
#define INPUT_SOURCE_TYPE_IBUS "ibus"

enum {
  NAME_COLUMN,
  TYPE_COLUMN,
  ID_COLUMN,
  SETUP_COLUMN,
  N_COLUMNS
};

static GSettings *input_sources_settings = NULL;
static GnomeXkbInfo *xkb_info = NULL;
static GtkWidget *input_chooser = NULL; /* weak pointer */

#ifdef HAVE_IBUS
static IBusBus *ibus = NULL;
static GHashTable *ibus_engines = NULL;
static GCancellable *ibus_cancellable = NULL;
static guint shell_name_watch_id = 0;

static const gchar *supported_ibus_engines[] = {
  /* Simplified Chinese */
  "pinyin",
  "bopomofo",
  "wubi",
  "erbi",
  /* Default in Fedora, where ibus-libpinyin replaces ibus-pinyin */
  "libpinyin",
  "libbopomofo",

  /* Traditional Chinese */
  /* https://bugzilla.gnome.org/show_bug.cgi?id=680840 */
  "chewing",
  "cangjie5",
  "cangjie3",
  "quick5",
  "quick3",
  "stroke5",

  /* Japanese */
  "anthy",
  "mozc-jp",
  "skk",

  /* Korean */
  "hangul",

  /* Thai */
  "m17n:th:kesmanee",
  "m17n:th:pattachote",
  "m17n:th:tis820",

  /* Vietnamese */
  "m17n:vi:tcvn",
  "m17n:vi:telex",
  "m17n:vi:viqr",
  "m17n:vi:vni",
  "Unikey",

  /* Sinhala */
  "m17n:si:wijesekera",
  "m17n:si:phonetic-dynamic",
  "m17n:si:trans",
  "sayura",

  /* Indic */
  /* https://fedoraproject.org/wiki/I18N/Indic#Keyboard_Layouts */

  /* Assamese */
  "m17n:as:phonetic",
  "m17n:as:inscript",
  "m17n:as:itrans",

  /* Bengali */
  "m17n:bn:inscript",
  "m17n:bn:itrans",
  "m17n:bn:probhat",

  /* Gujarati */
  "m17n:gu:inscript",
  "m17n:gu:itrans",
  "m17n:gu:phonetic",

  /* Hindi */
  "m17n:hi:inscript",
  "m17n:hi:itrans",
  "m17n:hi:phonetic",
  "m17n:hi:remington",
  "m17n:hi:typewriter",
  "m17n:hi:vedmata",

  /* Kannada */
  "m17n:kn:kgp",
  "m17n:kn:inscript",
  "m17n:kn:itrans",

  /* Kashmiri */
  "m17n:ks:inscript",

  /* Maithili */
  "m17n:mai:inscript",

  /* Malayalam */
  "m17n:ml:inscript",
  "m17n:ml:itrans",
  "m17n:ml:mozhi",
  "m17n:ml:swanalekha",

  /* Marathi */
  "m17n:mr:inscript",
  "m17n:mr:itrans",
  "m17n:mr:phonetic",

  /* Nepali */
  "m17n:ne:rom",
  "m17n:ne:trad",

  /* Oriya */
  "m17n:or:inscript",
  "m17n:or:itrans",
  "m17n:or:phonetic",

  /* Punjabi */
  "m17n:pa:inscript",
  "m17n:pa:itrans",
  "m17n:pa:phonetic",
  "m17n:pa:jhelum",

  /* Sanskrit */
  "m17n:sa:harvard-kyoto",

  /* Sindhi */
  "m17n:sd:inscript",

  /* Tamil */
  "m17n:ta:tamil99",
  "m17n:ta:inscript",
  "m17n:ta:itrans",
  "m17n:ta:phonetic",
  "m17n:ta:lk-renganathan",
  "m17n:ta:vutam",
  "m17n:ta:typewriter",

  /* Telugu */
  "m17n:te:inscript",
  "m17n:te:apple",
  "m17n:te:pothana",
  "m17n:te:rts",

  /* Urdu */
  "m17n:ur:phonetic",

  /* Inscript2 - https://bugzilla.gnome.org/show_bug.cgi?id=684854 */
  "m17n:as:inscript2",
  "m17n:bn:inscript2",
  "m17n:brx:inscript2-deva",
  "m17n:doi:inscript2-deva",
  "m17n:gu:inscript2",
  "m17n:hi:inscript2",
  "m17n:kn:inscript2",
  "m17n:kok:inscript2-deva",
  "m17n:mai:inscript2",
  "m17n:ml:inscript2",
  "m17n:mni:inscript2-beng",
  "m17n:mni:inscript2-mtei",
  "m17n:mr:inscript2",
  "m17n:ne:inscript2-deva",
  "m17n:or:inscript2",
  "m17n:pa:inscript2-guru",
  "m17n:sa:inscript2",
  "m17n:sat:inscript2-deva",
  "m17n:sat:inscript2-olck",
  "m17n:sd:inscript2-deva",
  "m17n:ta:inscript2",
  "m17n:te:inscript2",

  /* No corresponding XKB map available for the languages */

  /* Chinese Yi */
  "m17n:ii:phonetic",

  /* Tai-Viet */
  "m17n:tai:sonla",

  /* Kazakh in Arabic script */
  "m17n:kk:arabic",

  /* Yiddish */
  "m17n:yi:yivo",

  /* Canadian Aboriginal languages */
  "m17n:ath:phonetic",
  "m17n:bla:phonetic",
  "m17n:cr:western",
  "m17n:iu:phonetic",
  "m17n:nsk:phonetic",
  "m17n:oj:phonetic",

  /* Non-trivial engines, like transliteration-based instead of
     keymap-based.  Confirmation needed that the engines below are
     actually used by local language users. */

  /* Tibetan */
  "m17n:bo:ewts",
  "m17n:bo:tcrc",
  "m17n:bo:wylie",

  /* Esperanto */
  "m17n:eo:h-f",
  "m17n:eo:h",
  "m17n:eo:plena",
  "m17n:eo:q",
  "m17n:eo:vi",
  "m17n:eo:x",

  /* Amharic */
  "m17n:am:sera",

  /* Russian */
  "m17n:ru:translit",

  /* Classical Greek */
  "m17n:grc:mizuochi",

  /* Lao */
  "m17n:lo:lrt",

  /* Postfix modifier input methods */
  "m17n:da:post",
  "m17n:sv:post",
  NULL
};
#endif  /* HAVE_IBUS */

static void       populate_model             (GtkListStore  *store,
                                              GtkListStore  *active_sources_store);
static GtkWidget *input_chooser_new          (GtkWindow     *main_window,
                                              GtkListStore  *active_sources);
static gboolean   input_chooser_get_selected (GtkWidget     *chooser,
                                              GtkTreeModel **model,
                                              GtkTreeIter   *iter);
static GtkTreeModel *tree_view_get_actual_model (GtkTreeView *tv);

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

#ifdef HAVE_IBUS
static void
clear_ibus (void)
{
  if (shell_name_watch_id > 0)
    {
      g_bus_unwatch_name (shell_name_watch_id);
      shell_name_watch_id = 0;
    }
  g_cancellable_cancel (ibus_cancellable);
  g_clear_object (&ibus_cancellable);
  g_clear_pointer (&ibus_engines, g_hash_table_destroy);
  g_clear_object (&ibus);
}

static gchar *
engine_get_display_name (IBusEngineDesc *engine_desc)
{
  const gchar *name;
  const gchar *language_code;
  const gchar *language;
  gchar *display_name;

  name = ibus_engine_desc_get_longname (engine_desc);
  language_code = ibus_engine_desc_get_language (engine_desc);
  language = ibus_get_language_name (language_code);

  display_name = g_strdup_printf ("%s (%s)", language, name);

  return display_name;
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

static void
input_chooser_repopulate (GtkListStore *active_sources_store)
{
  GtkBuilder *builder;
  GtkListStore *model;

  if (!input_chooser)
    return;

  builder = g_object_get_data (G_OBJECT (input_chooser), "builder");
  model = GTK_LIST_STORE (gtk_builder_get_object (builder, "input_source_model"));

  gtk_list_store_clear (model);
  populate_model (model, active_sources_store);
}

static void
update_ibus_active_sources (GtkBuilder *builder)
{
  GtkTreeView *tv;
  GtkTreeModel *model;
  GtkTreeIter iter;
  gchar *type, *id;
  gboolean ret;

  tv = GTK_TREE_VIEW (WID ("active_input_sources"));
  model = tree_view_get_actual_model (tv);

  ret = gtk_tree_model_get_iter_first (model, &iter);
  while (ret)
    {
      gtk_tree_model_get (model, &iter,
                          TYPE_COLUMN, &type,
                          ID_COLUMN, &id,
                          -1);

      if (g_str_equal (type, INPUT_SOURCE_TYPE_IBUS))
        {
          IBusEngineDesc *engine_desc = NULL;
          GDesktopAppInfo *app_info = NULL;
          gchar *display_name = NULL;

          engine_desc = g_hash_table_lookup (ibus_engines, id);
          if (engine_desc)
            {
              display_name = engine_get_display_name (engine_desc);
              app_info = setup_app_info_for_id (id);

              gtk_list_store_set (GTK_LIST_STORE (model), &iter,
                                  NAME_COLUMN, display_name,
                                  SETUP_COLUMN, app_info,
                                  -1);
              g_free (display_name);
              if (app_info)
                g_object_unref (app_info);
            }
        }

      g_free (type);
      g_free (id);

      ret = gtk_tree_model_iter_next (model, &iter);
    }

  input_chooser_repopulate (GTK_LIST_STORE (model));
}

static void
fetch_ibus_engines_result (GObject      *object,
                           GAsyncResult *result,
                           GtkBuilder   *builder)
{
  gboolean show_all_sources;
  GList *list, *l;
  GError *error;

  error = NULL;
  list = ibus_bus_list_engines_async_finish (ibus, result, &error);

  g_clear_object (&ibus_cancellable);

  if (!list && error)
    {
      g_warning ("Couldn't finish IBus request: %s", error->message);
      g_error_free (error);
      return;
    }

  show_all_sources = g_settings_get_boolean (input_sources_settings, "show-all-sources");

  /* Maps engine ids to engine description objects */
  ibus_engines = g_hash_table_new_full (g_str_hash, g_str_equal, NULL, g_object_unref);

  for (l = list; l; l = l->next)
    {
      IBusEngineDesc *engine = l->data;
      const gchar *engine_id = ibus_engine_desc_get_name (engine);

      if (show_all_sources || strv_contains (supported_ibus_engines, engine_id))
        g_hash_table_replace (ibus_engines, (gpointer)engine_id, engine);
      else
        g_object_unref (engine);
    }
  g_list_free (list);

  update_ibus_active_sources (builder);
}

static void
fetch_ibus_engines (GtkBuilder *builder)
{
  ibus_cancellable = g_cancellable_new ();

  ibus_bus_list_engines_async (ibus,
                               -1,
                               ibus_cancellable,
                               (GAsyncReadyCallback)fetch_ibus_engines_result,
                               builder);

  /* We've got everything we needed, don't want to be called again. */
  g_signal_handlers_disconnect_by_func (ibus, fetch_ibus_engines, builder);
}

static void
maybe_start_ibus (void)
{
  /* IBus doesn't export API in the session bus. The only thing
   * we have there is a well known name which we can use as a
   * sure-fire way to activate it. */
  g_bus_unwatch_name (g_bus_watch_name (G_BUS_TYPE_SESSION,
                                        IBUS_SERVICE_IBUS,
                                        G_BUS_NAME_WATCHER_FLAGS_AUTO_START,
                                        NULL,
                                        NULL,
                                        NULL,
                                        NULL));
}

static void
on_shell_appeared (GDBusConnection *connection,
                   const gchar     *name,
                   const gchar     *name_owner,
                   gpointer         data)
{
  GtkBuilder *builder = data;

  if (!ibus)
    {
      ibus = ibus_bus_new_async ();
      if (ibus_bus_is_connected (ibus))
        fetch_ibus_engines (builder);
      else
        g_signal_connect_swapped (ibus, "connected",
                                  G_CALLBACK (fetch_ibus_engines), builder);
    }
  maybe_start_ibus ();
}
#endif  /* HAVE_IBUS */

static gboolean
add_source_to_table (GtkTreeModel *model,
                     GtkTreePath  *path,
                     GtkTreeIter  *iter,
                     gpointer      data)
{
  GHashTable *hash = data;
  gchar *type;
  gchar *id;

  gtk_tree_model_get (model, iter,
                      TYPE_COLUMN, &type,
                      ID_COLUMN, &id,
                      -1);

  g_hash_table_add (hash, g_strconcat (type, id, NULL));

  g_free (type);
  g_free (id);

  return FALSE;
}

static void
populate_model (GtkListStore *store,
                GtkListStore *active_sources_store)
{
  GHashTable *active_sources_table;
  GtkTreeIter iter;
  const gchar *name;
  GList *sources, *tmp;
  gchar *source_id = NULL;

  active_sources_table = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, NULL);

  gtk_tree_model_foreach (GTK_TREE_MODEL (active_sources_store),
                          add_source_to_table,
                          active_sources_table);

  sources = gnome_xkb_info_get_all_layouts (xkb_info);

  for (tmp = sources; tmp; tmp = tmp->next)
    {
      g_free (source_id);
      source_id = g_strconcat (INPUT_SOURCE_TYPE_XKB, tmp->data, NULL);

      if (g_hash_table_contains (active_sources_table, source_id))
        continue;

      gnome_xkb_info_get_layout_info (xkb_info, (const gchar *)tmp->data,
                                      &name, NULL, NULL, NULL);

      gtk_list_store_append (store, &iter);
      gtk_list_store_set (store, &iter,
                          NAME_COLUMN, name,
                          TYPE_COLUMN, INPUT_SOURCE_TYPE_XKB,
                          ID_COLUMN, tmp->data,
                          -1);
    }
  g_free (source_id);

  g_list_free (sources);

#ifdef HAVE_IBUS
  if (ibus_engines)
    {
      gchar *display_name;

      sources = g_hash_table_get_keys (ibus_engines);

      source_id = NULL;
      for (tmp = sources; tmp; tmp = tmp->next)
        {
          g_free (source_id);
          source_id = g_strconcat (INPUT_SOURCE_TYPE_IBUS, tmp->data, NULL);

          if (g_hash_table_contains (active_sources_table, source_id))
            continue;

          display_name = engine_get_display_name (g_hash_table_lookup (ibus_engines, tmp->data));

          gtk_list_store_append (store, &iter);
          gtk_list_store_set (store, &iter,
                              NAME_COLUMN, display_name,
                              TYPE_COLUMN, INPUT_SOURCE_TYPE_IBUS,
                              ID_COLUMN, tmp->data,
                              -1);
          g_free (display_name);
        }
      g_free (source_id);

      g_list_free (sources);
    }
#endif

  g_hash_table_destroy (active_sources_table);
}

static void
populate_with_active_sources (GtkListStore *store)
{
  GVariant *sources;
  GVariantIter iter;
  const gchar *name;
  const gchar *type;
  const gchar *id;
  gchar *display_name;
  GDesktopAppInfo *app_info;
  GtkTreeIter tree_iter;

  sources = g_settings_get_value (input_sources_settings, KEY_INPUT_SOURCES);

  g_variant_iter_init (&iter, sources);
  while (g_variant_iter_next (&iter, "(&s&s)", &type, &id))
    {
      display_name = NULL;
      app_info = NULL;

      if (g_str_equal (type, INPUT_SOURCE_TYPE_XKB))
        {
          gnome_xkb_info_get_layout_info (xkb_info, id, &name, NULL, NULL, NULL);
          if (!name)
            {
              g_warning ("Couldn't find XKB input source '%s'", id);
              continue;
            }
          display_name = g_strdup (name);
        }
      else if (g_str_equal (type, INPUT_SOURCE_TYPE_IBUS))
        {
#ifdef HAVE_IBUS
          IBusEngineDesc *engine_desc = NULL;

          if (ibus_engines)
            engine_desc = g_hash_table_lookup (ibus_engines, id);

          if (engine_desc)
            {
              display_name = engine_get_display_name (engine_desc);
              app_info = setup_app_info_for_id (id);
            }
#else
          g_warning ("IBus input source type specified but IBus support was not compiled");
          continue;
#endif
        }
      else
        {
          g_warning ("Unknown input source type '%s'", type);
          continue;
        }

      gtk_list_store_append (store, &tree_iter);
      gtk_list_store_set (store, &tree_iter,
                          NAME_COLUMN, display_name,
                          TYPE_COLUMN, type,
                          ID_COLUMN, id,
                          SETUP_COLUMN, app_info,
                          -1);
      g_free (display_name);
      if (app_info)
        g_object_unref (app_info);
    }

  g_variant_unref (sources);
}

static void
update_configuration (GtkTreeModel *model)
{
  GtkTreeIter iter;
  gchar *type;
  gchar *id;
  GVariantBuilder builder;
  GVariant *old_sources;
  const gchar *old_current_type;
  const gchar *old_current_id;
  guint old_current_index;
  guint old_n_sources;
  guint index;

  old_sources = g_settings_get_value (input_sources_settings, KEY_INPUT_SOURCES);
  old_current_index = g_settings_get_uint (input_sources_settings, KEY_CURRENT_INPUT_SOURCE);
  old_n_sources = g_variant_n_children (old_sources);

  if (old_n_sources > 0 && old_current_index < old_n_sources)
    {
      g_variant_get_child (old_sources,
                           old_current_index,
                           "(&s&s)",
                           &old_current_type,
                           &old_current_id);
    }
  else
    {
      old_current_type = "";
      old_current_id = "";
    }

  g_variant_builder_init (&builder, G_VARIANT_TYPE ("a(ss)"));
  index = 0;
  gtk_tree_model_get_iter_first (model, &iter);
  do
    {
      gtk_tree_model_get (model, &iter,
                          TYPE_COLUMN, &type,
                          ID_COLUMN, &id,
                          -1);
      if (index != old_current_index &&
          g_str_equal (type, old_current_type) &&
          g_str_equal (id, old_current_id))
        {
          g_settings_set_uint (input_sources_settings, KEY_CURRENT_INPUT_SOURCE, index);
        }
      g_variant_builder_add (&builder, "(ss)", type, id);
      g_free (type);
      g_free (id);
      index += 1;
    }
  while (gtk_tree_model_iter_next (model, &iter));

  g_settings_set_value (input_sources_settings, KEY_INPUT_SOURCES, g_variant_builder_end (&builder));
  g_settings_apply (input_sources_settings);

  g_variant_unref (old_sources);
}

static gboolean
get_selected_iter (GtkBuilder    *builder,
                   GtkTreeModel **model,
                   GtkTreeIter   *iter)
{
  GtkTreeSelection *selection;

  selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (WID ("active_input_sources")));

  return gtk_tree_selection_get_selected (selection, model, iter);
}

static gint
idx_from_model_iter (GtkTreeModel *model,
                     GtkTreeIter  *iter)
{
  GtkTreePath *path;
  gint idx;

  path = gtk_tree_model_get_path (model, iter);
  if (path == NULL)
    return -1;

  idx = gtk_tree_path_get_indices (path)[0];
  gtk_tree_path_free (path);

  return idx;
}

static void
update_button_sensitivity (GtkBuilder *builder)
{
  GtkWidget *remove_button;
  GtkWidget *up_button;
  GtkWidget *down_button;
  GtkWidget *show_button;
  GtkWidget *settings_button;
  GtkTreeView *tv;
  GtkTreeModel *model;
  GtkTreeIter iter;
  gint n_active;
  gint index;
  gboolean settings_sensitive;
  GDesktopAppInfo *app_info;

  remove_button = WID("input_source_remove");
  show_button = WID("input_source_show");
  up_button = WID("input_source_move_up");
  down_button = WID("input_source_move_down");
  settings_button = WID("input_source_settings");

  tv = GTK_TREE_VIEW (WID ("active_input_sources"));
  n_active = gtk_tree_model_iter_n_children (gtk_tree_view_get_model (tv), NULL);

  if (get_selected_iter (builder, &model, &iter))
    {
      index = idx_from_model_iter (model, &iter);
      gtk_tree_model_get (model, &iter, SETUP_COLUMN, &app_info, -1);
    }
  else
    {
      index = -1;
      app_info = NULL;
    }

  settings_sensitive = (index >= 0 && app_info != NULL);

  if (app_info)
    g_object_unref (app_info);

  gtk_widget_set_sensitive (remove_button, index >= 0 && n_active > 1);
  gtk_widget_set_sensitive (show_button, index >= 0);
  gtk_widget_set_sensitive (up_button, index > 0);
  gtk_widget_set_sensitive (down_button, index >= 0 && index < n_active - 1);
  gtk_widget_set_sensitive (settings_button, settings_sensitive);
}

static void
set_selected_path (GtkBuilder  *builder,
                   GtkTreePath *path)
{
  GtkTreeSelection *selection;

  selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (WID ("active_input_sources")));

  gtk_tree_selection_select_path (selection, path);
}

static GtkTreeModel *
tree_view_get_actual_model (GtkTreeView *tv)
{
  GtkTreeModel *filtered_store;

  filtered_store = gtk_tree_view_get_model (tv);

  return gtk_tree_model_filter_get_model (GTK_TREE_MODEL_FILTER (filtered_store));
}

static void
chooser_response (GtkWidget *chooser, gint response_id, gpointer data)
{
  GtkBuilder *builder = data;

  if (response_id == GTK_RESPONSE_OK)
    {
      GtkTreeModel *model;
      GtkTreeIter iter;

      if (input_chooser_get_selected (chooser, &model, &iter))
        {
          GtkTreeView *tv;
          GtkListStore *child_model;
          GtkTreeIter child_iter, filter_iter;
          gchar *name;
          gchar *type;
          gchar *id;
          GDesktopAppInfo *app_info = NULL;

          gtk_tree_model_get (model, &iter,
                              NAME_COLUMN, &name,
                              TYPE_COLUMN, &type,
                              ID_COLUMN, &id,
                              -1);

#ifdef HAVE_IBUS
          if (g_str_equal (type, INPUT_SOURCE_TYPE_IBUS))
            app_info = setup_app_info_for_id (id);
#endif

          tv = GTK_TREE_VIEW (WID ("active_input_sources"));
          child_model = GTK_LIST_STORE (tree_view_get_actual_model (tv));

          gtk_list_store_append (child_model, &child_iter);

          gtk_list_store_set (child_model, &child_iter,
                              NAME_COLUMN, name,
                              TYPE_COLUMN, type,
                              ID_COLUMN, id,
                              SETUP_COLUMN, app_info,
                              -1);
          g_free (name);
          g_free (type);
          g_free (id);
          if (app_info)
            g_object_unref (app_info);

          gtk_tree_model_filter_convert_child_iter_to_iter (GTK_TREE_MODEL_FILTER (gtk_tree_view_get_model (tv)),
                                                            &filter_iter,
                                                            &child_iter);
          gtk_tree_selection_select_iter (gtk_tree_view_get_selection (tv), &filter_iter);

          update_button_sensitivity (builder);
          update_configuration (GTK_TREE_MODEL (child_model));
        }
      else
        {
          g_debug ("nothing selected, nothing added");
        }
    }

  gtk_widget_destroy (GTK_WIDGET (chooser));
}

static void
add_input (GtkButton *button, gpointer data)
{
  GtkBuilder *builder = data;
  GtkWidget *chooser;
  GtkWidget *toplevel;
  GtkWidget *treeview;
  GtkListStore *active_sources;

  g_debug ("add an input source");

  toplevel = gtk_widget_get_toplevel (WID ("region_notebook"));
  treeview = WID ("active_input_sources");
  active_sources = GTK_LIST_STORE (tree_view_get_actual_model (GTK_TREE_VIEW (treeview)));

  chooser = input_chooser_new (GTK_WINDOW (toplevel), active_sources);
  g_signal_connect (chooser, "response",
                    G_CALLBACK (chooser_response), builder);
}

static void
remove_selected_input (GtkButton *button, gpointer data)
{
  GtkBuilder *builder = data;
  GtkTreeModel *model;
  GtkTreeModel *child_model;
  GtkTreeIter iter;
  GtkTreeIter child_iter;
  GtkTreePath *path;

  g_debug ("remove selected input source");

  if (get_selected_iter (builder, &model, &iter) == FALSE)
    return;

  path = gtk_tree_model_get_path (model, &iter);

  child_model = gtk_tree_model_filter_get_model (GTK_TREE_MODEL_FILTER (model));
  gtk_tree_model_filter_convert_iter_to_child_iter (GTK_TREE_MODEL_FILTER (model),
                                                    &child_iter,
                                                    &iter);
  gtk_list_store_remove (GTK_LIST_STORE (child_model), &child_iter);

  if (!gtk_tree_model_get_iter (model, &iter, path))
    gtk_tree_path_prev (path);

  set_selected_path (builder, path);

  gtk_tree_path_free (path);

  update_button_sensitivity (builder);
  update_configuration (child_model);
}

static void
move_selected_input_up (GtkButton *button, gpointer data)
{
  GtkBuilder *builder = data;
  GtkTreeModel *model;
  GtkTreeModel *child_model;
  GtkTreeIter iter, prev;
  GtkTreeIter child_iter, child_prev;
  GtkTreePath *path;

  g_debug ("move selected input source up");

  if (!get_selected_iter (builder, &model, &iter))
    return;

  prev = iter;
  if (!gtk_tree_model_iter_previous (model, &prev))
    return;

  path = gtk_tree_model_get_path (model, &prev);

  child_model = gtk_tree_model_filter_get_model (GTK_TREE_MODEL_FILTER (model));
  gtk_tree_model_filter_convert_iter_to_child_iter (GTK_TREE_MODEL_FILTER (model),
                                                    &child_iter,
                                                    &iter);
  gtk_tree_model_filter_convert_iter_to_child_iter (GTK_TREE_MODEL_FILTER (model),
                                                    &child_prev,
                                                    &prev);
  gtk_list_store_swap (GTK_LIST_STORE (child_model), &child_iter, &child_prev);

  set_selected_path (builder, path);
  gtk_tree_path_free (path);

  update_button_sensitivity (builder);
  update_configuration (child_model);
}

static void
move_selected_input_down (GtkButton *button, gpointer data)
{
  GtkBuilder *builder = data;
  GtkTreeModel *model;
  GtkTreeModel *child_model;
  GtkTreeIter iter, next;
  GtkTreeIter child_iter, child_next;
  GtkTreePath *path;

  g_debug ("move selected input source down");

  if (!get_selected_iter (builder, &model, &iter))
    return;

  next = iter;
  if (!gtk_tree_model_iter_next (model, &next))
    return;

  path = gtk_tree_model_get_path (model, &next);

  child_model = gtk_tree_model_filter_get_model (GTK_TREE_MODEL_FILTER (model));
  gtk_tree_model_filter_convert_iter_to_child_iter (GTK_TREE_MODEL_FILTER (model),
                                                    &child_iter,
                                                    &iter);
  gtk_tree_model_filter_convert_iter_to_child_iter (GTK_TREE_MODEL_FILTER (model),
                                                    &child_next,
                                                    &next);
  gtk_list_store_swap (GTK_LIST_STORE (child_model), &child_iter, &child_next);

  set_selected_path (builder, path);
  gtk_tree_path_free (path);

  update_button_sensitivity (builder);
  update_configuration (child_model);
}

static void
show_selected_layout (GtkButton *button, gpointer data)
{
  GtkBuilder *builder = data;
  GtkTreeModel *model;
  GtkTreeIter iter;
  gchar *type;
  gchar *id;
  gchar *kbd_viewer_args;
  const gchar *xkb_layout;
  const gchar *xkb_variant;

  g_debug ("show selected layout");

  if (!get_selected_iter (builder, &model, &iter))
    return;

  gtk_tree_model_get (model, &iter,
                      TYPE_COLUMN, &type,
                      ID_COLUMN, &id,
                      -1);

  if (g_str_equal (type, INPUT_SOURCE_TYPE_XKB))
    {
      gnome_xkb_info_get_layout_info (xkb_info, id, NULL, NULL, &xkb_layout, &xkb_variant);

      if (!xkb_layout || !xkb_layout[0])
        {
          g_warning ("Couldn't find XKB input source '%s'", id);
          goto exit;
        }
    }
  else if (g_str_equal (type, INPUT_SOURCE_TYPE_IBUS))
    {
#ifdef HAVE_IBUS
      IBusEngineDesc *engine_desc = NULL;

      if (ibus_engines)
        engine_desc = g_hash_table_lookup (ibus_engines, id);

      if (engine_desc)
        {
          xkb_layout = ibus_engine_desc_get_layout (engine_desc);
          xkb_variant = "";
        }
      else
        {
          g_warning ("Couldn't find IBus input source '%s'", id);
          goto exit;
        }
#else
      g_warning ("IBus input source type specified but IBus support was not compiled");
      goto exit;
#endif
    }
  else
    {
      g_warning ("Unknown input source type '%s'", type);
      goto exit;
    }

  if (xkb_variant[0])
    kbd_viewer_args = g_strdup_printf ("gkbd-keyboard-display -l \"%s\t%s\"",
                                       xkb_layout, xkb_variant);
  else
    kbd_viewer_args = g_strdup_printf ("gkbd-keyboard-display -l %s",
                                       xkb_layout);

  g_spawn_command_line_async (kbd_viewer_args, NULL);

  g_free (kbd_viewer_args);
 exit:
  g_free (type);
  g_free (id);
}

static void
show_selected_settings (GtkButton *button, gpointer data)
{
  GtkBuilder *builder = data;
  GtkTreeModel *model;
  GtkTreeIter iter;
  GdkAppLaunchContext *ctx;
  GDesktopAppInfo *app_info;
  gchar *id;
  GError *error = NULL;

  g_debug ("show selected layout");

  if (!get_selected_iter (builder, &model, &iter))
    return;

  gtk_tree_model_get (model, &iter, SETUP_COLUMN, &app_info, -1);

  if (!app_info)
    return;

  ctx = gdk_display_get_app_launch_context (gdk_display_get_default ());
  gdk_app_launch_context_set_timestamp (ctx, gtk_get_current_event_time ());

  gtk_tree_model_get (model, &iter, ID_COLUMN, &id, -1);
  g_app_launch_context_setenv (G_APP_LAUNCH_CONTEXT (ctx),
                               "IBUS_ENGINE_NAME",
                               id);
  g_free (id);

  if (!g_app_info_launch (G_APP_INFO (app_info), NULL, G_APP_LAUNCH_CONTEXT (ctx), &error))
    {
      g_warning ("Failed to launch input source setup: %s", error->message);
      g_error_free (error);
    }

  g_object_unref (ctx);
  g_object_unref (app_info);
}

static gboolean
go_to_shortcuts (GtkLinkButton *button,
                 CcRegionPanel *panel)
{
  CcShell *shell;
  const gchar *argv[] = { "shortcuts", "Typing", NULL };
  GError *error = NULL;

  shell = cc_panel_get_shell (CC_PANEL (panel));
  if (!cc_shell_set_active_panel_from_id (shell, "keyboard", argv, &error))
    {
      g_warning ("Failed to activate Keyboard panel: %s", error->message);
      g_error_free (error);
    }

  return TRUE;
}

static void
input_sources_changed (GSettings  *settings,
                       gchar      *key,
                       GtkBuilder *builder)
{
  GtkWidget *treeview;
  GtkTreeModel *store;
  GtkTreePath *path;
  GtkTreeIter iter;
  GtkTreeModel *model;

  treeview = WID("active_input_sources");
  store = tree_view_get_actual_model (GTK_TREE_VIEW (treeview));

  if (get_selected_iter (builder, &model, &iter))
    path = gtk_tree_model_get_path (model, &iter);
  else
    path = NULL;

  gtk_list_store_clear (GTK_LIST_STORE (store));
  populate_with_active_sources (GTK_LIST_STORE (store));

  if (path)
    {
      set_selected_path (builder, path);
      gtk_tree_path_free (path);
    }
}

static void
update_shortcut_label (GtkWidget  *widget,
		       const char *value)
{
  char *text;
  guint accel_key, *keycode;
  GdkModifierType mods;

  if (value == NULL || *value == '\0')
    {
      gtk_label_set_text (GTK_LABEL (widget), "\342\200\224");
      return;
    }
  gtk_accelerator_parse_with_keycode (value, &accel_key, &keycode, &mods);
  if (accel_key == 0 && keycode == NULL && mods == 0)
    {
      gtk_label_set_text (GTK_LABEL (widget), "\342\200\224");
      g_warning ("Failed to parse keyboard shortcut: '%s'", value);
      return;
    }

  text = gtk_accelerator_get_label_with_keycode (gtk_widget_get_display (widget), accel_key, *keycode, mods);
  g_free (keycode);
  gtk_label_set_text (GTK_LABEL (widget), text);
  g_free (text);
}

static void
update_shortcuts (GtkBuilder *builder)
{
  char *previous, *next;
  GSettings *settings;

  settings = g_settings_new ("org.gnome.settings-daemon.plugins.media-keys");

  previous = g_settings_get_string (settings, "switch-input-source-backward");
  next = g_settings_get_string (settings, "switch-input-source");

  update_shortcut_label (WID ("prev-source-shortcut-label"), previous);
  update_shortcut_label (WID ("next-source-shortcut-label"), next);

  g_free (previous);
  g_free (next);
}

static gboolean
active_sources_visible_func (GtkTreeModel *model,
                             GtkTreeIter  *iter,
                             gpointer      data)
{
  gchar *display_name;

  gtk_tree_model_get (model, iter, NAME_COLUMN, &display_name, -1);

  if (!display_name)
    return FALSE;

  g_free (display_name);

  return TRUE;
}

void
setup_input_tabs (GtkBuilder    *builder,
                  CcRegionPanel *panel)
{
  GtkWidget *treeview;
  GtkTreeViewColumn *column;
  GtkCellRenderer *cell;
  GtkListStore *store;
  GtkTreeModel *filtered_store;
  GtkTreeSelection *selection;

  /* set up the list of active inputs */
  treeview = WID("active_input_sources");
  column = gtk_tree_view_column_new ();
  cell = gtk_cell_renderer_text_new ();
  gtk_tree_view_column_pack_start (column, cell, TRUE);
  gtk_tree_view_column_add_attribute (column, cell, "text", NAME_COLUMN);
  gtk_tree_view_append_column (GTK_TREE_VIEW (treeview), column);

  store = gtk_list_store_new (N_COLUMNS,
                              G_TYPE_STRING,
                              G_TYPE_STRING,
                              G_TYPE_STRING,
                              G_TYPE_DESKTOP_APP_INFO);

  gtk_tree_view_set_model (GTK_TREE_VIEW (treeview), GTK_TREE_MODEL (store));

  input_sources_settings = g_settings_new (GNOME_DESKTOP_INPUT_SOURCES_DIR);
  g_settings_delay (input_sources_settings);
  g_object_weak_ref (G_OBJECT (builder), (GWeakNotify) g_object_unref, input_sources_settings);

  if (!xkb_info)
    xkb_info = gnome_xkb_info_new ();

#ifdef HAVE_IBUS
  ibus_init ();
  shell_name_watch_id = g_bus_watch_name (G_BUS_TYPE_SESSION,
                                          "org.gnome.Shell",
                                          G_BUS_NAME_WATCHER_FLAGS_NONE,
                                          on_shell_appeared,
                                          NULL,
                                          builder,
                                          NULL);
  g_object_weak_ref (G_OBJECT (builder), (GWeakNotify) clear_ibus, NULL);
#endif

  populate_with_active_sources (store);

  selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (treeview));
  g_signal_connect_swapped (selection, "changed",
                            G_CALLBACK (update_button_sensitivity), builder);

  /* Some input source types might have their info loaded
   * asynchronously. In that case we don't want to show them
   * immediately so we use a filter model on top of the real model
   * which mirrors the GSettings key. */
  filtered_store = gtk_tree_model_filter_new (GTK_TREE_MODEL (store), NULL);
  gtk_tree_model_filter_set_visible_func (GTK_TREE_MODEL_FILTER (filtered_store),
                                          active_sources_visible_func,
                                          NULL,
                                          NULL);
  gtk_tree_view_set_model (GTK_TREE_VIEW (treeview), filtered_store);

  /* set up the buttons */
  g_signal_connect (WID("input_source_add"), "clicked",
                    G_CALLBACK (add_input), builder);
  g_signal_connect (WID("input_source_remove"), "clicked",
                    G_CALLBACK (remove_selected_input), builder);
  g_signal_connect (WID("input_source_move_up"), "clicked",
                    G_CALLBACK (move_selected_input_up), builder);
  g_signal_connect (WID("input_source_move_down"), "clicked",
                    G_CALLBACK (move_selected_input_down), builder);
  g_signal_connect (WID("input_source_show"), "clicked",
                    G_CALLBACK (show_selected_layout), builder);
  g_signal_connect (WID("input_source_settings"), "clicked",
                    G_CALLBACK (show_selected_settings), builder);

  /* use an em dash is no shortcut */
  update_shortcuts (builder);

  g_signal_connect (WID("jump-to-shortcuts"), "activate-link",
                    G_CALLBACK (go_to_shortcuts), panel);

  g_signal_connect (G_OBJECT (input_sources_settings),
                    "changed::" KEY_INPUT_SOURCES,
                    G_CALLBACK (input_sources_changed),
                    builder);
}

static void
filter_clear (GtkEntry             *entry,
              GtkEntryIconPosition  icon_pos,
              GdkEvent             *event,
              gpointer              user_data)
{
  gtk_entry_set_text (entry, "");
}

static gchar **search_pattern_list;

static void
filter_changed (GtkBuilder *builder)
{
  GtkTreeModelFilter *filtered_model;
  GtkTreeView *tree_view;
  GtkTreeSelection *selection;
  GtkTreeIter selected_iter;
  GtkWidget *filter_entry;
  const gchar *pattern;
  gchar *upattern;

  filter_entry = WID ("input_source_filter");
  pattern = gtk_entry_get_text (GTK_ENTRY (filter_entry));
  upattern = g_utf8_strup (pattern, -1);
  if (!g_strcmp0 (pattern, ""))
    g_object_set (G_OBJECT (filter_entry),
                  "secondary-icon-name", "edit-find-symbolic",
                  "secondary-icon-activatable", FALSE,
                  "secondary-icon-sensitive", FALSE,
                  NULL);
  else
    g_object_set (G_OBJECT (filter_entry),
                  "secondary-icon-name", "edit-clear-symbolic",
                  "secondary-icon-activatable", TRUE,
                  "secondary-icon-sensitive", TRUE,
                  NULL);

  if (search_pattern_list != NULL)
    g_strfreev (search_pattern_list);

  search_pattern_list = g_strsplit (upattern, " ", -1);
  g_free (upattern);

  filtered_model = GTK_TREE_MODEL_FILTER (gtk_builder_get_object (builder, "filtered_input_source_model"));
  gtk_tree_model_filter_refilter (filtered_model);

  tree_view = GTK_TREE_VIEW (WID ("filtered_input_source_list"));
  selection = gtk_tree_view_get_selection (tree_view);
  if (gtk_tree_selection_get_selected (selection, NULL, &selected_iter))
    {
      GtkTreePath *path = gtk_tree_model_get_path (GTK_TREE_MODEL (filtered_model),
                                                   &selected_iter);
      gtk_tree_view_scroll_to_cell (tree_view, path, NULL, TRUE, 0.5, 0.5);
      gtk_tree_path_free (path);
    }
  else
    {
      GtkTreeIter iter;
      if (gtk_tree_model_get_iter_first (GTK_TREE_MODEL (filtered_model), &iter))
        gtk_tree_selection_select_iter (selection, &iter);
    }
}

static void
selection_changed (GtkTreeSelection *selection,
                   GtkBuilder       *builder)
{
  gtk_widget_set_sensitive (WID ("ok-button"),
                            gtk_tree_selection_get_selected (selection, NULL, NULL));
}

static void
row_activated (GtkTreeView       *tree_view,
               GtkTreePath       *path,
               GtkTreeViewColumn *column,
               GtkBuilder        *builder)
{
  GtkWidget *add_button;
  GtkWidget *dialog;

  add_button = WID ("ok-button");
  dialog = WID ("input_source_chooser");
  if (gtk_widget_is_sensitive (add_button))
    gtk_dialog_response (GTK_DIALOG (dialog), GTK_RESPONSE_OK);
}

static void
entry_activated (GtkBuilder *builder,
                 gpointer    data)
{
  row_activated (NULL, NULL, NULL, builder);
}

static gboolean
filter_func (GtkTreeModel *model,
             GtkTreeIter  *iter,
             gpointer      data)
{
  gchar *name = NULL;
  gchar **pattern;
  gboolean rv = TRUE;

  if (search_pattern_list == NULL || search_pattern_list[0] == NULL)
    return TRUE;

  gtk_tree_model_get (model, iter,
                      NAME_COLUMN, &name,
                      -1);

  pattern = search_pattern_list;
  do {
    gboolean is_pattern_found = FALSE;
    gchar *udesc = g_utf8_strup (name, -1);
    if (udesc != NULL && g_strstr_len (udesc, -1, *pattern))
      {
        is_pattern_found = TRUE;
      }
    g_free (udesc);

    if (!is_pattern_found)
      {
        rv = FALSE;
        break;
      }

  } while (*++pattern != NULL);

  g_free (name);

  return rv;
}

static GtkWidget *
input_chooser_new (GtkWindow    *main_window,
                   GtkListStore *active_sources)
{
  GtkBuilder *builder;
  GtkWidget *chooser;
  GtkWidget *filtered_list;
  GtkWidget *filter_entry;
  GtkTreeViewColumn *visible_column;
  GtkTreeSelection *selection;
  GtkListStore *model;
  GtkTreeModelFilter *filtered_model;
  GtkTreeIter iter;

  builder = gtk_builder_new ();
  gtk_builder_add_from_file (builder,
                             GNOMECC_UI_DIR "/gnome-region-panel-input-chooser.ui",
                             NULL);
  chooser = WID ("input_source_chooser");
  input_chooser = chooser;
  g_object_add_weak_pointer (G_OBJECT (chooser), (gpointer *) &input_chooser);
  g_object_set_data_full (G_OBJECT (chooser), "builder", builder, g_object_unref);

  filtered_list = WID ("filtered_input_source_list");
  filter_entry = WID ("input_source_filter");

  g_object_set_data (G_OBJECT (chooser),
                     "filtered_input_source_list", filtered_list);
  visible_column =
    gtk_tree_view_column_new_with_attributes ("Input Sources",
                                              gtk_cell_renderer_text_new (),
                                              "text", NAME_COLUMN,
                                              NULL);

  gtk_window_set_transient_for (GTK_WINDOW (chooser), main_window);

  gtk_tree_view_append_column (GTK_TREE_VIEW (filtered_list),
                               visible_column);
  /* We handle searching ourselves, thank you. */
  gtk_tree_view_set_enable_search (GTK_TREE_VIEW (filtered_list), FALSE);
  gtk_tree_view_set_search_column (GTK_TREE_VIEW (filtered_list), -1);

  g_signal_connect_swapped (G_OBJECT (filter_entry), "activate",
                            G_CALLBACK (entry_activated), builder);
  g_signal_connect_swapped (G_OBJECT (filter_entry), "notify::text",
                            G_CALLBACK (filter_changed), builder);

  g_signal_connect (G_OBJECT (filter_entry), "icon-release",
                    G_CALLBACK (filter_clear), NULL);

  filtered_model = GTK_TREE_MODEL_FILTER (gtk_builder_get_object (builder, "filtered_input_source_model"));
  model = GTK_LIST_STORE (gtk_builder_get_object (builder, "input_source_model"));

  populate_model (model, active_sources);

  gtk_tree_sortable_set_sort_column_id (GTK_TREE_SORTABLE (model),
                                        NAME_COLUMN, GTK_SORT_ASCENDING);

  gtk_tree_model_filter_set_visible_func (filtered_model,
                                          filter_func,
                                          NULL, NULL);

  selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (filtered_list));

  g_signal_connect (G_OBJECT (selection), "changed",
                    G_CALLBACK (selection_changed), builder);

  if (gtk_tree_model_get_iter_first (GTK_TREE_MODEL (filtered_model), &iter))
    gtk_tree_selection_select_iter (selection, &iter);

  g_signal_connect (G_OBJECT (filtered_list), "row-activated",
                    G_CALLBACK (row_activated), builder);

  gtk_widget_grab_focus (filter_entry);

  gtk_widget_show (chooser);

  return chooser;
}

static gboolean
input_chooser_get_selected (GtkWidget     *dialog,
                            GtkTreeModel **model,
                            GtkTreeIter   *iter)
{
  GtkWidget *tv;
  GtkTreeSelection *selection;

  tv = g_object_get_data (G_OBJECT (dialog), "filtered_input_source_list");
  selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (tv));

  return gtk_tree_selection_get_selected (selection, model, iter);
}
