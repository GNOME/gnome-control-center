/*
 * Copyright (C) 2011 Rodrigo Moya
 *
 * Written by: Rodrigo Moya <rodrigo@gnome.org>
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

#include <locale.h>
#include "cc-common-language.h"
#include "gdm-languages.h"
#include "gnome-region-panel-formats.h"

static void
selection_changed_cb (GtkComboBox *combo, gpointer user_data)
{
	const gchar *active_id, *locale;
	GDateTime *dt;
	gchar *s;
	struct lconv *num_info;
	GtkBuilder *builder = GTK_BUILDER (user_data);

	active_id = gtk_combo_box_get_active_id (combo);
	if (!active_id)
		return;

	locale = setlocale (LC_ALL, active_id);

	dt = g_date_time_new_now_local ();

	/* Display dates */
	s = g_date_time_format (dt, "%x");
	gtk_label_set_text (GTK_LABEL (gtk_builder_get_object (builder, "shortest_day_format")), s);
	g_free (s);
	
	/* Display times */
	s = g_date_time_format (dt, "%r");
	gtk_label_set_text (GTK_LABEL (gtk_builder_get_object (builder, "full_time_format")), s);
	g_free (s);

	s = g_date_time_format (dt, "%R");
	gtk_label_set_text (GTK_LABEL (gtk_builder_get_object (builder, "short_time_format")), s);
	g_free (s);

	/* Display numbers */
	s = g_strdup_printf ("%'.2f", 123456789.00);
	gtk_label_set_text (GTK_LABEL (gtk_builder_get_object (builder, "numbers_format")), s);
	g_free (s);

	/* Display currency and measurement */
	num_info = localeconv ();
	if (num_info != NULL) {
		gtk_label_set_text (GTK_LABEL (gtk_builder_get_object (builder, "currency_format")), num_info->currency_symbol);
	}

	setlocale (LC_ALL, locale);
}

void
setup_formats (GtkBuilder *builder)
{
	GtkWidget *combo;
	gchar **langs, *language, *current_lang;
	gint i;

	/* Setup formats selector */
	combo = GTK_WIDGET (gtk_builder_get_object (builder, "region_selector"));
	gtk_combo_box_set_id_column (GTK_COMBO_BOX (combo), 1);

	langs = gdm_get_all_language_names ();
	for (i = 0; langs[i] != NULL; i++) {
		language = gdm_get_language_from_name (langs[i], NULL);
		/* FIXME: sort while adding */
		gtk_combo_box_text_append (GTK_COMBO_BOX_TEXT (combo), langs[i], language);

		g_free (language);

	}

	g_signal_connect (G_OBJECT (combo), "changed",
			  G_CALLBACK (selection_changed_cb), builder);

	current_lang = cc_common_language_get_current_language ();
	gtk_combo_box_set_active_id (GTK_COMBO_BOX (combo), current_lang);
	g_free (current_lang);
}
