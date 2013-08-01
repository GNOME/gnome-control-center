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

#include <config.h>

#include "cc-window.h"

#include <glib/gi18n.h>
#include <gio/gio.h>
#include <gio/gdesktopappinfo.h>
#include <gtk/gtk.h>
#include <gdk/gdkkeysyms.h>
#include <gdk/gdkx.h>
#include <string.h>
#include <libgd/gd-stack.h>
#include <libgd/gd-header-bar.h>
#include <libgd/gd-header-button.h>
#include <libgd/gd-styled-text-renderer.h>

#include "cc-panel.h"
#include "cc-shell.h"
#include "cc-shell-category-view.h"
#include "cc-shell-model.h"
#include "cc-panel-loader.h"
#include "cc-util.h"

static void     cc_shell_iface_init         (CcShellInterface      *iface);

G_DEFINE_TYPE_WITH_CODE (CcWindow, cc_window, GTK_TYPE_APPLICATION_WINDOW,
                         G_IMPLEMENT_INTERFACE (CC_TYPE_SHELL, cc_shell_iface_init))

#define WINDOW_PRIVATE(o) \
  (G_TYPE_INSTANCE_GET_PRIVATE ((o), CC_TYPE_WINDOW, CcWindowPrivate))

/* Use a fixed width for the shell, since resizing horizontally is more awkward
 * for the user than resizing vertically
 * Both sizes are defined in https://live.gnome.org/Design/SystemSettings/ */
#define FIXED_WIDTH 740
#define FIXED_HEIGHT 636
#define SMALL_SCREEN_FIXED_HEIGHT 400

#define MIN_ICON_VIEW_HEIGHT 300

#define DEFAULT_WINDOW_TITLE N_("Settings")
#define DEFAULT_WINDOW_ICON_NAME "preferences-desktop"

#define SEARCH_PAGE "_search"
#define OVERVIEW_PAGE "_overview"

typedef enum {
	SMALL_SCREEN_UNSET,
	SMALL_SCREEN_TRUE,
	SMALL_SCREEN_FALSE
} CcSmallScreen;

struct _CcWindowPrivate
{
  GtkWidget  *stack;
  GtkWidget  *header;
  GtkWidget  *main_vbox;
  GtkWidget  *scrolled_window;
  GtkWidget  *search_scrolled;
  GtkWidget  *previous_button;
  GtkWidget  *top_right_box;
  GtkWidget  *search_entry;
  GtkWidget  *lock_button;
  GtkWidget  *current_panel_box;
  GtkWidget  *current_panel;
  char       *current_panel_id;
  GQueue     *previous_panels;

  GPtrArray  *custom_widgets;

  GtkListStore *store;

  GtkTreeModel *search_filter;
  GtkWidget *search_view;
  gchar *filter_string;

  CcPanel *active_panel;

  int monitor_num;
  CcSmallScreen small_screen;
};

enum
{
  PROP_0,
  PROP_ACTIVE_PANEL
};

static gboolean cc_window_set_active_panel_from_id (CcShell      *shell,
                                                    const gchar  *start_id,
                                                    const gchar **argv,
                                                    GError      **err);

static gint get_monitor_height (CcWindow *self);

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
activate_panel (CcWindow           *self,
                const gchar        *id,
                const gchar       **argv,
                const gchar        *name,
                GIcon              *gicon)
{
  CcWindowPrivate *priv = self->priv;
  GtkWidget *box;
  const gchar *icon_name;

  if (!id)
    return FALSE;

  priv->current_panel = GTK_WIDGET (cc_panel_loader_load_by_name (CC_SHELL (self), id, argv));
  cc_shell_set_active_panel (CC_SHELL (self), CC_PANEL (priv->current_panel));
  gtk_widget_show (priv->current_panel);

  gtk_lock_button_set_permission (GTK_LOCK_BUTTON (priv->lock_button),
                                  cc_panel_get_permission (CC_PANEL (priv->current_panel)));

  box = gtk_alignment_new (0, 0, 1, 1);

  gtk_container_add (GTK_CONTAINER (box), priv->current_panel);

  gd_stack_add_named (GD_STACK (priv->stack), box, id);

  /* switch to the new panel */
  gtk_widget_show (box);
  gd_stack_set_visible_child_name (GD_STACK (priv->stack), id);

  /* set the title of the window */
  icon_name = get_icon_name_from_g_icon (gicon);

  gtk_window_set_role (GTK_WINDOW (self), id);
  gd_header_bar_set_title (GD_HEADER_BAR (priv->header), name);
  gtk_window_set_default_icon_name (icon_name);
  gtk_window_set_icon_name (GTK_WINDOW (self), icon_name);

  priv->current_panel_box = box;

  return TRUE;
}

static void
_shell_remove_all_custom_widgets (CcWindowPrivate *priv)
{
  GtkWidget *widget;
  guint i;

  /* remove from the header */
  for (i = 0; i < priv->custom_widgets->len; i++)
    {
        widget = g_ptr_array_index (priv->custom_widgets, i);
        gtk_container_remove (GTK_CONTAINER (priv->top_right_box), widget);
    }
  g_ptr_array_set_size (priv->custom_widgets, 0);
}

static void
add_current_panel_to_history (CcShell    *shell,
                              const char *start_id)
{
  CcWindowPrivate *priv;

  g_return_if_fail (start_id != NULL);

  priv = CC_WINDOW (shell)->priv;

  if (!priv->current_panel_id ||
      g_strcmp0 (priv->current_panel_id, start_id) == 0)
    return;

  g_queue_push_head (priv->previous_panels, g_strdup (priv->current_panel_id));
  g_debug ("Added '%s' to the previous panels", priv->current_panel_id);
}

static void
shell_show_overview_page (CcWindow *self)
{
  CcWindowPrivate *priv = self->priv;

  gd_stack_set_visible_child_name (GD_STACK (priv->stack), OVERVIEW_PAGE);

  if (priv->current_panel_box)
    gtk_container_remove (GTK_CONTAINER (priv->stack), priv->current_panel_box);
  priv->current_panel = NULL;
  priv->current_panel_box = NULL;
  g_clear_pointer (&priv->current_panel_id, g_free);

  /* Clear the panel history */
  g_queue_free_full (self->priv->previous_panels, g_free);
  self->priv->previous_panels = g_queue_new ();

  /* clear the search text */
  g_free (priv->filter_string);
  priv->filter_string = g_strdup ("");
  gtk_entry_set_text (GTK_ENTRY (priv->search_entry), "");
  gtk_widget_grab_focus (priv->search_entry);

  gtk_lock_button_set_permission (GTK_LOCK_BUTTON (priv->lock_button), NULL);

  /* reset window title and icon */
  gtk_window_set_role (GTK_WINDOW (self), NULL);
  gd_header_bar_set_title (GD_HEADER_BAR (priv->header), NULL);
  gtk_window_set_default_icon_name (DEFAULT_WINDOW_ICON_NAME);
  gtk_window_set_icon_name (GTK_WINDOW (self), DEFAULT_WINDOW_ICON_NAME);

  cc_shell_set_active_panel (CC_SHELL (self), NULL);

  /* clear any custom widgets */
  _shell_remove_all_custom_widgets (priv);
}

void
cc_window_set_overview_page (CcWindow *center)
{
  shell_show_overview_page (center);
}

void
cc_window_set_search_item (CcWindow   *center,
                           const char *search)
{
  shell_show_overview_page (center);
  gtk_entry_set_text (GTK_ENTRY (center->priv->search_entry), search);
  gtk_editable_set_position (GTK_EDITABLE (center->priv->search_entry), -1);
}

static void
item_activated_cb (CcShellCategoryView *view,
                   gchar               *name,
                   gchar               *id,
                   CcWindow            *shell)
{
  cc_window_set_active_panel_from_id (CC_SHELL (shell), id, NULL, NULL);
}

static gboolean
category_focus_out (GtkWidget     *view,
                    GdkEventFocus *event,
                    CcWindow      *shell)
{
  gtk_icon_view_unselect_all (GTK_ICON_VIEW (view));

  return FALSE;
}

static gboolean
category_focus_in (GtkWidget     *view,
                   GdkEventFocus *event,
                   CcWindow      *shell)
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
get_item_views (CcWindow *shell)
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
is_prev_direction (GtkWidget *widget,
                   GtkDirectionType direction)
{
  if (gtk_widget_get_direction (widget) == GTK_TEXT_DIR_LTR &&
      direction == GTK_DIR_LEFT)
    return TRUE;
  if (gtk_widget_get_direction (widget) == GTK_TEXT_DIR_RTL &&
      direction == GTK_DIR_RIGHT)
    return TRUE;
  return FALSE;
}

static gboolean
is_next_direction (GtkWidget *widget,
                   GtkDirectionType direction)
{
  if (gtk_widget_get_direction (widget) == GTK_TEXT_DIR_LTR &&
      direction == GTK_DIR_RIGHT)
    return TRUE;
  if (gtk_widget_get_direction (widget) == GTK_TEXT_DIR_RTL &&
      direction == GTK_DIR_LEFT)
    return TRUE;
  return FALSE;
}

static GtkTreePath *
get_first_path (GtkIconView *view)
{
  GtkTreeModel *model;
  GtkTreeIter iter;

  model = gtk_icon_view_get_model (view);
  if (!gtk_tree_model_get_iter_first (model, &iter))
    return NULL;
  return gtk_tree_model_get_path (model, &iter);
}

static GtkTreePath *
get_last_path (GtkIconView *view)
{
  GtkTreeModel *model;
  GtkTreeIter iter;
  GtkTreePath *path;
  gboolean ret;

  model = gtk_icon_view_get_model (view);
  if (!gtk_tree_model_get_iter_first (model, &iter))
    return NULL;

  ret = TRUE;
  path = NULL;

  while (ret)
    {
      g_clear_pointer (&path, gtk_tree_path_free);
      path = gtk_tree_model_get_path (model, &iter);
      ret = gtk_tree_model_iter_next (model, &iter);
    }
  return path;
}

static gboolean
categories_keynav_failed (GtkIconView      *current_view,
                          GtkDirectionType  direction,
                          CcWindow         *shell)
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

  new_view = NULL;

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

  if (is_prev_direction (GTK_WIDGET (current_view), direction) && v != NULL)
    {
      if (gtk_icon_view_get_cursor (current_view, &path, NULL))
        {
          if (v->prev)
            new_view = v->prev->data;

          if (gtk_tree_path_prev (path))
            {
              new_view = current_view;
            }
          else if (new_view != NULL)
            {
              path = get_last_path (new_view);
            }
          else
            {
              goto out;
            }

          gtk_icon_view_set_cursor (new_view, path, NULL, FALSE);
          gtk_icon_view_select_path (new_view, path);
          gtk_tree_path_free (path);
          gtk_widget_grab_focus (GTK_WIDGET (new_view));

          res = TRUE;
        }
    }

  if (is_next_direction (GTK_WIDGET (current_view), direction) && v != NULL)
    {
      if (gtk_icon_view_get_cursor (current_view, &path, NULL))
        {
          GtkTreeIter iter;

          if (v->next)
            new_view = v->next->data;

          gtk_tree_path_next (path);
          model = gtk_icon_view_get_model (current_view);

          if (gtk_tree_model_get_iter (model, &iter, path))
            {
              new_view = current_view;
            }
          else if (new_view != NULL)
            {
              path = get_first_path (new_view);
            }
          else
            {
              goto out;
            }

          gtk_icon_view_set_cursor (new_view, path, NULL, FALSE);
          gtk_icon_view_select_path (new_view, path);
          gtk_tree_path_free (path);
          gtk_widget_grab_focus (GTK_WIDGET (new_view));

          res = TRUE;
        }
    }

out:
  g_list_free (views);

  return res;
}

static gboolean
model_filter_func (GtkTreeModel    *model,
                   GtkTreeIter     *iter,
                   CcWindowPrivate *priv)
{
  char **terms, **t;
  gboolean matches = FALSE;

  if (!priv->filter_string)
    return FALSE;

  terms = g_strsplit (priv->filter_string, " ", -1);
  for (t = terms; *t; t++)
    {
      matches = cc_shell_model_iter_matches_search (CC_SHELL_MODEL (model),
                                                    iter,
                                                    *t);
      if (!matches)
        break;
    }
  g_strfreev (terms);

  return matches;
}

static gboolean
category_filter_func (GtkTreeModel    *model,
                      GtkTreeIter     *iter,
                      CcPanelCategory  filter)
{
  guint category;

  gtk_tree_model_get (model, iter, COL_CATEGORY, &category, -1);

  return (category == filter);
}

static void
search_entry_changed_cb (GtkEntry *entry,
                         CcWindow *center)
{
  CcWindowPrivate *priv = center->priv;
  char *str;

  /* if the entry text was set manually (not by the user) */
  if (!g_strcmp0 (priv->filter_string, gtk_entry_get_text (entry)))
    return;

  /* Don't re-filter for added trailing or leading spaces */
  str = cc_util_normalize_casefold_and_unaccent (gtk_entry_get_text (entry));
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
      gd_stack_set_visible_child_name (GD_STACK (priv->stack), SEARCH_PAGE);
    }
}

static gboolean
search_entry_key_press_event_cb (GtkEntry        *entry,
                                 GdkEventKey     *event,
                                 CcWindow        *self)
{
  CcWindowPrivate *priv = self->priv;

  if (event->keyval == GDK_KEY_Return &&
      g_strcmp0 (priv->filter_string, "") != 0)
    {
      GtkTreePath *path;
      GtkTreeSelection *selection;

      path = gtk_tree_path_new_first ();

      selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (self->priv->search_view));
      gtk_tree_selection_select_path (selection, path);

      if (!gtk_tree_selection_path_is_selected (selection, path))
        {
          gtk_tree_path_free (path);
          return FALSE;
        }

      gtk_tree_view_row_activated (GTK_TREE_VIEW (priv->search_view), path,
                                   gtk_tree_view_get_column (GTK_TREE_VIEW (self->priv->search_view), 0));
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
on_search_row_activated (GtkTreeView       *treeview,
                         GtkTreePath       *path,
                         GtkTreeViewColumn *column,
                         CcWindow          *shell)
{
  GtkTreeSelection *selection;
  GtkTreeModel *model;
  GtkTreeIter   iter;
  char         *id = NULL;

  selection = gtk_tree_view_get_selection (treeview);

  if (!gtk_tree_selection_get_selected (selection, &model, &iter))
    return;

  gtk_tree_model_get (model, &iter,
                      COL_ID, &id,
                      -1);

  if (id)
    cc_window_set_active_panel_from_id (CC_SHELL (shell), id, NULL, NULL);

  gtk_tree_selection_unselect_all (selection);

  g_free (id);
}

static gboolean
on_search_button_press_event (GtkTreeView    *treeview,
                              GdkEventButton *event,
                              CcWindow       *shell)
{
  if (event->type == GDK_BUTTON_PRESS && event->button == 1)
    {
      GtkTreePath *path = NULL;
      GtkTreeSelection *selection;
      GtkTreeModel *model;
      GtkTreeIter iter;

      /* We don't check for the position being blank,
       * it could be the dead space between columns */
      gtk_tree_view_is_blank_at_pos (treeview,
                                     event->x, event->y,
                                     &path,
                                     NULL,
                                     NULL,
                                     NULL);
      if (path == NULL)
        return FALSE;

      model = gtk_tree_view_get_model (treeview);
      if (gtk_tree_model_get_iter (model, &iter, path) == FALSE)
        {
          gtk_tree_path_free (path);
          return FALSE;
        }

      selection = gtk_tree_view_get_selection (treeview);
      gtk_tree_selection_select_iter (selection, &iter);

      on_search_row_activated (treeview, NULL, NULL, shell);

      gtk_tree_path_free (path);

      return TRUE;
    }

  return FALSE;
}

static void
setup_search (CcWindow *shell)
{
  GtkWidget *search_view;
  GtkCellRenderer *renderer;
  GtkTreeViewColumn *column;
  CcWindowPrivate *priv = shell->priv;

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

  renderer = gd_styled_text_renderer_new ();
  gd_styled_text_renderer_add_class (GD_STYLED_TEXT_RENDERER (renderer), "dim-label");
  g_object_set (renderer,
                "xpad", 15,
                NULL);
  column = gtk_tree_view_column_new_with_attributes ("Description", renderer,
                                                     "text", COL_DESCRIPTION,
                                                     NULL);
  gtk_tree_view_column_set_expand (column, TRUE);
  gtk_tree_view_append_column (GTK_TREE_VIEW (priv->search_view), column);

  gtk_container_add (GTK_CONTAINER (priv->search_scrolled), search_view);

  g_signal_connect (priv->search_view, "row-activated",
                    G_CALLBACK (on_search_row_activated), shell);
  g_signal_connect (priv->search_view, "button-press-event",
                    G_CALLBACK (on_search_button_press_event), shell);

  priv->filter_string = g_strdup ("");

  gtk_widget_show (priv->search_view);
}

static void
add_category_view (CcWindow        *shell,
                   CcPanelCategory  category,
                   const char      *name)
{
  GtkTreeModel *filter;
  GtkWidget *categoryview;

  if (category > 0)
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
                                          GINT_TO_POINTER (category), NULL);

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
                    G_CALLBACK (categories_keynav_failed), shell);
}

static void
setup_model (CcWindow *shell)
{
  CcWindowPrivate *priv = shell->priv;

  priv->store = (GtkListStore *) cc_shell_model_new ();

  /* Add categories */
  add_category_view (shell, CC_CATEGORY_PERSONAL, C_("category", "Personal"));
  add_category_view (shell, CC_CATEGORY_HARDWARE, C_("category", "Hardware"));
  add_category_view (shell, CC_CATEGORY_SYSTEM, C_("category", "System"));

  cc_panel_loader_fill_model (CC_SHELL_MODEL (shell->priv->store));
}

static void
previous_button_clicked_cb (GtkButton *button,
                            CcWindow  *shell)
{
  g_debug ("Num previous panels? %d", g_queue_get_length (shell->priv->previous_panels));
  if (g_queue_is_empty (shell->priv->previous_panels)) {
    shell_show_overview_page (shell);
  } else {
    char *panel_name;

    panel_name = g_queue_pop_head (shell->priv->previous_panels);
    g_debug ("About to go to previous panel '%s'", panel_name);
    cc_window_set_active_panel_from_id (CC_SHELL (shell), panel_name, NULL, NULL);
    g_free (panel_name);
  }
}

static void
stack_page_notify_cb (GdStack     *stack,
                      GParamSpec  *spec,
                      CcWindow    *self)
{
  CcWindowPrivate *priv = self->priv;
  int nat_height;
  const char *id;

  id = gd_stack_get_visible_child_name (stack);

  /* make sure the home button is shown on all pages except the overview page */

  if (g_strcmp0 (id, OVERVIEW_PAGE) == 0 || g_strcmp0 (id, SEARCH_PAGE) == 0)
    {
      gint header_height, maximum_height;

      gtk_widget_hide (priv->previous_button);
      gtk_widget_show (priv->search_entry);
      gtk_widget_hide (priv->lock_button);

      gtk_widget_get_preferred_height_for_width (GTK_WIDGET (priv->main_vbox),
                                                 FIXED_WIDTH, NULL, &nat_height);
      gtk_widget_get_preferred_height_for_width (GTK_WIDGET (priv->header),
                                                 FIXED_WIDTH, NULL, &header_height);

      /* find the maximum height by using the monitor height minus an allowance
       * for title bar, etc. */
      maximum_height = get_monitor_height (self) - 100;

      if (maximum_height > 0 && nat_height + header_height > maximum_height)
        nat_height = maximum_height - header_height;

      gtk_scrolled_window_set_min_content_height (GTK_SCROLLED_WINDOW (priv->scrolled_window),
                                                  priv->small_screen == SMALL_SCREEN_TRUE ? SMALL_SCREEN_FIXED_HEIGHT : nat_height);
    }
  else
    {
      gtk_widget_show (priv->previous_button);
      gtk_widget_hide (priv->search_entry);
      /* set the scrolled window small so that it doesn't force
         the window to be larger than this panel */
      gtk_widget_get_preferred_height_for_width (GTK_WIDGET (self),
                                                 FIXED_WIDTH, NULL, &nat_height);
      gtk_scrolled_window_set_min_content_height (GTK_SCROLLED_WINDOW (priv->scrolled_window), MIN_ICON_VIEW_HEIGHT);
      gtk_window_resize (GTK_WINDOW (self),
                         FIXED_WIDTH,
                         nat_height);
    }
}

/* CcShell implementation */
static void
_shell_embed_widget_in_header (CcShell      *shell,
                               GtkWidget    *widget)
{
  CcWindowPrivate *priv = CC_WINDOW (shell)->priv;

  /* add to header */
  gtk_box_pack_end (GTK_BOX (priv->top_right_box), widget, FALSE, FALSE, 0);
  g_ptr_array_add (priv->custom_widgets, g_object_ref (widget));
}

/* CcShell implementation */
static gboolean
cc_window_set_active_panel_from_id (CcShell      *shell,
                                    const gchar  *start_id,
                                    const gchar **argv,
                                    GError      **err)
{
  GtkTreeIter iter;
  gboolean iter_valid;
  gchar *name = NULL;
  GIcon *gicon = NULL;
  CcWindowPrivate *priv = CC_WINDOW (shell)->priv;
  GtkWidget *old_panel;

  /* When loading the same panel again, just set the argv */
  if (g_strcmp0 (priv->current_panel_id, start_id) == 0)
    {
      g_object_set (G_OBJECT (priv->current_panel), "argv", argv, NULL);
      return TRUE;
    }

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
          if (gicon)
            g_object_unref (gicon);

          name = NULL;
          id = NULL;
          gicon = NULL;
        }

      iter_valid = gtk_tree_model_iter_next (GTK_TREE_MODEL (priv->store),
                                             &iter);
    }

  old_panel = priv->current_panel_box;

  if (!name)
    {
      g_warning ("Could not find settings panel \"%s\"", start_id);
    }
  else if (activate_panel (CC_WINDOW (shell), start_id, argv,
                           name, gicon) == FALSE)
    {
      /* Failed to activate the panel for some reason,
       * let's keep the old panel around instead */
    }
  else
    {
      /* Successful activation */
      g_free (priv->current_panel_id);
      priv->current_panel_id = g_strdup (start_id);

      if (old_panel)
        gtk_container_remove (GTK_CONTAINER (priv->stack), old_panel);
    }

  g_free (name);
  if (gicon)
    g_object_unref (gicon);

  return TRUE;
}

static gboolean
_shell_set_active_panel_from_id (CcShell      *shell,
                                 const gchar  *start_id,
                                 const gchar **argv,
                                 GError      **err)
{
  add_current_panel_to_history (shell, start_id);
  return cc_window_set_active_panel_from_id (shell, start_id, argv, err);
}

static GtkWidget *
_shell_get_toplevel (CcShell *shell)
{
  return GTK_WIDGET (shell);
}

/* GObject Implementation */
static void
cc_window_get_property (GObject    *object,
                        guint       property_id,
                        GValue     *value,
                        GParamSpec *pspec)
{
  CcWindowPrivate *priv = CC_WINDOW (object)->priv;

  switch (property_id)
    {
    case PROP_ACTIVE_PANEL:
      g_value_set_object (value, priv->active_panel);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
    }
}

static void
set_active_panel (CcWindow *shell,
                  CcPanel *panel)
{
  g_return_if_fail (CC_IS_SHELL (shell));
  g_return_if_fail (panel == NULL || CC_IS_PANEL (panel));

  if (panel != shell->priv->active_panel)
    {
      /* remove the old panel */
      g_clear_object (&shell->priv->active_panel);

      /* set the new panel */
      if (panel)
        {
          shell->priv->active_panel = g_object_ref (panel);
        }
      else
        {
          shell_show_overview_page (shell);
        }
      g_object_notify (G_OBJECT (shell), "active-panel");
    }
}

static void
cc_window_set_property (GObject      *object,
                        guint         property_id,
                        const GValue *value,
                        GParamSpec   *pspec)
{
  CcWindow *shell = CC_WINDOW (object);

  switch (property_id)
    {
    case PROP_ACTIVE_PANEL:
      set_active_panel (shell, g_value_get_object (value));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
    }
}

static void
cc_window_dispose (GObject *object)
{
  CcWindowPrivate *priv = CC_WINDOW (object)->priv;

  /* Avoid receiving notifications about the pages changing
   * when destroying the children one-by-one */
  if (priv->stack)
    {
      g_signal_handlers_disconnect_by_func (priv->stack, stack_page_notify_cb, object);
      priv->stack = NULL;
    }

  g_free (priv->current_panel_id);
  priv->current_panel_id = NULL;

  if (priv->custom_widgets)
    {
      g_ptr_array_unref (priv->custom_widgets);
      priv->custom_widgets = NULL;
    }

  g_clear_object (&priv->store);
  g_clear_object (&priv->search_filter);
  g_clear_object (&priv->active_panel);

  if (priv->previous_panels)
    {
      g_queue_free_full (priv->previous_panels, g_free);
      priv->previous_panels = NULL;
    }

  G_OBJECT_CLASS (cc_window_parent_class)->dispose (object);
}

static void
cc_window_finalize (GObject *object)
{
  CcWindowPrivate *priv = CC_WINDOW (object)->priv;

  g_free (priv->filter_string);

  G_OBJECT_CLASS (cc_window_parent_class)->finalize (object);
}

static void
cc_shell_iface_init (CcShellInterface *iface)
{
  iface->set_active_panel_from_id = _shell_set_active_panel_from_id;
  iface->embed_widget_in_header = _shell_embed_widget_in_header;
  iface->get_toplevel = _shell_get_toplevel;
}

static void
cc_window_class_init (CcWindowClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  g_type_class_add_private (klass, sizeof (CcWindowPrivate));

  object_class->get_property = cc_window_get_property;
  object_class->set_property = cc_window_set_property;
  object_class->dispose = cc_window_dispose;
  object_class->finalize = cc_window_finalize;

  g_object_class_override_property (object_class, PROP_ACTIVE_PANEL, "active-panel");
}

static gboolean
window_key_press_event (GtkWidget   *win,
                        GdkEventKey *event,
                        CcWindow    *self)
{
  GdkKeymap *keymap;
  gboolean retval;
  GdkModifierType state;
  gboolean is_rtl;

  if (event->state == 0)
    return FALSE;

  retval = FALSE;
  state = event->state;
  keymap = gdk_keymap_get_default ();
  gdk_keymap_add_virtual_modifiers (keymap, &state);
  state = state & gtk_accelerator_get_default_mod_mask ();
  is_rtl = gtk_widget_get_direction (win) == GTK_TEXT_DIR_RTL;

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
            gtk_widget_destroy (GTK_WIDGET (self));
            retval = TRUE;
            break;
          case GDK_KEY_W:
          case GDK_KEY_w:
            if (g_strcmp0 (gd_stack_get_visible_child_name (GD_STACK (self->priv->stack)), OVERVIEW_PAGE) != 0)
              shell_show_overview_page (self);
            retval = TRUE;
            break;
        }
    }
  else if (state == GDK_MOD1_MASK && event->keyval == GDK_KEY_Up)
    {
      if (g_strcmp0 (gd_stack_get_visible_child_name (GD_STACK (self->priv->stack)), OVERVIEW_PAGE) != 0)
        shell_show_overview_page (self);
      retval = TRUE;
    }
  else if ((!is_rtl && state == GDK_MOD1_MASK && event->keyval == GDK_KEY_Left) ||
           (is_rtl && state == GDK_MOD1_MASK && event->keyval == GDK_KEY_Right) ||
           event->keyval == GDK_KEY_Back)
    {
      previous_button_clicked_cb (NULL, self);
      retval = TRUE;
    }
  return retval;
}

static gint
get_monitor_height (CcWindow *self)
{
  GdkScreen *screen;
  GdkRectangle rect;

  /* We cannot use workarea here, as this wouldn't
   * be updated when we read it after a monitors-changed signal */
  screen = gtk_widget_get_screen (GTK_WIDGET (self));
  gdk_screen_get_monitor_geometry (screen, self->priv->monitor_num, &rect);

  return rect.height;
}

static gboolean
update_monitor_number (CcWindow *self)
{
  gboolean changed = FALSE;
  GtkWidget *widget;
  GdkScreen *screen;
  GdkWindow *window;
  int monitor;

  widget = GTK_WIDGET (self);

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
is_small (CcWindow *self)
{
  if (get_monitor_height (self) <= FIXED_HEIGHT)
    return SMALL_SCREEN_TRUE;
  return SMALL_SCREEN_FALSE;
}

static void
update_small_screen_settings (CcWindow *self)
{
  CcSmallScreen small;

  update_monitor_number (self);
  small = is_small (self);

  if (small == SMALL_SCREEN_TRUE)
    {
      gtk_window_set_resizable (GTK_WINDOW (self), TRUE);

      if (self->priv->small_screen != small)
        gtk_window_maximize (GTK_WINDOW (self));
    }
  else
    {
      if (self->priv->small_screen != small)
        gtk_window_unmaximize (GTK_WINDOW (self));

      gtk_window_set_resizable (GTK_WINDOW (self), FALSE);
    }

  self->priv->small_screen = small;

  /* And update the minimum sizes */
  stack_page_notify_cb (GD_STACK (self->priv->stack), NULL, self);
}

static gboolean
main_window_configure_cb (GtkWidget *widget,
                          GdkEvent  *event,
                          CcWindow  *self)
{
  update_small_screen_settings (self);
  return FALSE;
}

static void
application_set_cb (GObject    *object,
                    GParamSpec *pspec,
                    CcWindow   *self)
{
  /* update small screen settings now - to avoid visible resizing, we want
   * to do it before showing the window, and GtkApplicationWindow cannot be
   * realized unless its application property has been set */
  if (gtk_window_get_application (GTK_WINDOW (self)))
    {
      gtk_widget_realize (GTK_WIDGET (self));
      update_small_screen_settings (self);
    }
}

static void
monitors_changed_cb (GdkScreen *screen,
                     CcWindow  *self)
{
  /* We reset small_screen_set to make sure that the
   * window gets maximised if need be, in update_small_screen_settings() */
  self->priv->small_screen = SMALL_SCREEN_UNSET;
  update_small_screen_settings (self);
}

static void
gdk_window_set_cb (GObject    *object,
                   GParamSpec *pspec,
                   CcWindow   *self)
{
  GdkWindow *window;
  gchar *str;

  if (!GDK_IS_X11_DISPLAY (gdk_display_get_default ()))
    return;

  window = gtk_widget_get_window (GTK_WIDGET (self));

  if (!window)
    return;

  str = g_strdup_printf ("%u", (guint) GDK_WINDOW_XID (window));
  g_setenv ("GNOME_CONTROL_CENTER_XID", str, TRUE);
  g_free (str);
}

static void
create_main_page (CcWindow *self)
{
  CcWindowPrivate *priv = self->priv;
  GtkStyleContext *context;

  priv->scrolled_window = gtk_scrolled_window_new (NULL, NULL);
  context = gtk_widget_get_style_context (priv->scrolled_window);
  gtk_style_context_add_class (context, "view");
  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (priv->scrolled_window),
                                  GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
  gd_stack_add_named (GD_STACK (priv->stack), priv->scrolled_window, OVERVIEW_PAGE);

  priv->main_vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
  gtk_widget_set_margin_top (priv->main_vbox, 8);
  gtk_widget_set_margin_bottom (priv->main_vbox, 8);
  gtk_widget_set_margin_left (priv->main_vbox, 12);
  gtk_widget_set_margin_right (priv->main_vbox, 12);
  gtk_container_set_focus_vadjustment (GTK_CONTAINER (priv->main_vbox),
                                       gtk_scrolled_window_get_vadjustment (GTK_SCROLLED_WINDOW (priv->scrolled_window)));
  gtk_container_add (GTK_CONTAINER (priv->scrolled_window), priv->main_vbox);

  gtk_widget_set_size_request (priv->scrolled_window, FIXED_WIDTH, -1);

  /* load the available settings panels */
  setup_model (self);
}

static void
create_search_page (CcWindow *self)
{
  CcWindowPrivate *priv = self->priv;

  priv->search_scrolled = gtk_scrolled_window_new (NULL, NULL);
  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (priv->search_scrolled),
                                  GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
  gd_stack_add_named (GD_STACK (priv->stack), priv->search_scrolled, SEARCH_PAGE);

  /* setup search functionality */
  setup_search (self);
}

static void
create_header (CcWindow *self)
{
  CcWindowPrivate *priv = self->priv;
  GtkWidget *button;
  AtkObject *accessible;
  gboolean rtl;

  rtl = (gtk_widget_get_default_direction () == GTK_TEXT_DIR_RTL);

  priv->header = gd_header_bar_new ();

  priv->previous_button = button = gd_header_simple_button_new ();
  gd_header_button_set_symbolic_icon_name (GD_HEADER_BUTTON (button),
                                           rtl ? "go-previous-rtl-symbolic" : "go-previous-symbolic");
  gtk_widget_set_no_show_all (button, TRUE);
  accessible = gtk_widget_get_accessible (button);
  atk_object_set_name (accessible, _("All Settings"));
  gd_header_bar_pack_start (GD_HEADER_BAR (priv->header), button);
  g_signal_connect (button, "clicked", G_CALLBACK (previous_button_clicked_cb), self);

  priv->top_right_box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
  gd_header_bar_pack_end (GD_HEADER_BAR (priv->header), priv->top_right_box);

  priv->search_entry = gtk_search_entry_new ();
  gtk_container_add (GTK_CONTAINER (priv->top_right_box), priv->search_entry);
  gtk_entry_set_width_chars (GTK_ENTRY (priv->search_entry), 30);
  gtk_entry_set_invisible_char (GTK_ENTRY (priv->search_entry), 9679);
  g_signal_connect (priv->search_entry, "changed", G_CALLBACK (search_entry_changed_cb), self);
  g_signal_connect (priv->search_entry, "key-press-event", G_CALLBACK (search_entry_key_press_event_cb), self);

  priv->lock_button = gtk_lock_button_new (NULL);
  gtk_widget_set_no_show_all (button, TRUE);
  gtk_container_add (GTK_CONTAINER (priv->top_right_box), priv->lock_button);
}

static void
create_window (CcWindow *self)
{
  CcWindowPrivate *priv = self->priv;
  GtkWidget *box;
  GdkScreen *screen;

  box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
  gtk_container_add (GTK_CONTAINER (self), box);

  create_header (self);
  gtk_box_pack_start (GTK_BOX (box), priv->header, FALSE, FALSE, 0);

  priv->stack = gd_stack_new ();
  gd_stack_set_homogeneous (GD_STACK (priv->stack), TRUE);
  gd_stack_set_transition_type (GD_STACK (priv->stack), GD_STACK_TRANSITION_TYPE_CROSSFADE);
  gtk_box_pack_start (GTK_BOX (box), priv->stack, TRUE, TRUE, 0);

  create_main_page (self);
  create_search_page (self);

  /* connect various signals */
  screen = gtk_widget_get_screen (GTK_WIDGET (self));
  g_signal_connect (screen, "monitors-changed", G_CALLBACK (monitors_changed_cb), self);

  g_signal_connect (self, "configure-event", G_CALLBACK (main_window_configure_cb), self);
  g_signal_connect (self, "notify::application", G_CALLBACK (application_set_cb), self);
  g_signal_connect_after (self, "key_press_event",
                          G_CALLBACK (window_key_press_event), self);
  g_signal_connect (self, "notify::window", G_CALLBACK (gdk_window_set_cb), self);

  g_signal_connect (priv->stack, "notify::visible-child",
                    G_CALLBACK (stack_page_notify_cb), self);

  gtk_widget_show_all (box);
}

static void
cc_window_init (CcWindow *self)
{
  CcWindowPrivate *priv;

  priv = self->priv = WINDOW_PRIVATE (self);

  priv->monitor_num = -1;
  self->priv->small_screen = SMALL_SCREEN_UNSET;

  create_window (self);

  self->priv->previous_panels = g_queue_new ();

  /* keep a list of custom widgets to unload on panel change */
  priv->custom_widgets = g_ptr_array_new_with_free_func ((GDestroyNotify) g_object_unref);

  stack_page_notify_cb (GD_STACK (priv->stack), NULL, self);
}

CcWindow *
cc_window_new (GtkApplication *application)
{
  g_return_val_if_fail (GTK_IS_APPLICATION (application), NULL);

  return g_object_new (CC_TYPE_WINDOW,
                       "application", application,
                       "hide-titlebar-when-maximized", TRUE,
                       "resizable", TRUE,
                       "title", _(DEFAULT_WINDOW_TITLE),
                       "icon-name", DEFAULT_WINDOW_ICON_NAME,
                       "window-position", GTK_WIN_POS_CENTER,
                       NULL);
}

void
cc_window_present (CcWindow *center)
{
  gtk_window_present (GTK_WINDOW (center));
}

void
cc_window_show (CcWindow *center)
{
  gtk_window_present (GTK_WINDOW (center));
}
