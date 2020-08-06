/* -*- Mode: C; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* cc-list-row.c
 *
 * Copyright 2020 Red Hat Inc.
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
 * Author(s):
 *   Bastien Nocera <hadess@hadess.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#undef G_LOG_DOMAIN
#define G_LOG_DOMAIN "cc-power-profile-row"

#include <config.h>

#include <glib/gi18n.h>
#include "cc-power-profile-row.h"

struct _CcPowerProfileRow
{
  GtkListBoxRow parent_instance;

  CcPowerProfile power_profile;
  char *performance_inhibited;
  GtkRadioButton *button;
  GtkWidget *subtext;
};

G_DEFINE_TYPE (CcPowerProfileRow, cc_power_profile_row, GTK_TYPE_LIST_BOX_ROW)

enum {
  PROP_0,
  PROP_POWER_PROFILE,
  PROP_PERFORMANCE_INHIBITED,
  N_PROPS
};

enum {
  BUTTON_TOGGLED,
  N_SIGNALS
};

static GParamSpec *properties[N_PROPS];
static guint signals[N_SIGNALS];

static const char *
get_performance_inhibited_text (const char *inhibited)
{
  if (!inhibited || *inhibited == '\0')
    return NULL;

  if (g_str_equal (inhibited, "lap-detected"))
    return _("Lap detected: performance mode unavailable");
  if (g_str_equal (inhibited, "high-operating-temperature"))
    return _("High hardware temperature: performance mode unavailable");
  return _("Performance mode unavailable");
}

static void
performance_profile_set_inhibited (CcPowerProfileRow *row,
                                   const char        *performance_inhibited)
{
  const char *text;
  gboolean inhibited = FALSE;

  if (row->power_profile != CC_POWER_PROFILE_PERFORMANCE)
    return;

  gtk_style_context_remove_class (gtk_widget_get_style_context (row->subtext),
                                  GTK_STYLE_CLASS_DIM_LABEL);
  gtk_style_context_remove_class (gtk_widget_get_style_context (row->subtext),
                                  GTK_STYLE_CLASS_ERROR);

  text = get_performance_inhibited_text (performance_inhibited);
  if (text)
    inhibited = TRUE;
  else
    text = _("High performance and power usage.");
  gtk_label_set_text (GTK_LABEL (row->subtext), text);

  gtk_style_context_add_class (gtk_widget_get_style_context (row->subtext),
                               inhibited ? GTK_STYLE_CLASS_ERROR : GTK_STYLE_CLASS_DIM_LABEL);
  gtk_widget_set_sensitive (GTK_WIDGET (row), !inhibited);
}

static void
cc_power_profile_row_get_property (GObject    *object,
                          guint       prop_id,
                          GValue     *value,
                          GParamSpec *pspec)
{
  CcPowerProfileRow *self = (CcPowerProfileRow *)object;

  switch (prop_id)
    {
    case PROP_POWER_PROFILE:
      g_value_set_int (value, self->power_profile);
      break;

    case PROP_PERFORMANCE_INHIBITED:
      g_value_set_string (value, self->performance_inhibited);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
cc_power_profile_row_set_property (GObject      *object,
                          guint         prop_id,
                          const GValue *value,
                          GParamSpec   *pspec)
{
  CcPowerProfileRow *row = (CcPowerProfileRow *)object;

  switch (prop_id)
    {
    case PROP_POWER_PROFILE:
      g_assert (row->power_profile == -1);
      row->power_profile = g_value_get_int (value);
      g_assert (row->power_profile != -1);
      break;

    case PROP_PERFORMANCE_INHIBITED:
      cc_power_profile_row_set_performance_inhibited (row, g_value_get_string (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static GtkWidget *
performance_row_new (const gchar  *title,
                     const gchar  *icon_name,
                     const gchar  *class_name,
                     const gchar  *subtitle)
{
  PangoAttrList *attributes;
  GtkWidget *grid, *button, *label, *image;
  GtkStyleContext *context;

  grid = gtk_grid_new ();
  g_object_set (G_OBJECT (grid),
                "margin-top", 6,
                "margin-bottom", 6,
                NULL);
  gtk_widget_show (grid);

  button = gtk_radio_button_new (NULL);
  g_object_set (G_OBJECT (button),
                "margin-end", 18,
                "margin-start", 6,
                NULL);
  gtk_widget_show (button);
  g_object_set_data (G_OBJECT (grid), "button", button);
  gtk_grid_attach (GTK_GRID (grid), button, 0, 0, 1, 2);

  image = gtk_image_new_from_icon_name (icon_name, GTK_ICON_SIZE_MENU);
  gtk_widget_set_margin_end (image, 6);
  gtk_widget_show (image);
  gtk_grid_attach (GTK_GRID (grid), image, 1, 0, 1, 1);

  context = gtk_widget_get_style_context (image);
  gtk_style_context_add_class (context, "power-profile");
  if (class_name != NULL)
    gtk_style_context_add_class (context, class_name);

  label = gtk_label_new (title);
  g_object_set (G_OBJECT (label),
                "ellipsize", PANGO_ELLIPSIZE_END,
                "halign", GTK_ALIGN_START,
                "expand", TRUE,
                "use-markup", TRUE,
                "use-underline", TRUE,
                "visible", TRUE,
                "xalign", 0.0,
                NULL);
  gtk_widget_show (label);
  gtk_grid_attach (GTK_GRID (grid), label, 2, 0, 1, 1);

  attributes = pango_attr_list_new ();
  pango_attr_list_insert (attributes, pango_attr_scale_new (0.9));

  label = gtk_label_new (subtitle);
  g_object_set (G_OBJECT (label),
                "ellipsize", PANGO_ELLIPSIZE_END,
                "halign", GTK_ALIGN_START,
                "expand", TRUE,
                "use-markup", TRUE,
                "use-underline", TRUE,
                "visible", TRUE,
                "xalign", 0.0,
                "attributes", attributes,
                NULL);
  gtk_style_context_add_class (gtk_widget_get_style_context (label),
                               GTK_STYLE_CLASS_DIM_LABEL);
  g_object_set_data (G_OBJECT (grid), "subtext", label);
  gtk_grid_attach (GTK_GRID (grid), label, 1, 1, 2, 1);

  pango_attr_list_unref (attributes);

  return grid;
}

static void
cc_power_profile_row_button_toggled_cb (GObject *row)
{
  g_signal_emit (row, signals[BUTTON_TOGGLED], 0);
}

static void
cc_power_profile_row_constructed (GObject *object)
{
  CcPowerProfileRow *row;
  GtkWidget *box, *title;
  const char *text, *subtext, *icon_name, *class_name;

  row = CC_POWER_PROFILE_ROW (object);

  switch (row->power_profile)
    {
      case CC_POWER_PROFILE_PERFORMANCE:
        text = _("Performance");
        subtext = _("High performance and power usage.");
        icon_name = "power-profile-performance-symbolic";
        class_name = "performance";
        break;
      case CC_POWER_PROFILE_BALANCED:
        text = _("Balanced Power");
        subtext = _("Standard performance and power usage.");
        icon_name = "power-profile-balanced-symbolic";
        class_name = NULL;
        break;
      case CC_POWER_PROFILE_POWER_SAVER:
        text = _("Power Saver");
        subtext = _("Reduced performance and power usage.");
        icon_name = "power-profile-power-saver-symbolic";
        class_name = "low-power";
        break;
      default:
        g_assert_not_reached ();
    }

  gtk_list_box_row_set_selectable (GTK_LIST_BOX_ROW (row), FALSE);
  gtk_widget_show (GTK_WIDGET (row));
  box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 12);
  g_object_set (G_OBJECT (box),
                "margin-end", 12,
                "margin-start", 12,
                "visible", TRUE,
                NULL);
  gtk_container_add (GTK_CONTAINER (row), box);

  title = performance_row_new (text, icon_name, class_name, subtext);
  row->subtext = g_object_get_data (G_OBJECT (title), "subtext");
  row->button = g_object_get_data (G_OBJECT (title), "button");
  g_signal_connect_object (G_OBJECT (row->button), "toggled",
                           G_CALLBACK (cc_power_profile_row_button_toggled_cb),
                           row, G_CONNECT_SWAPPED);
  if (row->power_profile == CC_POWER_PROFILE_PERFORMANCE)
    performance_profile_set_inhibited (row, row->performance_inhibited);
  gtk_box_pack_start (GTK_BOX (box), title, TRUE, TRUE, 0);
}

static void
cc_power_profile_row_class_init (CcPowerProfileRowClass *klass)
{
  GObjectClass   *object_class = G_OBJECT_CLASS (klass);

  object_class->get_property = cc_power_profile_row_get_property;
  object_class->set_property = cc_power_profile_row_set_property;
  object_class->constructed = cc_power_profile_row_constructed;

  properties[PROP_POWER_PROFILE] =
    g_param_spec_int ("power-profile",
                      "Power Profile",
                      "Power profile for the row",
                      -1, CC_POWER_PROFILE_POWER_SAVER,
                      -1,
                      G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_CONSTRUCT_ONLY);

  properties[PROP_PERFORMANCE_INHIBITED] =
    g_param_spec_string ("performance-inhibited",
                         "Performance Inhibited",
                         "Performance inhibition reason",
                         NULL,
                         G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

  signals[BUTTON_TOGGLED] =
    g_signal_new ("button-toggled",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_FIRST,
                  0, NULL, NULL,
                  NULL,
                  G_TYPE_NONE, 0);

  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
cc_power_profile_row_init (CcPowerProfileRow *row)
{
  row->power_profile = -1;
}

CcPowerProfile
cc_power_profile_row_get_profile (CcPowerProfileRow *row)
{
  g_return_val_if_fail (CC_IS_POWER_PROFILE_ROW (row), -1);

  return row->power_profile;
}

GtkRadioButton *
cc_power_profile_row_get_radio_button (CcPowerProfileRow *row)
{
  g_return_val_if_fail (CC_IS_POWER_PROFILE_ROW (row), NULL);

  return row->button;
}

void
cc_power_profile_row_set_active (CcPowerProfileRow *row,
                                 gboolean           active)
{
  g_return_if_fail (CC_IS_POWER_PROFILE_ROW (row));

  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (row->button), active);
}

void
cc_power_profile_row_set_performance_inhibited (CcPowerProfileRow *row,
                                                const char        *performance_inhibited)
{
  g_return_if_fail (CC_IS_POWER_PROFILE_ROW (row));

  g_clear_pointer (&row->performance_inhibited, g_free);
  row->performance_inhibited = g_strdup (performance_inhibited);
  performance_profile_set_inhibited (row, row->performance_inhibited);
}

gboolean
cc_power_profile_row_get_active (CcPowerProfileRow *self)
{
  g_return_val_if_fail (CC_IS_POWER_PROFILE_ROW (self), FALSE);

  return gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (self->button));
}

GtkWidget *
cc_power_profile_row_new (CcPowerProfile  power_profile,
                          const char     *performance_inhibited)
{
  return g_object_new (CC_TYPE_POWER_PROFILE_ROW,
                       "power-profile", power_profile,
                       "performance-inhibited", performance_inhibited,
                       NULL);
}

CcPowerProfile
cc_power_profile_from_str (const char *profile)
{
  if (g_strcmp0 (profile, "power-saver") == 0)
    return CC_POWER_PROFILE_POWER_SAVER;
  if (g_strcmp0 (profile, "balanced") == 0)
    return CC_POWER_PROFILE_BALANCED;
  if (g_strcmp0 (profile, "performance") == 0)
    return CC_POWER_PROFILE_PERFORMANCE;

  g_assert_not_reached ();
}

const char *
cc_power_profile_to_str (CcPowerProfile profile)
{
  switch (profile)
  {
  case CC_POWER_PROFILE_POWER_SAVER:
    return "power-saver";
  case CC_POWER_PROFILE_BALANCED:
    return "balanced";
  case CC_POWER_PROFILE_PERFORMANCE:
    return "performance";
  default:
    g_assert_not_reached ();
  }
}
