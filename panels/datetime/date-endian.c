/*
 * Copyright (C) 2011 Red Hat, Inc.
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
 * Author: Bastien Nocera <hadess@hadess.net>
 *
 */

#include <langinfo.h>
#include <locale.h>
#include <glib.h>

#include "date-endian.h"

DateEndianess
date_endian_get_default (void)
{
	const char *fmt;

	fmt = nl_langinfo (D_FMT);
	g_return_val_if_fail (fmt != NULL, DATE_ENDIANESS_MIDDLE);

	/* FIXME, implement */

	return DATE_ENDIANESS_MIDDLE;
}

DateEndianess
date_endian_get_for_lang (const char *lang)
{
	const char *old_lang;
	DateEndianess endian;

	old_lang = setlocale (LC_TIME, lang);
	endian = date_endian_get_default ();
	setlocale (LC_TIME, old_lang);

	return endian;
}

