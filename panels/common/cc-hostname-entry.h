/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*-
 *
 * Copyright (C) 2013 Intel, Inc
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
 */

#pragma once

#include <adwaita.h>

G_BEGIN_DECLS

#define CC_TYPE_HOSTNAME_ENTRY (cc_hostname_entry_get_type())
G_DECLARE_FINAL_TYPE (CcHostnameEntry, cc_hostname_entry, CC, HOSTNAME_ENTRY, AdwEntryRow)

CcHostnameEntry *cc_hostname_entry_new (void);

G_END_DECLS
