/* -*- mode: c; style: linux -*- */

/* expr.c
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

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include "expr.h"

static gdouble  int_parse_sentence (GScanner *scanner);
static gdouble  int_parse_unary    (GScanner *scanner);
static gdouble  int_parse_atom     (GScanner *scanner);
static gdouble  int_parse_expr     (GScanner *scanner, gboolean expr,
				    gboolean neg);
static gdouble  int_parse_term     (GScanner *scanner, gboolean expr, 
				    gboolean inv);
static gdouble  int_parse_factor   (GScanner *scanner, gboolean expr);

gboolean 
parse_sentence (gchar *sentence, GScanner *scanner) 
{
	g_scanner_input_text (scanner, sentence, strlen (sentence));
	g_scanner_get_next_token (scanner);
	return int_parse_sentence (scanner) != 0.0;
}

gdouble
parse_expr (gchar *expr, gdouble var) 
{
	static GScannerConfig config;
	GScanner *scanner;
	gchar *var_string;
	gfloat ret;

	config.cset_skip_characters = " \t\n";
	config.cset_identifier_first = "abcdefghijklmnopqrstuvwxyz";
	config.cset_identifier_nth = "abcdefghijklmnopqrstuvwxyz_";
	config.scan_symbols = TRUE;
	config.scan_identifier = TRUE;

	scanner = g_scanner_new (&config);
	g_scanner_input_text (scanner, expr, strlen (expr));
	g_scanner_get_next_token (scanner);
	g_scanner_set_scope (scanner, 0);
	var_string = g_strdup_printf ("%f", var);
	g_scanner_scope_add_symbol (scanner, 0, "var", var_string);

	ret = int_parse_expr (scanner, TRUE, FALSE);

	g_free (var_string);
	g_scanner_destroy (scanner);
	return ret;
}

static gdouble
int_parse_sentence (GScanner *scanner) 
{
	GTokenType token_type;
	GTokenValue value;
	gdouble left;

	left = int_parse_unary (scanner);

	token_type = g_scanner_cur_token (scanner);
	value = g_scanner_cur_value (scanner);

	if ((token_type == G_TOKEN_CHAR && value.v_char == '&') ||
	    (token_type == G_TOKEN_SYMBOL && value.v_symbol == SYMBOL_AND)) 
	{
		g_scanner_get_next_token (scanner);
		return (int_parse_sentence (scanner) != 0.0 && 
			left != 0.0) ? 1.0 : 0.0;
	}
	else if ((token_type == G_TOKEN_CHAR && value.v_char == '|') || 
		 (token_type == G_TOKEN_SYMBOL && 
		  value.v_symbol == SYMBOL_OR)) 
	{
		g_scanner_get_next_token (scanner);
		return (int_parse_sentence (scanner) != 0.0 || 
			left != 0.0) ? 1.0 : 0.0;
	}

	return left;
}

static gdouble
int_parse_unary (GScanner *scanner) 
{
	GTokenType token_type;
	GTokenValue value;

	token_type = g_scanner_cur_token (scanner);
	value = g_scanner_cur_value (scanner);

	if ((token_type == G_TOKEN_CHAR && value.v_char == '!') ||
	    (token_type == G_TOKEN_SYMBOL && value.v_symbol == SYMBOL_NOT))
	{
		g_scanner_get_next_token (scanner);
		return (int_parse_unary (scanner) != 0.0) ? 0.0 : 1.0;
	} else {
		return (int_parse_atom (scanner) != 0.0) ? 1.0 : 0.0;
	}
}

static gdouble
int_parse_atom (GScanner *scanner) 
{
	GTokenType token_type;
	GTokenValue value;
	gdouble left;

	left = int_parse_expr (scanner, FALSE, FALSE);

	token_type = g_scanner_cur_token (scanner);

	if (token_type == G_TOKEN_CHAR) {
		value = g_scanner_cur_value (scanner);
		if (value.v_char == '=') {
			g_scanner_get_next_token (scanner);
			return (int_parse_expr (scanner, FALSE, FALSE)
				== left) ? 1.0 : 0.0;
		}
		else if (value.v_char == '<') {
			g_scanner_get_next_token (scanner);
			return (int_parse_expr (scanner, FALSE, FALSE)
				> left) ? 1.0 : 0.0;
		}
		else if (value.v_char == '>') {
			g_scanner_get_next_token (scanner);
			return (int_parse_expr (scanner, FALSE, FALSE)
				< left) ? 1.0 : 0.0;
		}
	}

	return left;
}

static gdouble
int_parse_expr (GScanner *scanner, gboolean expr, gboolean neg) 
{
	GTokenType token_type;
	GTokenValue value;
	gdouble left;
	gdouble ret;

	left = int_parse_term (scanner, expr, FALSE);

	token_type = g_scanner_cur_token (scanner);

	if (token_type == G_TOKEN_CHAR) {
		value = g_scanner_cur_value (scanner);
		if (value.v_char == '+') {
			g_scanner_get_next_token (scanner);
			return left + int_parse_expr (scanner, expr, FALSE);
		}
		else if (value.v_char == '-') {
			g_scanner_get_next_token (scanner);
			ret = int_parse_expr (scanner, expr, TRUE);
			if (neg) 
				return left + ret;
			else
				return left - ret;
		}
	}

	return left;
}

static gdouble
int_parse_term (GScanner *scanner, gboolean expr, gboolean inv)
{
	GTokenType token_type;
	GTokenValue value;
	gdouble left;
	gdouble ret;

	left = int_parse_factor (scanner, expr);

	token_type = g_scanner_cur_token (scanner);

	if (token_type == G_TOKEN_CHAR) {
		value = g_scanner_cur_value (scanner);
		if (token_type == '*') {
			g_scanner_get_next_token (scanner);
			return left * int_parse_term (scanner, expr, FALSE);
		}
		else if (token_type == '/') {
			g_scanner_get_next_token (scanner);
			ret = int_parse_term (scanner, expr, TRUE);
			if (inv) 
				return left * ret;
			else
				return left / ret;
		}
	}

	return left;
}

static gdouble
int_parse_factor (GScanner *scanner, gboolean expr) 
{
	GTokenType token_type;
	GTokenValue value;
	gdouble ret;

	token_type = g_scanner_cur_token (scanner);
	value = g_scanner_cur_value (scanner);
	g_scanner_get_next_token (scanner);

	if (token_type == G_TOKEN_CHAR && value.v_char == '(') {
		if (expr)
			ret = int_parse_expr (scanner, TRUE, FALSE);
		else
			ret = int_parse_sentence (scanner);

		g_scanner_get_next_token (scanner);
		return ret;
	}
	else if (token_type == G_TOKEN_INT) {
		return value.v_int;
	}
	else if (token_type == G_TOKEN_FLOAT) {
		return value.v_float;
	}
	else if (token_type == G_TOKEN_SYMBOL) {
		if (value.v_symbol == (gpointer) 1)
			return (gint) value.v_symbol;
		else
			return g_strtod (value.v_symbol, NULL);
	}
	else if (token_type == G_TOKEN_IDENTIFIER) {
		return 0.0;
	} else {
		g_scanner_error (scanner, "Parse error in expression");
	}

	return 0;
}
