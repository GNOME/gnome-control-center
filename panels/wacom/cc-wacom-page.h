/*
 * Copyright © 2011 Red Hat, Inc.
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
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 *
 * Authors: Peter Hutterer <peter.hutterer@redhat.com>
 *          Bastien Nocera <hadess@hadess.net>
 */

#pragma once

#include <gtk/gtk.h>
#include "cc-wacom-panel.h"
#include "cc-wacom-device.h"

G_BEGIN_DECLS

#define CC_TYPE_WACOM_PAGE (cc_wacom_page_get_type ())
G_DECLARE_FINAL_TYPE (CcWacomPage, cc_wacom_page, CC, WACOM_PAGE, GtkBox)

GtkWidget * cc_wacom_page_new (CcWacomPanel  *panel,
			       CcWacomDevice *stylus);

void cc_wacom_page_set_navigation (CcWacomPage *page,
				   GtkNotebook *notebook,
				   gboolean     ignore_first_page);

void        cc_wacom_page_calibrate        (CcWacomPage *page);

gboolean    cc_wacom_page_can_calibrate    (CcWacomPage *page);

G_END_DECLS
