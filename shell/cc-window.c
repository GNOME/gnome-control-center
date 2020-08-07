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

#define G_LOG_DOMAIN "cc-window"

#include <config.h>

#include "cc-debug.h"
#include "cc-window.h"

#include <glib/gi18n.h>
#include <gio/gio.h>
#include <gio/gdesktopappinfo.h>
#include <gtk/gtk.h>
#include <gdk/gdkkeysyms.h>
#include <gdk/gdkx.h>
#include <handy.h>
#include <string.h>
#include <time.h>

#include "cc-application.h"
#include "cc-panel.h"
#include "cc-shell.h"
#include "cc-shell-model.h"
#include "cc-panel-list.h"
#include "cc-panel-loader.h"
#include "cc-util.h"

#define MOUSE_BACK_BUTTON 8

#define DEFAULT_WINDOW_ICON_NAME "gnome-control-center"

struct _CcWindow
{
  GtkApplicationWindow parent;

  GtkRevealer       *back_revealer;
  GtkMessageDialog  *development_warning_dialog;
  GtkHeaderBar      *header;
  HdyLeaflet        *header_box;
  GtkSizeGroup      *header_sizegroup;
  HdyLeaflet        *main_leaflet;
  GtkHeaderBar      *panel_headerbar;
  CcPanelList       *panel_list;
  GtkButton         *previous_button;
  GtkSearchBar      *search_bar;
  GtkToggleButton   *search_button;
  GtkSearchEntry    *search_entry;
  GtkBox            *sidebar_box;
  GtkStack          *stack;
  GtkBox            *top_left_box;
  GtkBox            *top_right_box;

  GtkWidget  *current_panel;
  char       *current_panel_id;
  GQueue     *previous_panels;

  GPtrArray  *custom_widgets;

  CcShellModel *store;

  CcPanel *active_panel;
  GSettings *settings;

  CcPanelListView previous_list_view;
};

static void     cc_shell_iface_init         (CcShellInterface      *iface);

G_DEFINE_TYPE_WITH_CODE (CcWindow, cc_window, GTK_TYPE_APPLICATION_WINDOW,
                         G_IMPLEMENT_INTERFACE (CC_TYPE_SHELL, cc_shell_iface_init))

enum
{
  PROP_0,
  PROP_ACTIVE_PANEL,
  PROP_MODEL
};

/* Auxiliary methods */
static gboolean
in_flatpak_sandbox (void)
{
  return g_strcmp0 (PROFILE, "development") == 0;
}

static void
remove_all_custom_widgets (CcWindow *self)
{
  GtkWidget *parent;
  GtkWidget *widget;
  guint i;

  CC_ENTRY;

  /* remove from the header */
  for (i = 0; i < self->custom_widgets->len; i++)
    {
      widget = g_ptr_array_index (self->custom_widgets, i);
      parent = gtk_widget_get_parent (widget);

      g_assert (parent == GTK_WIDGET (self->top_right_box) || parent == GTK_WIDGET (self->top_left_box));
      gtk_container_remove (GTK_CONTAINER (parent), widget);
    }
  g_ptr_array_set_size (self->custom_widgets, 0);

  CC_EXIT;
}

static void
show_panel (CcWindow *self)
{
  hdy_leaflet_set_visible_child (self->main_leaflet, GTK_WIDGET (self->stack));
  hdy_leaflet_set_visible_child (self->header_box, GTK_WIDGET (self->panel_headerbar));
}

static void
show_sidebar (CcWindow *self)
{
  hdy_leaflet_set_visible_child (self->main_leaflet, GTK_WIDGET (self->sidebar_box));
  hdy_leaflet_set_visible_child (self->header_box, GTK_WIDGET (self->header));
}

static void
on_sidebar_activated_cb (CcWindow *self)
{
  show_panel (self);
}

static gboolean
activate_panel (CcWindow          *self,
                const gchar       *id,
                GVariant          *parameters,
                const gchar       *name,
                GIcon             *gicon,
                CcPanelVisibility  visibility)
{
  g_autoptr(GTimer) timer = NULL;
  GtkWidget *sidebar_widget;
  GtkWidget *title_widget;
  gdouble ellapsed_time;

  CC_ENTRY;

  if (!id)
    CC_RETURN (FALSE);

  if (visibility == CC_PANEL_HIDDEN)
    CC_RETURN (FALSE);

  /* clear any custom widgets */
  remove_all_custom_widgets (self);

  timer = g_timer_new ();

  g_settings_set_string (self->settings, "last-panel", id);

  /* Begin the profile */
  g_timer_start (timer);

  if (self->current_panel)
    g_signal_handlers_disconnect_by_data (self->current_panel, self);
  self->current_panel = GTK_WIDGET (cc_panel_loader_load_by_name (CC_SHELL (self), id, parameters));
  cc_shell_set_active_panel (CC_SHELL (self), CC_PANEL (self->current_panel));
  gtk_widget_show (self->current_panel);

  gtk_stack_add_named (self->stack, self->current_panel, id);

  /* switch to the new panel */
  gtk_widget_show (self->current_panel);
  gtk_stack_set_visible_child_name (self->stack, id);

  /* set the title of the window */
  gtk_window_set_role (GTK_WINDOW (self), id);
  gtk_header_bar_set_title (self->panel_headerbar, name);

  title_widget = cc_panel_get_title_widget (CC_PANEL (self->current_panel));
  gtk_header_bar_set_custom_title (self->panel_headerbar, title_widget);

  sidebar_widget = cc_panel_get_sidebar_widget (CC_PANEL (self->current_panel));
  cc_panel_list_add_sidebar_widget (self->panel_list, sidebar_widget);
  /* Ensure we show the panel when the leaflet is folded and a sidebar widget's
   * row is activated.
   */
  g_signal_connect_object (self->current_panel, "sidebar-activated", G_CALLBACK (on_sidebar_activated_cb), self, G_CONNECT_SWAPPED);

  /* Finish profiling */
  g_timer_stop (timer);

  ellapsed_time = g_timer_elapsed (timer, NULL);

  g_debug ("Time to open panel '%s': %lfs", name, ellapsed_time);

  CC_RETURN (TRUE);
}

static void
add_current_panel_to_history (CcWindow   *self,
                              const char *start_id)
{
  g_return_if_fail (start_id != NULL);

  if (!self->current_panel_id || g_strcmp0 (self->current_panel_id, start_id) == 0)
    return;

  g_queue_push_head (self->previous_panels, g_strdup (self->current_panel_id));
  g_debug ("Added '%s' to the previous panels", self->current_panel_id);
}

static gboolean
find_iter_for_panel_id (CcWindow    *self,
                        const gchar *panel_id,
                        GtkTreeIter *out_iter)
{
  GtkTreeIter iter;
  gboolean valid;

  valid = gtk_tree_model_get_iter_first (GTK_TREE_MODEL (self->store), &iter);

  while (valid)
    {
      g_autofree gchar *id = NULL;

      gtk_tree_model_get (GTK_TREE_MODEL (self->store),
                          &iter,
                          COL_ID, &id,
                          -1);

      if (g_strcmp0 (id, panel_id) == 0)
        break;

      valid = gtk_tree_model_iter_next (GTK_TREE_MODEL (self->store), &iter);
    }

  g_assert (out_iter != NULL);
  *out_iter = iter;

  return valid;
}

static void
update_list_title (CcWindow *self)
{
  CcPanelListView view;
  GtkTreeIter iter;
  g_autofree gchar *title = NULL;

  CC_ENTRY;

  view = cc_panel_list_get_view (self->panel_list);
  title = NULL;

  switch (view)
    {
    case CC_PANEL_LIST_PRIVACY:
      title = g_strdup (_("Privacy"));
      break;

    case CC_PANEL_LIST_MAIN:
      title = g_strdup (_("Settings"));
      break;

    case CC_PANEL_LIST_WIDGET:
      find_iter_for_panel_id (self, self->current_panel_id, &iter);
      gtk_tree_model_get (GTK_TREE_MODEL (self->store),
                          &iter,
                          COL_NAME, &title,
                          -1);
      break;

    case CC_PANEL_LIST_SEARCH:
      title = NULL;
      break;
    }

  if (title)
    gtk_header_bar_set_title (self->header, title);

  CC_EXIT;
}

static void
on_row_changed_cb (CcWindow     *self,
                   GtkTreePath  *path,
                   GtkTreeIter  *iter,
                   GtkTreeModel *model)
{
  g_autofree gchar *id = NULL;
  CcPanelVisibility visibility;

  gtk_tree_model_get (model, iter,
                      COL_ID, &id,
                      COL_VISIBILITY, &visibility,
                      -1);

  cc_panel_list_set_panel_visibility (self->panel_list, id, visibility);
}

static void
setup_model (CcWindow *self)
{
  GtkTreeModel *model;
  GtkTreeIter iter;
  gboolean valid;

  /* CcApplication must have a valid model at this point */
  g_assert (self->store != NULL);

  model = GTK_TREE_MODEL (self->store);

  cc_panel_loader_fill_model (self->store);

  /* Create a row for each panel */
  valid = gtk_tree_model_get_iter_first (model, &iter);

  while (valid)
    {
      CcPanelCategory category;
      g_autoptr(GIcon) icon = NULL;
      g_autofree gchar *name = NULL;
      g_autofree gchar *description = NULL;
      g_autofree gchar *id = NULL;
      g_auto(GStrv) keywords = NULL;
      CcPanelVisibility visibility;
      gboolean has_sidebar;
      const gchar *icon_name = NULL;

      gtk_tree_model_get (model, &iter,
                          COL_CATEGORY, &category,
                          COL_DESCRIPTION, &description,
                          COL_GICON, &icon,
                          COL_ID, &id,
                          COL_NAME, &name,
                          COL_KEYWORDS, &keywords,
                          COL_VISIBILITY, &visibility,
                          COL_HAS_SIDEBAR, &has_sidebar,
                          -1);

      if (G_IS_THEMED_ICON (icon))
        icon_name = g_themed_icon_get_names (G_THEMED_ICON (icon))[0];

      cc_panel_list_add_panel (self->panel_list,
                               category,
                               id,
                               name,
                               description,
                               keywords,
                               icon_name,
                               visibility,
                               has_sidebar);

      valid = gtk_tree_model_iter_next (model, &iter);
    }

  /* React to visibility changes */
  g_signal_connect_object (model, "row-changed", G_CALLBACK (on_row_changed_cb), self, G_CONNECT_SWAPPED);
}

static void
update_headerbar_buttons (CcWindow *self)
{
  gboolean is_main_view;

  CC_ENTRY;

  is_main_view = cc_panel_list_get_view (self->panel_list) == CC_PANEL_LIST_MAIN;

  gtk_widget_set_visible (GTK_WIDGET (self->previous_button), !is_main_view);
  gtk_widget_set_visible (GTK_WIDGET (self->search_button), is_main_view);

  update_list_title (self);

  CC_EXIT;
}

static gboolean
set_active_panel_from_id (CcWindow     *self,
                          const gchar  *start_id,
                          GVariant     *parameters,
                          gboolean      add_to_history,
                          gboolean      force_moving_to_the_panel,
                          GError      **error)
{
  g_autoptr(GIcon) gicon = NULL;
  g_autofree gchar *name = NULL;
  CcPanelVisibility visibility;
  GtkTreeIter iter;
  GtkWidget *old_panel;
  CcPanelListView view;
  gboolean activated;
  gboolean found;

  CC_ENTRY;

  view = cc_panel_list_get_view (self->panel_list);

  /* When loading the same panel again, just set its parameters */
  if (g_strcmp0 (self->current_panel_id, start_id) == 0)
    {
      g_object_set (G_OBJECT (self->current_panel), "parameters", parameters, NULL);
      if (force_moving_to_the_panel || self->previous_list_view == view)
        show_panel (self);
      self->previous_list_view = view;
      CC_RETURN (TRUE);
    }

  found = find_iter_for_panel_id (self, start_id, &iter);
  if (!found)
    {
      g_warning ("Could not find settings panel \"%s\"", start_id);
      CC_RETURN (TRUE);
    }

  old_panel = self->current_panel;

  gtk_tree_model_get (GTK_TREE_MODEL (self->store),
                      &iter,
                      COL_NAME, &name,
                      COL_GICON, &gicon,
                      COL_VISIBILITY, &visibility,
                      -1);

  /* Activate the panel */
  activated = activate_panel (self, start_id, parameters, name, gicon, visibility);

  /* Failed to activate the panel for some reason, let's keep the old
   * panel around instead */
  if (!activated)
    {
      g_debug ("Failed to activate panel");
      CC_RETURN (TRUE);
    }

  if (add_to_history)
    add_current_panel_to_history (self, start_id);

  if (force_moving_to_the_panel)
    show_panel (self);

  g_free (self->current_panel_id);
  self->current_panel_id = g_strdup (start_id);

  CC_TRACE_MSG ("Current panel id: %s", start_id);

  if (old_panel)
    gtk_container_remove (GTK_CONTAINER (self->stack), old_panel);

  cc_panel_list_set_active_panel (self->panel_list, start_id);

  update_headerbar_buttons (self);

  CC_RETURN (TRUE);
}

static void
set_active_panel (CcWindow *self,
                  CcPanel  *panel)
{
  g_return_if_fail (CC_IS_SHELL (self));
  g_return_if_fail (panel == NULL || CC_IS_PANEL (panel));

  if (panel != self->active_panel)
    {
      /* remove the old panel */
      g_clear_object (&self->active_panel);

      /* set the new panel */
      if (panel)
        self->active_panel = g_object_ref (panel);

      g_object_notify (G_OBJECT (self), "active-panel");
    }
}

static void
switch_to_previous_panel (CcWindow *self)
{
  g_autofree gchar *previous_panel_id = NULL;

  CC_ENTRY;

  if (g_queue_get_length (self->previous_panels) == 0)
    CC_RETURN ();

  previous_panel_id = g_queue_pop_head (self->previous_panels);

  g_debug ("Going to previous panel (%s)", previous_panel_id);

  set_active_panel_from_id (self, previous_panel_id, NULL, FALSE, FALSE, NULL);

  CC_EXIT;
}

/* Callbacks */
static void
on_main_leaflet_folded_changed_cb (CcWindow *self)
{
  GtkSelectionMode selection_mode;

  g_assert (CC_IS_WINDOW (self));

  selection_mode = GTK_SELECTION_SINGLE;

  if (hdy_leaflet_get_folded (self->main_leaflet))
    selection_mode = GTK_SELECTION_NONE;

  cc_panel_list_set_selection_mode (self->panel_list, selection_mode);
}

static void
show_panel_cb (CcWindow    *self,
               const gchar *panel_id)
{
  if (!panel_id)
    return;

  set_active_panel_from_id (self, panel_id, NULL, TRUE, FALSE, NULL);
}

static void
search_entry_activate_cb (CcWindow *self)
{
  gboolean changed;

  changed = cc_panel_list_activate (self->panel_list);

  gtk_search_bar_set_search_mode (self->search_bar, !changed);
}

static void
back_button_clicked_cb (CcWindow *self)
{
  show_sidebar (self);
}

static void
previous_button_clicked_cb (CcWindow *self)
{
  g_debug ("Num previous panels? %d", g_queue_get_length (self->previous_panels));

  /* When in search, simply unsed the search mode */
  if (gtk_search_bar_get_search_mode (self->search_bar))
    gtk_search_bar_set_search_mode (self->search_bar, FALSE);
  else
    cc_panel_list_go_previous (self->panel_list);

  update_headerbar_buttons (self);
}

static void
gdk_window_set_cb (CcWindow *self)
{
  GdkWindow *window;
  g_autofree gchar *str = NULL;

  if (!GDK_IS_X11_DISPLAY (gdk_display_get_default ()))
    return;

  window = gtk_widget_get_window (GTK_WIDGET (self));

  if (!window)
    return;

  str = g_strdup_printf ("%u", (guint) GDK_WINDOW_XID (window));
  g_setenv ("GNOME_CONTROL_CENTER_XID", str, TRUE);
}

static gboolean
window_map_event_cb (CcWindow *self)
{
  /* If focus ends up in a category icon view one of the items is
   * immediately selected which looks odd when we are starting up, so
   * we explicitly unset the focus here. */
  gtk_window_set_focus (GTK_WINDOW (self), NULL);
  return GDK_EVENT_PROPAGATE;
}

static gboolean
window_key_press_event_cb (CcWindow    *self,
                           GdkEventKey *event)
{
  GdkModifierType state;
  CcPanelListView view;
  GdkKeymap *keymap;
  gboolean retval;
  gboolean is_rtl;

  retval = GDK_EVENT_PROPAGATE;
  state = event->state;
  keymap = gdk_keymap_get_for_display (gtk_widget_get_display (GTK_WIDGET (self)));
  gdk_keymap_add_virtual_modifiers (keymap, &state);

  state = state & gtk_accelerator_get_default_mod_mask ();
  is_rtl = gtk_widget_get_direction (GTK_WIDGET (self)) == GTK_TEXT_DIR_RTL;
  view = cc_panel_list_get_view (self->panel_list);

  /* The search only happens when we're in the MAIN view */
  if (view == CC_PANEL_LIST_MAIN &&
      gtk_search_bar_handle_event (self->search_bar, (GdkEvent*) event) == GDK_EVENT_STOP)
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

            retval = !gtk_search_bar_get_search_mode (self->search_bar);
            gtk_search_bar_set_search_mode (self->search_bar, retval);
            if (retval)
              gtk_widget_grab_focus (GTK_WIDGET (self->search_entry));
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
      g_debug ("Going to previous panel");
      switch_to_previous_panel (self);
      retval = GDK_EVENT_STOP;
    }

  return retval;
}

static void
on_development_warning_dialog_responded_cb (CcWindow *self)
{
  g_debug ("Disabling development build warning dialog");
  g_settings_set_boolean (self->settings, "show-development-warning", FALSE);

  gtk_widget_hide (GTK_WIDGET (self->development_warning_dialog));
}

/* CcShell implementation */
static gboolean
cc_window_set_active_panel_from_id (CcShell      *shell,
                                    const gchar  *start_id,
                                    GVariant     *parameters,
                                    GError      **error)
{
  return set_active_panel_from_id (CC_WINDOW (shell), start_id, parameters, TRUE, TRUE, error);
}

static void
cc_window_embed_widget_in_header (CcShell         *shell,
                                  GtkWidget       *widget,
                                  GtkPositionType  position)
{
  CcWindow *self = CC_WINDOW (shell);

  CC_ENTRY;

  /* add to header */
  switch (position)
    {
    case GTK_POS_RIGHT:
      gtk_container_add (GTK_CONTAINER (self->top_right_box), widget);
      break;

    case GTK_POS_LEFT:
      gtk_container_add (GTK_CONTAINER (self->top_left_box), widget);
      break;

    case GTK_POS_TOP:
    case GTK_POS_BOTTOM:
    default:
      g_warning ("Invalid position passed");
      return;
    }

  g_ptr_array_add (self->custom_widgets, g_object_ref (widget));

  gtk_size_group_add_widget (self->header_sizegroup, widget);

  CC_EXIT;
}

static GtkWidget *
cc_window_get_toplevel (CcShell *self)
{
  return GTK_WIDGET (self);
}

static void
cc_shell_iface_init (CcShellInterface *iface)
{
  iface->set_active_panel_from_id = cc_window_set_active_panel_from_id;
  iface->embed_widget_in_header = cc_window_embed_widget_in_header;
  iface->get_toplevel = cc_window_get_toplevel;
}

/* GtkWidget overrides */
static void
cc_window_map (GtkWidget *widget)
{
  CcWindow *self = (CcWindow *) widget;

  GTK_WIDGET_CLASS (cc_window_parent_class)->map (widget);

  /* Show a warning for Flatpak builds */
  if (in_flatpak_sandbox () && g_settings_get_boolean (self->settings, "show-development-warning"))
    gtk_window_present (GTK_WINDOW (self->development_warning_dialog));
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

    case PROP_MODEL:
      g_value_set_object (value, self->store);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
    }
}

static void
cc_window_set_property (GObject      *object,
                        guint         property_id,
                        const GValue *value,
                        GParamSpec   *pspec)
{
  CcWindow *self = CC_WINDOW (object);

  switch (property_id)
    {
    case PROP_ACTIVE_PANEL:
      set_active_panel (self, g_value_get_object (value));
      break;

    case PROP_MODEL:
      g_assert (self->store == NULL);
      self->store = g_value_dup_object (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
    }
}

static void
cc_window_constructed (GObject *object)
{
  CcWindow *self = CC_WINDOW (object);
  g_autofree char *id = NULL;

  /* Add the panels */
  setup_model (self);

  /* After everything is loaded, select the last used panel, if any,
   * or the first visible panel */
  id = g_settings_get_string (self->settings, "last-panel");
  if (id != NULL && cc_shell_model_has_panel (self->store, id))
    cc_panel_list_set_active_panel (self->panel_list, id);
  else
    cc_panel_list_activate (self->panel_list);

  g_signal_connect_swapped (self->panel_list,
                            "notify::view",
                            G_CALLBACK (update_headerbar_buttons),
                            self);

  update_headerbar_buttons (self);
  show_sidebar (self);

  G_OBJECT_CLASS (cc_window_parent_class)->constructed (object);
}

static void
cc_window_dispose (GObject *object)
{
  CcWindow *self = CC_WINDOW (object);

  g_clear_pointer (&self->current_panel_id, g_free);
  g_clear_pointer (&self->custom_widgets, g_ptr_array_unref);
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

  g_clear_object (&self->settings);

  G_OBJECT_CLASS (cc_window_parent_class)->finalize (object);
}

static void
cc_window_class_init (CcWindowClass *klass)
{
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->get_property = cc_window_get_property;
  object_class->set_property = cc_window_set_property;
  object_class->constructed = cc_window_constructed;
  object_class->dispose = cc_window_dispose;
  object_class->finalize = cc_window_finalize;

  widget_class->map = cc_window_map;

  g_object_class_override_property (object_class, PROP_ACTIVE_PANEL, "active-panel");

  g_object_class_install_property (object_class,
                                   PROP_MODEL,
                                   g_param_spec_object ("model",
                                                        "Model",
                                                        "The CcShellModel of this application",
                                                        CC_TYPE_SHELL_MODEL,
                                                        G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/ControlCenter/gtk/cc-window.ui");

  gtk_widget_class_bind_template_child (widget_class, CcWindow, back_revealer);
  gtk_widget_class_bind_template_child (widget_class, CcWindow, development_warning_dialog);
  gtk_widget_class_bind_template_child (widget_class, CcWindow, header);
  gtk_widget_class_bind_template_child (widget_class, CcWindow, header_box);
  gtk_widget_class_bind_template_child (widget_class, CcWindow, header_sizegroup);
  gtk_widget_class_bind_template_child (widget_class, CcWindow, main_leaflet);
  gtk_widget_class_bind_template_child (widget_class, CcWindow, panel_headerbar);
  gtk_widget_class_bind_template_child (widget_class, CcWindow, panel_list);
  gtk_widget_class_bind_template_child (widget_class, CcWindow, previous_button);
  gtk_widget_class_bind_template_child (widget_class, CcWindow, search_bar);
  gtk_widget_class_bind_template_child (widget_class, CcWindow, search_button);
  gtk_widget_class_bind_template_child (widget_class, CcWindow, search_entry);
  gtk_widget_class_bind_template_child (widget_class, CcWindow, sidebar_box);
  gtk_widget_class_bind_template_child (widget_class, CcWindow, stack);
  gtk_widget_class_bind_template_child (widget_class, CcWindow, top_left_box);
  gtk_widget_class_bind_template_child (widget_class, CcWindow, top_right_box);

  gtk_widget_class_bind_template_callback (widget_class, back_button_clicked_cb);
  gtk_widget_class_bind_template_callback (widget_class, gdk_window_set_cb);
  gtk_widget_class_bind_template_callback (widget_class, on_main_leaflet_folded_changed_cb);
  gtk_widget_class_bind_template_callback (widget_class, on_development_warning_dialog_responded_cb);
  gtk_widget_class_bind_template_callback (widget_class, previous_button_clicked_cb);
  gtk_widget_class_bind_template_callback (widget_class, search_entry_activate_cb);
  gtk_widget_class_bind_template_callback (widget_class, show_panel_cb);
  gtk_widget_class_bind_template_callback (widget_class, update_list_title);
  gtk_widget_class_bind_template_callback (widget_class, window_key_press_event_cb);
  gtk_widget_class_bind_template_callback (widget_class, window_map_event_cb);

  g_type_ensure (CC_TYPE_PANEL_LIST);
}

static void
cc_window_init (CcWindow *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));

  gtk_widget_add_events (GTK_WIDGET (self), GDK_BUTTON_RELEASE_MASK);

  self->settings = g_settings_new ("org.gnome.ControlCenter");
  self->custom_widgets = g_ptr_array_new_with_free_func ((GDestroyNotify) g_object_unref);
  self->previous_panels = g_queue_new ();
  self->previous_list_view = cc_panel_list_get_view (self->panel_list);

  /* Add a custom CSS class on development builds */
  if (in_flatpak_sandbox ())
    gtk_style_context_add_class (gtk_widget_get_style_context (GTK_WIDGET (self)), "devel");
}

CcWindow *
cc_window_new (GtkApplication *application,
               CcShellModel   *model)
{
  g_return_val_if_fail (GTK_IS_APPLICATION (application), NULL);

  return g_object_new (CC_TYPE_WINDOW,
                       "application", application,
                       "resizable", TRUE,
                       "title", _("Settings"),
                       "icon-name", DEFAULT_WINDOW_ICON_NAME,
                       "window-position", GTK_WIN_POS_CENTER,
                       "show-menubar", FALSE,
                       "model", model,
                       NULL);
}

void
cc_window_set_search_item (CcWindow   *center,
                           const char *search)
{
  gtk_search_bar_set_search_mode (center->search_bar, TRUE);
  gtk_entry_set_text (GTK_ENTRY (center->search_entry), search);
  gtk_editable_set_position (GTK_EDITABLE (center->search_entry), -1);
}
