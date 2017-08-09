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

#include "cc-search-panel.h"
#include "cc-search-locations-dialog.h"
#include "cc-search-resources.h"
#include "shell/list-box-helper.h"

#include <gio/gdesktopappinfo.h>
#include <glib/gi18n.h>

CC_PANEL_REGISTER (CcSearchPanel, cc_search_panel)

#define WID(s) GTK_WIDGET (gtk_builder_get_object (self->priv->builder, s))

struct _CcSearchPanelPrivate
{
  GtkBuilder *builder;
  GtkWidget  *list_box;
  GtkWidget  *notification;

  GCancellable *load_cancellable;
  GSettings  *search_settings;
  GHashTable *sort_order;

  CcSearchLocationsDialog  *locations_dialog;
};

#define SHELL_PROVIDER_GROUP "Shell Search Provider"

static gint
list_sort_func (gconstpointer a,
                gconstpointer b,
                gpointer user_data)
{
  CcSearchPanel *self = user_data;
  GAppInfo *app_a, *app_b;
  const gchar *id_a, *id_b;
  gint idx_a, idx_b;
  gpointer lookup;

  app_a = g_object_get_data (G_OBJECT (a), "app-info");
  app_b = g_object_get_data (G_OBJECT (b), "app-info");

  id_a = g_app_info_get_id (app_a);
  id_b = g_app_info_get_id (app_b);

  /* find the index of the application in the GSettings preferences */
  idx_a = -1;
  idx_b = -1;

  lookup = g_hash_table_lookup (self->priv->sort_order, id_a);
  if (lookup)
    idx_a = GPOINTER_TO_INT (lookup) - 1;

  lookup = g_hash_table_lookup (self->priv->sort_order, id_b);
  if (lookup)
    idx_b = GPOINTER_TO_INT (lookup) - 1;

  /* if neither app is found, use alphabetical order */
  if ((idx_a == -1) && (idx_b == -1))
    return g_utf8_collate (g_app_info_get_name (app_a), g_app_info_get_name (app_b));

  /* if app_a isn't found, it's sorted after app_b */
  if (idx_a == -1)
    return 1;

  /* if app_b isn't found, it's sorted after app_a */
  if (idx_b == -1)
    return -1;

  /* finally, if both apps are found, return their order in the list */
  return (idx_a - idx_b);
}

static void
search_panel_invalidate_sort_order (CcSearchPanel *self)
{
  gchar **sort_order;
  gint idx;

  g_hash_table_remove_all (self->priv->sort_order);
  sort_order = g_settings_get_strv (self->priv->search_settings, "sort-order");

  for (idx = 0; sort_order[idx] != NULL; idx++)
    g_hash_table_insert (self->priv->sort_order, g_strdup (sort_order[idx]), GINT_TO_POINTER (idx + 1));

  gtk_list_box_invalidate_sort (GTK_LIST_BOX (self->priv->list_box));
  g_strfreev (sort_order);
}

static gint
propagate_compare_func (gconstpointer a,
                        gconstpointer b,
                        gpointer user_data)
{
  CcSearchPanel *self = user_data;
  const gchar *key_a = a, *key_b = b;
  gint idx_a, idx_b;

  idx_a = GPOINTER_TO_INT (g_hash_table_lookup (self->priv->sort_order, key_a));
  idx_b = GPOINTER_TO_INT (g_hash_table_lookup (self->priv->sort_order, key_b));

  return (idx_a - idx_b);
}

static void
search_panel_propagate_sort_order (CcSearchPanel *self)
{
  GList *keys, *l;
  GPtrArray *sort_order;

  sort_order = g_ptr_array_new ();
  keys = g_hash_table_get_keys (self->priv->sort_order);
  keys = g_list_sort_with_data (keys, propagate_compare_func, self);

  for (l = keys; l != NULL; l = l->next)
    g_ptr_array_add (sort_order, l->data);

  g_ptr_array_add (sort_order, NULL);
  g_settings_set_strv (self->priv->search_settings, "sort-order",
                       (const gchar **) sort_order->pdata);

  g_ptr_array_unref (sort_order);
  g_list_free (keys);
}

static void
search_panel_set_no_providers (CcSearchPanel *self)
{
  GtkWidget *w;

  /* center the list box in the scrolled window */
  gtk_widget_set_valign (self->priv->list_box, GTK_ALIGN_CENTER);

  w = gtk_label_new (_("No applications found"));
  gtk_widget_show (w);

  gtk_container_add (GTK_CONTAINER (self->priv->list_box), w);
}

static void
settings_button_clicked (GtkWidget *widget,
                         gpointer user_data)
{
  CcSearchPanel *self = user_data;

  if (self->priv->locations_dialog == NULL)
    {
      self->priv->locations_dialog = cc_search_locations_dialog_new (self);
      g_object_add_weak_pointer (G_OBJECT (self->priv->locations_dialog),
                                 (gpointer *) &self->priv->locations_dialog);
    }

  gtk_window_present (GTK_WINDOW (self->priv->locations_dialog));
}

static GVariant *
switch_settings_mapping_set_generic (const GValue *value,
                                     const GVariantType *expected_type,
                                     GtkWidget *row,
                                     gboolean default_enabled)
{
  CcSearchPanel *self = g_object_get_data (G_OBJECT (row), "self");
  GAppInfo *app_info = g_object_get_data (G_OBJECT (row), "app-info");
  gchar **apps;
  GPtrArray *new_apps;
  gint idx;
  gboolean remove, found;
  GVariant *variant;

  remove = !!g_value_get_boolean (value) == !!default_enabled;
  found = FALSE;
  new_apps = g_ptr_array_new_with_free_func (g_free);
  apps = g_settings_get_strv (self->priv->search_settings,
                              default_enabled ? "disabled" : "enabled");

  for (idx = 0; apps[idx] != NULL; idx++)
    {
      if (g_strcmp0 (apps[idx], g_app_info_get_id (app_info)) == 0)
        {
          found = TRUE;

          if (remove)
            continue;
        }

      g_ptr_array_add (new_apps, g_strdup (apps[idx]));
    }

  if (!found && !remove)
    g_ptr_array_add (new_apps, g_strdup (g_app_info_get_id (app_info)));

  g_ptr_array_add (new_apps, NULL);

  variant = g_variant_new_strv ((const gchar **) new_apps->pdata, -1);
  g_ptr_array_unref (new_apps);
  g_strfreev (apps);

  return variant;
}

static GVariant *
switch_settings_mapping_set_default_enabled (const GValue *value,
                                             const GVariantType *expected_type,
                                             gpointer user_data)
{
  return switch_settings_mapping_set_generic (value, expected_type,
                                              user_data, TRUE);
}

static GVariant *
switch_settings_mapping_set_default_disabled (const GValue *value,
                                              const GVariantType *expected_type,
                                              gpointer user_data)
{
  return switch_settings_mapping_set_generic (value, expected_type,
                                              user_data, FALSE);
}

static gboolean
switch_settings_mapping_get_generic (GValue *value,
                                     GVariant *variant,
                                     GtkWidget *row,
                                     gboolean default_enabled)
{
  GAppInfo *app_info = g_object_get_data (G_OBJECT (row), "app-info");
  const gchar **apps;
  gint idx;
  gboolean found;

  found = FALSE;
  apps = g_variant_get_strv (variant, NULL);

  for (idx = 0; apps[idx] != NULL; idx++)
    {
      if (g_strcmp0 (apps[idx], g_app_info_get_id (app_info)) == 0)
        {
          found = TRUE;
          break;
        }
    }

  g_free (apps);
  g_value_set_boolean (value, !!default_enabled != !!found);

  return TRUE;
}

static gboolean
switch_settings_mapping_get_default_enabled (GValue *value,
                                             GVariant *variant,
                                             gpointer user_data)
{
  return switch_settings_mapping_get_generic (value, variant,
                                              user_data, TRUE);
}

static gboolean
switch_settings_mapping_get_default_disabled (GValue *value,
                                              GVariant *variant,
                                              gpointer user_data)
{
  return switch_settings_mapping_get_generic (value, variant,
                                              user_data, FALSE);
}

static void
update_row_position (GtkWidget *row,
                     gpointer   user_data)
{
  CcSearchPanel *self = CC_SEARCH_PANEL (user_data);
  const gchar *app_id;
  GAppInfo *app_info;
  gint idx;

  app_info = g_object_get_data (G_OBJECT (row), "app-info");
  app_id = g_app_info_get_id (app_info);
  idx = gtk_list_box_row_get_index (GTK_LIST_BOX_ROW (row));

  g_hash_table_replace (self->priv->sort_order, g_strdup (app_id), GINT_TO_POINTER (idx));
}

static GtkTargetEntry drag_target_entries[] = {
   { "GTK_LIST_BOX_ROW", GTK_TARGET_SAME_APP, 0 }
};

static void
drag_begin (GtkWidget      *widget,
            GdkDragContext *context,
            GtkListBox     *list_box)
{
  GtkWidget *row;
  GtkAllocation alloc;
  cairo_surface_t *surface;
  cairo_t *cr;
  int x, y;

  /* Lets invalidate the list box sorting so we can move rows around.
   * We don't drop the sorting entirely because you want the list to be
   * sorted when there is no previous sorting stored. */
  gtk_list_box_set_sort_func (list_box, NULL, NULL, NULL);

  row = gtk_widget_get_ancestor (widget, GTK_TYPE_LIST_BOX_ROW);
  gtk_widget_get_allocation (row, &alloc);
  surface = gdk_window_create_similar_surface (gtk_widget_get_window (widget),
                                               CAIRO_CONTENT_COLOR_ALPHA,
                                               alloc.width, alloc.height);
  cr = cairo_create (surface);

  gtk_style_context_add_class (gtk_widget_get_style_context (row), "drag-icon");
  gtk_widget_draw (row, cr);
  gtk_style_context_remove_class (gtk_widget_get_style_context (row), "drag-icon");

  gtk_widget_translate_coordinates (widget, row, 0, 0, &x, &y);
  cairo_surface_set_device_offset (surface, -x, -y);
  gtk_drag_set_icon_surface (context, surface);

  cairo_destroy (cr);
  cairo_surface_destroy (surface);

  g_object_set_data (G_OBJECT (gtk_widget_get_parent (row)), "drag-row", row);
  gtk_style_context_add_class (gtk_widget_get_style_context (row), "draw-row");
}

static void
drag_end (GtkWidget      *widget,
          GdkDragContext *context,
          CcSearchPanel  *self)
{
  GtkWidget *row;

  row = gtk_widget_get_ancestor (widget, GTK_TYPE_LIST_BOX_ROW);
  g_object_set_data (G_OBJECT (gtk_widget_get_parent (row)), "drag-row", NULL);
  gtk_style_context_remove_class (gtk_widget_get_style_context (row), "drag-row");
  gtk_style_context_remove_class (gtk_widget_get_style_context (row), "drag-hover");

  /* Lets undo the things done in drag_begin. */
  gtk_list_box_set_sort_func (GTK_LIST_BOX (self->priv->list_box),
                              (GtkListBoxSortFunc)list_sort_func, self, NULL);
}

static void
drag_data_get (GtkWidget        *widget,
               GdkDragContext   *context,
               GtkSelectionData *selection_data,
               guint             info,
               guint             time,
               gpointer          data)
{
  gtk_selection_data_set (selection_data,
                          gdk_atom_intern_static_string ("GTK_LIST_BOX_ROW"),
                          32,
                          (const guchar *)&widget,
                          sizeof (gpointer));
}

static GtkListBoxRow *
get_last_row (CcSearchPanel *self)
{
  guint pos = g_hash_table_size (self->priv->sort_order) - 1;

  return gtk_list_box_get_row_at_index (GTK_LIST_BOX (self->priv->list_box), pos);
}

static GtkListBoxRow *
get_row_before (GtkListBox *list,
                GtkListBoxRow *row)
{
  int pos = gtk_list_box_row_get_index (row);

  return gtk_list_box_get_row_at_index (list, pos - 1);
}

static GtkListBoxRow *
get_row_after (GtkListBox *list,
               GtkListBoxRow *row)
{
  int pos = gtk_list_box_row_get_index (row);

  return gtk_list_box_get_row_at_index (list, pos + 1);
}

static void
drag_data_received (GtkWidget        *widget,
                    GdkDragContext   *context,
                    gint              x,
                    gint              y,
                    GtkSelectionData *selection_data,
                    guint             info,
                    guint32           time,
                    gpointer          data)
{
  CcSearchPanel *self = CC_SEARCH_PANEL (data);
  GtkWidget *row_before;
  GtkWidget *row_after;
  GtkWidget *row;
  GtkWidget *source;
  gboolean success = TRUE;
  int pos;

  row_before = GTK_WIDGET (g_object_get_data (G_OBJECT (widget), "row-before"));
  row_after = GTK_WIDGET (g_object_get_data (G_OBJECT (widget), "row-after"));

  g_object_set_data (G_OBJECT (widget), "row-before", NULL);
  g_object_set_data (G_OBJECT (widget), "row-after", NULL);

  if (row_before)
    gtk_style_context_remove_class (gtk_widget_get_style_context (row_before), "drag-hover-bottom");
  if (row_after)
    gtk_style_context_remove_class (gtk_widget_get_style_context (row_after), "drag-hover-top");

  row = (gpointer)* (gpointer*)gtk_selection_data_get_data (selection_data);
  source = gtk_widget_get_ancestor (row, GTK_TYPE_LIST_BOX_ROW);

  if (source == row_after)
    {
      success = FALSE;
      goto out;
    }

  g_object_ref (source);
  gtk_container_remove (GTK_CONTAINER (gtk_widget_get_parent (source)), source);

  if (row_after)
    pos = gtk_list_box_row_get_index (GTK_LIST_BOX_ROW (row_after));
  else
    pos = gtk_list_box_row_get_index (GTK_LIST_BOX_ROW (row_before)) + 1;


  gtk_list_box_insert (GTK_LIST_BOX (widget), source, pos);
  g_object_unref (source);

  gtk_container_foreach (GTK_CONTAINER (self->priv->list_box), update_row_position, self);

  search_panel_propagate_sort_order (self);

out:
  gtk_drag_finish (context, success, TRUE, time);
}

static gboolean
drag_motion (GtkWidget *widget,
             GdkDragContext *context,
             int x,
             int y,
             guint time,
             gpointer data)
{
  CcSearchPanel *self = CC_SEARCH_PANEL (data);
  GtkAllocation alloc;
  GtkWidget *row;
  int hover_row_y;
  int hover_row_height;
  GtkWidget *drag_row;
  GtkWidget *row_before;
  GtkWidget *row_after;

  row = GTK_WIDGET (gtk_list_box_get_row_at_y (GTK_LIST_BOX (widget), y));

  drag_row = GTK_WIDGET (g_object_get_data (G_OBJECT (widget), "drag-row"));
  row_before = GTK_WIDGET (g_object_get_data (G_OBJECT (widget), "row-before"));
  row_after = GTK_WIDGET (g_object_get_data (G_OBJECT (widget), "row-after"));

  gtk_style_context_remove_class (gtk_widget_get_style_context (drag_row), "drag-hover");
  if (row_before)
    gtk_style_context_remove_class (gtk_widget_get_style_context (row_before), "drag-hover-bottom");
  if (row_after)
    gtk_style_context_remove_class (gtk_widget_get_style_context (row_after), "drag-hover-top");

  if (row)
    {
      gtk_widget_get_allocation (row, &alloc);
      hover_row_y = alloc.y;
      hover_row_height = alloc.height;

      if (y < hover_row_y + hover_row_height/2)
        {
          row_after = row;
          row_before = GTK_WIDGET (get_row_before (GTK_LIST_BOX (widget), GTK_LIST_BOX_ROW (row)));
        }
      else
        {
          row_before = row;
          row_after = GTK_WIDGET (get_row_after (GTK_LIST_BOX (widget), GTK_LIST_BOX_ROW (row)));
        }
    }
  else
    {
      row_before = GTK_WIDGET (get_last_row (self));
      row_after = NULL;
    }

  g_object_set_data (G_OBJECT (widget), "row-before", row_before);
  g_object_set_data (G_OBJECT (widget), "row-after", row_after);

  if (drag_row == row_before || drag_row == row_after)
    {
      gtk_style_context_add_class (gtk_widget_get_style_context (drag_row), "drag-hover");
      return FALSE;
    }

  if (row_before)
    gtk_style_context_add_class (gtk_widget_get_style_context (row_before), "drag-hover-bottom");
  if (row_after)
    gtk_style_context_add_class (gtk_widget_get_style_context (row_after), "drag-hover-top");

  return TRUE;
}

static void
drag_leave (GtkWidget *widget,
            GdkDragContext *context,
            guint time)
{
  GtkWidget *drag_row;
  GtkWidget *row_before;
  GtkWidget *row_after;

  drag_row = GTK_WIDGET (g_object_get_data (G_OBJECT (widget), "drag-row"));
  row_before = GTK_WIDGET (g_object_get_data (G_OBJECT (widget), "row-before"));
  row_after = GTK_WIDGET (g_object_get_data (G_OBJECT (widget), "row-after"));

  gtk_style_context_remove_class (gtk_widget_get_style_context (drag_row), "drag-hover");
  if (row_before)
    gtk_style_context_remove_class (gtk_widget_get_style_context (row_before), "drag-hover-bottom");
  if (row_after)
    gtk_style_context_remove_class (gtk_widget_get_style_context (row_after), "drag-hover-top");
}

static void
swap_rows (GtkWidget *source,
           GtkWidget *target,
           GtkWidget *list_box)
{
  int index;

  index = gtk_list_box_row_get_index (GTK_LIST_BOX_ROW (target));

  g_object_ref (source);
  gtk_container_remove (GTK_CONTAINER (list_box), source);
  gtk_list_box_insert (GTK_LIST_BOX (list_box), source, index);
  g_object_unref (source);

  gtk_list_box_select_row (GTK_LIST_BOX (list_box), GTK_LIST_BOX_ROW (source));
  gtk_widget_grab_focus (source);
}

static gboolean
drag_key_press (GtkListBoxRow *row,
                GdkEventKey   *event,
                CcSearchPanel *self)
{
  GtkWidget *target;
  guint index;

  index = gtk_list_box_row_get_index (row);

  if ((event->state & GDK_MOD1_MASK) != 0 && event->keyval == GDK_KEY_Up)
    index--;
  else if ((event->state & GDK_MOD1_MASK) != 0 && event->keyval == GDK_KEY_Down)
    index++;
  else
    return GDK_EVENT_PROPAGATE;

  target = GTK_WIDGET (gtk_list_box_get_row_at_index (GTK_LIST_BOX (self->priv->list_box), index));
  if (target) {
    gtk_list_box_set_sort_func (GTK_LIST_BOX (self->priv->list_box), NULL, NULL, NULL);

    swap_rows (GTK_WIDGET (row), target, self->priv->list_box);

    gtk_container_foreach (GTK_CONTAINER (self->priv->list_box), update_row_position, self);
    search_panel_propagate_sort_order (self);

    gtk_list_box_set_sort_func (GTK_LIST_BOX (self->priv->list_box),
                                (GtkListBoxSortFunc)list_sort_func, self, NULL);
  }

  return GDK_EVENT_STOP;
}

static void
search_panel_add_one_app_info (CcSearchPanel *self,
                               GAppInfo *app_info,
                               gboolean default_enabled)
{
  GtkWidget *row, *box, *handle, *w;
  GIcon *icon;
  gint width, height;

  /* gnome-control-center is special cased in the shell,
     and is not configurable */
  if (g_strcmp0 (g_app_info_get_id (app_info),
                 "gnome-control-center.desktop") == 0)
    return;

  /* reset valignment of the list box */
  gtk_widget_set_valign (self->priv->list_box, GTK_ALIGN_FILL);

  row = gtk_list_box_row_new ();
  box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 10);
  gtk_container_add (GTK_CONTAINER (row), box);
  gtk_widget_set_hexpand (box, TRUE);
  gtk_container_set_border_width (GTK_CONTAINER (box), 10);
  g_object_set_data_full (G_OBJECT (row), "app-info",
                          g_object_ref (app_info), g_object_unref);
  g_object_set_data (G_OBJECT (row), "self", self);
  gtk_container_add (GTK_CONTAINER (self->priv->list_box), row);
  g_signal_connect (row, "key-press-event", G_CALLBACK (drag_key_press), self);

  /* Drag and Drop */
  handle = gtk_event_box_new ();
  gtk_container_add (GTK_CONTAINER (handle),
                     gtk_image_new_from_icon_name ("open-menu-symbolic", GTK_ICON_SIZE_MENU));
  gtk_container_add (GTK_CONTAINER (box), handle);

  gtk_style_context_add_class (gtk_widget_get_style_context (row), "row");
  gtk_drag_source_set (handle, GDK_BUTTON1_MASK, drag_target_entries, 1, GDK_ACTION_MOVE);
  g_signal_connect (handle, "drag-begin", G_CALLBACK (drag_begin), self->priv->list_box);
  g_signal_connect (handle, "drag-end", G_CALLBACK (drag_end), self);
  g_signal_connect (handle, "drag-data-get", G_CALLBACK (drag_data_get), NULL);

  icon = g_app_info_get_icon (app_info);
  if (icon == NULL)
    icon = g_themed_icon_new ("application-x-executable");
  else
    g_object_ref (icon);

  w = gtk_image_new_from_gicon (icon, GTK_ICON_SIZE_DND);
  gtk_icon_size_lookup (GTK_ICON_SIZE_DND, &width, &height);
  gtk_image_set_pixel_size (GTK_IMAGE (w), MAX (width, height));
  gtk_container_add (GTK_CONTAINER (box), w);
  g_object_unref (icon);

  w = gtk_label_new (g_app_info_get_name (app_info));
  gtk_container_add (GTK_CONTAINER (box), w);

  w = gtk_switch_new ();
  gtk_widget_set_valign (w, GTK_ALIGN_CENTER);
  gtk_box_pack_end (GTK_BOX (box), w, FALSE, FALSE, 0);

  if (default_enabled)
    {
      g_settings_bind_with_mapping (self->priv->search_settings, "disabled",
                                    w, "active",
                                    G_SETTINGS_BIND_DEFAULT,
                                    switch_settings_mapping_get_default_enabled,
                                    switch_settings_mapping_set_default_enabled,
                                    row, NULL);
    }
  else
    {
      g_settings_bind_with_mapping (self->priv->search_settings, "enabled",
                                    w, "active",
                                    G_SETTINGS_BIND_DEFAULT,
                                    switch_settings_mapping_get_default_disabled,
                                    switch_settings_mapping_set_default_disabled,
                                    row, NULL);
    }

  gtk_widget_show_all (row);
}

static void
search_panel_add_one_provider (CcSearchPanel *self,
                               GFile *provider)
{
  gchar *path, *desktop_id;
  GKeyFile *keyfile;
  GAppInfo *app_info;
  GError *error = NULL;
  gboolean default_disabled;

  path = g_file_get_path (provider);
  keyfile = g_key_file_new ();
  g_key_file_load_from_file (keyfile, path, G_KEY_FILE_NONE, &error);

  if (error != NULL)
    {
      g_warning ("Error loading %s: %s - search provider will be ignored",
                 path, error->message);
      goto out;
    }

  if (!g_key_file_has_group (keyfile, SHELL_PROVIDER_GROUP))
    {
      g_debug ("Shell search provider group missing from '%s', ignoring", path);
      goto out;
    }

  desktop_id = g_key_file_get_string (keyfile, SHELL_PROVIDER_GROUP,
                                      "DesktopId", &error);

  if (error != NULL)
    {
      g_warning ("Unable to read desktop ID from %s: %s - search provider will be ignored",
                 path, error->message);
      goto out;
    }

  app_info = G_APP_INFO (g_desktop_app_info_new (desktop_id));

  if (app_info == NULL)
    {
      g_debug ("Could not find application with desktop ID '%s' referenced in '%s', ignoring",
               desktop_id, path);
      g_free (desktop_id);
      goto out;
    }

  g_free (desktop_id);
  default_disabled = g_key_file_get_boolean (keyfile, SHELL_PROVIDER_GROUP,
                                             "DefaultDisabled", NULL);
  search_panel_add_one_app_info (self, app_info, !default_disabled);
  g_object_unref (app_info);

 out:
  g_free (path);
  g_clear_error (&error);
  g_key_file_unref (keyfile);
}

static void
search_providers_discover_ready (GObject *source,
                                 GAsyncResult *result,
                                 gpointer user_data)
{
  GList *providers, *l;
  GFile *provider;
  CcSearchPanel *self = CC_SEARCH_PANEL (source);
  GError *error = NULL;

  providers = g_task_propagate_pointer (G_TASK (result), &error);

  if (g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
    {
      g_error_free (error);
      return;
    }

  g_clear_object (&self->priv->load_cancellable);

  if (providers == NULL)
    {
      search_panel_set_no_providers (self);
      return;
    }

  for (l = providers; l != NULL; l = l->next)
    {
      provider = l->data;
      search_panel_add_one_provider (self, provider);
      g_object_unref (provider);
    }

  /* propagate a write to GSettings, to make sure we always have
   * all the providers in the list.
   */
  search_panel_propagate_sort_order (self);
  g_list_free (providers);
}

static GList *
search_providers_discover_one_directory (const gchar *system_dir,
                                         GCancellable *cancellable)
{
  GList *providers = NULL;
  gchar *providers_path;
  GFile *providers_location, *provider;
  GFileInfo *info;
  GFileEnumerator *enumerator;
  GError *error = NULL;

  providers_path = g_build_filename (system_dir, "gnome-shell", "search-providers", NULL);
  providers_location = g_file_new_for_path (providers_path);

  enumerator = g_file_enumerate_children (providers_location,
                                          "standard::type,standard::name,standard::content-type",
                                          G_FILE_QUERY_INFO_NONE,
                                          cancellable, &error);

  if (error != NULL)
    {
      if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_NOT_FOUND) &&
          !g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
        g_warning ("Error opening %s: %s - search provider configuration won't be possible",
                   providers_path, error->message);
      g_clear_error (&error);

      goto out;
    }

  while ((info = g_file_enumerator_next_file (enumerator, cancellable, &error)) != NULL)
    {
      provider = g_file_get_child (providers_location, g_file_info_get_name (info));
      providers = g_list_prepend (providers, provider);
      g_object_unref (info);
    }

  if (error != NULL)
    {
      if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
        g_warning ("Error reading from %s: %s - search providers might be missing from the panel",
                   providers_path, error->message);
      g_clear_error (&error);
    }

 out:
  g_clear_object (&enumerator);
  g_clear_object (&providers_location);
  g_free (providers_path);

  return providers;
}

static void
search_providers_discover_thread (GTask *task,
                                  gpointer source_object,
                                  gpointer task_data,
                                  GCancellable *cancellable)
{
  GList *providers = NULL;
  const gchar * const *system_data_dirs;
  int idx;

  system_data_dirs = g_get_system_data_dirs ();
  for (idx = 0; system_data_dirs[idx] != NULL; idx++)
    {
      providers = g_list_concat (search_providers_discover_one_directory (system_data_dirs[idx], cancellable),
                                 providers);

      if (g_task_return_error_if_cancelled (task))
        {
          g_list_free_full (providers, g_object_unref);
          return;
        }
    }

  g_task_return_pointer (task, providers, NULL);
}

static void
populate_search_providers (CcSearchPanel *self)
{
  GTask *task;

  self->priv->load_cancellable = g_cancellable_new ();
  task = g_task_new (self, self->priv->load_cancellable,
                     search_providers_discover_ready, self);
  g_task_run_in_thread (task, search_providers_discover_thread);
  g_object_unref (task);
}

static void
on_row_selected (GtkListBox *list_box,
                 GtkListBoxRow *row,
                 CcSearchPanel *self)
{
  gtk_revealer_set_reveal_child (GTK_REVEALER (self->priv->notification), TRUE);
}

static void
cc_search_panel_dispose (GObject *object)
{
  CcSearchPanelPrivate *priv = CC_SEARCH_PANEL (object)->priv;

  if (priv->load_cancellable != NULL)
    g_cancellable_cancel (priv->load_cancellable);
  g_clear_object (&priv->load_cancellable);

  G_OBJECT_CLASS (cc_search_panel_parent_class)->dispose (object);
}

static void
cc_search_panel_finalize (GObject *object)
{
  CcSearchPanelPrivate *priv = CC_SEARCH_PANEL (object)->priv;

  g_clear_object (&priv->builder);
  g_clear_object (&priv->search_settings);
  g_hash_table_destroy (priv->sort_order);

  if (priv->locations_dialog)
    gtk_widget_destroy (GTK_WIDGET (priv->locations_dialog));

  G_OBJECT_CLASS (cc_search_panel_parent_class)->finalize (object);
}

static void
cc_search_panel_constructed (GObject *object)
{
  CcSearchPanel *self = CC_SEARCH_PANEL (object);
  GtkWidget *box, *widget, *search_box;

  G_OBJECT_CLASS (cc_search_panel_parent_class)->constructed (object);

  /* add the disable all switch */
  search_box = WID ("search_vbox");
  box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 6);

  widget = gtk_switch_new ();
  gtk_widget_set_valign (widget, GTK_ALIGN_CENTER);
  gtk_box_pack_start (GTK_BOX (box), widget, FALSE, FALSE, 4);

  g_settings_bind (self->priv->search_settings, "disable-external",
                   widget, "active",
                   G_SETTINGS_BIND_DEFAULT |
                   G_SETTINGS_BIND_INVERT_BOOLEAN);

  g_object_bind_property (widget, "active",
                          search_box, "sensitive",
                          G_BINDING_DEFAULT |
                          G_BINDING_SYNC_CREATE);

  gtk_widget_show_all (box);
  cc_shell_embed_widget_in_header (cc_panel_get_shell (CC_PANEL (self)), box);
}

static const char *css =
  ".row:not(:first-child) { "
  "  border-top: 1px solid alpha(gray,0.5); "
  "  border-bottom: 1px solid transparent; "
  "}"
  ".row:first-child { "
  "  border-top: 1px solid transparent; "
  "  border-bottom: 1px solid transparent; "
  "}"
  ".row:last-child { "
  "  border-top: 1px solid alpha(gray,0.5); "
  "  border-bottom: 1px solid alpha(gray,0.5); "
  "}"
  ".row.drag-icon { "
  "  background: @theme_base_color; "
  "  border: 1px solid @borders; "
  "}"
  ".row.drag-row { "
  "  color: gray; "
  "  background: alpha(gray,0.2); "
  "}"
  ".row.drag-hover image, "
  ".row.drag-hover label { "
  "  color: @theme_text_color; "
  "}"
  ".row.drag-hover-top {"
  "  border-top: 48px solid @theme_bg_color; "
  "}"
  ".row.drag-hover-bottom {"
  "  border-bottom: 1px solid @theme_bg_color; "
  "}"
;

static void
cc_search_panel_init (CcSearchPanel *self)
{
  GtkCssProvider *provider;
  GError    *error;
  GtkWidget *widget;
  GtkWidget *frame;
  guint res;

  self->priv = G_TYPE_INSTANCE_GET_PRIVATE (self, CC_TYPE_SEARCH_PANEL, CcSearchPanelPrivate);
  g_resources_register (cc_search_get_resource ());

  self->priv->builder = gtk_builder_new ();

  error = NULL;
  res = gtk_builder_add_from_resource (self->priv->builder,
                                       "/org/gnome/control-center/search/search.ui",
                                       &error);

  if (res == 0)
    {
      g_warning ("Could not load interface file: %s",
                 (error != NULL) ? error->message : "unknown error");
      g_clear_error (&error);
      return;
    }

  frame = WID ("search_frame");
  widget = GTK_WIDGET (gtk_list_box_new ());
  gtk_list_box_set_sort_func (GTK_LIST_BOX (widget),
                              (GtkListBoxSortFunc)list_sort_func, self, NULL);
  gtk_list_box_set_header_func (GTK_LIST_BOX (widget), cc_list_box_update_header_func, NULL, NULL);
  gtk_container_add (GTK_CONTAINER (frame), widget);
  self->priv->list_box = widget;
  gtk_widget_show (widget);
  g_signal_connect (widget, "row-selected", G_CALLBACK (on_row_selected), self);

  self->priv->notification = WID ("notification");

  /* Drag and Drop */
  gtk_drag_dest_set (self->priv->list_box,
                     GTK_DEST_DEFAULT_MOTION | GTK_DEST_DEFAULT_DROP,
                     drag_target_entries, 1,
                     GDK_ACTION_MOVE);
  g_signal_connect (self->priv->list_box,
                    "drag-data-received",
                    G_CALLBACK (drag_data_received), self);
  g_signal_connect (self->priv->list_box,
                    "drag-motion",
                    G_CALLBACK (drag_motion), self);
  g_signal_connect (self->priv->list_box,
                    "drag-leave",
                    G_CALLBACK (drag_leave), NULL);

  provider = gtk_css_provider_new ();
  gtk_css_provider_load_from_data (provider, css, -1, NULL);
  gtk_style_context_add_provider_for_screen (gdk_screen_get_default (),
                                             GTK_STYLE_PROVIDER (provider),
                                             GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
  g_object_unref (provider);

  widget = WID ("settings_button");
  g_signal_connect (widget, "clicked",
                    G_CALLBACK (settings_button_clicked), self);
  gtk_widget_set_sensitive (widget, cc_search_locations_dialog_is_available ());

  self->priv->search_settings = g_settings_new ("org.gnome.desktop.search-providers");
  self->priv->sort_order = g_hash_table_new_full (g_str_hash, g_str_equal,
                                                  g_free, NULL);
  g_signal_connect_swapped (self->priv->search_settings, "changed::sort-order",
                            G_CALLBACK (search_panel_invalidate_sort_order), self);
  search_panel_invalidate_sort_order (self);

  populate_search_providers (self);

  widget = WID ("search_vbox");
  gtk_container_add (GTK_CONTAINER (self), widget);
}

static void
cc_search_panel_class_init (CcSearchPanelClass *klass)
{
  GObjectClass *oclass = G_OBJECT_CLASS (klass);

  oclass->constructed = cc_search_panel_constructed;
  oclass->dispose = cc_search_panel_dispose;
  oclass->finalize = cc_search_panel_finalize;

  g_type_class_add_private (klass, sizeof (CcSearchPanelPrivate));
}
