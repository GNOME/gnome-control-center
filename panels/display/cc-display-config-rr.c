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

#include "cc-display-config-rr.h"

#define GNOME_DESKTOP_USE_UNSTABLE_API
#include <libgnome-desktop/gnome-rr.h>
#include <libgnome-desktop/gnome-rr-config.h>

struct _CcDisplayModeRR
{
  CcDisplayMode parent_instance;

  GnomeRRMode *rr_mode;
};

G_DEFINE_TYPE (CcDisplayModeRR,
               cc_display_mode_rr,
               CC_TYPE_DISPLAY_MODE)

static void
cc_display_mode_rr_get_resolution (CcDisplayMode *pself,
                                   int *w, int *h)
{
  CcDisplayModeRR *self = CC_DISPLAY_MODE_RR (pself);

  if (w)
    *w = gnome_rr_mode_get_width (self->rr_mode);
  if (h)
    *h = gnome_rr_mode_get_height (self->rr_mode);
}

static double rr_supported_scales[] = { 1.0, 0.0 };

static const double *
cc_display_mode_rr_get_supported_scales (CcDisplayMode *pself)
{
  return rr_supported_scales;
}

static double
cc_display_mode_rr_get_preferred_scale (CcDisplayMode *pself)
{
  return rr_supported_scales[0];
}

static gboolean
cc_display_mode_rr_is_interlaced (CcDisplayMode *pself)
{
  CcDisplayModeRR *self = CC_DISPLAY_MODE_RR (pself);

  return gnome_rr_mode_get_is_interlaced (self->rr_mode);
}

static int
cc_display_mode_rr_get_freq (CcDisplayMode *pself)
{
  CcDisplayModeRR *self = CC_DISPLAY_MODE_RR (pself);

  return gnome_rr_mode_get_freq (self->rr_mode);
}

static double
cc_display_mode_rr_get_freq_f (CcDisplayMode *pself)
{
  CcDisplayModeRR *self = CC_DISPLAY_MODE_RR (pself);

  return gnome_rr_mode_get_freq_f (self->rr_mode);
}

static void
cc_display_mode_rr_init (CcDisplayModeRR *self)
{
}

static void
cc_display_mode_rr_finalize (GObject *object)
{
  G_OBJECT_CLASS (cc_display_mode_rr_parent_class)->finalize (object);
}

static void
cc_display_mode_rr_class_init (CcDisplayModeRRClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  CcDisplayModeClass *parent_class = CC_DISPLAY_MODE_CLASS (klass);

  gobject_class->finalize = cc_display_mode_rr_finalize;

  parent_class->get_resolution = cc_display_mode_rr_get_resolution;
  parent_class->get_supported_scales = cc_display_mode_rr_get_supported_scales;
  parent_class->get_preferred_scale = cc_display_mode_rr_get_preferred_scale;
  parent_class->is_interlaced = cc_display_mode_rr_is_interlaced;
  parent_class->get_freq = cc_display_mode_rr_get_freq;
  parent_class->get_freq_f = cc_display_mode_rr_get_freq_f;
}

static CcDisplayMode *
cc_display_mode_rr_new (GnomeRRMode *mode)
{
  CcDisplayModeRR *self = g_object_new (CC_TYPE_DISPLAY_MODE_RR, NULL);
  self->rr_mode = mode;

  return CC_DISPLAY_MODE (self);
}


static const GnomeRRRotation rotation_map[] =
{
    GNOME_RR_ROTATION_0,
    GNOME_RR_ROTATION_90,
    GNOME_RR_ROTATION_180,
    GNOME_RR_ROTATION_270,
    GNOME_RR_REFLECT_X | GNOME_RR_ROTATION_0,
    GNOME_RR_REFLECT_X | GNOME_RR_ROTATION_90,
    GNOME_RR_REFLECT_X | GNOME_RR_ROTATION_180,
    GNOME_RR_REFLECT_X | GNOME_RR_ROTATION_270,
};

struct _CcDisplayMonitorRR
{
  CcDisplayMonitor parent_instance;
  CcDisplayConfigRR *config;

  GnomeRROutput *output;
  GnomeRROutputInfo *output_info;

  GList *modes;
  CcDisplayMode *current_mode;
  CcDisplayMode *preferred_mode;
};

G_DEFINE_TYPE (CcDisplayMonitorRR,
               cc_display_monitor_rr,
               CC_TYPE_DISPLAY_MONITOR)

static void
cc_display_config_rr_set_primary (CcDisplayConfigRR *self,
                                  CcDisplayMonitorRR *new_primary);
static void
cc_display_config_rr_unset_primary (CcDisplayConfigRR *self,
                                    CcDisplayMonitorRR *old_primary);

static const char *
cc_display_monitor_rr_get_display_name (CcDisplayMonitor *pself)
{
  CcDisplayMonitorRR *self = CC_DISPLAY_MONITOR_RR (pself);

  return gnome_rr_output_info_get_display_name (self->output_info);
}

static const char *
cc_display_monitor_rr_get_connector_name (CcDisplayMonitor *pself)
{
  CcDisplayMonitorRR *self = CC_DISPLAY_MONITOR_RR (pself);

  return gnome_rr_output_get_name (self->output);
}

static gboolean
cc_display_monitor_rr_is_builtin (CcDisplayMonitor *pself)
{
  CcDisplayMonitorRR *self = CC_DISPLAY_MONITOR_RR (pself);

  return gnome_rr_output_is_builtin_display (self->output);
}

static gboolean
cc_display_monitor_rr_is_primary (CcDisplayMonitor *pself)
{
  CcDisplayMonitorRR *self = CC_DISPLAY_MONITOR_RR (pself);

  return gnome_rr_output_info_get_primary (self->output_info);
}

static void
cc_display_monitor_rr_set_primary (CcDisplayMonitor *pself,
                                   gboolean primary)
{
  CcDisplayMonitorRR *self = CC_DISPLAY_MONITOR_RR (pself);

  if (primary)
    cc_display_config_rr_set_primary (self->config, self);
  else
    cc_display_config_rr_unset_primary (self->config, self);
}

static gboolean
cc_display_monitor_rr_is_active (CcDisplayMonitor *pself)
{
  CcDisplayMonitorRR *self = CC_DISPLAY_MONITOR_RR (pself);

  return gnome_rr_output_info_is_active (self->output_info);
}

static void
cc_display_monitor_rr_set_active (CcDisplayMonitor *pself,
                                  gboolean active)
{
  CcDisplayMonitorRR *self = CC_DISPLAY_MONITOR_RR (pself);

  gnome_rr_output_info_set_active (self->output_info, active);
  g_signal_emit_by_name (self, "active");
}

static CcDisplayRotation
cc_display_monitor_rr_get_rotation (CcDisplayMonitor *pself)
{
  CcDisplayMonitorRR *self = CC_DISPLAY_MONITOR_RR (pself);
  int i;

  for (i = 0; i < G_N_ELEMENTS (rotation_map); ++i)
    if (rotation_map[i] == gnome_rr_output_info_get_rotation (self->output_info))
      return i;

  return 0;
}

static void
cc_display_monitor_rr_set_rotation (CcDisplayMonitor *pself,
                                    CcDisplayRotation rotation)
{
  CcDisplayMonitorRR *self = CC_DISPLAY_MONITOR_RR (pself);

  gnome_rr_output_info_set_rotation (self->output_info,
                                     rotation_map[rotation]);
  g_signal_emit_by_name (self, "rotation");
}

static gboolean
cc_display_monitor_rr_supports_rotation (CcDisplayMonitor *pself,
                                         CcDisplayRotation rotation)
{
  CcDisplayMonitorRR *self = CC_DISPLAY_MONITOR_RR (pself);

  return gnome_rr_output_info_supports_rotation (self->output_info,
                                                 rotation_map[rotation]);
}

static void
cc_display_monitor_rr_get_physical_size (CcDisplayMonitor *pself,
                                         int *w, int *h)
{
  CcDisplayMonitorRR *self = CC_DISPLAY_MONITOR_RR (pself);

  return gnome_rr_output_get_physical_size (self->output, w, h);
}

static void
cc_display_monitor_rr_get_geometry (CcDisplayMonitor *pself,
                                    int *x, int *y, int *w, int *h)
{
  CcDisplayMonitorRR *self = CC_DISPLAY_MONITOR_RR (pself);

  return gnome_rr_output_info_get_geometry (self->output_info, x, y, w, h);
}

static CcDisplayMode *
cc_display_monitor_rr_get_mode (CcDisplayMonitor *pself)
{
  CcDisplayMonitorRR *self = CC_DISPLAY_MONITOR_RR (pself);

  return self->current_mode;
}

static CcDisplayMode *
cc_display_monitor_rr_get_preferred_mode (CcDisplayMonitor *pself)
{
  CcDisplayMonitorRR *self = CC_DISPLAY_MONITOR_RR (pself);

  return self->preferred_mode;
}

static guint32
cc_display_monitor_rr_get_id (CcDisplayMonitor *pself)
{
  CcDisplayMonitorRR *self = CC_DISPLAY_MONITOR_RR (pself);

  return gnome_rr_output_get_id (self->output);
}

static GList *
cc_display_monitor_rr_get_modes (CcDisplayMonitor *pself)
{
  CcDisplayMonitorRR *self = CC_DISPLAY_MONITOR_RR (pself);

  return self->modes;
}

static gboolean
cc_display_monitor_rr_supports_underscanning (CcDisplayMonitor *pself)
{
  CcDisplayMonitorRR *self = CC_DISPLAY_MONITOR_RR (pself);

  return gnome_rr_output_supports_underscanning (self->output);
}

static gboolean
cc_display_monitor_rr_get_underscanning (CcDisplayMonitor *pself)
{
  CcDisplayMonitorRR *self = CC_DISPLAY_MONITOR_RR (pself);

  return gnome_rr_output_info_get_underscanning (self->output_info);
}

static void
cc_display_monitor_rr_set_underscanning (CcDisplayMonitor *pself,
                                         gboolean underscanning)
{
  CcDisplayMonitorRR *self = CC_DISPLAY_MONITOR_RR (pself);

  return gnome_rr_output_info_set_underscanning (self->output_info, underscanning);
}

static void
cc_display_monitor_rr_set_mode (CcDisplayMonitor *pself,
                                CcDisplayMode *mode)
{
  CcDisplayMonitorRR *self = CC_DISPLAY_MONITOR_RR (pself);
  int x, y, w, h;

  gnome_rr_output_info_get_geometry (self->output_info, &x, &y, NULL, NULL);
  cc_display_mode_get_resolution (mode, &w, &h);
  gnome_rr_output_info_set_geometry (self->output_info, x, y, w, h);

  gnome_rr_output_info_set_refresh_rate (self->output_info,
                                         cc_display_mode_get_freq (mode));
  g_signal_emit_by_name (self, "mode");
}

static void
cc_display_monitor_rr_set_position (CcDisplayMonitor *pself,
                                    int x, int y)
{
  CcDisplayMonitorRR *self = CC_DISPLAY_MONITOR_RR (pself);
  int w, h;

  gnome_rr_output_info_get_geometry (self->output_info, NULL, NULL, &w, &h);
  gnome_rr_output_info_set_geometry (self->output_info, x, y, w, h);
}

static double
cc_display_monitor_rr_get_scale (CcDisplayMonitor *pself)
{
  return rr_supported_scales[0];
}

static void
cc_display_monitor_rr_set_scale (CcDisplayMonitor *pself,
                                 double scale)
{
}

static void
cc_display_monitor_rr_init (CcDisplayMonitorRR *self)
{
}

static void
cc_display_monitor_rr_finalize (GObject *object)
{
  CcDisplayMonitorRR *self = CC_DISPLAY_MONITOR_RR (object);

  g_list_foreach (self->modes, (GFunc) g_object_unref, NULL);
  g_clear_pointer (&self->modes, g_list_free);

  G_OBJECT_CLASS (cc_display_monitor_rr_parent_class)->finalize (object);
}

static void
cc_display_monitor_rr_class_init (CcDisplayMonitorRRClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  CcDisplayMonitorClass *parent_class = CC_DISPLAY_MONITOR_CLASS (klass);

  gobject_class->finalize = cc_display_monitor_rr_finalize;

  parent_class->get_display_name = cc_display_monitor_rr_get_display_name;
  parent_class->get_connector_name = cc_display_monitor_rr_get_connector_name;
  parent_class->is_builtin = cc_display_monitor_rr_is_builtin;
  parent_class->is_primary = cc_display_monitor_rr_is_primary;
  parent_class->set_primary = cc_display_monitor_rr_set_primary;
  parent_class->is_active = cc_display_monitor_rr_is_active;
  parent_class->set_active = cc_display_monitor_rr_set_active;
  parent_class->get_rotation = cc_display_monitor_rr_get_rotation;
  parent_class->set_rotation = cc_display_monitor_rr_set_rotation;
  parent_class->supports_rotation = cc_display_monitor_rr_supports_rotation;
  parent_class->get_physical_size = cc_display_monitor_rr_get_physical_size;
  parent_class->get_geometry = cc_display_monitor_rr_get_geometry;
  parent_class->get_mode = cc_display_monitor_rr_get_mode;
  parent_class->get_preferred_mode = cc_display_monitor_rr_get_preferred_mode;
  parent_class->get_id = cc_display_monitor_rr_get_id;
  parent_class->get_modes = cc_display_monitor_rr_get_modes;
  parent_class->supports_underscanning = cc_display_monitor_rr_supports_underscanning;
  parent_class->get_underscanning = cc_display_monitor_rr_get_underscanning;
  parent_class->set_underscanning = cc_display_monitor_rr_set_underscanning;
  parent_class->set_mode = cc_display_monitor_rr_set_mode;
  parent_class->set_position = cc_display_monitor_rr_set_position;
  parent_class->get_scale = cc_display_monitor_rr_get_scale;
  parent_class->set_scale = cc_display_monitor_rr_set_scale;
}

static CcDisplayMonitor *
cc_display_monitor_rr_new (GnomeRROutput     *output,
                           GnomeRROutputInfo *output_info,
                           CcDisplayConfigRR *config)
{
  CcDisplayMonitorRR *self = g_object_new (CC_TYPE_DISPLAY_MONITOR_RR, NULL);
  GnomeRRMode **modes = gnome_rr_output_list_modes (output);
  GnomeRRMode *preferred_mode = gnome_rr_output_get_preferred_mode (output);
  GnomeRRMode *current_mode = gnome_rr_output_get_current_mode (output);
  gint i;

  self->config = config;
  self->output = output;
  self->output_info = output_info;

  for (i = 0; modes[i] != NULL; ++i)
    {
      CcDisplayMode *mode = cc_display_mode_rr_new (modes[i]);
      self->modes = g_list_prepend (self->modes, mode);

      if (current_mode &&
          gnome_rr_mode_get_id (current_mode) == gnome_rr_mode_get_id (modes[i]))
        self->current_mode = mode;

      if (preferred_mode &&
          gnome_rr_mode_get_id (preferred_mode) == gnome_rr_mode_get_id (modes[i]))
        self->preferred_mode = mode;
    }

  return CC_DISPLAY_MONITOR (self);
}

struct _CcDisplayConfigRR
{
  CcDisplayConfig parent_instance;

  GnomeRRScreen *rr_screen;
  GnomeRRConfig *rr_config;

  GList *monitors;
  CcDisplayMonitorRR *primary;
  GList *clone_modes;
};

G_DEFINE_TYPE (CcDisplayConfigRR,
               cc_display_config_rr,
               CC_TYPE_DISPLAY_CONFIG)

enum
{
  PROP_0,
  PROP_GNOME_RR_SCREEN,
};

static void
cc_display_config_rr_set_primary (CcDisplayConfigRR *self,
                                  CcDisplayMonitorRR *new_primary)
{
  if (self->primary == new_primary)
    return;

  gnome_rr_output_info_set_primary (self->primary->output_info, FALSE);
  g_signal_emit_by_name (self->primary, "primary");

  self->primary = new_primary;
  gnome_rr_output_info_set_primary (self->primary->output_info, TRUE);
  g_signal_emit_by_name (self->primary, "primary");

  g_signal_emit_by_name (self, "primary");
}

static void
cc_display_config_rr_unset_primary (CcDisplayConfigRR *self,
                                    CcDisplayMonitorRR *old_primary)
{
  GList *l;

  if (self->primary != old_primary)
    return;

  for (l = self->monitors; l != NULL; l = l->next)
    {
      CcDisplayMonitorRR *monitor = l->data;
      if (gnome_rr_output_info_is_active (monitor->output_info) &&
          monitor != old_primary)
        {
          cc_display_config_rr_set_primary (self, monitor);
          break;
        }
    }
}

static GList *
cc_display_config_rr_get_monitors (CcDisplayConfig *pself)
{
  CcDisplayConfigRR *self = CC_DISPLAY_CONFIG_RR (pself);

  return self->monitors;
}

static gboolean
cc_display_config_rr_is_applicable (CcDisplayConfig *pself)
{
  CcDisplayConfigRR *self = CC_DISPLAY_CONFIG_RR (pself);

  return gnome_rr_config_applicable (self->rr_config, self->rr_screen, NULL);
}

static gboolean
cc_display_config_rr_equal (CcDisplayConfig *pself,
                            CcDisplayConfig *pother)
{
  CcDisplayConfigRR *self = CC_DISPLAY_CONFIG_RR (pself);
  CcDisplayConfigRR *other = CC_DISPLAY_CONFIG_RR (pother);
  gboolean same_modes, same_primary, same_cloning;

  same_modes = gnome_rr_config_equal (self->rr_config, other->rr_config);
  same_primary = (gnome_rr_output_get_id (self->primary->output) ==
                  gnome_rr_output_get_id (other->primary->output));
  same_cloning = (gnome_rr_config_get_clone (self->rr_config) ==
                  gnome_rr_config_get_clone (other->rr_config));
  return same_modes && same_primary && same_cloning;
}

static gboolean
cc_display_config_rr_apply (CcDisplayConfig *pself,
                            GError **error)
{
  CcDisplayConfigRR *self = CC_DISPLAY_CONFIG_RR (pself);

  gnome_rr_config_sanitize (self->rr_config);
  gnome_rr_config_ensure_primary (self->rr_config);

  return gnome_rr_config_apply_persistent (self->rr_config, self->rr_screen, error);
}

static gboolean
cc_display_config_rr_is_cloning (CcDisplayConfig *pself)
{
  CcDisplayConfigRR *self = CC_DISPLAY_CONFIG_RR (pself);

  return gnome_rr_config_get_clone (self->rr_config);
}

static void
cc_display_config_rr_set_cloning (CcDisplayConfig *pself,
                                  gboolean clone)
{
  CcDisplayConfigRR *self = CC_DISPLAY_CONFIG_RR (pself);

  return gnome_rr_config_set_clone (self->rr_config, clone);
}

static GList *
cc_display_config_rr_get_cloning_modes (CcDisplayConfig *pself)
{
  CcDisplayConfigRR *self = CC_DISPLAY_CONFIG_RR (pself);

  return self->clone_modes;
}

static gboolean
cc_display_config_rr_is_layout_logical (CcDisplayConfig *pself)
{
  return FALSE;
}

static void
cc_display_config_rr_init (CcDisplayConfigRR *self)
{
}

static void
cc_display_config_rr_constructed (GObject *object)
{
  CcDisplayConfigRR *self = CC_DISPLAY_CONFIG_RR (object);
  GnomeRROutputInfo **output_infos;
  GnomeRRMode **clone_modes;
  gint i;

  self->rr_config = gnome_rr_config_new_current (self->rr_screen, NULL);
  gnome_rr_config_ensure_primary (self->rr_config);

  output_infos = gnome_rr_config_get_outputs (self->rr_config);

  for (i = 0; output_infos[i] != NULL; ++i)
    {
      GnomeRROutput *output;
      CcDisplayMonitor *monitor;

      if (!gnome_rr_output_info_is_primary_tile (output_infos[i]))
        continue;

      output = gnome_rr_screen_get_output_by_name (self->rr_screen,
                                                   gnome_rr_output_info_get_name (output_infos[i]));

      monitor = cc_display_monitor_rr_new (output, output_infos[i], self);
      self->monitors = g_list_prepend (self->monitors, monitor);

      if (cc_display_monitor_is_primary (monitor))
        self->primary = CC_DISPLAY_MONITOR_RR (monitor);
    }

  clone_modes = gnome_rr_screen_list_clone_modes (self->rr_screen);

  for (i = 0; clone_modes[i] != NULL; ++i)
    {
      CcDisplayMode *mode = cc_display_mode_rr_new (clone_modes[i]);
      self->clone_modes = g_list_prepend (self->clone_modes, mode);
    }

  G_OBJECT_CLASS (cc_display_config_rr_parent_class)->constructed (object);
}

static void
cc_display_config_rr_set_property (GObject      *object,
                                   guint         prop_id,
                                   const GValue *value,
                                   GParamSpec   *pspec)
{
  CcDisplayConfigRR *self = CC_DISPLAY_CONFIG_RR (object);

  switch (prop_id)
    {
    case PROP_GNOME_RR_SCREEN:
      self->rr_screen = g_value_dup_object (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
cc_display_config_rr_get_property (GObject    *object,
                                   guint       prop_id,
                                   GValue     *value,
                                   GParamSpec *pspec)
{
  CcDisplayConfigRR *self = CC_DISPLAY_CONFIG_RR (object);

  switch (prop_id)
    {
    case PROP_GNOME_RR_SCREEN:
      g_value_set_object (value, self->rr_screen);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
cc_display_config_rr_finalize (GObject *object)
{
  CcDisplayConfigRR *self = CC_DISPLAY_CONFIG_RR (object);

  g_clear_object (&self->rr_screen);
  g_clear_object (&self->rr_config);

  g_list_foreach (self->monitors, (GFunc) g_object_unref, NULL);
  g_clear_pointer (&self->monitors, g_list_free);

  g_list_foreach (self->clone_modes, (GFunc) g_object_unref, NULL);
  g_clear_pointer (&self->clone_modes, g_list_free);

  G_OBJECT_CLASS (cc_display_config_rr_parent_class)->finalize (object);
}

static void
cc_display_config_rr_class_init (CcDisplayConfigRRClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  CcDisplayConfigClass *parent_class = CC_DISPLAY_CONFIG_CLASS (klass);
  GParamSpec *pspec;

  gobject_class->constructed = cc_display_config_rr_constructed;
  gobject_class->set_property = cc_display_config_rr_set_property;
  gobject_class->get_property = cc_display_config_rr_get_property;
  gobject_class->finalize = cc_display_config_rr_finalize;

  parent_class->get_monitors = cc_display_config_rr_get_monitors;
  parent_class->is_applicable = cc_display_config_rr_is_applicable;
  parent_class->equal = cc_display_config_rr_equal;
  parent_class->apply = cc_display_config_rr_apply;
  parent_class->is_cloning = cc_display_config_rr_is_cloning;
  parent_class->set_cloning = cc_display_config_rr_set_cloning;
  parent_class->get_cloning_modes = cc_display_config_rr_get_cloning_modes;
  parent_class->is_layout_logical = cc_display_config_rr_is_layout_logical;

  pspec = g_param_spec_object ("gnome-rr-screen",
                               "GnomeRRScreen",
                               "GnomeRRScreen",
                               GNOME_TYPE_RR_SCREEN,
                               G_PARAM_READWRITE |
                               G_PARAM_STATIC_STRINGS |
                               G_PARAM_CONSTRUCT_ONLY);
  g_object_class_install_property (gobject_class, PROP_GNOME_RR_SCREEN, pspec);
}
