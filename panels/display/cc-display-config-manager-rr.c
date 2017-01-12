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
#include "cc-display-config-manager-rr.h"

#define GNOME_DESKTOP_USE_UNSTABLE_API
#include <libgnome-desktop/gnome-rr.h>

struct _CcDisplayConfigManagerRR
{
  CcDisplayConfigManager parent_instance;

  GnomeRRScreen *rr_screen;
};

G_DEFINE_TYPE (CcDisplayConfigManagerRR,
               cc_display_config_manager_rr,
               CC_TYPE_DISPLAY_CONFIG_MANAGER)

static CcDisplayConfig *
cc_display_config_manager_rr_get_current (CcDisplayConfigManager *pself)
{
  CcDisplayConfigManagerRR *self = CC_DISPLAY_CONFIG_MANAGER_RR (pself);

  if (!self->rr_screen)
    return NULL;

  return g_object_new (CC_TYPE_DISPLAY_CONFIG_RR,
                       "gnome-rr-screen", self->rr_screen, NULL);
}

static void
screen_changed (CcDisplayConfigManagerRR *self)
{
  gnome_rr_screen_refresh (self->rr_screen, NULL);
  _cc_display_config_manager_emit_changed (CC_DISPLAY_CONFIG_MANAGER (self));
}

static void
screen_ready (GObject      *object,
              GAsyncResult *result,
              gpointer      data)
{
  CcDisplayConfigManagerRR *self = CC_DISPLAY_CONFIG_MANAGER_RR (data);
  GError *error = NULL;

  self->rr_screen = gnome_rr_screen_new_finish (result, &error);
  if (!self->rr_screen)
    {
      g_warning ("Error obtaining GnomeRRScreen: %s", error->message);
      g_clear_error (&error);
    }
  else
    {
      g_signal_connect_object (self->rr_screen, "changed",
                               G_CALLBACK (screen_changed),
                               self,
                               G_CONNECT_SWAPPED);
      gnome_rr_screen_refresh (self->rr_screen, NULL);
    }

  _cc_display_config_manager_emit_changed (CC_DISPLAY_CONFIG_MANAGER (self));
  g_object_unref (self);
}

static void
cc_display_config_manager_rr_init (CcDisplayConfigManagerRR *self)
{
  gnome_rr_screen_new_async (gdk_screen_get_default (),
                             screen_ready,
                             g_object_ref (self));
}

static void
cc_display_config_manager_rr_finalize (GObject *object)
{
  CcDisplayConfigManagerRR *self = CC_DISPLAY_CONFIG_MANAGER_RR (object);

  g_clear_object (&self->rr_screen);

  G_OBJECT_CLASS (cc_display_config_manager_rr_parent_class)->finalize (object);
}

static void
cc_display_config_manager_rr_class_init (CcDisplayConfigManagerRRClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  CcDisplayConfigManagerClass *parent_class = CC_DISPLAY_CONFIG_MANAGER_CLASS (klass);

  gobject_class->finalize = cc_display_config_manager_rr_finalize;

  parent_class->get_current = cc_display_config_manager_rr_get_current;
}

CcDisplayConfigManager *
cc_display_config_manager_rr_new (void)
{
  return g_object_new (CC_TYPE_DISPLAY_CONFIG_MANAGER_RR, NULL);
}
