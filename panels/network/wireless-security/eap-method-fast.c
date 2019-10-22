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
 * Copyright 2012 - 2014 Red Hat, Inc.
 */

#include "nm-default.h"

#include <ctype.h>
#include <string.h>

#include "eap-method.h"
#include "wireless-security.h"
#include "utils.h"
#include "helpers.h"

#define I_NAME_COLUMN   0
#define I_METHOD_COLUMN 1

struct _EAPMethodFAST {
	EAPMethod parent;

	GtkEntry             *anon_identity_entry;
	GtkLabel             *anon_identity_label;
	GtkComboBox          *inner_auth_combo;
	GtkLabel             *inner_auth_label;
	GtkBox               *inner_auth_box;
	GtkFileChooserButton *pac_file_button;
	GtkLabel             *pac_file_label;
	GtkCheckButton       *pac_provision_check;
	GtkComboBox          *pac_provision_combo;

	GtkSizeGroup *size_group;
	WirelessSecurity *sec_parent;
	gboolean is_editor;
};

static void
destroy (EAPMethod *parent)
{
	EAPMethodFAST *self = (EAPMethodFAST *) parent;

	g_clear_object (&self->size_group);
}

static gboolean
validate (EAPMethod *parent, GError **error)
{
	EAPMethodFAST *self = (EAPMethodFAST *) parent;
	GtkTreeModel *model;
	GtkTreeIter iter;
	g_autoptr(EAPMethod) eap = NULL;
	const char *file;
	gboolean provisioning;
	gboolean valid = TRUE;

	provisioning = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (self->pac_provision_check));
	file = gtk_file_chooser_get_filename (GTK_FILE_CHOOSER (self->pac_file_button));
	if (!provisioning && !file) {
		widget_set_error (GTK_WIDGET (self->pac_file_button));
		g_set_error_literal (error, NMA_ERROR, NMA_ERROR_GENERIC, _("missing EAP-FAST PAC file"));
		valid = FALSE;
	} else
		widget_unset_error (GTK_WIDGET (self->pac_file_button));

	model = gtk_combo_box_get_model (self->inner_auth_combo);
	gtk_combo_box_get_active_iter (self->inner_auth_combo, &iter);
	gtk_tree_model_get (model, &iter, I_METHOD_COLUMN, &eap, -1);
	g_assert (eap);
	valid = eap_method_validate (eap, valid ? error : NULL) && valid;
	return valid;
}

static void
add_to_size_group (EAPMethod *parent, GtkSizeGroup *group)
{
	EAPMethodFAST *self = (EAPMethodFAST *) parent;
	GtkTreeModel *model;
	GtkTreeIter iter;
	g_autoptr(EAPMethod) eap = NULL;

	g_clear_object (&self->size_group);
	self->size_group = g_object_ref (group);

	gtk_size_group_add_widget (group, GTK_WIDGET (self->anon_identity_label));
	gtk_size_group_add_widget (group, GTK_WIDGET (self->pac_file_label));
	gtk_size_group_add_widget (group, GTK_WIDGET (self->pac_provision_check));
	gtk_size_group_add_widget (group, GTK_WIDGET (self->inner_auth_label));

	model = gtk_combo_box_get_model (self->inner_auth_combo);
	gtk_combo_box_get_active_iter (self->inner_auth_combo, &iter);
	gtk_tree_model_get (model, &iter, I_METHOD_COLUMN, &eap, -1);
	g_assert (eap);
	eap_method_add_to_size_group (eap, group);
}

static void
fill_connection (EAPMethod *parent, NMConnection *connection, NMSettingSecretFlags flags)
{
	EAPMethodFAST *self = (EAPMethodFAST *) parent;
	NMSetting8021x *s_8021x;
	const char *text;
	char *filename;
	g_autoptr(EAPMethod) eap = NULL;
	GtkTreeModel *model;
	GtkTreeIter iter;
	gboolean enabled;
	int pac_provisioning = 0;

	s_8021x = nm_connection_get_setting_802_1x (connection);
	g_assert (s_8021x);

	nm_setting_802_1x_add_eap_method (s_8021x, "fast");

	text = gtk_entry_get_text (self->anon_identity_entry);
	if (text && strlen (text))
		g_object_set (s_8021x, NM_SETTING_802_1X_ANONYMOUS_IDENTITY, text, NULL);

	filename = gtk_file_chooser_get_filename (GTK_FILE_CHOOSER (self->pac_file_button));
	g_object_set (s_8021x, NM_SETTING_802_1X_PAC_FILE, filename, NULL);

	enabled = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (self->pac_provision_check));

	if (!enabled)
		g_object_set (G_OBJECT (s_8021x), NM_SETTING_802_1X_PHASE1_FAST_PROVISIONING, "0", NULL);
	else {
		pac_provisioning = gtk_combo_box_get_active (self->pac_provision_combo);

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

	model = gtk_combo_box_get_model (self->inner_auth_combo);
	gtk_combo_box_get_active_iter (self->inner_auth_combo, &iter);
	gtk_tree_model_get (model, &iter, I_METHOD_COLUMN, &eap, -1);
	g_assert (eap);

	eap_method_fill_connection (eap, connection, flags);
}

static void
inner_auth_combo_changed_cb (EAPMethodFAST *self)
{
	g_autoptr(EAPMethod) eap = NULL;
	GList *elt, *children;
	GtkTreeModel *model;
	GtkTreeIter iter;
	GtkWidget *eap_widget;

	/* Remove any previous wireless security widgets */
	children = gtk_container_get_children (GTK_CONTAINER (self->inner_auth_box));
	for (elt = children; elt; elt = g_list_next (elt))
		gtk_container_remove (GTK_CONTAINER (self->inner_auth_box), GTK_WIDGET (elt->data));
	g_list_free (children);

	model = gtk_combo_box_get_model (self->inner_auth_combo);
	gtk_combo_box_get_active_iter (self->inner_auth_combo, &iter);
	gtk_tree_model_get (model, &iter, I_METHOD_COLUMN, &eap, -1);
	g_assert (eap);

	eap_widget = eap_method_get_widget (eap);
	g_assert (eap_widget);
	gtk_widget_unparent (eap_widget);

	if (self->size_group)
		eap_method_add_to_size_group (eap, self->size_group);
	gtk_container_add (GTK_CONTAINER (self->inner_auth_box), eap_widget);

	wireless_security_notify_changed (self->sec_parent);
}

static void
inner_auth_combo_init (EAPMethodFAST *self,
                       NMConnection *connection,
                       NMSetting8021x *s_8021x,
                       gboolean secrets_only)
{
	g_autoptr(GtkListStore) auth_model = NULL;
	GtkTreeIter iter;
	g_autoptr(EAPMethodSimple) em_gtc = NULL;
	g_autoptr(EAPMethodSimple) em_mschap_v2 = NULL;
	guint32 active = 0;
	const char *phase2_auth = NULL;
	EAPMethodSimpleFlags simple_flags;

	auth_model = gtk_list_store_new (2, G_TYPE_STRING, eap_method_get_type ());

	if (s_8021x) {
		if (nm_setting_802_1x_get_phase2_auth (s_8021x))
			phase2_auth = nm_setting_802_1x_get_phase2_auth (s_8021x);
		else if (nm_setting_802_1x_get_phase2_autheap (s_8021x))
			phase2_auth = nm_setting_802_1x_get_phase2_autheap (s_8021x);
	}

	simple_flags = EAP_METHOD_SIMPLE_FLAG_PHASE2;
	if (self->is_editor)
		simple_flags |= EAP_METHOD_SIMPLE_FLAG_IS_EDITOR;
	if (secrets_only)
		simple_flags |= EAP_METHOD_SIMPLE_FLAG_SECRETS_ONLY;

	em_gtc = eap_method_simple_new (self->sec_parent,
	                                connection,
	                                EAP_METHOD_SIMPLE_TYPE_GTC,
	                                simple_flags);
	gtk_list_store_append (auth_model, &iter);
	gtk_list_store_set (auth_model, &iter,
	                    I_NAME_COLUMN, _("GTC"),
	                    I_METHOD_COLUMN, em_gtc,
	                    -1);

	/* Check for defaulting to GTC */
	if (phase2_auth && !strcasecmp (phase2_auth, "gtc"))
		active = 0;

	em_mschap_v2 = eap_method_simple_new (self->sec_parent,
	                                      connection,
	                                      EAP_METHOD_SIMPLE_TYPE_MSCHAP_V2,
	                                      simple_flags);
	gtk_list_store_append (auth_model, &iter);
	gtk_list_store_set (auth_model, &iter,
	                    I_NAME_COLUMN, _("MSCHAPv2"),
	                    I_METHOD_COLUMN, em_mschap_v2,
	                    -1);

	/* Check for defaulting to MSCHAPv2 */
	if (phase2_auth && !strcasecmp (phase2_auth, "mschapv2"))
		active = 1;

	gtk_combo_box_set_model (self->inner_auth_combo, GTK_TREE_MODEL (auth_model));
	gtk_combo_box_set_active (self->inner_auth_combo, active);

	g_signal_connect_swapped (self->inner_auth_combo, "changed", G_CALLBACK (inner_auth_combo_changed_cb), self);
}

static void
update_secrets (EAPMethod *parent, NMConnection *connection)
{
	EAPMethodFAST *self = (EAPMethodFAST *) parent;
	eap_method_phase2_update_secrets_helper (parent,
	                                         connection,
	                                         self->inner_auth_combo,
	                                         I_METHOD_COLUMN);
}

static void
pac_toggled_cb (EAPMethodFAST *self)
{
	gboolean enabled = FALSE;

	enabled = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (self->pac_provision_check));
	gtk_widget_set_sensitive (GTK_WIDGET (self->pac_provision_combo), enabled);

	wireless_security_notify_changed (self->sec_parent);
}

static void
changed_cb (EAPMethodFAST *self)
{
	wireless_security_notify_changed (self->sec_parent);
}

EAPMethodFAST *
eap_method_fast_new (WirelessSecurity *ws_parent,
                     NMConnection *connection,
                     gboolean is_editor,
                     gboolean secrets_only)
{
	EAPMethod *parent;
	EAPMethodFAST *self;
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
	                          "/org/gnome/ControlCenter/network/eap-method-fast.ui",
	                          "grid",
	                          "anon_identity_entry",
	                          FALSE);
	if (!parent)
		return NULL;

	parent->password_flags_name = NM_SETTING_802_1X_PASSWORD;
	self = (EAPMethodFAST *) parent;
	self->sec_parent = ws_parent;
	self->is_editor = is_editor;

	self->anon_identity_entry = GTK_ENTRY (gtk_builder_get_object (parent->builder, "anon_identity_entry"));
	self->anon_identity_label = GTK_LABEL (gtk_builder_get_object (parent->builder, "anon_identity_label"));
	self->inner_auth_combo = GTK_COMBO_BOX (gtk_builder_get_object (parent->builder, "inner_auth_combo"));
	self->inner_auth_label = GTK_LABEL (gtk_builder_get_object (parent->builder, "inner_auth_label"));
	self->inner_auth_box = GTK_BOX (gtk_builder_get_object (parent->builder, "inner_auth_box"));
	self->pac_file_button = GTK_FILE_CHOOSER_BUTTON (gtk_builder_get_object (parent->builder, "pac_file_button"));
	self->pac_file_label = GTK_LABEL (gtk_builder_get_object (parent->builder, "pac_file_label"));
	self->pac_provision_check = GTK_CHECK_BUTTON (gtk_builder_get_object (parent->builder, "pac_provision_check"));
	self->pac_provision_combo = GTK_COMBO_BOX (gtk_builder_get_object (parent->builder, "pac_provision_combo"));

	if (connection)
		s_8021x = nm_connection_get_setting_802_1x (connection);

	gtk_combo_box_set_active (self->pac_provision_combo, 0);
	if (s_8021x) {
		const char *fast_prov;

		fast_prov = nm_setting_802_1x_get_phase1_fast_provisioning (s_8021x);
		if (fast_prov) {
			if (!strcmp (fast_prov, "0"))
				provisioning_enabled = FALSE;
			else if (!strcmp (fast_prov, "1"))
				gtk_combo_box_set_active (self->pac_provision_combo, 0);
			else if (!strcmp (fast_prov, "2"))
				gtk_combo_box_set_active (self->pac_provision_combo, 1);
			else if (!strcmp (fast_prov, "3"))
				gtk_combo_box_set_active (self->pac_provision_combo, 2);
		}
	}
	gtk_widget_set_sensitive (GTK_WIDGET (self->pac_provision_combo), provisioning_enabled);
	g_signal_connect_swapped (self->pac_provision_combo, "changed", G_CALLBACK (changed_cb), self);

	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (self->pac_provision_check), provisioning_enabled);
	g_signal_connect_swapped (self->pac_provision_check, "toggled", G_CALLBACK (pac_toggled_cb), self);

	if (s_8021x && nm_setting_802_1x_get_anonymous_identity (s_8021x))
		gtk_entry_set_text (self->anon_identity_entry, nm_setting_802_1x_get_anonymous_identity (s_8021x));
	g_signal_connect_swapped (self->anon_identity_entry, "changed", G_CALLBACK (changed_cb), self);

	gtk_file_chooser_set_local_only (GTK_FILE_CHOOSER (self->pac_file_button), TRUE);
	gtk_file_chooser_button_set_title (self->pac_file_button,
	                                   _("Choose a PAC file"));
	g_signal_connect_swapped (self->pac_file_button, "selection-changed", G_CALLBACK (changed_cb), self);

	filter = gtk_file_filter_new ();
	gtk_file_filter_add_pattern (filter, "*.pac");
	gtk_file_filter_set_name (filter, _("PAC files (*.pac)"));
	gtk_file_chooser_add_filter (GTK_FILE_CHOOSER (self->pac_file_button), filter);
	filter = gtk_file_filter_new ();
	gtk_file_filter_add_pattern (filter, "*");
	gtk_file_filter_set_name (filter, _("All files"));
	gtk_file_chooser_add_filter (GTK_FILE_CHOOSER (self->pac_file_button), filter);

	if (connection && s_8021x) {
		filename = nm_setting_802_1x_get_pac_file (s_8021x);
		if (filename)
			gtk_file_chooser_set_filename (GTK_FILE_CHOOSER (self->pac_file_button), filename);
	}

	inner_auth_combo_init (self, connection, s_8021x, secrets_only);
	inner_auth_combo_changed_cb (self);

	if (secrets_only) {
		gtk_widget_hide (GTK_WIDGET (self->anon_identity_label));
		gtk_widget_hide (GTK_WIDGET (self->anon_identity_entry));
		gtk_widget_hide (GTK_WIDGET (self->pac_provision_check));
		gtk_widget_hide (GTK_WIDGET (self->pac_provision_combo));
		gtk_widget_hide (GTK_WIDGET (self->pac_file_label));
		gtk_widget_hide (GTK_WIDGET (self->pac_file_button));
		gtk_widget_hide (GTK_WIDGET (self->inner_auth_label));
		gtk_widget_hide (GTK_WIDGET (self->inner_auth_combo));
	}

	return self;
}

