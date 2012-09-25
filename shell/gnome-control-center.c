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


#include "gnome-control-center.h"

#include <glib/gi18n.h>
#include <gio/gio.h>
#include <gio/gdesktopappinfo.h>
#include <gtk/gtk.h>
#include <gdk/gdkkeysyms.h>
#include <string.h>
#ifdef HAVE_CHEESE
#include <clutter-gtk/clutter-gtk.h>
#endif /* HAVE_CHEESE */
#define GMENU_I_KNOW_THIS_IS_UNSTABLE
#include <gmenu-tree.h>

#include "cc-panel.h"
#include "cc-shell.h"
#include "cc-shell-category-view.h"
#include "cc-shell-model.h"

G_DEFINE_TYPE (GnomeControlCenter, gnome_control_center, CC_TYPE_SHELL)

#define CONTROL_CENTER_PRIVATE(o) \
  (G_TYPE_INSTANCE_GET_PRIVATE ((o), GNOME_TYPE_CONTROL_CENTER, GnomeControlCenterPrivate))

#define W(b,x) GTK_WIDGET (gtk_builder_get_object (b, x))

/* Use a fixed width for the shell, since resizing horizontally is more awkward
 * for the user than resizing vertically
 * Both sizes are defined in https://live.gnome.org/Design/SystemSettings/ */
#define FIXED_WIDTH 740
#define FIXED_HEIGHT 636
#define SMALL_SCREEN_FIXED_HEIGHT 400

#define MIN_ICON_VIEW_HEIGHT 300

typedef enum {
	SMALL_SCREEN_UNSET,
	SMALL_SCREEN_TRUE,
	SMALL_SCREEN_FALSE
} CcSmallScreen;

struct _GnomeControlCenterPrivate
{
  GtkBuilder *builder;
  GtkWidget  *notebook;
  GtkWidget  *main_vbox;
  GtkWidget  *scrolled_window;
  GtkWidget  *search_scrolled;
  GtkWidget  *current_panel_box;
  GtkWidget  *current_panel;
  char       *current_panel_id;
  GtkWidget  *window;
  GtkWidget  *search_entry;
  GtkWidget  *lock_button;
  GPtrArray  *custom_widgets;

  GMenuTree  *menu_tree;
  GtkListStore *store;
  GHashTable *category_views;

  GtkTreeModel *search_filter;
  GtkWidget *search_view;
  gchar *filter_string;

  guint32 last_time;

  GIOExtensionPoint *extension_point;

  gchar *default_window_title;
  gchar *default_window_icon;

  int monitor_num;
  CcSmallScreen small_screen;
};

/* Notebook helpers */
static GtkWidget *
notebook_get_selected_page (GtkWidget *notebook)
{
  int curr;

  curr = gtk_notebook_get_current_page (GTK_NOTEBOOK (notebook));
  if (curr == -1)
    return NULL;
  return gtk_notebook_get_nth_page (GTK_NOTEBOOK (notebook), curr);
}

static void
notebook_select_page (GtkWidget *notebook,
		      GtkWidget *page)
{
  int i, num;

  num = gtk_notebook_get_n_pages (GTK_NOTEBOOK (notebook));
  for (i = 0; i < num; i++)
    {
      if (gtk_notebook_get_nth_page (GTK_NOTEBOOK (notebook), i) == page)
        {
          gtk_notebook_set_current_page (GTK_NOTEBOOK (notebook), i);
          return;
        }
    }

  g_warning ("Couldn't select GtkNotebook page %p", page);
}

static void
notebook_remove_page (GtkWidget *notebook,
		      GtkWidget *page)
{
  int i, num;

  num = gtk_notebook_get_n_pages (GTK_NOTEBOOK (notebook));
  for (i = 0; i < num; i++)
    {
      if (gtk_notebook_get_nth_page (GTK_NOTEBOOK (notebook), i) == page)
        {
          gtk_notebook_remove_page (GTK_NOTEBOOK (notebook), i);
          return;
        }
    }

  g_warning ("Couldn't find GtkNotebook page to remove %p", page);
}

static void
notebook_add_page (GtkWidget *notebook,
		   GtkWidget *page)
{
  gtk_notebook_append_page (GTK_NOTEBOOK (notebook), page, NULL);
}

static const gchar *
get_icon_name_from_g_icon (GIcon *gicon)
{
  const gchar * const *names;
  GtkIconTheme *icon_theme;
  int i;

  if (!G_IS_THEMED_ICON (gicon))
    return NULL;

  names = g_themed_icon_get_names (G_THEMED_ICON (gicon));
  icon_theme = gtk_icon_theme_get_default ();

  for (i = 0; names[i] != NULL; i++)
    {
      if (gtk_icon_theme_has_icon (icon_theme, names[i]))
        return names[i];
    }

  return NULL;
}

static gboolean
activate_panel (GnomeControlCenter *shell,
                const gchar        *id,
		const gchar       **argv,
                const gchar        *desktop_file,
                const gchar        *name,
                GIcon              *gicon)
{
  GnomeControlCenterPrivate *priv = shell->priv;
  GType panel_type = G_TYPE_INVALID;
  GList *panels, *l;
  GtkWidget *box;
  const gchar *icon_name;

  /* check if there is an plugin that implements this panel */
  panels = g_io_extension_point_get_extensions (priv->extension_point);

  if (!desktop_file)
    return FALSE;
  if (!id)
    return FALSE;

  for (l = panels; l != NULL; l = l->next)
    {
      GIOExtension *extension;
      const gchar *name;

      extension = l->data;

      name = g_io_extension_get_name (extension);

      if (!g_strcmp0 (name, id))
        {
          panel_type = g_io_extension_get_type (extension);
          break;
        }
    }

  if (panel_type == G_TYPE_INVALID)
    {
      g_warning ("Could not find the loadable module for panel '%s'", id);
      return FALSE;
    }

  /* create the panel plugin */
  priv->current_panel = g_object_new (panel_type, "shell", shell, "argv", argv, NULL);
  cc_shell_set_active_panel (CC_SHELL (shell), CC_PANEL (priv->current_panel));
  gtk_widget_show (priv->current_panel);

  gtk_lock_button_set_permission (GTK_LOCK_BUTTON (priv->lock_button),
                                  cc_panel_get_permission (CC_PANEL (priv->current_panel)));

  box = gtk_alignment_new (0, 0, 1, 1);
  gtk_alignment_set_padding (GTK_ALIGNMENT (box), 6, 6, 6, 6);

  gtk_container_add (GTK_CONTAINER (box), priv->current_panel);

  gtk_widget_set_name (box, id);
  notebook_add_page (priv->notebook, box);

  /* switch to the new panel */
  gtk_widget_show (box);
  notebook_select_page (priv->notebook, box);

  /* set the title of the window */
  icon_name = get_icon_name_from_g_icon (gicon);
  gtk_window_set_role (GTK_WINDOW (priv->window), id);
  gtk_window_set_title (GTK_WINDOW (priv->window), name);
  gtk_window_set_default_icon_name (icon_name);
  gtk_window_set_icon_name (GTK_WINDOW (priv->window), icon_name);

  priv->current_panel_box = box;

  return TRUE;
}

static void
_shell_remove_all_custom_widgets (GnomeControlCenterPrivate *priv)
{
  GtkBox *box;
  GtkWidget *widget;
  guint i;

  /* remove from the header */
  box = GTK_BOX (W (priv->builder, "topright"));
  for (i = 0; i < priv->custom_widgets->len; i++)
    {
        widget = g_ptr_array_index (priv->custom_widgets, i);
        gtk_container_remove (GTK_CONTAINER (box), widget);
    }
  g_ptr_array_set_size (priv->custom_widgets, 0);
}

static void
shell_show_overview_page (GnomeControlCenter *center)
{
  GnomeControlCenterPrivate *priv = center->priv;

  notebook_select_page (priv->notebook, priv->scrolled_window);

  if (priv->current_panel_box)
    notebook_remove_page (priv->notebook, priv->current_panel_box);
  priv->current_panel = NULL;
  priv->current_panel_box = NULL;
  g_clear_pointer (&priv->current_panel_id, g_free);

  /* clear the search text */
  g_free (priv->filter_string);
  priv->filter_string = g_strdup ("");
  gtk_entry_set_text (GTK_ENTRY (priv->search_entry), "");
  gtk_widget_grab_focus (priv->search_entry);

  gtk_lock_button_set_permission (GTK_LOCK_BUTTON (priv->lock_button), NULL);

  /* reset window title and icon */
  gtk_window_set_role (GTK_WINDOW (priv->window), NULL);
  gtk_window_set_title (GTK_WINDOW (priv->window), priv->default_window_title);
  gtk_window_set_default_icon_name (priv->default_window_icon);
  gtk_window_set_icon_name (GTK_WINDOW (priv->window),
                            priv->default_window_icon);

  cc_shell_set_active_panel (CC_SHELL (center), NULL);

  /* clear any custom widgets */
  _shell_remove_all_custom_widgets (priv);
}

void
gnome_control_center_set_overview_page (GnomeControlCenter *center)
{
  shell_show_overview_page (center);
}

static void
item_activated_cb (CcShellCategoryView *view,
                   gchar               *name,
                   gchar               *id,
                   gchar               *desktop_file,
                   GnomeControlCenter  *shell)
{
  GError *err = NULL;

  if (!cc_shell_set_active_panel_from_id (CC_SHELL (shell), id, NULL, &err))
    {
      /* TODO: show message to user */
      if (err)
        {
          g_warning ("Could not active panel \"%s\": %s", id, err->message);
          g_error_free (err);
        }
    }
}

static gboolean
category_focus_out (GtkWidget          *view,
                    GdkEventFocus      *event,
                    GnomeControlCenter *shell)
{
  gtk_icon_view_unselect_all (GTK_ICON_VIEW (view));

  return FALSE;
}

static gboolean
category_focus_in (GtkWidget          *view,
                   GdkEventFocus      *event,
                   GnomeControlCenter *shell)
{
  GtkTreePath *path;

  if (!gtk_icon_view_get_cursor (GTK_ICON_VIEW (view), &path, NULL))
    {
      path = gtk_tree_path_new_from_indices (0, -1);
      gtk_icon_view_set_cursor (GTK_ICON_VIEW (view), path, NULL, FALSE);
    }

  gtk_icon_view_select_path (GTK_ICON_VIEW (view), path);
  gtk_tree_path_free (path);

  return FALSE;
}

static GList *
get_item_views (GnomeControlCenter *shell)
{
  GList *list, *l;
  GList *res;

  list = gtk_container_get_children (GTK_CONTAINER (shell->priv->main_vbox));
  res = NULL;
  for (l = list; l; l = l->next)
    {
      if (!CC_IS_SHELL_CATEGORY_VIEW (l->data))
        continue;
      res = g_list_append (res, cc_shell_category_view_get_item_view (CC_SHELL_CATEGORY_VIEW (l->data)));
    }

  g_list_free (list);

  return res;
}

static gboolean
keynav_failed (GtkIconView        *current_view,
               GtkDirectionType    direction,
               GnomeControlCenter *shell)
{
  GList *views, *v;
  GtkIconView *new_view;
  GtkTreePath *path;
  GtkTreeModel *model;
  GtkTreeIter iter;
  gint col, c, dist, d;
  GtkTreePath *sel;
  gboolean res;

  res = FALSE;

  views = get_item_views (shell);

  for (v = views; v; v = v->next)
    {
      if (v->data == current_view)
        break;
    }

  if (direction == GTK_DIR_DOWN && v != NULL && v->next != NULL)
    {
      new_view = v->next->data;

      if (gtk_icon_view_get_cursor (current_view, &path, NULL))
        {
          col = gtk_icon_view_get_item_column (current_view, path);
          gtk_tree_path_free (path);

          sel = NULL;
          dist = 1000;
          model = gtk_icon_view_get_model (new_view);
          gtk_tree_model_get_iter_first (model, &iter);
          do {
            path = gtk_tree_model_get_path (model, &iter);
            c = gtk_icon_view_get_item_column (new_view, path);
            d = ABS (c - col);
            if (d < dist)
              {
                if (sel)
                  gtk_tree_path_free (sel);
                sel = path;
                dist = d;
              }
            else
              gtk_tree_path_free (path);
          } while (gtk_tree_model_iter_next (model, &iter));

          gtk_icon_view_set_cursor (new_view, sel, NULL, FALSE);
          gtk_tree_path_free (sel);
        }

      gtk_widget_grab_focus (GTK_WIDGET (new_view));

      res = TRUE;
    }

  if (direction == GTK_DIR_UP && v != NULL && v->prev != NULL)
    {
      new_view = v->prev->data;

      if (gtk_icon_view_get_cursor (current_view, &path, NULL))
        {
          col = gtk_icon_view_get_item_column (current_view, path);
          gtk_tree_path_free (path);

          sel = NULL;
          dist = 1000;
          model = gtk_icon_view_get_model (new_view);
          gtk_tree_model_get_iter_first (model, &iter);
          do {
            path = gtk_tree_model_get_path (model, &iter);
            c = gtk_icon_view_get_item_column (new_view, path);
            d = ABS (c - col);
            if (d <= dist)
              {
                if (sel)
                  gtk_tree_path_free (sel);
                sel = path;
                dist = d;
              }
            else
              gtk_tree_path_free (path);
          } while (gtk_tree_model_iter_next (model, &iter));

          gtk_icon_view_set_cursor (new_view, sel, NULL, FALSE);
          gtk_tree_path_free (sel);
        }

      gtk_widget_grab_focus (GTK_WIDGET (new_view));

      res = TRUE;
    }

  g_list_free (views);

  return res;
}

static gboolean
model_filter_func (GtkTreeModel              *model,
                   GtkTreeIter               *iter,
                   GnomeControlCenterPrivate *priv)
{
  gchar *name, *description;
  gchar *needle, *haystack;
  gboolean result;
  gchar **keywords;

  gtk_tree_model_get (model, iter,
                      COL_NAME, &name,
                      COL_DESCRIPTION, &description,
                      COL_KEYWORDS, &keywords,
                      -1);

  if (!priv->filter_string || !name)
    {
      g_free (name);
      g_free (description);
      g_strfreev (keywords);
      return FALSE;
    }

  needle = g_utf8_casefold (priv->filter_string, -1);
  haystack = g_utf8_casefold (name, -1);

  result = (strstr (haystack, needle) != NULL);

  if (!result && description)
    {
      gchar *folded;

      folded = g_utf8_casefold (description, -1);
      result = (strstr (folded, needle) != NULL);
      g_free (folded);
    }

  if (!result && keywords)
    {
      gint i;
      gchar *keyword;

      for (i = 0; !result && keywords[i]; i++)
        {
          keyword = g_utf8_casefold (keywords[i], -1);
          result = strstr (keyword, needle) == keyword;
          g_free (keyword);
        }
    }

  g_free (name);
  g_free (haystack);
  g_free (needle);
  g_strfreev (keywords);

  return result;
}

static gboolean
category_filter_func (GtkTreeModel *model,
                      GtkTreeIter  *iter,
                      gchar        *filter)
{
  gchar *category;
  gboolean result;

  gtk_tree_model_get (model, iter, COL_CATEGORY, &category, -1);

  result = (g_strcmp0 (category, filter) == 0);

  g_free (category);

  return result;
}

static void
search_entry_changed_cb (GtkEntry           *entry,
                         GnomeControlCenter *center)
{
  GnomeControlCenterPrivate *priv = center->priv;
  char *str;

  /* if the entry text was set manually (not by the user) */
  if (!g_strcmp0 (priv->filter_string, gtk_entry_get_text (entry)))
    return;

  /* Don't re-filter for added trailing or leading spaces */
  str = g_strdup (gtk_entry_get_text (entry));
  g_strstrip (str);
  if (!g_strcmp0 (str, priv->filter_string))
    {
      g_free (str);
      return;
    }

  g_free (priv->filter_string);
  priv->filter_string = str;

  if (!g_strcmp0 (priv->filter_string, ""))
    {
      shell_show_overview_page (center);
    }
  else
    {
      gtk_tree_model_filter_refilter (GTK_TREE_MODEL_FILTER (priv->search_filter));
      notebook_select_page (priv->notebook, priv->search_scrolled);
    }
}

static gboolean
search_entry_key_press_event_cb (GtkEntry    *entry,
                                 GdkEventKey *event,
                                 GnomeControlCenterPrivate   *priv)
{
  if (event->keyval == GDK_KEY_Return)
    {
      GtkTreePath *path;

      path = gtk_tree_path_new_first ();

      priv->last_time = event->time;

      gtk_icon_view_item_activated (GTK_ICON_VIEW (priv->search_view), path);

      gtk_tree_path_free (path);
      return TRUE;
    }

  if (event->keyval == GDK_KEY_Escape)
    {
      gtk_entry_set_text (entry, "");
      return TRUE;
    }

  return FALSE;
}

static void
on_search_selection_changed (GtkTreeSelection   *selection,
                             GnomeControlCenter *shell)
{
  GtkTreeModel *model;
  GtkTreeIter   iter;
  char         *id = NULL;

  if (!gtk_tree_selection_get_selected (selection, &model, &iter))
    return;

  gtk_tree_model_get (model, &iter,
                      COL_ID, &id,
                      -1);

  if (id)
    cc_shell_set_active_panel_from_id (CC_SHELL (shell), id, NULL, NULL);

  gtk_tree_selection_unselect_all (selection);

  g_free (id);
}

static void
setup_search (GnomeControlCenter *shell)
{
  GtkWidget *search_view, *widget;
  GtkCellRenderer *renderer;
  GtkTreeViewColumn *column;
  GnomeControlCenterPrivate *priv = shell->priv;

  g_return_if_fail (priv->store != NULL);

  /* create the search filter */
  priv->search_filter = gtk_tree_model_filter_new (GTK_TREE_MODEL (priv->store),
                                                   NULL);

  gtk_tree_model_filter_set_visible_func (GTK_TREE_MODEL_FILTER (priv->search_filter),
                                          (GtkTreeModelFilterVisibleFunc)
                                          model_filter_func,
                                          priv, NULL);

  /* set up the search view */
  priv->search_view = search_view = gtk_tree_view_new ();
  gtk_tree_view_set_headers_visible (GTK_TREE_VIEW (search_view), FALSE);
  gtk_tree_view_set_model (GTK_TREE_VIEW (search_view),
                           GTK_TREE_MODEL (priv->search_filter));

  renderer = gtk_cell_renderer_pixbuf_new ();
  g_object_set (renderer,
                "follow-state", TRUE,
                "xpad", 15,
                "ypad", 10,
                "stock-size", GTK_ICON_SIZE_DIALOG,
                NULL);
  column = gtk_tree_view_column_new_with_attributes ("Icon", renderer,
                                                     "gicon", COL_GICON,
                                                     NULL);
  gtk_tree_view_column_set_expand (column, FALSE);
  gtk_tree_view_append_column (GTK_TREE_VIEW (priv->search_view), column);

  renderer = gtk_cell_renderer_text_new ();
  g_object_set (renderer,
                "xpad", 0,
                NULL);
  column = gtk_tree_view_column_new_with_attributes ("Name", renderer,
                                                     "text", COL_NAME,
                                                     NULL);
  gtk_tree_view_column_set_expand (column, FALSE);
  gtk_tree_view_append_column (GTK_TREE_VIEW (priv->search_view), column);

  renderer = gtk_cell_renderer_text_new ();
  g_object_set (renderer,
                "xpad", 15,
                NULL);
  column = gtk_tree_view_column_new_with_attributes ("Description", renderer,
                                                     "text", COL_DESCRIPTION,
                                                     NULL);
  gtk_tree_view_column_set_expand (column, TRUE);
  gtk_tree_view_append_column (GTK_TREE_VIEW (priv->search_view), column);

  priv->search_scrolled = W (priv->builder, "search-scrolled-window");
  gtk_container_add (GTK_CONTAINER (priv->search_scrolled), search_view);

  g_signal_connect (gtk_tree_view_get_selection (GTK_TREE_VIEW (priv->search_view)),
                    "changed",
                    G_CALLBACK (on_search_selection_changed),
                    shell);

  /* setup the search entry widget */
  widget = (GtkWidget*) gtk_builder_get_object (priv->builder, "search-entry");
  priv->search_entry = widget;
  priv->filter_string = g_strdup ("");

  g_signal_connect (widget, "changed", G_CALLBACK (search_entry_changed_cb),
                    shell);
  g_signal_connect (widget, "key-press-event",
                    G_CALLBACK (search_entry_key_press_event_cb), priv);

  gtk_widget_show (priv->search_view);
}

static void
setup_lock (GnomeControlCenter *shell)
{
  GnomeControlCenterPrivate *priv = shell->priv;

  priv->lock_button = W (priv->builder, "lock-button");
}

static void
maybe_add_category_view (GnomeControlCenter *shell,
                         const char         *name)
{
  GtkTreeModel *filter;
  GtkWidget *categoryview;

  if (g_hash_table_lookup (shell->priv->category_views, name) != NULL)
    return;

  if (g_hash_table_size (shell->priv->category_views) > 0)
    {
      GtkWidget *separator;
      separator = gtk_separator_new (GTK_ORIENTATION_HORIZONTAL);
      gtk_widget_set_margin_top (separator, 11);
      gtk_widget_set_margin_bottom (separator, 10);
      gtk_box_pack_start (GTK_BOX (shell->priv->main_vbox), separator, FALSE, FALSE, 0);
      gtk_widget_show (separator);
    }

  /* create new category view for this category */
  filter = gtk_tree_model_filter_new (GTK_TREE_MODEL (shell->priv->store),
                                      NULL);
  gtk_tree_model_filter_set_visible_func (GTK_TREE_MODEL_FILTER (filter),
                                          (GtkTreeModelFilterVisibleFunc) category_filter_func,
                                          g_strdup (name), g_free);

  categoryview = cc_shell_category_view_new (name, filter);
  gtk_box_pack_start (GTK_BOX (shell->priv->main_vbox), categoryview, FALSE, TRUE, 0);

  g_signal_connect (cc_shell_category_view_get_item_view (CC_SHELL_CATEGORY_VIEW (categoryview)),
                    "desktop-item-activated",
                    G_CALLBACK (item_activated_cb), shell);

  gtk_widget_show (categoryview);

  g_signal_connect (cc_shell_category_view_get_item_view (CC_SHELL_CATEGORY_VIEW (categoryview)),
                    "focus-in-event",
                    G_CALLBACK (category_focus_in), shell);
  g_signal_connect (cc_shell_category_view_get_item_view (CC_SHELL_CATEGORY_VIEW (categoryview)),
                    "focus-out-event",
                    G_CALLBACK (category_focus_out), shell);
  g_signal_connect (cc_shell_category_view_get_item_view (CC_SHELL_CATEGORY_VIEW (categoryview)),
                    "keynav-failed",
                    G_CALLBACK (keynav_failed), shell);

  g_hash_table_insert (shell->priv->category_views, g_strdup (name), categoryview);
}

static void
reload_menu (GnomeControlCenter *shell)
{
  GError *error;
  GMenuTreeDirectory *d;
  GMenuTreeIter *iter;
  GMenuTreeItemType next_type;

  error = NULL;
  if (!gmenu_tree_load_sync (shell->priv->menu_tree, &error))
    {
      g_warning ("Could not load control center menu: %s", error->message);
      g_clear_error (&error);
      return;
    }


  d = gmenu_tree_get_root_directory (shell->priv->menu_tree);
  iter = gmenu_tree_directory_iter (d);

  while ((next_type = gmenu_tree_iter_next (iter)) != GMENU_TREE_ITEM_INVALID)
    {
      if (next_type == GMENU_TREE_ITEM_DIRECTORY)
        {
          GMenuTreeDirectory *subdir;
          const gchar *dir_name;
          GMenuTreeIter *sub_iter;
          GMenuTreeItemType sub_next_type;

          subdir = gmenu_tree_iter_get_directory (iter);
          dir_name = gmenu_tree_directory_get_name (subdir);

          maybe_add_category_view (shell, dir_name);

          /* add the items from this category to the model */
          sub_iter = gmenu_tree_directory_iter (subdir);
          while ((sub_next_type = gmenu_tree_iter_next (sub_iter)) != GMENU_TREE_ITEM_INVALID)
            {
              if (sub_next_type == GMENU_TREE_ITEM_ENTRY)
                {
                  GMenuTreeEntry *item = gmenu_tree_iter_get_entry (sub_iter);
                  cc_shell_model_add_item (CC_SHELL_MODEL (shell->priv->store),
                                           dir_name,
                                           item);
                  gmenu_tree_item_unref (item);
                }
            }

          gmenu_tree_iter_unref (sub_iter);
          gmenu_tree_item_unref (subdir);
        }
    }

  gmenu_tree_iter_unref (iter);
}

static void
on_menu_changed (GMenuTree          *monitor,
                 GnomeControlCenter *shell)
{
  gtk_list_store_clear (shell->priv->store);
  reload_menu (shell);
}

static void
setup_model (GnomeControlCenter *shell)
{
  GnomeControlCenterPrivate *priv = shell->priv;

  gtk_widget_set_margin_top (shell->priv->main_vbox, 8);
  gtk_widget_set_margin_bottom (shell->priv->main_vbox, 8);
  gtk_widget_set_margin_left (shell->priv->main_vbox, 12);
  gtk_widget_set_margin_right (shell->priv->main_vbox, 12);
  gtk_container_set_focus_vadjustment (GTK_CONTAINER (shell->priv->main_vbox),
                                       gtk_scrolled_window_get_vadjustment (GTK_SCROLLED_WINDOW (shell->priv->scrolled_window)));

  priv->store = (GtkListStore *) cc_shell_model_new ();
  priv->category_views = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, NULL);
  priv->menu_tree = gmenu_tree_new_for_path (MENUDIR "/gnomecc.menu", 0);

  reload_menu (shell);

  g_signal_connect (priv->menu_tree, "changed", G_CALLBACK (on_menu_changed), shell);
}

static void
load_panel_plugins (GnomeControlCenter *shell)
{
  GList *modules;

  /* only allow this function to be run once to prevent modules being loaded
   * twice
   */
  if (shell->priv->extension_point)
    return;

  /* make sure the base type is registered */
  g_type_from_name ("CcPanel");

  shell->priv->extension_point
    = g_io_extension_point_register (CC_SHELL_PANEL_EXTENSION_POINT);

  /* load all the plugins in the panels directory */
  modules = g_io_modules_load_all_in_directory (PANELS_DIR);
  g_list_free (modules);

}


static void
home_button_clicked_cb (GtkButton *button,
                        GnomeControlCenter *shell)
{
  shell_show_overview_page (shell);
}

static void
notebook_page_notify_cb (GtkNotebook              *notebook,
			 GParamSpec               *spec,
                         GnomeControlCenterPrivate *priv)
{
  int nat_height;
  GtkWidget *child;

  child = notebook_get_selected_page (GTK_WIDGET (notebook));

  /* make sure the home button is shown on all pages except the overview page */

  if (child == priv->scrolled_window || child == priv->search_scrolled)
    {
      gtk_widget_hide (W (priv->builder, "home-button"));
      gtk_widget_show (W (priv->builder, "search-entry"));
      gtk_widget_hide (W (priv->builder, "lock-button"));

      gtk_widget_get_preferred_height_for_width (GTK_WIDGET (priv->main_vbox),
                                                 FIXED_WIDTH, NULL, &nat_height);
      gtk_scrolled_window_set_min_content_height (GTK_SCROLLED_WINDOW (priv->scrolled_window),
                                                  priv->small_screen == SMALL_SCREEN_TRUE ? SMALL_SCREEN_FIXED_HEIGHT : nat_height);
    }
  else
    {
      gtk_widget_show (W (priv->builder, "home-button"));
      gtk_widget_hide (W (priv->builder, "search-entry"));
      /* set the scrolled window small so that it doesn't force
         the window to be larger than this panel */
      gtk_widget_get_preferred_height_for_width (GTK_WIDGET (priv->window),
                                                 FIXED_WIDTH, NULL, &nat_height);
      gtk_scrolled_window_set_min_content_height (GTK_SCROLLED_WINDOW (priv->scrolled_window), MIN_ICON_VIEW_HEIGHT);
      gtk_window_resize (GTK_WINDOW (priv->window),
                         FIXED_WIDTH,
                         nat_height);
    }
}

/* CcShell implementation */
static void
_shell_embed_widget_in_header (CcShell      *shell,
                               GtkWidget    *widget)
{
  GnomeControlCenterPrivate *priv = GNOME_CONTROL_CENTER (shell)->priv;
  GtkBox *box;

  /* add to header */
  box = GTK_BOX (W (priv->builder, "topright"));
  gtk_box_pack_end (box, widget, FALSE, FALSE, 0);
  g_ptr_array_add (priv->custom_widgets, g_object_ref (widget));
}

/* CcShell implementation */
static gboolean
_shell_set_active_panel_from_id (CcShell      *shell,
                                 const gchar  *start_id,
				 const gchar **argv,
                                 GError      **err)
{
  GtkTreeIter iter;
  gboolean iter_valid;
  gchar *name = NULL;
  gchar *desktop = NULL;
  GIcon *gicon = NULL;
  GnomeControlCenterPrivate *priv = GNOME_CONTROL_CENTER (shell)->priv;
  GtkWidget *old_panel;

  /* When loading the same panel again, just set the argv */
  if (g_strcmp0 (priv->current_panel_id, start_id) == 0)
    {
      g_object_set (G_OBJECT (priv->current_panel), "argv", argv, NULL);
      return TRUE;
    }

  g_clear_pointer (&priv->current_panel_id, g_free);

  /* clear any custom widgets */
  _shell_remove_all_custom_widgets (priv);

  iter_valid = gtk_tree_model_get_iter_first (GTK_TREE_MODEL (priv->store),
                                              &iter);

  /* find the details for this item */
  while (iter_valid)
    {
      gchar *id;

      gtk_tree_model_get (GTK_TREE_MODEL (priv->store), &iter,
                          COL_NAME, &name,
                          COL_DESKTOP_FILE, &desktop,
                          COL_GICON, &gicon,
                          COL_ID, &id,
                          -1);

      if (id && !strcmp (id, start_id))
        {
          g_free (id);
          break;
        }
      else
        {
          g_free (id);
          g_free (name);
          g_free (desktop);
	  if (gicon)
	    g_object_unref (gicon);

          name = NULL;
          id = NULL;
          desktop = NULL;
          gicon = NULL;
        }

      iter_valid = gtk_tree_model_iter_next (GTK_TREE_MODEL (priv->store),
                                             &iter);
    }

  if (!name)
    {
      g_warning ("Could not find settings panel \"%s\"", start_id);
    }
  else if (activate_panel (GNOME_CONTROL_CENTER (shell), start_id, argv, desktop,
                           name, gicon) == FALSE)
    {
      /* Failed to activate the panel for some reason */
      old_panel = priv->current_panel_box;
      priv->current_panel_box = NULL;
      notebook_select_page (priv->notebook, priv->scrolled_window);
      if (old_panel)
        notebook_remove_page (priv->notebook, old_panel);
    }
  else
    {
      priv->current_panel_id = g_strdup (start_id);
    }

  g_free (name);
  g_free (desktop);
  if (gicon)
    g_object_unref (gicon);

  return TRUE;
}

static GtkWidget *
_shell_get_toplevel (CcShell *shell)
{
  return GNOME_CONTROL_CENTER (shell)->priv->window;
}

/* GObject Implementation */
static void
gnome_control_center_get_property (GObject    *object,
                                   guint       property_id,
                                   GValue     *value,
                                   GParamSpec *pspec)
{
  switch (property_id)
    {
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
    }
}

static void
gnome_control_center_set_property (GObject      *object,
                                   guint         property_id,
                                   const GValue *value,
                                   GParamSpec   *pspec)
{
  switch (property_id)
    {
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
    }
}

static void
gnome_control_center_dispose (GObject *object)
{
  GnomeControlCenterPrivate *priv = GNOME_CONTROL_CENTER (object)->priv;

  g_free (priv->current_panel_id);

  if (priv->custom_widgets)
    {
      g_ptr_array_unref (priv->custom_widgets);
      priv->custom_widgets = NULL;
    }
  if (priv->window)
    {
      gtk_widget_destroy (priv->window);
      priv->window = NULL;

      /* destroying the window will destroy its children */
      priv->notebook = NULL;
      priv->search_entry = NULL;
      priv->search_view = NULL;
    }

  if (priv->builder)
    {
      g_object_unref (priv->builder);
      priv->builder = NULL;
    }

  if (priv->store)
    {
      g_object_unref (priv->store);
      priv->store = NULL;
    }

  if (priv->search_filter)
    {
      g_object_unref (priv->search_filter);
      priv->search_filter = NULL;
    }


  G_OBJECT_CLASS (gnome_control_center_parent_class)->dispose (object);
}

static void
gnome_control_center_finalize (GObject *object)
{
  GnomeControlCenterPrivate *priv = GNOME_CONTROL_CENTER (object)->priv;

  if (priv->filter_string)
    {
      g_free (priv->filter_string);
      priv->filter_string = NULL;
    }

  if (priv->default_window_title)
    {
      g_free (priv->default_window_title);
      priv->default_window_title = NULL;
    }

  if (priv->default_window_icon)
    {
      g_free (priv->default_window_icon);
      priv->default_window_icon = NULL;
    }

  if (priv->menu_tree)
    {
      g_signal_handlers_disconnect_by_func (priv->menu_tree,
					    G_CALLBACK (on_menu_changed), object);
      g_object_unref (priv->menu_tree);
    }

  if (priv->category_views)
    {
      g_hash_table_destroy (priv->category_views);
    }

  G_OBJECT_CLASS (gnome_control_center_parent_class)->finalize (object);
}

static void
gnome_control_center_class_init (GnomeControlCenterClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  CcShellClass *shell_class = CC_SHELL_CLASS (klass);

  g_type_class_add_private (klass, sizeof (GnomeControlCenterPrivate));

  object_class->get_property = gnome_control_center_get_property;
  object_class->set_property = gnome_control_center_set_property;
  object_class->dispose = gnome_control_center_dispose;
  object_class->finalize = gnome_control_center_finalize;

  shell_class->set_active_panel_from_id = _shell_set_active_panel_from_id;
  shell_class->embed_widget_in_header = _shell_embed_widget_in_header;
  shell_class->get_toplevel = _shell_get_toplevel;
}

static gboolean
window_key_press_event (GtkWidget          *win,
			GdkEventKey        *event,
			GnomeControlCenter *self)
{
  GdkKeymap *keymap;
  gboolean retval;
  GdkModifierType state;

  if (event->state == 0)
    return FALSE;

  retval = FALSE;
  state = event->state;
  keymap = gdk_keymap_get_default ();
  gdk_keymap_add_virtual_modifiers (keymap, &state);
  state = state & gtk_accelerator_get_default_mod_mask ();

  if (state == GDK_CONTROL_MASK)
    {
      switch (event->keyval)
        {
          case GDK_KEY_s:
          case GDK_KEY_S:
          case GDK_KEY_f:
          case GDK_KEY_F:
            if (gtk_widget_get_visible (self->priv->search_entry))
              {
                gtk_widget_grab_focus (self->priv->search_entry);
                retval = TRUE;
              }
            break;
          case GDK_KEY_Q:
          case GDK_KEY_q:
            g_object_unref (self);
            retval = TRUE;
            break;
          case GDK_KEY_W:
          case GDK_KEY_w:
            if (notebook_get_selected_page (self->priv->notebook) != self->priv->scrolled_window)
              shell_show_overview_page (self);
            retval = TRUE;
            break;
        }
    }
  return retval;
}

static gint
get_monitor_height (GnomeControlCenter *self)
{
  GdkScreen *screen;
  GdkRectangle rect;

  /* We cannot use workarea here, as this wouldn't
   * be updated when we read it after a monitors-changed signal */
  screen = gtk_widget_get_screen (self->priv->window);
  gdk_screen_get_monitor_geometry (screen, self->priv->monitor_num, &rect);

  return rect.height;
}

static gboolean
update_monitor_number (GnomeControlCenter *self)
{
  gboolean changed = FALSE;
  GtkWidget *widget;
  GdkScreen *screen;
  GdkWindow *window;
  int monitor;

  widget = self->priv->window;

  window = gtk_widget_get_window (widget);
  screen = gtk_widget_get_screen (widget);
  monitor = gdk_screen_get_monitor_at_window (screen, window);
  if (self->priv->monitor_num != monitor)
    {
      self->priv->monitor_num = monitor;
      changed = TRUE;
    }

  return changed;
}

static CcSmallScreen
is_small (GnomeControlCenter *self)
{
  if (get_monitor_height (self) <= FIXED_HEIGHT)
    return SMALL_SCREEN_TRUE;
  return SMALL_SCREEN_FALSE;
}

static void
update_small_screen_settings (GnomeControlCenter *self)
{
  CcSmallScreen small;

  update_monitor_number (self);
  small = is_small (self);

  if (small == SMALL_SCREEN_TRUE)
    {
      gtk_window_set_resizable (GTK_WINDOW (self->priv->window), TRUE);

      if (self->priv->small_screen != small)
        gtk_window_maximize (GTK_WINDOW (self->priv->window));
    }
  else
    {
      if (self->priv->small_screen != small)
        gtk_window_unmaximize (GTK_WINDOW (self->priv->window));

      gtk_window_set_resizable (GTK_WINDOW (self->priv->window), FALSE);
    }

  self->priv->small_screen = small;

  /* And update the minimum sizes */
  notebook_page_notify_cb (GTK_NOTEBOOK (self->priv->notebook), NULL, self->priv);
}

static gboolean
main_window_configure_cb (GtkWidget *widget,
                          GdkEvent  *event,
                          GnomeControlCenter *self)
{
  update_small_screen_settings (self);
  return FALSE;
}

static void
application_set_cb (GObject    *object,
                    GParamSpec *pspec,
                    GnomeControlCenter *self)
{
  /* update small screen settings now - to avoid visible resizing, we want
   * to do it before showing the window, and GtkApplicationWindow cannot be
   * realized unless its application property has been set */
  if (gtk_window_get_application (GTK_WINDOW (self->priv->window)))
    {
      gtk_widget_realize (self->priv->window);
      update_small_screen_settings (self);
    }
}

static void
monitors_changed_cb (GdkScreen *screen,
                     GnomeControlCenter *self)
{
  /* We reset small_screen_set to make sure that the
   * window gets maximised if need be, in update_small_screen_settings() */
  self->priv->small_screen = SMALL_SCREEN_UNSET;
  update_small_screen_settings (self);
}

static void
gnome_control_center_init (GnomeControlCenter *self)
{
  GError *err = NULL;
  GnomeControlCenterPrivate *priv;
  GdkScreen *screen;

  priv = self->priv = CONTROL_CENTER_PRIVATE (self);

#ifdef HAVE_CHEESE
  if (gtk_clutter_init (NULL, NULL) != CLUTTER_INIT_SUCCESS)
    {
      g_critical ("Clutter-GTK init failed");
      return;
    }
#endif /* HAVE_CHEESE */

  priv->monitor_num = -1;
  self->priv->small_screen = SMALL_SCREEN_UNSET;

  /* load the user interface */
  priv->builder = gtk_builder_new ();

  if (!gtk_builder_add_from_file (priv->builder, UIDIR "/shell.ui", &err))
    {
      g_critical ("Could not build interface: %s", err->message);
      g_error_free (err);

      return;
    }

  /* connect various signals */
  priv->window = W (priv->builder, "main-window");
  gtk_window_set_hide_titlebar_when_maximized (GTK_WINDOW (priv->window), TRUE);
  screen = gtk_widget_get_screen (priv->window);
  g_signal_connect (screen, "monitors-changed", G_CALLBACK (monitors_changed_cb), self);
  g_signal_connect (priv->window, "configure-event", G_CALLBACK (main_window_configure_cb), self);
  g_signal_connect (priv->window, "notify::application", G_CALLBACK (application_set_cb), self);
  g_signal_connect_swapped (priv->window, "delete-event", G_CALLBACK (g_object_unref), self);
  g_signal_connect_after (priv->window, "key_press_event",
                          G_CALLBACK (window_key_press_event), self);

  priv->notebook = W (priv->builder, "notebook");

  /* Main scrolled window */
  priv->scrolled_window = W (priv->builder, "scrolledwindow1");

  gtk_widget_set_size_request (priv->scrolled_window, FIXED_WIDTH, -1);
  priv->main_vbox = W (priv->builder, "main-vbox");
  g_signal_connect (priv->notebook, "notify::page",
                    G_CALLBACK (notebook_page_notify_cb), priv);

  g_signal_connect (gtk_builder_get_object (priv->builder, "home-button"),
                    "clicked", G_CALLBACK (home_button_clicked_cb), self);

  /* keep a list of custom widgets to unload on panel change */
  priv->custom_widgets = g_ptr_array_new_with_free_func ((GDestroyNotify) g_object_unref);

  /* load the available settings panels */
  setup_model (self);

  /* load the panels that are implemented as plugins */
  load_panel_plugins (self);

  /* setup search functionality */
  setup_search (self);

  setup_lock (self);

  /* store default window title and name */
  priv->default_window_title = g_strdup (gtk_window_get_title (GTK_WINDOW (priv->window)));
  priv->default_window_icon = g_strdup (gtk_window_get_icon_name (GTK_WINDOW (priv->window)));

  notebook_page_notify_cb (GTK_NOTEBOOK (priv->notebook), NULL, priv);
}

GnomeControlCenter *
gnome_control_center_new (void)
{
  return g_object_new (GNOME_TYPE_CONTROL_CENTER, NULL);
}

void
gnome_control_center_present (GnomeControlCenter *center)
{
  gtk_window_present (GTK_WINDOW (center->priv->window));
}

void
gnome_control_center_show (GnomeControlCenter *center,
			   GtkApplication     *app)
{
  gtk_window_set_application (GTK_WINDOW (center->priv->window), app);
  gtk_widget_show (gtk_bin_get_child (GTK_BIN (center->priv->window)));
}
