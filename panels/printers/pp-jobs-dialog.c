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
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
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

#include "pp-jobs-dialog.h"
#include "pp-utils.h"

#define EMPTY_TEXT "\xe2\x80\x94"

#define CLOCK_SCHEMA "org.gnome.desktop.interface"
#define CLOCK_FORMAT_KEY "clock-format"

static void pp_jobs_dialog_hide (PpJobsDialog *dialog);

struct _PpJobsDialog {
  GtkBuilder *builder;
  GtkWidget  *parent;

  GtkWidget  *dialog;

  UserResponseCallback user_callback;
  gpointer             user_data;

  gchar *printer_name;

  cups_job_t *jobs;
  gint num_jobs;
  gint current_job_id;

  gint ref_count;
};

enum
{
  JOB_ID_COLUMN,
  JOB_TITLE_COLUMN,
  JOB_STATE_COLUMN,
  JOB_CREATION_TIME_COLUMN,
  JOB_N_COLUMNS
};

static void
update_jobs_list_cb (cups_job_t *jobs,
                     gint        num_of_jobs,
                     gpointer    user_data)
{
  GtkTreeSelection *selection;
  PpJobsDialog     *dialog = (PpJobsDialog *) user_data;
  GtkListStore     *store;
  GtkTreeView      *treeview;
  GtkTreeIter       select_iter;
  GtkTreeIter       iter;
  GSettings        *settings;
  gboolean          select_iter_set = FALSE;
  gint              i;
  gint              select_index = 0;

  treeview = (GtkTreeView*)
    gtk_builder_get_object (dialog->builder, "job-treeview");

  if (dialog->num_jobs > 0)
    cupsFreeJobs (dialog->num_jobs, dialog->jobs);

  dialog->num_jobs = num_of_jobs;
  dialog->jobs = jobs;

  store = gtk_list_store_new (JOB_N_COLUMNS,
                              G_TYPE_INT,
                              G_TYPE_STRING,
                              G_TYPE_STRING,
                              G_TYPE_STRING);

  if (dialog->current_job_id >= 0)
    {
      for (i = 0; i < dialog->num_jobs; i++)
        {
          select_index = i;
          if (dialog->jobs[i].id >= dialog->current_job_id)
            break;
        }
    }

  for (i = 0; i < dialog->num_jobs; i++)
    {
      GDesktopClockFormat  value;
      GDateTime           *time;
      struct tm *ts;
      gchar     *time_string;
      gchar     *state = NULL;

      ts = localtime (&(dialog->jobs[i].creation_time));
      time = g_date_time_new_local (ts->tm_year,
                                    ts->tm_mon,
                                    ts->tm_mday,
                                    ts->tm_hour,
                                    ts->tm_min,
                                    ts->tm_sec);

      settings = g_settings_new (CLOCK_SCHEMA);
      value = g_settings_get_enum (settings, CLOCK_FORMAT_KEY);

      if (value == G_DESKTOP_CLOCK_FORMAT_24H)
        time_string = g_date_time_format (time, "%k:%M");
      else
        time_string = g_date_time_format (time, "%l:%M %p");

      g_date_time_unref (time);

      switch (dialog->jobs[i].state)
        {
          case IPP_JOB_PENDING:
            /* Translators: Job's state (job is waiting to be printed) */
            state = g_strdup (C_("print job", "Pending"));
            break;
          case IPP_JOB_HELD:
            /* Translators: Job's state (job is held for printing) */
            state = g_strdup (C_("print job", "Held"));
            break;
          case IPP_JOB_PROCESSING:
            /* Translators: Job's state (job is currently printing) */
            state = g_strdup (C_("print job", "Processing"));
            break;
          case IPP_JOB_STOPPED:
            /* Translators: Job's state (job has been stopped) */
            state = g_strdup (C_("print job", "Stopped"));
            break;
          case IPP_JOB_CANCELED:
            /* Translators: Job's state (job has been canceled) */
            state = g_strdup (C_("print job", "Canceled"));
            break;
          case IPP_JOB_ABORTED:
            /* Translators: Job's state (job has aborted due to error) */
            state = g_strdup (C_("print job", "Aborted"));
            break;
          case IPP_JOB_COMPLETED:
            /* Translators: Job's state (job has completed successfully) */
            state = g_strdup (C_("print job", "Completed"));
            break;
        }

      gtk_list_store_append (store, &iter);
      gtk_list_store_set (store, &iter,
                          JOB_ID_COLUMN, dialog->jobs[i].id,
                          JOB_TITLE_COLUMN, dialog->jobs[i].title,
                          JOB_STATE_COLUMN, state,
                          JOB_CREATION_TIME_COLUMN, time_string,
                          -1);

      if (i == select_index)
        {
          select_iter = iter;
          select_iter_set = TRUE;
          dialog->current_job_id = dialog->jobs[i].id;
        }

      g_free (time_string);
      g_free (state);
    }

  gtk_tree_view_set_model (treeview, GTK_TREE_MODEL (store));

  if (select_iter_set &&
      (selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (treeview))))
    {
      gtk_tree_selection_select_iter (selection, &select_iter);
    }

  g_object_unref (store);
  dialog->ref_count--;
}

static void
update_jobs_list (PpJobsDialog *dialog)
{
  if (dialog->printer_name)
    {
      dialog->ref_count++;
      cups_get_jobs_async (dialog->printer_name,
                           TRUE,
                           CUPS_WHICHJOBS_ACTIVE,
                           update_jobs_list_cb,
                           dialog);
    }
}

static void
job_selection_changed_cb (GtkTreeSelection *selection,
                          gpointer          user_data)
{
  PpJobsDialog *dialog = (PpJobsDialog *) user_data;
  GtkTreeModel *model;
  GtkTreeIter   iter;
  GtkWidget    *widget;
  gboolean      release_button_sensitive = FALSE;
  gboolean      hold_button_sensitive = FALSE;
  gboolean      cancel_button_sensitive = FALSE;
  gint          id = -1;
  gint          i;

  if (gtk_tree_selection_get_selected (selection, &model, &iter))
    {
      gtk_tree_model_get (model, &iter,
                          JOB_ID_COLUMN, &id,
                          -1);
    }
  else
    {
      id = -1;
    }

  dialog->current_job_id = id;

  if (dialog->current_job_id >= 0 &&
      dialog->jobs != NULL)
    {
      for (i = 0; i < dialog->num_jobs; i++)
        {
          if (dialog->jobs[i].id == dialog->current_job_id)
            {
              ipp_jstate_t job_state = dialog->jobs[i].state;

              release_button_sensitive = job_state == IPP_JOB_HELD;
              hold_button_sensitive = job_state == IPP_JOB_PENDING;
              cancel_button_sensitive = job_state < IPP_JOB_CANCELED;

              break;
            }
        }
    }

  widget = (GtkWidget*)
    gtk_builder_get_object (dialog->builder, "job-release-button");
  gtk_widget_set_sensitive (widget, release_button_sensitive);

  widget = (GtkWidget*)
    gtk_builder_get_object (dialog->builder, "job-hold-button");
  gtk_widget_set_sensitive (widget, hold_button_sensitive);

  widget = (GtkWidget*)
    gtk_builder_get_object (dialog->builder, "job-cancel-button");
  gtk_widget_set_sensitive (widget, cancel_button_sensitive);
}

static void
populate_jobs_list (PpJobsDialog *dialog)
{
  GtkTreeViewColumn *column;
  GtkCellRenderer   *renderer;
  GtkCellRenderer   *title_renderer;
  GtkTreeView       *treeview;

  treeview = (GtkTreeView*)
    gtk_builder_get_object (dialog->builder, "job-treeview");

  renderer = gtk_cell_renderer_text_new ();
  title_renderer = gtk_cell_renderer_text_new ();

  /* Translators: Name of column showing titles of print jobs */
  column = gtk_tree_view_column_new_with_attributes (_("Job Title"), title_renderer,
                                                     "text", JOB_TITLE_COLUMN, NULL);
  g_object_set (G_OBJECT (title_renderer), "ellipsize", PANGO_ELLIPSIZE_END, NULL);
  gtk_tree_view_column_set_fixed_width (column, 180);
  gtk_tree_view_column_set_min_width (column, 180);
  gtk_tree_view_column_set_max_width (column, 180);
  gtk_tree_view_append_column (treeview, column);

  /* Translators: Name of column showing statuses of print jobs */
  column = gtk_tree_view_column_new_with_attributes (_("Job State"), renderer,
                                                     "text", JOB_STATE_COLUMN, NULL);
  gtk_tree_view_column_set_expand (column, TRUE);
  gtk_tree_view_append_column (treeview, column);

  /* Translators: Name of column showing times of creation of print jobs */
  column = gtk_tree_view_column_new_with_attributes (_("Time"), renderer,
                                                     "text", JOB_CREATION_TIME_COLUMN, NULL);
  gtk_tree_view_column_set_expand (column, TRUE);
  gtk_tree_view_append_column (treeview, column);

  g_signal_connect (gtk_tree_view_get_selection (treeview),
                    "changed", G_CALLBACK (job_selection_changed_cb), dialog);

  update_jobs_list (dialog);
}

static void
job_process_cb_cb (gpointer user_data)
{
}

static void
job_process_cb (GtkButton *button,
                gpointer   user_data)
{
  PpJobsDialog *dialog = (PpJobsDialog *) user_data;
  GtkWidget    *widget;

  if (dialog->current_job_id >= 0)
    {
      if ((GtkButton*) gtk_builder_get_object (dialog->builder,
                                               "job-cancel-button") ==
          button)
        {
          job_cancel_purge_async (dialog->current_job_id,
                                  FALSE,
                                  NULL,
                                  job_process_cb_cb,
                                  dialog);
        }
      else if ((GtkButton*) gtk_builder_get_object (dialog->builder,
                                                    "job-hold-button") ==
               button)
        {
          job_set_hold_until_async (dialog->current_job_id,
                                    "indefinite",
                                    NULL,
                                    job_process_cb_cb,
                                    dialog);
        }
      else
        {
          job_set_hold_until_async (dialog->current_job_id,
                                    "no-hold",
                                    NULL,
                                    job_process_cb_cb,
                                    dialog);
        }
  }

  widget = (GtkWidget*)
    gtk_builder_get_object (dialog->builder, "job-release-button");
  gtk_widget_set_sensitive (widget, FALSE);

  widget = (GtkWidget*)
    gtk_builder_get_object (dialog->builder, "job-hold-button");
  gtk_widget_set_sensitive (widget, FALSE);

  widget = (GtkWidget*)
    gtk_builder_get_object (dialog->builder, "job-cancel-button");
  gtk_widget_set_sensitive (widget, FALSE);
}

static void
jobs_dialog_response_cb (GtkDialog *dialog,
                         gint       response_id,
                         gpointer   user_data)
{
  PpJobsDialog *jobs_dialog = (PpJobsDialog*) user_data;

  pp_jobs_dialog_hide (jobs_dialog);

  jobs_dialog->user_callback (GTK_DIALOG (jobs_dialog->dialog),
                              response_id,
                              jobs_dialog->user_data);
}

static void
update_alignment_padding (GtkWidget     *widget,
                          GtkAllocation *allocation,
                          gpointer       user_data)
{
  GtkAllocation  allocation2;
  PpJobsDialog  *dialog = (PpJobsDialog*) user_data;
  GtkWidget     *action_area;
  gint           offset_left, offset_right;
  guint          padding_left, padding_right,
                 padding_top, padding_bottom;

  action_area = (GtkWidget*)
    gtk_builder_get_object (dialog->builder, "dialog-action-area1");
  gtk_widget_get_allocation (action_area, &allocation2);

  offset_left = allocation2.x - allocation->x;
  offset_right = (allocation->x + allocation->width) -
                 (allocation2.x + allocation2.width);

  gtk_alignment_get_padding  (GTK_ALIGNMENT (widget),
                              &padding_top, &padding_bottom,
                              &padding_left, &padding_right);
  if (allocation->x >= 0 && allocation2.x >= 0)
    {
      if (offset_left > 0 && offset_left != padding_left)
        gtk_alignment_set_padding (GTK_ALIGNMENT (widget),
                                   padding_top, padding_bottom,
                                   offset_left, padding_right);

      gtk_alignment_get_padding  (GTK_ALIGNMENT (widget),
                                  &padding_top, &padding_bottom,
                                  &padding_left, &padding_right);
      if (offset_right > 0 && offset_right != padding_right)
        gtk_alignment_set_padding (GTK_ALIGNMENT (widget),
                                   padding_top, padding_bottom,
                                   padding_left, offset_right);
    }
}

PpJobsDialog *
pp_jobs_dialog_new (GtkWindow            *parent,
                    UserResponseCallback  user_callback,
                    gpointer              user_data,
                    gchar                *printer_name)
{
  PpJobsDialog *dialog;
  GtkWidget    *widget;
  GError       *error = NULL;
  gchar        *objects[] = { "jobs-dialog", NULL };
  guint         builder_result;
  gchar        *title;

  dialog = g_new0 (PpJobsDialog, 1);

  dialog->builder = gtk_builder_new ();
  dialog->parent = GTK_WIDGET (parent);

  builder_result = gtk_builder_add_objects_from_file (dialog->builder,
                                                      DATADIR"/jobs-dialog.ui",
                                                      objects, &error);

  if (builder_result == 0)
    {
      g_warning ("Could not load ui: %s", error->message);
      g_error_free (error);
      return NULL;
    }

  dialog->dialog = (GtkWidget *) gtk_builder_get_object (dialog->builder, "jobs-dialog");
  dialog->user_callback = user_callback;
  dialog->user_data = user_data;
  dialog->printer_name = g_strdup (printer_name);
  dialog->current_job_id = -1;
  dialog->ref_count = 0;

  /* connect signals */
  g_signal_connect (dialog->dialog, "delete-event", G_CALLBACK (gtk_widget_hide_on_delete), NULL);
  g_signal_connect (dialog->dialog, "response", G_CALLBACK (jobs_dialog_response_cb), dialog);

  widget = (GtkWidget*)
    gtk_builder_get_object (dialog->builder, "content-alignment");
  g_signal_connect (widget, "size-allocate", G_CALLBACK (update_alignment_padding), dialog);

  widget = (GtkWidget*)
    gtk_builder_get_object (dialog->builder, "job-cancel-button");
  g_signal_connect (widget, "clicked", G_CALLBACK (job_process_cb), dialog);

  widget = (GtkWidget*)
    gtk_builder_get_object (dialog->builder, "job-hold-button");
  g_signal_connect (widget, "clicked", G_CALLBACK (job_process_cb), dialog);

  widget = (GtkWidget*)
    gtk_builder_get_object (dialog->builder, "job-release-button");
  g_signal_connect (widget, "clicked", G_CALLBACK (job_process_cb), dialog);

  widget = (GtkWidget*)
    gtk_builder_get_object (dialog->builder, "jobs-title");
  title = g_strdup_printf (_("%s Active Jobs"), printer_name);
  gtk_label_set_label (GTK_LABEL (widget), title);
  g_free (title);

  populate_jobs_list (dialog);

  gtk_window_set_transient_for (GTK_WINDOW (dialog->dialog), GTK_WINDOW (parent));
  gtk_window_present (GTK_WINDOW (dialog->dialog));
  gtk_widget_show_all (GTK_WIDGET (dialog->dialog));

  return dialog;
}

void
pp_jobs_dialog_update (PpJobsDialog *dialog)
{
  update_jobs_list (dialog);
}

static gboolean
pp_jobs_dialog_free_idle (gpointer user_data)
{
  PpJobsDialog *dialog = (PpJobsDialog*) user_data;

  if (dialog->ref_count == 0)
    {
      gtk_widget_destroy (GTK_WIDGET (dialog->dialog));
      dialog->dialog = NULL;

      g_object_unref (dialog->builder);
      dialog->builder = NULL;

      if (dialog->num_jobs > 0)
        cupsFreeJobs (dialog->num_jobs, dialog->jobs);

      g_free (dialog->printer_name);

      g_free (dialog);

      return FALSE;
    }
  else
    {
      return TRUE;
    }
}

void
pp_jobs_dialog_free (PpJobsDialog *dialog)
{
  g_idle_add (pp_jobs_dialog_free_idle, dialog);
}

static void
pp_jobs_dialog_hide (PpJobsDialog *dialog)
{
  gtk_widget_hide (GTK_WIDGET (dialog->dialog));
}
