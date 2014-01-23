/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*-
 *
 * Copyright 2012 - 2013 Red Hat, Inc,
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
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 *
 * Author: Marek Kasik <mkasik@redhat.com>
 */

#include "pp-authentication-dialog.h"

struct _PpAuthenticationDialogPrivate
{
  GtkBuilder *builder;
  GtkWidget  *dialog;

  gchar *text;
  gchar *username;
};

#define PP_AUTHENTICATION_DIALOG_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE ((obj), PP_TYPE_AUTHENTICATION_DIALOG, PpAuthenticationDialogPrivate))

#define WID(s) GTK_WIDGET (gtk_builder_get_object (priv->builder, s))

static void pp_authentication_dialog_finalize     (GObject      *object);
static void pp_authentication_dialog_get_property (GObject      *object,
                                                   guint         prop_id,
                                                   GValue       *value,
                                                   GParamSpec   *param_spec);
static void pp_authentication_dialog_set_property (GObject      *object,
                                                   guint         prop_id,
                                                   const GValue *value,
                                                   GParamSpec   *param_spec);

enum
{
  RESPONSE,
  LAST_SIGNAL
};

enum
{
  PROP_0 = 0,
  PROP_TEXT,
  PROP_USERNAME,
};

static guint signals[LAST_SIGNAL] = { 0 };

G_DEFINE_TYPE (PpAuthenticationDialog, pp_authentication_dialog, G_TYPE_OBJECT)

static void
pp_authentication_dialog_class_init (PpAuthenticationDialogClass *klass)
{
  GObjectClass *object_class;

  object_class = G_OBJECT_CLASS (klass);
  object_class->finalize = pp_authentication_dialog_finalize;
  object_class->set_property = pp_authentication_dialog_set_property;
  object_class->get_property = pp_authentication_dialog_get_property;

  g_type_class_add_private (object_class, sizeof (PpAuthenticationDialogPrivate));

  g_object_class_install_property (object_class, PROP_TEXT,
    g_param_spec_string ("text",
                         "Text",
                         "Text of the dialog",
                         NULL,
                         G_PARAM_READWRITE));

  g_object_class_install_property (object_class, PROP_USERNAME,
    g_param_spec_string ("username",
                         "Username",
                         "Initial username",
                         NULL,
                         G_PARAM_READWRITE));

  /**
   * PpAuthenticationDialog::response:
   *
   * The signal which gets emitted after user enters authentication informations.
   */
  signals[RESPONSE] =
    g_signal_new ("response",
                  G_TYPE_FROM_CLASS (object_class),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (PpAuthenticationDialogClass, response),
                  NULL, NULL,
                  g_cclosure_marshal_generic,
                  G_TYPE_NONE, 3, G_TYPE_INT, G_TYPE_STRING, G_TYPE_STRING);
}

static void
pp_authentication_dialog_get_property (GObject    *object,
                                       guint       prop_id,
                                       GValue     *value,
                                       GParamSpec *param_spec)
{
  PpAuthenticationDialog        *dialog = PP_AUTHENTICATION_DIALOG (object);
  PpAuthenticationDialogPrivate *priv = dialog->priv;

  switch (prop_id)
    {
      case PROP_TEXT:
        g_value_set_string (value, priv->text);
        break;
      case PROP_USERNAME:
        g_value_set_string (value, priv->username);
        break;
      default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object,
                                           prop_id,
                                           param_spec);
      break;
    }
}

static void
pp_authentication_dialog_set_property (GObject      *object,
                                       guint         prop_id,
                                       const GValue *value,
                                       GParamSpec   *param_spec)
{
  PpAuthenticationDialog        *dialog = PP_AUTHENTICATION_DIALOG (object);
  PpAuthenticationDialogPrivate *priv = dialog->priv;

  switch (prop_id)
    {
      case PROP_TEXT:
        g_free (priv->text);
        priv->text = g_value_dup_string (value);
        if (priv->text)
          {
            gtk_label_set_text (GTK_LABEL (WID ("authentication-text")),
                                priv->text);
          }
        break;
      case PROP_USERNAME:
        g_free (priv->username);
        priv->username = g_value_dup_string (value);
        if (priv->username)
          {
            gtk_entry_set_text (GTK_ENTRY (WID ("username-entry")),
                                priv->username);
          }
        break;
      default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object,
                                           prop_id,
                                           param_spec);
        break;
    }
}

PpAuthenticationDialog *
pp_authentication_dialog_new (GtkWindow   *parent,
                              const gchar *text,
                              const gchar *default_username)
{
  PpAuthenticationDialogPrivate *priv;
  PpAuthenticationDialog        *dialog;
  GtkWidget                     *widget;

  dialog = g_object_new (PP_TYPE_AUTHENTICATION_DIALOG,
                         "text", text,
                         "username", default_username,
                         NULL);
  priv = dialog->priv;

  if (default_username && strlen (default_username) > 0)
    {
      widget = WID ("password-entry");
    }
  else
    {
      widget = WID ("username-entry");
    }

  gtk_widget_grab_focus (widget);

  gtk_window_set_transient_for (GTK_WINDOW (priv->dialog), GTK_WINDOW (parent));

  return PP_AUTHENTICATION_DIALOG (dialog);
}

static gchar *
get_entry_text (const gchar            *object_name,
                PpAuthenticationDialog *dialog)
{
  PpAuthenticationDialogPrivate *priv = dialog->priv;

  return g_strdup (gtk_entry_get_text (GTK_ENTRY (WID (object_name))));
}

static void
authentication_dialog_response_cb (GtkDialog *_dialog,
                                   gint       response_id,
                                   gpointer   user_data)
{
  PpAuthenticationDialog *dialog = (PpAuthenticationDialog*) user_data;
  gchar                  *password = NULL;
  gchar                  *username = NULL;

  if (response_id == GTK_RESPONSE_OK)
    {
      username = get_entry_text ("username-entry", dialog);
      password = get_entry_text ("password-entry", dialog);

      if (username[0] == '\0')
        g_clear_pointer (&username, g_free);

      if (password[0] == '\0')
        g_clear_pointer (&password, g_free);
    }

  g_signal_emit (dialog,
                 signals[RESPONSE],
                 0,
                 response_id,
                 username,
                 password);

  g_clear_pointer (&password, g_free);
  g_clear_pointer (&username, g_free);
}

static void
data_changed_cb (GtkEditable *editable,
                 gpointer     user_data)
{
  PpAuthenticationDialog        *dialog = (PpAuthenticationDialog*) user_data;
  PpAuthenticationDialogPrivate *priv = dialog->priv;

  gtk_widget_set_sensitive (WID ("authentication-button"),
                            strlen (gtk_entry_get_text (GTK_ENTRY (WID ("password-entry")))) > 0 &&
                            strlen (gtk_entry_get_text (GTK_ENTRY (WID ("username-entry")))) > 0);
}

static void
pp_authentication_dialog_init (PpAuthenticationDialog *dialog)
{
  PpAuthenticationDialogPrivate *priv;
  GError                        *error = NULL;
  gchar                         *objects[] = { "authentication-dialog", NULL };
  guint                          builder_result;

  dialog->priv = priv = PP_AUTHENTICATION_DIALOG_GET_PRIVATE (dialog);

  priv->builder = gtk_builder_new ();

  builder_result = gtk_builder_add_objects_from_resource (priv->builder,
                                                          "/org/gnome/control-center/printers/authentication-dialog.ui",
                                                          objects, &error);

  if (builder_result == 0)
    {
      g_warning ("Could not load ui: %s", error->message);
      g_clear_error (&error);
      return;
    }

  /* Construct dialog */
  priv->dialog = WID ("authentication-dialog");

  /* Connect signals */
  g_signal_connect (priv->dialog, "delete-event", G_CALLBACK (gtk_widget_hide_on_delete), NULL);
  g_signal_connect (priv->dialog, "response", G_CALLBACK (authentication_dialog_response_cb), dialog);
  g_signal_connect (G_OBJECT (WID ("password-entry")), "changed", G_CALLBACK (data_changed_cb), dialog);
  g_signal_connect (G_OBJECT (WID ("username-entry")), "changed", G_CALLBACK (data_changed_cb), dialog);

  gtk_widget_show (priv->dialog);
}

static void
pp_authentication_dialog_finalize (GObject *object)
{
  PpAuthenticationDialog        *dialog = PP_AUTHENTICATION_DIALOG (object);
  PpAuthenticationDialogPrivate *priv = dialog->priv;

  g_clear_pointer (&priv->dialog, gtk_widget_destroy);

  g_object_unref (priv->builder);
  g_free (priv->text);
  g_free (priv->username);

  G_OBJECT_CLASS (pp_authentication_dialog_parent_class)->finalize (object);
}
