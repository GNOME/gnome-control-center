/* -*- mode: c; style: linux -*- */

/* util.h
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

#ifndef __UTIL_H
#define __UTIL_H

#include <time.h>
#include <glib.h>

/* Uncomment this if you want debugs: */
#define DEBUG_ME_MORE

#ifdef DEBUG_ME_MORE
#  ifdef __GNUC__
#    define DEBUG_MSG(str, args...) \
              g_log (G_LOG_DOMAIN, G_LOG_LEVEL_DEBUG, "(%d:%s) " str, \
		     getpid (), __FUNCTION__ , ## args)
#  else
#    define DEBUG_MSG(str, args...)
#  endif
#else
/* This was redefined here because it was messing with the frontend->backend 
 * talk. Arturo */
#  define DEBUG_MSG(str, args...)
#endif


gboolean extract_number (char **str, int *number, int digits);
struct tm *parse_date (char *str);

#endif /* __UTIL_H */
