/* -*- mode: c; c-basic-offset: 2; indent-tabs-mode: nil; -*- */
/* cc-timelike-editor-layout.c
 *
 * Copyright 2025 GNOME Foundation, Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * Author(s):
 *   Philip Withnall <pwithnall@gnome.org>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include <gtk/gtk.h>

#include "cc-timelike-editor-layout.h"
#include "cc-timelike-entry.h"

/**
 * CcTimelikeEditorLayout:
 *
 * A #GtkLayoutManager for the child widgets inside #CcTimelikeEditor.
 *
 * Although #CcTimelikeEditor’s child widgets are seemingly layed out on a grid
 * pattern, a custom layout manager is needed to align the up/down buttons with
 * the relevant parts of the #CcTimelikeEntry’s text.
 *
 * This is done by calling cc_timelike_entry_get_hours_and_minutes_midpoints()
 * to get the X coordinates of the midpoints of the hours and minutes parts of
 * the entry’s text, and aligning the midpoint of the up/down buttons with
 * those.
 *
 * The rest of the widget layout is fairly basic, and allocates any additional
 * space to the #CcTimelikeEntry child widget.
 *
 * As this layout manager is specific to #CcTimelikeEditor, it is not very
 * generalised, and will probably need to be modified if #CcTimelikeEditor (or
 * its default CSS) is modified.
 *
 * In particular, the layout manager doesn’t support RTL (because
 * #CcTimelikeEditor doesn’t support that, because times are always LTR); and
 * the layout manager hard-codes the order of the child widgets. It expects 5
 * child buttons, in the order:
 *  - 0: hours up
 *  - 1: minutes up
 *  - 2: hours down
 *  - 3: minutes down
 *  - 4: AM/PM button (may be hidden)
 *
 * A more generalised implementation of the layout manager would add a type
 * derived from #GtkLayoutChild which allowed specifying which button is which.
 */

#define ROW_SPACING_DEFAULT 6
#define COLUMN_SPACING_DEFAULT 6

struct _CcTimelikeEditorLayout {
  GtkLayoutManager parent_instance;

  unsigned int row_spacing;
  unsigned int column_spacing;
};

G_DEFINE_TYPE (CcTimelikeEditorLayout, cc_timelike_editor_layout, GTK_TYPE_LAYOUT_MANAGER)

typedef enum {
  PROP_ROW_SPACING = 1,
  PROP_COLUMN_SPACING,
} CcTimelikeEditorLayoutProperty;

static GParamSpec *props[PROP_COLUMN_SPACING + 1];

static void
cc_timelike_editor_layout_get_property (GObject    *object,
                                        guint       property_id,
                                        GValue     *value,
                                        GParamSpec *pspec)
{
  CcTimelikeEditorLayout *self = CC_TIMELIKE_EDITOR_LAYOUT (object);

  switch ((CcTimelikeEditorLayoutProperty) property_id)
    {
    case PROP_ROW_SPACING:
      g_value_set_uint (value, cc_timelike_editor_layout_get_row_spacing (self));
      break;
    case PROP_COLUMN_SPACING:
      g_value_set_uint (value, cc_timelike_editor_layout_get_column_spacing (self));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
    }
}

static void
cc_timelike_editor_layout_set_property (GObject      *object,
                                        guint         property_id,
                                        const GValue *value,
                                        GParamSpec   *pspec)
{
  CcTimelikeEditorLayout *self = CC_TIMELIKE_EDITOR_LAYOUT (object);

  switch ((CcTimelikeEditorLayoutProperty) property_id)
    {
    case PROP_ROW_SPACING:
      cc_timelike_editor_layout_set_row_spacing (self, g_value_get_uint (value));
      break;
    case PROP_COLUMN_SPACING:
      cc_timelike_editor_layout_set_column_spacing (self, g_value_get_uint (value));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
    }
}

static void
cc_timelike_editor_layout_measure (GtkLayoutManager *layout_manager,
                                   GtkWidget        *widget,
                                   GtkOrientation    orientation,
                                   int               for_size,
                                   int              *out_minimum,
                                   int              *out_natural,
                                   int              *out_minimum_baseline,
                                   int              *out_natural_baseline)
{
  CcTimelikeEditorLayout *self = CC_TIMELIKE_EDITOR_LAYOUT (layout_manager);
  GtkWidget *child;
  int max_up_down_button_minimum = 0, max_up_down_button_natural = 0;
  int am_pm_button_minimum = 0, am_pm_button_natural = 0;
  int entry_minimum = 0, entry_natural = 0;
  unsigned int button_pos;

  for (child = gtk_widget_get_first_child (widget), button_pos = 0;
       child != NULL;
       child = gtk_widget_get_next_sibling (child))
    {
      int child_minimum = 0, child_natural = 0;

      if (!gtk_widget_should_layout (child))
        continue;

      gtk_widget_measure (child, orientation, for_size,
                          &child_minimum, &child_natural,
                          NULL, NULL);

      if (GTK_IS_BUTTON (child) && button_pos < 4)
        {
          /* Up/Down buttons */
          max_up_down_button_minimum = MAX (max_up_down_button_minimum, child_minimum);
          max_up_down_button_natural = MAX (max_up_down_button_natural, child_natural);
          button_pos++;
        }
      else if (GTK_IS_BUTTON (child) && button_pos == 4)
        {
          /* AM/PM button; may not be visible, in which case these will default to 0. */
          am_pm_button_minimum = child_minimum;
          am_pm_button_natural = child_natural;
          button_pos++;
        }
      else if (CC_IS_TIMELIKE_ENTRY (child))
        {
          entry_minimum = child_minimum;
          entry_natural = child_natural;
        }
      else
        {
          g_assert_not_reached ();
        }
    }

  if (orientation == GTK_ORIENTATION_HORIZONTAL)
    {
      *out_minimum = MAX (2 * max_up_down_button_minimum, entry_minimum) + ((am_pm_button_minimum > 0) ? am_pm_button_minimum + self->column_spacing : 0);
      *out_natural = MAX (2 * max_up_down_button_natural, entry_natural) + ((am_pm_button_natural > 0) ? am_pm_button_natural + self->column_spacing : 0);
    }
  else if (orientation == GTK_ORIENTATION_VERTICAL)
    {
      *out_minimum = 2 * max_up_down_button_minimum + 2 * self->row_spacing + MAX (entry_minimum, am_pm_button_minimum);
      *out_natural = 2 * max_up_down_button_natural + 2 * self->row_spacing + MAX (entry_natural, am_pm_button_natural);
    }
  else
    {
      g_assert_not_reached ();
    }

  *out_minimum_baseline = -1;
  *out_natural_baseline = -1;
}

static void
cc_timelike_editor_layout_allocate (GtkLayoutManager *layout_manager,
                                    GtkWidget        *widget,
                                    int               width,
                                    int               height,
                                    int               baseline)
{
  CcTimelikeEditorLayout *self = CC_TIMELIKE_EDITOR_LAYOUT (layout_manager);
  GtkWidget *child;
  CcTimelikeEntry *entry = NULL;
  int max_up_down_button_minimum = 0, max_up_down_button_natural = 0;
  int entry_width_minimum = 0, entry_width_natural = 0;
  int am_pm_button_width_minimum = 0, am_pm_button_width_natural = 0;
  graphene_point_t hours_midpoint = { 0, 0 }, minutes_midpoint = { 0, 0 };
  graphene_point_t hours_midpoint_self, minutes_midpoint_self;
  int entry_extra_width, entry_height;
  int up_down_button_size;
  int am_pm_button_width;
  unsigned int button_pos;

  /* Get the up/down button sizes. Take the largest from both dimensions of all
   * buttons, to make them equal and square. */
  for (child = gtk_widget_get_first_child (widget), button_pos = 0;
       child != NULL;
       child = gtk_widget_get_next_sibling (child))
    {
      GtkOrientation orientation;
      int child_minimum = 0, child_natural = 0;

      if (!gtk_widget_should_layout (child) ||
          !GTK_IS_BUTTON (child))
        continue;

      if (button_pos < 4)
        {
          for (orientation = GTK_ORIENTATION_HORIZONTAL;
               orientation <= GTK_ORIENTATION_VERTICAL;
               orientation++)
            {
              gtk_widget_measure (child, orientation, -1,
                                  &child_minimum, &child_natural,
                                  NULL, NULL);

              max_up_down_button_minimum = MAX (max_up_down_button_minimum, child_minimum);
              max_up_down_button_natural = MAX (max_up_down_button_natural, child_natural);
            }
        }
      else if (button_pos == 4)
        {
          gtk_widget_measure (child, GTK_ORIENTATION_HORIZONTAL, -1,
                              &am_pm_button_width_minimum, &am_pm_button_width_natural,
                              NULL, NULL);
        }
      else
        {
          g_assert_not_reached ();
        }

      button_pos++;
    }

  /* We don’t support these buttons growing; only the entry and AM/PM button can grow. */
  g_assert (max_up_down_button_minimum == max_up_down_button_natural);
  up_down_button_size = max_up_down_button_minimum;
  g_assert (up_down_button_size > 0);

  g_assert (am_pm_button_width_minimum == am_pm_button_width_natural);
  am_pm_button_width = am_pm_button_width_minimum;

  /* Allocate the entry first, so we can get the offsets from it for the buttons. */
  for (child = gtk_widget_get_first_child (widget);
       child != NULL;
       child = gtk_widget_get_next_sibling (child))
    {
      if (!gtk_widget_should_layout (child))
        continue;

      if (CC_IS_TIMELIKE_ENTRY (child))
        {
          GtkAllocation child_allocation;
          int child_minimum = 0, child_natural = 0;
          gboolean success;

          entry = CC_TIMELIKE_ENTRY (child);

          gtk_widget_measure (child, GTK_ORIENTATION_HORIZONTAL, -1,
                              &entry_width_minimum, &entry_width_natural,
                              NULL, NULL);
          gtk_widget_measure (child, GTK_ORIENTATION_VERTICAL, width,
                              &child_minimum, &child_natural,
                              NULL, NULL);

          child_allocation.width = width - ((am_pm_button_width > 0) ? self->column_spacing + am_pm_button_width : 0);
          child_allocation.height = CLAMP (height - 2 * (int) self->row_spacing - 2 * up_down_button_size, child_minimum, child_natural);
          child_allocation.x = 0;
          child_allocation.y = up_down_button_size + self->row_spacing;

          gtk_widget_size_allocate (child, &child_allocation, -1);

          cc_timelike_entry_get_hours_and_minutes_midpoints (entry,
                                                             &hours_midpoint.x,
                                                             &minutes_midpoint.x);

          success = gtk_widget_compute_point (child, widget,
                                              &hours_midpoint, &hours_midpoint_self);
          g_assert (success);

          success = gtk_widget_compute_point (child, widget,
                                              &minutes_midpoint, &minutes_midpoint_self);
          g_assert (success);

          entry_extra_width = (child_allocation.width >= entry_width_natural) ? child_allocation.width - entry_width_natural : 0;
          entry_height = child_allocation.height;

          break;
        }
    }

  g_assert (entry != NULL);

  /* Now allocate the up/down buttons. button_pos counts the button position:
   *  - 0: hours up
   *  - 1: minutes up
   *  - 2: hours down
   *  - 3: minutes down
   *  - 4: AM/PM button (may be hidden)
   */
  for (child = gtk_widget_get_first_child (widget), button_pos = 0;
       child != NULL;
       child = gtk_widget_get_next_sibling (child))
    {
      if (!gtk_widget_should_layout (child) ||
          !GTK_IS_BUTTON (child))
        continue;

      if (button_pos < 4)
        {
          GtkAllocation child_allocation;
          int child_minimum = 0, child_natural = 0;
          gboolean is_hours = (button_pos % 2 == 0);
          gboolean is_up = (button_pos < 2);

          gtk_widget_measure (child, GTK_ORIENTATION_VERTICAL, width,
                              &child_minimum, &child_natural,
                              NULL, NULL);

          child_allocation.width = up_down_button_size;
          child_allocation.height = up_down_button_size;
          child_allocation.x = entry_extra_width / 2 + (is_hours ? hours_midpoint_self.x : minutes_midpoint_self.x) - up_down_button_size / 2;
          child_allocation.y = is_up ? 0 : up_down_button_size + 2 * self->row_spacing + entry_height;

          gtk_widget_size_allocate (child, &child_allocation, -1);
        }
      else if (button_pos == 4)
        {
          GtkAllocation child_allocation;

          child_allocation.width = am_pm_button_width;
          child_allocation.height = entry_height;
          child_allocation.x = width - am_pm_button_width;
          child_allocation.y = up_down_button_size + self->row_spacing;

          gtk_widget_size_allocate (child, &child_allocation, -1);
        }
      else
        {
          g_assert_not_reached ();
        }

      button_pos++;
    }
}

static void
cc_timelike_editor_layout_class_init (CcTimelikeEditorLayoutClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkLayoutManagerClass *layout_manager_class = GTK_LAYOUT_MANAGER_CLASS (klass);

  object_class->get_property = cc_timelike_editor_layout_get_property;
  object_class->set_property = cc_timelike_editor_layout_set_property;

  layout_manager_class->measure = cc_timelike_editor_layout_measure;
  layout_manager_class->allocate = cc_timelike_editor_layout_allocate;

  /**
   * CcTimelikeEditorLayout:row-spacing:
   *
   * Spacing between the rows, in logical pixels.
   *
   * This is the spacing between the row of ‘up’ buttons and the timelike entry,
   * and the timelike entry and the row of ‘down’ buttons.
   */
  props[PROP_ROW_SPACING] =
    g_param_spec_uint ("row-spacing",
                       NULL, NULL,
                       0, G_MAXUINT, 6,
                       G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_EXPLICIT_NOTIFY);

  /**
   * CcTimelikeEditorLayout:column-spacing:
   *
   * Spacing between the columns, in logical pixels.
   *
   * This is the spacing between the timelike entry, and the AM/PM button (if
   * the latter is visible).
   */
  props[PROP_COLUMN_SPACING] =
    g_param_spec_uint ("column-spacing",
                       NULL, NULL,
                       0, G_MAXUINT, COLUMN_SPACING_DEFAULT,
                       G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_EXPLICIT_NOTIFY);

  g_object_class_install_properties (object_class, G_N_ELEMENTS (props), props);
}

static void
cc_timelike_editor_layout_init (CcTimelikeEditorLayout *self)
{
  /* Default spacing. */
  self->row_spacing = ROW_SPACING_DEFAULT;
  self->column_spacing = COLUMN_SPACING_DEFAULT;
}

/**
 * cc_timelike_editor_layout_new:
 *
 * Create a new #CcTimelikeEditorLayout.
 *
 * Returns: (transfer full): a new #CcTimelikeEditorLayout
 */
CcTimelikeEditorLayout *
cc_timelike_editor_layout_new (void)
{
  return g_object_new (CC_TYPE_TIMELIKE_EDITOR_LAYOUT, NULL);
}

/**
 * cc_timelike_editor_layout_get_row_spacing:
 * @self: a #CcTimelikeEditorLayout
 *
 * Get the value of #CcTimelikeEditorLayout:row-spacing.
 *
 * Returns: row spacing, in logical pixels
 */
unsigned int
cc_timelike_editor_layout_get_row_spacing (CcTimelikeEditorLayout *self)
{
  g_return_val_if_fail (CC_IS_TIMELIKE_EDITOR_LAYOUT (self), 0);

  return self->row_spacing;
}

/**
 * cc_timelike_editor_layout_set_row_spacing:
 * @self: a #CcTimelikeEditorLayout
 * @row_spacing: row spacing, in logical pixels
 *
 * Set the value of #CcTimelikeEditorLayout:row-spacing.
 */
void
cc_timelike_editor_layout_set_row_spacing (CcTimelikeEditorLayout *self,
                                           unsigned int            row_spacing)
{
  g_return_if_fail (CC_IS_TIMELIKE_EDITOR_LAYOUT (self));

  if (self->row_spacing == row_spacing)
    return;

  self->row_spacing = row_spacing;
  gtk_layout_manager_layout_changed (GTK_LAYOUT_MANAGER (self));
  g_object_notify_by_pspec (G_OBJECT (self), props[PROP_ROW_SPACING]);
}

/**
 * cc_timelike_editor_layout_get_column_spacing:
 * @self: a #CcTimelikeEditorLayout
 *
 * Get the value of #CcTimelikeEditorLayout:column-spacing.
 *
 * Returns: column spacing, in logical pixels
 */
unsigned int
cc_timelike_editor_layout_get_column_spacing (CcTimelikeEditorLayout *self)
{
  g_return_val_if_fail (CC_IS_TIMELIKE_EDITOR_LAYOUT (self), 0);

  return self->column_spacing;
}

/**
 * cc_timelike_editor_layout_set_column_spacing:
 * @self: a #CcTimelikeEditorLayout
 * @column_spacing: column spacing, in logical pixels
 *
 * Set the value of #CcTimelikeEditorLayout:column-spacing.
 */
void
cc_timelike_editor_layout_set_column_spacing (CcTimelikeEditorLayout *self,
                                              unsigned int            column_spacing)
{
  g_return_if_fail (CC_IS_TIMELIKE_EDITOR_LAYOUT (self));

  if (self->column_spacing == column_spacing)
    return;

  self->column_spacing = column_spacing;
  gtk_layout_manager_layout_changed (GTK_LAYOUT_MANAGER (self));
  g_object_notify_by_pspec (G_OBJECT (self), props[PROP_COLUMN_SPACING]);
}
