/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */
/*
 * Copyright 2024 GNOME Foundation, Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General
 * Public License along with this library; if not, see <http://www.gnu.org/licenses/>.
 *
 * Authors:
 *  - Philip Withnall <pwithnall@gnome.org>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include <glib.h>
#include <glib-object.h>
#include <gtk/gtk.h>

#include "cc-bar-chart-group.h"
#include "cc-bar-chart-bar.h"

/**
 * CcBarChartGroup:
 *
 * #CcBarChartGroup is a grouping of bars in a #CcBarChart.
 *
 * It contains only #CcBarChartBar children. Currently, exactly one bar is
 * supported per group, but this could be relaxed in future to support multiple
 * grouped bars.
 *
 * #CcBarChartGroup forms the touch landing area for highlighting and selecting
 * a #CcBarChartBar, regardless of its rendered height. The group as a whole
 * may be selected, indicated by #CcBarChartGroup:is-selected.
 *
 * # CSS nodes
 *
 * |[<!-- language="plain" -->
 * bar-group[:hover][:selected]
 * ╰── bar[:hover][:selected]
 * ]|
 *
 * #CcBarChartGroup uses a single CSS node named `bar-group`. Each bar is a
 * sub-node named `bar`. Bars and groups may have `:hover` or `:selected`
 * pseudo-selectors to indicate whether they are selected or being hovered over
 * with the mouse.
 *
 * # Accessibility
 *
 * #CcBarChartGroup uses the %GTK_ACCESSIBLE_ROLE_GROUP role and #CcBarChartBar
 * uses the %GTK_ACCESSIBLE_ROLE_LIST_ITEM role.
 */
struct _CcBarChartGroup {
  GtkWidget parent_instance;

  /* Configured state: */
  gboolean selectable;
  enum
    {
      SELECTION_STATE_NONE,
      SELECTION_STATE_GROUP,
      SELECTION_STATE_BAR,
    }
  selection_state;
  unsigned int selected_bar_index;  /* only defined if selection_state == SELECTION_STATE_BAR */

  double scale;  /* number of pixels per data value */
  GPtrArray *bars;  /* (not nullable) (owned) (element-type CcBarChartBar) */
};

G_DEFINE_TYPE (CcBarChartGroup, cc_bar_chart_group, GTK_TYPE_WIDGET)

typedef enum {
  PROP_SELECTABLE = 1,
  PROP_IS_SELECTED,
  PROP_SELECTED_INDEX,
  PROP_SELECTED_INDEX_SET,
  PROP_SCALE,
} CcBarChartGroupProperty;

static GParamSpec *props[PROP_SCALE + 1];

static void cc_bar_chart_group_get_property (GObject    *object,
                                             guint       property_id,
                                             GValue     *value,
                                             GParamSpec *pspec);
static void cc_bar_chart_group_set_property (GObject      *object,
                                             guint         property_id,
                                             const GValue *value,
                                             GParamSpec   *pspec);
static void cc_bar_chart_group_dispose (GObject *object);
static void cc_bar_chart_group_size_allocate (GtkWidget *widget,
                                              int        width,
                                              int        height,
                                              int        baseline);
static void cc_bar_chart_group_measure (GtkWidget      *widget,
                                        GtkOrientation  orientation,
                                        int             for_size,
                                        int            *minimum,
                                        int            *natural,
                                        int            *minimum_baseline,
                                        int            *natural_baseline);
static gboolean cc_bar_chart_group_focus (GtkWidget        *widget,
                                          GtkDirectionType  direction);
static void gesture_click_pressed_cb (GtkGestureClick *gesture,
                                      guint            n_press,
                                      double           x,
                                      double           y,
                                      gpointer         user_data);

static gboolean find_index_for_bar (CcBarChartGroup *self,
                                    CcBarChartBar   *bar,
                                    unsigned int    *out_idx);
static gboolean bar_is_focusable (CcBarChartBar *bar);
static CcBarChartBar *get_adjacent_focusable_bar (CcBarChartGroup *self,
                                                  CcBarChartBar   *bar,
                                                  int              direction);
static CcBarChartBar *get_first_focusable_bar (CcBarChartGroup *self);
static CcBarChartBar *get_last_focusable_bar (CcBarChartGroup *self);

static void
cc_bar_chart_group_class_init (CcBarChartGroupClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->get_property = cc_bar_chart_group_get_property;
  object_class->set_property = cc_bar_chart_group_set_property;
  object_class->dispose = cc_bar_chart_group_dispose;

  widget_class->size_allocate = cc_bar_chart_group_size_allocate;
  widget_class->measure = cc_bar_chart_group_measure;
  widget_class->focus = cc_bar_chart_group_focus;

  /**
   * CcBarChartGroup:selectable:
   *
   * Whether the group itself can be selected.
   *
   * If `FALSE`, any attempt to select the group will select its first bar
   * instead.
   */
  props[PROP_SELECTABLE] =
    g_param_spec_boolean ("selectable",
                          NULL, NULL,
                          TRUE,
                          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_EXPLICIT_NOTIFY);

  /**
   * CcBarChartGroup:is-selected:
   *
   * Whether the group itself is currently selected.
   */
  props[PROP_IS_SELECTED] =
    g_param_spec_boolean ("is-selected",
                          NULL, NULL,
                          FALSE,
                          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_EXPLICIT_NOTIFY);

  /**
   * CcBarChartGroup:selected-index:
   *
   * Index of the currently selected bar.
   *
   * If nothing is currently selected, the value of this property is undefined.
   * See #CcBarChartGroup:selected-index-set to check this.
   */
  props[PROP_SELECTED_INDEX] =
    g_param_spec_uint ("selected-index",
                       NULL, NULL,
                       0, G_MAXUINT, 0,
                       G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_EXPLICIT_NOTIFY);

  /**
   * CcBarChartGroup:selected-index-set:
   *
   * Whether a bar is currently selected.
   *
   * If this property is `TRUE`, the value of #CcBarChartGroup:selected-index is
   * defined.
   */
  props[PROP_SELECTED_INDEX_SET] =
    g_param_spec_boolean ("selected-index-set",
                          NULL, NULL,
                          FALSE,
                          G_PARAM_READABLE | G_PARAM_STATIC_STRINGS | G_PARAM_EXPLICIT_NOTIFY);

  /**
   * CcBarChartGroup:scale:
   *
   * Scale used to render the bars in the group, in pixels per data unit.
   *
   * It must be greater than `0.0`.
   *
   * This is used internally and does not need to be set by code outside
   * #CcBarChart.
   */
  props[PROP_SCALE] =
    g_param_spec_double ("scale",
                         NULL, NULL,
                         -G_MAXDOUBLE, G_MAXDOUBLE, 1.0,
                         G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_EXPLICIT_NOTIFY);

  g_object_class_install_properties (object_class, G_N_ELEMENTS (props), props);

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/control-center/wellbeing/cc-bar-chart-group.ui");

  gtk_widget_class_set_css_name (widget_class, "bar-group");
  gtk_widget_class_set_accessible_role (widget_class, GTK_ACCESSIBLE_ROLE_GROUP);
}

static void
cc_bar_chart_group_init (CcBarChartGroup *self)
{
  g_autoptr(GtkGestureClick) gesture = NULL;

  gtk_widget_init_template (GTK_WIDGET (self));

  self->selectable = TRUE;
  self->bars = g_ptr_array_new_null_terminated (1, (GDestroyNotify) gtk_widget_unparent, TRUE);

  /* Handle clicks */
  gesture = GTK_GESTURE_CLICK (gtk_gesture_click_new ());
  g_signal_connect (gesture, "pressed", G_CALLBACK (gesture_click_pressed_cb), self);
  gtk_widget_add_controller (GTK_WIDGET (self), GTK_EVENT_CONTROLLER (g_steal_pointer (&gesture)));
}

static void
cc_bar_chart_group_get_property (GObject    *object,
                                 guint       property_id,
                                 GValue     *value,
                                 GParamSpec *pspec)
{
  CcBarChartGroup *self = CC_BAR_CHART_GROUP (object);

  switch ((CcBarChartGroupProperty) property_id)
    {
    case PROP_SELECTABLE:
      g_value_set_boolean (value, cc_bar_chart_group_get_selectable (self));
      break;
    case PROP_IS_SELECTED:
      g_value_set_boolean (value, cc_bar_chart_group_get_is_selected (self));
      break;
    case PROP_SELECTED_INDEX: {
      size_t idx;
      gboolean valid = cc_bar_chart_group_get_selected_index (self, &idx);
      g_value_set_uint (value, valid ? idx : 0);
      break;
    }
    case PROP_SELECTED_INDEX_SET:
      g_value_set_boolean (value, cc_bar_chart_group_get_selected_index (self, NULL));
      break;
    case PROP_SCALE:
      g_value_set_double (value, cc_bar_chart_group_get_scale (self));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
    }
}

static void
cc_bar_chart_group_set_property (GObject      *object,
                                 guint         property_id,
                                 const GValue *value,
                                 GParamSpec   *pspec)
{
  CcBarChartGroup *self = CC_BAR_CHART_GROUP (object);

  switch ((CcBarChartGroupProperty) property_id)
    {
    case PROP_SELECTABLE:
      cc_bar_chart_group_set_selectable (self, g_value_get_boolean (value));
      break;
    case PROP_IS_SELECTED:
      cc_bar_chart_group_set_is_selected (self, g_value_get_boolean (value));
      break;
    case PROP_SELECTED_INDEX:
      cc_bar_chart_group_set_selected_index (self, TRUE, g_value_get_uint (value));
      break;
    case PROP_SELECTED_INDEX_SET:
      /* Read only */
      g_assert_not_reached ();
      break;
    case PROP_SCALE:
      cc_bar_chart_group_set_scale (self, g_value_get_double (value));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
    }
}

static void
cc_bar_chart_group_dispose (GObject *object)
{
  CcBarChartGroup *self = CC_BAR_CHART_GROUP (object);

  g_clear_pointer (&self->bars, g_ptr_array_unref);
  gtk_widget_dispose_template (GTK_WIDGET (object), CC_TYPE_BAR_CHART_GROUP);

  G_OBJECT_CLASS (cc_bar_chart_group_parent_class)->dispose (object);
}

static void
cc_bar_chart_group_size_allocate (GtkWidget *widget,
                                  int        width,
                                  int        height,
                                  int        baseline)
{
  CcBarChartGroup *self = CC_BAR_CHART_GROUP (widget);

  for (unsigned int i = 0; i < self->bars->len; i++)
    {
      CcBarChartBar *bar = self->bars->pdata[i];
      int bar_top_y, bar_bottom_y, bar_left_x, bar_right_x;
      GtkAllocation child_alloc;

      /* If drawing RTL, reverse the bar positions. */
      if (gtk_widget_get_direction (GTK_WIDGET (self)) == GTK_TEXT_DIR_RTL)
        bar = self->bars->pdata[self->bars->len - i - 1];

      bar_left_x = width * i / self->bars->len;
      bar_right_x = width * (i + 1) / self->bars->len;

      bar_top_y = height - cc_bar_chart_bar_get_value (bar) * self->scale;
      bar_bottom_y = height;

      child_alloc.x = bar_left_x;
      child_alloc.y = bar_top_y;
      child_alloc.width = bar_right_x - bar_left_x;
      child_alloc.height = bar_bottom_y - bar_top_y;

      gtk_widget_size_allocate (GTK_WIDGET (bar), &child_alloc, -1);
    }
}

static void
cc_bar_chart_group_measure (GtkWidget      *widget,
                            GtkOrientation  orientation,
                            int             for_size,
                            int            *minimum,
                            int            *natural,
                            int            *minimum_baseline,
                            int            *natural_baseline)
{
  CcBarChartGroup *self = CC_BAR_CHART_GROUP (widget);

  if (orientation == GTK_ORIENTATION_HORIZONTAL)
    {
      int total_minimum_width = 0, total_natural_width = 0;

      for (unsigned int i = 0; i < self->bars->len; i++)
        {
          CcBarChartBar *bar = self->bars->pdata[i];
          int bar_minimum_width = 0, bar_natural_width = 0;

          gtk_widget_measure (GTK_WIDGET (bar), orientation, -1,
                              &bar_minimum_width, &bar_natural_width,
                              NULL, NULL);

          total_minimum_width += bar_minimum_width;
          total_natural_width += bar_natural_width;
        }

      *minimum = total_minimum_width;
      *natural = total_natural_width;
      *minimum_baseline = -1;
      *natural_baseline = -1;
    }
  else if (orientation == GTK_ORIENTATION_VERTICAL)
    {
      int maximum_minimum_height = 0, maximum_natural_height = 0;

      for (unsigned int i = 0; i < self->bars->len; i++)
        {
          CcBarChartBar *bar = self->bars->pdata[i];
          int bar_minimum_height = 0, bar_natural_height = 0;

          gtk_widget_measure (GTK_WIDGET (bar), orientation, -1,
                              &bar_minimum_height, &bar_natural_height,
                              NULL, NULL);

          maximum_minimum_height = MAX (maximum_minimum_height, bar_minimum_height);
          maximum_natural_height = MAX (maximum_natural_height, bar_natural_height);
        }

      *minimum = maximum_minimum_height;
      *natural = maximum_natural_height;
      *minimum_baseline = -1;
      *natural_baseline = -1;
    }
  else
    {
      g_assert_not_reached ();
    }
}

static gboolean
cc_bar_chart_group_focus (GtkWidget        *widget,
                          GtkDirectionType  direction)
{
  CcBarChartGroup *self = CC_BAR_CHART_GROUP (widget);
  GtkWidget *focus_child;
  CcBarChartBar *next_focus_bar = NULL;

  /* Reverse the direction if in RTL mode, as the chart presents things on a
   * left–right axis. */
  if (gtk_widget_get_direction (widget) == GTK_TEXT_DIR_RTL)
    {
      switch (direction)
        {
        case GTK_DIR_TAB_BACKWARD:
          direction = GTK_DIR_TAB_FORWARD;
          break;
        case GTK_DIR_TAB_FORWARD:
          direction = GTK_DIR_TAB_BACKWARD;
          break;
        case GTK_DIR_LEFT:
          direction = GTK_DIR_RIGHT;
          break;
        case GTK_DIR_RIGHT:
          direction = GTK_DIR_LEFT;
          break;
        case GTK_DIR_UP:
        case GTK_DIR_DOWN:
        default:
          /* No change. */
          break;
        }
    }

  focus_child = gtk_widget_get_focus_child (widget);

  if (focus_child != NULL)
    {
      /* Can the focus move around inside the currently focused child widget? */
      if (gtk_widget_child_focus (focus_child, direction))
        return TRUE;

      if (CC_IS_BAR_CHART_BAR (focus_child) &&
          (direction == GTK_DIR_LEFT || direction == GTK_DIR_TAB_BACKWARD))
        next_focus_bar = get_adjacent_focusable_bar (self, CC_BAR_CHART_BAR (focus_child), -1);
      else if (CC_IS_BAR_CHART_BAR (focus_child) &&
               (direction == GTK_DIR_RIGHT || direction == GTK_DIR_TAB_FORWARD))
        next_focus_bar = get_adjacent_focusable_bar (self, CC_BAR_CHART_BAR (focus_child), 1);
    }
  else
    {
      /* No current focus bar. If a bar is selected, focus on that. Otherwise,
       * focus on the first/last focusable bar, depending on which direction
       * we’re coming in from. */
      if (self->selection_state == SELECTION_STATE_BAR)
        next_focus_bar = self->bars->pdata[self->selected_bar_index];

      if (next_focus_bar == NULL &&
          (direction == GTK_DIR_UP || direction == GTK_DIR_LEFT || direction == GTK_DIR_TAB_BACKWARD))
        next_focus_bar = get_last_focusable_bar (self);
      else if (next_focus_bar == NULL &&
               (direction == GTK_DIR_DOWN || direction == GTK_DIR_RIGHT || direction == GTK_DIR_TAB_FORWARD))
        next_focus_bar = get_first_focusable_bar (self);
    }

  if (next_focus_bar == NULL)
    return FALSE;

  return gtk_widget_child_focus (GTK_WIDGET (next_focus_bar), direction);
}

static void
gesture_click_pressed_cb (GtkGestureClick *gesture,
                          guint            n_press,
                          double           x,
                          double           y,
                          gpointer         user_data)
{
  CcBarChartGroup *self = CC_BAR_CHART_GROUP (user_data);
  GtkWidget *bar;
  unsigned int bar_idx = 0;

  if (gtk_widget_get_focus_on_click (GTK_WIDGET (self)) && !gtk_widget_has_focus (GTK_WIDGET (self)))
    gtk_widget_grab_focus (GTK_WIDGET (self));

  bar = gtk_widget_pick (GTK_WIDGET (self), x, y, GTK_PICK_DEFAULT);

  if (!CC_IS_BAR_CHART_BAR (bar) ||
      !find_index_for_bar (self, CC_BAR_CHART_BAR (bar), &bar_idx))
    bar = NULL;

  /* Select and focus the bar or group. */
  if (bar != NULL)
    {
      if (bar_is_focusable (CC_BAR_CHART_BAR (bar)))
        gtk_widget_set_focus_child (GTK_WIDGET (self), bar);
      cc_bar_chart_group_set_selected_index (self, TRUE, bar_idx);
    }
  else
    {
      /* already grabbed focus above */
      cc_bar_chart_group_set_is_selected (self, TRUE);
    }
}

static gboolean
find_index_for_bar (CcBarChartGroup *self,
                    CcBarChartBar   *bar,
                    unsigned int    *out_idx)
{
  g_assert (gtk_widget_is_ancestor (GTK_WIDGET (bar), GTK_WIDGET (self)));
  g_assert (self->bars != NULL);

  return g_ptr_array_find (self->bars, bar, out_idx);
}

static gboolean
bar_is_focusable (CcBarChartBar *bar)
{
  GtkWidget *widget = GTK_WIDGET (bar);

  return (gtk_widget_is_visible (widget) &&
          gtk_widget_is_sensitive (widget) &&
          gtk_widget_get_focusable (widget) &&
          gtk_widget_get_can_focus (widget));
}

/* direction == -1 means get previous sensitive and visible bar;
 * direction == 1 means get next one. */
static CcBarChartBar *
get_adjacent_focusable_bar (CcBarChartGroup *self,
                            CcBarChartBar   *bar,
                            int              direction)
{
  unsigned int bar_idx, i;

  g_assert (gtk_widget_is_ancestor (GTK_WIDGET (bar), GTK_WIDGET (self)));
  g_assert (self->bars != NULL);
  g_assert (direction == -1 || direction == 1);

  if (!find_index_for_bar (self, bar, &bar_idx))
    return NULL;

  i = bar_idx;

  while (!((direction == -1 && i == 0) ||
           (direction == 1 && i >= self->bars->len - 1)))
    {
      CcBarChartBar *adjacent_bar;

      i += direction;
      adjacent_bar = self->bars->pdata[i];

      if (bar_is_focusable (adjacent_bar))
        return adjacent_bar;
    }

  return NULL;
}

static CcBarChartBar *
get_first_focusable_bar (CcBarChartGroup *self)
{
  g_assert (self->bars != NULL);

  for (unsigned int i = 0; i < self->bars->len; i++)
    {
      CcBarChartBar *bar = self->bars->pdata[i];

      if (bar_is_focusable (bar))
        return bar;
    }

  return NULL;
}

static CcBarChartBar *
get_last_focusable_bar (CcBarChartGroup *self)
{
  g_assert (self->bars != NULL);

  for (unsigned int i = 0; i < self->bars->len; i++)
    {
      CcBarChartBar *bar = self->bars->pdata[self->bars->len - 1 - i];

      if (bar_is_focusable (bar))
        return bar;
    }

  return NULL;
}

/**
 * cc_bar_chart_group_new:
 *
 * Create a new #CcBarChartGroup.
 *
 * Returns: (transfer full): the new #CcBarChartGroup
 */
CcBarChartGroup *
cc_bar_chart_group_new (void)
{
  return g_object_new (CC_TYPE_BAR_CHART_GROUP, NULL);
}

/**
 * cc_bar_chart_group_get_bars:
 * @self: a #CcBarChartGroup
 * @out_n_bars: (out) (optional): return location for the number of bars,
 *   or `NULL` to ignore
 *
 * Get the bars in the group.
 *
 * If there are currently no bars in the group, `NULL` is returned and
 * @out_n_bars is set to `0`.
 *
 * Returns: (array length=out_n_bars zero-terminated=1) (nullable) (transfer none): array of
 *   bars in the group, or `NULL` if empty
 */
CcBarChartBar * const *
cc_bar_chart_group_get_bars (CcBarChartGroup *self,
                             size_t          *out_n_bars)
{
  g_return_val_if_fail (CC_IS_BAR_CHART_GROUP (self), NULL);

  if (out_n_bars != NULL)
    *out_n_bars = self->bars->len;

  return (self->bars->len != 0) ? (CcBarChartBar * const *) self->bars->pdata : NULL;
}

/**
 * cc_bar_chart_group_insert_bar:
 * @self: a #CcBarChartGroup
 * @idx: position to insert the bar at, or `-1` to append
 * @bar: (transfer none): bar to insert; will be sunk if floating
 *
 * Insert @bar into the group at index @idx.
 *
 * Pass `-1` to @idx to append the bar.
 *
 * @bar will be unparented from its existing parent (if set) first, so this
 * method can be used to rearrange bars within the group.
 */
void
cc_bar_chart_group_insert_bar (CcBarChartGroup *self,
                               int              idx,
                               CcBarChartBar   *bar)
{
  CcBarChartBar *previous_sibling;

  g_return_if_fail (CC_IS_BAR_CHART_GROUP (self));
  g_return_if_fail (CC_IS_BAR_CHART_BAR (bar));

  g_object_ref_sink (bar);

  gtk_widget_unparent (GTK_WIDGET (bar));
  gtk_widget_set_parent (GTK_WIDGET (bar), GTK_WIDGET (self));

  if (idx < 0 && self->bars->len > 0)
    previous_sibling = self->bars->pdata[self->bars->len - 1];
  else if (self->bars->len == 0 || idx == 0)
    previous_sibling = NULL;
  else
    previous_sibling = self->bars->pdata[idx - 1];

  gtk_widget_insert_after (GTK_WIDGET (bar), GTK_WIDGET (self), GTK_WIDGET (previous_sibling));

  g_ptr_array_insert (self->bars, idx, bar);

  g_object_unref (bar);

  gtk_widget_queue_resize (GTK_WIDGET (self));
}

/**
 * cc_bar_chart_group_remove_bar:
 * @self: a #CcBarChartGroup
 * @bar: (transfer none): bar to remove
 *
 * Remove @bar from the group.
 *
 * It is an error to call this on a @bar which is not currently in the group.
 */
void
cc_bar_chart_group_remove_bar (CcBarChartGroup *self,
                               CcBarChartBar   *bar)
{
  gboolean was_removed;

  g_return_if_fail (CC_IS_BAR_CHART_GROUP (self));
  g_return_if_fail (CC_IS_BAR_CHART_BAR (bar));

  was_removed = g_ptr_array_remove (self->bars, bar);
  g_assert (was_removed);

  gtk_widget_unparent (GTK_WIDGET (bar));

  gtk_widget_queue_resize (GTK_WIDGET (self));
}

/**
 * cc_bar_chart_group_get_selectable:
 * @self: a #CcBarChartGroup
 *
 * Get the value of #CcBarChartGroup:selectable.
 *
 * Returns: `TRUE` if the group itself is selectable, `FALSE` otherwise
 */
gboolean
cc_bar_chart_group_get_selectable (CcBarChartGroup *self)
{
  g_return_val_if_fail (CC_IS_BAR_CHART_GROUP (self), FALSE);

  return self->selectable;
}

/**
 * cc_bar_chart_group_set_selectable:
 * @self: a #CcBarChartGroup
 * @selectable: `TRUE` if the group itself is selectable, `FALSE` otherwise
 *
 * Set the value of #CcBarChartGroup:selectable.
 */
void
cc_bar_chart_group_set_selectable (CcBarChartGroup *self,
                                   gboolean         selectable)
{
  g_return_if_fail (CC_IS_BAR_CHART_GROUP (self));

  if (self->selectable == selectable)
    return;

  self->selectable = selectable;

  g_object_freeze_notify (G_OBJECT (self));
  g_object_notify_by_pspec (G_OBJECT (self), props[PROP_SELECTABLE]);

  /* If the group is currently selected but shouldn’t be any more, run through
   * the default fall-through selection logic again. */
  if (!self->selectable && self->selection_state == SELECTION_STATE_GROUP)
    cc_bar_chart_group_set_is_selected (self, TRUE);

  g_object_thaw_notify (G_OBJECT (self));
}

static void
set_or_unset_selection_state_flags (CcBarChartGroup *self,
                                    gboolean         set)
{
  GtkWidget *selected_widget;

  switch (self->selection_state)
    {
    case SELECTION_STATE_BAR:
      selected_widget = GTK_WIDGET (self->bars->pdata[self->selected_bar_index]);
      break;
    case SELECTION_STATE_GROUP:
      selected_widget = GTK_WIDGET (self);
      break;
    case SELECTION_STATE_NONE:
    default:
      selected_widget = NULL;
      break;
    }

  if (selected_widget != NULL)
    {
      if (set)
        gtk_widget_set_state_flags (selected_widget, GTK_STATE_FLAG_SELECTED, FALSE);
      else
        gtk_widget_unset_state_flags (selected_widget, GTK_STATE_FLAG_SELECTED);

      gtk_accessible_update_state (GTK_ACCESSIBLE (selected_widget),
                                   GTK_ACCESSIBLE_STATE_SELECTED, set,
                                   -1);
    }
}

/**
 * cc_bar_chart_group_get_is_selected:
 * @self: a #CcBarChartGroup
 *
 * Get the value of #CcBarChartGroup:is-selected.
 *
 * Returns: `TRUE` if the group is selected, `FALSE` otherwise
 */
gboolean
cc_bar_chart_group_get_is_selected (CcBarChartGroup *self)
{
  g_return_val_if_fail (CC_IS_BAR_CHART_GROUP (self), FALSE);

  return (self->selection_state == SELECTION_STATE_GROUP);
}

/**
 * cc_bar_chart_group_set_is_selected:
 * @self: a #CcBarChartGroup
 * @is_selected: `TRUE` if the group is selected, `FALSE` otherwise
 *
 * Set the value of #CcBarChartGroup:is-selected.
 */
void
cc_bar_chart_group_set_is_selected (CcBarChartGroup *self,
                                    gboolean         is_selected)
{
  g_return_if_fail (CC_IS_BAR_CHART_GROUP (self));

  /* If the group is not selectable, pass this through to the first bar. */
  if (!self->selectable)
    {
      if (self->bars->len > 0)
        cc_bar_chart_group_set_selected_index (self, is_selected, 0);
      return;
    }

  if ((self->selection_state == SELECTION_STATE_GROUP) == is_selected)
    return;

  /* Update state and flags. */
  set_or_unset_selection_state_flags (self, FALSE);
  self->selection_state = is_selected ? SELECTION_STATE_GROUP : SELECTION_STATE_NONE;
  set_or_unset_selection_state_flags (self, TRUE);

  /* Re-render */
  gtk_widget_queue_draw (GTK_WIDGET (self));

  g_object_freeze_notify (G_OBJECT (self));
  g_object_notify_by_pspec (G_OBJECT (self), props[PROP_IS_SELECTED]);
  g_object_notify_by_pspec (G_OBJECT (self), props[PROP_SELECTED_INDEX]);
  g_object_notify_by_pspec (G_OBJECT (self), props[PROP_SELECTED_INDEX_SET]);
  g_object_thaw_notify (G_OBJECT (self));
}

/**
 * cc_bar_chart_group_get_selected_index:
 * @self: a #CcBarChartGroup
 * @out_index: (out) (optional): return location for the selected index, or
 *   `NULL` to ignore
 *
 * Get the currently selected bar index.
 *
 * If no bar is currently selected, or if the group as a whole is selected
 * (see cc_bar_chart_group_get_is_selected()), @out_index will be set to `0` and
 * `FALSE` will be returned.
 *
 * Returns: `TRUE` if a bar is currently selected, `FALSE` otherwise
 */
gboolean
cc_bar_chart_group_get_selected_index (CcBarChartGroup *self,
                                       size_t          *out_index)
{
  g_return_val_if_fail (CC_IS_BAR_CHART_GROUP (self), FALSE);

  if (out_index != NULL)
    *out_index = (self->selection_state == SELECTION_STATE_BAR) ? self->selected_bar_index : 0;

  return (self->selection_state == SELECTION_STATE_BAR);
}

/**
 * cc_bar_chart_group_set_selected_index:
 * @self: a #CcBarChartGroup
 * @is_selected: `TRUE` if a bar should be selected, `FALSE` if everything
 *   should be unselected
 * @idx: index of the data to select, ignored if @is_selected is `FALSE`
 *
 * Set the currently selected bar index, or unselect everything.
 *
 * If @is_selected is `TRUE`, the bar at @idx will be selected. If @is_selected
 * is `FALSE`, @idx will be ignored and all bars (and the group itself) will be
 * unselected.
 */
void
cc_bar_chart_group_set_selected_index (CcBarChartGroup *self,
                                       gboolean         is_selected,
                                       size_t           idx)
{
  g_return_if_fail (CC_IS_BAR_CHART_GROUP (self));
  g_return_if_fail (!is_selected || idx < self->bars->len);

  if ((is_selected && self->selection_state == SELECTION_STATE_BAR && self->selected_bar_index == idx) ||
      (!is_selected && self->selection_state == SELECTION_STATE_NONE))
    return;

  /* Update state and flags. */
  set_or_unset_selection_state_flags (self, FALSE);

  self->selection_state = is_selected ? SELECTION_STATE_BAR : SELECTION_STATE_NONE;
  self->selected_bar_index = is_selected ? idx : 0;

  set_or_unset_selection_state_flags (self, TRUE);

  /* Re-render */
  gtk_widget_queue_draw (GTK_WIDGET (self));

  g_object_freeze_notify (G_OBJECT (self));
  g_object_notify_by_pspec (G_OBJECT (self), props[PROP_IS_SELECTED]);
  g_object_notify_by_pspec (G_OBJECT (self), props[PROP_SELECTED_INDEX]);
  g_object_notify_by_pspec (G_OBJECT (self), props[PROP_SELECTED_INDEX_SET]);
  g_object_thaw_notify (G_OBJECT (self));
}

/**
 * cc_bar_chart_group_get_scale:
 * @self: a #CcBarChartGroup
 *
 * Get the value of #CcBarChartGroup:scale.
 *
 * Returns: pixels per data unit to render the bars with
 */
double
cc_bar_chart_group_get_scale (CcBarChartGroup *self)
{
  g_return_val_if_fail (CC_IS_BAR_CHART_GROUP (self), NAN);

  return self->scale;
}

/**
 * cc_bar_chart_group_set_scale:
 * @self: a #CcBarChartGroup
 * @scale: pixels per data unit to render the bars with
 *
 * Set the value of #CcBarChartGroup:scale.
 */
void
cc_bar_chart_group_set_scale (CcBarChartGroup *self,
                              double           scale)
{
  g_return_if_fail (CC_IS_BAR_CHART_GROUP (self));
  g_return_if_fail (scale > 0.0);

  if (scale == self->scale)
    return;

  self->scale = scale;

  /* Re-render */
  gtk_widget_queue_allocate (GTK_WIDGET (self));

  g_object_notify_by_pspec (G_OBJECT (self), props[PROP_SCALE]);
}
