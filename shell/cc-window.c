/*
 * Copyright (c) 2009, 2010 Intel, Inc.
 * Copyright (c) 2010 Red Hat, Inc.
 * Copyright (c) 2016 Endless, Inc.
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
#include "cc-panel-list.h"
#include "cc-panel-loader.h"
#include "cc-util.h"

#define MOUSE_BACK_BUTTON 8

#define DEFAULT_WINDOW_ICON_NAME "gnome-control-center"

struct _CcWindow
{
  GtkApplicationWindow parent;

  GtkWidget  *stack;
  GtkWidget  *header;
  GtkWidget  *header_box;
  GtkWidget  *list_scrolled;
  GtkWidget  *panel_headerbar;
  GtkWidget  *search_scrolled;
  GtkWidget  *panel_list;
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

  CcPanel *active_panel;
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
  gtk_header_bar_set_title (GTK_HEADER_BAR (self->panel_headerbar), name);
  gtk_window_set_default_icon_name (icon_name);
  gtk_window_set_icon_name (GTK_WINDOW (self), icon_name);

  title_widget = cc_panel_get_title_widget (CC_PANEL (self->current_panel));
  gtk_header_bar_set_custom_title (GTK_HEADER_BAR (self->panel_headerbar), title_widget);

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
  /* TODO: need design input on a possibly new overview page. For now,
   * just go back to the main section of the panel list. */
  cc_panel_list_set_view (CC_PANEL_LIST (self->panel_list), CC_PANEL_LIST_MAIN);
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
show_panel_cb (CcPanelList *panel_list,
               const gchar *panel_id,
               CcWindow    *self)
{
  if (panel_id)
    cc_window_set_active_panel_from_id (CC_SHELL (self), panel_id, NULL, NULL);
  else
    shell_show_overview_page (self);
}

static void
update_list_title (CcWindow *self)
{
  CcPanelListView view;
  const gchar *title;

  view = cc_panel_list_get_view (CC_PANEL_LIST (self->panel_list));

  switch (view)
    {
    case CC_PANEL_LIST_DETAILS:
      title = _("Details");
      break;

    case CC_PANEL_LIST_DEVICES:
      title = _("Devices");
      break;

    case CC_PANEL_LIST_MAIN:
      title = _("Settings");
      break;

    case CC_PANEL_LIST_SEARCH:
      title = NULL;
      break;
    }

  if (title)
    gtk_header_bar_set_title (GTK_HEADER_BAR (self->header), title);
}

static void
panel_list_view_changed_cb (CcPanelList *panel_list,
                            GParamSpec  *pspec,
                            CcWindow    *self)
{
  gboolean is_main_view;

  is_main_view = cc_panel_list_get_view (panel_list) == CC_PANEL_LIST_MAIN;

  gtk_widget_set_visible (self->previous_button, !is_main_view);
  gtk_widget_set_visible (self->search_button, is_main_view);

  update_list_title (self);
}

static void
search_entry_activate_cb (GtkEntry *entry,
                          CcWindow *self)
{
  gboolean changed;

  changed = cc_panel_list_activate (CC_PANEL_LIST (self->panel_list));

  gtk_search_bar_set_search_mode (GTK_SEARCH_BAR (self->search_bar), !changed);
}


static void
setup_model (CcWindow *shell)
{
  GtkTreeModel *model;
  GtkTreeIter iter;
  gboolean valid;

  shell->store = (GtkListStore *) cc_shell_model_new ();
  model = GTK_TREE_MODEL (shell->store);

  cc_panel_loader_fill_model (CC_SHELL_MODEL (shell->store));

  /* Create a row for each panel */
  valid = gtk_tree_model_get_iter_first (model, &iter);

  while (valid)
    {
      CcPanelCategory category;
      GIcon *icon;
      gchar *name, *description, *id, *symbolic_icon;
      const gchar *icon_name;

      gtk_tree_model_get (model, &iter,
                          COL_CATEGORY, &category,
                          COL_DESCRIPTION, &description,
                          COL_GICON, &icon,
                          COL_ID, &id,
                          COL_NAME, &name,
                          -1);

      icon_name = get_icon_name_from_g_icon (icon);
      symbolic_icon = g_strdup_printf ("%s-symbolic", icon_name);

      cc_panel_list_add_panel (CC_PANEL_LIST (shell->panel_list),
                               category,
                               id,
                               name,
                               description,
                               symbolic_icon);

      valid = gtk_tree_model_iter_next (model, &iter);

      g_clear_pointer (&symbolic_icon, g_free);
      g_clear_pointer (&description, g_free);
      g_clear_pointer (&name, g_free);
      g_clear_pointer (&id, g_free);
      g_clear_object (&icon);
    }
}

static void
previous_button_clicked_cb (GtkButton *button,
                            CcWindow  *shell)
{
  g_debug ("Num previous panels? %d", g_queue_get_length (shell->previous_panels));

  /* When in search, simply unsed the search mode */
  if (gtk_search_bar_get_search_mode (GTK_SEARCH_BAR (shell->search_bar)))
    gtk_search_bar_set_search_mode (GTK_SEARCH_BAR (shell->search_bar), FALSE);
  else
    cc_panel_list_set_view (CC_PANEL_LIST (shell->panel_list), CC_PANEL_LIST_MAIN);
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

      cc_panel_list_set_active_panel (CC_PANEL_LIST (self->panel_list), start_id);
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

  g_free (self->current_panel_id);
  self->current_panel_id = NULL;

  if (self->custom_widgets)
    {
      g_ptr_array_unref (self->custom_widgets);
      self->custom_widgets = NULL;
    }

  g_clear_object (&self->store);
  g_clear_object (&self->active_panel);

  G_OBJECT_CLASS (cc_window_parent_class)->dispose (object);
}

static void
cc_window_finalize (GObject *object)
{
  CcWindow *self = CC_WINDOW (object);

  if (self->previous_panels)
    {
      g_queue_free_full (self->previous_panels, g_free);
      self->previous_panels = NULL;
    }

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
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->get_property = cc_window_get_property;
  object_class->set_property = cc_window_set_property;
  object_class->dispose = cc_window_dispose;
  object_class->finalize = cc_window_finalize;

  g_object_class_override_property (object_class, PROP_ACTIVE_PANEL, "active-panel");

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/ControlCenter/gtk/window.ui");

  gtk_widget_class_bind_template_child (widget_class, CcWindow, header);
  gtk_widget_class_bind_template_child (widget_class, CcWindow, header_box);
  gtk_widget_class_bind_template_child (widget_class, CcWindow, header_sizegroup);
  gtk_widget_class_bind_template_child (widget_class, CcWindow, list_scrolled);
  gtk_widget_class_bind_template_child (widget_class, CcWindow, lock_button);
  gtk_widget_class_bind_template_child (widget_class, CcWindow, panel_headerbar);
  gtk_widget_class_bind_template_child (widget_class, CcWindow, previous_button);
  gtk_widget_class_bind_template_child (widget_class, CcWindow, search_bar);
  gtk_widget_class_bind_template_child (widget_class, CcWindow, search_button);
  gtk_widget_class_bind_template_child (widget_class, CcWindow, search_entry);
  gtk_widget_class_bind_template_child (widget_class, CcWindow, stack);
  gtk_widget_class_bind_template_child (widget_class, CcWindow, top_right_box);

  gtk_widget_class_bind_template_callback (widget_class, previous_button_clicked_cb);
  gtk_widget_class_bind_template_callback (widget_class, gdk_window_set_cb);
  gtk_widget_class_bind_template_callback (widget_class, search_entry_activate_cb);
  gtk_widget_class_bind_template_callback (widget_class, update_list_title);
  gtk_widget_class_bind_template_callback (widget_class, window_map_event_cb);
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
  CcPanelListView view;
  gboolean is_rtl;

  retval = GDK_EVENT_PROPAGATE;
  state = event->state;
  keymap = gdk_keymap_get_default ();
  gdk_keymap_add_virtual_modifiers (keymap, &state);
  state = state & gtk_accelerator_get_default_mod_mask ();
  is_rtl = gtk_widget_get_direction (win) == GTK_TEXT_DIR_RTL;
  view = cc_panel_list_get_view (CC_PANEL_LIST (self->panel_list));

  /* The search only happens when we're in the MAIN view */
  if (view == CC_PANEL_LIST_MAIN &&
      gtk_search_bar_handle_event (GTK_SEARCH_BAR (self->search_bar), (GdkEvent*) event) == GDK_EVENT_STOP)
    {
      return GDK_EVENT_STOP;
    }

  if (state == GDK_CONTROL_MASK)
    {
      switch (event->keyval)
        {
          case GDK_KEY_s:
          case GDK_KEY_S:
          case GDK_KEY_f:
          case GDK_KEY_F:
            /* The search only happens when we're in the MAIN view */
            if (view != CC_PANEL_LIST_MAIN &&
                view != CC_PANEL_LIST_SEARCH)
              {
                break;
              }

            retval = !gtk_search_bar_get_search_mode (GTK_SEARCH_BAR (self->search_bar));
            gtk_search_bar_set_search_mode (GTK_SEARCH_BAR (self->search_bar), retval);
            if (retval)
              gtk_widget_grab_focus (self->search_entry);
            retval = GDK_EVENT_STOP;
            break;
          case GDK_KEY_Q:
          case GDK_KEY_q:
          case GDK_KEY_W:
          case GDK_KEY_w:
            gtk_widget_destroy (GTK_WIDGET (self));
            retval = GDK_EVENT_STOP;
            break;
        }
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

static void
split_decorations (GtkSettings *settings,
                   GParamSpec  *pspec,
                   CcWindow    *self)
{
  g_autofree gchar *layout = NULL;
  g_autofree gchar *layout_start = NULL;
  g_autofree gchar *layout_end = NULL;
  g_autofree gchar **buttons;

  g_object_get (settings, "gtk-decoration-layout", &layout, NULL);

  buttons = g_strsplit (layout, ":", -1);
  layout_start = g_strconcat ("", buttons[0], ":", NULL);

  if (g_strv_length (buttons) > 1)
      layout_end = g_strconcat (":", buttons[1], NULL);
  else
      layout_end = g_strdup ("");

  gtk_header_bar_set_decoration_layout (GTK_HEADER_BAR (self->header), layout_start);
  gtk_header_bar_set_decoration_layout (GTK_HEADER_BAR (self->panel_headerbar), layout_end);
}

static void
create_window (CcWindow *self)
{
  GtkSettings *settings;
  AtkObject *accessible;

  /* previous button */
  accessible = gtk_widget_get_accessible (self->previous_button);
  atk_object_set_name (accessible, _("All Settings"));

  gtk_window_set_titlebar (GTK_WINDOW (self), self->header_box);
  gtk_widget_show_all (self->header_box);

  /*
   * We have to create the listbox here because declaring it in window.ui
   * and letting GtkBuilder handle it would hit the bug where the focus is
   * not tracked.
   */
  self->panel_list = cc_panel_list_new ();

  g_signal_connect (self->panel_list, "show-panel", G_CALLBACK (show_panel_cb), self);
  g_signal_connect (self->panel_list, "notify::view", G_CALLBACK (panel_list_view_changed_cb), self);

  g_object_bind_property (self->search_bar,
                          "search-mode-enabled",
                          self->panel_list,
                          "search-mode",
                          G_BINDING_BIDIRECTIONAL);

  g_object_bind_property (self->search_entry,
                          "text",
                          self->panel_list,
                          "search-query",
                          G_BINDING_DEFAULT);

  gtk_container_add (GTK_CONTAINER (self->list_scrolled), self->panel_list);
  gtk_widget_show (self->panel_list);

  setup_model (self);

  /* connect various signals */
  g_signal_connect_after (self, "key_press_event",
                          G_CALLBACK (window_key_press_event), self);
  gtk_widget_add_events (GTK_WIDGET (self), GDK_BUTTON_RELEASE_MASK);
  g_signal_connect (self, "button-release-event",
                    G_CALLBACK (window_button_release_event), self);

  /* handle decorations for the split headers. */
  settings = gtk_settings_get_default ();
  g_signal_connect (settings,
                    "notify::gtk-decoration-layout",
                    G_CALLBACK (split_decorations),
                    self);

  split_decorations (settings, NULL, self);
}

static void
cc_window_init (CcWindow *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));

  create_window (self);

  self->previous_panels = g_queue_new ();

  /* keep a list of custom widgets to unload on panel change */
  self->custom_widgets = g_ptr_array_new_with_free_func ((GDestroyNotify) g_object_unref);

  /* After everything is loaded, select the first visible panel */
  cc_panel_list_activate (CC_PANEL_LIST (self->panel_list));
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
                       "show-menubar", FALSE,
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
