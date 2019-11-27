/* bg-pictures-source.h */
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
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 *
 * Author: Thomas Wood <thomas.wood@intel.com>
 *
 */

#pragma once

#include <gtk/gtk.h>
#include "bg-source.h"
#include "cc-background-item.h"

G_BEGIN_DECLS

#define BG_TYPE_PICTURES_SOURCE (bg_pictures_source_get_type ())
G_DECLARE_FINAL_TYPE (BgPicturesSource, bg_pictures_source, BG, PICTURES_SOURCE, BgSource)

BgPicturesSource *bg_pictures_source_new            (GtkWidget *widget);
char             *bg_pictures_source_get_cache_path (void);
char             *bg_pictures_source_get_unique_path(const char *uri);
gboolean          bg_pictures_source_add            (BgPicturesSource     *bg_source,
						     const char           *uri,
						     GtkTreeRowReference **ret_row_ref);
gboolean          bg_pictures_source_remove         (BgPicturesSource *bg_source,
						     const char       *uri);
gboolean          bg_pictures_source_is_known       (BgPicturesSource *bg_source,
						     const char       *uri);

const char * const * bg_pictures_get_support_content_types (void);

G_END_DECLS
