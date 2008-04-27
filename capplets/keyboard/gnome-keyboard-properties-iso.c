/* -*- mode: c; style: linux -*- */

/* gnome-keyboard-properties-xkbltadd.c
 * Copyright (C) 2007 Sergey V. Udaltsov
 *
 * Written by: Sergey V. Udaltsov <svu@gnome.org>
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
#include <stdlib.h>

#include <gnome.h>
#include <glib/gi18n.h>
#include <glade/glade.h>

#include <gnome-keyboard-properties-xkb.h>

#define ISO_CODES_DATADIR    ISO_CODES_PREFIX "/share/xml/iso-codes"
#define ISO_CODES_LOCALESDIR ISO_CODES_PREFIX "/share/locale"

static GHashTable *country_code_names = NULL;
static GHashTable *lang_code_names = NULL;

typedef struct {
	const gchar *domain;
	const gchar *attr_names[];
} LookupParams;

typedef struct {
	GHashTable *code_names;
	const gchar *tag_name;
	LookupParams *params;
} CodeBuildStruct;

static LookupParams countryLookup = { "iso_3166", {"alpha_2_code", NULL} };
static LookupParams languageLookup =
    { "iso_639", {"iso_639_2B_code", "iso_639_2T_code", NULL} };

static void
iso_codes_parse_start_tag (GMarkupParseContext * ctx,
			   const gchar * element_name,
			   const gchar ** attr_names,
			   const gchar ** attr_values,
			   gpointer user_data, GError ** error)
{
	const gchar *name;
	const gchar **san = attr_names, **sav = attr_values;
	CodeBuildStruct *cbs = (CodeBuildStruct *) user_data;

	/* Is this the tag we are looking for? */
	if (!g_str_equal (element_name, cbs->tag_name) ||
	    attr_names == NULL || attr_values == NULL) {
		return;
	}

	name = NULL;

	/* What would be the value? */
	while (*attr_names && *attr_values) {
		if (g_str_equal (*attr_names, "name")) {
			name = *attr_values;
			break;
		}

		attr_names++;
		attr_values++;
	}

	if (!name) {
		return;
	}

	attr_names = san;
	attr_values = sav;

	/* Walk again the attributes */
	while (*attr_names && *attr_values) {
		const gchar **attr = cbs->params->attr_names;
		/* Look through all the attributess we are interested in */
		while (*attr) {
			if (g_str_equal (*attr_names, *attr)) {
				if (**attr_values) {
					g_hash_table_insert (cbs->
							     code_names,
							     g_strdup
							     (*attr_values),
							     g_strdup
							     (name));
				}
			}
			attr++;
		}

		attr_names++;
		attr_values++;
	}
}

static GHashTable *
iso_code_names_init (LookupParams * params)
{
	GError *err = NULL;
	gchar *buf, *filename, *tag_name;
	gsize buf_len;
	CodeBuildStruct cbs;

	GHashTable *ht = g_hash_table_new_full (g_str_hash, g_str_equal,
						g_free, g_free);

	tag_name = g_strdup_printf ("%s_entry", params->domain);

	cbs.code_names = ht;
	cbs.tag_name = tag_name;
	cbs.params = params;

	bindtextdomain (params->domain, ISO_CODES_LOCALESDIR);
	bind_textdomain_codeset (params->domain, "UTF-8");

	filename =
	    g_strdup_printf ("%s/%s.xml", ISO_CODES_DATADIR,
			     params->domain);
	if (g_file_get_contents (filename, &buf, &buf_len, &err)) {
		GMarkupParseContext *ctx;
		GMarkupParser parser = {
			iso_codes_parse_start_tag,
			NULL, NULL, NULL, NULL
		};

		ctx = g_markup_parse_context_new (&parser, 0, &cbs, NULL);
		if (!g_markup_parse_context_parse
		    (ctx, buf, buf_len, &err)) {
			g_warning ("Failed to parse '%s/%s.xml': %s",
				   ISO_CODES_DATADIR,
				   params->domain, err->message);
			g_error_free (err);
		}

		g_markup_parse_context_free (ctx);
		g_free (buf);
	} else {
		g_warning ("Failed to load '%s/%s.xml': %s",
			   ISO_CODES_DATADIR, params->domain,
			   err->message);
		g_error_free (err);
	}
	g_free (filename);
	g_free (tag_name);

	return ht;
}

const char *
get_language_iso_code (const char *code)
{
	const gchar *name;

	if (!lang_code_names) {
		lang_code_names = iso_code_names_init (&languageLookup);
	}

	name = g_hash_table_lookup (lang_code_names, code);
	if (!name) {
		return NULL;
	}

	return dgettext ("iso_639", name);
}

const char *
get_country_iso_code (const char *code)
{
	const gchar *name;

	if (!country_code_names) {
		country_code_names = iso_code_names_init (&countryLookup);
	}

	name = g_hash_table_lookup (country_code_names, code);
	if (!name) {
		return NULL;
	}

	return dgettext ("iso_3166", name);
}
