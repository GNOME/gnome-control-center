/* -*- mode: c; style: linux -*- */

/* util.c
 * Copyright (C) 2000-2001 Ximian, Inc.
 *
 * Written by Bradford Hovinen (hovinen@ximian.com)
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

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <ctype.h>
#include <stdlib.h>
#include <string.h>

#include "util.h"

/* Read a fixed-digit number from a string, advancing the string
 * pointer. Return TRUE if the extraction was successful, FALSE if
 * there was no number to extract or if the number was too short.
 */

gboolean
extract_number (char **str, int *number, int digits) 
{
	char buf[64];

	if (!isdigit (**str)) return FALSE;
	if (digits > 63) digits = 63;

	strncpy (buf, *str, digits);
	buf[digits] = '\0';
	*number = atoi (buf);
	if (strlen (buf) < digits) return FALSE;
	*str += digits;
	return TRUE;
}

struct tm *
parse_date (char *str) 
{
	struct tm *date;
	gboolean ok;
	gint value;

	ok = extract_number (&str, &value, 4);
	if (!ok) return NULL;

	date = g_new (struct tm, 1);
	date->tm_year = value - 1900;
	date->tm_mon = 11;
	date->tm_mday = 31;
	date->tm_hour = 23;
	date->tm_min = 59;
	date->tm_sec = 59;

	if (extract_number (&str, &value, 2))
		date->tm_mon = value - 1;
	else
		return date;

	if (extract_number (&str, &value, 2))
		date->tm_mday = value;
	else
		return date;

	if (extract_number (&str, &value, 2))
		date->tm_hour = value;
	else
		return date;

	if (extract_number (&str, &value, 2))
		date->tm_min = value;
	else
		return date;

	if (extract_number (&str, &value, 2))
		date->tm_sec = value;

	return date;
}

struct tm *
dup_date (const struct tm *date) 
{
	struct tm *date1;

	date1 = g_new (struct tm, 1);
	memcpy (date1, date, sizeof (struct tm));
	return date1;
}
