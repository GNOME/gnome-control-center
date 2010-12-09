/*
 * Copyright (C) 2010 Intel, Inc
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
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 *
 * Author: Thomas Wood <thomas.wood@intel.com>
 *
 */

#include <config.h>

#include "cc-datetime-panel.h"

#include <glib/gi18n-lib.h>

#define GETTEXT_PACKAGE_TIMEZONES GETTEXT_PACKAGE "-timezones"

void
g_io_module_load (GIOModule *module)
{
  bindtextdomain (GETTEXT_PACKAGE, GNOMELOCALEDIR);
  bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");

  bindtextdomain (GETTEXT_PACKAGE_TIMEZONES, GNOMELOCALEDIR);
  bind_textdomain_codeset (GETTEXT_PACKAGE_TIMEZONES, "UTF-8");

  /* register the panel */
  cc_date_time_panel_register (module);
}

void
g_io_module_unload (GIOModule *module)
{
}
