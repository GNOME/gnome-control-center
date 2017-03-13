/* -*- Mode: C; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*- */
/* NetworkManager -- Network link manager
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301 USA.
 *
 * (C) Copyright 2015 Red Hat, Inc.
 */

#ifndef __NM_DEFAULT_H__
#define __NM_DEFAULT_H__

#define LIBNM_GLIB_BUILD
#define NETWORKMANAGER_COMPILATION

/*****************************************************************************/

/* always include these headers for our internal source files. */

#ifndef ___CONFIG_H__
#define ___CONFIG_H__
#include <config.h>
#endif

#include <stdlib.h>

#include <gtk/gtk.h>

#include <libnm/nm-connection.h>
#include <libnm/nm-setting-wireless-security.h>
#include <libnm/nm-setting-8021x.h>

static inline gboolean
nm_clear_g_source (guint *id)
{
	if (id && *id) {
		g_source_remove (*id);
		*id = 0;
		return TRUE;
	}
	return FALSE;
}

/*****************************************************************************/

#include <glib/gi18n.h>

/*****************************************************************************/

#endif /* __NM_DEFAULT_H__ */
