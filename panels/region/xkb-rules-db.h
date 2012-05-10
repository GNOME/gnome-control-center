/*
 * Copyright (C) 2012 Red Hat, Inc.
 *
 * Written by: Rui Matos <rmatos@redhat.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
 * 02111-1307, USA.
 */

#ifndef __XKB_RULES_DB_H__
#define __XKB_RULES_DB_H__

#include <glib.h>

GSList         *xkb_rules_db_get_all_layout_names       (void);
gboolean        xkb_rules_db_get_layout_info            (const gchar  *name,
                                                         const gchar **short_name,
                                                         const gchar **xkb_layout,
                                                         const gchar **xkb_variant);
gboolean        xkb_rules_db_get_layout_info_for_language (const gchar  *language,
                                                           const gchar **name,
                                                           const gchar **short_name,
                                                           const gchar **xkb_layout,
                                                           const gchar **xkb_variant);

#endif  /* __XKB_RULES_DB_H__ */
