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

#include <string.h>
#include "expr.h"

typedef enum {
	TYPE_BOOLEAN, TYPE_DOUBLE, TYPE_STRING
} value_type_t;

typedef struct {
	value_type_t type;
	union {
		gboolean v_bool;
		gdouble v_double;
		gchar *v_string;
	} u;
} value_t;

static value_t  int_parse_sentence (GScanner *scanner);
static value_t  int_parse_unary    (GScanner *scanner);
static value_t  int_parse_atom     (GScanner *scanner);
static value_t  int_parse_expr     (GScanner *scanner, gboolean expr,
				    gboolean neg);
static value_t  int_parse_term     (GScanner *scanner, gboolean expr, 
				    gboolean inv);
static value_t  int_parse_factor   (GScanner *scanner, gboolean expr);

gboolean 
parse_sentence (gchar *sentence, GScanner *scanner) 
{
	value_t val;

	g_scanner_input_text (scanner, sentence, strlen (sentence));
	g_scanner_get_next_token (scanner);
	val = int_parse_sentence (scanner);
	g_assert (val.type == TYPE_BOOLEAN);

	return val.u.v_bool;
}

gdouble
parse_expr (gchar *expr, gdouble var) 
{
	static GScannerConfig config;
	GScanner *scanner;
	gchar *var_string;
	value_t ret;

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
	g_assert (ret.type == TYPE_DOUBLE);

	g_free (var_string);
	g_scanner_destroy (scanner);
	return ret.u.v_double;
}

static value_t
int_parse_sentence (GScanner *scanner) 
{
	GTokenType token_type;
	GTokenValue value;
	value_t left, right;

	left = int_parse_unary (scanner);
	g_assert (left.type == TYPE_BOOLEAN);

	token_type = g_scanner_cur_token (scanner);
	value = g_scanner_cur_value (scanner);

	if ((token_type == G_TOKEN_CHAR && value.v_char == '&') ||
	    (token_type == G_TOKEN_SYMBOL && value.v_symbol == SYMBOL_AND)) 
	{
		g_scanner_get_next_token (scanner);
		right = int_parse_sentence (scanner);
		g_assert (right.type == TYPE_BOOLEAN);
		left.u.v_bool = left.u.v_bool && right.u.v_bool;
		return left;
	}
	else if ((token_type == G_TOKEN_CHAR && value.v_char == '|') || 
		 (token_type == G_TOKEN_SYMBOL && 
		  value.v_symbol == SYMBOL_OR)) 
	{
		g_scanner_get_next_token (scanner);
		right = int_parse_sentence (scanner);
		g_assert (right.type == TYPE_BOOLEAN);
		left.u.v_bool = left.u.v_bool || right.u.v_bool;
		return left;
	}

	return left;
}

static value_t
int_parse_unary (GScanner *scanner) 
{
	GTokenType token_type;
	GTokenValue value;
	value_t op;

	token_type = g_scanner_cur_token (scanner);
	value = g_scanner_cur_value (scanner);

	if ((token_type == G_TOKEN_CHAR && value.v_char == '!') ||
	    (token_type == G_TOKEN_SYMBOL && value.v_symbol == SYMBOL_NOT))
	{
		g_scanner_get_next_token (scanner);
		op = int_parse_unary (scanner);
		g_assert (op.type == TYPE_BOOLEAN);
		op.u.v_bool = !op.u.v_bool;
		return op;
	} else {
		op = int_parse_atom (scanner);
		g_assert (op.type == TYPE_BOOLEAN);
		return op;
	}
}

static value_t
int_parse_atom (GScanner *scanner) 
{
	GTokenType token_type;
	GTokenValue value;
	value_t left, right;

	left = int_parse_expr (scanner, FALSE, FALSE);

	token_type = g_scanner_cur_token (scanner);

	if (token_type == G_TOKEN_CHAR) {
		value = g_scanner_cur_value (scanner);
		if (value.v_char == '=') {
			g_scanner_get_next_token (scanner);
			right = int_parse_expr (scanner, FALSE, FALSE);

			if (left.type != right.type)
				left.u.v_bool = FALSE;
			else if (left.type == TYPE_DOUBLE)
				left.u.v_bool = (left.u.v_double ==
						 right.u.v_double);
			else if (left.type == TYPE_STRING)
				left.u.v_bool =
					(strcmp (left.u.v_string,
						 right.u.v_string) == 0);
		}
		else if (value.v_char == '<') {
			g_scanner_get_next_token (scanner);
			right = int_parse_expr (scanner, FALSE, FALSE);
			
			if (left.type != TYPE_DOUBLE ||
			    right.type != TYPE_DOUBLE) 
				left.u.v_bool = FALSE;
			else
				left.u.v_bool = (left.u.v_double <
						 right.u.v_double);
		}
		else if (value.v_char == '>') {
			g_scanner_get_next_token (scanner);
			right = int_parse_expr (scanner, FALSE, FALSE);
			
			if (left.type != TYPE_DOUBLE ||
			    right.type != TYPE_DOUBLE) 
				left.u.v_bool = FALSE;
			else
				left.u.v_bool = (left.u.v_double >
						 right.u.v_double);
		}
	} else {
		if (left.type == TYPE_DOUBLE)
			left.u.v_bool = (left.u.v_double != 0.0);
		else if (left.type == TYPE_STRING)
			/* Assume this is a command line identifier
			 * that could not be found */
			left.u.v_bool = FALSE;
	}

	left.type = TYPE_BOOLEAN;

	return left;
}

static value_t
int_parse_expr (GScanner *scanner, gboolean expr, gboolean neg) 
{
	GTokenType token_type;
	GTokenValue value;
	value_t left, right;

	left = int_parse_term (scanner, expr, FALSE);

	token_type = g_scanner_cur_token (scanner);

	if (token_type == G_TOKEN_CHAR) {
		value = g_scanner_cur_value (scanner);
		if (value.v_char == '+') {
			g_scanner_get_next_token (scanner);
			right = int_parse_expr (scanner, expr, FALSE);

			if (left.type != TYPE_DOUBLE || 
			    right.type != TYPE_DOUBLE)
				left.u.v_double = 0.0;
			else
				left.u.v_double += right.u.v_double;

			left.type = TYPE_DOUBLE;
		}
		else if (value.v_char == '-') {
			g_scanner_get_next_token (scanner);
			right = int_parse_expr (scanner, expr, TRUE);

			if (left.type != TYPE_DOUBLE ||
			    right.type != TYPE_DOUBLE)
				left.u.v_double = 0.0;
			else if (neg) 
				left.u.v_double += right.u.v_double;
			else
				left.u.v_double -= right.u.v_double;

			left.type = TYPE_DOUBLE;
		}
	}

	return left;
}

static value_t
int_parse_term (GScanner *scanner, gboolean expr, gboolean inv)
{
	GTokenType token_type;
	GTokenValue value;
	value_t left, right;

	left = int_parse_factor (scanner, expr);

	token_type = g_scanner_cur_token (scanner);

	if (token_type == G_TOKEN_CHAR) {
		value = g_scanner_cur_value (scanner);
		if (token_type == '*') {
			g_scanner_get_next_token (scanner);
			right = int_parse_term (scanner, expr, FALSE);

			if (left.type != TYPE_DOUBLE || 
			    right.type != TYPE_DOUBLE)
				left.u.v_double = 0.0;
			else
				left.u.v_double *= right.u.v_double;

			left.type = TYPE_DOUBLE;
		}
		else if (token_type == '/') {
			g_scanner_get_next_token (scanner);
			right = int_parse_expr (scanner, expr, TRUE);

			if (left.type != TYPE_DOUBLE ||
			    right.type != TYPE_DOUBLE)
				left.u.v_double = 0.0;
			else if (inv) 
				left.u.v_double *= right.u.v_double;
			else
				left.u.v_double /= right.u.v_double;

			left.type = TYPE_DOUBLE;
		}
	}

	return left;
}

static value_t
int_parse_factor (GScanner *scanner, gboolean expr) 
{
	GTokenType token_type;
	GTokenValue value;
	value_t ret;

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
		ret.type = TYPE_DOUBLE;
		ret.u.v_double = (gdouble) value.v_int;

		return ret;
	}
	else if (token_type == G_TOKEN_FLOAT) {
		ret.type = TYPE_DOUBLE;
		ret.u.v_double = (gdouble) value.v_float;

		return ret;
	}
	else if (token_type == G_TOKEN_SYMBOL) {
		if (value.v_symbol == (gpointer) 1) {
			ret.type = TYPE_BOOLEAN;
			ret.u.v_bool = TRUE;
		} else {
			ret.type = TYPE_DOUBLE;
			ret.u.v_double = g_strtod (value.v_symbol, NULL);
		}

		return ret;
	}
	else if (token_type == G_TOKEN_IDENTIFIER) {
		ret.type = TYPE_STRING;
		ret.u.v_string = value.v_identifier;

		return ret;
	} else {
		g_scanner_error (scanner, "Parse error in expression");
	}

	ret.type = TYPE_BOOLEAN;
	ret.u.v_bool = FALSE;
	return ret;
}
