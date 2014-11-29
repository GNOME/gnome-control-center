/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*-
 *
 * Copyright (C) 2012 Red Hat, Inc
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
 * Author: Cosimo Cecchi <cosimoc@gnome.org>
 */

#include "cc-search-locations-dialog.h"

#include <glib/gi18n.h>

#define TRACKER_SCHEMA "org.freedesktop.Tracker.Miner.Files"
#define TRACKER_KEY_RECURSIVE_DIRECTORIES "index-recursive-directories"
#define TRACKER_KEY_SINGLE_DIRECTORIES "index-single-directories"

static GSettings *tracker_preferences = NULL;

typedef enum {
  PLACE_XDG,
  PLACE_BOOKMARKS,
  PLACE_OTHER
} PlaceType;

typedef struct {
  GFile *location;
  gchar *display_name;
  PlaceType place_type;
  GIcon *icon;
  GCancellable *cancellable;
  const gchar *settings_key;
} Place;

static void
place_free (Place * p)
{
  if (p->cancellable != NULL)
    {
      g_cancellable_cancel (p->cancellable);
      g_object_unref (p->cancellable);
    }

  g_clear_object (&p->icon);
  g_object_unref (p->location);
  g_free (p->display_name);

  g_slice_free (Place, p);
}

static GList *
get_bookmarks (void)
{
  GFile *file;
  gchar *contents;
  gchar *path;
  gchar **lines;
  GList *bookmarks;
  GError *error = NULL;

  path = g_build_filename (g_get_user_config_dir (), "gtk-3.0",
                           "bookmarks", NULL);
  file = g_file_new_for_path (path);
  g_free (path);

  contents = NULL;
  g_file_load_contents (file, NULL, &contents, NULL, NULL, &error);
  g_object_unref (file);

  bookmarks = NULL;
  lines = NULL;

  if (error != NULL)
    {
      g_error_free (error);
    }
  else
    {
      gint idx;

      lines = g_strsplit (contents, "\n", -1);
      for (idx = 0; lines[idx]; idx++)
        {
          /* Ignore empty or invalid lines that cannot be parsed properly */
          if (lines[idx][0] != '\0' && lines[idx][0] != ' ')
            {
              /* gtk 2.7/2.8 might have labels appended to bookmarks which are separated by a space */
              /* we must seperate the bookmark uri and the potential label */
              char *space, *label;
              Place *bookmark;

              label = NULL;
              space = strchr (lines[idx], ' ');
              if (space)
                {
                  *space = '\0';
                  label = g_strdup (space + 1);
                }

              bookmark = g_slice_new0 (Place);
              bookmark->location = g_file_new_for_uri (lines[idx]);

              if (label != NULL)
                bookmark->display_name = label;
              else
                bookmark->display_name = g_file_get_basename (bookmark->location);

              bookmark->place_type = PLACE_BOOKMARKS;

              bookmarks = g_list_prepend (bookmarks, bookmark);
            }
	}
    }

  g_strfreev (lines);
  g_free (contents);

  return g_list_reverse (bookmarks);
}

static const gchar *
get_user_special_dir_if_not_home (GUserDirectory idx)
{
  const gchar *path;
  path = g_get_user_special_dir (idx);
  if (g_strcmp0 (path, g_get_home_dir ()) == 0)
    return NULL;

  return path;
}

static GList *
get_xdg_dirs (void)
{
  GList *xdg_dirs = NULL;
  gint idx;
  const gchar *path;
  Place *xdg_dir;

  for (idx = 0; idx < G_USER_N_DIRECTORIES; idx++)
    {
      path = get_user_special_dir_if_not_home (idx);
      if (path == NULL)
        continue;

      if (idx == G_USER_DIRECTORY_TEMPLATES ||
          idx == G_USER_DIRECTORY_PUBLIC_SHARE ||
          idx == G_USER_DIRECTORY_DESKTOP)
        continue;

      xdg_dir = g_slice_new0 (Place);
      xdg_dir->location = g_file_new_for_path (path);
      xdg_dir->display_name = g_file_get_basename (xdg_dir->location);
      xdg_dir->place_type = PLACE_XDG;

      xdg_dirs = g_list_prepend (xdg_dirs, xdg_dir);
    }

  return g_list_reverse (xdg_dirs);
}

static const gchar *
path_to_tracker_dir (const gchar *path)
{
  const gchar *value;

  if (g_strcmp0 (path, get_user_special_dir_if_not_home (G_USER_DIRECTORY_DESKTOP)) == 0)
    value = "&DESKTOP";
  else if (g_strcmp0 (path, get_user_special_dir_if_not_home (G_USER_DIRECTORY_DOCUMENTS)) == 0)
    value = "&DOCUMENTS";
  else if (g_strcmp0 (path, get_user_special_dir_if_not_home (G_USER_DIRECTORY_DOWNLOAD)) == 0)
    value = "&DOWNLOAD";
  else if (g_strcmp0 (path, get_user_special_dir_if_not_home (G_USER_DIRECTORY_MUSIC)) == 0)
    value = "&MUSIC";
  else if (g_strcmp0 (path, get_user_special_dir_if_not_home (G_USER_DIRECTORY_PICTURES)) == 0)
    value = "&PICTURES";
  else if (g_strcmp0 (path, get_user_special_dir_if_not_home (G_USER_DIRECTORY_PUBLIC_SHARE)) == 0)
    value = "&PUBLIC_SHARE";
  else if (g_strcmp0 (path, get_user_special_dir_if_not_home (G_USER_DIRECTORY_TEMPLATES)) == 0)
    value = "&TEMPLATES";
  else if (g_strcmp0 (path, get_user_special_dir_if_not_home (G_USER_DIRECTORY_VIDEOS)) == 0)
    value = "&VIDEOS";
  else if (g_strcmp0 (path, g_get_home_dir ()) == 0)
    value = "$HOME";
  else
    value = path;

  return value;
}

static const gchar *
path_from_tracker_dir (const gchar *value)
{
  const gchar *path;

  if (g_strcmp0 (value, "&DESKTOP") == 0)
    path = get_user_special_dir_if_not_home (G_USER_DIRECTORY_DESKTOP);
  else if (g_strcmp0 (value, "&DOCUMENTS") == 0)
    path = get_user_special_dir_if_not_home (G_USER_DIRECTORY_DOCUMENTS);
  else if (g_strcmp0 (value, "&DOWNLOAD") == 0)
    path = get_user_special_dir_if_not_home (G_USER_DIRECTORY_DOWNLOAD);
  else if (g_strcmp0 (value, "&MUSIC") == 0)
    path = get_user_special_dir_if_not_home (G_USER_DIRECTORY_MUSIC);
  else if (g_strcmp0 (value, "&PICTURES") == 0)
    path = get_user_special_dir_if_not_home (G_USER_DIRECTORY_PICTURES);
  else if (g_strcmp0 (value, "&PUBLIC_SHARE") == 0)
    path = get_user_special_dir_if_not_home (G_USER_DIRECTORY_PUBLIC_SHARE);
  else if (g_strcmp0 (value, "&TEMPLATES") == 0)
    path = get_user_special_dir_if_not_home (G_USER_DIRECTORY_TEMPLATES);
  else if (g_strcmp0 (value, "&VIDEOS") == 0)
    path = get_user_special_dir_if_not_home (G_USER_DIRECTORY_VIDEOS);
  else if (g_strcmp0 (value, "$HOME") == 0)
    path = g_get_home_dir ();
  else
    path = value;

  return path;
}

static GList *
get_tracker_locations (void)
{
  gchar **locations;
  GList *list;
  gint idx;
  Place *location;
  const gchar *path;

  locations = g_settings_get_strv (tracker_preferences, TRACKER_KEY_RECURSIVE_DIRECTORIES);
  list = NULL;

  for (idx = 0; locations[idx] != NULL; idx++)
    {
      path = path_from_tracker_dir (locations[idx]);

      location = g_slice_new0 (Place);
      location->location = g_file_new_for_commandline_arg (path);
      location->display_name = g_file_get_basename (location->location);
      location->place_type = PLACE_OTHER;

      list = g_list_prepend (list, location);
    }

  g_strfreev (locations);

  return g_list_reverse (list);
}

static GList *
get_places_list (void)
{
  GList *list, *l;
  GHashTable *places;
  Place *place, *old_place;
  GList *places_list;

  places = g_hash_table_new_full (g_file_hash, (GEqualFunc) g_file_equal, NULL, (GDestroyNotify) place_free);

  /* add home */
  place = g_slice_new0 (Place);
  place->location = g_file_new_for_path (g_get_home_dir ());
  place->place_type = PLACE_XDG;
  place->display_name = g_strdup (_("Home"));
  g_hash_table_insert (places, place->location, place);

  /* first, load the XDG dirs */
  list = get_xdg_dirs ();
  for (l = list; l != NULL; l = l->next)
    {
      place = l->data;
      g_hash_table_insert (places, place->location, place);
    }
  g_list_free (list);

  /* then, insert all the tracker locations that are not XDG dirs */
  list = get_tracker_locations ();
  for (l = list; l != NULL; l = l->next)
    {
      place = l->data;
      old_place = g_hash_table_lookup (places, place->location);
      if (old_place == NULL)
        g_hash_table_insert (places, place->location, place);
      else
        place_free (place);
    }
  g_list_free (list);

  /* finally, load bookmarks, and possibly update attributes */
  list = get_bookmarks ();
  for (l = list; l != NULL; l = l->next)
    {
      place = l->data;
      old_place = g_hash_table_lookup (places, place->location);
      if (old_place == NULL)
        {
          g_hash_table_insert (places, place->location, place);
        }
      else
        {
          g_free (old_place->display_name);
          old_place->display_name = g_strdup (place->display_name);

          if (old_place->place_type == PLACE_OTHER)
            old_place->place_type = PLACE_BOOKMARKS;

          place_free (place);
        }
    }
  g_list_free (list);

  places_list = g_hash_table_get_values (places);
  g_hash_table_steal_all (places);
  g_hash_table_unref (places);

  return places_list;
}

static gboolean
switch_tracker_get_mapping (GValue *value,
                            GVariant *variant,
                            gpointer user_data)
{
  Place *place = user_data;
  const gchar **locations;
  GFile *location;
  gint idx;
  gboolean found;

  found = FALSE;
  locations = g_variant_get_strv (variant, NULL);
  for (idx = 0; locations[idx] != NULL; idx++)
    {
      location = g_file_new_for_path (path_from_tracker_dir(locations[idx]));
      if ((found = g_file_equal (location, place->location)))
        break;
    }

  g_value_set_boolean (value, found);
  return TRUE;
}

static GPtrArray *
place_get_new_settings_values (Place *place,
                               gboolean remove)
{
  gchar **values;
  gchar *path;
  GPtrArray *new_values;
  const gchar *tracker_dir;
  gboolean found;
  gint idx;

  new_values = g_ptr_array_new_with_free_func (g_free);
  values = g_settings_get_strv (tracker_preferences, place->settings_key);
  path = g_file_get_path (place->location);
  tracker_dir = path_to_tracker_dir (path);

  found = FALSE;

  for (idx = 0; values[idx] != NULL; idx++)
    {
      if (g_strcmp0 (values[idx], tracker_dir) == 0)
        {
          found = TRUE;

          if (remove)
            continue;
        }

      g_ptr_array_add (new_values, g_strdup (values[idx]));
    }

  if (!found && !remove)
    g_ptr_array_add (new_values, g_strdup (tracker_dir));

  g_ptr_array_add (new_values, NULL);

  g_strfreev (values);
  g_free (path);

  return new_values;
}

static GVariant *
switch_tracker_set_mapping (const GValue *value,
                            const GVariantType *expected_type,
                            gpointer user_data)
{
  Place *place = user_data;
  GPtrArray *new_values;
  gboolean remove;
  GVariant *variant;

  remove = !g_value_get_boolean (value);
  new_values = place_get_new_settings_values (place, remove);
  variant = g_variant_new_strv ((const gchar **) new_values->pdata, -1);

  g_ptr_array_unref (new_values);

  return variant;
}

static void
place_query_info_ready (GObject *source,
                        GAsyncResult *res,
                        gpointer user_data)
{
  GtkWidget *row, *box, *w;
  Place *place;
  GFileInfo *info;
  const gchar *desktop_path;
  gchar *path;

  info = g_file_query_info_finish (G_FILE (source), res, NULL);
  if (!info)
    return;

  row = user_data;
  place = g_object_get_data (G_OBJECT (row), "place");
  g_clear_object (&place->cancellable);

  box = gtk_bin_get_child (GTK_BIN (row));

  /* FIXME: GLib is currently buggy and returns a non-existent icon name
   * when asked for the desktop symbolic icon.
   */
  desktop_path = g_get_user_special_dir (G_USER_DIRECTORY_DESKTOP);
  path = g_file_get_path (G_FILE (source));

  if (g_strcmp0 (path, desktop_path) == 0)
    place->icon = g_themed_icon_new ("folder-symbolic");
  else
    place->icon = g_object_ref (g_file_info_get_symbolic_icon (info));

  if (g_strcmp0 (path, g_get_home_dir ()) == 0)
    place->settings_key = TRACKER_KEY_SINGLE_DIRECTORIES;
  else
    place->settings_key = TRACKER_KEY_RECURSIVE_DIRECTORIES;

  g_free (path);

  w = gtk_image_new_from_gicon (place->icon, GTK_ICON_SIZE_MENU);
  gtk_container_add (GTK_CONTAINER (box), w);

  w = gtk_label_new (place->display_name);
  gtk_container_add (GTK_CONTAINER (box), w);

  w = gtk_switch_new ();
  gtk_box_pack_end (GTK_BOX (box), w, FALSE, FALSE, 0);
  g_settings_bind_with_mapping (tracker_preferences, place->settings_key,
                                w, "active",
                                G_SETTINGS_BIND_DEFAULT,
                                switch_tracker_get_mapping,
                                switch_tracker_set_mapping,
                                place, NULL);

  gtk_widget_show_all (row);
  g_object_unref (info);
}

static const gchar *
get_heading_name (PlaceType place)
{
  if (place == PLACE_XDG)
    return C_("Search Location", "Places");
  if (place == PLACE_BOOKMARKS)
    return C_("Search Location", "Bookmarks");
  if (place == PLACE_OTHER)
    return C_("Search Location", "Other");

  g_assert_not_reached ();
  return NULL;
}

static void
place_header_func (GtkListBoxRow *row,
                   GtkListBoxRow *before,
                   gpointer user_data)
{
  gboolean need_separator;
  GtkWidget *current;
  Place *place, *place_before;
  gchar *text;
  GtkWidget *w;

  need_separator = FALSE;
  place = g_object_get_data (G_OBJECT (row), "place");

  if (before != NULL)
    {
      place_before = g_object_get_data (G_OBJECT (before), "place");
      if (place_before->place_type < place->place_type)
        /* use a separator before the first item of a new type */
        need_separator = TRUE;
    }
  else
    {
      /* always put a separator before the first item */
      need_separator = TRUE;
    }

  current = gtk_list_box_row_get_header (row);
  if (need_separator && current == NULL)
    {
      text = g_strdup_printf ("<b>%s</b>", get_heading_name (place->place_type));
      w = gtk_label_new (NULL);
      g_object_set (w,
                    "margin-top", 6,
                    "margin-end", 10,
                    "margin-bottom", 6,
                    "margin-start", 10,
                    NULL);
      gtk_label_set_markup (GTK_LABEL (w), text);
      gtk_widget_set_halign (w, GTK_ALIGN_START);
      gtk_style_context_add_class (gtk_widget_get_style_context (w), "dim-label");

      gtk_list_box_row_set_header (row, w);

      g_free (text);
    }
  else if (!need_separator && current != NULL)
    {
      gtk_list_box_row_set_header (row, NULL);
    }
}

static gint
place_compare_func (gconstpointer a,
                    gconstpointer b,
                    gpointer user_data)
{
  GtkWidget *child_a, *child_b;
  Place *place_a, *place_b;
  gchar *path;
  gboolean is_home;

  child_a = GTK_WIDGET (a);
  child_b = GTK_WIDGET (b);

  place_a = g_object_get_data (G_OBJECT (child_a), "place");
  place_b = g_object_get_data (G_OBJECT (child_b), "place");

  path = g_file_get_path (place_a->location);
  is_home = (g_strcmp0 (path, g_get_home_dir ()) == 0);
  g_free (path);

  if (is_home)
    return -1;

  if (place_a->place_type == place_b->place_type)
    return g_utf8_collate (place_a->display_name, place_b->display_name);

  if (place_a->place_type == PLACE_XDG)
    return -1;

  if ((place_a->place_type == PLACE_BOOKMARKS) && (place_b->place_type == PLACE_OTHER))
    return -1;

  return 1;
}

static GtkWidget *
create_row_for_place (Place *place)
{
  GtkWidget *child, *row;

  row = gtk_list_box_row_new ();
  child = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 6);
  gtk_container_add (GTK_CONTAINER (row), child);
  g_object_set (row, "margin-start", 16, "margin-end", 16, NULL);
  g_object_set_data_full (G_OBJECT (row), "place", place, (GDestroyNotify) place_free);

  place->cancellable = g_cancellable_new ();
  g_file_query_info_async (place->location, "standard::symbolic-icon",
                           G_FILE_QUERY_INFO_NONE, G_PRIORITY_DEFAULT,
                           place->cancellable, place_query_info_ready, row);

  return row;
}

static void
populate_list_box (GtkWidget *list_box)
{
  GList *places, *l;
  GtkWidget *row;

  places = get_places_list ();
  for (l = places; l != NULL; l = l->next)
    {
      /* assumes ownership of place */
      row = create_row_for_place (l->data);
      gtk_container_add (GTK_CONTAINER (list_box), row);
    }

  g_list_free (places);
}

static void
list_box_row_selected (GtkListBox *list_box,
                       GtkListBoxRow *row,
                       gpointer user_data)
{
  GtkWidget *remove_button = user_data;
  Place *place;
  gboolean sensitive = FALSE;

  if (row != NULL)
    {
      place = g_object_get_data (G_OBJECT (row), "place");
      sensitive = (place->place_type == PLACE_OTHER);
    }

  gtk_widget_set_sensitive (remove_button, sensitive);
}

static void
remove_button_clicked (GtkWidget *widget,
                       gpointer user_data)
{
  GtkWidget *list_box = user_data;
  GtkListBoxRow *row;
  Place *place;
  GPtrArray *new_values;

  row = gtk_list_box_get_selected_row (GTK_LIST_BOX (list_box));
  place = g_object_get_data (G_OBJECT (row), "place");
  new_values = place_get_new_settings_values (place, TRUE);
  g_settings_set_strv (tracker_preferences, place->settings_key, (const gchar **) new_values->pdata);

  g_ptr_array_unref (new_values);
}

static void
add_file_chooser_response (GtkDialog *widget,
                           GtkResponseType response,
                           gpointer user_data)
{
  Place *place;
  GPtrArray *new_values;

  if (response != GTK_RESPONSE_OK)
    {
      gtk_widget_destroy (GTK_WIDGET (widget));
      return;
    }

  place = g_slice_new0 (Place);
  place->location = gtk_file_chooser_get_file (GTK_FILE_CHOOSER (widget));
  place->settings_key = TRACKER_KEY_RECURSIVE_DIRECTORIES;
  place->display_name = g_file_get_basename (place->location);

  new_values = place_get_new_settings_values (place, FALSE);
  g_settings_set_strv (tracker_preferences, place->settings_key, (const gchar **) new_values->pdata);

  g_ptr_array_unref (new_values);
  gtk_widget_destroy (GTK_WIDGET (widget));
  place_free (place);
}

static void
add_button_clicked (GtkWidget *widget,
                    gpointer user_data)
{
  GtkWidget *list_box = user_data;
  GtkWidget *file_chooser;

  file_chooser = gtk_file_chooser_dialog_new (_("Select Location"),
                                              GTK_WINDOW (gtk_widget_get_toplevel (widget)),
                                              GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER,
                                              _("_Cancel"), GTK_RESPONSE_CANCEL,
                                              _("_OK"), GTK_RESPONSE_OK,
                                              NULL);
  gtk_window_set_modal (GTK_WINDOW (file_chooser), TRUE);
  g_signal_connect (file_chooser, "response",
                    G_CALLBACK (add_file_chooser_response), list_box);
  gtk_widget_show (file_chooser);
}

static void
locations_dialog_refresh (GtkWidget *list_box)
{
  gtk_container_foreach (GTK_CONTAINER (list_box), (GtkCallback) gtk_widget_destroy, NULL);
  populate_list_box (list_box);
}

static void
locations_dialog_destroy (GtkWidget *widget)
{
  g_clear_object (&tracker_preferences);
}

GtkWidget *
cc_search_locations_dialog_new (CcSearchPanel *self)
{
  GtkWidget *locations_dialog, *widget, *list_box;
  GtkBuilder *dialog_builder;
  GError *error = NULL;

  dialog_builder = gtk_builder_new ();
  gtk_builder_add_from_resource (dialog_builder,
                                 "/org/gnome/control-center/search/search-locations-dialog.ui",
                                 &error);

  if (error != NULL)
    {
      g_warning ("Could not load interface file: %s", error->message);
      g_error_free (error);
      return NULL;
    }

  tracker_preferences = g_settings_new (TRACKER_SCHEMA);

  locations_dialog = GTK_WIDGET (gtk_builder_get_object (dialog_builder, "locations_dialog"));
  widget = GTK_WIDGET (gtk_builder_get_object (dialog_builder, "locations_scrolledwindow"));
  list_box = GTK_WIDGET (gtk_list_box_new ());
  gtk_container_add (GTK_CONTAINER (widget), list_box);
  gtk_list_box_set_sort_func (GTK_LIST_BOX (list_box),
                              (GtkListBoxSortFunc)place_compare_func, NULL, NULL);
  gtk_list_box_set_header_func (GTK_LIST_BOX (list_box),
                                place_header_func, NULL, NULL);
  gtk_widget_show (list_box);

  widget = GTK_WIDGET (gtk_builder_get_object (dialog_builder, "locations_remove"));
  gtk_widget_set_sensitive (widget, FALSE);
  g_signal_connect (list_box, "row-selected",
                    G_CALLBACK (list_box_row_selected), widget);
  g_signal_connect (widget, "clicked",
                    G_CALLBACK (remove_button_clicked), list_box);
  g_signal_connect_swapped (tracker_preferences, "changed::" TRACKER_KEY_RECURSIVE_DIRECTORIES,
                            G_CALLBACK (locations_dialog_refresh), list_box);

  widget = GTK_WIDGET (gtk_builder_get_object (dialog_builder, "locations_add"));
  g_signal_connect (widget, "clicked",
                    G_CALLBACK (add_button_clicked), list_box);

  gtk_window_set_transient_for (GTK_WINDOW (locations_dialog),
                                GTK_WINDOW (gtk_widget_get_toplevel (GTK_WIDGET (self))));
  g_signal_connect (locations_dialog, "response",
                    G_CALLBACK (gtk_widget_destroy), NULL);
  g_signal_connect (locations_dialog, "destroy",
                    G_CALLBACK (locations_dialog_destroy), NULL);

  populate_list_box (list_box);

  g_object_unref (dialog_builder);

  return locations_dialog;
}

gboolean
cc_search_locations_dialog_is_available (void)
{
  GSettingsSchemaSource *source;
  GSettingsSchema *schema;

  source = g_settings_schema_source_get_default ();
  if (!source)
    return FALSE;

  schema = g_settings_schema_source_lookup (source, TRACKER_SCHEMA, TRUE);
  if (!schema)
    return FALSE;

  g_settings_schema_unref (schema);
  return TRUE;
}
