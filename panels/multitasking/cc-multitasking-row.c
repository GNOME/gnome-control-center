/* cc-multitasking-row.c
 *
 * Copyright 2018 Purism SPC
 *           2021 Georges Basile Stavracas Neto <georges.stavracas@gmail.com>
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

#include "cc-multitasking-row.h"

struct _CcMultitaskingRow
{
  HdyPreferencesRow  parent;

  GtkBox            *artwork_box;
  GtkBox            *header;
  GtkImage          *image;
  GtkBox            *prefixes;
  GtkLabel          *subtitle;
  GtkBox            *suffixes;
  GtkLabel          *title;
  GtkBox            *title_box;

  GtkWidget         *previous_parent;

  gboolean           use_underline;
  gint               title_lines;
  gint               subtitle_lines;
  GtkWidget         *activatable_widget;
};

static void cc_multitasking_row_buildable_init (GtkBuildableIface *iface);

G_DEFINE_TYPE_WITH_CODE (CcMultitaskingRow, cc_multitasking_row, HDY_TYPE_PREFERENCES_ROW,
                         G_IMPLEMENT_INTERFACE (GTK_TYPE_BUILDABLE, cc_multitasking_row_buildable_init))

enum
{
  PROP_0,
  PROP_ICON_NAME,
  PROP_ACTIVATABLE_WIDGET,
  PROP_SUBTITLE,
  PROP_USE_UNDERLINE,
  PROP_TITLE_LINES,
  PROP_SUBTITLE_LINES,
  N_PROPS,
};

enum
{
  SIGNAL_ACTIVATED,
  SIGNAL_LAST_SIGNAL,
};

static GParamSpec *props[N_PROPS] = { NULL, };
static guint signals[SIGNAL_LAST_SIGNAL] = { 0, };

static void
row_activated_cb (CcMultitaskingRow *self,
                  GtkListBoxRow     *row)
{
  /* No need to use GTK_LIST_BOX_ROW() for a pointer comparison. */
  if ((GtkListBoxRow *) self == row)
    cc_multitasking_row_activate (self);
}

static void
parent_cb (CcMultitaskingRow *self)
{
  GtkWidget *parent = gtk_widget_get_parent (GTK_WIDGET (self));

  if (self->previous_parent != NULL)
    {
      g_signal_handlers_disconnect_by_func (self->previous_parent,
                                            G_CALLBACK (row_activated_cb),
                                            self);
      self->previous_parent = NULL;
    }

  if (parent == NULL || !GTK_IS_LIST_BOX (parent))
    return;

  self->previous_parent = parent;
  g_signal_connect_swapped (parent, "row-activated", G_CALLBACK (row_activated_cb), self);
}

static void
update_subtitle_visibility (CcMultitaskingRow *self)
{
  gtk_widget_set_visible (GTK_WIDGET (self->subtitle),
                          gtk_label_get_text (self->subtitle) != NULL &&
                          g_strcmp0 (gtk_label_get_text (self->subtitle), "") != 0);
}

static void
cc_multitasking_row_get_property (GObject    *object,
                                  guint       prop_id,
                                  GValue     *value,
                                  GParamSpec *pspec)
{
  CcMultitaskingRow *self = CC_MULTITASKING_ROW (object);

  switch (prop_id)
    {
    case PROP_ICON_NAME:
      g_value_set_string (value, cc_multitasking_row_get_icon_name (self));
      break;
    case PROP_ACTIVATABLE_WIDGET:
      g_value_set_object (value, (GObject *) cc_multitasking_row_get_activatable_widget (self));
      break;
    case PROP_SUBTITLE:
      g_value_set_string (value, cc_multitasking_row_get_subtitle (self));
      break;
    case PROP_SUBTITLE_LINES:
      g_value_set_int (value, cc_multitasking_row_get_subtitle_lines (self));
      break;
    case PROP_TITLE_LINES:
      g_value_set_int (value, cc_multitasking_row_get_title_lines (self));
      break;
    case PROP_USE_UNDERLINE:
      g_value_set_boolean (value, cc_multitasking_row_get_use_underline (self));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
cc_multitasking_row_set_property (GObject      *object,
                                  guint         prop_id,
                                  const GValue *value,
                                  GParamSpec   *pspec)
{
  CcMultitaskingRow *self = CC_MULTITASKING_ROW (object);

  switch (prop_id)
    {
    case PROP_ICON_NAME:
      cc_multitasking_row_set_icon_name (self, g_value_get_string (value));
      break;
    case PROP_ACTIVATABLE_WIDGET:
      cc_multitasking_row_set_activatable_widget (self, (GtkWidget*) g_value_get_object (value));
      break;
    case PROP_SUBTITLE:
      cc_multitasking_row_set_subtitle (self, g_value_get_string (value));
      break;
    case PROP_SUBTITLE_LINES:
      cc_multitasking_row_set_subtitle_lines (self, g_value_get_int (value));
      break;
    case PROP_TITLE_LINES:
      cc_multitasking_row_set_title_lines (self, g_value_get_int (value));
      break;
    case PROP_USE_UNDERLINE:
      cc_multitasking_row_set_use_underline (self, g_value_get_boolean (value));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
cc_multitasking_row_dispose (GObject *object)
{
  CcMultitaskingRow *self = CC_MULTITASKING_ROW (object);

  if (self->previous_parent != NULL) {
    g_signal_handlers_disconnect_by_func (self->previous_parent, G_CALLBACK (row_activated_cb), self);
    self->previous_parent = NULL;
  }

  G_OBJECT_CLASS (cc_multitasking_row_parent_class)->dispose (object);
}

static void
cc_multitasking_row_show_all (GtkWidget *widget)
{
  CcMultitaskingRow *self = CC_MULTITASKING_ROW (widget);

  g_return_if_fail (CC_IS_MULTITASKING_ROW (self));

  gtk_container_foreach (GTK_CONTAINER (self->prefixes),
                         (GtkCallback) gtk_widget_show_all,
                         NULL);

  gtk_container_foreach (GTK_CONTAINER (self->suffixes),
                         (GtkCallback) gtk_widget_show_all,
                         NULL);

  GTK_WIDGET_CLASS (cc_multitasking_row_parent_class)->show_all (widget);
}

static void
cc_multitasking_row_destroy (GtkWidget *widget)
{
  CcMultitaskingRow *self = CC_MULTITASKING_ROW (widget);

  if (self->header)
    {
      gtk_widget_destroy (GTK_WIDGET (self->header));
      self->header = NULL;
    }

  cc_multitasking_row_set_activatable_widget (self, NULL);

  self->prefixes = NULL;
  self->suffixes = NULL;

  GTK_WIDGET_CLASS (cc_multitasking_row_parent_class)->destroy (widget);
}

static void
cc_multitasking_row_add (GtkContainer *container,
                    GtkWidget    *child)
{
  CcMultitaskingRow *self = CC_MULTITASKING_ROW (container);

  /* When constructing the widget, we want the box to be added as the child of
   * the GtkListBoxRow, as an implementation detail.
   */
  if (!self->header)
    {
      GTK_CONTAINER_CLASS (cc_multitasking_row_parent_class)->add (container, child);
    }
  else
    {
      gtk_container_add (GTK_CONTAINER (self->suffixes), child);
      gtk_widget_show (GTK_WIDGET (self->suffixes));
    }
}

static void
cc_multitasking_row_remove (GtkContainer *container,
                       GtkWidget    *child)
{
  CcMultitaskingRow *self = CC_MULTITASKING_ROW (container);

  if (child == GTK_WIDGET (self->header))
    GTK_CONTAINER_CLASS (cc_multitasking_row_parent_class)->remove (container, child);
  else if (gtk_widget_get_parent (child) == GTK_WIDGET (self->prefixes))
    gtk_container_remove (GTK_CONTAINER (self->prefixes), child);
  else
    gtk_container_remove (GTK_CONTAINER (self->suffixes), child);
}

typedef struct {
  CcMultitaskingRow *row;
  GtkCallback callback;
  gpointer callback_data;
} ForallData;

static void
for_non_internal_child (GtkWidget *widget,
                        gpointer   callback_data)
{
  ForallData *data = callback_data;
  CcMultitaskingRow *self = data->row;

  if (widget != (GtkWidget *) self->image &&
      widget != (GtkWidget *) self->prefixes &&
      widget != (GtkWidget *) self->suffixes &&
      widget != (GtkWidget *) self->title_box)
    {
      data->callback (widget, data->callback_data);
    }
}

static void
cc_multitasking_row_forall (GtkContainer *container,
                       gboolean      include_internals,
                       GtkCallback   callback,
                       gpointer      callback_data)
{
  CcMultitaskingRow *self = CC_MULTITASKING_ROW (container);
  ForallData data;

  if (include_internals)
    {
      GTK_CONTAINER_CLASS (cc_multitasking_row_parent_class)->forall (GTK_CONTAINER (self),
                                                                      include_internals,
                                                                      callback,
                                                                      callback_data);
      return;
    }

  data.row = self;
  data.callback = callback;
  data.callback_data = callback_data;

  if (self->prefixes)
    {
      GTK_CONTAINER_GET_CLASS (self->prefixes)->forall (GTK_CONTAINER (self->prefixes),
                                                        include_internals,
                                                        for_non_internal_child,
                                                        &data);
    }
  if (self->suffixes)
    {
      GTK_CONTAINER_GET_CLASS (self->suffixes)->forall (GTK_CONTAINER (self->suffixes),
                                                        include_internals,
                                                        for_non_internal_child,
                                                        &data);
    }
  if (self->header)
    {
      GTK_CONTAINER_GET_CLASS (self->header)->forall (GTK_CONTAINER (self->header),
                                                      include_internals,
                                                      for_non_internal_child,
                                                      &data);
    }
}

static void
cc_multitasking_row_class_init (CcMultitaskingRowClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);
  GtkContainerClass *container_class = GTK_CONTAINER_CLASS (klass);

  object_class->get_property = cc_multitasking_row_get_property;
  object_class->set_property = cc_multitasking_row_set_property;
  object_class->dispose = cc_multitasking_row_dispose;

  widget_class->destroy = cc_multitasking_row_destroy;
  widget_class->show_all = cc_multitasking_row_show_all;

  container_class->add = cc_multitasking_row_add;
  container_class->remove = cc_multitasking_row_remove;
  container_class->forall = cc_multitasking_row_forall;

  props[PROP_ICON_NAME] =
    g_param_spec_string ("icon-name",
                         "Icon name",
                         "Icon name",
                         "",
                         G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_EXPLICIT_NOTIFY);

  props[PROP_ACTIVATABLE_WIDGET] =
      g_param_spec_object ("activatable-widget",
                           "Activatable widget",
                           "The widget to be activated when the row is activated",
                           GTK_TYPE_WIDGET,
                           G_PARAM_READWRITE);

  props[PROP_SUBTITLE] =
    g_param_spec_string ("subtitle",
                         "Subtitle",
                         "Subtitle",
                         "",
                         G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_EXPLICIT_NOTIFY);

  props[PROP_USE_UNDERLINE] =
    g_param_spec_boolean ("use-underline",
                          "Use underline",
                          "If set, an underline in the text indicates the next character should be used for the mnemonic accelerator key",
                          FALSE,
                          G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY);

  props[PROP_TITLE_LINES] =
    g_param_spec_int ("title-lines",
                      "Number of title lines",
                      "The desired number of title lines",
                      0, G_MAXINT,
                      1,
                      G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY);

  props[PROP_SUBTITLE_LINES] =
    g_param_spec_int ("subtitle-lines",
                      "Number of subtitle lines",
                      "The desired number of subtitle lines",
                      0, G_MAXINT,
                      1,
                      G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY);

  g_object_class_install_properties (object_class, N_PROPS, props);

  signals[SIGNAL_ACTIVATED] =
    g_signal_new ("activated",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  0,
                  NULL, NULL, NULL,
                  G_TYPE_NONE,
                  0);

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/control-center/multitasking/cc-multitasking-row.ui");

  gtk_widget_class_bind_template_child (widget_class, CcMultitaskingRow, artwork_box);
  gtk_widget_class_bind_template_child (widget_class, CcMultitaskingRow, header);
  gtk_widget_class_bind_template_child (widget_class, CcMultitaskingRow, image);
  gtk_widget_class_bind_template_child (widget_class, CcMultitaskingRow, prefixes);
  gtk_widget_class_bind_template_child (widget_class, CcMultitaskingRow, subtitle);
  gtk_widget_class_bind_template_child (widget_class, CcMultitaskingRow, suffixes);
  gtk_widget_class_bind_template_child (widget_class, CcMultitaskingRow, title);
  gtk_widget_class_bind_template_child (widget_class, CcMultitaskingRow, title_box);
}

static gboolean
string_is_not_empty (GBinding     *binding,
                     const GValue *from_value,
                     GValue       *to_value,
                     gpointer      user_data)
{
  const gchar *string = g_value_get_string (from_value);

  g_value_set_boolean (to_value, string != NULL && g_strcmp0 (string, "") != 0);

  return TRUE;
}

static void
cc_multitasking_row_init (CcMultitaskingRow *self)
{
  self->title_lines = 1;
  self->subtitle_lines = 1;

  gtk_widget_init_template (GTK_WIDGET (self));

  g_object_bind_property_full (self, "title",
                               self->title, "visible",
                               G_BINDING_SYNC_CREATE,
                               string_is_not_empty,
                               NULL, NULL, NULL);

  update_subtitle_visibility (self);

  g_signal_connect (self, "notify::parent", G_CALLBACK (parent_cb), NULL);
}

static void
cc_multitasking_row_buildable_add_child (GtkBuildable *buildable,
                                         GtkBuilder   *builder,
                                         GObject      *child,
                                         const gchar  *type)
{
  CcMultitaskingRow *self = CC_MULTITASKING_ROW (buildable);

  if (self->header == NULL || !type)
    gtk_container_add (GTK_CONTAINER (self), GTK_WIDGET (child));
  else if (type && strcmp (type, "prefix") == 0)
    cc_multitasking_row_add_prefix (self, GTK_WIDGET (child));
  else if (type && strcmp (type, "artwork") == 0)
    cc_multitasking_row_add_artwork(self, GTK_WIDGET (child));
  else
    GTK_BUILDER_WARN_INVALID_CHILD_TYPE (self, type);
}

static void
cc_multitasking_row_buildable_init (GtkBuildableIface *iface)
{
  iface->add_child = cc_multitasking_row_buildable_add_child;
}

const gchar *
cc_multitasking_row_get_subtitle (CcMultitaskingRow *self)
{
  g_return_val_if_fail (CC_IS_MULTITASKING_ROW (self), NULL);

  return gtk_label_get_text (self->subtitle);
}

void
cc_multitasking_row_set_subtitle (CcMultitaskingRow *self,
                             const gchar  *subtitle)
{
  g_return_if_fail (CC_IS_MULTITASKING_ROW (self));

  if (g_strcmp0 (gtk_label_get_text (self->subtitle), subtitle) == 0)
    return;

  gtk_label_set_text (self->subtitle, subtitle);
  gtk_widget_set_visible (GTK_WIDGET (self->subtitle),
                          subtitle != NULL && g_strcmp0 (subtitle, "") != 0);

  g_object_notify_by_pspec (G_OBJECT (self), props[PROP_SUBTITLE]);
}

const gchar *
cc_multitasking_row_get_icon_name (CcMultitaskingRow *self)
{
  const gchar *icon_name;

  g_return_val_if_fail (CC_IS_MULTITASKING_ROW (self), NULL);

  gtk_image_get_icon_name (self->image, &icon_name, NULL);

  return icon_name;
}

void
cc_multitasking_row_set_icon_name (CcMultitaskingRow *self,
                                   const gchar       *icon_name)
{
  const gchar *old_icon_name;

  g_return_if_fail (CC_IS_MULTITASKING_ROW (self));

  gtk_image_get_icon_name (self->image, &old_icon_name, NULL);
  if (g_strcmp0 (old_icon_name, icon_name) == 0)
    return;

  gtk_image_set_from_icon_name (self->image, icon_name, GTK_ICON_SIZE_INVALID);
  gtk_widget_set_visible (GTK_WIDGET (self->image),
                          icon_name != NULL && g_strcmp0 (icon_name, "") != 0);

  g_object_notify_by_pspec (G_OBJECT (self), props[PROP_ICON_NAME]);
}

GtkWidget *
cc_multitasking_row_get_activatable_widget (CcMultitaskingRow *self)
{
  g_return_val_if_fail (CC_IS_MULTITASKING_ROW (self), NULL);

  return self->activatable_widget;
}

static void
activatable_widget_weak_notify (gpointer  data,
                                GObject  *where_the_object_was)
{
  CcMultitaskingRow *self = CC_MULTITASKING_ROW (data);

  self->activatable_widget = NULL;

  g_object_notify_by_pspec (G_OBJECT (self), props[PROP_ACTIVATABLE_WIDGET]);
}

void
cc_multitasking_row_set_activatable_widget (CcMultitaskingRow *self,
                                            GtkWidget         *widget)
{
  g_return_if_fail (CC_IS_MULTITASKING_ROW (self));
  g_return_if_fail (widget == NULL || GTK_IS_WIDGET (widget));

  if (self->activatable_widget == widget)
    return;

  if (self->activatable_widget)
    g_object_weak_unref (G_OBJECT (self->activatable_widget),
                         activatable_widget_weak_notify,
                         self);

  self->activatable_widget = widget;

  if (self->activatable_widget != NULL) {
    g_object_weak_ref (G_OBJECT (self->activatable_widget),
                       activatable_widget_weak_notify,
                       self);
    gtk_list_box_row_set_activatable (GTK_LIST_BOX_ROW (self), TRUE);
  }

  g_object_notify_by_pspec (G_OBJECT (self), props[PROP_ACTIVATABLE_WIDGET]);
}

gboolean
cc_multitasking_row_get_use_underline (CcMultitaskingRow *self)
{
  g_return_val_if_fail (CC_IS_MULTITASKING_ROW (self), FALSE);

  return self->use_underline;
}

void
cc_multitasking_row_set_use_underline (CcMultitaskingRow *self,
                                       gboolean           use_underline)
{
  g_return_if_fail (CC_IS_MULTITASKING_ROW (self));

  use_underline = !!use_underline;

  if (self->use_underline == use_underline)
    return;

  self->use_underline = use_underline;
  hdy_preferences_row_set_use_underline (HDY_PREFERENCES_ROW (self), self->use_underline);
  gtk_label_set_use_underline (self->title, self->use_underline);
  gtk_label_set_use_underline (self->subtitle, self->use_underline);
  gtk_label_set_mnemonic_widget (self->title, GTK_WIDGET (self));
  gtk_label_set_mnemonic_widget (self->subtitle, GTK_WIDGET (self));

  g_object_notify_by_pspec (G_OBJECT (self), props[PROP_USE_UNDERLINE]);
}

gint
cc_multitasking_row_get_title_lines (CcMultitaskingRow *self)
{
  g_return_val_if_fail (CC_IS_MULTITASKING_ROW (self), 0);

  return self->title_lines;
}

void
cc_multitasking_row_set_title_lines (CcMultitaskingRow *self,
                                     gint               title_lines)
{
  g_return_if_fail (CC_IS_MULTITASKING_ROW (self));
  g_return_if_fail (title_lines >= 0);

  if (self->title_lines == title_lines)
    return;

  self->title_lines = title_lines;

  gtk_label_set_lines (self->title, title_lines);
  gtk_label_set_ellipsize (self->title, title_lines == 0 ? PANGO_ELLIPSIZE_NONE : PANGO_ELLIPSIZE_END);

  g_object_notify_by_pspec (G_OBJECT (self), props[PROP_TITLE_LINES]);
}

gint
cc_multitasking_row_get_subtitle_lines (CcMultitaskingRow *self)
{
  g_return_val_if_fail (CC_IS_MULTITASKING_ROW (self), 0);

  return self->subtitle_lines;
}

void
cc_multitasking_row_set_subtitle_lines (CcMultitaskingRow *self,
                                        gint               subtitle_lines)
{
  g_return_if_fail (CC_IS_MULTITASKING_ROW (self));
  g_return_if_fail (subtitle_lines >= 0);

  if (self->subtitle_lines == subtitle_lines)
    return;

  self->subtitle_lines = subtitle_lines;

  gtk_label_set_lines (self->subtitle, subtitle_lines);
  gtk_label_set_ellipsize (self->subtitle, subtitle_lines == 0 ? PANGO_ELLIPSIZE_NONE : PANGO_ELLIPSIZE_END);

  g_object_notify_by_pspec (G_OBJECT (self), props[PROP_SUBTITLE_LINES]);
}

void
cc_multitasking_row_add_prefix (CcMultitaskingRow *self,
                                GtkWidget         *widget)
{
  g_return_if_fail (CC_IS_MULTITASKING_ROW (self));
  g_return_if_fail (GTK_IS_WIDGET (self));

  gtk_box_pack_start (self->prefixes, widget, FALSE, TRUE, 0);
  gtk_widget_show (GTK_WIDGET (self->prefixes));
}

void
cc_multitasking_row_add_artwork (CcMultitaskingRow *self,
                                 GtkWidget         *widget)
{
  g_return_if_fail (CC_IS_MULTITASKING_ROW (self));
  g_return_if_fail (GTK_IS_WIDGET (self));

  /* HACK: the artwork box pushes the title too much to the top, so we
   * need to compensate this here.
   */
  gtk_widget_set_margin_top (GTK_WIDGET (self->header), 12);

  gtk_box_pack_start (self->artwork_box, widget, FALSE, TRUE, 0);
  gtk_widget_show (GTK_WIDGET (self->artwork_box));
}

void
cc_multitasking_row_activate (CcMultitaskingRow *self)
{
  g_return_if_fail (CC_IS_MULTITASKING_ROW (self));

  if (self->activatable_widget)
    gtk_widget_mnemonic_activate (self->activatable_widget, FALSE);

  g_signal_emit (self, signals[SIGNAL_ACTIVATED], 0);
}
