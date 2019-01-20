/*
 * Copyright (C) 2016  Red Hat, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 *
 */

#include <math.h>
#include "cc-display-config.h"

static const double known_diagonals[] = {
  12.1,
  13.3,
  15.6
};

static char *
diagonal_to_str (double d)
{
  int i;

  for (i = 0; i < G_N_ELEMENTS (known_diagonals); i++)
    {
      double delta;

      delta = fabs(known_diagonals[i] - d);
      if (delta < 0.1)
          return g_strdup_printf ("%0.1lf\"", known_diagonals[i]);
    }

  return g_strdup_printf ("%d\"", (int) (d + 0.5));
}

static char *
make_display_size_string (int width_mm,
                          int height_mm)
{
  char *inches = NULL;

  if (width_mm > 0 && height_mm > 0)
    {
      double d = sqrt (width_mm * width_mm + height_mm * height_mm);

      inches = diagonal_to_str (d / 25.4);
    }

  return inches;
}

static char *
make_output_ui_name (CcDisplayMonitor *output)
{
  int width_mm, height_mm;
  g_autofree char *size = NULL;

  cc_display_monitor_get_physical_size (output, &width_mm, &height_mm);
  size = make_display_size_string (width_mm, height_mm);
  if (size)
    return g_strdup_printf ("%s (%s)", cc_display_monitor_get_display_name (output), size);
  else
    return g_strdup_printf ("%s", cc_display_monitor_get_display_name (output));
}



G_DEFINE_TYPE (CcDisplayMode,
               cc_display_mode,
               G_TYPE_OBJECT)

static void
cc_display_mode_init (CcDisplayMode *self)
{
}

static void
cc_display_mode_class_init (CcDisplayModeClass *klass)
{
}

void
cc_display_mode_get_resolution (CcDisplayMode *self, int *w, int *h)
{
  return CC_DISPLAY_MODE_GET_CLASS (self)->get_resolution (self, w, h);
}

const double *
cc_display_mode_get_supported_scales (CcDisplayMode *self)
{
  return CC_DISPLAY_MODE_GET_CLASS (self)->get_supported_scales (self);
}

double
cc_display_mode_get_preferred_scale (CcDisplayMode *self)
{
  return CC_DISPLAY_MODE_GET_CLASS (self)->get_preferred_scale (self);
}

gboolean
cc_display_mode_is_interlaced (CcDisplayMode *self)
{
  return CC_DISPLAY_MODE_GET_CLASS (self)->is_interlaced (self);
}

int
cc_display_mode_get_freq (CcDisplayMode *self)
{
  return CC_DISPLAY_MODE_GET_CLASS (self)->get_freq (self);
}

double
cc_display_mode_get_freq_f (CcDisplayMode *self)
{
  return CC_DISPLAY_MODE_GET_CLASS (self)->get_freq_f (self);
}


struct _CcDisplayMonitorPrivate {
  int ui_number;
  gchar *ui_name;
  gchar *ui_number_name;
  gboolean is_usable;
};
typedef struct _CcDisplayMonitorPrivate CcDisplayMonitorPrivate;

G_DEFINE_TYPE_WITH_PRIVATE (CcDisplayMonitor,
                            cc_display_monitor,
                            G_TYPE_OBJECT)
#define CC_DISPLAY_MONITOR_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), CC_TYPE_DISPLAY_MONITOR, CcDisplayMonitorPrivate))

static void
cc_display_monitor_init (CcDisplayMonitor *self)
{
  CcDisplayMonitorPrivate *priv = CC_DISPLAY_MONITOR_GET_PRIVATE (self);

  priv->ui_number = 0;
  priv->ui_name = NULL;
  priv->ui_number_name = NULL;
  priv->is_usable = TRUE;
}

static void
cc_display_monitor_finalize (GObject *object)
{
  CcDisplayMonitor *self = CC_DISPLAY_MONITOR (object);
  CcDisplayMonitorPrivate *priv = CC_DISPLAY_MONITOR_GET_PRIVATE (self);

  g_clear_pointer (&priv->ui_name, g_free);
  g_clear_pointer (&priv->ui_number_name, g_free);

  G_OBJECT_CLASS (cc_display_monitor_parent_class)->finalize (object);
}

static void
cc_display_monitor_class_init (CcDisplayMonitorClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  gobject_class->finalize = cc_display_monitor_finalize;

  g_signal_new ("rotation",
                CC_TYPE_DISPLAY_MONITOR,
                G_SIGNAL_RUN_LAST,
                0, NULL, NULL, NULL,
                G_TYPE_NONE, 0);
  g_signal_new ("mode",
                CC_TYPE_DISPLAY_MONITOR,
                G_SIGNAL_RUN_LAST,
                0, NULL, NULL, NULL,
                G_TYPE_NONE, 0);
  g_signal_new ("primary",
                CC_TYPE_DISPLAY_MONITOR,
                G_SIGNAL_RUN_LAST,
                0, NULL, NULL, NULL,
                G_TYPE_NONE, 0);
  g_signal_new ("active",
                CC_TYPE_DISPLAY_MONITOR,
                G_SIGNAL_RUN_LAST,
                0, NULL, NULL, NULL,
                G_TYPE_NONE, 0);
  g_signal_new ("scale",
                CC_TYPE_DISPLAY_MONITOR,
                G_SIGNAL_RUN_LAST,
                0, NULL, NULL, NULL,
                G_TYPE_NONE, 0);
  g_signal_new ("position-changed",
                CC_TYPE_DISPLAY_MONITOR,
                G_SIGNAL_RUN_LAST,
                0, NULL, NULL, NULL,
                G_TYPE_NONE, 0);
  g_signal_new ("is-usable",
                CC_TYPE_DISPLAY_MONITOR,
                G_SIGNAL_RUN_LAST,
                0, NULL, NULL, NULL,
                G_TYPE_NONE, 0);
}

const char *
cc_display_monitor_get_display_name (CcDisplayMonitor *self)
{
  return CC_DISPLAY_MONITOR_GET_CLASS (self)->get_display_name (self);
}

const char *
cc_display_monitor_get_connector_name (CcDisplayMonitor *self)
{
  return CC_DISPLAY_MONITOR_GET_CLASS (self)->get_connector_name (self);
}

gboolean
cc_display_monitor_is_builtin (CcDisplayMonitor *self)
{
  return CC_DISPLAY_MONITOR_GET_CLASS (self)->is_builtin (self);
}

gboolean
cc_display_monitor_is_primary (CcDisplayMonitor *self)
{
  return CC_DISPLAY_MONITOR_GET_CLASS (self)->is_primary (self);
}

void
cc_display_monitor_set_primary (CcDisplayMonitor *self, gboolean primary)
{
  return CC_DISPLAY_MONITOR_GET_CLASS (self)->set_primary (self, primary);
}

gboolean
cc_display_monitor_is_active (CcDisplayMonitor *self)
{
  return CC_DISPLAY_MONITOR_GET_CLASS (self)->is_active (self);
}

void
cc_display_monitor_set_active (CcDisplayMonitor *self, gboolean active)
{
  return CC_DISPLAY_MONITOR_GET_CLASS (self)->set_active (self, active);
}

CcDisplayRotation
cc_display_monitor_get_rotation (CcDisplayMonitor *self)
{
  return CC_DISPLAY_MONITOR_GET_CLASS (self)->get_rotation (self);
}

void
cc_display_monitor_set_rotation (CcDisplayMonitor *self,
                                 CcDisplayRotation rotation)
{
  return CC_DISPLAY_MONITOR_GET_CLASS (self)->set_rotation (self, rotation);
}

gboolean
cc_display_monitor_supports_rotation (CcDisplayMonitor *self, CcDisplayRotation r)
{
  return CC_DISPLAY_MONITOR_GET_CLASS (self)->supports_rotation (self, r);
}

void
cc_display_monitor_get_physical_size (CcDisplayMonitor *self, int *w, int *h)
{
  return CC_DISPLAY_MONITOR_GET_CLASS (self)->get_physical_size (self, w, h);
}

void
cc_display_monitor_get_geometry (CcDisplayMonitor *self, int *x, int *y, int *w, int *h)
{
  return CC_DISPLAY_MONITOR_GET_CLASS (self)->get_geometry (self, x, y, w, h);
}

CcDisplayMode *
cc_display_monitor_get_mode (CcDisplayMonitor *self)
{
  return CC_DISPLAY_MONITOR_GET_CLASS (self)->get_mode (self);
}

CcDisplayMode *
cc_display_monitor_get_preferred_mode (CcDisplayMonitor *self)
{
  return CC_DISPLAY_MONITOR_GET_CLASS (self)->get_preferred_mode (self);
}

guint32
cc_display_monitor_get_id (CcDisplayMonitor *self)
{
  return CC_DISPLAY_MONITOR_GET_CLASS (self)->get_id (self);
}

GList *
cc_display_monitor_get_modes (CcDisplayMonitor *self)
{
  return CC_DISPLAY_MONITOR_GET_CLASS (self)->get_modes (self);
}

gboolean
cc_display_monitor_supports_underscanning (CcDisplayMonitor *self)
{
  return CC_DISPLAY_MONITOR_GET_CLASS (self)->supports_underscanning (self);
}

gboolean
cc_display_monitor_get_underscanning (CcDisplayMonitor *self)
{
  return CC_DISPLAY_MONITOR_GET_CLASS (self)->get_underscanning (self);
}

void
cc_display_monitor_set_underscanning (CcDisplayMonitor *self,
                                      gboolean underscanning)
{
  return CC_DISPLAY_MONITOR_GET_CLASS (self)->set_underscanning (self, underscanning);
}

void
cc_display_monitor_set_mode (CcDisplayMonitor *self, CcDisplayMode *m)
{
  return CC_DISPLAY_MONITOR_GET_CLASS (self)->set_mode (self, m);
}

void
cc_display_monitor_set_position (CcDisplayMonitor *self, int x, int y)
{
  return CC_DISPLAY_MONITOR_GET_CLASS (self)->set_position (self, x, y);
}

double
cc_display_monitor_get_scale (CcDisplayMonitor *self)
{
  return CC_DISPLAY_MONITOR_GET_CLASS (self)->get_scale (self);
}

void
cc_display_monitor_set_scale (CcDisplayMonitor *self, double s)
{
  return CC_DISPLAY_MONITOR_GET_CLASS (self)->set_scale (self, s);
}

gboolean
cc_display_monitor_is_useful (CcDisplayMonitor *self)
{
  return CC_DISPLAY_MONITOR_GET_PRIVATE (self)->is_usable &&
         cc_display_monitor_is_active (self);
}

gboolean
cc_display_monitor_is_usable (CcDisplayMonitor *self)
{
  return CC_DISPLAY_MONITOR_GET_PRIVATE (self)->is_usable;
}

void
cc_display_monitor_set_usable (CcDisplayMonitor *self, gboolean is_usable)
{
  CC_DISPLAY_MONITOR_GET_PRIVATE (self)->is_usable = is_usable;

  g_signal_emit_by_name (self, "is-usable");
}

gint
cc_display_monitor_get_ui_number (CcDisplayMonitor *self)
{
  return CC_DISPLAY_MONITOR_GET_PRIVATE (self)->ui_number;
}

const char *
cc_display_monitor_get_ui_name (CcDisplayMonitor *self)
{
  return CC_DISPLAY_MONITOR_GET_PRIVATE (self)->ui_name;
}

const char *
cc_display_monitor_get_ui_number_name (CcDisplayMonitor *self)
{
  return CC_DISPLAY_MONITOR_GET_PRIVATE (self)->ui_number_name;
}

static void
cc_display_monitor_set_ui_info (CcDisplayMonitor *self, gint ui_number, gchar *ui_name)
{
  CcDisplayMonitorPrivate *priv = CC_DISPLAY_MONITOR_GET_PRIVATE (self);

  priv->ui_number = ui_number;
  g_free (priv->ui_name);
  priv->ui_name = ui_name;
  priv->ui_number_name = g_strdup_printf ("%d\u2003%s", ui_number, ui_name);
}

struct _CcDisplayConfigPrivate {
  GList *ui_sorted_monitors;
};
typedef struct _CcDisplayConfigPrivate CcDisplayConfigPrivate;

G_DEFINE_TYPE_WITH_PRIVATE (CcDisplayConfig,
                            cc_display_config,
                            G_TYPE_OBJECT)
#define CC_DISPLAY_CONFIG_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), CC_TYPE_DISPLAY_CONFIG, CcDisplayConfigPrivate))

static void
cc_display_config_init (CcDisplayConfig *self)
{
  CcDisplayConfigPrivate *priv = CC_DISPLAY_CONFIG_GET_PRIVATE (self);

  priv->ui_sorted_monitors = NULL;
}

static void
cc_display_config_constructed (GObject *object)
{
  CcDisplayConfig *self = CC_DISPLAY_CONFIG (object);
  CcDisplayConfigPrivate *priv = CC_DISPLAY_CONFIG_GET_PRIVATE (self);
  GList *monitors = cc_display_config_get_monitors (self);
  GList *item;
  gint ui_number = 1;

  for (item = monitors; item != NULL; item = item->next)
    {
      CcDisplayMonitor *monitor = item->data;

      if (cc_display_monitor_is_builtin (monitor))
        priv->ui_sorted_monitors = g_list_prepend (priv->ui_sorted_monitors, monitor);
      else
        priv->ui_sorted_monitors = g_list_append (priv->ui_sorted_monitors, monitor);
    }

  for (item = priv->ui_sorted_monitors; item != NULL; item = item->next)
    {
      CcDisplayMonitor *monitor = item->data;
      char *ui_name;
      ui_name = make_output_ui_name (monitor);

      cc_display_monitor_set_ui_info (monitor, ui_number, ui_name);

      ui_number += 1;
    }
}

static void
cc_display_config_finalize (GObject *object)
{
  CcDisplayConfig *self = CC_DISPLAY_CONFIG (object);
  CcDisplayConfigPrivate *priv = CC_DISPLAY_CONFIG_GET_PRIVATE (self);

  g_list_free (priv->ui_sorted_monitors);

  G_OBJECT_CLASS (cc_display_config_parent_class)->finalize (object);
}

static void
cc_display_config_class_init (CcDisplayConfigClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  g_signal_new ("primary",
                CC_TYPE_DISPLAY_CONFIG,
                G_SIGNAL_RUN_LAST,
                0, NULL, NULL, NULL,
                G_TYPE_NONE, 0);

  gobject_class->constructed = cc_display_config_constructed;
  gobject_class->finalize = cc_display_config_finalize;
}

GList *
cc_display_config_get_monitors (CcDisplayConfig *self)
{
  return CC_DISPLAY_CONFIG_GET_CLASS (self)->get_monitors (self);
}

GList *
cc_display_config_get_ui_sorted_monitors (CcDisplayConfig *self)
{
  return CC_DISPLAY_CONFIG_GET_PRIVATE (self)->ui_sorted_monitors;
}

int
cc_display_config_count_useful_monitors (CcDisplayConfig *self)
{
  CcDisplayConfigPrivate *priv = CC_DISPLAY_CONFIG_GET_PRIVATE (self);
  GList *outputs, *l;
  guint count = 0;

  outputs = priv->ui_sorted_monitors;
  for (l = outputs; l != NULL; l = l->next)
    {
      CcDisplayMonitor *output = l->data;
      if (!cc_display_monitor_is_useful (output))
        continue;
      else
        count++;
    }
  return count;

}

gboolean
cc_display_config_is_applicable (CcDisplayConfig *self)
{
  return CC_DISPLAY_CONFIG_GET_CLASS (self)->is_applicable (self);
}

gboolean
cc_display_config_equal (CcDisplayConfig *self,
                         CcDisplayConfig *other)
{
  return CC_DISPLAY_CONFIG_GET_CLASS (self)->equal (self, other);
}

gboolean
cc_display_config_apply (CcDisplayConfig *self,
                         GError **error)
{
  return CC_DISPLAY_CONFIG_GET_CLASS (self)->apply (self, error);
}

gboolean
cc_display_config_is_cloning (CcDisplayConfig *self)
{
  return CC_DISPLAY_CONFIG_GET_CLASS (self)->is_cloning (self);
}

void
cc_display_config_set_cloning (CcDisplayConfig *self,
                               gboolean clone)
{
  return CC_DISPLAY_CONFIG_GET_CLASS (self)->set_cloning (self, clone);
}

GList *
cc_display_config_get_cloning_modes (CcDisplayConfig *self)
{
  return CC_DISPLAY_CONFIG_GET_CLASS (self)->get_cloning_modes (self);
}

gboolean
cc_display_config_is_layout_logical (CcDisplayConfig *self)
{
  return CC_DISPLAY_CONFIG_GET_CLASS (self)->is_layout_logical (self);
}
