/* cc-content-row.c
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

#include "cc-content-row.h"

/**
 * CcContentRow:
 *
 * A [class@Gtk.ListBoxRow] used to present actions.
 *
 * <picture>
 *   <source srcset="action-row-dark.png" media="(prefers-color-scheme: dark)">
 *   <img src="action-row.png" alt="action-row">
 * </picture>
 *
 * The `CcContentRow` widget can have a title, a subtitle and an icon. The row
 * can receive additional widgets at its end, or prefix widgets at its start, or
 * content widgets below.
 *
 * It is convenient to present a preference and its related actions.
 *
 * `CcContentRow` is unactivatable by default, giving it an activatable widget
 * will automatically make it activatable, but unsetting it won't change the
 * row's activatability.
 *
 * ## CcContentRow as GtkBuildable
 *
 * The `CcContentRow` implementation of the [iface@Gtk.Buildable] interface
 * supports adding a child at its end by specifying “suffix” or omitting the
 * “type” attribute of a <child> element.
 *
 * It also supports adding a child as a prefix widget by specifying “prefix” as
 * the “type” attribute of a <child> element.
 *
 * It also supports adding a child as a content widget by specifying “content” as
 * the “type” attribute of a <child> element.
 *
 * ## CSS nodes
 *
 * `CcContentRow` has a main CSS node with name `row`.
 *
 * It contains the subnode `box.header` for its main horizontal box, and
 * `box.title` for the vertical box containing the title and subtitle labels.
 *
 * It contains subnodes `label.title` and `label.subtitle` representing
 * respectively the title label and subtitle label.
 */

typedef struct
{
  GtkBox    *contents;
  GtkWidget *header;
  GtkBox    *prefixes;
  GtkLabel  *subtitle;
  GtkBox    *suffixes;
  GtkLabel  *title;
  GtkBox    *title_box;

  GtkWidget *previous_parent;

  int title_lines;
  int subtitle_lines;

  gboolean subtitle_selectable;

  GtkWidget *activatable_widget;
  GBinding  *activatable_binding;
} CcContentRowPrivate;

static void cc_content_row_buildable_init (GtkBuildableIface *iface);

G_DEFINE_TYPE_WITH_CODE (CcContentRow, cc_content_row, ADW_TYPE_PREFERENCES_ROW,
                         G_ADD_PRIVATE (CcContentRow)
                         G_IMPLEMENT_INTERFACE (GTK_TYPE_BUILDABLE,
                         cc_content_row_buildable_init))

static GtkBuildableIface *parent_buildable_iface;

enum {
  PROP_0,
  PROP_SUBTITLE,
  PROP_ACTIVATABLE_WIDGET,
  PROP_TITLE_LINES,
  PROP_SUBTITLE_LINES,
  PROP_SUBTITLE_SELECTABLE,
  LAST_PROP,
};

static GParamSpec *props[LAST_PROP];

enum {
  SIGNAL_ACTIVATED,
  SIGNAL_LAST_SIGNAL,
};

static guint signals[SIGNAL_LAST_SIGNAL];

static gboolean
string_is_not_empty (CcContentRow *self,
                     const char   *string)
{
  return string && string[0];
}

static void
row_activated_cb (CcContentRow  *self,
                  GtkListBoxRow *row)
{
  /* No need to use GTK_LIST_BOX_ROW() for a pointer comparison. */
  if ((GtkListBoxRow *) self == row)
    cc_content_row_activate (self);
}

static void
parent_cb (CcContentRow *self)
{
  CcContentRowPrivate *priv = cc_content_row_get_instance_private (self);
  GtkWidget *parent = gtk_widget_get_parent (GTK_WIDGET (self));

  if (priv->previous_parent != NULL) {
    g_signal_handlers_disconnect_by_func (priv->previous_parent, G_CALLBACK (row_activated_cb), self);
    priv->previous_parent = NULL;
  }

  if (parent == NULL || !GTK_IS_LIST_BOX (parent))
    return;

  priv->previous_parent = parent;
  g_signal_connect_swapped (parent, "row-activated", G_CALLBACK (row_activated_cb), self);
}

static void
cc_content_row_get_property (GObject    *object,
                             guint       prop_id,
                             GValue     *value,
                             GParamSpec *pspec)
{
  CcContentRow *self = CC_CONTENT_ROW (object);

  switch (prop_id) {
  case PROP_SUBTITLE:
    g_value_set_string (value, cc_content_row_get_subtitle (self));
    break;
  case PROP_ACTIVATABLE_WIDGET:
    g_value_set_object (value, (GObject *) cc_content_row_get_activatable_widget (self));
    break;
  case PROP_SUBTITLE_LINES:
    g_value_set_int (value, cc_content_row_get_subtitle_lines (self));
    break;
  case PROP_TITLE_LINES:
    g_value_set_int (value, cc_content_row_get_title_lines (self));
    break;
  case PROP_SUBTITLE_SELECTABLE:
    g_value_set_boolean (value, cc_content_row_get_subtitle_selectable (self));
    break;
  default:
    G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
  }
}

static void
cc_content_row_set_property (GObject      *object,
                             guint         prop_id,
                             const GValue *value,
                             GParamSpec   *pspec)
{
  CcContentRow *self = CC_CONTENT_ROW (object);

  switch (prop_id) {
  case PROP_SUBTITLE:
    cc_content_row_set_subtitle (self, g_value_get_string (value));
    break;
  case PROP_ACTIVATABLE_WIDGET:
    cc_content_row_set_activatable_widget (self, (GtkWidget*) g_value_get_object (value));
    break;
  case PROP_SUBTITLE_LINES:
    cc_content_row_set_subtitle_lines (self, g_value_get_int (value));
    break;
  case PROP_TITLE_LINES:
    cc_content_row_set_title_lines (self, g_value_get_int (value));
    break;
  case PROP_SUBTITLE_SELECTABLE:
    cc_content_row_set_subtitle_selectable (self, g_value_get_boolean (value));
    break;
  default:
    G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
  }
}

static void
cc_content_row_dispose (GObject *object)
{
  CcContentRow *self = CC_CONTENT_ROW (object);
  CcContentRowPrivate *priv = cc_content_row_get_instance_private (self);

  if (priv->previous_parent != NULL) {
    g_signal_handlers_disconnect_by_func (priv->previous_parent, G_CALLBACK (row_activated_cb), self);
    priv->previous_parent = NULL;
  }

  cc_content_row_set_activatable_widget (self, NULL);

  G_OBJECT_CLASS (cc_content_row_parent_class)->dispose (object);
}

static void
cc_content_row_activate_real (CcContentRow *self)
{
  CcContentRowPrivate *priv = cc_content_row_get_instance_private (self);

  if (priv->activatable_widget)
    gtk_widget_mnemonic_activate (priv->activatable_widget, FALSE);

  g_signal_emit (self, signals[SIGNAL_ACTIVATED], 0);
}

static void
cc_content_row_class_init (CcContentRowClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->get_property = cc_content_row_get_property;
  object_class->set_property = cc_content_row_set_property;
  object_class->dispose = cc_content_row_dispose;

  klass->activate = cc_content_row_activate_real;

  /**
   * CcContentRow:subtitle:
   *
   * The subtitle for this row.
   *
   * The subtitle is interpreted as Pango markup unless
   * [property@PreferencesRow:use-markup] is set to `FALSE`.
   */
  props[PROP_SUBTITLE] =
    g_param_spec_string ("subtitle", NULL, NULL,
                         "",
                         G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_EXPLICIT_NOTIFY);

  /**
   * CcContentRow:activatable-widget:
   *
   * The widget to activate when the row is activated.
   *
   * The row can be activated either by clicking on it, calling
   * [method@ContentRow.activate], or via mnemonics in the title.
   * See the [property@PreferencesRow:use-underline] property to enable
   * mnemonics.
   *
   * The target widget will be activated by emitting the
   * [signal@Gtk.Widget::mnemonic-activate] signal on it.
   */
  props[PROP_ACTIVATABLE_WIDGET] =
    g_param_spec_object ("activatable-widget", NULL, NULL,
                         GTK_TYPE_WIDGET,
                         G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_EXPLICIT_NOTIFY);

  /**
   * CcContentRow:title-lines:
   *
   * The number of lines at the end of which the title label will be ellipsized.
   *
   * If the value is 0, the number of lines won't be limited.
   */
  props[PROP_TITLE_LINES] =
    g_param_spec_int ("title-lines", NULL, NULL,
                      0, G_MAXINT,
                      0,
                      G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_EXPLICIT_NOTIFY);

  /**
   * CcContentRow:subtitle-lines:
   *
   * The number of lines at the end of which the subtitle label will be
   * ellipsized.
   *
   * If the value is 0, the number of lines won't be limited.
   */
  props[PROP_SUBTITLE_LINES] =
    g_param_spec_int ("subtitle-lines", NULL, NULL,
                      0, G_MAXINT,
                      0,
                      G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_EXPLICIT_NOTIFY);

  /**
   * CcContentRow:subtitle-selectable:
   *
   * Whether the user can copy the subtitle from the label.
   *
   * See also [property@Gtk.Label:selectable].
   *
   * Since: 1.3
   */
  props[PROP_SUBTITLE_SELECTABLE] =
    g_param_spec_boolean ("subtitle-selectable", NULL, NULL,
                          FALSE,
                          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_EXPLICIT_NOTIFY);

  g_object_class_install_properties (object_class, LAST_PROP, props);

  /**
   * CcContentRow::activated:
   *
   * This signal is emitted after the row has been activated.
   */
  signals[SIGNAL_ACTIVATED] =
    g_signal_new ("activated",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  0,
                  NULL, NULL, NULL,
                  G_TYPE_NONE,
                  0);

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/control-center/common/cc-content-row.ui");

  gtk_widget_class_bind_template_child_private (widget_class, CcContentRow, contents);
  gtk_widget_class_bind_template_child_private (widget_class, CcContentRow, header);
  gtk_widget_class_bind_template_child_private (widget_class, CcContentRow, prefixes);
  gtk_widget_class_bind_template_child_private (widget_class, CcContentRow, subtitle);
  gtk_widget_class_bind_template_child_private (widget_class, CcContentRow, suffixes);
  gtk_widget_class_bind_template_child_private (widget_class, CcContentRow, title);
  gtk_widget_class_bind_template_child_private (widget_class, CcContentRow, title_box);
  gtk_widget_class_bind_template_callback (widget_class, string_is_not_empty);
}

static void
cc_content_row_init (CcContentRow *self)
{
  g_autoptr(GtkCssProvider) provider = NULL;

  gtk_widget_init_template (GTK_WIDGET (self));

  provider = gtk_css_provider_new ();
  gtk_css_provider_load_from_resource (provider, "/org/gnome/control-center/common/common.css");
  gtk_style_context_add_provider_for_display (gdk_display_get_default (),
                                              GTK_STYLE_PROVIDER (provider),
                                              GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);

  g_signal_connect (self, "notify::parent", G_CALLBACK (parent_cb), NULL);
}

static void
cc_content_row_buildable_add_child (GtkBuildable *buildable,
                                    GtkBuilder   *builder,
                                    GObject      *child,
                                    const char   *type)
{
  CcContentRow *self = CC_CONTENT_ROW (buildable);
  CcContentRowPrivate *priv = cc_content_row_get_instance_private (self);

  if (!priv->header)
    parent_buildable_iface->add_child (buildable, builder, child, type);
  else if (g_strcmp0 (type, "prefix") == 0)
    cc_content_row_add_prefix (self, GTK_WIDGET (child));
  else if (g_strcmp0 (type, "suffix") == 0)
    cc_content_row_add_suffix (self, GTK_WIDGET (child));
  else if (g_strcmp0 (type, "content") == 0)
    cc_content_row_add_content (self, GTK_WIDGET (child));
  else if (!type && GTK_IS_WIDGET (child))
    cc_content_row_add_suffix (self, GTK_WIDGET (child));
  else
    parent_buildable_iface->add_child (buildable, builder, child, type);
}

static void
cc_content_row_buildable_init (GtkBuildableIface *iface)
{
  parent_buildable_iface = g_type_interface_peek_parent (iface);
  iface->add_child = cc_content_row_buildable_add_child;
}

/**
 * cc_content_row_new:
 *
 * Creates a new `CcContentRow`.
 *
 * Returns: the newly created `CcContentRow`
 */
GtkWidget *
cc_content_row_new (void)
{
  return g_object_new (ADW_TYPE_ACTION_ROW, NULL);
}

/**
 * cc_content_row_add_prefix:
 * @self: a content row
 * @widget: a widget
 *
 * Adds a prefix widget to @self.
 */
void
cc_content_row_add_prefix (CcContentRow *self,
                           GtkWidget    *widget)
{
  CcContentRowPrivate *priv;

  g_return_if_fail (CC_IS_CONTENT_ROW (self));
  g_return_if_fail (GTK_IS_WIDGET (widget));
  g_return_if_fail (gtk_widget_get_parent (widget) == NULL);

  priv = cc_content_row_get_instance_private (self);

  gtk_box_prepend (priv->prefixes, widget);
  gtk_widget_set_visible (GTK_WIDGET (priv->prefixes), TRUE);
}

/**
 * cc_content_row_add_suffix:
 * @self: a content row
 * @widget: a widget
 *
 * Adds a suffix widget to @self.
 */
void
cc_content_row_add_suffix (CcContentRow *self,
                           GtkWidget    *widget)
{
  CcContentRowPrivate *priv;

  g_return_if_fail (CC_IS_CONTENT_ROW (self));
  g_return_if_fail (GTK_IS_WIDGET (widget));
  g_return_if_fail (gtk_widget_get_parent (widget) == NULL);

  priv = cc_content_row_get_instance_private (self);

  gtk_box_append (priv->suffixes, widget);
  gtk_widget_set_visible (GTK_WIDGET (priv->suffixes), TRUE);
}

/**
 * cc_content_row_add_content:
 * @self: a content row
 * @widget: a widget
 *
 * Adds a content widget to @self.
 */
void
cc_content_row_add_content (CcContentRow *self,
                            GtkWidget    *widget)
{
  CcContentRowPrivate *priv;

  g_return_if_fail (CC_IS_CONTENT_ROW (self));
  g_return_if_fail (GTK_IS_WIDGET (widget));
  g_return_if_fail (gtk_widget_get_parent (widget) == NULL);

  priv = cc_content_row_get_instance_private (self);

  gtk_box_append (priv->contents, widget);
  gtk_widget_set_visible (GTK_WIDGET (priv->contents), TRUE);
}

/* From adw-widget-utils-private.h */
#define ADW_CRITICAL_CANNOT_REMOVE_CHILD(parent, child) \
G_STMT_START { \
  g_critical ("%s:%d: tried to remove non-child %p of type '%s' from %p of type '%s'", \
              __FILE__, __LINE__, \
              (child), \
              G_OBJECT_TYPE_NAME ((GObject*) (child)), \
              (parent), \
              G_OBJECT_TYPE_NAME ((GObject*) (parent))); \
} G_STMT_END

/**
 * cc_content_row_remove:
 * @self: a content row
 * @widget: the child to be removed
 *
 * Removes a child from @self.
 */
void
cc_content_row_remove (CcContentRow *self,
                       GtkWidget    *child)
{
  CcContentRowPrivate *priv;
  GtkWidget *parent;

  g_return_if_fail (CC_IS_CONTENT_ROW (self));
  g_return_if_fail (GTK_IS_WIDGET (child));

  priv = cc_content_row_get_instance_private (self);

  parent = gtk_widget_get_parent (child);

  if (parent == GTK_WIDGET (priv->prefixes) ||
      parent == GTK_WIDGET (priv->suffixes) ||
      parent == GTK_WIDGET (priv->contents)) {
    gtk_box_remove (GTK_BOX (parent), child);
    gtk_widget_set_visible (parent, gtk_widget_get_first_child (parent) != NULL);
  }
  else {
    ADW_CRITICAL_CANNOT_REMOVE_CHILD (self, child);
  }
}

/**
 * cc_content_row_get_subtitle:
 * @self: a content row
 *
 * Gets the subtitle for @self.
 *
 * Returns: (nullable): the subtitle for @self
 */
const char *
cc_content_row_get_subtitle (CcContentRow *self)
{
  CcContentRowPrivate *priv;

  g_return_val_if_fail (CC_IS_CONTENT_ROW (self), NULL);

  priv = cc_content_row_get_instance_private (self);

  return gtk_label_get_text (priv->subtitle);
}

/**
 * cc_content_row_set_subtitle:
 * @self: a content row
 * @subtitle: the subtitle
 *
 * Sets the subtitle for @self.
 *
 * The subtitle is interpreted as Pango markup unless
 * [property@PreferencesRow:use-markup] is set to `FALSE`.
 */
void
cc_content_row_set_subtitle (CcContentRow *self,
                             const char   *subtitle)
{
  CcContentRowPrivate *priv;

  g_return_if_fail (CC_IS_CONTENT_ROW (self));

  priv = cc_content_row_get_instance_private (self);

  if (g_strcmp0 (gtk_label_get_text (priv->subtitle), subtitle) == 0)
    return;

  gtk_label_set_label (priv->subtitle, subtitle);

  g_object_notify_by_pspec (G_OBJECT (self), props[PROP_SUBTITLE]);
}

/**
 * cc_content_row_get_activatable_widget:
 * @self: a content row
 *
 * Gets the widget activated when @self is activated.
 *
 * Returns: (nullable) (transfer none): the activatable widget for @self
 */
GtkWidget *
cc_content_row_get_activatable_widget (CcContentRow *self)
{
  CcContentRowPrivate *priv;

  g_return_val_if_fail (CC_IS_CONTENT_ROW (self), NULL);

  priv = cc_content_row_get_instance_private (self);

  return priv->activatable_widget;
}

static void
activatable_widget_weak_notify (gpointer  data,
                                GObject  *where_the_object_was)
{
  CcContentRow *self = CC_CONTENT_ROW (data);
  CcContentRowPrivate *priv = cc_content_row_get_instance_private (self);

  priv->activatable_widget = NULL;
  priv->activatable_binding = NULL;

  g_object_notify_by_pspec (G_OBJECT (self), props[PROP_ACTIVATABLE_WIDGET]);
}

/**
 * cc_content_row_set_activatable_widget:
 * @self: a content row
 * @widget: (nullable): the target widget
 *
 * Sets the widget to activate when @self is activated.
 *
 * The row can be activated either by clicking on it, calling
 * [method@ActionRow.activate], or via mnemonics in the title.
 * See the [property@PreferencesRow:use-underline] property to enable mnemonics.
 *
 * The target widget will be activated by emitting the
 * [signal@Gtk.Widget::mnemonic-activate] signal on it.
 */
void
cc_content_row_set_activatable_widget (CcContentRow *self,
                                       GtkWidget    *widget)
{
  CcContentRowPrivate *priv;

  g_return_if_fail (CC_IS_CONTENT_ROW (self));
  g_return_if_fail (widget == NULL || GTK_IS_WIDGET (widget));

  priv = cc_content_row_get_instance_private (self);

  if (priv->activatable_widget == widget)
    return;

  g_clear_pointer (&priv->activatable_binding, g_binding_unbind);

  if (priv->activatable_widget) {
    gtk_accessible_reset_relation (GTK_ACCESSIBLE (priv->activatable_widget),
                                   GTK_ACCESSIBLE_RELATION_LABELLED_BY);
    gtk_accessible_reset_relation (GTK_ACCESSIBLE (priv->activatable_widget),
                                   GTK_ACCESSIBLE_RELATION_DESCRIBED_BY);

    g_object_weak_unref (G_OBJECT (priv->activatable_widget),
                         activatable_widget_weak_notify,
                         self);
  }

  priv->activatable_widget = widget;

  if (priv->activatable_widget != NULL) {
    g_object_weak_ref (G_OBJECT (priv->activatable_widget),
                       activatable_widget_weak_notify,
                       self);

    priv->activatable_binding =
      g_object_bind_property (widget, "sensitive",
                              self, "activatable",
                              G_BINDING_SYNC_CREATE);

    gtk_accessible_update_relation (GTK_ACCESSIBLE (priv->activatable_widget),
                                    GTK_ACCESSIBLE_RELATION_LABELLED_BY, priv->title, NULL,
                                    GTK_ACCESSIBLE_RELATION_DESCRIBED_BY, priv->subtitle, NULL,
                                    -1);
  }

  g_object_notify_by_pspec (G_OBJECT (self), props[PROP_ACTIVATABLE_WIDGET]);
}

/**
 * cc_content_row_get_title_lines:
 * @self: a content row
 *
 * Gets the number of lines at the end of which the title label will be
 * ellipsized.
 *
 * Returns: the number of lines at the end of which the title label will be
 *   ellipsized
 */
int
cc_content_row_get_title_lines (CcContentRow *self)
{
  CcContentRowPrivate *priv;

  g_return_val_if_fail (CC_IS_CONTENT_ROW (self), 0);

  priv = cc_content_row_get_instance_private (self);

  return priv->title_lines;
}

/**
 * cc_content_row_set_title_lines:
 * @self: a content row
 * @title_lines: the number of lines at the end of which the title label will be ellipsized
 *
 * Sets the number of lines at the end of which the title label will be
 * ellipsized.
 *
 * If the value is 0, the number of lines won't be limited.
 */
void
cc_content_row_set_title_lines (CcContentRow *self,
                                int           title_lines)
{
  CcContentRowPrivate *priv;

  g_return_if_fail (CC_IS_CONTENT_ROW (self));
  g_return_if_fail (title_lines >= 0);

  priv = cc_content_row_get_instance_private (self);

  if (priv->title_lines == title_lines)
    return;

  priv->title_lines = title_lines;

  gtk_label_set_lines (priv->title, title_lines);
  gtk_label_set_ellipsize (priv->title, title_lines == 0 ? PANGO_ELLIPSIZE_NONE : PANGO_ELLIPSIZE_END);

  g_object_notify_by_pspec (G_OBJECT (self), props[PROP_TITLE_LINES]);
}

/**
 * cc_content_row_get_subtitle_lines:
 * @self: a content row
 *
 * Gets the number of lines at the end of which the subtitle label will be
 * ellipsized.
 *
 * Returns: the number of lines at the end of which the subtitle label will be
 *   ellipsized
 */
int
cc_content_row_get_subtitle_lines (CcContentRow *self)
{
  CcContentRowPrivate *priv;

  g_return_val_if_fail (CC_IS_CONTENT_ROW (self), 0);

  priv = cc_content_row_get_instance_private (self);

  return priv->subtitle_lines;
}

/**
 * cc_content_row_set_subtitle_lines:
 * @self: a content row
 * @subtitle_lines: the number of lines at the end of which the subtitle label will be ellipsized
 *
 * Sets the number of lines at the end of which the subtitle label will be
 * ellipsized.
 *
 * If the value is 0, the number of lines won't be limited.
 */
void
cc_content_row_set_subtitle_lines (CcContentRow *self,
                                   int           subtitle_lines)
{
  CcContentRowPrivate *priv;

  g_return_if_fail (CC_IS_CONTENT_ROW (self));
  g_return_if_fail (subtitle_lines >= 0);

  priv = cc_content_row_get_instance_private (self);

  if (priv->subtitle_lines == subtitle_lines)
    return;

  priv->subtitle_lines = subtitle_lines;

  gtk_label_set_lines (priv->subtitle, subtitle_lines);
  gtk_label_set_ellipsize (priv->subtitle, subtitle_lines == 0 ? PANGO_ELLIPSIZE_NONE : PANGO_ELLIPSIZE_END);

  g_object_notify_by_pspec (G_OBJECT (self), props[PROP_SUBTITLE_LINES]);
}

/**
 * cc_content_row_get_subtitle_selectable:
 * @self: a content row
 *
 * Gets whether the user can copy the subtitle from the label
 *
 * Returns: whether the user can copy the subtitle from the label
 *
 * Since: 1.3
 */
gboolean
cc_content_row_get_subtitle_selectable (CcContentRow *self)
{
  CcContentRowPrivate *priv = cc_content_row_get_instance_private (self);

  g_return_val_if_fail (CC_IS_CONTENT_ROW (self), FALSE);

  return priv->subtitle_selectable;
}

/**
 * cc_content_row_set_subtitle_selectable:
 * @self: a content row
 * @subtitle_selectable: `TRUE` if the user can copy the subtitle from the label
 *
 * Sets whether the user can copy the subtitle from the label
 *
 * See also [property@Gtk.Label:selectable].
 *
 * Since: 1.3
 */
void
cc_content_row_set_subtitle_selectable (CcContentRow *self,
                                        gboolean      subtitle_selectable)
{
  CcContentRowPrivate *priv = cc_content_row_get_instance_private (self);

  g_return_if_fail (CC_IS_CONTENT_ROW (self));

  subtitle_selectable = !!subtitle_selectable;

  if (priv->subtitle_selectable == subtitle_selectable)
    return;

  priv->subtitle_selectable = subtitle_selectable;

  g_object_notify_by_pspec (G_OBJECT (self), props[PROP_SUBTITLE_SELECTABLE]);
}

/**
 * cc_content_row_activate:
 * @self: a content row
 *
 * Activates @self.
 */
void
cc_content_row_activate (CcContentRow *self)
{
  g_return_if_fail (CC_IS_CONTENT_ROW (self));

  CC_CONTENT_ROW_GET_CLASS (self)->activate (self);
}
