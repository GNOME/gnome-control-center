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

#include <gio/gio.h>
#include <math.h>
#include "cc-display-config.h"

#define MODE_BASE_FORMAT "siiddad"
#define MODE_FORMAT "(" MODE_BASE_FORMAT "a{sv})"
#define MODES_FORMAT "a" MODE_FORMAT
#define MONITOR_SPEC_FORMAT "(ssss)"
#define MONITOR_FORMAT "(" MONITOR_SPEC_FORMAT MODES_FORMAT "a{sv})"
#define MONITORS_FORMAT "a" MONITOR_FORMAT

#define LOGICAL_MONITOR_MONITORS_FORMAT "a" MONITOR_SPEC_FORMAT
#define LOGICAL_MONITOR_FORMAT "(iidub" LOGICAL_MONITOR_MONITORS_FORMAT "a{sv})"
#define LOGICAL_MONITORS_FORMAT "a" LOGICAL_MONITOR_FORMAT

#define CURRENT_STATE_FORMAT "(u" MONITORS_FORMAT LOGICAL_MONITORS_FORMAT "a{sv})"

enum
{
  PROP_0,
  PROP_STATE,
  PROP_CONNECTION,
};

typedef enum _CcDisplayModeFlags
{
  MODE_PREFERRED = 1 << 0,
  MODE_CURRENT = 1 << 1,
  MODE_INTERLACED = 1 << 2,
} CcDisplayModeFlags;

typedef enum _CcDisplayLayoutMode
{
  CC_DISPLAY_LAYOUT_MODE_LOGICAL = 1,
  CC_DISPLAY_LAYOUT_MODE_PHYSICAL = 2
} CcDisplayLayoutMode;

typedef enum _CcDisplayConfigMethod
{
  CC_DISPLAY_CONFIG_METHOD_VERIFY = 0,
  CC_DISPLAY_CONFIG_METHOD_TEMPORARY = 1,
  CC_DISPLAY_CONFIG_METHOD_PERSISTENT = 2
} CcDisplayConfigMethod;

typedef enum _CcDisplayMonitorUnderscanning
{
  UNDERSCANNING_UNSUPPORTED = 0,
  UNDERSCANNING_DISABLED,
  UNDERSCANNING_ENABLED
} CcDisplayMonitorUnderscanning;

struct _CcDisplayMode
{
  GObject parent_instance;

  CcDisplayMonitor *monitor;

  char *id;
  int width;
  int height;
  double refresh_rate;
  CcDisplayModeRefreshRateMode refresh_rate_mode;
  double preferred_scale;
  GArray *supported_scales;
  guint32 flags;
};

struct _CcDisplayLogicalMonitor
{
  GObject parent_instance;

  int x;
  int y;
  double scale;
  CcDisplayRotation rotation;
  gboolean primary;

  GHashTable *monitors;
};

struct _CcDisplayMonitor
{
  GObject parent_instance;

  int ui_number;
  char *ui_name;
  char *ui_number_name;
  gboolean is_usable;

  CcDisplayConfig *config;

  char *connector_name;
  char *vendor_name;
  char *product_name;
  char *product_serial;
  char *display_name;

  int width_mm;
  int height_mm;
  gboolean builtin;
  CcDisplayMonitorUnderscanning underscanning;
  CcDisplayMonitorPrivacy privacy_screen;
  int max_width;
  int max_height;
  int min_refresh_rate;

  GList *modes;
  CcDisplayMode *current_mode;
  CcDisplayMode *preferred_mode;

  gboolean supports_variable_refresh_rate;

  CcDisplayColorMode color_mode;
  GList *supported_color_modes;

  CcDisplayLogicalMonitor *logical_monitor;
};

struct _CcDisplayConfig
{
  GObject parent_instance;

  GList *ui_sorted_monitors;

  GVariant *state;
  GDBusConnection *connection;
  GDBusProxy *proxy;

  int min_width;
  int min_height;

  guint panel_orientation_managed;

  guint32 serial;
  gboolean supports_mirroring;
  gboolean supports_changing_layout_mode;
  gboolean global_scale_required;
  CcDisplayLayoutMode layout_mode;

  GList *monitors;
  CcDisplayMonitor *primary;

  GHashTable *logical_monitors;
};

G_DEFINE_TYPE (CcDisplayMode,
               cc_display_mode,
               G_TYPE_OBJECT)

G_DEFINE_TYPE (CcDisplayMonitor,
               cc_display_monitor,
               G_TYPE_OBJECT)

G_DEFINE_TYPE (CcDisplayLogicalMonitor,
               cc_display_logical_monitor,
               G_TYPE_OBJECT)

G_DEFINE_TYPE (CcDisplayConfig,
               cc_display_config,
               G_TYPE_OBJECT)

static const double known_diagonals[] = {
  12.1,
  13.3,
  15.6
};

static gint
logical_monitor_sort_x_axis (gconstpointer a,
                             gconstpointer b)
{
  const CcDisplayLogicalMonitor *ma = a;
  const CcDisplayLogicalMonitor *mb = b;

  return ma->x - mb->x;
}

static gint
logical_monitor_sort_y_axis (gconstpointer a,
                             gconstpointer b)
{
  const CcDisplayLogicalMonitor *ma = a;
  const CcDisplayLogicalMonitor *mb = b;

  return ma->y - mb->y;
}

static void
logical_monitor_add_x_delta (gpointer d1,
                             gpointer d2)
{
  CcDisplayLogicalMonitor *m = d1;
  int delta = GPOINTER_TO_INT (d2);

  m->x += delta;
}

static void
logical_monitor_add_y_delta (gpointer d1,
                             gpointer d2)
{
  CcDisplayLogicalMonitor *m = d1;
  int delta = GPOINTER_TO_INT (d2);

  m->y += delta;
}

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

static void
cc_display_mode_init (CcDisplayMode *self)
{
  self->supported_scales = g_array_new (FALSE, FALSE, sizeof (double));
}

static void
cc_display_mode_finalize (GObject *object)
{
  CcDisplayMode *self = CC_DISPLAY_MODE (object);

  g_free (self->id);
  g_array_free (self->supported_scales, TRUE);

  G_OBJECT_CLASS (cc_display_mode_parent_class)->finalize (object);
}

static void
cc_display_mode_class_init (CcDisplayModeClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  gobject_class->finalize = cc_display_mode_finalize;
}

gboolean
cc_display_mode_is_clone_mode (CcDisplayMode *self)
{
  return !self->id;
}

void
cc_display_mode_get_resolution (CcDisplayMode *self, int *w, int *h)
{
  if (w)
    *w = self->width;
  if (h)
    *h = self->height;
}

static gboolean
cc_display_mode_is_supported_scale (CcDisplayMode *self,
                                    double         scale)
{
  unsigned int i;

  for (i = 0; i < self->supported_scales->len; i++)
    {
      double v = g_array_index (self->supported_scales, double, i);

      if (G_APPROX_VALUE (v, scale, DBL_EPSILON))
        return TRUE;
    }
  return FALSE;
}

static gboolean
is_scale_allowed_by_active_monitors (CcDisplayConfig *self,
                                     CcDisplayMode   *mode,
                                     double           scale)
{
  GList *l;

  for (l = self->monitors; l != NULL; l = l->next)
    {
      CcDisplayMonitor *m = CC_DISPLAY_MONITOR (l->data);

      if (!cc_display_monitor_is_active (CC_DISPLAY_MONITOR (m)))
        continue;

      if (!cc_display_mode_is_supported_scale (mode, scale))
        return FALSE;
    }

  return TRUE;
}

static gboolean
is_scaled_mode_allowed (CcDisplayConfig *self,
                        CcDisplayMode   *mode,
                        double           scale)
{
  gint width, height;

  /* Do the math as if the monitor is always in landscape mode. */
  width = round (mode->width / scale);
  height = round (mode->height / scale);

  if (MAX (width, height) < self->min_width ||
      MIN (width, height) < self->min_height)
    return FALSE;

  if (!self->global_scale_required)
    return TRUE;

  return is_scale_allowed_by_active_monitors (self, CC_DISPLAY_MODE (mode), scale);
}

GArray *
cc_display_mode_get_supported_scales (CcDisplayMode *self)
{
  CcDisplayConfig *config = CC_DISPLAY_CONFIG (self->monitor->config);

  if (cc_display_config_is_cloning (config))
    {
      GArray *scales = g_array_copy (self->supported_scales);
      int i;

      for (i = scales->len - 1; i >= 0; i--)
        {
          double scale = g_array_index (scales, double, i);

          if (!is_scale_allowed_by_active_monitors (self->monitor->config,
                                                    self, scale))
            g_array_remove_index (scales, i);
        }

      return g_steal_pointer (&scales);
    }

  return g_array_ref (self->supported_scales);
}

double
cc_display_mode_get_preferred_scale (CcDisplayMode *self)
{
  return self->preferred_scale;
}

CcDisplayModeRefreshRateMode
cc_display_mode_get_refresh_rate_mode (CcDisplayMode *self)
{
  return self->refresh_rate_mode;
}

gboolean
cc_display_mode_is_interlaced (CcDisplayMode *self)
{
  return !!(self->flags & MODE_INTERLACED);
}

gboolean
cc_display_mode_is_preferred (CcDisplayMode *self)
{
  return !!(self->flags & MODE_PREFERRED);
}

int
cc_display_mode_get_freq (CcDisplayMode *self)
{
  return self->refresh_rate;
}

double
cc_display_mode_get_freq_f (CcDisplayMode *self)
{
  return self->refresh_rate;
}

static CcDisplayMode *
cc_display_mode_new_virtual (int     width,
                             int     height,
                             double  preferred_scale,
                             GArray *supported_scales)
{
  g_autoptr(GVariant) properties_variant = NULL;
  CcDisplayMode *self;

  self = g_object_new (CC_TYPE_DISPLAY_MODE, NULL);

  self->width = width;
  self->height = height;
  self->preferred_scale = preferred_scale;
  self->supported_scales = g_array_ref (supported_scales);

  return self;
}

static CcDisplayMode *
cc_display_mode_new (CcDisplayMonitor *monitor,
                     GVariant         *variant)
{
  double d;
  g_autoptr(GVariantIter) scales_iter = NULL;
  g_autoptr(GVariant) properties_variant = NULL;
  gboolean is_current;
  gboolean is_preferred;
  gboolean is_interlaced;
  char *refresh_rate_mode_str;
  CcDisplayMode *self;

  self = g_object_new (CC_TYPE_DISPLAY_MODE, NULL);
  self->monitor = monitor;

  g_variant_get (variant, "(" MODE_BASE_FORMAT "@a{sv})",
                 &self->id,
                 &self->width,
                 &self->height,
                 &self->refresh_rate,
                 &self->preferred_scale,
                 &scales_iter,
                 &properties_variant);

  while (g_variant_iter_next (scales_iter, "d", &d))
    g_array_append_val (self->supported_scales, d);

  if (!g_variant_lookup (properties_variant, "is-current", "b", &is_current))
    is_current = FALSE;
  if (!g_variant_lookup (properties_variant, "is-preferred", "b", &is_preferred))
    is_preferred = FALSE;
  if (!g_variant_lookup (properties_variant, "is-interlaced", "b", &is_interlaced))
    is_interlaced = FALSE;
  if (!g_variant_lookup (properties_variant, "refresh-rate-mode", "&s", &refresh_rate_mode_str))
    refresh_rate_mode_str = "fixed";

  if (is_current)
    self->flags |= MODE_CURRENT;
  if (is_preferred)
    self->flags |= MODE_PREFERRED;
  if (is_interlaced)
    self->flags |= MODE_INTERLACED;

  if (g_strcmp0 (refresh_rate_mode_str, "fixed") == 0)
    self->refresh_rate_mode = MODE_REFRESH_RATE_MODE_FIXED;
  else if (g_strcmp0 (refresh_rate_mode_str, "variable") == 0)
    self->refresh_rate_mode = MODE_REFRESH_RATE_MODE_VARIABLE;

  return self;
}

static void
cc_display_monitor_init (CcDisplayMonitor *self)
{
  self->ui_number = 0;
  self->is_usable = TRUE;
  self->underscanning = UNDERSCANNING_UNSUPPORTED;
  self->max_width = G_MAXINT;
  self->max_height = G_MAXINT;
}

static void
cc_display_monitor_finalize (GObject *object)
{
  CcDisplayMonitor *self = CC_DISPLAY_MONITOR (object);

  g_free (self->ui_name);
  g_free (self->ui_number_name);
  g_free (self->connector_name);
  g_free (self->vendor_name);
  g_free (self->product_name);
  g_free (self->product_serial);
  g_free (self->display_name);

  g_list_free_full (self->modes, g_object_unref);
  g_list_free (self->supported_color_modes);

  if (self->logical_monitor)
    {
      g_hash_table_remove (self->logical_monitor->monitors, self);
      g_object_unref (self->logical_monitor);
    }

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
  if (self->display_name)
    return self->display_name;

  return self->connector_name;
}

const char *
cc_display_monitor_get_connector_name (CcDisplayMonitor *self)
{
  return self->connector_name;
}

const char *
cc_display_monitor_get_vendor_name (CcDisplayMonitor *self)
{
  return self->vendor_name;
}

const char *
cc_display_monitor_get_product_name (CcDisplayMonitor *self)
{
  return self->product_name;
}

const char *
cc_display_monitor_get_product_serial (CcDisplayMonitor *self)
{
  return self->product_serial;
}

gboolean
cc_display_monitor_is_builtin (CcDisplayMonitor *self)
{
  return self->builtin;
}

gboolean
cc_display_monitor_is_primary (CcDisplayMonitor *self)
{
  if (self->logical_monitor)
    return self->logical_monitor->primary;

  return FALSE;
}

static void
cc_display_config_set_primary (CcDisplayConfig  *self,
                               CcDisplayMonitor *new_primary)
{
  if (self->primary == new_primary)
    return;

  if (!new_primary->logical_monitor)
    return;

  if (self->primary && self->primary->logical_monitor)
    {
      self->primary->logical_monitor->primary = FALSE;
      g_signal_emit_by_name (self->primary, "primary");
    }

  self->primary = new_primary;
  self->primary->logical_monitor->primary = TRUE;

  g_signal_emit_by_name (self->primary, "primary");
  g_signal_emit_by_name (self, "primary");
}

static void
cc_display_config_unset_primary (CcDisplayConfig  *self,
                                 CcDisplayMonitor *old_primary)
{
  GList *l;

  if (self->primary != old_primary)
    return;

  for (l = self->monitors; l; l = l->next)
    {
      CcDisplayMonitor *monitor = l->data;
      if (monitor->logical_monitor &&
          monitor != old_primary)
        {
          cc_display_config_set_primary (self, monitor);
          break;
        }
    }

  if (self->primary == old_primary)
    self->primary = NULL;
}

void
cc_display_monitor_set_primary (CcDisplayMonitor *self,
                                gboolean          primary)
{
  if (primary)
    cc_display_config_set_primary (self->config, self);
  else
    cc_display_config_unset_primary (self->config, self);
}

gboolean
cc_display_monitor_is_active (CcDisplayMonitor *self)
{
  return self->logical_monitor != NULL;
}

static void
cc_display_monitor_set_logical_monitor (CcDisplayMonitor        *self,
                                        CcDisplayLogicalMonitor *logical_monitor)
{
  gboolean was_primary = FALSE;

  if (self->logical_monitor)
    {
      was_primary = self->logical_monitor->primary;
      if (was_primary)
        cc_display_config_unset_primary (self->config, self);
      g_hash_table_remove (self->logical_monitor->monitors, self);
      g_object_unref (self->logical_monitor);
    }

  self->logical_monitor = logical_monitor;

  if (self->logical_monitor)
    {
      g_hash_table_add (self->logical_monitor->monitors, self);
      g_object_ref (self->logical_monitor);
      /* unset primary with NULL will select this monitor if it is the only one.*/
      if (was_primary)
        cc_display_config_set_primary (self->config, self);
      else
        cc_display_config_unset_primary (self->config, NULL);
    }
}

static void
remove_logical_monitor (gpointer  data,
                        GObject  *object)
{
  CcDisplayConfig *self = CC_DISPLAY_CONFIG (data);

  g_hash_table_remove (self->logical_monitors, object);
}

static void
register_logical_monitor (CcDisplayConfig         *self,
                          CcDisplayLogicalMonitor *logical_monitor)
{
  g_hash_table_add (self->logical_monitors, logical_monitor);
  g_object_weak_ref (G_OBJECT (logical_monitor), remove_logical_monitor, self);
  g_object_unref (logical_monitor);
}

static gboolean
logical_monitor_is_rotated (CcDisplayLogicalMonitor *lm)
{
  switch (lm->rotation)
    {
    case CC_DISPLAY_ROTATION_90:
    case CC_DISPLAY_ROTATION_270:
    case CC_DISPLAY_ROTATION_90_FLIPPED:
    case CC_DISPLAY_ROTATION_270_FLIPPED:
      return TRUE;
    default:
      return FALSE;
    }
}

static int
logical_monitor_width (CcDisplayLogicalMonitor *lm)
{
  CcDisplayMonitor *monitor;
  CcDisplayMode *mode;
  GHashTableIter iter;
  int width;

  g_hash_table_iter_init (&iter, lm->monitors);
  g_hash_table_iter_next (&iter, (void **) &monitor, NULL);
  mode = CC_DISPLAY_MODE (monitor->current_mode);
  if (logical_monitor_is_rotated (lm))
    width = mode ? mode->height : 0;
  else
    width = mode ? mode->width : 0;

  if (monitor->config->layout_mode == CC_DISPLAY_LAYOUT_MODE_LOGICAL)
    return round (width / lm->scale);
  else
    return width;
}


static void
cc_display_config_append_right (CcDisplayConfig         *self,
                                CcDisplayLogicalMonitor *monitor)
{
  GList *x_axis;
  CcDisplayLogicalMonitor *last;

  if (g_hash_table_size (self->logical_monitors) == 0)
    {
      monitor->x = 0;
      monitor->y = 0;
      return;
    }

  x_axis = g_hash_table_get_keys (self->logical_monitors);
  x_axis = g_list_sort (x_axis, logical_monitor_sort_x_axis);
  last = g_list_last (x_axis)->data;
  monitor->x = last->x + logical_monitor_width (last);
  monitor->y = last->y;

  g_list_free (x_axis);
}

void
cc_display_monitor_set_active (CcDisplayMonitor *self, gboolean active)
{
  if (!self->current_mode && active)
    {
      if (self->preferred_mode)
        self->current_mode = self->preferred_mode;
      else if (self->modes)
        self->current_mode = (CcDisplayMode *) self->modes->data;
      else
        g_warning ("Couldn't find a mode to activate monitor at %s", self->connector_name);
    }

  if (!self->logical_monitor && active)
    {
      CcDisplayLogicalMonitor *logical_monitor;

      logical_monitor = g_object_new (CC_TYPE_DISPLAY_LOGICAL_MONITOR, NULL);
      cc_display_monitor_set_logical_monitor (self, logical_monitor);
      cc_display_config_append_right (self->config, logical_monitor);
      register_logical_monitor (self->config, logical_monitor);
    }
  else if (self->logical_monitor && !active)
    {
      cc_display_monitor_set_logical_monitor (self, NULL);
    }

  g_signal_emit_by_name (self, "active");
}

CcDisplayRotation
cc_display_monitor_get_rotation (CcDisplayMonitor *self)
{
  if (self->logical_monitor)
    return self->logical_monitor->rotation;

  return CC_DISPLAY_ROTATION_NONE;
}

void
cc_display_monitor_set_rotation (CcDisplayMonitor  *self,
                                 CcDisplayRotation  rotation)
{
  if (!self->logical_monitor)
    return;

  if (self->logical_monitor->rotation != rotation)
    {
      self->logical_monitor->rotation = rotation;

      g_signal_emit_by_name (self, "rotation");
    }
}

gboolean
cc_display_monitor_supports_rotation (CcDisplayMonitor *self, CcDisplayRotation r)
{
  return TRUE;
}

void
cc_display_monitor_get_physical_size (CcDisplayMonitor *self, int *w, int *h)
{
  if (w)
    *w = self->width_mm;
  if (h)
    *h = self->height_mm;
}

void
cc_display_monitor_get_geometry (CcDisplayMonitor *self, int *x, int *y, int *w, int *h)
{
  CcDisplayMode *mode = NULL;

  if (self->logical_monitor)
    {
      if (x)
        *x = self->logical_monitor->x;
      if (y)
        *y = self->logical_monitor->y;
    }
  else
    {
      if (x)
        *x = -1;
      if (y)
        *y = -1;
    }

  if (self->current_mode)
    mode = self->current_mode;
  else if (self->preferred_mode)
    mode = self->preferred_mode;
  else if (self->modes)
    mode = CC_DISPLAY_MODE (self->modes->data);

  if (mode)
    cc_display_mode_get_resolution (mode, w, h);
  else
    {
      g_warning ("Monitor at %s has no modes?", self->connector_name);
      if (w)
        *w = -1;
      if (h)
        *h = -1;
    }
}

int
cc_display_monitor_get_min_freq (CcDisplayMonitor *self)
{
  return self->min_refresh_rate;
}

CcDisplayMode *
cc_display_monitor_get_mode (CcDisplayMonitor *self)
{
  return self->current_mode;
}

CcDisplayMode *
cc_display_monitor_get_preferred_mode (CcDisplayMonitor *self)
{
  return self->preferred_mode;
}

guint32
cc_display_monitor_get_id (CcDisplayMonitor *self)
{
  return 0;
}

GList *
cc_display_monitor_get_modes (CcDisplayMonitor *self)
{
  return self->modes;
}

gboolean
cc_display_monitor_supports_variable_refresh_rate (CcDisplayMonitor *self)
{
  return self->supports_variable_refresh_rate;
}

GList *
cc_display_monitor_get_supported_color_modes (CcDisplayMonitor *self)
{
  return self->supported_color_modes;
}

gboolean
cc_display_monitor_supports_color_mode (CcDisplayMonitor   *self,
                                        CcDisplayColorMode  color_mode)
{
  GList *supported_color_modes =
    cc_display_monitor_get_supported_color_modes (self);

  return !!g_list_find (supported_color_modes, GUINT_TO_POINTER (color_mode));
}

CcDisplayColorMode
cc_display_monitor_get_color_mode (CcDisplayMonitor *self)
{
  return self->color_mode;
}

void
cc_display_monitor_set_color_mode (CcDisplayMonitor   *self,
                                   CcDisplayColorMode  color_mode)
{
  self->color_mode = color_mode;
}

gboolean
cc_display_monitor_supports_underscanning (CcDisplayMonitor *self)
{
  return self->underscanning != UNDERSCANNING_UNSUPPORTED;
}

gboolean
cc_display_monitor_get_underscanning (CcDisplayMonitor *self)
{
  return self->underscanning == UNDERSCANNING_ENABLED;
}

void
cc_display_monitor_set_underscanning (CcDisplayMonitor *self,
                                      gboolean underscanning)
{
  if (self->underscanning == UNDERSCANNING_UNSUPPORTED)
    return;

  if (underscanning)
    self->underscanning = UNDERSCANNING_ENABLED;
  else
    self->underscanning = UNDERSCANNING_DISABLED;
}

CcDisplayMonitorPrivacy
cc_display_monitor_get_privacy (CcDisplayMonitor *self)
{
  return self->privacy_screen;
}

static CcDisplayMode *
cc_display_monitor_get_closest_mode (CcDisplayMonitor             *self,
                                     int                           width,
                                     int                           height,
                                     double                        refresh_rate,
                                     CcDisplayModeRefreshRateMode  refresh_rate_mode,
                                     guint32                       flags)
{
  CcDisplayMode *best = NULL;
  GList *l;

  for (l = self->modes; l != NULL; l = l->next)
    {
      CcDisplayMode *similar = l->data;

      if (similar->width != width ||
          similar->height != height ||
          similar->refresh_rate_mode != refresh_rate_mode)
        continue;

      if (similar->refresh_rate == refresh_rate &&
          (similar->flags & MODE_INTERLACED) == (flags & MODE_INTERLACED))
        {
          best = similar;
          break;
        }

      /* There might be a better heuristic. */
      if (!best || best->refresh_rate < similar->refresh_rate)
        {
          best = similar;
          continue;
        }
    }

  return CC_DISPLAY_MODE (best);
}

void
cc_display_monitor_set_mode (CcDisplayMonitor *self,
                             CcDisplayMode    *new_mode)
{
  CcDisplayMode *mode;

  g_return_if_fail (new_mode != NULL);

  mode = cc_display_monitor_get_closest_mode (self,
                                              new_mode->width,
                                              new_mode->height,
                                              new_mode->refresh_rate,
                                              new_mode->refresh_rate_mode,
                                              new_mode->flags);

  self->current_mode = mode;

  if (!cc_display_mode_is_supported_scale (mode,
                                           cc_display_monitor_get_scale (self)))
    {
      cc_display_monitor_set_scale (self,
                                    cc_display_mode_get_preferred_scale (mode));
    }

  g_signal_emit_by_name (self, "mode");
}

void
cc_display_monitor_set_compatible_clone_mode (CcDisplayMonitor *self,
                                              CcDisplayMode    *clone_mode)
{
  GList *l;
  CcDisplayMode *best_mode = NULL;
  int clone_width, clone_height;

  g_return_if_fail (cc_display_mode_is_clone_mode (clone_mode));

  cc_display_mode_get_resolution (clone_mode, &clone_width, &clone_height);

  for (l = self->modes; l; l = l->next)
    {
      CcDisplayMode *mode = l->data;
      int width, height;

      cc_display_mode_get_resolution (mode, &width, &height);
      if (width != clone_width || height != clone_height)
        continue;

      if (!best_mode)
        {
          best_mode = mode;
          continue;
        }

      if (cc_display_mode_get_freq_f (mode) >
          cc_display_mode_get_freq_f (best_mode))
        best_mode = mode;
    }

  g_return_if_fail (best_mode);

  cc_display_monitor_set_mode (CC_DISPLAY_MONITOR (self), best_mode);
}

void
cc_display_monitor_set_refresh_rate_mode (CcDisplayMonitor             *self,
                                          CcDisplayModeRefreshRateMode  refresh_rate_mode)
{
  CcDisplayMode *current_mode = self->current_mode;
  CcDisplayMode *new_mode;

  g_return_if_fail (current_mode != NULL);

  new_mode = cc_display_monitor_get_closest_mode (self,
                                                  current_mode->width,
                                                  current_mode->height,
                                                  current_mode->refresh_rate,
                                                  refresh_rate_mode,
                                                  current_mode->flags);

  g_return_if_fail (new_mode != NULL);

  cc_display_monitor_set_mode (CC_DISPLAY_MONITOR (self), new_mode);
}

void
cc_display_monitor_set_position (CcDisplayMonitor *self, int x, int y)
{
  if (self->logical_monitor)
    {
      gboolean notify = FALSE;

      if (self->logical_monitor->x != x || self->logical_monitor->y != y)
        notify = TRUE;

      self->logical_monitor->x = x;
      self->logical_monitor->y = y;

      if (notify)
        g_signal_emit_by_name (self, "position-changed");
    }
}

double
cc_display_monitor_get_scale (CcDisplayMonitor *self)
{
  if (self->logical_monitor)
    return self->logical_monitor->scale;

  return 1.0;
}

void
cc_display_monitor_set_scale (CcDisplayMonitor *self,
                              double            scale)
{
  if (!self->current_mode)
    return;

  if (!cc_display_mode_is_supported_scale (self->current_mode, scale))
    return;

  if (!self->logical_monitor)
    return;

  if (!G_APPROX_VALUE (self->logical_monitor->scale, scale, DBL_EPSILON))
    {
      self->logical_monitor->scale = scale;

      g_signal_emit_by_name (self, "scale");
    }
}

gboolean
cc_display_monitor_is_useful (CcDisplayMonitor *self)
{
  return self->is_usable &&
         cc_display_monitor_is_active (self);
}

gboolean
cc_display_monitor_is_usable (CcDisplayMonitor *self)
{
  return self->is_usable;
}

void
cc_display_monitor_set_usable (CcDisplayMonitor *self,
                               gboolean          is_usable)
{
  self->is_usable = is_usable;

  g_signal_emit_by_name (self, "is-usable");
}

gint
cc_display_monitor_get_ui_number (CcDisplayMonitor *self)
{
  return self->ui_number;
}

const char *
cc_display_monitor_get_ui_name (CcDisplayMonitor *self)
{
  return self->ui_name;
}

const char *
cc_display_monitor_get_ui_number_name (CcDisplayMonitor *self)
{
  return self->ui_number_name;
}

char *
cc_display_monitor_dup_ui_number_name (CcDisplayMonitor *self)
{
  return g_strdup (self->ui_number_name);
}

static void
cc_display_monitor_set_ui_info (CcDisplayMonitor *self, gint ui_number, gchar *ui_name)
{
  self->ui_number = ui_number;
  g_free (self->ui_name);
  self->ui_name = ui_name;
  g_free (self->ui_number_name);
  self->ui_number_name = g_strdup_printf ("%d\u2003%s", ui_number, ui_name);
}

static void
update_panel_orientation_managed (CcDisplayConfig *self)
{
  g_autoptr(GVariant) v = NULL;
  gboolean panel_orientation_managed = FALSE;

  if (self->proxy != NULL)
    {
      v = g_dbus_proxy_get_cached_property (self->proxy, "PanelOrientationManaged");
      if (v)
        {
          panel_orientation_managed = g_variant_get_boolean (v);
        }
    }

  if (panel_orientation_managed == self->panel_orientation_managed)
    return;

  self->panel_orientation_managed = panel_orientation_managed;
  g_signal_emit_by_name (self, "panel-orientation-managed", self->panel_orientation_managed);
}

static void
proxy_properties_changed_cb (CcDisplayConfig *self,
                             GVariant        *changed_properties,
                             GStrv            invalidated_properties)
{
  GVariantDict dict;

  g_variant_dict_init (&dict, changed_properties);

  if (g_variant_dict_contains (&dict, "PanelOrientationManaged"))
    update_panel_orientation_managed (self);
}

static void
apply_global_scale_requirement (CcDisplayConfig  *self,
                                CcDisplayMonitor *monitor)
{
  GList *l;
  double scale = cc_display_monitor_get_scale (monitor);

  for (l = self->monitors; l != NULL; l = l->next)
    {
      CcDisplayMonitor *m = l->data;
      if (m != monitor)
        cc_display_monitor_set_scale (m, scale);
    }
}

static void
construct_modes (CcDisplayMonitor *self,
                 GVariantIter     *modes)
{
  CcDisplayMode *mode;

  while (TRUE)
    {
      g_autoptr(GVariant) variant = NULL;

      if (!g_variant_iter_next (modes, "@"MODE_FORMAT, &variant))
        break;

      mode = cc_display_mode_new (self, variant);
      self->modes = g_list_prepend (self->modes, mode);

      if (mode->flags & MODE_PREFERRED)
        self->preferred_mode = CC_DISPLAY_MODE (mode);
      if (mode->flags & MODE_CURRENT)
        self->current_mode = CC_DISPLAY_MODE (mode);

      if (mode->refresh_rate_mode == MODE_REFRESH_RATE_MODE_VARIABLE)
        self->supports_variable_refresh_rate = TRUE;
    }

  self->modes = g_list_reverse (self->modes);
}

static CcDisplayMonitor *
cc_display_monitor_new (GVariant        *variant,
                        CcDisplayConfig *config)
{
  CcDisplayMonitor *self;
  gchar *s1, *s2, *s3, *s4;
  g_autoptr(GVariantIter) modes = NULL;
  g_autoptr(GVariantIter) props = NULL;

  self = g_object_new (CC_TYPE_DISPLAY_MONITOR, NULL);
  self->config = config;

  g_variant_get (variant, MONITOR_FORMAT,
                 &s1, &s2, &s3, &s4, &modes, &props);
  self->connector_name = s1;
  self->vendor_name = s2;
  self->product_name = s3;
  self->product_serial = s4;

  construct_modes (self, modes);

  while (TRUE)
    {
      const char *s;
      g_autoptr(GVariant) v = NULL;

      if (!g_variant_iter_next (props, "{&sv}", &s, &v))
        break;

      if (g_str_equal (s, "width-mm"))
        {
          g_variant_get (v, "i", &self->width_mm);
        }
      else if (g_str_equal (s, "height-mm"))
        {
          g_variant_get (v, "i", &self->height_mm);
        }
      else if (g_str_equal (s, "is-underscanning"))
        {
          gboolean underscanning = FALSE;
          g_variant_get (v, "b", &underscanning);
          if (underscanning)
            self->underscanning = UNDERSCANNING_ENABLED;
          else
            self->underscanning = UNDERSCANNING_DISABLED;
        }
      else if (g_str_equal (s, "max-screen-size"))
        {
          g_variant_get (v, "ii", &self->max_width, &self->max_height);
        }
      else if (g_str_equal (s, "is-builtin"))
        {
          g_variant_get (v, "b", &self->builtin);
        }
      else if (g_str_equal (s, "display-name"))
        {
          g_variant_get (v, "s", &self->display_name);
        }
      else if (g_str_equal (s, "privacy-screen-state"))
        {
          gboolean enabled;
          gboolean locked;
          g_variant_get (v, "(bb)", &enabled, &locked);

          if (enabled)
            self->privacy_screen = CC_DISPLAY_MONITOR_PRIVACY_ENABLED;
          else
            self->privacy_screen = CC_DISPLAY_MONITOR_PRIVACY_DISABLED;

          if (locked)
            self->privacy_screen |= CC_DISPLAY_MONITOR_PRIVACY_LOCKED;
        }
      else if (g_str_equal (s, "min-refresh-rate"))
        {
          g_variant_get (v, "i", &self->min_refresh_rate);
        }
      else if (g_str_equal (s, "color-mode"))
        {
          guint32 color_mode;

          g_variant_get (v, "u", &color_mode);
          self->color_mode = color_mode;
        }
      else if (g_str_equal (s, "supported-color-modes"))
        {
          g_autoptr (GVariantIter) iter = NULL;
          guint32 color_mode;

          g_variant_get (v, "au", &iter);
          while (g_variant_iter_next (iter, "u", &color_mode))
            {
              self->supported_color_modes =
                g_list_append (self->supported_color_modes,
                               GUINT_TO_POINTER (color_mode));
            }
        }
    }

  return self;
}

static gboolean
cc_display_logical_monitor_equal (const CcDisplayLogicalMonitor *m1,
                                  const CcDisplayLogicalMonitor *m2)
{
  if (!m1 && !m2)
    return TRUE;
  else if (!m1 || !m2)
    return FALSE;

  return m1->x == m2->x &&
    m1->y == m2->y &&
    G_APPROX_VALUE (m1->scale, m2->scale, DBL_EPSILON) &&
    m1->rotation == m2->rotation &&
    m1->primary == m2->primary;
}

static void
cc_display_logical_monitor_finalize (GObject *object)
{
  CcDisplayLogicalMonitor *self = CC_DISPLAY_LOGICAL_MONITOR (object);

  g_warn_if_fail (g_hash_table_size (self->monitors) == 0);
  g_clear_pointer (&self->monitors, g_hash_table_destroy);

  G_OBJECT_CLASS (cc_display_logical_monitor_parent_class)->finalize (object);
}

static void
cc_display_logical_monitor_class_init (CcDisplayLogicalMonitorClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  gobject_class->finalize = cc_display_logical_monitor_finalize;
}

static void
cc_display_logical_monitor_init (CcDisplayLogicalMonitor *self)
{
  self->scale = 1.0;
  self->monitors = g_hash_table_new (NULL, NULL);
}

static CcDisplayMonitor *
monitor_from_spec (CcDisplayConfig *self,
                   const char      *connector,
                   const char      *vendor,
                   const char      *product,
                   const char      *serial)
{
  GList *l;

  for (l = self->monitors; l; l = l->next)
    {
      CcDisplayMonitor *m = l->data;

      if (g_str_equal (m->connector_name, connector) &&
          g_str_equal (m->vendor_name, vendor) &&
          g_str_equal (m->product_name, product) &&
          g_str_equal (m->product_serial, serial))
        return m;
    }

  return NULL;
}

static void
construct_monitors (CcDisplayConfig *self,
                    GVariantIter    *monitors,
                    GVariantIter    *logical_monitors)
{
  while (TRUE)
    {
      CcDisplayMonitor *monitor;
      g_autoptr(GVariant) variant = NULL;

      if (!g_variant_iter_next (monitors, "@"MONITOR_FORMAT, &variant))
        break;

      monitor = cc_display_monitor_new (variant, self);
      self->monitors = g_list_prepend (self->monitors, monitor);

      if (self->global_scale_required)
        g_signal_connect_object (monitor, "scale",
                                 G_CALLBACK (apply_global_scale_requirement),
                                 self, G_CONNECT_SWAPPED);
    }

  self->monitors = g_list_reverse (self->monitors);

  while (TRUE)
    {
      g_autoptr(GVariant) variant = NULL;
      CcDisplayLogicalMonitor *logical_monitor;
      g_autoptr(GVariantIter) monitor_specs = NULL;
      const gchar *s1, *s2, *s3, *s4;
      gboolean primary;

      if (!g_variant_iter_next (logical_monitors, "@"LOGICAL_MONITOR_FORMAT, &variant))
        break;

      logical_monitor = g_object_new (CC_TYPE_DISPLAY_LOGICAL_MONITOR, NULL);
      g_variant_get (variant, LOGICAL_MONITOR_FORMAT,
                     &logical_monitor->x,
                     &logical_monitor->y,
                     &logical_monitor->scale,
                     &logical_monitor->rotation,
                     &primary,
                     &monitor_specs,
                     NULL);

      while (g_variant_iter_next (monitor_specs, "(&s&s&s&s)", &s1, &s2, &s3, &s4))
        {
          CcDisplayMonitor *m = monitor_from_spec (self, s1, s2, s3, s4);
          if (!m)
            {
              g_warning ("Couldn't find monitor given spec: %s, %s, %s, %s",
                         s1, s2, s3, s4);
              continue;
            }

          cc_display_monitor_set_logical_monitor (m, logical_monitor);
        }

      if (g_hash_table_size (logical_monitor->monitors) > 0)
        {
          if (primary)
            {
              CcDisplayMonitor *m = NULL;
              GHashTableIter iter;
              g_hash_table_iter_init (&iter, logical_monitor->monitors);
              g_hash_table_iter_next (&iter, (void **) &m, NULL);

              cc_display_config_set_primary (self, m);
            }
        }
      else
        {
          g_warning ("Got an empty logical monitor, ignoring");
        }

      register_logical_monitor (self, logical_monitor);
    }
}

static void
filter_out_invalid_scaled_modes (CcDisplayConfig *self)
{
  GList *l;

  for (l = self->monitors; l; l = l->next)
    {
      CcDisplayMonitor *monitor = l->data;
      GList *ll = monitor->modes;

      while (ll != NULL)
        {
          CcDisplayMode *mode = ll->data;
          GList *current = ll;
          double current_scale = -1;
          int i;

          ll = ll->next;

          if (monitor->current_mode != CC_DISPLAY_MODE (mode) &&
              monitor->preferred_mode != CC_DISPLAY_MODE (mode) &&
              !is_scaled_mode_allowed (self, mode, 1.0))
            {
              g_clear_object (&mode);
              monitor->modes = g_list_delete_link (monitor->modes, current);
              continue;
            }

          if (monitor->current_mode == CC_DISPLAY_MODE (mode))
            current_scale = cc_display_monitor_get_scale (CC_DISPLAY_MONITOR (monitor));

          for (i = mode->supported_scales->len - 1; i >= 0; i--)
            {
              float scale = g_array_index (mode->supported_scales, double, i);

              if (!G_APPROX_VALUE (scale, current_scale, DBL_EPSILON) &&
                  !G_APPROX_VALUE (scale, mode->preferred_scale, DBL_EPSILON) &&
                  !is_scaled_mode_allowed (self, mode, scale))
                {
                  g_array_remove_index (mode->supported_scales, i);
                }
            }
        }
    }
}

static void
cc_display_config_constructed (GObject *object)
{
  CcDisplayConfig *self = CC_DISPLAY_CONFIG (object);
  GList *monitors;
  GList *l;
  g_autoptr(GVariantIter) monitors_iter = NULL;
  g_autoptr(GVariantIter) logical_monitors_iter = NULL;
  g_autoptr(GVariantIter) props_iter = NULL;
  g_autoptr(GError) error = NULL;

  g_variant_get (self->state,
                 CURRENT_STATE_FORMAT,
                 &self->serial,
                 &monitors_iter,
                 &logical_monitors_iter,
                 &props_iter);

  while (TRUE)
    {
      const char *s;
      g_autoptr(GVariant) v = NULL;

      if (!g_variant_iter_next (props_iter, "{&sv}", &s, &v))
	break;

      if (g_str_equal (s, "supports-mirroring"))
        {
          g_variant_get (v, "b", &self->supports_mirroring);
        }
      else if (g_str_equal (s, "supports-changing-layout-mode"))
        {
          g_variant_get (v, "b", &self->supports_changing_layout_mode);
        }
      else if (g_str_equal (s, "global-scale-required"))
        {
          g_variant_get (v, "b", &self->global_scale_required);
        }
      else if (g_str_equal (s, "layout-mode"))
        {
          guint32 u = 0;
          g_variant_get (v, "u", &u);
          if (u >= CC_DISPLAY_LAYOUT_MODE_LOGICAL &&
              u <= CC_DISPLAY_LAYOUT_MODE_PHYSICAL)
            self->layout_mode = u;
        }
    }

  construct_monitors (self, monitors_iter, logical_monitors_iter);
  filter_out_invalid_scaled_modes (self);

  self->proxy = g_dbus_proxy_new_sync (self->connection,
                                       G_DBUS_PROXY_FLAGS_NONE,
                                       NULL,
                                       "org.gnome.Mutter.DisplayConfig",
                                       "/org/gnome/Mutter/DisplayConfig",
                                       "org.gnome.Mutter.DisplayConfig",
                                       NULL,
                                       &error);
  if (error)
    g_warning ("Could not create DisplayConfig proxy: %s", error->message);

  g_signal_connect_swapped (self->proxy, "g-properties-changed",
                            G_CALLBACK (proxy_properties_changed_cb), self);
  update_panel_orientation_managed (self);

  monitors = cc_display_config_get_monitors (self);
  for (l = monitors; l; l = l->next)
    {
      CcDisplayMonitor *monitor = l->data;

      if (cc_display_monitor_is_builtin (monitor))
        {
          self->ui_sorted_monitors = g_list_prepend (self->ui_sorted_monitors,
                                                     monitor);
        }
      else
        {
          self->ui_sorted_monitors = g_list_append (self->ui_sorted_monitors,
                                                    monitor);
        }
    }

  cc_display_config_update_ui_numbers_names(self);
}

static void
cc_display_config_set_property (GObject      *object,
                                guint         prop_id,
                                const GValue *value,
                                GParamSpec   *pspec)
{
  CcDisplayConfig *self = CC_DISPLAY_CONFIG (object);

  switch (prop_id)
    {
    case PROP_STATE:
      self->state = g_value_dup_variant (value);
      break;
    case PROP_CONNECTION:
      self->connection = g_value_dup_object (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
cc_display_config_get_property (GObject    *object,
                                guint       prop_id,
                                GValue     *value,
                                GParamSpec *pspec)
{
  CcDisplayConfig *self = CC_DISPLAY_CONFIG (object);

  switch (prop_id)
    {
    case PROP_STATE:
      g_value_set_variant (value, self->state);
      break;
    case PROP_CONNECTION:
      g_value_set_object (value, self->connection);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
cc_display_config_dispose (GObject *object)
{
  CcDisplayConfig *self = CC_DISPLAY_CONFIG (object);

  if (self->logical_monitors)
    {
      GHashTableIter iter;
      gpointer monitor;

      g_hash_table_iter_init (&iter, self->logical_monitors);

      while (g_hash_table_iter_next (&iter, &monitor, NULL))
        g_object_weak_unref (G_OBJECT (monitor), remove_logical_monitor, self);
    }

  G_OBJECT_CLASS (cc_display_config_parent_class)->dispose (object);
}

static void
cc_display_config_finalize (GObject *object)
{
  CcDisplayConfig *self = CC_DISPLAY_CONFIG (object);

  g_list_free (self->ui_sorted_monitors);

  g_clear_pointer (&self->state, g_variant_unref);
  g_clear_object (&self->connection);
  g_clear_object (&self->proxy);

  g_clear_list (&self->monitors, g_object_unref);
  g_clear_pointer (&self->logical_monitors, g_hash_table_destroy);

  G_OBJECT_CLASS (cc_display_config_parent_class)->finalize (object);
}

static void
cc_display_config_class_init (CcDisplayConfigClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  GParamSpec *pspec;

  gobject_class->set_property = cc_display_config_set_property;
  gobject_class->get_property = cc_display_config_get_property;
  gobject_class->finalize = cc_display_config_finalize;
  gobject_class->constructed = cc_display_config_constructed;
  gobject_class->dispose = cc_display_config_dispose;

  pspec = g_param_spec_variant ("state",
                                "GVariant",
                                "GVariant",
                                G_VARIANT_TYPE (CURRENT_STATE_FORMAT),
                                NULL,
                                G_PARAM_READWRITE |
                                G_PARAM_STATIC_STRINGS |
                                G_PARAM_CONSTRUCT_ONLY);
  g_object_class_install_property (gobject_class, PROP_STATE, pspec);

  pspec = g_param_spec_object ("connection",
                               "GDBusConnection",
                               "GDBusConnection",
                                G_TYPE_DBUS_CONNECTION,
                                G_PARAM_READWRITE |
                                G_PARAM_STATIC_STRINGS |
                                G_PARAM_CONSTRUCT_ONLY);
  g_object_class_install_property (gobject_class, PROP_CONNECTION, pspec);

  g_signal_new ("primary",
                CC_TYPE_DISPLAY_CONFIG,
                G_SIGNAL_RUN_LAST,
                0, NULL, NULL, NULL,
                G_TYPE_NONE, 0);
  g_signal_new ("panel-orientation-managed",
                CC_TYPE_DISPLAY_CONFIG,
                G_SIGNAL_RUN_LAST,
                0, NULL, NULL, NULL,
                G_TYPE_NONE, 1, G_TYPE_BOOLEAN);
}

static void
cc_display_config_init (CcDisplayConfig *self)
{
  self->serial = 0;
  self->supports_mirroring = TRUE;
  self->supports_changing_layout_mode = FALSE;
  self->global_scale_required = FALSE;
  self->layout_mode = CC_DISPLAY_LAYOUT_MODE_LOGICAL;
  self->logical_monitors = g_hash_table_new (NULL, NULL);
}

GList *
cc_display_config_get_monitors (CcDisplayConfig *self)
{
  g_return_val_if_fail (CC_IS_DISPLAY_CONFIG (self), NULL);

  return self->monitors;
}

GList *
cc_display_config_get_ui_sorted_monitors (CcDisplayConfig *self)
{
  g_return_val_if_fail (CC_IS_DISPLAY_CONFIG (self), NULL);

  return self->ui_sorted_monitors;
}

int
cc_display_config_count_useful_monitors (CcDisplayConfig *self)
{
  GList *outputs, *l;
  guint count = 0;

  g_return_val_if_fail (CC_IS_DISPLAY_CONFIG (self), 0);

  outputs = self->ui_sorted_monitors;
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

static void
cc_display_config_ensure_non_offset_coords (CcDisplayConfig *self)
{
  GList *x_axis, *y_axis;
  CcDisplayLogicalMonitor *m;

  if (g_hash_table_size (self->logical_monitors) == 0)
    return;

  x_axis = g_hash_table_get_keys (self->logical_monitors);
  x_axis = g_list_sort (x_axis, logical_monitor_sort_x_axis);
  y_axis = g_hash_table_get_keys (self->logical_monitors);
  y_axis = g_list_sort (y_axis, logical_monitor_sort_y_axis);

  m = x_axis->data;
  if (m->x != 0)
    {
      g_list_foreach (x_axis, logical_monitor_add_x_delta,
                      GINT_TO_POINTER (- m->x));
    }

  m = y_axis->data;
  if (m->y != 0)
    {
      g_list_foreach (y_axis, logical_monitor_add_y_delta,
                      GINT_TO_POINTER (- m->y));
    }

  g_list_free (x_axis);
  g_list_free (y_axis);
}

static GVariant *
build_monitors_variant (GHashTable *monitors)
{
  GVariantBuilder builder;
  GHashTableIter iter;
  CcDisplayMonitor *monitor;

  g_variant_builder_init (&builder, G_VARIANT_TYPE_ARRAY);
  g_hash_table_iter_init (&iter, monitors);

  while (g_hash_table_iter_next (&iter, (void **) &monitor, NULL))
    {
      GVariantBuilder props_builder;
      CcDisplayMode *mode;

      if (!monitor->current_mode)
        continue;

      g_variant_builder_init (&props_builder, G_VARIANT_TYPE ("a{sv}"));
      g_variant_builder_add (&props_builder, "{sv}",
                             "color-mode",
                             g_variant_new_uint32 (monitor->color_mode));
      g_variant_builder_add (&props_builder, "{sv}",
                             "underscanning",
                             g_variant_new_boolean (monitor->underscanning ==
                                                    UNDERSCANNING_ENABLED));

      mode = CC_DISPLAY_MODE (monitor->current_mode);
      g_variant_builder_add (&builder, "(ss@*)",
                             monitor->connector_name,
                             mode->id,
                             g_variant_builder_end (&props_builder));
    }

  return g_variant_builder_end (&builder);
}

static GVariant *
build_logical_monitors_parameter (CcDisplayConfig *self)
{
  GVariantBuilder builder;
  GHashTableIter iter;
  CcDisplayLogicalMonitor *logical_monitor;

  g_variant_builder_init (&builder, G_VARIANT_TYPE ("a(iiduba(ssa{sv}))"));
  g_hash_table_iter_init (&iter, self->logical_monitors);

  while (g_hash_table_iter_next (&iter, (void **) &logical_monitor, NULL))
    g_variant_builder_add (&builder, "(iidub@*)",
                           logical_monitor->x,
                           logical_monitor->y,
                           logical_monitor->scale,
                           logical_monitor->rotation,
                           logical_monitor->primary,
                           build_monitors_variant (logical_monitor->monitors));

  return g_variant_builder_end (&builder);
}

static GVariant *
build_apply_parameters (CcDisplayConfig       *self,
                        CcDisplayConfigMethod  method)
{
  GVariantBuilder props_builder;
  g_variant_builder_init (&props_builder, G_VARIANT_TYPE ("a{sv}"));

  if (self->supports_changing_layout_mode)
    g_variant_builder_add (&props_builder, "{sv}",
                           "layout-mode", g_variant_new_uint32 (self->layout_mode));

  return g_variant_new ("(uu@*@*)",
                        self->serial,
                        method,
                        build_logical_monitors_parameter (self),
                        g_variant_builder_end (&props_builder));
}

static gboolean
config_apply (CcDisplayConfig        *self,
              CcDisplayConfigMethod   method,
              GError                **error)
{
  g_autoptr(GVariant) retval = NULL;

  cc_display_config_ensure_non_offset_coords (self);

  retval = g_dbus_proxy_call_sync (self->proxy,
                                   "ApplyMonitorsConfig",
                                   build_apply_parameters (self, method),
                                   G_DBUS_CALL_FLAGS_NO_AUTO_START,
                                   -1,
                                   NULL,
                                   error);
  return retval != NULL;
}

gboolean
cc_display_config_is_applicable (CcDisplayConfig *self)
{
  g_autoptr(GError) error = NULL;

  g_return_val_if_fail (CC_IS_DISPLAY_CONFIG (self), FALSE);

  if (!config_apply (self, CC_DISPLAY_CONFIG_METHOD_VERIFY, &error))
    {
      g_warning ("Config not applicable: %s", error->message);
      return FALSE;
    }
  else
    {
      return TRUE;
    }
}

void
cc_display_config_set_mode_on_all_outputs (CcDisplayConfig *config,
                                           CcDisplayMode   *clone_mode)
{
  GList *outputs, *l;

  g_return_if_fail (CC_IS_DISPLAY_CONFIG (config));
  g_return_if_fail (cc_display_mode_is_clone_mode (clone_mode));

  outputs = cc_display_config_get_monitors (config);
  for (l = outputs; l; l = l->next)
    {
      CcDisplayMonitor *output = l->data;
      cc_display_monitor_set_compatible_clone_mode (output, clone_mode);
      cc_display_monitor_set_position (output, 0, 0);
    }
}

static gboolean
cc_display_mode_equal (const CcDisplayMode *m1,
                       const CcDisplayMode *m2)
{
  if (!m1 && !m2)
    return TRUE;
  else if (!m1 || !m2)
    return FALSE;

  return m1->width == m2->width &&
    m1->height == m2->height &&
    m1->refresh_rate == m2->refresh_rate &&
    m1->refresh_rate_mode == m2->refresh_rate_mode &&
    (m1->flags & MODE_INTERLACED) == (m2->flags & MODE_INTERLACED);
}

gboolean
cc_display_config_equal (CcDisplayConfig *self,
                         CcDisplayConfig *other)
{
  GList *l;

  g_return_val_if_fail (CC_IS_DISPLAY_CONFIG (self), FALSE);
  g_return_val_if_fail (CC_IS_DISPLAY_CONFIG (other), FALSE);

  cc_display_config_ensure_non_offset_coords (self);
  cc_display_config_ensure_non_offset_coords (other);

  for (l = self->monitors; l != NULL; l = l->next)
    {
      CcDisplayMonitor *m1 = l->data;
      CcDisplayMonitor *m2 = monitor_from_spec (other,
                                                m1->connector_name,
                                                m1->vendor_name,
                                                m1->product_name,
                                                m1->product_serial);
      if (!m2)
        return FALSE;

      if (m1->underscanning != m2->underscanning)
        return FALSE;

      if (!cc_display_logical_monitor_equal (m1->logical_monitor, m2->logical_monitor))
        return FALSE;

      /* Modes should not be compared if both monitors have no logical monitor. */
      if (m1->logical_monitor == NULL && m2->logical_monitor == NULL)
        continue;

      if (!cc_display_mode_equal (CC_DISPLAY_MODE (m1->current_mode),
                                  CC_DISPLAY_MODE (m2->current_mode)))
        return FALSE;

      if (m1->color_mode != m2->color_mode)
        return FALSE;
    }

  return TRUE;
}

gboolean
cc_display_config_apply (CcDisplayConfig *self,
                         GError **error)
{
  if (!CC_IS_DISPLAY_CONFIG (self))
    {
      g_warning ("Cannot apply invalid configuration");
      g_set_error (error,
                   G_IO_ERROR,
                   G_IO_ERROR_FAILED,
                   "Cannot apply invalid configuration");
      return FALSE;
    }

  return config_apply (self, CC_DISPLAY_CONFIG_METHOD_PERSISTENT, error);
}

gboolean
cc_display_config_is_cloning (CcDisplayConfig *self)
{
  unsigned int n_active_monitors = 0;
  GList *l;

  g_return_val_if_fail (CC_IS_DISPLAY_CONFIG (self), FALSE);

  for (l = self->monitors; l != NULL; l = l->next)
    {
      if (cc_display_monitor_is_active (CC_DISPLAY_MONITOR (l->data)))
        n_active_monitors += 1;
    }

  return n_active_monitors > 1 && g_hash_table_size (self->logical_monitors) == 1;
}

static void
cc_display_config_make_linear (CcDisplayConfig *self)
{
  CcDisplayLogicalMonitor *primary;
  GList *logical_monitors, *l;
  int x;

  if (self->primary && self->primary->logical_monitor)
    {
      primary = self->primary->logical_monitor;
      primary->x = primary->y = 0;
      x = logical_monitor_width (primary);
    }
  else
    {
      primary = NULL;
      x = 0;
    }

  logical_monitors = g_hash_table_get_keys (self->logical_monitors);
  for (l = logical_monitors; l != NULL; l = l->next)
    {
      CcDisplayLogicalMonitor *m = l->data;

      if (m == primary)
        continue;

      m->x = x;
      m->y = 0;
      x += logical_monitor_width (m);
    }

  g_list_free (logical_monitors);
}

void
cc_display_config_set_cloning (CcDisplayConfig *self,
                               gboolean clone)
{
  gboolean is_cloning;
  CcDisplayLogicalMonitor *logical_monitor;
  GList *l;

  g_return_if_fail (CC_IS_DISPLAY_CONFIG (self));

  is_cloning = cc_display_config_is_cloning (self);
  if (clone && !is_cloning)
    {
      logical_monitor = g_object_new (CC_TYPE_DISPLAY_LOGICAL_MONITOR, NULL);
      for (l = self->monitors; l != NULL; l = l->next)
        {
          cc_display_monitor_set_logical_monitor (CC_DISPLAY_MONITOR (l->data),
                                                  logical_monitor);
        }
      register_logical_monitor (self, logical_monitor);
    }
  else if (!clone && is_cloning)
    {
      for (l = self->monitors; l != NULL; l = l->next)
        {
          logical_monitor = g_object_new (CC_TYPE_DISPLAY_LOGICAL_MONITOR, NULL);
          cc_display_monitor_set_logical_monitor (CC_DISPLAY_MONITOR (l->data),
                                                  logical_monitor);
          register_logical_monitor (self, logical_monitor);
        }
      cc_display_config_make_linear (self);
    }
}

static gboolean
is_mode_better (CcDisplayMode *mode,
                CcDisplayMode *other_mode)
{
  if (mode->width * mode->height > other_mode->width * other_mode->height)
    return TRUE;
  else if (mode->width * mode->height < other_mode->width * other_mode->height)
    return FALSE;

  if (!(mode->flags & MODE_INTERLACED) &&
      (other_mode->flags & MODE_INTERLACED))
    return TRUE;

  return FALSE;
}

static gboolean
mode_supports_scale (CcDisplayMode *mode,
                     double         scale)
{
  g_autoptr(GArray) scales = NULL;
  int i;

  scales = cc_display_mode_get_supported_scales (mode);
  for (i = 0; i < scales->len; i++)
    {
      if (G_APPROX_VALUE (scale, g_array_index (scales, double, i),
                          DBL_EPSILON))
        return TRUE;
    }

  return FALSE;
}

static void
remove_unsupported_scales (CcDisplayMode *mode,
                           GArray        *supported_scales)
{
  g_autoptr(GArray) mode_scales = NULL;
  int i;

  mode_scales = cc_display_mode_get_supported_scales (mode);
  i = 0;
  while (i < supported_scales->len)
    {
      double scale;

      if (i == supported_scales->len)
        break;

      scale = g_array_index (supported_scales, double, i);

      if (mode_supports_scale (mode, scale))
        {
          i++;
          continue;
        }

      g_array_remove_range (supported_scales, i, 1);
    }
}

static gboolean
monitor_has_compatible_clone_mode (CcDisplayMonitor *monitor,
                                   CcDisplayMode    *mode,
                                   GArray           *supported_scales)
{
  GList *l;

  for (l = monitor->modes; l; l = l->next)
    {
      CcDisplayMode *other_mode = l->data;

      if (other_mode->width != mode->width ||
          other_mode->height != mode->height)
        continue;

      if ((other_mode->flags & MODE_INTERLACED) !=
          (mode->flags & MODE_INTERLACED))
        continue;

      remove_unsupported_scales (CC_DISPLAY_MODE (other_mode), supported_scales);

      return TRUE;
    }

  return FALSE;
}

static gboolean
monitors_has_compatible_clone_mode (CcDisplayConfig *self,
                                    CcDisplayMode   *mode,
                                    GArray          *supported_scales)
{
  GList *l;

  for (l = self->monitors; l; l = l->next)
    {
      CcDisplayMonitor *monitor = l->data;

      if (!monitor_has_compatible_clone_mode (monitor, mode, supported_scales))
        return FALSE;
    }

  return TRUE;
}

GList *
cc_display_config_generate_cloning_modes (CcDisplayConfig *self)
{
  CcDisplayMonitor *base_monitor = NULL;
  GList *l;
  GList *clone_modes = NULL;
  CcDisplayMode *best_mode = NULL;

  g_return_val_if_fail (CC_IS_DISPLAY_CONFIG (self), NULL);

  for (l = self->monitors; l; l = l->next)
    {
      CcDisplayMonitor *monitor = l->data;

      if (cc_display_monitor_is_active (monitor))
        {
          base_monitor = CC_DISPLAY_MONITOR (monitor);
          break;
        }
    }

  if (!base_monitor)
    return NULL;

  for (l = base_monitor->modes; l; l = l->next)
    {
      CcDisplayMode *mode = l->data;
      CcDisplayMode *virtual_mode;
      g_autoptr (GArray) supported_scales = NULL;

      supported_scales =
        cc_display_mode_get_supported_scales (CC_DISPLAY_MODE (mode));

      if (!monitors_has_compatible_clone_mode (self, mode, supported_scales))
        continue;

      virtual_mode = cc_display_mode_new_virtual (mode->width,
                                                  mode->height,
                                                  mode->preferred_scale,
                                                  supported_scales);
      clone_modes = g_list_append (clone_modes, virtual_mode);

      if (!best_mode || is_mode_better (virtual_mode, best_mode))
        best_mode = virtual_mode;
    }

  if (best_mode)
    best_mode->flags |= MODE_PREFERRED;

  return clone_modes;
}

gboolean
cc_display_config_is_layout_logical (CcDisplayConfig *self)
{
  g_return_val_if_fail (CC_IS_DISPLAY_CONFIG (self), FALSE);

  return self->layout_mode == CC_DISPLAY_LAYOUT_MODE_LOGICAL;
}

gboolean
cc_display_config_is_scaled_mode_valid (CcDisplayConfig *self,
                                        CcDisplayMode   *mode,
                                        double           scale)
{
  g_return_val_if_fail (CC_IS_DISPLAY_CONFIG (self), FALSE);
  g_return_val_if_fail (CC_IS_DISPLAY_MODE (mode), FALSE);

  if (cc_display_config_is_cloning (self))
    return is_scale_allowed_by_active_monitors (self, mode, scale);

  return cc_display_mode_is_supported_scale (mode, scale);
}

gboolean
cc_display_config_get_panel_orientation_managed (CcDisplayConfig *self)
{
  return self->panel_orientation_managed;
}

void
cc_display_config_update_ui_numbers_names (CcDisplayConfig *self)
{
  GList *l;
  gint ui_number = 1;

  for (l = self->ui_sorted_monitors; l; l = l->next)
    {
      CcDisplayMonitor *monitor = l->data;
      char *ui_name;
      gint current_ui_number = 0;

      ui_name = make_output_ui_name (monitor);

      /* Prevents gaps in monitor numbering. Monitors
       * with number 0 will not be visible in the UI.
       */
      if (cc_display_monitor_is_usable (monitor))
        {
          current_ui_number = ui_number;
          ui_number += 1;
        }

      cc_display_monitor_set_ui_info (monitor, current_ui_number, ui_name);
    }
}
