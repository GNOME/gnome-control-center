/* -*- Mode: C; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*- */
/* vim: set ft=c ts=4 sts=4 sw=4 noexpandtab smartindent: */

/* EAP-FAST authentication method (RFC4851)
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
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 *
 * (C) Copyright 2012 Red Hat, Inc.
 */

#include "config.h"

#include <glib/gi18n.h>
#include <ctype.h>
#include <string.h>

#include <nm-setting-connection.h>
#include <nm-setting-8021x.h>

#include "eap-method.h"
#include "wireless-security.h"

#define I_NAME_COLUMN   0
#define I_METHOD_COLUMN 1

struct _EAPMethodFAST {
	EAPMethod parent;

	GtkSizeGroup *size_group;
	WirelessSecurity *sec_parent;
	gboolean is_editor;
};

static void
destroy (EAPMethod *parent)
{
	EAPMethodFAST *method = (EAPMethodFAST *) parent;

	if (method->size_group)
		g_object_unref (method->size_group);
}

static gboolean
validate (EAPMethod *parent)
{
	GtkWidget *widget;
	GtkTreeModel *model;
	GtkTreeIter iter;
	EAPMethod *eap = NULL;
	const char *file;
	gboolean provisioning;
	gboolean valid = FALSE;

	widget = GTK_WIDGET (gtk_builder_get_object (parent->builder, "eap_fast_pac_provision_checkbutton"));
	g_assert (widget);
	provisioning = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (widget));
	widget = GTK_WIDGET (gtk_builder_get_object (parent->builder, "eap_fast_pac_file_button"));
	g_assert (widget);
	file = gtk_file_chooser_get_filename (GTK_FILE_CHOOSER (widget));
	if (!provisioning && !file)
		return FALSE;

	widget = GTK_WIDGET (gtk_builder_get_object (parent->builder, "eap_fast_inner_auth_combo"));
	g_assert (widget);
	model = gtk_combo_box_get_model (GTK_COMBO_BOX (widget));
	gtk_combo_box_get_active_iter (GTK_COMBO_BOX (widget), &iter);
	gtk_tree_model_get (model, &iter, I_METHOD_COLUMN, &eap, -1);
	g_assert (eap);
	valid = eap_method_validate (eap);
	eap_method_unref (eap);
	return valid;
}

static void
add_to_size_group (EAPMethod *parent, GtkSizeGroup *group)
{
	EAPMethodFAST *method = (EAPMethodFAST *) parent;
	GtkWidget *widget;
	GtkTreeModel *model;
	GtkTreeIter iter;
	EAPMethod *eap;

	if (method->size_group)
		g_object_unref (method->size_group);
	method->size_group = g_object_ref (group);

	widget = GTK_WIDGET (gtk_builder_get_object (parent->builder, "eap_fast_anon_identity_label"));
	g_assert (widget);
	gtk_size_group_add_widget (group, widget);

	widget = GTK_WIDGET (gtk_builder_get_object (parent->builder, "eap_fast_pac_file_label"));
	g_assert (widget);
	gtk_size_group_add_widget (group, widget);

	widget = GTK_WIDGET (gtk_builder_get_object (parent->builder, "eap_fast_pac_provision_checkbutton"));
	g_assert (widget);
	gtk_size_group_add_widget (group, widget);

	widget = GTK_WIDGET (gtk_builder_get_object (parent->builder, "eap_fast_inner_auth_label"));
	g_assert (widget);
	gtk_size_group_add_widget (group, widget);

	widget = GTK_WIDGET (gtk_builder_get_object (parent->builder, "eap_fast_inner_auth_combo"));
	g_assert (widget);

	model = gtk_combo_box_get_model (GTK_COMBO_BOX (widget));
	gtk_combo_box_get_active_iter (GTK_COMBO_BOX (widget), &iter);
	gtk_tree_model_get (model, &iter, I_METHOD_COLUMN, &eap, -1);
	g_assert (eap);
	eap_method_add_to_size_group (eap, group);
	eap_method_unref (eap);
}

static void
fill_connection (EAPMethod *parent, NMConnection *connection)
{
	NMSetting8021x *s_8021x;
	GtkWidget *widget;
	const char *text;
	char *filename;
	EAPMethod *eap = NULL;
	GtkTreeModel *model;
	GtkTreeIter iter;
	gboolean enabled;
	int pac_provisioning = 0;

	s_8021x = nm_connection_get_setting_802_1x (connection);
	g_assert (s_8021x);

	nm_setting_802_1x_add_eap_method (s_8021x, "fast");

	widget = GTK_WIDGET (gtk_builder_get_object (parent->builder, "eap_fast_anon_identity_entry"));
	g_assert (widget);
	text = gtk_entry_get_text (GTK_ENTRY (widget));
	if (text && strlen (text))
		g_object_set (s_8021x, NM_SETTING_802_1X_ANONYMOUS_IDENTITY, text, NULL);

	widget = GTK_WIDGET (gtk_builder_get_object (parent->builder, "eap_fast_pac_file_button"));
	g_assert (widget);
	filename = gtk_file_chooser_get_filename (GTK_FILE_CHOOSER (widget));
	g_object_set (s_8021x, NM_SETTING_802_1X_PAC_FILE, filename, NULL);

	widget = GTK_WIDGET (gtk_builder_get_object (parent->builder, "eap_fast_pac_provision_checkbutton"));
	enabled = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (widget));

	if (!enabled)
		g_object_set (G_OBJECT (s_8021x), NM_SETTING_802_1X_PHASE1_FAST_PROVISIONING, "0", NULL);
	else {
		widget = GTK_WIDGET (gtk_builder_get_object (parent->builder, "eap_fast_pac_provision_combo"));
		pac_provisioning = gtk_combo_box_get_active (GTK_COMBO_BOX (widget));

		switch (pac_provisioning) {
		case 0:  /* Anonymous */
			g_object_set (G_OBJECT (s_8021x), NM_SETTING_802_1X_PHASE1_FAST_PROVISIONING, "1", NULL);
			break;
		case 1:  /* Authenticated */
			g_object_set (G_OBJECT (s_8021x), NM_SETTING_802_1X_PHASE1_FAST_PROVISIONING, "2", NULL);
			break;
		case 2:  /* Both - anonymous and authenticated */
			g_object_set (G_OBJECT (s_8021x), NM_SETTING_802_1X_PHASE1_FAST_PROVISIONING, "3", NULL);
			break;
		default: /* Should not happen */
			g_object_set (G_OBJECT (s_8021x), NM_SETTING_802_1X_PHASE1_FAST_PROVISIONING, "1", NULL);
			break;
		}
	}

	widget = GTK_WIDGET (gtk_builder_get_object (parent->builder, "eap_fast_inner_auth_combo"));
	model = gtk_combo_box_get_model (GTK_COMBO_BOX (widget));
	gtk_combo_box_get_active_iter (GTK_COMBO_BOX (widget), &iter);
	gtk_tree_model_get (model, &iter, I_METHOD_COLUMN, &eap, -1);
	g_assert (eap);

	eap_method_fill_connection (eap, connection);
	eap_method_unref (eap);
}

static void
inner_auth_combo_changed_cb (GtkWidget *combo, gpointer user_data)
{
	EAPMethod *parent = (EAPMethod *) user_data;
	EAPMethodFAST *method = (EAPMethodFAST *) parent;
	GtkWidget *vbox;
	EAPMethod *eap = NULL;
	GList *elt, *children;
	GtkTreeModel *model;
	GtkTreeIter iter;
	GtkWidget *eap_widget;

	vbox = GTK_WIDGET (gtk_builder_get_object (parent->builder, "eap_fast_inner_auth_vbox"));
	g_assert (vbox);

	/* Remove any previous wireless security widgets */
	children = gtk_container_get_children (GTK_CONTAINER (vbox));
	for (elt = children; elt; elt = g_list_next (elt))
		gtk_container_remove (GTK_CONTAINER (vbox), GTK_WIDGET (elt->data));
	g_list_free (children);

	model = gtk_combo_box_get_model (GTK_COMBO_BOX (combo));
	gtk_combo_box_get_active_iter (GTK_COMBO_BOX (combo), &iter);
	gtk_tree_model_get (model, &iter, I_METHOD_COLUMN, &eap, -1);
	g_assert (eap);

	eap_widget = eap_method_get_widget (eap);
	g_assert (eap_widget);
	gtk_widget_unparent (eap_widget);

	if (method->size_group)
		eap_method_add_to_size_group (eap, method->size_group);
	gtk_container_add (GTK_CONTAINER (vbox), eap_widget);

	eap_method_unref (eap);

	wireless_security_changed_cb (combo, method->sec_parent);
}

static GtkWidget *
inner_auth_combo_init (EAPMethodFAST *method,
                       NMConnection *connection,
                       NMSetting8021x *s_8021x,
                       gboolean secrets_only)
{
	EAPMethod *parent = (EAPMethod *) method;
	GtkWidget *combo;
	GtkListStore *auth_model;
	GtkTreeIter iter;
	EAPMethodSimple *em_gtc;
	EAPMethodSimple *em_mschap_v2;
	guint32 active = 0;
	const char *phase2_auth = NULL;

	auth_model = gtk_list_store_new (2, G_TYPE_STRING, eap_method_get_g_type ());

	if (s_8021x) {
		if (nm_setting_802_1x_get_phase2_auth (s_8021x))
			phase2_auth = nm_setting_802_1x_get_phase2_auth (s_8021x);
		else if (nm_setting_802_1x_get_phase2_autheap (s_8021x))
			phase2_auth = nm_setting_802_1x_get_phase2_autheap (s_8021x);
	}

	em_gtc = eap_method_simple_new (method->sec_parent,
	                                connection,
	                                EAP_METHOD_SIMPLE_TYPE_GTC,
	                                TRUE,
	                                method->is_editor,
	                                secrets_only);
	gtk_list_store_append (auth_model, &iter);
	gtk_list_store_set (auth_model, &iter,
	                    I_NAME_COLUMN, _("GTC"),
	                    I_METHOD_COLUMN, em_gtc,
	                    -1);
	eap_method_unref (EAP_METHOD (em_gtc));

	/* Check for defaulting to GTC */
	if (phase2_auth && !strcasecmp (phase2_auth, "gtc"))
		active = 0;

	em_mschap_v2 = eap_method_simple_new (method->sec_parent,
	                                      connection,
	                                      EAP_METHOD_SIMPLE_TYPE_MSCHAP_V2,
	                                      TRUE,
	                                      method->is_editor, secrets_only);
	gtk_list_store_append (auth_model, &iter);
	gtk_list_store_set (auth_model, &iter,
	                    I_NAME_COLUMN, _("MSCHAPv2"),
	                    I_METHOD_COLUMN, em_mschap_v2,
	                    -1);
	eap_method_unref (EAP_METHOD (em_mschap_v2));

	/* Check for defaulting to MSCHAPv2 */
	if (phase2_auth && !strcasecmp (phase2_auth, "mschapv2"))
		active = 1;

	combo = GTK_WIDGET (gtk_builder_get_object (parent->builder, "eap_fast_inner_auth_combo"));
	g_assert (combo);

	gtk_combo_box_set_model (GTK_COMBO_BOX (combo), GTK_TREE_MODEL (auth_model));
	g_object_unref (G_OBJECT (auth_model));
	gtk_combo_box_set_active (GTK_COMBO_BOX (combo), active);

	g_signal_connect (G_OBJECT (combo), "changed",
	                  (GCallback) inner_auth_combo_changed_cb,
	                  method);
	return combo;
}

static void
update_secrets (EAPMethod *parent, NMConnection *connection)
{
	eap_method_phase2_update_secrets_helper (parent,
	                                         connection,
	                                         "eap_fast_inner_auth_combo",
	                                         I_METHOD_COLUMN);
}

static void
pac_toggled_cb (GtkWidget *widget, gpointer user_data)
{
	EAPMethod *parent = (EAPMethod *) user_data;
	EAPMethodFAST *method = (EAPMethodFAST *) parent;
	gboolean enabled = FALSE;
	GtkWidget *provision_combo;

	provision_combo = GTK_WIDGET (gtk_builder_get_object (parent->builder, "eap_fast_pac_provision_combo"));
	g_return_if_fail (provision_combo);

	enabled = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (widget));

	gtk_widget_set_sensitive (provision_combo, enabled);

	wireless_security_changed_cb (widget, method->sec_parent);
}

EAPMethodFAST *
eap_method_fast_new (WirelessSecurity *ws_parent,
                     NMConnection *connection,
                     gboolean is_editor,
                     gboolean secrets_only)
{
	EAPMethod *parent;
	EAPMethodFAST *method;
	GtkWidget *widget;
	GtkFileFilter *filter;
	NMSetting8021x *s_8021x = NULL;
	const char *filename;
	gboolean provisioning_enabled = TRUE;

	parent = eap_method_init (sizeof (EAPMethodFAST),
	                          validate,
	                          add_to_size_group,
	                          fill_connection,
	                          update_secrets,
	                          destroy,
	                          UIDIR "/eap-method-fast.ui",
	                          "eap_fast_notebook",
	                          "eap_fast_anon_identity_entry",
	                          FALSE);
	if (!parent)
		return NULL;

	method = (EAPMethodFAST *) parent;
	method->sec_parent = ws_parent;
	method->is_editor = is_editor;

	if (connection)
		s_8021x = nm_connection_get_setting_802_1x (connection);


	widget = GTK_WIDGET (gtk_builder_get_object (parent->builder, "eap_fast_pac_provision_combo"));
	g_assert (widget);
	gtk_combo_box_set_active (GTK_COMBO_BOX (widget), 0);
	if (s_8021x) {
		const char *fast_prov;

		fast_prov = nm_setting_802_1x_get_phase1_fast_provisioning (s_8021x);
		if (fast_prov) {
			if (!strcmp (fast_prov, "0"))
				provisioning_enabled = FALSE;
			else if (!strcmp (fast_prov, "1"))
				gtk_combo_box_set_active (GTK_COMBO_BOX (widget), 0);
			else if (!strcmp (fast_prov, "2"))
				gtk_combo_box_set_active (GTK_COMBO_BOX (widget), 1);
			else if (!strcmp (fast_prov, "3"))
				gtk_combo_box_set_active (GTK_COMBO_BOX (widget), 2);
		}
	}
	gtk_widget_set_sensitive (widget, provisioning_enabled);
	g_signal_connect (G_OBJECT (widget), "changed",
	                  (GCallback) wireless_security_changed_cb,
	                  ws_parent);

	widget = GTK_WIDGET (gtk_builder_get_object (parent->builder, "eap_fast_pac_provision_checkbutton"));
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (widget), provisioning_enabled);
	g_signal_connect (G_OBJECT (widget), "toggled", G_CALLBACK (pac_toggled_cb), parent);

	widget = GTK_WIDGET (gtk_builder_get_object (parent->builder, "eap_fast_anon_identity_entry"));
	if (s_8021x && nm_setting_802_1x_get_anonymous_identity (s_8021x))
		gtk_entry_set_text (GTK_ENTRY (widget), nm_setting_802_1x_get_anonymous_identity (s_8021x));
	g_signal_connect (G_OBJECT (widget), "changed",
	                  (GCallback) wireless_security_changed_cb,
	                  ws_parent);

	widget = GTK_WIDGET (gtk_builder_get_object (parent->builder, "eap_fast_pac_file_button"));
	g_assert (widget);
	gtk_file_chooser_set_local_only (GTK_FILE_CHOOSER (widget), TRUE);
	gtk_file_chooser_button_set_title (GTK_FILE_CHOOSER_BUTTON (widget),
	                                   _("Choose a PAC file..."));
	g_signal_connect (G_OBJECT (widget), "selection-changed",
	                  (GCallback) wireless_security_changed_cb,
	                  ws_parent);

	filter = gtk_file_filter_new ();
	gtk_file_filter_add_pattern (filter, "*.pac");
	gtk_file_filter_set_name (filter, _("PAC files (*.pac)"));
	gtk_file_chooser_add_filter (GTK_FILE_CHOOSER (widget), filter);
	filter = gtk_file_filter_new ();
	gtk_file_filter_add_pattern (filter, "*");
	gtk_file_filter_set_name (filter, _("All files"));
	gtk_file_chooser_add_filter (GTK_FILE_CHOOSER (widget), filter);

	if (connection && s_8021x) {
		filename = nm_setting_802_1x_get_pac_file (s_8021x);
		if (filename)
			gtk_file_chooser_set_filename (GTK_FILE_CHOOSER (widget), filename);
	}

	widget = inner_auth_combo_init (method, connection, s_8021x, secrets_only);
	inner_auth_combo_changed_cb (widget, (gpointer) method);

	if (secrets_only) {
		widget = GTK_WIDGET (gtk_builder_get_object (parent->builder, "eap_fast_anon_identity_label"));
		gtk_widget_hide (widget);
		widget = GTK_WIDGET (gtk_builder_get_object (parent->builder, "eap_fast_anon_identity_entry"));
		gtk_widget_hide (widget);
		widget = GTK_WIDGET (gtk_builder_get_object (parent->builder, "eap_fast_pac_provision_checkbutton"));
		gtk_widget_hide (widget);
		widget = GTK_WIDGET (gtk_builder_get_object (parent->builder, "eap_fast_pac_provision_combo"));
		gtk_widget_hide (widget);
		widget = GTK_WIDGET (gtk_builder_get_object (parent->builder, "eap_fast_pac_file_label"));
		gtk_widget_hide (widget);
		widget = GTK_WIDGET (gtk_builder_get_object (parent->builder, "eap_fast_pac_file_button"));
		gtk_widget_hide (widget);
		widget = GTK_WIDGET (gtk_builder_get_object (parent->builder, "eap_fast_inner_auth_label"));
		gtk_widget_hide (widget);
		widget = GTK_WIDGET (gtk_builder_get_object (parent->builder, "eap_fast_inner_auth_combo"));
		gtk_widget_hide (widget);
	}

	return method;
}

