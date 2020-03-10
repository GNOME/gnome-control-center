/*
 * Copyright (C) 2017  Red Hat, Inc.
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
#include <gio/gio.h>

#include "cc-display-config-dbus.h"

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

typedef enum _CcDisplayModeFlags
{
  MODE_PREFERRED = 1 << 0,
  MODE_CURRENT = 1 << 1,
  MODE_INTERLACED = 1 << 2,
} CcDisplayModeFlags;

struct _CcDisplayModeDBus
{
  CcDisplayMode parent_instance;

  char *id;
  int width;
  int height;
  double refresh_rate;
  double preferred_scale;
  GArray *supported_scales;
  guint32 flags;
};

G_DEFINE_TYPE (CcDisplayModeDBus,
               cc_display_mode_dbus,
               CC_TYPE_DISPLAY_MODE)

static gboolean
cc_display_mode_dbus_equal (const CcDisplayModeDBus *m1,
                            const CcDisplayModeDBus *m2)
{
  if (!m1 && !m2)
    return TRUE;
  else if (!m1 || !m2)
    return FALSE;

  return m1->width == m2->width &&
    m1->height == m2->height &&
    m1->refresh_rate == m2->refresh_rate &&
    (m1->flags & MODE_INTERLACED) == (m2->flags & MODE_INTERLACED);
}

static void
cc_display_mode_dbus_get_resolution (CcDisplayMode *pself,
                                     int *w, int *h)
{
  CcDisplayModeDBus *self = CC_DISPLAY_MODE_DBUS (pself);

  if (w)
    *w = self->width;
  if (h)
    *h = self->height;
}

static const double *
cc_display_mode_dbus_get_supported_scales (CcDisplayMode *pself)
{
  CcDisplayModeDBus *self = CC_DISPLAY_MODE_DBUS (pself);

  return (const double *) self->supported_scales->data;
}

static double
cc_display_mode_dbus_get_preferred_scale (CcDisplayMode *pself)
{
  CcDisplayModeDBus *self = CC_DISPLAY_MODE_DBUS (pself);

  return self->preferred_scale;
}

static gboolean
cc_display_mode_dbus_is_supported_scale (CcDisplayMode *pself,
                                         double scale)
{
  CcDisplayModeDBus *self = CC_DISPLAY_MODE_DBUS (pself);

  guint i;
  for (i = 0; i < self->supported_scales->len; i++)
    if (g_array_index (self->supported_scales, double, i) == scale)
      return TRUE;
  return FALSE;
}


static gboolean
cc_display_mode_dbus_is_interlaced (CcDisplayMode *pself)
{
  CcDisplayModeDBus *self = CC_DISPLAY_MODE_DBUS (pself);

  return !!(self->flags & MODE_INTERLACED);
}

static int
cc_display_mode_dbus_get_freq (CcDisplayMode *pself)
{
  CcDisplayModeDBus *self = CC_DISPLAY_MODE_DBUS (pself);

  return self->refresh_rate;
}

static double
cc_display_mode_dbus_get_freq_f (CcDisplayMode *pself)
{
  CcDisplayModeDBus *self = CC_DISPLAY_MODE_DBUS (pself);

  return self->refresh_rate;
}

static void
cc_display_mode_dbus_init (CcDisplayModeDBus *self)
{
  self->supported_scales = g_array_new (TRUE, TRUE, sizeof (double));
}

static void
cc_display_mode_dbus_finalize (GObject *object)
{
  CcDisplayModeDBus *self = CC_DISPLAY_MODE_DBUS (object);

  g_free (self->id);
  g_array_free (self->supported_scales, TRUE);

  G_OBJECT_CLASS (cc_display_mode_dbus_parent_class)->finalize (object);
}

static void
cc_display_mode_dbus_class_init (CcDisplayModeDBusClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  CcDisplayModeClass *parent_class = CC_DISPLAY_MODE_CLASS (klass);

  gobject_class->finalize = cc_display_mode_dbus_finalize;

  parent_class->get_resolution = cc_display_mode_dbus_get_resolution;
  parent_class->get_supported_scales = cc_display_mode_dbus_get_supported_scales;
  parent_class->get_preferred_scale = cc_display_mode_dbus_get_preferred_scale;
  parent_class->is_interlaced = cc_display_mode_dbus_is_interlaced;
  parent_class->get_freq = cc_display_mode_dbus_get_freq;
  parent_class->get_freq_f = cc_display_mode_dbus_get_freq_f;
}

static CcDisplayModeDBus *
cc_display_mode_dbus_new (GVariant *variant)
{
  double d;
  g_autoptr(GVariantIter) scales_iter = NULL;
  g_autoptr(GVariant) properties_variant = NULL;
  gboolean is_current;
  gboolean is_preferred;
  gboolean is_interlaced;
  CcDisplayModeDBus *self = g_object_new (CC_TYPE_DISPLAY_MODE_DBUS, NULL);

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

  if (is_current)
    self->flags |= MODE_CURRENT;
  if (is_preferred)
    self->flags |= MODE_PREFERRED;
  if (is_interlaced)
    self->flags |= MODE_INTERLACED;

  return self;
}


#define CC_TYPE_DISPLAY_LOGICAL_MONITOR (cc_display_logical_monitor_get_type ())
G_DECLARE_FINAL_TYPE (CcDisplayLogicalMonitor, cc_display_logical_monitor,
                      CC, DISPLAY_LOGICAL_MONITOR, GObject)

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

G_DEFINE_TYPE (CcDisplayLogicalMonitor,
               cc_display_logical_monitor,
               G_TYPE_OBJECT)

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
    m1->scale == m2->scale &&
    m1->rotation == m2->rotation &&
    m1->primary == m2->primary;
}

static void
cc_display_logical_monitor_init (CcDisplayLogicalMonitor *self)
{
  self->scale = 1.0;
  self->monitors = g_hash_table_new (NULL, NULL);
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


typedef enum _CcDisplayMonitorUnderscanning
{
  UNDERSCANNING_UNSUPPORTED = 0,
  UNDERSCANNING_DISABLED,
  UNDERSCANNING_ENABLED
} CcDisplayMonitorUnderscanning;

struct _CcDisplayMonitorDBus
{
  CcDisplayMonitor parent_instance;
  CcDisplayConfigDBus *config;

  gchar *connector_name;
  gchar *vendor_name;
  gchar *product_name;
  gchar *product_serial;
  gchar *display_name;

  int width_mm;
  int height_mm;
  gboolean builtin;
  CcDisplayMonitorUnderscanning underscanning;
  int max_width;
  int max_height;

  GList *modes;
  CcDisplayMode *current_mode;
  CcDisplayMode *preferred_mode;

  CcDisplayLogicalMonitor *logical_monitor;
};

G_DEFINE_TYPE (CcDisplayMonitorDBus,
               cc_display_monitor_dbus,
               CC_TYPE_DISPLAY_MONITOR)

static void
register_logical_monitor (CcDisplayConfigDBus *self,
                          CcDisplayLogicalMonitor *logical_monitor);
static void
cc_display_config_dbus_set_primary (CcDisplayConfigDBus *self,
                                    CcDisplayMonitorDBus *new_primary);
static void
cc_display_config_dbus_unset_primary (CcDisplayConfigDBus *self,
                                      CcDisplayMonitorDBus *old_primary);
static void
cc_display_config_dbus_ensure_non_offset_coords (CcDisplayConfigDBus *self);
static void
cc_display_config_dbus_append_right (CcDisplayConfigDBus *self,
                                     CcDisplayLogicalMonitor *monitor);
static void
cc_display_config_dbus_make_linear (CcDisplayConfigDBus *self);


static const char *
cc_display_monitor_dbus_get_display_name (CcDisplayMonitor *pself)
{
  CcDisplayMonitorDBus *self = CC_DISPLAY_MONITOR_DBUS (pself);

  if (self->display_name)
    return self->display_name;

  return self->connector_name;
}

static const char *
cc_display_monitor_dbus_get_connector_name (CcDisplayMonitor *pself)
{
  CcDisplayMonitorDBus *self = CC_DISPLAY_MONITOR_DBUS (pself);

  return self->connector_name;
}

static gboolean
cc_display_monitor_dbus_is_builtin (CcDisplayMonitor *pself)
{
  CcDisplayMonitorDBus *self = CC_DISPLAY_MONITOR_DBUS (pself);

  return self->builtin;
}

static gboolean
cc_display_monitor_dbus_is_primary (CcDisplayMonitor *pself)
{
  CcDisplayMonitorDBus *self = CC_DISPLAY_MONITOR_DBUS (pself);

  if (self->logical_monitor)
    return self->logical_monitor->primary;

  return FALSE;
}

static void
cc_display_monitor_dbus_set_primary (CcDisplayMonitor *pself,
                                     gboolean primary)
{
  CcDisplayMonitorDBus *self = CC_DISPLAY_MONITOR_DBUS (pself);

  if (primary)
    cc_display_config_dbus_set_primary (self->config, self);
  else
    cc_display_config_dbus_unset_primary (self->config, self);
}

static gboolean
cc_display_monitor_dbus_is_active (CcDisplayMonitor *pself)
{
  CcDisplayMonitorDBus *self = CC_DISPLAY_MONITOR_DBUS (pself);

  return self->logical_monitor != NULL;
}

static void
cc_display_monitor_dbus_set_logical_monitor (CcDisplayMonitorDBus *self,
                                             CcDisplayLogicalMonitor *logical_monitor)
{
  gboolean was_primary = FALSE;

  if (self->logical_monitor)
    {
      was_primary = self->logical_monitor->primary;
      if (was_primary)
        cc_display_config_dbus_unset_primary (self->config, self);
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
        cc_display_config_dbus_set_primary (self->config, self);
      else
        cc_display_config_dbus_unset_primary (self->config, NULL);
    }
}

static void
cc_display_monitor_dbus_set_active (CcDisplayMonitor *pself,
                                    gboolean active)
{
  CcDisplayMonitorDBus *self = CC_DISPLAY_MONITOR_DBUS (pself);

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
      cc_display_monitor_dbus_set_logical_monitor (self, logical_monitor);
      cc_display_config_dbus_append_right (self->config, logical_monitor);
      register_logical_monitor (self->config, logical_monitor);
    }
  else if (self->logical_monitor && !active)
    {
      cc_display_monitor_dbus_set_logical_monitor (self, NULL);
    }

  g_signal_emit_by_name (self, "active");
}

static CcDisplayRotation
cc_display_monitor_dbus_get_rotation (CcDisplayMonitor *pself)
{
  CcDisplayMonitorDBus *self = CC_DISPLAY_MONITOR_DBUS (pself);

  if (self->logical_monitor)
    return self->logical_monitor->rotation;

  return CC_DISPLAY_ROTATION_NONE;
}

static void
cc_display_monitor_dbus_set_rotation (CcDisplayMonitor *pself,
                                      CcDisplayRotation rotation)
{
  CcDisplayMonitorDBus *self = CC_DISPLAY_MONITOR_DBUS (pself);

  if (!self->logical_monitor)
    return;

  if (self->logical_monitor->rotation != rotation)
    {
      self->logical_monitor->rotation = rotation;

      g_signal_emit_by_name (self, "rotation");
    }
}

static gboolean
cc_display_monitor_dbus_supports_rotation (CcDisplayMonitor *pself,
                                           CcDisplayRotation rotation)
{
  return TRUE;
}

static void
cc_display_monitor_dbus_get_physical_size (CcDisplayMonitor *pself,
                                           int *w, int *h)
{
  CcDisplayMonitorDBus *self = CC_DISPLAY_MONITOR_DBUS (pself);

  if (w)
    *w = self->width_mm;
  if (h)
    *h = self->height_mm;
}

static void
cc_display_monitor_dbus_get_geometry (CcDisplayMonitor *pself,
                                      int *x, int *y, int *w, int *h)
{
  CcDisplayMonitorDBus *self = CC_DISPLAY_MONITOR_DBUS (pself);
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

static CcDisplayMode *
cc_display_monitor_dbus_get_mode (CcDisplayMonitor *pself)
{
  CcDisplayMonitorDBus *self = CC_DISPLAY_MONITOR_DBUS (pself);

  return self->current_mode;
}

static CcDisplayMode *
cc_display_monitor_dbus_get_preferred_mode (CcDisplayMonitor *pself)
{
  CcDisplayMonitorDBus *self = CC_DISPLAY_MONITOR_DBUS (pself);

  return self->preferred_mode;
}

static guint32
cc_display_monitor_dbus_get_id (CcDisplayMonitor *pself)
{
  return 0;
}

static GList *
cc_display_monitor_dbus_get_modes (CcDisplayMonitor *pself)
{
  CcDisplayMonitorDBus *self = CC_DISPLAY_MONITOR_DBUS (pself);

  return self->modes;
}

static gboolean
cc_display_monitor_dbus_supports_underscanning (CcDisplayMonitor *pself)
{
  CcDisplayMonitorDBus *self = CC_DISPLAY_MONITOR_DBUS (pself);

  return self->underscanning != UNDERSCANNING_UNSUPPORTED;
}

static gboolean
cc_display_monitor_dbus_get_underscanning (CcDisplayMonitor *pself)
{
  CcDisplayMonitorDBus *self = CC_DISPLAY_MONITOR_DBUS (pself);

  return self->underscanning == UNDERSCANNING_ENABLED;
}

static void
cc_display_monitor_dbus_set_underscanning (CcDisplayMonitor *pself,
                                           gboolean underscanning)
{
  CcDisplayMonitorDBus *self = CC_DISPLAY_MONITOR_DBUS (pself);

  if (self->underscanning == UNDERSCANNING_UNSUPPORTED)
    return;

  if (underscanning)
    self->underscanning = UNDERSCANNING_ENABLED;
  else
    self->underscanning = UNDERSCANNING_DISABLED;
}

static CcDisplayMode *
cc_display_monitor_dbus_get_closest_mode (CcDisplayMonitorDBus *self,
                                          CcDisplayModeDBus *mode)
{
  CcDisplayModeDBus *best = NULL;
  GList *l;

  for (l = self->modes; l != NULL; l = l->next)
    {
      CcDisplayModeDBus *similar = l->data;

      if (similar->width != mode->width ||
          similar->height != mode->height)
        continue;

      if (similar->refresh_rate == mode->refresh_rate &&
          (similar->flags & MODE_INTERLACED) == (mode->flags & MODE_INTERLACED))
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

static void
cc_display_monitor_dbus_set_mode (CcDisplayMonitor *pself,
                                  CcDisplayMode *new_mode)
{
  CcDisplayMonitorDBus *self = CC_DISPLAY_MONITOR_DBUS (pself);
  CcDisplayMode *mode;

  g_return_if_fail (new_mode != NULL);

  mode = cc_display_monitor_dbus_get_closest_mode (self, CC_DISPLAY_MODE_DBUS (new_mode));

  self->current_mode = mode;

  if (!cc_display_mode_dbus_is_supported_scale (mode, cc_display_monitor_get_scale (pself)))
    cc_display_monitor_set_scale (pself, cc_display_mode_get_preferred_scale (mode));

  g_signal_emit_by_name (self, "mode");
}

static void
cc_display_monitor_dbus_set_position (CcDisplayMonitor *pself,
                                      int x, int y)
{
  CcDisplayMonitorDBus *self = CC_DISPLAY_MONITOR_DBUS (pself);

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

static double
cc_display_monitor_dbus_get_scale (CcDisplayMonitor *pself)
{
  CcDisplayMonitorDBus *self = CC_DISPLAY_MONITOR_DBUS (pself);

  if (self->logical_monitor)
    return self->logical_monitor->scale;

  return 1.0;
}

static void
cc_display_monitor_dbus_set_scale (CcDisplayMonitor *pself,
                                   double scale)
{
  CcDisplayMonitorDBus *self = CC_DISPLAY_MONITOR_DBUS (pself);

  if (!self->current_mode)
    return;

  if (!cc_display_mode_dbus_is_supported_scale (self->current_mode, scale))
    return;

  if (!self->logical_monitor)
    return;

  if (self->logical_monitor->scale != scale)
    {
      self->logical_monitor->scale = scale;

      g_signal_emit_by_name (self, "scale");
    }
}

static void
cc_display_monitor_dbus_init (CcDisplayMonitorDBus *self)
{
  self->underscanning = UNDERSCANNING_UNSUPPORTED;
  self->max_width = G_MAXINT;
  self->max_height = G_MAXINT;
}

static void
cc_display_monitor_dbus_finalize (GObject *object)
{
  CcDisplayMonitorDBus *self = CC_DISPLAY_MONITOR_DBUS (object);

  g_free (self->connector_name);
  g_free (self->vendor_name);
  g_free (self->product_name);
  g_free (self->product_serial);
  g_free (self->display_name);

  g_list_foreach (self->modes, (GFunc) g_object_unref, NULL);
  g_clear_pointer (&self->modes, g_list_free);

  if (self->logical_monitor)
    {
      g_hash_table_remove (self->logical_monitor->monitors, self);
      g_object_unref (self->logical_monitor);
    }

  G_OBJECT_CLASS (cc_display_monitor_dbus_parent_class)->finalize (object);
}

static void
cc_display_monitor_dbus_class_init (CcDisplayMonitorDBusClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  CcDisplayMonitorClass *parent_class = CC_DISPLAY_MONITOR_CLASS (klass);

  gobject_class->finalize = cc_display_monitor_dbus_finalize;

  parent_class->get_display_name = cc_display_monitor_dbus_get_display_name;
  parent_class->get_connector_name = cc_display_monitor_dbus_get_connector_name;
  parent_class->is_builtin = cc_display_monitor_dbus_is_builtin;
  parent_class->is_primary = cc_display_monitor_dbus_is_primary;
  parent_class->set_primary = cc_display_monitor_dbus_set_primary;
  parent_class->is_active = cc_display_monitor_dbus_is_active;
  parent_class->set_active = cc_display_monitor_dbus_set_active;
  parent_class->get_rotation = cc_display_monitor_dbus_get_rotation;
  parent_class->set_rotation = cc_display_monitor_dbus_set_rotation;
  parent_class->supports_rotation = cc_display_monitor_dbus_supports_rotation;
  parent_class->get_physical_size = cc_display_monitor_dbus_get_physical_size;
  parent_class->get_geometry = cc_display_monitor_dbus_get_geometry;
  parent_class->get_mode = cc_display_monitor_dbus_get_mode;
  parent_class->get_preferred_mode = cc_display_monitor_dbus_get_preferred_mode;
  parent_class->get_id = cc_display_monitor_dbus_get_id;
  parent_class->get_modes = cc_display_monitor_dbus_get_modes;
  parent_class->supports_underscanning = cc_display_monitor_dbus_supports_underscanning;
  parent_class->get_underscanning = cc_display_monitor_dbus_get_underscanning;
  parent_class->set_underscanning = cc_display_monitor_dbus_set_underscanning;
  parent_class->set_mode = cc_display_monitor_dbus_set_mode;
  parent_class->set_position = cc_display_monitor_dbus_set_position;
  parent_class->get_scale = cc_display_monitor_dbus_get_scale;
  parent_class->set_scale = cc_display_monitor_dbus_set_scale;
}

static void
construct_modes (CcDisplayMonitorDBus *self,
                 GVariantIter *modes)
{
  CcDisplayModeDBus *mode;

  while (TRUE)
    {
      g_autoptr(GVariant) variant = NULL;

      if (!g_variant_iter_next (modes, "@"MODE_FORMAT, &variant))
        break;

      mode = cc_display_mode_dbus_new (variant);
      self->modes = g_list_prepend (self->modes, mode);

      if (mode->flags & MODE_PREFERRED)
        self->preferred_mode = CC_DISPLAY_MODE (mode);
      if (mode->flags & MODE_CURRENT)
        self->current_mode = CC_DISPLAY_MODE (mode);
    }
}

static CcDisplayMonitorDBus *
cc_display_monitor_dbus_new (GVariant *variant,
                             CcDisplayConfigDBus *config)
{
  CcDisplayMonitorDBus *self = g_object_new (CC_TYPE_DISPLAY_MONITOR_DBUS, NULL);
  gchar *s1, *s2, *s3, *s4;
  g_autoptr(GVariantIter) modes = NULL;
  g_autoptr(GVariantIter) props = NULL;

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
    }

  return self;
}


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

struct _CcDisplayConfigDBus
{
  CcDisplayConfig parent_instance;

  GVariant *state;
  GDBusConnection *connection;

  int min_width;
  int min_height;

  guint32 serial;
  gboolean supports_mirroring;
  gboolean supports_changing_layout_mode;
  gboolean global_scale_required;
  CcDisplayLayoutMode layout_mode;

  GList *monitors;
  CcDisplayMonitorDBus *primary;

  GHashTable *logical_monitors;

  GList *clone_modes;
};

G_DEFINE_TYPE (CcDisplayConfigDBus,
               cc_display_config_dbus,
               CC_TYPE_DISPLAY_CONFIG)

enum
{
  PROP_0,
  PROP_STATE,
  PROP_CONNECTION,
};

static GList *
cc_display_config_dbus_get_monitors (CcDisplayConfig *pself)
{
  CcDisplayConfigDBus *self = CC_DISPLAY_CONFIG_DBUS (pself);

  return self->monitors;
}

static GVariant *
build_monitors_variant (GHashTable *monitors)
{
  GVariantBuilder builder;
  GHashTableIter iter;
  CcDisplayMonitorDBus *monitor;

  g_variant_builder_init (&builder, G_VARIANT_TYPE_ARRAY);
  g_hash_table_iter_init (&iter, monitors);

  while (g_hash_table_iter_next (&iter, (void **) &monitor, NULL))
    {
      GVariantBuilder props_builder;
      CcDisplayModeDBus *mode_dbus;

      if (!monitor->current_mode)
        continue;

      g_variant_builder_init (&props_builder, G_VARIANT_TYPE ("a{sv}"));
      g_variant_builder_add (&props_builder, "{sv}",
                             "underscanning",
                             g_variant_new_boolean (monitor->underscanning == UNDERSCANNING_ENABLED));

      mode_dbus = CC_DISPLAY_MODE_DBUS (monitor->current_mode);
      g_variant_builder_add (&builder, "(ss@*)",
                             monitor->connector_name,
                             mode_dbus->id,
                             g_variant_builder_end (&props_builder));
    }

  return g_variant_builder_end (&builder);
}

static GVariant *
build_logical_monitors_parameter (CcDisplayConfigDBus *self)
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
build_apply_parameters (CcDisplayConfigDBus *self,
                        CcDisplayConfigMethod method)
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
config_apply (CcDisplayConfigDBus *self,
              CcDisplayConfigMethod method,
              GError **error)
{
  g_autoptr(GVariant) retval = NULL;

  cc_display_config_dbus_ensure_non_offset_coords (self);

  retval = g_dbus_connection_call_sync (self->connection,
                                        "org.gnome.Mutter.DisplayConfig",
                                        "/org/gnome/Mutter/DisplayConfig",
                                        "org.gnome.Mutter.DisplayConfig",
                                        "ApplyMonitorsConfig",
                                        build_apply_parameters (self, method),
                                        NULL,
                                        G_DBUS_CALL_FLAGS_NO_AUTO_START,
                                        -1,
                                        NULL,
                                        error);
  return retval != NULL;
}

static gboolean
cc_display_config_dbus_is_applicable (CcDisplayConfig *pself)
{
  CcDisplayConfigDBus *self = CC_DISPLAY_CONFIG_DBUS (pself);
  g_autoptr(GError) error = NULL;

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

static CcDisplayMonitorDBus *
monitor_from_spec (CcDisplayConfigDBus *self,
                   const gchar *connector,
                   const gchar *vendor,
                   const gchar *product,
                   const gchar *serial)
{
  GList *l;
  for (l = self->monitors; l != NULL; l = l->next)
    {
      CcDisplayMonitorDBus *m = l->data;
      if (g_str_equal (m->connector_name, connector) &&
          g_str_equal (m->vendor_name, vendor) &&
          g_str_equal (m->product_name, product) &&
          g_str_equal (m->product_serial, serial))
        return m;
    }
  return NULL;
}

static gboolean
cc_display_config_dbus_equal (CcDisplayConfig *pself,
                              CcDisplayConfig *pother)
{
  CcDisplayConfigDBus *self = CC_DISPLAY_CONFIG_DBUS (pself);
  CcDisplayConfigDBus *other = CC_DISPLAY_CONFIG_DBUS (pother);
  GList *l;

  g_return_val_if_fail (pself, FALSE);
  g_return_val_if_fail (pother, FALSE);

  cc_display_config_dbus_ensure_non_offset_coords (self);
  cc_display_config_dbus_ensure_non_offset_coords (other);

  for (l = self->monitors; l != NULL; l = l->next)
    {
      CcDisplayMonitorDBus *m1 = l->data;
      CcDisplayMonitorDBus *m2 = monitor_from_spec (other,
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

      if (!cc_display_mode_dbus_equal (CC_DISPLAY_MODE_DBUS (m1->current_mode),
                                       CC_DISPLAY_MODE_DBUS (m2->current_mode)))
        return FALSE;
    }

  return TRUE;
}

static void
cc_display_config_dbus_set_primary (CcDisplayConfigDBus *self,
                                    CcDisplayMonitorDBus *new_primary)
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
cc_display_config_dbus_unset_primary (CcDisplayConfigDBus *self,
                                      CcDisplayMonitorDBus *old_primary)
{
  GList *l;

  if (self->primary != old_primary)
    return;

  for (l = self->monitors; l != NULL; l = l->next)
    {
      CcDisplayMonitorDBus *monitor = l->data;
      if (monitor->logical_monitor &&
          monitor != old_primary)
        {
          cc_display_config_dbus_set_primary (self, monitor);
          break;
        }
    }

  if (self->primary == old_primary)
    self->primary = NULL;
}

static gboolean
cc_display_config_dbus_is_cloning (CcDisplayConfig *pself)
{
  CcDisplayConfigDBus *self = CC_DISPLAY_CONFIG_DBUS (pself);
  guint n_active_monitors = 0;
  GList *l;

  for (l = self->monitors; l != NULL; l = l->next)
    if (cc_display_monitor_is_active (CC_DISPLAY_MONITOR (l->data)))
      n_active_monitors += 1;

  return n_active_monitors > 1 && g_hash_table_size (self->logical_monitors) == 1;
}

static void
cc_display_config_dbus_set_cloning (CcDisplayConfig *pself,
                                    gboolean clone)
{
  CcDisplayConfigDBus *self = CC_DISPLAY_CONFIG_DBUS (pself);
  gboolean is_cloning = cc_display_config_is_cloning (pself);
  CcDisplayLogicalMonitor *logical_monitor;
  GList *l;

  if (clone && !is_cloning)
    {
      logical_monitor = g_object_new (CC_TYPE_DISPLAY_LOGICAL_MONITOR, NULL);
      for (l = self->monitors; l != NULL; l = l->next)
        cc_display_monitor_dbus_set_logical_monitor (CC_DISPLAY_MONITOR_DBUS (l->data),
                                                     logical_monitor);
      register_logical_monitor (self, logical_monitor);
    }
  else if (!clone && is_cloning)
    {
      for (l = self->monitors; l != NULL; l = l->next)
        {
          logical_monitor = g_object_new (CC_TYPE_DISPLAY_LOGICAL_MONITOR, NULL);
          cc_display_monitor_dbus_set_logical_monitor (CC_DISPLAY_MONITOR_DBUS (l->data),
                                                       logical_monitor);
          register_logical_monitor (self, logical_monitor);
        }
      cc_display_config_dbus_make_linear (self);
    }
}

static GList *
cc_display_config_dbus_get_cloning_modes (CcDisplayConfig *pself)
{
  CcDisplayConfigDBus *self = CC_DISPLAY_CONFIG_DBUS (pself);

  return self->clone_modes;
}

static gboolean
cc_display_config_dbus_apply (CcDisplayConfig *pself,
                              GError **error)
{
  CcDisplayConfigDBus *self = CC_DISPLAY_CONFIG_DBUS (pself);

  return config_apply (self, CC_DISPLAY_CONFIG_METHOD_PERSISTENT, error);
}

static gboolean
cc_display_config_dbus_is_layout_logical (CcDisplayConfig *pself)
{
  CcDisplayConfigDBus *self = CC_DISPLAY_CONFIG_DBUS (pself);

  return self->layout_mode == CC_DISPLAY_LAYOUT_MODE_LOGICAL;
}

static gboolean
is_scaled_mode_allowed (CcDisplayConfigDBus *self,
                        CcDisplayMode       *pmode,
                        double               scale)
{
  gint width, height;
  CcDisplayModeDBus *mode = CC_DISPLAY_MODE_DBUS (pmode);

  if (!cc_display_mode_dbus_is_supported_scale (pmode, scale))
    return FALSE;

  /* Do the math as if the monitor is always in landscape mode. */
  width = round (mode->width / scale);
  height = round (mode->height / scale);

  return (MAX (width, height) >= self->min_width &&
          MIN (width, height) >= self->min_height);
}

static gboolean
is_scale_allowed_by_active_monitors (CcDisplayConfigDBus *self,
                                     CcDisplayMode       *mode,
                                     double               scale)
{
  GList *l;

  for (l = self->monitors; l != NULL; l = l->next)
    {
      CcDisplayMonitorDBus *m = CC_DISPLAY_MONITOR_DBUS (l->data);

      if (!cc_display_monitor_is_active (CC_DISPLAY_MONITOR (m)))
        continue;

      if (!is_scaled_mode_allowed (self, mode, scale))
        return FALSE;
    }

  return TRUE;
}

static void
cc_display_config_dbus_set_minimum_size (CcDisplayConfig *pself,
                                         int              width,
                                         int              height)
{
  CcDisplayConfigDBus *self = CC_DISPLAY_CONFIG_DBUS (pself);

  g_assert (width >= 0 && height >= 0);

  self->min_width = width;
  self->min_height = height;
}

static gboolean
cc_display_config_dbus_is_scaled_mode_valid (CcDisplayConfig *pself,
                                             CcDisplayMode   *mode,
                                             double           scale)
{
  CcDisplayConfigDBus *self = CC_DISPLAY_CONFIG_DBUS (pself);

  if (self->global_scale_required || cc_display_config_is_cloning (pself))
    return is_scale_allowed_by_active_monitors (self, mode, scale);

  return is_scaled_mode_allowed (self, mode, scale);
}

static void
cc_display_config_dbus_init (CcDisplayConfigDBus *self)
{
  self->serial = 0;
  self->supports_mirroring = TRUE;
  self->supports_changing_layout_mode = FALSE;
  self->global_scale_required = FALSE;
  self->layout_mode = CC_DISPLAY_LAYOUT_MODE_LOGICAL;
  self->logical_monitors = g_hash_table_new (NULL, NULL);
}

static void
gather_clone_modes (CcDisplayConfigDBus *self)
{
  guint n_monitors = g_list_length (self->monitors);
  CcDisplayMonitorDBus *monitor;
  GList *l;

  if (n_monitors < 2)
    return;

  monitor = self->monitors->data;
  for (l = monitor->modes; l != NULL; l = l->next)
    {
      CcDisplayModeDBus *mode = l->data;
      gboolean valid = TRUE;
      GList *ll;
      for (ll = self->monitors->next; ll != NULL; ll = ll->next)
        {
          CcDisplayMonitorDBus *other_monitor = ll->data;
          if (!cc_display_monitor_dbus_get_closest_mode (other_monitor, mode))
            {
              valid = FALSE;
              break;
            }
        }
      if (valid)
        self->clone_modes = g_list_prepend (self->clone_modes, mode);
    }
}

static void
remove_logical_monitor (gpointer data,
                        GObject *object)
{
  CcDisplayConfigDBus *self = CC_DISPLAY_CONFIG_DBUS (data);

  g_hash_table_remove (self->logical_monitors, object);
}

static void
register_logical_monitor (CcDisplayConfigDBus *self,
                          CcDisplayLogicalMonitor *logical_monitor)
{
  g_hash_table_add (self->logical_monitors, logical_monitor);
  g_object_weak_ref (G_OBJECT (logical_monitor), remove_logical_monitor, self);
  g_object_unref (logical_monitor);
}

static void
apply_global_scale_requirement (CcDisplayConfigDBus *self,
                                CcDisplayMonitor    *monitor)
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
construct_monitors (CcDisplayConfigDBus *self,
                    GVariantIter *monitors,
                    GVariantIter *logical_monitors)
{
  while (TRUE)
    {
      CcDisplayMonitorDBus *monitor;
      g_autoptr(GVariant) variant = NULL;

      if (!g_variant_iter_next (monitors, "@"MONITOR_FORMAT, &variant))
        break;

      monitor = cc_display_monitor_dbus_new (variant, self);
      self->monitors = g_list_prepend (self->monitors, monitor);

      if (self->global_scale_required)
        g_signal_connect_object (monitor, "scale",
                                 G_CALLBACK (apply_global_scale_requirement),
                                 self, G_CONNECT_SWAPPED);
    }

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
          CcDisplayMonitorDBus *m = monitor_from_spec (self, s1, s2, s3, s4);
          if (!m)
            {
              g_warning ("Couldn't find monitor given spec: %s, %s, %s, %s",
                         s1, s2, s3, s4);
              continue;
            }

          cc_display_monitor_dbus_set_logical_monitor (m, logical_monitor);
        }

      if (g_hash_table_size (logical_monitor->monitors) > 0)
        {
          if (primary)
            {
              CcDisplayMonitorDBus *m = NULL;
              GHashTableIter iter;
              g_hash_table_iter_init (&iter, logical_monitor->monitors);
              g_hash_table_iter_next (&iter, (void **) &m, NULL);

              cc_display_config_dbus_set_primary (self, m);
            }
        }
      else
        {
          g_warning ("Got an empty logical monitor, ignoring");
        }

      register_logical_monitor (self, logical_monitor);
    }

  gather_clone_modes (self);
}

static void
cc_display_config_dbus_constructed (GObject *object)
{
  CcDisplayConfigDBus *self = CC_DISPLAY_CONFIG_DBUS (object);
  g_autoptr(GVariantIter) monitors = NULL;
  g_autoptr(GVariantIter) logical_monitors = NULL;
  g_autoptr(GVariantIter) props = NULL;

  g_variant_get (self->state,
                 CURRENT_STATE_FORMAT,
                 &self->serial,
                 &monitors,
                 &logical_monitors,
                 &props);

  while (TRUE)
    {
      const char *s;
      g_autoptr(GVariant) v = NULL;

      if (!g_variant_iter_next (props, "{&sv}", &s, &v))
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

  construct_monitors (self, monitors, logical_monitors);

  G_OBJECT_CLASS (cc_display_config_dbus_parent_class)->constructed (object);
}

static void
cc_display_config_dbus_set_property (GObject      *object,
                                     guint         prop_id,
                                     const GValue *value,
                                     GParamSpec   *pspec)
{
  CcDisplayConfigDBus *self = CC_DISPLAY_CONFIG_DBUS (object);

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
cc_display_config_dbus_get_property (GObject    *object,
                                     guint       prop_id,
                                     GValue     *value,
                                     GParamSpec *pspec)
{
  CcDisplayConfigDBus *self = CC_DISPLAY_CONFIG_DBUS (object);

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
cc_display_config_dbus_finalize (GObject *object)
{
  CcDisplayConfigDBus *self = CC_DISPLAY_CONFIG_DBUS (object);

  g_clear_pointer (&self->state, g_variant_unref);
  g_clear_object (&self->connection);

  g_list_foreach (self->monitors, (GFunc) g_object_unref, NULL);
  g_clear_pointer (&self->monitors, g_list_free);
  g_clear_pointer (&self->logical_monitors, g_hash_table_destroy);
  g_clear_pointer (&self->clone_modes, g_list_free);

  G_OBJECT_CLASS (cc_display_config_dbus_parent_class)->finalize (object);
}

static void
cc_display_config_dbus_class_init (CcDisplayConfigDBusClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  CcDisplayConfigClass *parent_class = CC_DISPLAY_CONFIG_CLASS (klass);
  GParamSpec *pspec;

  gobject_class->constructed = cc_display_config_dbus_constructed;
  gobject_class->set_property = cc_display_config_dbus_set_property;
  gobject_class->get_property = cc_display_config_dbus_get_property;
  gobject_class->finalize = cc_display_config_dbus_finalize;

  parent_class->get_monitors = cc_display_config_dbus_get_monitors;
  parent_class->is_applicable = cc_display_config_dbus_is_applicable;
  parent_class->equal = cc_display_config_dbus_equal;
  parent_class->apply = cc_display_config_dbus_apply;
  parent_class->is_cloning = cc_display_config_dbus_is_cloning;
  parent_class->set_cloning = cc_display_config_dbus_set_cloning;
  parent_class->get_cloning_modes = cc_display_config_dbus_get_cloning_modes;
  parent_class->is_layout_logical = cc_display_config_dbus_is_layout_logical;
  parent_class->is_scaled_mode_valid = cc_display_config_dbus_is_scaled_mode_valid;
  parent_class->set_minimum_size = cc_display_config_dbus_set_minimum_size;

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
}

static gint
sort_x_axis (gconstpointer a, gconstpointer b)
{
  const CcDisplayLogicalMonitor *ma = a;
  const CcDisplayLogicalMonitor *mb = b;
  return ma->x - mb->x;
}

static gint
sort_y_axis (gconstpointer a, gconstpointer b)
{
  const CcDisplayLogicalMonitor *ma = a;
  const CcDisplayLogicalMonitor *mb = b;
  return ma->y - mb->y;
}

static void
add_x_delta (gpointer d1, gpointer d2)
{
  CcDisplayLogicalMonitor *m = d1;
  int delta = GPOINTER_TO_INT (d2);
  m->x += delta;
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
  CcDisplayMonitorDBus *monitor;
  CcDisplayModeDBus *mode;
  GHashTableIter iter;
  int width;

  g_hash_table_iter_init (&iter, lm->monitors);
  g_hash_table_iter_next (&iter, (void **) &monitor, NULL);
  mode = CC_DISPLAY_MODE_DBUS (monitor->current_mode);
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
add_y_delta (gpointer d1, gpointer d2)
{
  CcDisplayLogicalMonitor *m = d1;
  int delta = GPOINTER_TO_INT (d2);
  m->y += delta;
}

static void
cc_display_config_dbus_ensure_non_offset_coords (CcDisplayConfigDBus *self)
{
  GList *x_axis, *y_axis;
  CcDisplayLogicalMonitor *m;

  if (g_hash_table_size (self->logical_monitors) == 0)
    return;

  x_axis = g_hash_table_get_keys (self->logical_monitors);
  x_axis = g_list_sort (x_axis, sort_x_axis);
  y_axis = g_hash_table_get_keys (self->logical_monitors);
  y_axis = g_list_sort (y_axis, sort_y_axis);

  m = x_axis->data;
  if (m->x != 0)
    g_list_foreach (x_axis, add_x_delta, GINT_TO_POINTER (- m->x));

  m = y_axis->data;
  if (m->y != 0)
    g_list_foreach (y_axis, add_y_delta, GINT_TO_POINTER (- m->y));

  g_list_free (x_axis);
  g_list_free (y_axis);
}

static void
cc_display_config_dbus_append_right (CcDisplayConfigDBus *self,
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
  x_axis = g_list_sort (x_axis, sort_x_axis);
  last = g_list_last (x_axis)->data;
  monitor->x = last->x + logical_monitor_width (last);
  monitor->y = last->y;

  g_list_free (x_axis);
}

static void
cc_display_config_dbus_make_linear (CcDisplayConfigDBus *self)
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
