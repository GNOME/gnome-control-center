/* -*- mode: c; style: linux -*- */

/* expr.h
 * Copyright (C) 2000 Helix Code, Inc.
 *
 * Written by Bradford Hovinen (hovinen@helixcode.com)
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

#ifndef __EXPR_H
#define __EXPR_H

#include <glib.h>

#define SYMBOL_AND ((gpointer) -1)
#define SYMBOL_OR  ((gpointer) -2)
#define SYMBOL_NOT ((gpointer) -3)

gboolean parse_sentence (gchar *sentence, GScanner *scanner);
gdouble parse_expr (gchar *expr, gdouble var);

#endif /* __EXPR_H */
