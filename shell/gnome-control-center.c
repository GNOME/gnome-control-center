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
#define GMENU_I_KNOW_THIS_IS_UNSTABLE
#include <gmenu-tree.h>

#include "cc-panel.h"
#include "cc-shell.h"
#include "shell-search-renderer.h"
#include "cc-shell-category-view.h"
#include "cc-shell-model.h"

G_DEFINE_TYPE (GnomeControlCenter, gnome_control_center, CC_TYPE_SHELL)

#define CONTROL_CENTER_PRIVATE(o) \
  (G_TYPE_INSTANCE_GET_PRIVATE ((o), GNOME_TYPE_CONTROL_CENTER, GnomeControlCenterPrivate))

#define W(b,x) GTK_WIDGET (gtk_builder_get_object (b, x))

enum
{
  OVERVIEW_PAGE,
  SEARCH_PAGE,
  CAPPLET_PAGE
};


struct _GnomeControlCenterPrivate
{
  GtkBuilder *builder;
  GtkWidget  *notebook;
  GtkWidget  *window;
  GtkWidget  *search_entry;

  GtkListStore *store;

  GtkTreeModel *search_filter;
  GtkWidget *search_view;
  GtkCellRenderer *search_renderer;
  gchar *filter_string;

  guint32 last_time;
};


static void
activate_panel (GnomeControlCenter *shell,
                const gchar *id,
                const gchar *desktop_file)
{
  GnomeControlCenterPrivate *priv = shell->priv;
  GAppInfo *appinfo;
  GError *err = NULL;
  GdkAppLaunchContext *ctx;
  GKeyFile *key_file;

  /* start app directly */

  if (!desktop_file)
    return;

  key_file = g_key_file_new ();
  g_key_file_load_from_file (key_file, desktop_file, 0, &err);

  if (err)
    {
      g_warning ("Error starting \"%s\": %s", id, err->message);

      g_error_free (err);
      err = NULL;
      return;
    }

  appinfo = (GAppInfo*) g_desktop_app_info_new_from_keyfile (key_file);

  g_key_file_free (key_file);


  ctx = gdk_app_launch_context_new ();
  gdk_app_launch_context_set_screen (ctx, gdk_screen_get_default ());
  gdk_app_launch_context_set_timestamp (ctx, priv->last_time);

  g_app_info_launch (appinfo, NULL, G_APP_LAUNCH_CONTEXT (ctx), &err);

  g_object_unref (appinfo);
  g_object_unref (ctx);

  if (err)
    {
      g_warning ("Error starting \"%s\": %s", id, err->message);
      g_error_free (err);
      err = NULL;
    }
}

static void
shell_show_overview_page (GnomeControlCenterPrivate *priv)
{
  gtk_notebook_set_current_page (GTK_NOTEBOOK (priv->notebook), OVERVIEW_PAGE);

  gtk_notebook_remove_page (GTK_NOTEBOOK (priv->notebook), CAPPLET_PAGE);

  gtk_label_set_text (GTK_LABEL (gtk_builder_get_object (priv->builder,
                                                         "label-title")), "");
  gtk_widget_hide (GTK_WIDGET (gtk_builder_get_object (priv->builder,
                                                       "title-alignment")));

  /* clear the search text */
  g_free (priv->filter_string);
  priv->filter_string = g_strdup ("");
  gtk_entry_set_text (GTK_ENTRY (priv->search_entry), "");
}



static void
item_activated_cb (CcShellCategoryView *view,
                   gchar               *name,
                   gchar               *id,
                   gchar               *desktop_file,
                   GnomeControlCenter  *shell)
{
  GError *err = NULL;

  if (!cc_shell_set_active_panel_from_id (CC_SHELL (shell), id, &err))
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
model_filter_func (GtkTreeModel              *model,
                   GtkTreeIter               *iter,
                   GnomeControlCenterPrivate *priv)
{
  gchar *name, *target;
  gchar *needle, *haystack;
  gboolean result;

  gtk_tree_model_get (model, iter, COL_NAME, &name,
                      COL_SEARCH_TARGET, &target, -1);

  if (!priv->filter_string || !name || !target)
    {
      g_free (name);
      g_free (target);
      return FALSE;
    }

  needle = g_utf8_casefold (priv->filter_string, -1);
  haystack = g_utf8_casefold (target, -1);

  result = (strstr (haystack, needle) != NULL);

  g_free (name);
  g_free (target);
  g_free (haystack);
  g_free (needle);

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
search_entry_changed_cb (GtkEntry                  *entry,
                         GnomeControlCenterPrivate *priv)
{

  /* if the entry text was set manually (not by the user) */
  if (!g_strcmp0 (priv->filter_string, gtk_entry_get_text (entry)))
    return;

  g_free (priv->filter_string);
  priv->filter_string = g_strdup (gtk_entry_get_text (entry));

  g_object_set (priv->search_renderer,
                "search-string", priv->filter_string,
                NULL);

  if (!g_strcmp0 (priv->filter_string, ""))
    {
      shell_show_overview_page (priv);
    }
  else
    {
      gtk_tree_model_filter_refilter (GTK_TREE_MODEL_FILTER (priv->search_filter));
      gtk_notebook_set_current_page (GTK_NOTEBOOK (priv->notebook), SEARCH_PAGE);

      gtk_label_set_text (GTK_LABEL (gtk_builder_get_object (priv->builder,
                                                             "label-title")),
                          "");
      gtk_widget_hide (GTK_WIDGET (gtk_builder_get_object (priv->builder,
                                                           "title-alignment")));
    }
}

static gboolean
search_entry_key_press_event_cb (GtkEntry    *entry,
                                 GdkEventKey *event,
                                 GnomeControlCenterPrivate   *priv)
{
  if (event->keyval == GDK_Return)
    {
      GtkTreePath *path;

      path = gtk_tree_path_new_first ();

      priv->last_time = event->time;

      gtk_icon_view_item_activated (GTK_ICON_VIEW (priv->search_view), path);

      gtk_tree_path_free (path);
      return TRUE;
    }

  if (event->keyval == GDK_Escape)
    {
      gtk_entry_set_text (entry, "");
      return TRUE;
    }

  return FALSE;
}

static void
search_entry_clear_cb (GtkEntry *entry)
{
  gtk_entry_set_text (entry, "");
}


static void
setup_search (GnomeControlCenter *shell)
{
  GtkWidget *search_scrolled, *search_view, *widget;
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
  priv->search_view = search_view = cc_shell_item_view_new ();
  gtk_icon_view_set_orientation (GTK_ICON_VIEW (search_view),
                                 GTK_ORIENTATION_HORIZONTAL);
  gtk_icon_view_set_spacing (GTK_ICON_VIEW (search_view), 6);
  gtk_icon_view_set_model (GTK_ICON_VIEW (search_view),
                           GTK_TREE_MODEL (priv->search_filter));
  gtk_icon_view_set_pixbuf_column (GTK_ICON_VIEW (search_view), COL_PIXBUF);

  search_scrolled = W (priv->builder, "search-scrolled-window");
  gtk_container_add (GTK_CONTAINER (search_scrolled), search_view);


  /* add the custom renderer */
  priv->search_renderer = (GtkCellRenderer*) shell_search_renderer_new ();
  gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (search_view),
                              priv->search_renderer, TRUE);
  gtk_cell_layout_add_attribute (GTK_CELL_LAYOUT (search_view),
                                 priv->search_renderer,
                                 "title", COL_NAME);
  gtk_cell_layout_add_attribute (GTK_CELL_LAYOUT (search_view),
                                 priv->search_renderer,
                                 "search-target", COL_SEARCH_TARGET);

  /* connect the activated signal */
  g_signal_connect (search_view, "desktop-item-activated",
                    G_CALLBACK (item_activated_cb), shell);

  /* setup the search entry widget */
  widget = (GtkWidget*) gtk_builder_get_object (priv->builder, "search-entry");
  priv->search_entry = widget;

  g_signal_connect (widget, "changed", G_CALLBACK (search_entry_changed_cb),
                    priv);
  g_signal_connect (widget, "key-press-event",
                    G_CALLBACK (search_entry_key_press_event_cb), priv);

  g_signal_connect (widget, "icon-release", G_CALLBACK (search_entry_clear_cb),
                    priv);
}

static void
fill_model (GnomeControlCenter *shell)
{
  GSList *list, *l;
  GMenuTreeDirectory *d;
  GMenuTree *tree;
  GtkWidget *vbox;

  GnomeControlCenterPrivate *priv = shell->priv;

  vbox = W (priv->builder, "main-vbox");

  tree = gmenu_tree_lookup (MENUDIR "/gnomecc.menu", 0);

  if (!tree)
    {
      g_warning ("Could not find control center menu");
      return;
    }

  d = gmenu_tree_get_root_directory (tree);

  list = gmenu_tree_directory_get_contents (d);

  priv->store = (GtkListStore *) cc_shell_model_new ();

  for (l = list; l; l = l->next)
    {
      GMenuTreeItemType type;
      type = gmenu_tree_item_get_type (l->data);

      if (type == GMENU_TREE_ITEM_DIRECTORY)
        {
          GtkTreeModel *filter;
          GtkWidget *categoryview;
          GSList *contents, *f;
          const gchar *dir_name;

          contents = gmenu_tree_directory_get_contents (l->data);
          dir_name = gmenu_tree_directory_get_name (l->data);

          /* create new category view for this category */
          filter = gtk_tree_model_filter_new (GTK_TREE_MODEL (priv->store),
                                              NULL);
          gtk_tree_model_filter_set_visible_func (GTK_TREE_MODEL_FILTER (filter),
                                                  (GtkTreeModelFilterVisibleFunc) category_filter_func,
                                                  g_strdup (dir_name), g_free);

          categoryview = cc_shell_category_view_new (dir_name, filter);
          gtk_box_pack_start (GTK_BOX (vbox), categoryview, FALSE, TRUE, 6);
          g_signal_connect (cc_shell_category_view_get_item_view (CC_SHELL_CATEGORY_VIEW (categoryview)),
                            "desktop-item-activated",
                            G_CALLBACK (item_activated_cb), shell);
          gtk_widget_show (categoryview);

          /* add the items from this category to the model */
          for (f = contents; f; f = f->next)
            {
              if (gmenu_tree_item_get_type (f->data) == GMENU_TREE_ITEM_ENTRY)
                {
                  cc_shell_model_add_item (CC_SHELL_MODEL (priv->store),
                                           dir_name,
                                           f->data);
                }
            }
        }
    }

  g_slist_free (list);

}


static void
home_button_clicked_cb (GtkButton *button,
                        GnomeControlCenter *shell)
{
  shell_show_overview_page (shell->priv);
}

static void
notebook_switch_page_cb (GtkNotebook               *book,
                         GtkNotebookPage           *page,
                         gint                       page_num,
                         GnomeControlCenterPrivate *priv)
{
  /* make sure the home button is shown on all pages except the overview page */

  if (page_num == OVERVIEW_PAGE)
    gtk_widget_hide (W (priv->builder, "home-button"));
  else
    gtk_widget_show (W (priv->builder, "home-button"));

  if (page_num == CAPPLET_PAGE)
    gtk_widget_hide (W (priv->builder, "search-entry"));
  else
    gtk_widget_show (W (priv->builder, "search-entry"));
}

/* CcShell implementation */
static gboolean
_shell_set_active_panel_from_id (CcShell      *shell,
                                 const gchar  *start_id,
                                 GError      **err)
{
  GtkTreeIter iter;
  gboolean iter_valid;
  gchar *name = NULL;
  gchar *desktop;
  GnomeControlCenterPrivate *priv = GNOME_CONTROL_CENTER (shell)->priv;


  iter_valid = gtk_tree_model_get_iter_first (GTK_TREE_MODEL (priv->store),
                                              &iter);

  /* find the details for this item */
  while (iter_valid)
    {
      gchar *id;

      gtk_tree_model_get (GTK_TREE_MODEL (priv->store), &iter,
                          COL_NAME, &name,
                          COL_ID, &id,
                          COL_DESKTOP_FILE, &desktop,
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

          name = NULL;
          id = NULL;
        }

      iter_valid = gtk_tree_model_iter_next (GTK_TREE_MODEL (priv->store),
                                             &iter);
    }

  if (!name)
    {
      g_warning ("Could not find settings panel \"%s\"", start_id);
      return FALSE;
    }
  else
    {
      activate_panel (GNOME_CONTROL_CENTER (shell), start_id, desktop);

      g_free (name);
      g_free (desktop);

      return TRUE;
    }
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

  if (priv->window)
    {
      gtk_widget_destroy (priv->window);
      priv->window = NULL;

      /* destroying the window will destroy its children */
      priv->notebook = NULL;
      priv->search_entry = NULL;
      priv->search_view = NULL;
      priv->search_renderer = NULL;
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
}

static void
gnome_control_center_init (GnomeControlCenter *self)
{
  GError *err = NULL;
  GtkWidget *vbox;
  GnomeControlCenterPrivate *priv;

  priv = self->priv = CONTROL_CENTER_PRIVATE (self);

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
  g_signal_connect (priv->window, "delete-event", G_CALLBACK (gtk_main_quit),
                    NULL);

  priv->notebook = W (priv->builder, "notebook");

  g_signal_connect (priv->notebook, "switch-page",
                    G_CALLBACK (notebook_switch_page_cb), priv);

  g_signal_connect (gtk_builder_get_object (priv->builder, "home-button"),
                    "clicked", G_CALLBACK (home_button_clicked_cb), self);


  /* setup background colours */
  vbox = W (priv->builder, "main-vbox");
  gtk_widget_set_size_request (vbox, 0, -1);

  gtk_widget_modify_bg (vbox->parent, GTK_STATE_NORMAL,
                        &vbox->style->base[GTK_STATE_NORMAL]);
  gtk_widget_modify_fg (vbox->parent, GTK_STATE_NORMAL,
                        &vbox->style->text[GTK_STATE_NORMAL]);

  /* load the available settings panels */
  fill_model (self);

  /* setup search functionality */
  setup_search (self);

  gtk_widget_show_all (priv->window);
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
