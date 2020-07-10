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
#include "list-box-helper.h"

#include <glib/gi18n.h>

#define TRACKER_SCHEMA "org.freedesktop.Tracker.Miner.Files"
#define TRACKER_KEY_RECURSIVE_DIRECTORIES "index-recursive-directories"
#define TRACKER_KEY_SINGLE_DIRECTORIES "index-single-directories"

typedef enum {
  PLACE_XDG,
  PLACE_BOOKMARKS,
  PLACE_OTHER
} PlaceType;

typedef struct {
  CcSearchLocationsDialog *dialog;
  GFile *location;
  gchar *display_name;
  PlaceType place_type;
  GCancellable *cancellable;
  const gchar *settings_key;
} Place;

struct _CcSearchLocationsDialog {
  GtkDialog parent;

  GSettings *tracker_preferences;

  GtkWidget *places_list;
  GtkWidget *bookmarks_list;
  GtkWidget *others_list;
  GtkWidget *locations_add;
};

struct _CcSearchLocationsDialogClass {
  GtkDialogClass parent_class;
};

G_DEFINE_TYPE (CcSearchLocationsDialog, cc_search_locations_dialog, GTK_TYPE_DIALOG)

static void
cc_search_locations_dialog_finalize (GObject *object)
{
  CcSearchLocationsDialog *self = CC_SEARCH_LOCATIONS_DIALOG (object);

  g_clear_object (&self->tracker_preferences);

  G_OBJECT_CLASS (cc_search_locations_dialog_parent_class)->finalize (object);
}

static void
cc_search_locations_dialog_init (CcSearchLocationsDialog *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));
}

static Place *
place_new (CcSearchLocationsDialog *dialog,
           GFile *location,
           gchar *display_name,
           PlaceType place_type)
{
  Place *new_place = g_new0 (Place, 1);

  new_place->dialog = dialog;
  new_place->location = location;
  if (display_name != NULL)
    new_place->display_name = display_name;
  else
    new_place->display_name = g_file_get_basename (location);
  if (g_strcmp0 (g_file_get_path (location), g_get_home_dir ()) == 0)
    new_place->settings_key = TRACKER_KEY_SINGLE_DIRECTORIES;
  else
    new_place->settings_key = TRACKER_KEY_RECURSIVE_DIRECTORIES;
  new_place->place_type = place_type;

  return new_place;
}

static void
place_free (Place * p)
{
  g_cancellable_cancel (p->cancellable);
  g_clear_object (&p->cancellable);

  g_object_unref (p->location);
  g_free (p->display_name);

  g_free (p);
}

G_DEFINE_AUTOPTR_CLEANUP_FUNC (Place, place_free)

static GList *
get_bookmarks (CcSearchLocationsDialog *self)
{
  g_autoptr(GFile) file = NULL;
  g_autofree gchar *contents = NULL;
  g_autofree gchar *path = NULL;
  GList *bookmarks = NULL;
  GError *error = NULL;

  path = g_build_filename (g_get_user_config_dir (), "gtk-3.0",
                           "bookmarks", NULL);
  file = g_file_new_for_path (path);
  if (g_file_load_contents (file, NULL, &contents, NULL, NULL, &error))
    {
      gint idx;
      g_auto(GStrv) lines = NULL;

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

              bookmark = place_new (self,
                                    g_file_new_for_uri (lines[idx]),
                                    label,
                                    PLACE_BOOKMARKS);

              bookmarks = g_list_prepend (bookmarks, bookmark);
            }
	}
    }

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
get_xdg_dirs (CcSearchLocationsDialog *self)
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

      xdg_dir = place_new (self,
                           g_file_new_for_path (path),
                           NULL,
                           PLACE_XDG);

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

static GPtrArray *
place_get_new_settings_values (CcSearchLocationsDialog *self,
                               Place *place,
                               gboolean remove)
{
  g_auto(GStrv) values = NULL;
  g_autofree gchar *path = NULL;
  GPtrArray *new_values;
  const gchar *tracker_dir;
  gboolean found;
  gint idx;

  new_values = g_ptr_array_new_with_free_func (g_free);
  values = g_settings_get_strv (self->tracker_preferences, place->settings_key);
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

  return new_values;
}


static GList *
get_tracker_locations (CcSearchLocationsDialog *self)
{
  g_auto(GStrv) locations = NULL;
  GFile *file;
  GList *list;
  gint idx;
  Place *location;
  const gchar *path;

  locations = g_settings_get_strv (self->tracker_preferences, TRACKER_KEY_RECURSIVE_DIRECTORIES);
  list = NULL;

  for (idx = 0; locations[idx] != NULL; idx++)
    {
      path = path_from_tracker_dir (locations[idx]);

      file = g_file_new_for_commandline_arg (path);
      location = place_new (self,
                            file,
                            NULL,
                            PLACE_OTHER);

      if (file != NULL && g_file_query_exists (file, NULL))
        {
          list = g_list_prepend (list, location);
        }
      else
        {
          g_autoptr(GPtrArray) new_values = NULL;

          new_values = place_get_new_settings_values (self, location, TRUE);
          g_settings_set_strv (self->tracker_preferences,
                               TRACKER_KEY_RECURSIVE_DIRECTORIES,
                               (const gchar **) new_values->pdata);
        }
    }

  return g_list_reverse (list);
}

static GList *
get_places_list (CcSearchLocationsDialog *self)
{
  g_autoptr(GList) xdg_list = NULL;
  g_autoptr(GList) tracker_list = NULL;
  g_autoptr(GList) bookmark_list = NULL;
  GList *l;
  g_autoptr(GHashTable) places = NULL;
  Place *place, *old_place;
  GList *places_list;

  places = g_hash_table_new_full (g_file_hash, (GEqualFunc) g_file_equal, NULL, (GDestroyNotify) place_free);

  /* add home */
  place = place_new (self,
                     g_file_new_for_path (g_get_home_dir ()),
                     g_strdup (_("Home")),
                     PLACE_XDG);
  g_hash_table_insert (places, place->location, place);

  /* first, load the XDG dirs */
  xdg_list = get_xdg_dirs (self);
  for (l = xdg_list; l != NULL; l = l->next)
    {
      place = l->data;
      g_hash_table_insert (places, place->location, place);
    }

  /* then, insert all the tracker locations that are not XDG dirs */
  tracker_list = get_tracker_locations (self);
  for (l = tracker_list; l != NULL; l = l->next)
    {
      g_autoptr(Place) p = l->data;
      old_place = g_hash_table_lookup (places, p->location);
      if (old_place == NULL)
        {
          g_hash_table_insert (places, p->location, p);
          g_steal_pointer (&p);
        }
    }

  /* finally, load bookmarks, and possibly update attributes */
  bookmark_list = get_bookmarks (self);
  for (l = bookmark_list; l != NULL; l = l->next)
    {
      g_autoptr(Place) p = l->data;
      old_place = g_hash_table_lookup (places, p->location);
      if (old_place == NULL)
        {
          g_hash_table_insert (places, p->location, p);
          g_steal_pointer (&p);
        }
      else
        {
          g_free (old_place->display_name);
          old_place->display_name = g_strdup (p->display_name);

          if (old_place->place_type == PLACE_OTHER)
            old_place->place_type = PLACE_BOOKMARKS;
        }
    }

  places_list = g_hash_table_get_values (places);
  g_hash_table_steal_all (places);

  return places_list;
}

static gboolean
switch_tracker_get_mapping (GValue *value,
                            GVariant *variant,
                            gpointer user_data)
{
  Place *place = user_data;
  g_autofree const gchar **locations = NULL;
  GFile *location;
  gint idx;
  gboolean found;

  found = FALSE;
  locations = g_variant_get_strv (variant, NULL);
  for (idx = 0; locations[idx] != NULL; idx++)
    {
      location = g_file_new_for_path (path_from_tracker_dir(locations[idx]));
      found = g_file_equal (location, place->location);
      g_object_unref (location);

      if (found)
        break;
    }

  g_value_set_boolean (value, found);
  return TRUE;
}

static GVariant *
switch_tracker_set_mapping (const GValue *value,
                            const GVariantType *expected_type,
                            gpointer user_data)
{
  Place *place = user_data;
  g_autoptr(GPtrArray) new_values = NULL;
  gboolean remove;

  remove = !g_value_get_boolean (value);
  new_values = place_get_new_settings_values (place->dialog, place, remove);
  return g_variant_new_strv ((const gchar **) new_values->pdata, -1);
}

static void
place_query_info_ready (GObject *source,
                        GAsyncResult *res,
                        gpointer user_data)
{
  GtkWidget *row, *box, *w;
  Place *place;
  g_autoptr(GFileInfo) info = NULL;
  g_autofree gchar *path = NULL;

  info = g_file_query_info_finish (G_FILE (source), res, NULL);
  if (!info)
    return;

  row = user_data;
  place = g_object_get_data (G_OBJECT (row), "place");
  g_clear_object (&place->cancellable);

  box = gtk_bin_get_child (GTK_BIN (row));
  gtk_widget_show (box);

  w = gtk_label_new (place->display_name);
  gtk_widget_show (w);
  gtk_container_add (GTK_CONTAINER (box), w);

  w = gtk_switch_new ();
  gtk_widget_show (w);
  gtk_widget_set_valign (w, GTK_ALIGN_CENTER);
  gtk_box_pack_end (GTK_BOX (box), w, FALSE, FALSE, 0);
  g_settings_bind_with_mapping (place->dialog->tracker_preferences, place->settings_key,
                                w, "active",
                                G_SETTINGS_BIND_DEFAULT,
                                switch_tracker_get_mapping,
                                switch_tracker_set_mapping,
                                place, NULL);
}

static void
remove_button_clicked (CcSearchLocationsDialog *self,
                       GtkWidget *button)
{
  g_autoptr(GPtrArray) new_values = NULL;
  Place *place;

  place = g_object_get_data (G_OBJECT (button), "place");
  new_values = place_get_new_settings_values (self, place, TRUE);
  g_settings_set_strv (self->tracker_preferences, place->settings_key, (const gchar **) new_values->pdata);
}

static gint
place_compare_func (gconstpointer a,
                    gconstpointer b,
                    gpointer user_data)
{
  GtkWidget *child_a, *child_b;
  Place *place_a, *place_b;
  g_autofree gchar *path = NULL;
  gboolean is_home;

  child_a = GTK_WIDGET (a);
  child_b = GTK_WIDGET (b);

  place_a = g_object_get_data (G_OBJECT (child_a), "place");
  place_b = g_object_get_data (G_OBJECT (child_b), "place");

  path = g_file_get_path (place_a->location);
  is_home = (g_strcmp0 (path, g_get_home_dir ()) == 0);

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
create_row_for_place (CcSearchLocationsDialog *self, Place *place)
{
  GtkWidget *child, *row, *remove_button;

  row = gtk_list_box_row_new ();
  gtk_widget_show (row);
  gtk_list_box_row_set_selectable (GTK_LIST_BOX_ROW (row), FALSE);
  gtk_list_box_row_set_activatable (GTK_LIST_BOX_ROW (row), FALSE);
  child = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 6);
  gtk_widget_show (child);
  gtk_container_add (GTK_CONTAINER (row), child);
  g_object_set (row, "margin", 5, "margin-left", 16, NULL);
  g_object_set_data_full (G_OBJECT (row), "place", place, (GDestroyNotify) place_free);

  if (place->place_type == PLACE_OTHER)
    {
      remove_button = gtk_button_new_from_icon_name ("window-close-symbolic", GTK_ICON_SIZE_MENU);
      gtk_widget_show (remove_button);
      g_object_set_data (G_OBJECT (remove_button), "place", place);
      gtk_style_context_add_class (gtk_widget_get_style_context (remove_button), "flat");
      gtk_box_pack_end (GTK_BOX (child), remove_button, FALSE, FALSE, 2);

      g_signal_connect_swapped (remove_button, "clicked",
                                G_CALLBACK (remove_button_clicked), self);
    }

  place->cancellable = g_cancellable_new ();
  g_file_query_info_async (place->location, G_FILE_ATTRIBUTE_STANDARD_DISPLAY_NAME,
                           G_FILE_QUERY_INFO_NONE, G_PRIORITY_DEFAULT,
                           place->cancellable, place_query_info_ready, row);

  return row;
}

static void
populate_list_boxes (CcSearchLocationsDialog *self)
{
  g_autoptr(GList) places = NULL;
  GList *l;
  Place *place;
  GtkWidget *row;

  places = get_places_list (self);
  for (l = places; l != NULL; l = l->next)
    {
      place = l->data;
      row = create_row_for_place (self, place);

      switch (place->place_type)
        {
          case PLACE_XDG:
            gtk_container_add (GTK_CONTAINER (self->places_list), row);
            break;
          case PLACE_BOOKMARKS:
            gtk_container_add (GTK_CONTAINER (self->bookmarks_list), row);
            break;
          case PLACE_OTHER:
            gtk_container_add (GTK_CONTAINER (self->others_list), row);
            break;
          default:
            g_assert_not_reached ();
        }
    }
}

static void
add_file_chooser_response (CcSearchLocationsDialog *self,
                           GtkResponseType response,
                           GtkWidget *widget)
{
  g_autoptr(Place) place = NULL;
  g_autoptr(GPtrArray) new_values = NULL;

  if (response != GTK_RESPONSE_OK)
    {
      gtk_widget_destroy (GTK_WIDGET (widget));
      return;
    }

  place = place_new (self,
                     gtk_file_chooser_get_file (GTK_FILE_CHOOSER (widget)),
                     NULL,
                     0);

  place->settings_key = TRACKER_KEY_RECURSIVE_DIRECTORIES;

  new_values = place_get_new_settings_values (self, place, FALSE);
  g_settings_set_strv (self->tracker_preferences, place->settings_key, (const gchar **) new_values->pdata);

  gtk_widget_destroy (GTK_WIDGET (widget));
}

static void
add_button_clicked (CcSearchLocationsDialog *self)
{
  GtkWidget *file_chooser;

  file_chooser = gtk_file_chooser_dialog_new (_("Select Location"),
                                              GTK_WINDOW (gtk_widget_get_toplevel (GTK_WIDGET (self))),
                                              GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER,
                                              _("_Cancel"), GTK_RESPONSE_CANCEL,
                                              _("_OK"), GTK_RESPONSE_OK,
                                              NULL);
  gtk_window_set_modal (GTK_WINDOW (file_chooser), TRUE);
  g_signal_connect_swapped (file_chooser, "response",
                            G_CALLBACK (add_file_chooser_response), self);
  gtk_widget_show (file_chooser);
}

static void
other_places_refresh (CcSearchLocationsDialog *self)
{
  g_autoptr(GList) places = NULL;
  GList *l;
  Place *place;
  GtkWidget *row;

  gtk_container_foreach (GTK_CONTAINER (self->others_list), (GtkCallback) gtk_widget_destroy, NULL);

  places = get_places_list (self);
  for (l = places; l != NULL; l = l->next)
    {
      place = l->data;
      if (place->place_type != PLACE_OTHER)
        continue;

      row = create_row_for_place (self, place);
      gtk_container_add (GTK_CONTAINER (self->others_list), row);
    }
}

CcSearchLocationsDialog *
cc_search_locations_dialog_new (CcSearchPanel *panel)
{
  CcSearchLocationsDialog *self;

  self = g_object_new (CC_SEARCH_LOCATIONS_DIALOG_TYPE,
                       "use-header-bar", TRUE,
                       NULL);

  self->tracker_preferences = g_settings_new (TRACKER_SCHEMA);
  populate_list_boxes (self);

  gtk_list_box_set_sort_func (GTK_LIST_BOX (self->others_list),
                              (GtkListBoxSortFunc)place_compare_func, NULL, NULL);

  gtk_list_box_set_header_func (GTK_LIST_BOX (self->others_list), cc_list_box_update_header_func, NULL, NULL);

  g_signal_connect_swapped (self->tracker_preferences, "changed::" TRACKER_KEY_RECURSIVE_DIRECTORIES,
                            G_CALLBACK (other_places_refresh), self);

  gtk_window_set_transient_for (GTK_WINDOW (self),
                                GTK_WINDOW (gtk_widget_get_toplevel (GTK_WIDGET (panel))));

  return self;
}

gboolean
cc_search_locations_dialog_is_available (void)
{
  GSettingsSchemaSource *source;
  g_autoptr(GSettingsSchema) schema = NULL;

  source = g_settings_schema_source_get_default ();
  if (!source)
    return FALSE;

  schema = g_settings_schema_source_lookup (source, TRACKER_SCHEMA, TRUE);
  return schema != NULL;
}

static void
cc_search_locations_dialog_class_init (CcSearchLocationsDialogClass *klass)
{
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);
  GObjectClass   *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = cc_search_locations_dialog_finalize;

  gtk_widget_class_set_template_from_resource (widget_class,
                                               "/org/gnome/control-center/search/cc-search-locations-dialog.ui");

  gtk_widget_class_bind_template_child (widget_class, CcSearchLocationsDialog, places_list);
  gtk_widget_class_bind_template_child (widget_class, CcSearchLocationsDialog, bookmarks_list);
  gtk_widget_class_bind_template_child (widget_class, CcSearchLocationsDialog, others_list);
  gtk_widget_class_bind_template_child (widget_class, CcSearchLocationsDialog, locations_add);

  gtk_widget_class_bind_template_callback (widget_class, add_button_clicked);
}
