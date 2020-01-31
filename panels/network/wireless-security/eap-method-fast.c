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

#include <glib/gi18n.h>

#include "eap-method.h"
#include "eap-method-fast.h"
#include "eap-method-simple.h"
#include "helpers.h"
#include "ui-helpers.h"

#define I_NAME_COLUMN 0
#define I_ID_COLUMN   1

struct _EAPMethodFAST {
	GtkGrid parent;

	GtkEntry             *anon_identity_entry;
	GtkLabel             *anon_identity_label;
	GtkComboBox          *inner_auth_combo;
	GtkLabel             *inner_auth_label;
	GtkListStore         *inner_auth_model;
	GtkBox               *inner_auth_box;
	GtkFileChooserButton *pac_file_button;
	GtkLabel             *pac_file_label;
	GtkCheckButton       *pac_provision_check;
	GtkComboBox          *pac_provision_combo;

	EAPMethodSimple      *em_gtc;
	EAPMethodSimple      *em_mschap_v2;
};

static void eap_method_iface_init (EAPMethodInterface *);

G_DEFINE_TYPE_WITH_CODE (EAPMethodFAST, eap_method_fast, GTK_TYPE_GRID,
                         G_IMPLEMENT_INTERFACE (eap_method_get_type (), eap_method_iface_init))

static EAPMethod *
get_inner_method (EAPMethodFAST *self)
{
	GtkTreeIter iter;
	g_autofree gchar *id = NULL;

	if (!gtk_combo_box_get_active_iter (self->inner_auth_combo, &iter))
		return NULL;
	gtk_tree_model_get (GTK_TREE_MODEL (self->inner_auth_model), &iter, I_ID_COLUMN, &id, -1);

	if (strcmp (id, "gtc") == 0)
		return EAP_METHOD (self->em_gtc);
	if (strcmp (id, "mschapv2") == 0)
		return EAP_METHOD (self->em_mschap_v2);

	return NULL;
}

static gboolean
validate (EAPMethod *parent, GError **error)
{
	EAPMethodFAST *self = (EAPMethodFAST *) parent;
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

	return eap_method_validate (get_inner_method (self), valid ? error : NULL) && valid;
}

static void
add_to_size_group (EAPMethod *parent, GtkSizeGroup *group)
{
	EAPMethodFAST *self = (EAPMethodFAST *) parent;

	gtk_size_group_add_widget (group, GTK_WIDGET (self->anon_identity_label));
	gtk_size_group_add_widget (group, GTK_WIDGET (self->pac_file_label));
	gtk_size_group_add_widget (group, GTK_WIDGET (self->pac_provision_check));
	gtk_size_group_add_widget (group, GTK_WIDGET (self->inner_auth_label));

	eap_method_add_to_size_group (EAP_METHOD (self->em_gtc), group);
	eap_method_add_to_size_group (EAP_METHOD (self->em_mschap_v2), group);
}

static void
fill_connection (EAPMethod *parent, NMConnection *connection, NMSettingSecretFlags flags)
{
	EAPMethodFAST *self = (EAPMethodFAST *) parent;
	NMSetting8021x *s_8021x;
	const char *text;
	char *filename;
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

	eap_method_fill_connection (get_inner_method (self), connection, flags);
}

static void
inner_auth_combo_changed_cb (EAPMethodFAST *self)
{
	EAPMethod *inner_method;
	GList *children;

	inner_method = get_inner_method (self);

	/* Remove the previous method and migrate username/password across */
	children = gtk_container_get_children (GTK_CONTAINER (self->inner_auth_box));
	if (children != NULL) {
		EAPMethod *old_eap = g_list_nth_data (children, 0);
		eap_method_set_username (inner_method, eap_method_get_username (old_eap));
		eap_method_set_password (inner_method, eap_method_get_password (old_eap));
		eap_method_set_show_password (inner_method, eap_method_get_show_password (old_eap));
		gtk_container_remove (GTK_CONTAINER (self->inner_auth_box), GTK_WIDGET (old_eap));
	}

	gtk_container_add (GTK_CONTAINER (self->inner_auth_box), g_object_ref (GTK_WIDGET (inner_method)));

	eap_method_emit_changed (EAP_METHOD (self));
}

static void
update_secrets (EAPMethod *parent, NMConnection *connection)
{
	EAPMethodFAST *self = (EAPMethodFAST *) parent;

	eap_method_update_secrets (EAP_METHOD (self->em_gtc), connection);
	eap_method_update_secrets (EAP_METHOD (self->em_mschap_v2), connection);
}

static GtkWidget *
get_default_field (EAPMethod *parent)
{
	EAPMethodFAST *self = (EAPMethodFAST *) parent;
	return GTK_WIDGET (self->anon_identity_entry);
}

static const gchar *
get_password_flags_name (EAPMethod *parent)
{
	return NM_SETTING_802_1X_PASSWORD;
}

static const gchar *
get_username (EAPMethod *method)
{
	EAPMethodFAST *self = EAP_METHOD_FAST (method);
	return eap_method_get_username (get_inner_method (self));
}

static void
set_username (EAPMethod *method, const gchar *username)
{
	EAPMethodFAST *self = EAP_METHOD_FAST (method);
	return eap_method_set_username (get_inner_method (self), username);
}

static const gchar *
get_password (EAPMethod *method)
{
	EAPMethodFAST *self = EAP_METHOD_FAST (method);
	return eap_method_get_password (get_inner_method (self));
}

static void
set_password (EAPMethod *method, const gchar *password)
{
	EAPMethodFAST *self = EAP_METHOD_FAST (method);
	return eap_method_set_password (get_inner_method (self), password);
}

static gboolean
get_show_password (EAPMethod *method)
{
	EAPMethodFAST *self = EAP_METHOD_FAST (method);
	return eap_method_get_show_password (get_inner_method (self));
}

static void
set_show_password (EAPMethod *method, gboolean show_password)
{
	EAPMethodFAST *self = EAP_METHOD_FAST (method);
	return eap_method_set_show_password (get_inner_method (self), show_password);
}

static void
pac_toggled_cb (EAPMethodFAST *self)
{
	gboolean enabled = FALSE;

	enabled = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (self->pac_provision_check));
	gtk_widget_set_sensitive (GTK_WIDGET (self->pac_provision_combo), enabled);

	eap_method_emit_changed (EAP_METHOD (self));
}

static void
changed_cb (EAPMethodFAST *self)
{
	eap_method_emit_changed (EAP_METHOD (self));
}

static void
eap_method_fast_init (EAPMethodFAST *self)
{
	gtk_widget_init_template (GTK_WIDGET (self));
}

static void
eap_method_fast_class_init (EAPMethodFASTClass *klass)
{
        GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

	gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/ControlCenter/network/eap-method-fast.ui");

	gtk_widget_class_bind_template_child (widget_class, EAPMethodFAST, anon_identity_entry);
	gtk_widget_class_bind_template_child (widget_class, EAPMethodFAST, anon_identity_label);
	gtk_widget_class_bind_template_child (widget_class, EAPMethodFAST, inner_auth_combo);
	gtk_widget_class_bind_template_child (widget_class, EAPMethodFAST, inner_auth_label);
	gtk_widget_class_bind_template_child (widget_class, EAPMethodFAST, inner_auth_model);
	gtk_widget_class_bind_template_child (widget_class, EAPMethodFAST, inner_auth_box);
	gtk_widget_class_bind_template_child (widget_class, EAPMethodFAST, pac_file_button);
	gtk_widget_class_bind_template_child (widget_class, EAPMethodFAST, pac_file_label);
	gtk_widget_class_bind_template_child (widget_class, EAPMethodFAST, pac_provision_check);
	gtk_widget_class_bind_template_child (widget_class, EAPMethodFAST, pac_provision_combo);
}

static void
eap_method_iface_init (EAPMethodInterface *iface)
{
	iface->validate = validate;
	iface->add_to_size_group = add_to_size_group;
	iface->fill_connection = fill_connection;
	iface->update_secrets = update_secrets;
	iface->get_default_field = get_default_field;
	iface->get_password_flags_name = get_password_flags_name;
	iface->get_username = get_username;
	iface->set_username = set_username;
	iface->get_password = get_password;
	iface->set_password = set_password;
	iface->get_show_password = get_show_password;
	iface->set_show_password = set_show_password;
}

EAPMethodFAST *
eap_method_fast_new (NMConnection *connection)
{
	EAPMethodFAST *self;
	GtkFileFilter *filter;
	NMSetting8021x *s_8021x = NULL;
	const char *filename;
	gboolean provisioning_enabled = TRUE;
	const gchar *phase2_auth = NULL;
	GtkTreeIter iter;

	self = g_object_new (eap_method_fast_get_type (), NULL);

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

	self->em_gtc = eap_method_simple_new (connection, "gtc", TRUE, FALSE);
	gtk_widget_show (GTK_WIDGET (self->em_gtc));
	g_signal_connect_object (self->em_gtc, "changed", G_CALLBACK (eap_method_emit_changed), self, G_CONNECT_SWAPPED);

	self->em_mschap_v2 = eap_method_simple_new (connection, "mschapv2", TRUE, FALSE);
	gtk_widget_show (GTK_WIDGET (self->em_mschap_v2));
	g_signal_connect_object (self->em_mschap_v2, "changed", G_CALLBACK (eap_method_emit_changed), self, G_CONNECT_SWAPPED);

	if (s_8021x) {
		if (nm_setting_802_1x_get_phase2_auth (s_8021x))
			phase2_auth = nm_setting_802_1x_get_phase2_auth (s_8021x);
		else if (nm_setting_802_1x_get_phase2_autheap (s_8021x))
			phase2_auth = nm_setting_802_1x_get_phase2_autheap (s_8021x);
	}
	if (phase2_auth == NULL)
		phase2_auth = "gtc";

	if (gtk_tree_model_get_iter_first (GTK_TREE_MODEL (self->inner_auth_model), &iter)) {
		do {
			g_autofree gchar *id = NULL;
			gtk_tree_model_get (GTK_TREE_MODEL (self->inner_auth_model), &iter, I_ID_COLUMN, &id, -1);
			if (strcmp (id, phase2_auth) == 0)
				gtk_combo_box_set_active_iter (self->inner_auth_combo, &iter);
		} while (gtk_tree_model_iter_next (GTK_TREE_MODEL (self->inner_auth_model), &iter));
	}

	g_signal_connect_swapped (self->inner_auth_combo, "changed", G_CALLBACK (inner_auth_combo_changed_cb), self);
	inner_auth_combo_changed_cb (self);

	return self;
}

