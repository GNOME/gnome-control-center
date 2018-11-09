/* main.c
 *
 * Copyright 2018 Georges Basile Stavracas Neto <georges.stavracas@gmail.com>
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

#include "config.h"

#include <stdlib.h>
#include <glib/gi18n.h>
#include <gtk/gtk.h>

#include "shell/cc-application.h"
#include "shell/cc-panel-loader.h"
#include "shell/resources.h"
#include "test-panels-resources.h"

#include "gtp-dynamic-panel.h"
#include "gtp-header-widget.h"
#include "gtp-static-init.h"

/* Test panels */
static CcPanelLoaderVtable test_panels[] = {
  { "dynamic-panel", gtp_dynamic_panel_get_type, NULL                 },
  { "header-widget", gtp_header_widget_get_type, NULL                 },
  { "static-init",   gtp_static_init_get_type,   gtp_static_init_func },
};

gint
main (gint   argc,
      gchar *argv[])
{
  g_autoptr(GtkApplication) application = NULL;

  /* Manually register GResources */
  g_resources_register (gnome_control_center_get_resource ());
  g_resources_register (test_panels_get_resource ());

  /* Override the panels vtable with the test panels */
  cc_panel_loader_override_vtable (test_panels, G_N_ELEMENTS (test_panels));

  application = cc_application_new ();

  return g_application_run (G_APPLICATION (application), argc, argv);
}
