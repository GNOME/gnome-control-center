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
#include <libgd/gd.h>

#include "cc-panel.h"
#include "cc-shell.h"
#include "cc-shell-category-view.h"
#include "cc-shell-model.h"
#include "cc-panel-loader.h"
#include "cc-util.h"

/* Use a fixed width for the shell, since resizing horizontally is more awkward
 * for the user than resizing vertically
 * Both sizes are defined in https://live.gnome.org/Design/SystemSettings/ */
#define FIXED_WIDTH 740
#define FIXED_HEIGHT 636
#define SMALL_SCREEN_FIXED_HEIGHT 400

#define MIN_ICON_VIEW_HEIGHT 300

#define MOUSE_BACK_BUTTON 8

#define DEFAULT_WINDOW_TITLE N_("All Settings")
#define DEFAULT_WINDOW_ICON_NAME "preferences-system"

#define SEARCH_PAGE "_search"
#define OVERVIEW_PAGE "_overview"

typedef enum {
	SMALL_SCREEN_UNSET,
	SMALL_SCREEN_TRUE,
	SMALL_SCREEN_FALSE
} CcSmallScreen;

struct _CcWindow
{
  GtkApplicationWindow parent;

  GtkWidget  *stack;
  GtkWidget  *header;
  GtkWidget  *main_vbox;
  GtkWidget  *scrolled_window;
  GtkWidget  *search_scrolled;
  GtkWidget  *previous_button;
  GtkWidget  *top_right_box;
  GtkWidget  *search_button;
  GtkWidget  *search_bar;
  GtkWidget  *search_entry;
  GtkWidget  *lock_button;
  GtkWidget  *current_panel_box;
  GtkWidget  *current_panel;
  char       *current_panel_id;
  GQueue     *previous_panels;

  GtkSizeGroup *header_sizegroup;

  GPtrArray  *custom_widgets;

  GtkListStore *store;

  GtkTreeModel *search_filter;
  GtkWidget *search_view;
  gchar *filter_string;
  gchar **filter_terms;

  CcPanel *active_panel;

  int monitor_num;
  CcSmallScreen small_screen;
};

static void     cc_shell_iface_init         (CcShellInterface      *iface);

G_DEFINE_TYPE_WITH_CODE (CcWindow, cc_window, GTK_TYPE_APPLICATION_WINDOW,
                         G_IMPLEMENT_INTERFACE (CC_TYPE_SHELL, cc_shell_iface_init))

enum
{
  PROP_0,
  PROP_ACTIVE_PANEL
};

static gboolean cc_window_set_active_panel_from_id (CcShell      *shell,
                                                    const gchar  *start_id,
                                                    GVariant     *parameters,
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
                GVariant           *parameters,
                const gchar        *name,
                GIcon              *gicon)
{
  GtkWidget *box, *title_widget;
  const gchar *icon_name;

  if (!id)
    return FALSE;

  self->current_panel = GTK_WIDGET (cc_panel_loader_load_by_name (CC_SHELL (self), id, parameters));
  cc_shell_set_active_panel (CC_SHELL (self), CC_PANEL (self->current_panel));
  gtk_widget_show (self->current_panel);

  gtk_lock_button_set_permission (GTK_LOCK_BUTTON (self->lock_button),
                                  cc_panel_get_permission (CC_PANEL (self->current_panel)));

  box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);

  gtk_box_pack_start (GTK_BOX (box), self->current_panel,
                      TRUE, TRUE, 0);

  gtk_stack_add_named (GTK_STACK (self->stack), box, id);

  /* switch to the new panel */
  gtk_widget_show (box);
  gtk_stack_set_visible_child_name (GTK_STACK (self->stack), id);

  /* set the title of the window */
  icon_name = get_icon_name_from_g_icon (gicon);

  gtk_window_set_role (GTK_WINDOW (self), id);
  gtk_header_bar_set_title (GTK_HEADER_BAR (self->header), name);
  gtk_window_set_default_icon_name (icon_name);
  gtk_window_set_icon_name (GTK_WINDOW (self), icon_name);

  title_widget = cc_panel_get_title_widget (CC_PANEL (self->current_panel));
  gtk_header_bar_set_custom_title (GTK_HEADER_BAR (self->header), title_widget);

  self->current_panel_box = box;

  return TRUE;
}

static void
_shell_remove_all_custom_widgets (CcWindow *self)
{
  GtkWidget *widget;
  guint i;

  /* remove from the header */
  for (i = 0; i < self->custom_widgets->len; i++)
    {
        widget = g_ptr_array_index (self->custom_widgets, i);
        gtk_container_remove (GTK_CONTAINER (self->top_right_box), widget);
    }
  g_ptr_array_set_size (self->custom_widgets, 0);
}

static void
add_current_panel_to_history (CcShell    *shell,
                              const char *start_id)
{
  CcWindow *self;

  g_return_if_fail (start_id != NULL);

  self = CC_WINDOW (shell);

  if (!self->current_panel_id ||
      g_strcmp0 (self->current_panel_id, start_id) == 0)
    return;

  g_queue_push_head (self->previous_panels, g_strdup (self->current_panel_id));
  g_debug ("Added '%s' to the previous panels", self->current_panel_id);
}

static void
shell_show_overview_page (CcWindow *self)
{
  gtk_stack_set_visible_child_name (GTK_STACK (self->stack), OVERVIEW_PAGE);

  if (self->current_panel_box)
    gtk_container_remove (GTK_CONTAINER (self->stack), self->current_panel_box);
  self->current_panel = NULL;
  self->current_panel_box = NULL;
  g_clear_pointer (&self->current_panel_id, g_free);

  /* Clear the panel history */
  g_queue_free_full (self->previous_panels, g_free);
  self->previous_panels = g_queue_new ();

  /* clear the search text */
  g_free (self->filter_string);
  self->filter_string = g_strdup ("");
  gtk_entry_set_text (GTK_ENTRY (self->search_entry), "");
  if (gtk_search_bar_get_search_mode (GTK_SEARCH_BAR (self->search_bar)))
    gtk_widget_grab_focus (self->search_entry);

  gtk_lock_button_set_permission (GTK_LOCK_BUTTON (self->lock_button), NULL);

  /* reset window title and icon */
  gtk_window_set_role (GTK_WINDOW (self), NULL);
  gtk_header_bar_set_title (GTK_HEADER_BAR (self->header), _(DEFAULT_WINDOW_TITLE));
  gtk_header_bar_set_custom_title (GTK_HEADER_BAR (self->header), NULL);
  gtk_window_set_default_icon_name (DEFAULT_WINDOW_ICON_NAME);
  gtk_window_set_icon_name (GTK_WINDOW (self), DEFAULT_WINDOW_ICON_NAME);

  cc_shell_set_active_panel (CC_SHELL (self), NULL);

  /* clear any custom widgets */
  _shell_remove_all_custom_widgets (self);
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
  gtk_search_bar_set_search_mode (GTK_SEARCH_BAR (center->search_bar), TRUE);
  gtk_entry_set_text (GTK_ENTRY (center->search_entry), search);
  gtk_editable_set_position (GTK_EDITABLE (center->search_entry), -1);
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

  list = gtk_container_get_children (GTK_CONTAINER (shell->main_vbox));
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
          g_assert (gtk_tree_model_get_iter_first (model, &iter));
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
          g_assert (gtk_tree_model_get_iter_first (model, &iter));
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
model_filter_func (GtkTreeModel *model,
                   GtkTreeIter  *iter,
                   CcWindow     *self)
{
  char **t;
  gboolean matches = FALSE;

  if (!self->filter_string || !self->filter_terms)
    return FALSE;

  for (t = self->filter_terms; *t; t++)
    {
      matches = cc_shell_model_iter_matches_search (CC_SHELL_MODEL (model),
                                                    iter,
                                                    *t);
      if (!matches)
        break;
    }

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
                         CcWindow *self)
{
  char *str;

  /* if the entry text was set manually (not by the user) */
  if (!g_strcmp0 (self->filter_string, gtk_entry_get_text (entry)))
    {
      cc_shell_model_set_sort_terms (CC_SHELL_MODEL (self->store), NULL);
      return;
    }

  /* Don't re-filter for added trailing or leading spaces */
  str = cc_util_normalize_casefold_and_unaccent (gtk_entry_get_text (entry));
  g_strstrip (str);
  if (!g_strcmp0 (str, self->filter_string))
    {
      g_free (str);
      return;
    }

  g_free (self->filter_string);
  self->filter_string = str;

  g_strfreev (self->filter_terms);
  self->filter_terms = g_strsplit (self->filter_string, " ", -1);

  cc_shell_model_set_sort_terms (CC_SHELL_MODEL (self->store), self->filter_terms);

  if (!g_strcmp0 (self->filter_string, ""))
    {
      shell_show_overview_page (self);
    }
  else
    {
      gtk_tree_model_filter_refilter (GTK_TREE_MODEL_FILTER (self->search_filter));
      gtk_stack_set_visible_child_name (GTK_STACK (self->stack), SEARCH_PAGE);
    }
}

static gboolean
search_entry_key_press_event_cb (GtkEntry        *entry,
                                 GdkEventKey     *event,
                                 CcWindow        *self)
{
  if (event->keyval == GDK_KEY_Return &&
      g_strcmp0 (self->filter_string, "") != 0)
    {
      GtkTreePath *path;
      GtkTreeSelection *selection;

      path = gtk_tree_path_new_first ();

      selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (self->search_view));
      gtk_tree_selection_select_path (selection, path);

      if (!gtk_tree_selection_path_is_selected (selection, path))
        {
          gtk_tree_path_free (path);
          return FALSE;
        }

      gtk_tree_view_row_activated (GTK_TREE_VIEW (self->search_view), path,
                                   gtk_tree_view_get_column (GTK_TREE_VIEW (self->search_view), 0));
      gtk_tree_path_free (path);
      return TRUE;
    }

  if (event->keyval == GDK_KEY_Escape)
    {
      gtk_search_bar_set_search_mode (GTK_SEARCH_BAR (self->search_bar), FALSE);
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
setup_search (CcWindow *self)
{
  GtkWidget *search_view;
  GtkCellRenderer *renderer;
  GtkTreeViewColumn *column;

  g_return_if_fail (self->store != NULL);

  /* create the search filter */
  self->search_filter = gtk_tree_model_filter_new (GTK_TREE_MODEL (self->store),
                                                   NULL);

  gtk_tree_model_filter_set_visible_func (GTK_TREE_MODEL_FILTER (self->search_filter),
                                          (GtkTreeModelFilterVisibleFunc)
                                          model_filter_func,
                                          self, NULL);

  /* set up the search view */
  self->search_view = search_view = gtk_tree_view_new ();
  gtk_tree_view_set_headers_visible (GTK_TREE_VIEW (search_view), FALSE);
  gtk_tree_view_set_enable_search (GTK_TREE_VIEW (search_view), FALSE);
  gtk_tree_view_set_model (GTK_TREE_VIEW (search_view),
                           GTK_TREE_MODEL (self->search_filter));
  /* This needs to happen after setting the model, otherwise
   * the search column will be the first string column */
  gtk_tree_view_set_search_column (GTK_TREE_VIEW (search_view), -1);

  renderer = gtk_cell_renderer_pixbuf_new ();
  g_object_set (renderer,
                "xpad", 15,
                "ypad", 10,
                "stock-size", GTK_ICON_SIZE_DIALOG,
                "follow-state", TRUE,
                NULL);
  column = gtk_tree_view_column_new_with_attributes ("Icon", renderer,
                                                     "gicon", COL_GICON,
                                                     NULL);
  gtk_tree_view_column_set_expand (column, FALSE);
  gtk_tree_view_append_column (GTK_TREE_VIEW (self->search_view), column);

  renderer = gtk_cell_renderer_text_new ();
  g_object_set (renderer,
                "xpad", 0,
                NULL);
  column = gtk_tree_view_column_new_with_attributes ("Name", renderer,
                                                     "text", COL_NAME,
                                                     NULL);
  gtk_tree_view_column_set_expand (column, FALSE);
  gtk_tree_view_append_column (GTK_TREE_VIEW (self->search_view), column);

  renderer = gd_styled_text_renderer_new ();
  gd_styled_text_renderer_add_class (GD_STYLED_TEXT_RENDERER (renderer), "dim-label");
  g_object_set (renderer,
                "xpad", 15,
                "ellipsize", PANGO_ELLIPSIZE_END,
                NULL);
  column = gtk_tree_view_column_new_with_attributes ("Description", renderer,
                                                     "text", COL_DESCRIPTION,
                                                     NULL);
  gtk_tree_view_column_set_expand (column, TRUE);
  gtk_tree_view_append_column (GTK_TREE_VIEW (self->search_view), column);

  gtk_container_add (GTK_CONTAINER (self->search_scrolled), search_view);

  g_signal_connect (self->search_view, "row-activated",
                    G_CALLBACK (on_search_row_activated), self);
  g_signal_connect (self->search_view, "button-press-event",
                    G_CALLBACK (on_search_button_press_event), self);

  self->filter_string = g_strdup ("");

  gtk_widget_show (self->search_view);
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
      gtk_box_pack_start (GTK_BOX (shell->main_vbox), separator, FALSE, FALSE, 0);
      gtk_widget_show (separator);
    }

  /* create new category view for this category */
  filter = gtk_tree_model_filter_new (GTK_TREE_MODEL (shell->store),
                                      NULL);
  gtk_tree_model_filter_set_visible_func (GTK_TREE_MODEL_FILTER (filter),
                                          (GtkTreeModelFilterVisibleFunc) category_filter_func,
                                          GINT_TO_POINTER (category), NULL);

  categoryview = cc_shell_category_view_new (name, filter);
  gtk_box_pack_start (GTK_BOX (shell->main_vbox), categoryview, FALSE, TRUE, 0);

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
 shell->store = (GtkListStore *) cc_shell_model_new ();

  /* Add categories */
  add_category_view (shell, CC_CATEGORY_PERSONAL, C_("category", "Personal"));
  add_category_view (shell, CC_CATEGORY_HARDWARE, C_("category", "Hardware"));
  add_category_view (shell, CC_CATEGORY_SYSTEM, C_("category", "System"));

  cc_panel_loader_fill_model (CC_SHELL_MODEL (shell->store));
}

static void
previous_button_clicked_cb (GtkButton *button,
                            CcWindow  *shell)
{
  g_debug ("Num previous panels? %d", g_queue_get_length (shell->previous_panels));
  if (g_queue_is_empty (shell->previous_panels)) {
    shell_show_overview_page (shell);
  } else {
    char *panel_name;

    panel_name = g_queue_pop_head (shell->previous_panels);
    g_debug ("About to go to previous panel '%s'", panel_name);
    cc_window_set_active_panel_from_id (CC_SHELL (shell), panel_name, NULL, NULL);
    g_free (panel_name);
  }
}

static void
stack_page_notify_cb (GtkStack     *stack,
                      GParamSpec  *spec,
                      CcWindow    *self)
{
  int nat_height;
  const char *id;

  id = gtk_stack_get_visible_child_name (stack);

  /* make sure the home button is shown on all pages except the overview page */

  if (g_strcmp0 (id, OVERVIEW_PAGE) == 0 || g_strcmp0 (id, SEARCH_PAGE) == 0)
    {
      gint header_height, maximum_height;

      gtk_widget_hide (self->previous_button);
      gtk_widget_show (self->search_button);
      gtk_widget_show (self->search_bar);
      gtk_widget_hide (self->lock_button);

      gtk_widget_get_preferred_height_for_width (GTK_WIDGET (self->main_vbox),
                                                 FIXED_WIDTH, NULL, &nat_height);
      gtk_widget_get_preferred_height_for_width (GTK_WIDGET (self->header),
                                                 FIXED_WIDTH, NULL, &header_height);

      /* find the maximum height by using the monitor height minus an allowance
       * for title bar, etc. */
      maximum_height = get_monitor_height (self) - 100;

      if (maximum_height > 0 && nat_height + header_height > maximum_height)
        nat_height = maximum_height - header_height;

      gtk_scrolled_window_set_min_content_height (GTK_SCROLLED_WINDOW (self->scrolled_window),
                                                  self->small_screen == SMALL_SCREEN_TRUE ? SMALL_SCREEN_FIXED_HEIGHT : nat_height);
    }
  else
    {
      gtk_widget_show (self->previous_button);
      gtk_widget_hide (self->search_button);
      gtk_widget_hide (self->search_bar);
      /* set the scrolled window small so that it doesn't force
         the window to be larger than this panel */
      gtk_widget_get_preferred_height_for_width (GTK_WIDGET (self),
                                                 FIXED_WIDTH, NULL, &nat_height);
      gtk_scrolled_window_set_min_content_height (GTK_SCROLLED_WINDOW (self->scrolled_window), MIN_ICON_VIEW_HEIGHT);
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
  CcWindow *self = CC_WINDOW (shell);

  /* add to header */
  gtk_box_pack_end (GTK_BOX (self->top_right_box), widget, FALSE, FALSE, 0);
  g_ptr_array_add (self->custom_widgets, g_object_ref (widget));

  gtk_size_group_add_widget (self->header_sizegroup, widget);
}

/* CcShell implementation */
static gboolean
cc_window_set_active_panel_from_id (CcShell      *shell,
                                    const gchar  *start_id,
                                    GVariant     *parameters,
                                    GError      **err)
{
  GtkTreeIter iter;
  gboolean iter_valid;
  gchar *name = NULL;
  GIcon *gicon = NULL;
  CcWindow *self = CC_WINDOW (shell);
  GtkWidget *old_panel;

  /* When loading the same panel again, just set its parameters */
  if (g_strcmp0 (self->current_panel_id, start_id) == 0)
    {
      g_object_set (G_OBJECT (self->current_panel), "parameters", parameters, NULL);
      return TRUE;
    }

  /* clear any custom widgets */
  _shell_remove_all_custom_widgets (self);

  iter_valid = gtk_tree_model_get_iter_first (GTK_TREE_MODEL (self->store),
                                              &iter);

  /* find the details for this item */
  while (iter_valid)
    {
      gchar *id;

      gtk_tree_model_get (GTK_TREE_MODEL (self->store), &iter,
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

      iter_valid = gtk_tree_model_iter_next (GTK_TREE_MODEL (self->store),
                                             &iter);
    }

  old_panel = self->current_panel_box;

  if (!name)
    {
      g_warning ("Could not find settings panel \"%s\"", start_id);
    }
  else if (activate_panel (CC_WINDOW (shell), start_id, parameters,
                           name, gicon) == FALSE)
    {
      /* Failed to activate the panel for some reason,
       * let's keep the old panel around instead */
    }
  else
    {
      /* Successful activation */
      g_free (self->current_panel_id);
      self->current_panel_id = g_strdup (start_id);

      if (old_panel)
        gtk_container_remove (GTK_CONTAINER (self->stack), old_panel);
    }

  g_free (name);
  if (gicon)
    g_object_unref (gicon);

  return TRUE;
}

static gboolean
_shell_set_active_panel_from_id (CcShell      *shell,
                                 const gchar  *start_id,
                                 GVariant     *parameters,
                                 GError      **err)
{
  add_current_panel_to_history (shell, start_id);
  return cc_window_set_active_panel_from_id (shell, start_id, parameters, err);
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
  CcWindow *self = CC_WINDOW (object);

  switch (property_id)
    {
    case PROP_ACTIVE_PANEL:
      g_value_set_object (value, self->active_panel);
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

  if (panel != shell->active_panel)
    {
      /* remove the old panel */
      g_clear_object (&shell->active_panel);

      /* set the new panel */
      if (panel)
        {
          shell->active_panel = g_object_ref (panel);
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
  CcWindow *self = CC_WINDOW (object);

  /* Avoid receiving notifications about the pages changing
   * when destroying the children one-by-one */
  if (self->stack)
    {
      g_signal_handlers_disconnect_by_func (self->stack, stack_page_notify_cb, object);
      self->stack = NULL;
    }

  g_free (self->current_panel_id);
  self->current_panel_id = NULL;

  if (self->custom_widgets)
    {
      g_ptr_array_unref (self->custom_widgets);
      self->custom_widgets = NULL;
    }

  g_clear_object (&self->store);
  g_clear_object (&self->search_filter);
  g_clear_object (&self->active_panel);
  g_clear_object (&self->header_sizegroup);

  if (self->previous_panels)
    {
      g_queue_free_full (self->previous_panels, g_free);
      self->previous_panels = NULL;
    }

  G_OBJECT_CLASS (cc_window_parent_class)->dispose (object);
}

static void
cc_window_finalize (GObject *object)
{
  CcWindow *self = CC_WINDOW (object);

  g_free (self->filter_string);
  g_strfreev (self->filter_terms);

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

  object_class->get_property = cc_window_get_property;
  object_class->set_property = cc_window_set_property;
  object_class->dispose = cc_window_dispose;
  object_class->finalize = cc_window_finalize;

  g_object_class_override_property (object_class, PROP_ACTIVE_PANEL, "active-panel");
}

static gboolean
window_button_release_event (GtkWidget          *win,
			     GdkEventButton     *event,
			     CcWindow           *self)
{
  /* back button */
  if (event->button == MOUSE_BACK_BUTTON)
    shell_show_overview_page (self);
  return FALSE;
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
  gboolean overview;
  gboolean search;
  const gchar *id;

  retval = GDK_EVENT_PROPAGATE;
  state = event->state;
  keymap = gdk_keymap_get_default ();
  gdk_keymap_add_virtual_modifiers (keymap, &state);
  state = state & gtk_accelerator_get_default_mod_mask ();
  is_rtl = gtk_widget_get_direction (win) == GTK_TEXT_DIR_RTL;

  id = gtk_stack_get_visible_child_name (GTK_STACK (self->stack));
  overview = g_str_equal (id, OVERVIEW_PAGE);
  search = g_str_equal (id, SEARCH_PAGE);

  if ((overview || search) &&
      gtk_search_bar_handle_event (GTK_SEARCH_BAR (self->search_bar), (GdkEvent*) event) == GDK_EVENT_STOP)
    return GDK_EVENT_STOP;

  if (state == GDK_CONTROL_MASK)
    {
      switch (event->keyval)
        {
          case GDK_KEY_s:
          case GDK_KEY_S:
          case GDK_KEY_f:
          case GDK_KEY_F:
            if (!overview && !search)
              break;
            retval = !gtk_search_bar_get_search_mode (GTK_SEARCH_BAR (self->search_bar));
            gtk_search_bar_set_search_mode (GTK_SEARCH_BAR (self->search_bar), retval);
            if (retval)
              gtk_widget_grab_focus (self->search_entry);
            retval = GDK_EVENT_STOP;
            break;
          case GDK_KEY_Q:
          case GDK_KEY_q:
            gtk_widget_destroy (GTK_WIDGET (self));
            retval = GDK_EVENT_STOP;
            break;
          case GDK_KEY_W:
          case GDK_KEY_w:
            if (!overview)
              shell_show_overview_page (self);
            else
              gtk_widget_destroy (GTK_WIDGET (self));
            retval = GDK_EVENT_STOP;
            break;
        }
    }
  else if (state == GDK_MOD1_MASK && event->keyval == GDK_KEY_Up)
    {
      if (!overview)
        shell_show_overview_page (self);
      retval = GDK_EVENT_STOP;
    }
  else if ((!is_rtl && state == GDK_MOD1_MASK && event->keyval == GDK_KEY_Left) ||
           (is_rtl && state == GDK_MOD1_MASK && event->keyval == GDK_KEY_Right) ||
           event->keyval == GDK_KEY_Back)
    {
      previous_button_clicked_cb (NULL, self);
      retval = GDK_EVENT_STOP;
    }
  return retval;
}

static gint
get_monitor_height (CcWindow *self)
{
  GdkScreen *screen;
  GdkRectangle rect;

  if (self->monitor_num < 0)
    return 0;

  /* We cannot use workarea here, as this wouldn't
   * be updated when we read it after a monitors-changed signal */
  screen = gtk_widget_get_screen (GTK_WIDGET (self));
  gdk_screen_get_monitor_geometry (screen, self->monitor_num, &rect);

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
  if (self->monitor_num != monitor)
    {
      self->monitor_num = monitor;
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

      if (self->small_screen != small)
        gtk_window_maximize (GTK_WINDOW (self));
    }
  else
    {
      if (self->small_screen != small)
        gtk_window_unmaximize (GTK_WINDOW (self));

      gtk_window_set_resizable (GTK_WINDOW (self), FALSE);
    }

  self->small_screen = small;

  /* And update the minimum sizes */
  stack_page_notify_cb (GTK_STACK (self->stack), NULL, self);
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
  self->small_screen = SMALL_SCREEN_UNSET;
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

static gboolean
window_map_event_cb (GtkWidget *widget,
                     GdkEvent  *event,
                     CcWindow  *self)
{
  /* If focus ends up in a category icon view one of the items is
   * immediately selected which looks odd when we are starting up, so
   * we explicitly unset the focus here. */
  gtk_window_set_focus (GTK_WINDOW (self), NULL);
  return GDK_EVENT_PROPAGATE;
}

static void
create_main_page (CcWindow *self)
{
  GtkStyleContext *context;

  self->scrolled_window = gtk_scrolled_window_new (NULL, NULL);
  context = gtk_widget_get_style_context (self->scrolled_window);
  gtk_style_context_add_class (context, "view");
  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (self->scrolled_window),
                                  GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
  gtk_stack_add_named (GTK_STACK (self->stack), self->scrolled_window, OVERVIEW_PAGE);

  self->main_vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
  gtk_widget_set_margin_top (self->main_vbox, 8);
  gtk_widget_set_margin_bottom (self->main_vbox, 8);
  gtk_widget_set_margin_start (self->main_vbox, 12);
  gtk_widget_set_margin_end (self->main_vbox, 12);
  gtk_container_set_focus_vadjustment (GTK_CONTAINER (self->main_vbox),
                                       gtk_scrolled_window_get_vadjustment (GTK_SCROLLED_WINDOW (self->scrolled_window)));
  gtk_container_add (GTK_CONTAINER (self->scrolled_window), self->main_vbox);

  gtk_widget_set_size_request (self->scrolled_window, FIXED_WIDTH, -1);

  /* load the available settings panels */
  setup_model (self);
}

static void
create_search_page (CcWindow *self)
{
  self->search_scrolled = gtk_scrolled_window_new (NULL, NULL);
  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (self->search_scrolled),
                                  GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
  gtk_stack_add_named (GTK_STACK (self->stack), self->search_scrolled, SEARCH_PAGE);

  /* setup search functionality */
  setup_search (self);
}

static void
create_header (CcWindow *self)
{
  GtkWidget *image;
  AtkObject *accessible;

  self->header = gtk_header_bar_new ();
  gtk_header_bar_set_show_close_button (GTK_HEADER_BAR (self->header), TRUE);

  self->header_sizegroup = gtk_size_group_new (GTK_SIZE_GROUP_VERTICAL);

  /* previous button */
  self->previous_button = gtk_button_new_from_icon_name ("go-previous-symbolic", GTK_ICON_SIZE_MENU);
  gtk_widget_set_valign (self->previous_button, GTK_ALIGN_CENTER);
  gtk_widget_set_no_show_all (self->previous_button, TRUE);
  accessible = gtk_widget_get_accessible (self->previous_button);
  atk_object_set_name (accessible, _("All Settings"));
  gtk_header_bar_pack_start (GTK_HEADER_BAR (self->header), self->previous_button);
  g_signal_connect (self->previous_button, "clicked", G_CALLBACK (previous_button_clicked_cb), self);
  gtk_size_group_add_widget (self->header_sizegroup, self->previous_button);

  /* toggle search button */
  self->search_button = gtk_toggle_button_new ();
  image = gtk_image_new_from_icon_name ("edit-find-symbolic", GTK_ICON_SIZE_MENU);
  gtk_button_set_image (GTK_BUTTON (self->search_button), image);
  gtk_widget_set_valign (self->search_button, GTK_ALIGN_CENTER);
  gtk_style_context_add_class (gtk_widget_get_style_context (self->search_button),
                               "image-button");
  gtk_header_bar_pack_end (GTK_HEADER_BAR (self->header), self->search_button);

  self->top_right_box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
  gtk_header_bar_pack_end (GTK_HEADER_BAR (self->header), self->top_right_box);

  self->lock_button = gtk_lock_button_new (NULL);
  gtk_style_context_add_class (gtk_widget_get_style_context (self->lock_button),
                               "text-button");
  gtk_widget_set_valign (self->lock_button, GTK_ALIGN_CENTER);
  gtk_widget_set_no_show_all (self->lock_button, TRUE);
  gtk_container_add (GTK_CONTAINER (self->top_right_box), self->lock_button);
  gtk_size_group_add_widget (self->header_sizegroup, self->lock_button);
}

static void
create_window (CcWindow *self)
{
  GtkWidget *box;
  GdkScreen *screen;

  box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
  gtk_container_add (GTK_CONTAINER (self), box);

  create_header (self);
  gtk_window_set_titlebar (GTK_WINDOW (self), self->header);
  gtk_header_bar_set_title (GTK_HEADER_BAR (self->header), _(DEFAULT_WINDOW_TITLE));
  gtk_widget_show_all (self->header);

  /* search bar */
  self->search_bar = gtk_search_bar_new ();
  self->search_entry = gtk_search_entry_new ();
  gtk_entry_set_width_chars (GTK_ENTRY (self->search_entry), 30);
  g_signal_connect (self->search_entry, "search-changed", G_CALLBACK (search_entry_changed_cb), self);
  g_signal_connect (self->search_entry, "key-press-event", G_CALLBACK (search_entry_key_press_event_cb), self);
  gtk_container_add (GTK_CONTAINER (self->search_bar), self->search_entry);
  gtk_container_add (GTK_CONTAINER (box), self->search_bar);

  g_object_bind_property (self->search_button, "active",
                          self->search_bar, "search-mode-enabled",
                          G_BINDING_BIDIRECTIONAL);

  self->stack = gtk_stack_new ();
  gtk_stack_set_homogeneous (GTK_STACK (self->stack), TRUE);
  gtk_stack_set_transition_type (GTK_STACK (self->stack), GTK_STACK_TRANSITION_TYPE_CROSSFADE);
  gtk_box_pack_start (GTK_BOX (box), self->stack, TRUE, TRUE, 0);

  create_main_page (self);
  create_search_page (self);

  /* connect various signals */
  screen = gtk_widget_get_screen (GTK_WIDGET (self));
  g_signal_connect (screen, "monitors-changed", G_CALLBACK (monitors_changed_cb), self);

  g_signal_connect (self, "configure-event", G_CALLBACK (main_window_configure_cb), self);
  g_signal_connect (self, "notify::application", G_CALLBACK (application_set_cb), self);
  g_signal_connect_after (self, "key_press_event",
                          G_CALLBACK (window_key_press_event), self);
  gtk_widget_add_events (GTK_WIDGET (self), GDK_BUTTON_RELEASE_MASK);
  g_signal_connect (self, "button-release-event",
                    G_CALLBACK (window_button_release_event), self);
  g_signal_connect (self, "map-event", G_CALLBACK (window_map_event_cb), self);

  g_signal_connect (self, "notify::window", G_CALLBACK (gdk_window_set_cb), self);

  g_signal_connect (self->stack, "notify::visible-child",
                    G_CALLBACK (stack_page_notify_cb), self);

  gtk_widget_show_all (box);
}

static void
cc_window_init (CcWindow *self)
{
  self->monitor_num = -1;
  self->small_screen = SMALL_SCREEN_UNSET;

  create_window (self);

  self->previous_panels = g_queue_new ();

  /* keep a list of custom widgets to unload on panel change */
  self->custom_widgets = g_ptr_array_new_with_free_func ((GDestroyNotify) g_object_unref);

  stack_page_notify_cb (GTK_STACK (self->stack), NULL, self);
}

CcWindow *
cc_window_new (GtkApplication *application)
{
  g_return_val_if_fail (GTK_IS_APPLICATION (application), NULL);

  return g_object_new (CC_TYPE_WINDOW,
                       "application", application,
                       "resizable", TRUE,
                       "title", _("Settings"),
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
