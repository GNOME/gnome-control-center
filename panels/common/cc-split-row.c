/* cc-split-row.c
 *
 * Copyright 2018 Purism SPC
 *           2021 Georges Basile Stavracas Neto <georges.stavracas@gmail.com>
 *           2023 Red Hat, Inc
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
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
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "cc-split-row.h"

#include "cc-mask-paintable.h"

/**
 * CcSplitRow:
 *
 * A [class@Gtk.ListBoxRow] containing two mutually-exclusive
 * options with an illustration.
 */

struct _CcSplitRow
{
  CcContentRow       parent;

  GtkBox            *box;

  GtkPicture        *default_option_picture;
  GtkPicture        *alternative_option_picture;

  CcMaskPaintable   *default_option_mask;
  CcMaskPaintable   *alternative_option_mask;

  GtkWidget         *alternative_option_box;
  GtkWidget         *default_option_box;

  GtkWidget         *default_checkbutton_image;
  GtkWidget         *alternative_checkbutton_image;

  gchar             *alternative_resource_path;
  gchar             *default_resource_path;

  gchar             *alternative_option_title;
  gchar             *alternative_option_subtitle;
  gchar             *default_option_title;
  gchar             *default_option_subtitle;

  gboolean           use_default;
  gboolean           compact;
};

G_DEFINE_FINAL_TYPE (CcSplitRow, cc_split_row, CC_TYPE_CONTENT_ROW);

enum
{
  PROP_0,
  PROP_USE_DEFAULT,
  PROP_COMPACT,
  PROP_ALTERNATIVE_ILLUSTRATION_RESOURCE,
  PROP_ALTERNATIVE_OPTION_TITLE,
  PROP_ALTERNATIVE_OPTION_SUBTITLE,
  PROP_DEFAULT_ILLUSTRATION_RESOURCE,
  PROP_DEFAULT_OPTION_TITLE,
  PROP_DEFAULT_OPTION_SUBTITLE,
  N_PROPS,
};

static GParamSpec *props[N_PROPS] = { NULL, };

/*
 * Private Methods
 */

static void
set_use_default (CcSplitRow *self,
                 gboolean    use_default)
{
  if (use_default)
    {
      gtk_widget_set_state_flags (self->default_checkbutton_image, GTK_STATE_FLAG_CHECKED, FALSE);
      gtk_widget_unset_state_flags (self->alternative_checkbutton_image, GTK_STATE_FLAG_CHECKED);
    }
  else
    {
      gtk_widget_set_state_flags (self->alternative_checkbutton_image, GTK_STATE_FLAG_CHECKED, FALSE);
      gtk_widget_unset_state_flags (self->default_checkbutton_image, GTK_STATE_FLAG_CHECKED);
    }

  gtk_accessible_update_state (GTK_ACCESSIBLE (self->default_option_box),
                               GTK_ACCESSIBLE_STATE_CHECKED, use_default,
                               -1);

  gtk_accessible_update_state (GTK_ACCESSIBLE (self->alternative_option_box),
                               GTK_ACCESSIBLE_STATE_CHECKED, !use_default,
                               -1);
}

static void
on_option_focus_leave_cb (CcMaskPaintable *mask)
{
  GtkMediaStream *stream;
  GdkPaintable *paintable;

  paintable = cc_mask_paintable_get_paintable (mask);

  if (!GTK_IS_MEDIA_STREAM (paintable))
    return;

  stream = GTK_MEDIA_STREAM (paintable);
  gtk_media_stream_set_loop (stream, FALSE);
  gtk_media_stream_pause (stream);
}

static void
on_option_focus_enter_cb (CcMaskPaintable *mask)
{
  GtkMediaStream *stream;
  GdkPaintable *paintable;

  paintable = cc_mask_paintable_get_paintable (mask);

  if (!GTK_IS_MEDIA_STREAM (paintable))
    return;

  stream = GTK_MEDIA_STREAM (paintable);
  gtk_media_stream_set_loop (stream, TRUE);
  gtk_media_stream_play (stream);
}

static void
on_option_released_cb (GtkWidget       *option_box,
                       gint             n_press,
                       gdouble          x,
                       gdouble          y,
                       GtkGestureClick *gesture)
{
  GtkWidget *widget = gtk_event_controller_get_widget (GTK_EVENT_CONTROLLER (gesture));
  CcSplitRow *self = CC_SPLIT_ROW (gtk_widget_get_ancestor (widget, CC_TYPE_SPLIT_ROW));

  g_assert (CC_IS_SPLIT_ROW (self));
  g_assert (GTK_IS_BOX (option_box));
  g_assert (GTK_IS_GESTURE_CLICK (gesture));
  g_assert (GTK_IS_BOX (widget));

  gtk_gesture_set_state (GTK_GESTURE (gesture), GTK_EVENT_SEQUENCE_CLAIMED);
  if (gtk_widget_contains (widget, x, y))
    {
      if (!gtk_widget_grab_focus (widget))
        g_assert_not_reached ();
      cc_split_row_set_use_default (self, self->default_option_box == option_box);
    }
}

/*
 * GObject Overrides
 */

static void
cc_split_row_dispose (GObject *object)
{
  CcSplitRow *self = CC_SPLIT_ROW (object);

  g_clear_pointer (&self->default_resource_path, g_free);
  g_clear_pointer (&self->alternative_resource_path, g_free);
  g_clear_pointer (&self->alternative_option_title, g_free);
  g_clear_pointer (&self->alternative_option_subtitle, g_free);
  g_clear_pointer (&self->default_option_title, g_free);
  g_clear_pointer (&self->default_option_subtitle, g_free);

  G_OBJECT_CLASS (cc_split_row_parent_class)->dispose (object);
}

static void
cc_split_row_get_property (GObject      *object,
                           guint         prop_id,
                           GValue       *value,
                           GParamSpec   *pspec)
{
  CcSplitRow *self = CC_SPLIT_ROW (object);

  switch (prop_id)
    {
    case PROP_USE_DEFAULT:
      g_value_set_boolean (value, cc_split_row_get_use_default (self));
      break;
    case PROP_COMPACT:
      g_value_set_boolean (value, cc_split_row_get_compact (self));
      break;
    case PROP_ALTERNATIVE_ILLUSTRATION_RESOURCE:
      g_value_set_string (value, cc_split_row_get_alternative_illustration_resource (self));
      break;
    case PROP_ALTERNATIVE_OPTION_TITLE:
      g_value_set_string (value, cc_split_row_get_alternative_option_title (self));
      break;
    case PROP_ALTERNATIVE_OPTION_SUBTITLE:
      g_value_set_string (value, cc_split_row_get_alternative_option_subtitle (self));
      break;
    case PROP_DEFAULT_ILLUSTRATION_RESOURCE:
      g_value_set_string (value, cc_split_row_get_default_illustration_resource (self));
      break;
    case PROP_DEFAULT_OPTION_TITLE:
      g_value_set_string (value, cc_split_row_get_default_option_title (self));
      break;
    case PROP_DEFAULT_OPTION_SUBTITLE:
      g_value_set_string (value, cc_split_row_get_default_option_subtitle (self));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
cc_split_row_set_property (GObject      *object,
                           guint         prop_id,
                           const GValue *value,
                           GParamSpec   *pspec)
{
  CcSplitRow *self = CC_SPLIT_ROW (object);

  switch (prop_id)
    {
    case PROP_USE_DEFAULT:
      cc_split_row_set_use_default (self, g_value_get_boolean (value));
      break;
    case PROP_COMPACT:
      cc_split_row_set_compact (self, g_value_get_boolean (value));
      break;
    case PROP_ALTERNATIVE_ILLUSTRATION_RESOURCE:
      cc_split_row_set_alternative_illustration_resource (self, g_value_get_string (value));
      break;
    case PROP_ALTERNATIVE_OPTION_TITLE:
      cc_split_row_set_alternative_option_title (self, g_value_get_string (value));
      break;
    case PROP_ALTERNATIVE_OPTION_SUBTITLE:
      cc_split_row_set_alternative_option_subtitle (self, g_value_get_string (value));
      break;
    case PROP_DEFAULT_ILLUSTRATION_RESOURCE:
      cc_split_row_set_default_illustration_resource (self, g_value_get_string (value));
      break;
    case PROP_DEFAULT_OPTION_TITLE:
      cc_split_row_set_default_option_title (self, g_value_get_string (value));
      break;
    case PROP_DEFAULT_OPTION_SUBTITLE:
      cc_split_row_set_default_option_subtitle (self, g_value_get_string (value));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

/*
 * GtkWidget Overrides
 */

static gboolean
cc_split_row_child_focus (GtkWidget        *widget,
                          GtkDirectionType  direction)
{
  CcSplitRow *self = CC_SPLIT_ROW (widget);
  GtkWidget *child_focus;
  gboolean is_tab, is_rtl, is_start, is_end;

  is_tab = direction == GTK_DIR_TAB_FORWARD || direction == GTK_DIR_TAB_BACKWARD;

  child_focus = gtk_widget_get_focus_child (widget);

  if (child_focus && is_tab)
    return FALSE;

  is_rtl = gtk_widget_get_direction (widget) == GTK_TEXT_DIR_RTL;
  is_start = (direction == GTK_DIR_LEFT && !is_rtl) || (direction == GTK_DIR_RIGHT && is_rtl);
  is_end = (direction == GTK_DIR_RIGHT && !is_rtl) || (direction == GTK_DIR_LEFT && is_rtl);

  if (is_start)
    {
      cc_split_row_set_use_default (self, TRUE);
      return gtk_widget_grab_focus (self->default_option_box);
    }
  else if (is_end)
    {
      cc_split_row_set_use_default (self, FALSE);
      return gtk_widget_grab_focus (self->alternative_option_box);
    }

  return GTK_WIDGET_CLASS (cc_split_row_parent_class)->focus (widget, direction);
}

static gboolean
cc_split_row_grab_focus (GtkWidget *widget)
{
  CcSplitRow *self = CC_SPLIT_ROW (widget);

  if (cc_split_row_get_use_default (self))
    return gtk_widget_grab_focus (self->default_option_box);
  else
    return gtk_widget_grab_focus (self->alternative_option_box);
}

/*
 * Initialization
 */

static void
cc_split_row_class_init (CcSplitRowClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->dispose = cc_split_row_dispose;
  object_class->get_property = cc_split_row_get_property;
  object_class->set_property = cc_split_row_set_property;

  widget_class->focus = cc_split_row_child_focus;
  widget_class->grab_focus = cc_split_row_grab_focus;

  /**
   * CcSplitRow:use-default:
   *
   * Whether the default option is checked.
   */
  props[PROP_USE_DEFAULT] =
    g_param_spec_boolean ("use-default", NULL, NULL,
                          TRUE,
                          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_EXPLICIT_NOTIFY);

  /**
   * CcSplitRow:compact:
   *
   * Whether @self is using a compact layout.
   */
  props[PROP_COMPACT] =
    g_param_spec_boolean ("compact", NULL, NULL,
                          FALSE,
                          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_EXPLICIT_NOTIFY);

  /**
   * CcSplitRow:alternative-illustration-resource:
   *
   * The resource path for the illustration of the alternative option.
   */
  props[PROP_ALTERNATIVE_ILLUSTRATION_RESOURCE] =
    g_param_spec_string ("alternative-illustration-resource", NULL, NULL,
                         "",
                         G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_EXPLICIT_NOTIFY);

  /**
   * CcSplitRow:alternative-option-title:
   *
   * The title for the alternative option of @self.
   */
  props[PROP_ALTERNATIVE_OPTION_TITLE] =
    g_param_spec_string ("alternative-option-title", NULL, NULL,
                         "",
                         G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_EXPLICIT_NOTIFY);

  /**
   * CcSplitRow:alternative-option-subtitle:
   *
   * The subtitle for the alternative option of @self.
   */
  props[PROP_ALTERNATIVE_OPTION_SUBTITLE] =
    g_param_spec_string ("alternative-option-subtitle", NULL, NULL,
                         "",
                         G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_EXPLICIT_NOTIFY);

  /**
   * CcSplitRow:default-illustration-resource:
   *
   * The resource path for the illustration of the default option.
   */
  props[PROP_DEFAULT_ILLUSTRATION_RESOURCE] =
    g_param_spec_string ("default-illustration-resource", NULL, NULL,
                         "",
                         G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_EXPLICIT_NOTIFY);

  /**
   * CcSplitRow:default-option-title:
   *
   * The title for the default option of @self.
   */
  props[PROP_DEFAULT_OPTION_TITLE] =
    g_param_spec_string ("default-option-title", NULL, NULL,
                         "",
                         G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_EXPLICIT_NOTIFY);

  /**
   * CcSplitRow:default-option-subtitle:
   *
   * The subtitle for the default option of @self.
   */
  props[PROP_DEFAULT_OPTION_SUBTITLE] =
    g_param_spec_string ("default-option-subtitle", NULL, NULL,
                         "",
                         G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_EXPLICIT_NOTIFY);

  g_object_class_install_properties (object_class, N_PROPS, props);

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/control-center/common/cc-split-row.ui");

  gtk_widget_class_bind_template_child (widget_class, CcSplitRow, box);
  gtk_widget_class_bind_template_child (widget_class, CcSplitRow, alternative_option_box);
  gtk_widget_class_bind_template_child (widget_class, CcSplitRow, default_option_box);
  gtk_widget_class_bind_template_child (widget_class, CcSplitRow, default_checkbutton_image);
  gtk_widget_class_bind_template_child (widget_class, CcSplitRow, alternative_checkbutton_image);
  gtk_widget_class_bind_template_child (widget_class, CcSplitRow, alternative_option_picture);
  gtk_widget_class_bind_template_child (widget_class, CcSplitRow, alternative_option_mask);
  gtk_widget_class_bind_template_child (widget_class, CcSplitRow, default_option_picture);
  gtk_widget_class_bind_template_child (widget_class, CcSplitRow, default_option_mask);

  gtk_widget_class_bind_template_callback (widget_class, on_option_focus_enter_cb);
  gtk_widget_class_bind_template_callback (widget_class, on_option_focus_leave_cb);
  gtk_widget_class_bind_template_callback (widget_class, on_option_released_cb);
}

static void
cc_split_row_init (CcSplitRow *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));
  gtk_widget_add_css_class (GTK_WIDGET (self), "illustrated");
}

/*
 * Public Methods
 */

/**
 * cc_split_row_get_default_illustration_resource:
 * @self: a #CcSplitRow
 *
 * Get the resource path for the illustration of the default option.
 *
 * Returns: the resource path for the illustration of the default option
 */
const gchar *
cc_split_row_get_default_illustration_resource (CcSplitRow *self)
{
  return self->default_resource_path;
}

/**
 * cc_split_row_set_default_illustration_resource:
 * @self: a #CcSplitRow
 * @resource_path: the resource path for the illustration of the default option
 *
 * Set the resource path for the illustration of the default option.
 */
void
cc_split_row_set_default_illustration_resource (CcSplitRow  *self,
                                                const gchar *resource_path)
{
  g_return_if_fail (CC_IS_SPLIT_ROW (self));

  g_set_str (&self->default_resource_path, resource_path);

  cc_mask_paintable_set_resource_scaled (self->default_option_mask, resource_path, GTK_WIDGET (self));

  gtk_widget_set_visible (GTK_WIDGET (self->default_option_picture),
                          resource_path != NULL && g_strcmp0 (resource_path, "") != 0);

  g_object_notify_by_pspec (G_OBJECT (self), props[PROP_DEFAULT_ILLUSTRATION_RESOURCE]);
}

/**
 * cc_split_row_get_alternative_illustration_resource:
 * @self: a #CcSplitRow
 *
 * Get the resource path for the illustration of the alternative option.
 *
 * Returns: the resource path for the illustration of the alternative option
 */
const gchar *
cc_split_row_get_alternative_illustration_resource (CcSplitRow *self)
{
  return self->alternative_resource_path;
}

/**
 * cc_split_row_set_alternative_illustration_resource:
 * @self: a #CcSplitRow
 * @resource_path: the resource path for the illustration of the alternative option
 *
 * Set the resource path for the illustration of the alternative option.
 */
void
cc_split_row_set_alternative_illustration_resource (CcSplitRow  *self,
                                                    const gchar *resource_path)
{
  g_return_if_fail (CC_IS_SPLIT_ROW (self));

  g_set_str (&self->alternative_resource_path, resource_path);

  cc_mask_paintable_set_resource_scaled (self->alternative_option_mask, resource_path, GTK_WIDGET (self));

  gtk_widget_set_visible (GTK_WIDGET (self->alternative_option_picture),
                          resource_path != NULL && g_strcmp0 (resource_path, "") != 0);

  g_object_notify_by_pspec (G_OBJECT (self), props[PROP_ALTERNATIVE_ILLUSTRATION_RESOURCE]);
}

/**
 * cc_split_row_get_use_default:
 * @self: a #CcSplitRow
 *
 * Get whether the default option is checked.
 *
 * Returns:
 *   %TRUE if the default option is checked,
 *   %FALSE if the alternative option is checked
 */
gboolean
cc_split_row_get_use_default (CcSplitRow *self)
{
  return gtk_widget_get_state_flags (self->default_checkbutton_image) & GTK_STATE_FLAG_CHECKED;
}

/**
 * cc_split_row_set_use_default:
 * @self: a #CcSplitRow
 * @use_default:
 *   %TRUE if the default option should be checked,
 *   %FALSE if the alternative option should be checked.
 *
 * Set whether the default option should be checked.
 */
void
cc_split_row_set_use_default (CcSplitRow *self,
                              gboolean    use_default)
{
  g_return_if_fail (CC_IS_SPLIT_ROW (self));

  set_use_default (self, use_default);

  g_object_notify_by_pspec (G_OBJECT (self), props[PROP_USE_DEFAULT]);
}

/**
 * cc_split_row_get_compact:
 * @self: a #CcSplitRow
 *
 * Get whether @self is using a compact layout.
 *
 * Returns: %TRUE if @self is using a compact layout
 */
gboolean
cc_split_row_get_compact (CcSplitRow *self)
{
  return self->compact;
}

/**
 * cc_split_row_set_compact:
 * @self: a #CcSplitRow
 * @compact: %TRUE if @self should use a compact layout
 *
 * Set whether @self should use a compact layout.
 */
void
cc_split_row_set_compact (CcSplitRow *self,
                          gboolean    compact)
{
  g_return_if_fail (CC_IS_SPLIT_ROW (self));

  self->compact = !!compact;

  gtk_orientable_set_orientation (GTK_ORIENTABLE (self->box),
                                  compact ? GTK_ORIENTATION_VERTICAL : GTK_ORIENTATION_HORIZONTAL);
  gtk_box_set_spacing (self->box, compact ? 6 : 18);

  g_object_notify_by_pspec (G_OBJECT (self), props[PROP_COMPACT]);
}

/**
 * cc_split_row_get_default_option_title:
 * @self: a #CcSplitRow
 *
 * Get the title for the default option of @self.
 *
 * Returns: the title for the default option
 */
const gchar *
cc_split_row_get_default_option_title (CcSplitRow *self)
{
  g_return_val_if_fail (CC_IS_SPLIT_ROW (self), NULL);

  return self->default_option_title;
}

/**
 * cc_split_row_set_default_option_title:
 * @self: a #CcSplitRow
 * @title: the new title for the default option
 *
 * Set the title for the default option of @self.
 */
void
cc_split_row_set_default_option_title (CcSplitRow  *self,
                                       const gchar *title)
{
  g_return_if_fail (CC_IS_SPLIT_ROW (self));

  if (g_set_str (&self->default_option_title, title))
    g_object_notify_by_pspec (G_OBJECT (self), props[PROP_DEFAULT_OPTION_TITLE]);
}

/**
 * cc_split_row_get_default_option_subtitle:
 * @self: a #CcSplitRow
 *
 * Get the subtitle for the default option of @self.
 *
 * Returns: the subtitle for the default option
 */
const gchar *
cc_split_row_get_default_option_subtitle (CcSplitRow *self)
{
  g_return_val_if_fail (CC_IS_SPLIT_ROW (self), NULL);

  return self->default_option_subtitle;
}

/**
 * cc_split_row_set_default_option_subtitle:
 * @self: a #CcSplitRow
 * @subtitle: the new subtitle for the default option
 *
 * Set the subtitle for the default option of @self.
 */
void
cc_split_row_set_default_option_subtitle (CcSplitRow  *self,
                                          const gchar *subtitle)
{
  g_return_if_fail (CC_IS_SPLIT_ROW (self));

  if (g_set_str (&self->default_option_subtitle, subtitle))
    g_object_notify_by_pspec (G_OBJECT (self), props[PROP_DEFAULT_OPTION_SUBTITLE]);
}

/**
 * cc_split_row_get_alternative_option_title:
 * @self: a #CcSplitRow
 *
 * Get the title for the alternative option of @self.
 *
 * Returns: the title for the alternative option
 */
const gchar *
cc_split_row_get_alternative_option_title (CcSplitRow *self)
{
  g_return_val_if_fail (CC_IS_SPLIT_ROW (self), NULL);

  return self->alternative_option_title;
}

/**
 * cc_split_row_set_alternative_option_title:
 * @self: a #CcSplitRow
 * @title: the new title for the alternative option
 *
 * Set the title for the alternative option of @self.
 */
void
cc_split_row_set_alternative_option_title (CcSplitRow  *self,
                                           const gchar *title)
{
  g_return_if_fail (CC_IS_SPLIT_ROW (self));

  if (g_set_str (&self->alternative_option_title, title))
    g_object_notify_by_pspec (G_OBJECT (self), props[PROP_ALTERNATIVE_OPTION_TITLE]);
}

/**
 * cc_split_row_get_alternative_option_subtitle:
 * @self: a #CcSplitRow
 *
 * Get the subtitle for the alternative option of @self.
 *
 * Returns: the subtitle for the alternative option
 */
const gchar *
cc_split_row_get_alternative_option_subtitle (CcSplitRow *self)
{
  g_return_val_if_fail (CC_IS_SPLIT_ROW (self), NULL);

  return self->alternative_option_subtitle;
}

/**
 * cc_split_row_set_alternative_option_subtitle:
 * @self: a #CcSplitRow
 * @subtitle: the new subtitle for the alternative option
 *
 * Set the subtitle for the alternative option of @self.
 */
void
cc_split_row_set_alternative_option_subtitle (CcSplitRow  *self,
                                              const gchar *subtitle)
{
  g_return_if_fail (CC_IS_SPLIT_ROW (self));

  if (g_set_str (&self->alternative_option_subtitle, subtitle))
    g_object_notify_by_pspec (G_OBJECT (self), props[PROP_ALTERNATIVE_OPTION_SUBTITLE]);
}

