/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*-
 *
 * Copyright 2012  Red Hat, Inc,
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

#include "config.h"

#include <unistd.h>
#include <stdlib.h>

#include <glib.h>
#include <glib/gi18n.h>
#include <glib/gstdio.h>
#include <gtk/gtk.h>
#include <gdesktop-enums.h>

#include <cups/cups.h>

#include "list-box-helper.h"
#include "pp-jobs-dialog.h"
#include "pp-utils.h"
#include "pp-job.h"
#include "pp-cups.h"
#include "pp-printer.h"

#define EMPTY_TEXT "\xe2\x80\x94"

#define CLOCK_SCHEMA "org.gnome.desktop.interface"
#define CLOCK_FORMAT_KEY "clock-format"

static void pp_jobs_dialog_hide (PpJobsDialog *self);

struct _PpJobsDialog {
  GtkBuilder *builder;
  GtkWidget  *parent;

  GtkWidget  *dialog;
  GListStore *store;
  GtkListBox *listbox;

  UserResponseCallback user_callback;
  gpointer             user_data;

  gchar *printer_name;

  gchar    **actual_auth_info_required;
  gboolean   jobs_filled;
  gboolean   pop_up_authentication_popup;

  GCancellable *get_jobs_cancellable;
};

static gboolean
is_info_required (PpJobsDialog *self,
                  const gchar  *info)
{
  gboolean   required = FALSE;
  gint       i;

  if (self != NULL && self->actual_auth_info_required != NULL)
    {
      for (i = 0; self->actual_auth_info_required[i] != NULL; i++)
        {
          if (g_strcmp0 (self->actual_auth_info_required[i], info) == 0)
            {
              required = TRUE;
              break;
            }
        }
    }

  return required;
}

static gboolean
is_domain_required (PpJobsDialog *self)
{
  return is_info_required (self, "domain");
}

static gboolean
is_username_required (PpJobsDialog *self)
{
  return is_info_required (self, "username");
}

static gboolean
is_password_required (PpJobsDialog *self)
{
  return is_info_required (self, "password");
}

static gboolean
auth_popup_filled (PpJobsDialog *self)
{
  gboolean domain_required;
  gboolean username_required;
  gboolean password_required;
  guint16  domain_length;
  guint16  username_length;
  guint16  password_length;

  domain_required = is_domain_required (self);
  username_required = is_username_required (self);
  password_required = is_password_required (self);

  domain_length = gtk_entry_get_text_length (GTK_ENTRY (gtk_builder_get_object (self->builder, "domain-entry")));
  username_length = gtk_entry_get_text_length (GTK_ENTRY (gtk_builder_get_object (self->builder, "username-entry")));
  password_length = gtk_entry_get_text_length (GTK_ENTRY (gtk_builder_get_object (self->builder, "password-entry")));

  return (!domain_required || domain_length > 0) &&
         (!username_required || username_length > 0) &&
         (!password_required || password_length > 0);
}

static void
auth_entries_changed (GtkEntry     *entry,
                      PpJobsDialog *self)
{
  GtkWidget *widget;

  widget = GTK_WIDGET (gtk_builder_get_object (self->builder, "authenticate-button"));
  gtk_widget_set_sensitive (widget, auth_popup_filled (self));
}

static void
auth_entries_activated (GtkEntry     *entry,
                        PpJobsDialog *self)
{
  GtkWidget *widget;

  widget = GTK_WIDGET (gtk_builder_get_object (self->builder, "authenticate-button"));
  if (auth_popup_filled (self))
    gtk_button_clicked (GTK_BUTTON (widget));
}

static void
authenticate_popover_update (PpJobsDialog *self)
{
  GtkWidget *widget;
  gboolean   domain_required;
  gboolean   username_required;
  gboolean   password_required;

  domain_required = is_domain_required (self);
  username_required = is_username_required (self);
  password_required = is_password_required (self);

  widget = GTK_WIDGET (gtk_builder_get_object (GTK_BUILDER (self->builder), "domain-label"));
  gtk_widget_set_visible (widget, domain_required);
  widget = GTK_WIDGET (gtk_builder_get_object (GTK_BUILDER (self->builder), "domain-entry"));
  gtk_widget_set_visible (widget, domain_required);
  if (domain_required)
    gtk_entry_set_text (GTK_ENTRY (widget), "");

  widget = GTK_WIDGET (gtk_builder_get_object (GTK_BUILDER (self->builder), "username-label"));
  gtk_widget_set_visible (widget, username_required);
  widget = GTK_WIDGET (gtk_builder_get_object (GTK_BUILDER (self->builder), "username-entry"));
  gtk_widget_set_visible (widget, username_required);
  if (username_required)
    gtk_entry_set_text (GTK_ENTRY (widget), cupsUser ());

  widget = GTK_WIDGET (gtk_builder_get_object (GTK_BUILDER (self->builder), "password-label"));
  gtk_widget_set_visible (widget, password_required);
  widget = GTK_WIDGET (gtk_builder_get_object (GTK_BUILDER (self->builder), "password-entry"));
  gtk_widget_set_visible (widget, password_required);
  if (password_required)
    gtk_entry_set_text (GTK_ENTRY (widget), "");

  widget = GTK_WIDGET (gtk_builder_get_object (GTK_BUILDER (self->builder), "authenticate-button"));
  gtk_widget_set_sensitive (widget, FALSE);
}

static void
job_stop_cb (GtkButton *button,
             PpJob     *job)
{
  pp_job_cancel_purge_async (job, FALSE);
}

static void
job_pause_cb (GtkButton *button,
              PpJob     *job)
{
  gint job_state;

  g_object_get (job, "state", &job_state, NULL);

  pp_job_set_hold_until_async (job, job_state == IPP_JOB_HELD ? "no-hold" : "indefinite");

  gtk_button_set_image (button,
                        gtk_image_new_from_icon_name (job_state == IPP_JOB_HELD ?
                                                      "media-playback-pause-symbolic" : "media-playback-start-symbolic",
                                                      GTK_ICON_SIZE_SMALL_TOOLBAR));
}

static GtkWidget *
create_listbox_row (gpointer item,
                    gpointer user_data)
{
  GtkWidget  *widget;
  GtkWidget  *box;
  PpJob      *job = (PpJob *)item;
  gchar     **auth_info_required;
  gchar      *title;
  gchar      *state_string = NULL;
  gint        job_state;

  g_object_get (job,
                "title", &title,
                "state", &job_state,
                "auth-info-required", &auth_info_required,
                NULL);

  switch (job_state)
    {
      case IPP_JOB_PENDING:
        /* Translators: Job's state (job is waiting to be printed) */
        state_string = g_strdup (C_("print job", "Pending"));
        break;
      case IPP_JOB_HELD:
        if (auth_info_required == NULL)
          {
            /* Translators: Job's state (job is held for printing) */
            state_string = g_strdup (C_("print job", "Paused"));
          }
        else
          {
            /* Translators: Job's state (job needs authentication to proceed further) */
            state_string = g_strdup_printf ("<span foreground=\"#ff0000\">%s</span>", C_("print job", "Authentication required"));
          }
        break;
      case IPP_JOB_PROCESSING:
        /* Translators: Job's state (job is currently printing) */
        state_string = g_strdup (C_("print job", "Processing"));
        break;
      case IPP_JOB_STOPPED:
        /* Translators: Job's state (job has been stopped) */
        state_string = g_strdup (C_("print job", "Stopped"));
        break;
      case IPP_JOB_CANCELED:
        /* Translators: Job's state (job has been canceled) */
        state_string = g_strdup (C_("print job", "Canceled"));
        break;
      case IPP_JOB_ABORTED:
        /* Translators: Job's state (job has aborted due to error) */
        state_string = g_strdup (C_("print job", "Aborted"));
        break;
      case IPP_JOB_COMPLETED:
        /* Translators: Job's state (job has completed successfully) */
        state_string = g_strdup (C_("print job", "Completed"));
        break;
    }

  box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
  g_object_set (box, "margin", 6, NULL);
  gtk_container_set_border_width (GTK_CONTAINER (box), 2);

  widget = gtk_label_new (title);
  gtk_label_set_max_width_chars (GTK_LABEL (widget), 40);
  gtk_label_set_ellipsize (GTK_LABEL (widget), PANGO_ELLIPSIZE_END);
  gtk_widget_set_halign (widget, GTK_ALIGN_START);
  gtk_box_pack_start (GTK_BOX (box), widget, TRUE, TRUE, 10);

  widget = gtk_label_new (NULL);
  gtk_label_set_markup (GTK_LABEL (widget), state_string);
  gtk_widget_set_halign (widget, GTK_ALIGN_END);
  gtk_widget_set_margin_end (widget, 64);
  gtk_widget_set_margin_start (widget, 64);
  gtk_box_pack_start (GTK_BOX (box), widget, FALSE, FALSE, 10);

  widget = gtk_button_new_from_icon_name (job_state == IPP_JOB_HELD ? "media-playback-start-symbolic" : "media-playback-pause-symbolic",
                                          GTK_ICON_SIZE_SMALL_TOOLBAR);
  g_signal_connect (widget, "clicked", G_CALLBACK (job_pause_cb), item);
  gtk_widget_set_sensitive (widget, auth_info_required == NULL);
  gtk_box_pack_start (GTK_BOX (box), widget, FALSE, FALSE, 4);

  widget = gtk_button_new_from_icon_name ("edit-delete-symbolic",
                                          GTK_ICON_SIZE_SMALL_TOOLBAR);
  g_signal_connect (widget, "clicked", G_CALLBACK (job_stop_cb), item);
  gtk_box_pack_start (GTK_BOX (box), widget, FALSE, FALSE, 4);

  gtk_widget_show_all (box);

  return box;
}

static void
pop_up_authentication_popup (PpJobsDialog *self)
{
  GtkWidget *widget;

  if (self->actual_auth_info_required != NULL)
    {
      widget = GTK_WIDGET (gtk_builder_get_object (GTK_BUILDER (self->builder), "authenticate-jobs-button"));
      gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (widget), TRUE);
    }
}

static void
update_jobs_list_cb (GObject      *source_object,
                     GAsyncResult *result,
                     gpointer      user_data)
{
  PpJobsDialog     *self = user_data;
  PpPrinter        *printer = PP_PRINTER (source_object);
  GtkWidget        *clear_all_button;
  GtkWidget        *infobar;
  GtkWidget        *label;
  GtkStack         *stack;
  g_autoptr(GError) error = NULL;
  GList            *jobs, *l;
  PpJob            *job;
  gchar           **auth_info_required = NULL;
  gchar            *text;
  gint              num_of_jobs, num_of_auth_jobs = 0;

  g_list_store_remove_all (self->store);

  stack = GTK_STACK (gtk_builder_get_object (GTK_BUILDER (self->builder), "stack"));
  clear_all_button = GTK_WIDGET (gtk_builder_get_object (GTK_BUILDER (self->builder), "jobs-clear-all-button"));

  jobs = pp_printer_get_jobs_finish (printer, result, &error);
  if (error != NULL)
    {
      if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
        {
          g_warning ("Could not get jobs: %s", error->message);
        }

      return;
    }

  num_of_jobs = g_list_length (jobs);
  if (num_of_jobs > 0)
    {
      gtk_widget_set_sensitive (clear_all_button, TRUE);
      gtk_stack_set_visible_child_name (stack, "list-jobs-page");
    }
  else
    {
      gtk_widget_set_sensitive (clear_all_button, FALSE);
      gtk_stack_set_visible_child_name (stack, "no-jobs-page");
    }

  for (l = jobs; l != NULL; l = l->next)
    {
      job = PP_JOB (l->data);

      g_list_store_append (self->store, job);

      g_object_get (G_OBJECT (job),
                    "auth-info-required", &auth_info_required,
                    NULL);
      if (auth_info_required != NULL)
        {
          num_of_auth_jobs++;

          if (self->actual_auth_info_required == NULL)
            self->actual_auth_info_required = auth_info_required;
          else
            g_strfreev (auth_info_required);

          auth_info_required = NULL;
        }
    }

  infobar = GTK_WIDGET (gtk_builder_get_object (GTK_BUILDER (self->builder), "authentication-infobar"));
  if (num_of_auth_jobs > 0)
    {
      label = GTK_WIDGET (gtk_builder_get_object (GTK_BUILDER (self->builder), "authenticate-jobs-label"));

      /* Translators: This label shows how many jobs of this printer needs to be authenticated to be printed. */
      text = g_strdup_printf (ngettext ("%u Job Requires Authentication", "%u Jobs Require Authentication", num_of_auth_jobs), num_of_auth_jobs);
      gtk_label_set_text (GTK_LABEL (label), text);
      g_free (text);

      gtk_widget_show (infobar);
    }
  else
    {
      gtk_widget_hide (infobar);
    }

  authenticate_popover_update (self);

  g_list_free (jobs);
  g_clear_object (&self->get_jobs_cancellable);

  if (!self->jobs_filled)
    {
      if (self->pop_up_authentication_popup)
        {
          pop_up_authentication_popup (self);
          self->pop_up_authentication_popup = FALSE;
        }

      self->jobs_filled = TRUE;
    }
}

static void
update_jobs_list (PpJobsDialog *self)
{
  PpPrinter *printer;

  if (self->printer_name != NULL)
    {
      g_cancellable_cancel (self->get_jobs_cancellable);
      g_clear_object (&self->get_jobs_cancellable);

      self->get_jobs_cancellable = g_cancellable_new ();

      printer = pp_printer_new (self->printer_name);
      pp_printer_get_jobs_async (printer,
                                 TRUE,
                                 CUPS_WHICHJOBS_ACTIVE,
                                 self->get_jobs_cancellable,
                                 update_jobs_list_cb,
                                 self);
    }
}

static void
jobs_dialog_response_cb (GtkDialog *dialog,
                         gint       response_id,
                         gpointer   user_data)
{
  PpJobsDialog *self = (PpJobsDialog*) user_data;

  pp_jobs_dialog_hide (self);

  self->user_callback (GTK_DIALOG (self->dialog),
                       response_id,
                       self->user_data);
}

static void
on_clear_all_button_clicked (GtkButton *button,
                             gpointer   user_data)
{
  PpJobsDialog *self = user_data;
  guint num_items;
  guint i;

  num_items = g_list_model_get_n_items (G_LIST_MODEL (self->store));

  for (i = 0; i < num_items; i++)
    {
      PpJob *job = PP_JOB (g_list_model_get_item (G_LIST_MODEL (self->store), i));

      pp_job_cancel_purge_async (job, FALSE);
    }
}

static void
pp_job_authenticate_cb (GObject      *source_object,
                        GAsyncResult *res,
                        gpointer      user_data)
{
  PpJobsDialog     *self = user_data;
  gboolean          result;
  g_autoptr(GError) error = NULL;
  PpJob            *job = PP_JOB (source_object);

  result = pp_job_authenticate_finish (job, res, &error);
  if (result)
    {
      pp_jobs_dialog_update (self);
    }
  else if (error != NULL)
    {
      if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
        {
          g_warning ("Could not authenticate job: %s", error->message);
        }
    }
}

static void
authenticate_button_clicked (GtkWidget *button,
                             gpointer   user_data)
{
  PpJobsDialog *self = user_data;
  GtkWidget    *widget;
  PpJob        *job;
  gchar       **auth_info_required = NULL;
  gchar       **auth_info;
  guint         num_items;
  gint          i;

  auth_info = g_new0 (gchar *, g_strv_length (self->actual_auth_info_required) + 1);
  for (i = 0; self->actual_auth_info_required[i] != NULL; i++)
    {
      if (g_strcmp0 (self->actual_auth_info_required[i], "domain") == 0)
        widget = GTK_WIDGET (gtk_builder_get_object (GTK_BUILDER (self->builder), "domain-entry"));
      else if (g_strcmp0 (self->actual_auth_info_required[i], "username") == 0)
        widget = GTK_WIDGET (gtk_builder_get_object (GTK_BUILDER (self->builder), "username-entry"));
      else if (g_strcmp0 (self->actual_auth_info_required[i], "password") == 0)
        widget = GTK_WIDGET (gtk_builder_get_object (GTK_BUILDER (self->builder), "password-entry"));
      else
        widget = NULL;

      if (widget != NULL)
        auth_info[i] = g_strdup (gtk_entry_get_text (GTK_ENTRY (widget)));
    }

  num_items = g_list_model_get_n_items (G_LIST_MODEL (self->store));
  for (i = 0; i < num_items; i++)
    {
      job = PP_JOB (g_list_model_get_item (G_LIST_MODEL (self->store), i));

      g_object_get (job, "auth-info-required", &auth_info_required, NULL);
      if (auth_info_required != NULL)
        {
          pp_job_authenticate_async (job, auth_info, NULL, pp_job_authenticate_cb, self);

          g_strfreev (auth_info_required);
          auth_info_required = NULL;
        }
    }

  g_strfreev (auth_info);
}

static gboolean
key_press_event_cb (GtkWidget   *widget,
                    GdkEventKey *event,
                    gpointer     user_data)
{
  if (event->keyval == GDK_KEY_Escape)
    gtk_dialog_response (GTK_DIALOG (widget), GTK_RESPONSE_CLOSE);

  return FALSE;
}

PpJobsDialog *
pp_jobs_dialog_new (GtkWindow            *parent,
                    UserResponseCallback  user_callback,
                    gpointer              user_data,
                    gchar                *printer_name)
{
  PpJobsDialog     *self;
  GtkWidget        *widget;
  g_autoptr(GError) error = NULL;
  gchar            *objects[] = { "jobs-dialog", "authentication_popover", NULL };
  gchar            *text;
  guint             builder_result;
  gchar            *title;

  self = g_new0 (PpJobsDialog, 1);

  self->builder = gtk_builder_new ();
  self->parent = GTK_WIDGET (parent);

  builder_result = gtk_builder_add_objects_from_resource (self->builder,
                                                          "/org/gnome/control-center/printers/jobs-dialog.ui",
                                                          objects, &error);

  if (builder_result == 0)
    {
      g_warning ("Could not load ui: %s", error->message);
      return NULL;
    }

  self->dialog = (GtkWidget *) gtk_builder_get_object (self->builder, "jobs-dialog");
  self->user_callback = user_callback;
  self->user_data = user_data;
  self->printer_name = g_strdup (printer_name);
  self->actual_auth_info_required = NULL;
  self->jobs_filled = FALSE;
  self->pop_up_authentication_popup = FALSE;

  /* connect signals */
  g_signal_connect (self->dialog, "delete-event", G_CALLBACK (gtk_widget_hide_on_delete), NULL);
  g_signal_connect (self->dialog, "response", G_CALLBACK (jobs_dialog_response_cb), self);
  g_signal_connect (self->dialog, "key-press-event", G_CALLBACK (key_press_event_cb), NULL);

  widget = GTK_WIDGET (gtk_builder_get_object (self->builder, "jobs-clear-all-button"));
  g_signal_connect (widget, "clicked", G_CALLBACK (on_clear_all_button_clicked), self);

  widget = GTK_WIDGET (gtk_builder_get_object (self->builder, "authenticate-button"));
  g_signal_connect (widget, "clicked", G_CALLBACK (authenticate_button_clicked), self);

  widget = GTK_WIDGET (gtk_builder_get_object (self->builder, "domain-entry"));
  g_signal_connect (widget, "changed", G_CALLBACK (auth_entries_changed), self);
  g_signal_connect (widget, "activate", G_CALLBACK (auth_entries_activated), self);

  widget = GTK_WIDGET (gtk_builder_get_object (self->builder, "username-entry"));
  g_signal_connect (widget, "changed", G_CALLBACK (auth_entries_changed), self);
  g_signal_connect (widget, "activate", G_CALLBACK (auth_entries_activated), self);

  widget = GTK_WIDGET (gtk_builder_get_object (self->builder, "password-entry"));
  g_signal_connect (widget, "changed", G_CALLBACK (auth_entries_changed), self);
  g_signal_connect (widget, "activate", G_CALLBACK (auth_entries_activated), self);

  /* Translators: This is the printer name for which we are showing the active jobs */
  title = g_strdup_printf (C_("Printer jobs dialog title", "%s — Active Jobs"), printer_name);
  gtk_window_set_title (GTK_WINDOW (self->dialog), title);
  g_free (title);

  /* Translators: The printer needs authentication info to print. */
  text = g_strdup_printf (_("Enter credentials to print from %s."), printer_name);
  widget = GTK_WIDGET (gtk_builder_get_object (GTK_BUILDER (self->builder), "authentication-label"));
  gtk_label_set_text (GTK_LABEL (widget), text);
  g_free (text);

  self->listbox = GTK_LIST_BOX (gtk_builder_get_object (self->builder, "jobs-listbox"));
  gtk_list_box_set_header_func (self->listbox,
                                cc_list_box_update_header_func, NULL, NULL);
  self->store = g_list_store_new (pp_job_get_type ());
  gtk_list_box_bind_model (self->listbox, G_LIST_MODEL (self->store),
                           create_listbox_row, NULL, NULL);

  update_jobs_list (self);

  gtk_window_set_transient_for (GTK_WINDOW (self->dialog), GTK_WINDOW (parent));
  gtk_window_present (GTK_WINDOW (self->dialog));
  gtk_widget_show_all (GTK_WIDGET (self->dialog));

  return self;
}

void
pp_jobs_dialog_update (PpJobsDialog *self)
{
  update_jobs_list (self);
}

void
pp_jobs_dialog_set_callback (PpJobsDialog         *self,
                             UserResponseCallback  user_callback,
                             gpointer              user_data)
{
  g_return_if_fail (self != NULL);

  self->user_callback = user_callback;
  self->user_data = user_data;
}

void
pp_jobs_dialog_free (PpJobsDialog *self)
{
  g_cancellable_cancel (self->get_jobs_cancellable);
  g_clear_object (&self->get_jobs_cancellable);

  gtk_widget_destroy (GTK_WIDGET (self->dialog));
  self->dialog = NULL;

  g_strfreev (self->actual_auth_info_required);

  g_clear_object (&self->builder);
  g_free (self->printer_name);
  g_free (self);
}

static void
pp_jobs_dialog_hide (PpJobsDialog *self)
{
  gtk_widget_hide (GTK_WIDGET (self->dialog));
}

void
pp_jobs_dialog_authenticate_jobs (PpJobsDialog *self)
{
  if (self->jobs_filled)
    pop_up_authentication_popup (self);
  else
    self->pop_up_authentication_popup = TRUE;
}
