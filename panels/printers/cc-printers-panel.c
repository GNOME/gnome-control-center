/*
 * Copyright (C) 2010 Red Hat, Inc
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
 */

#include <config.h>

#include "cc-printers-panel.h"

#include <string.h>
#include <glib/gi18n-lib.h>
#include <dbus/dbus-glib.h>

#include <cups/cups.h>

G_DEFINE_DYNAMIC_TYPE (CcPrintersPanel, cc_printers_panel, CC_TYPE_PANEL)

#define PRINTERS_PANEL_PRIVATE(o) \
  (G_TYPE_INSTANCE_GET_PRIVATE ((o), CC_TYPE_PRINTERS_PANEL, CcPrintersPanelPrivate))

#define MECHANISM_BUS "org.opensuse.CupsPkHelper.Mechanism"

struct _CcPrintersPanelPrivate
{
  GtkBuilder *builder;

  cups_dest_t *dests;
  int num_dests;
  int current_dest;

  cups_job_t *jobs;
  int num_jobs;
  int current_job;

  gchar **allowed_users;
  int num_allowed_users;
  int current_allowed_user;

  gpointer dummy;
};

static void actualize_jobs_list (CcPrintersPanel *self);
static void actualize_printers_list (CcPrintersPanel *self);
static void actualize_allowed_users_list (CcPrintersPanel *self);
static void printer_disable_cb (GtkToggleButton *togglebutton, gpointer user_data);

static void
cc_printers_panel_get_property (GObject    *object,
                               guint       property_id,
                               GValue     *value,
                               GParamSpec *pspec)
{
  switch (property_id)
    {
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
    }
}

static void
cc_printers_panel_set_property (GObject      *object,
                               guint         property_id,
                               const GValue *value,
                               GParamSpec   *pspec)
{
  switch (property_id)
    {
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
    }
}

static void
cc_printers_panel_dispose (GObject *object)
{
  G_OBJECT_CLASS (cc_printers_panel_parent_class)->dispose (object);
}

static void
cc_printers_panel_finalize (GObject *object)
{
  G_OBJECT_CLASS (cc_printers_panel_parent_class)->finalize (object);
}

static void
cc_printers_panel_class_init (CcPrintersPanelClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  g_type_class_add_private (klass, sizeof (CcPrintersPanelPrivate));

  object_class->get_property = cc_printers_panel_get_property;
  object_class->set_property = cc_printers_panel_set_property;
  object_class->dispose = cc_printers_panel_dispose;
  object_class->finalize = cc_printers_panel_finalize;
}

static void
cc_printers_panel_class_finalize (CcPrintersPanelClass *klass)
{
}

enum
{
  PRINTER_NAME_COLUMN,
  PRINTER_ID_COLUMN,
  PRINTER_N_COLUMNS
};

static void
printer_selection_changed_cb (GtkTreeSelection *selection,
                              gpointer          user_data)
{
  CcPrintersPanelPrivate *priv;
  CcPrintersPanel        *self = (CcPrintersPanel*) user_data;
  GtkTreeModel           *model;
  GtkTreeIter             iter;
  const gchar            *none = "---";
  GtkWidget              *widget;
  gboolean                paused = FALSE;
  gchar                  *instance = NULL;
  gchar                  *location = NULL;
  gchar                  *device_uri = NULL;
  int                     id, i;

  priv = PRINTERS_PANEL_PRIVATE (self);

  if (gtk_tree_selection_get_selected (selection, &model, &iter))
    gtk_tree_model_get (model, &iter,
			PRINTER_ID_COLUMN, &id,
			-1);
  else
    id = -1;

  priv->current_dest = id;

  actualize_jobs_list (self);
  actualize_allowed_users_list (self);

  if (priv->current_dest >= 0 &&
      priv->current_dest < priv->num_dests &&
      priv->dests != NULL)
    {
      if (priv->dests[id].instance)
        instance = g_strdup_printf ("%s / %s", priv->dests[id].name, priv->dests[id].instance);
      else
        instance = g_strdup (priv->dests[id].name);

      widget = (GtkWidget*)
        gtk_builder_get_object (priv->builder, "printer-name-label");
      gtk_label_set_text (GTK_LABEL (widget), instance);
      g_free (instance);

      for (i = 0; i < priv->dests[id].num_options; i++)
        {
          if (g_strcmp0 (priv->dests[id].options[i].name, "printer-location") == 0)
            location = g_strdup (priv->dests[id].options[i].value);
          else if (g_strcmp0 (priv->dests[id].options[i].name, "device-uri") == 0)
            device_uri = g_strdup (priv->dests[id].options[i].value);
          else if (g_strcmp0 (priv->dests[id].options[i].name, "printer-state") == 0)
            paused = (g_strcmp0 (priv->dests[id].options[i].value, "5") == 0);
        }

      widget = (GtkWidget*)
        gtk_builder_get_object (priv->builder, "printer-location-label");

      if (location)
        {
          gtk_label_set_text (GTK_LABEL (widget), location);
          g_free (location);
        }
      else
        gtk_label_set_text (GTK_LABEL (widget), none);

      widget = (GtkWidget*)
        gtk_builder_get_object (priv->builder, "printer-URI-entry");
      if (device_uri)
        {
          gtk_entry_set_text (GTK_ENTRY (widget), device_uri);
          g_free (device_uri);
        }
      else
        gtk_entry_set_text (GTK_ENTRY (widget), none);

      widget = (GtkWidget*)
        gtk_builder_get_object (priv->builder, "printer-disable-button");
      gtk_widget_set_sensitive (widget, TRUE);
      g_signal_handlers_block_by_func(G_OBJECT (widget), printer_disable_cb, self);
      gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (widget), paused);
      g_signal_handlers_unblock_by_func(G_OBJECT (widget), printer_disable_cb, self);
    }
  else
    {
      widget = (GtkWidget*)
        gtk_builder_get_object (priv->builder, "printer-name-label");
      gtk_label_set_text (GTK_LABEL (widget), "");

      widget = (GtkWidget*)
        gtk_builder_get_object (priv->builder, "printer-location-label");
      gtk_label_set_text (GTK_LABEL (widget), "");

      widget = (GtkWidget*)
        gtk_builder_get_object (priv->builder, "printer-URI-entry");
      gtk_entry_set_text (GTK_ENTRY (widget), "");

      widget = (GtkWidget*)
        gtk_builder_get_object (priv->builder, "printer-disable-button");
      gtk_widget_set_sensitive (widget, FALSE);
    }
}

static void
actualize_printers_list (CcPrintersPanel *self)
{
  CcPrintersPanelPrivate *priv;
  GtkListStore           *store;
  GtkTreeIter             selected_iter;
  GtkTreeView            *treeview;
  GtkTreeIter             iter;
  gchar                  *current_printer_instance = NULL;
  gchar                  *current_printer_name = NULL;
  int                     current_dest = -1;
  int                     i;

  priv = PRINTERS_PANEL_PRIVATE (self);

  if (priv->current_dest >= 0 &&
      priv->current_dest < priv->num_dests &&
      priv->dests != NULL)
    {
      current_printer_name = g_strdup (priv->dests[priv->current_dest].name);
      if (priv->dests[priv->current_dest].instance)
        current_printer_instance = g_strdup (priv->dests[priv->current_dest].instance);
    }

  priv->num_dests = cupsGetDests (&priv->dests);
  priv->current_dest = -1;

  treeview = (GtkTreeView*)
    gtk_builder_get_object (priv->builder, "printer-treeview");

  store = gtk_list_store_new (PRINTER_N_COLUMNS, G_TYPE_STRING, G_TYPE_INT);

  for (i = 0; i < priv->num_dests; i++)
    {
      gchar *instance;

      gtk_list_store_append (store, &iter);

      if (priv->dests[i].instance)
        {
          instance = g_strdup_printf ("%s / %s", priv->dests[i].name, priv->dests[i].instance);

          if (current_printer_instance &&
              g_strcmp0 (current_printer_name, priv->dests[i].name) == 0 &&
              g_strcmp0 (current_printer_instance, priv->dests[i].instance) == 0)
            {
              current_dest = i;
              selected_iter = iter;
            }
        }
      else
        {
          instance = g_strdup (priv->dests[i].name);

          if (current_printer_instance == NULL &&
              g_strcmp0 (current_printer_name, priv->dests[i].name) == 0)
            {
              current_dest = i;
              selected_iter = iter;
            }
        }


      gtk_list_store_set (store, &iter,
                          PRINTER_NAME_COLUMN, instance,
                          PRINTER_ID_COLUMN, i,
                          -1);
      g_free (instance);
    }

  gtk_tree_view_set_model (treeview, GTK_TREE_MODEL (store));

  if (current_dest >= 0)
    {
      priv->current_dest = current_dest;
      gtk_tree_selection_select_iter (
        gtk_tree_view_get_selection (GTK_TREE_VIEW (treeview)),
        &selected_iter);
    }
}

static void
populate_printers_list (CcPrintersPanel *self)
{
  CcPrintersPanelPrivate *priv;
  GtkTreeViewColumn      *column;
  GtkCellRenderer        *renderer;
  GtkWidget              *treeview;

  priv = PRINTERS_PANEL_PRIVATE (self);

  treeview = (GtkWidget*)
    gtk_builder_get_object (priv->builder, "printer-treeview");

  actualize_printers_list (self);

  renderer = gtk_cell_renderer_text_new ();
  column = gtk_tree_view_column_new_with_attributes ("Printer", renderer,
                                                     "text", PRINTER_NAME_COLUMN, NULL);

  g_signal_connect (gtk_tree_view_get_selection (GTK_TREE_VIEW (treeview)),
                    "changed", G_CALLBACK (printer_selection_changed_cb), self);

  gtk_tree_view_append_column (GTK_TREE_VIEW (treeview), column);
}

enum
{
  JOB_ID_COLUMN,
  JOB_TITLE_COLUMN,
  JOB_STATE_COLUMN,
  JOB_USER_COLUMN,
  JOB_CREATION_TIME_COLUMN,
  JOB_N_COLUMNS
};

static void
actualize_jobs_list (CcPrintersPanel *self)
{
  CcPrintersPanelPrivate *priv;
  GtkListStore           *store;
  GtkTreeView            *treeview;
  GtkTreeIter             iter;
  int                     i;

  priv = PRINTERS_PANEL_PRIVATE (self);

  treeview = (GtkTreeView*)
    gtk_builder_get_object (priv->builder, "job-treeview");

  priv->current_job = -1;
  if (priv->current_dest >= 0 &&
      priv->current_dest < priv->num_dests &&
      priv->dests != NULL)
    priv->num_jobs = cupsGetJobs (&priv->jobs, priv->dests[priv->current_dest].name, 1, CUPS_WHICHJOBS_ACTIVE);
  else
    {
      priv->num_jobs = -1;
      priv->jobs = NULL;
    }

  store = gtk_list_store_new (JOB_N_COLUMNS, G_TYPE_INT, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING);

  for (i = 0; i < priv->num_jobs; i++)
    {
      struct tm *ts;
      gchar     *time_string;
      gchar     *state = NULL;

      ts = localtime(&(priv->jobs[i].creation_time));
      time_string = g_strdup_printf ("%02d:%02d:%02d", ts->tm_hour, ts->tm_min, ts->tm_sec);

      switch (priv->jobs[i].state)
        {
          case IPP_JOB_PENDING:
            state = g_strdup (_("Pending"));
            break;
          case IPP_JOB_HELD:
            state = g_strdup (_("Held"));
            break;
          case IPP_JOB_PROCESSING:
            state = g_strdup (_("Processing"));
            break;
          case IPP_JOB_STOPPED:
            state = g_strdup (_("Stopped"));
            break;
          case IPP_JOB_CANCELED:
            state = g_strdup (_("Canceled"));
            break;
          case IPP_JOB_ABORTED:
            state = g_strdup (_("Aborted"));
            break;
          case IPP_JOB_COMPLETED:
            state = g_strdup (_("Completed"));
            break;
        }

      gtk_list_store_append (store, &iter);
      gtk_list_store_set (store, &iter,
                          JOB_ID_COLUMN, i,
                          JOB_TITLE_COLUMN, priv->jobs[i].title,
                          JOB_STATE_COLUMN, state,
                          JOB_USER_COLUMN, priv->jobs[i].user,
                          JOB_CREATION_TIME_COLUMN, time_string,
                          -1);

      g_free (time_string);
      g_free (state);
    }

  gtk_tree_view_set_model (treeview, GTK_TREE_MODEL (store));
}

static void
job_selection_changed_cb (GtkTreeSelection *selection,
                          gpointer          user_data)
{
  CcPrintersPanelPrivate *priv;
  CcPrintersPanel        *self = (CcPrintersPanel*) user_data;
  GtkTreeModel           *model;
  GtkTreeIter             iter;
  int                     id;

  priv = PRINTERS_PANEL_PRIVATE (self);

  if (gtk_tree_selection_get_selected (selection, &model, &iter))
    gtk_tree_model_get (model, &iter,
			JOB_ID_COLUMN, &id,
			-1);
  priv->current_job = id;
}

static void
populate_jobs_list (CcPrintersPanel *self)
{

  CcPrintersPanelPrivate *priv;
  GtkTreeViewColumn      *column;
  GtkCellRenderer        *renderer;
  GtkTreeView            *treeview;

  priv = PRINTERS_PANEL_PRIVATE (self);

  actualize_jobs_list (self);

  treeview = (GtkTreeView*)
    gtk_builder_get_object (priv->builder, "job-treeview");

  renderer = gtk_cell_renderer_text_new ();

  column = gtk_tree_view_column_new_with_attributes (_("Job Title"), renderer,
                                                     "text", JOB_TITLE_COLUMN, NULL);
  gtk_tree_view_append_column (treeview, column);

  column = gtk_tree_view_column_new_with_attributes (_("Job State"), renderer,
                                                     "text", JOB_STATE_COLUMN, NULL);
  gtk_tree_view_append_column (treeview, column);

  column = gtk_tree_view_column_new_with_attributes (_("User"), renderer,
                                                     "text", JOB_USER_COLUMN, NULL);
  gtk_tree_view_append_column (treeview, column);

  column = gtk_tree_view_column_new_with_attributes (_("Time"), renderer,
                                                     "text", JOB_CREATION_TIME_COLUMN, NULL);
  gtk_tree_view_append_column (treeview, column);

  g_signal_connect (gtk_tree_view_get_selection (treeview),
                    "changed", G_CALLBACK (job_selection_changed_cb), self);
}

enum
{
  ALLOWED_USERS_ID_COLUMN,
  ALLOWED_USERS_NAME_COLUMN,
  ALLOWED_USERS_N_COLUMNS
};

static int
ccGetAllowedUsers (gchar ***allowed_users, char *printer_name)
{
  const char * const   attrs[1] = { "requesting-user-name-allowed" };
  http_t              *http;
  ipp_t               *request = NULL;
  gchar              **users = NULL;
  ipp_t               *response;
  char                 uri[HTTP_MAX_URI + 1];
  int                  num_allowed_users = 0;

  http = httpConnectEncrypt (cupsServer (),
                             ippPort (),
                             cupsEncryption ());

  if (http || !allowed_users)
    {
      request = ippNewRequest (IPP_GET_PRINTER_ATTRIBUTES);

      g_snprintf (uri, sizeof (uri), "ipp://localhost/printers/%s", printer_name);
      ippAddString (request,
                    IPP_TAG_OPERATION,
                    IPP_TAG_URI,
                    "printer-uri",
                    NULL,
                    uri);
       ippAddStrings (request,
                      IPP_TAG_OPERATION,
                      IPP_TAG_KEYWORD,
                      "requested-attributes",
                      1,
                      NULL,
                      attrs);

       response = cupsDoRequest (http, request, "/");
       if (response)
         {
           ipp_attribute_t *attr = NULL;
           ipp_attribute_t *allowed = NULL;

           for (attr = response->attrs; attr != NULL; attr = attr->next)
             {
               if (attr->group_tag == IPP_TAG_PRINTER &&
                   attr->value_tag == IPP_TAG_NAME &&
                   !g_strcmp0(attr->name, "requesting-user-name-allowed"))
                 allowed = attr;
             }

            if (allowed && allowed->num_values > 0)
              {
                int i;

                num_allowed_users = allowed->num_values;
                users = g_new (gchar*, num_allowed_users);

                for (i = 0; i < num_allowed_users; i ++)
                  users[i] = g_strdup (allowed->values[i].string.text);
	      }
         }
     }

  *allowed_users = users;
  return num_allowed_users;
}

static void
actualize_allowed_users_list (CcPrintersPanel *self)
{
  CcPrintersPanelPrivate *priv;
  GtkListStore           *store;
  GtkTreeView            *treeview;
  GtkTreeIter             iter;
  int                     i;

  priv = PRINTERS_PANEL_PRIVATE (self);

  treeview = (GtkTreeView*)
    gtk_builder_get_object (priv->builder, "allowed-users-treeview");

  if (priv->allowed_users)
    {
      for (i = 0; i < priv->num_allowed_users; i++)
        g_free (priv->allowed_users[i]);
      g_free (priv->allowed_users);
      priv->allowed_users = NULL;
      priv->num_allowed_users = 0;
    }

  priv->current_allowed_user = -1;

  if (priv->current_dest >= 0 &&
      priv->current_dest < priv->num_dests &&
      priv->dests != NULL)
    priv->num_allowed_users = ccGetAllowedUsers (&priv->allowed_users, priv->dests[priv->current_dest].name);

  store = gtk_list_store_new (ALLOWED_USERS_N_COLUMNS, G_TYPE_INT, G_TYPE_STRING);

  for (i = 0; i < priv->num_allowed_users; i++)
    {
      gtk_list_store_append (store, &iter);
      gtk_list_store_set (store, &iter,
                          ALLOWED_USERS_ID_COLUMN, i,
                          ALLOWED_USERS_NAME_COLUMN, priv->allowed_users[i],
                          -1);
    }

  gtk_tree_view_set_model (treeview, GTK_TREE_MODEL (store));
}

static void
allowed_users_selection_changed_cb (GtkTreeSelection *selection,
                                    gpointer          user_data)
{
  CcPrintersPanelPrivate *priv;
  CcPrintersPanel        *self = (CcPrintersPanel*) user_data;
  GtkTreeModel           *model;
  GtkTreeIter             iter;
  int                     id;

  priv = PRINTERS_PANEL_PRIVATE (self);

  if (gtk_tree_selection_get_selected (selection, &model, &iter))
    gtk_tree_model_get (model, &iter,
			ALLOWED_USERS_ID_COLUMN, &id,
			-1);
  priv->current_allowed_user = id;
}

static void
populate_allowed_users_list (CcPrintersPanel *self)
{

  CcPrintersPanelPrivate *priv;
  GtkTreeViewColumn      *column;
  GtkCellRenderer        *renderer;
  GtkTreeView            *treeview;

  priv = PRINTERS_PANEL_PRIVATE (self);

  actualize_allowed_users_list (self);

  treeview = (GtkTreeView*)
    gtk_builder_get_object (priv->builder, "allowed-users-treeview");

  gtk_tree_view_set_headers_visible (treeview, FALSE);

  renderer = gtk_cell_renderer_text_new ();

  column = gtk_tree_view_column_new_with_attributes (NULL, renderer,
                                                     "text", ALLOWED_USERS_NAME_COLUMN, NULL);
  gtk_tree_view_append_column (treeview, column);

  g_signal_connect (gtk_tree_view_get_selection (treeview),
                    "changed", G_CALLBACK (allowed_users_selection_changed_cb), self);
}

static DBusGProxy *
get_dbus_proxy ()
{
  DBusGConnection *system_bus;
  DBusGProxy      *proxy;
  GError          *error;

  error = NULL;
  system_bus = dbus_g_bus_get (DBUS_BUS_SYSTEM, &error);
  if (system_bus == NULL)
    {
      g_warning (_("Could not connect to system bus: %s"),
                 error->message);
      g_error_free (error);
      return NULL;
    }

  error = NULL;

  proxy = dbus_g_proxy_new_for_name (system_bus,
                                     MECHANISM_BUS,
                                     "/",
                                     MECHANISM_BUS);
  return proxy;
}

static void
job_process_cb (GtkToolButton *toolbutton,
                gpointer       user_data)
{
  CcPrintersPanelPrivate *priv;
  CcPrintersPanel        *self = (CcPrintersPanel*) user_data;
  DBusGProxy             *proxy;
  gboolean                ret = FALSE;
  GError                 *error = NULL;
  char                   *ret_error = NULL;
  int                     id = -1;

  priv = PRINTERS_PANEL_PRIVATE (self);

  if (priv->current_job >= 0 &&
      priv->current_job < priv->num_jobs &&
      priv->jobs != NULL)
    id = priv->jobs[priv->current_job].id;

  if (id >= 0)
    {
      proxy = get_dbus_proxy ();

      if (!proxy)
        return;

      if ((GtkToolButton*) gtk_builder_get_object (priv->builder,
                                                   "job-cancel-button") ==
          toolbutton)
        ret = dbus_g_proxy_call (proxy, "JobCancelPurge", &error,
                                 G_TYPE_INT, id,
                                 G_TYPE_BOOLEAN, FALSE,
                                 G_TYPE_INVALID,
                                 G_TYPE_STRING, &ret_error,
                                 G_TYPE_INVALID);
      else if ((GtkToolButton*) gtk_builder_get_object (priv->builder,
                                                        "job-delete-button") ==
               toolbutton)
        ret = dbus_g_proxy_call (proxy, "JobCancelPurge", &error,
                                 G_TYPE_INT, id,
                                 G_TYPE_BOOLEAN, TRUE,
                                 G_TYPE_INVALID,
                                 G_TYPE_STRING, &ret_error,
                                 G_TYPE_INVALID);
      else if ((GtkToolButton*) gtk_builder_get_object (priv->builder,
                                                        "job-pause-button") ==
               toolbutton)
        ret = dbus_g_proxy_call (proxy, "JobSetHoldUntil", &error,
                                 G_TYPE_INT, id,
                                 G_TYPE_STRING, "indefinite",
                                 G_TYPE_INVALID,
                                 G_TYPE_STRING, &ret_error,
                                 G_TYPE_INVALID);
      else if ((GtkToolButton*) gtk_builder_get_object (priv->builder,
                                                        "job-release-button") ==
               toolbutton)
        ret = dbus_g_proxy_call (proxy, "JobSetHoldUntil", &error,
                                 G_TYPE_INT, id,
                                 G_TYPE_STRING, "no-hold",
                                 G_TYPE_INVALID,
                                 G_TYPE_STRING, &ret_error,
                                 G_TYPE_INVALID);

      if (error || (ret_error && ret_error[0] != '\0'))
        {
          if (error)
            g_warning ("%s", error->message);

          if (ret_error && ret_error[0] != '\0')
            g_warning ("%s", ret_error);
        }
      else
        actualize_jobs_list (self);
  }

  return;
}

static void
printer_disable_cb (GtkToggleButton *togglebutton,
                    gpointer         user_data)
{
  CcPrintersPanelPrivate *priv;
  CcPrintersPanel        *self = (CcPrintersPanel*) user_data;
  DBusGProxy             *proxy;
  gboolean                ret = FALSE;
  gboolean                paused = FALSE;
  GError                 *error = NULL;
  char                   *ret_error = NULL;
  char                   *name = NULL;
  int                     i;

  priv = PRINTERS_PANEL_PRIVATE (self);

  if (priv->current_dest >= 0 &&
      priv->current_dest < priv->num_dests &&
      priv->dests != NULL)
    {
      name = priv->dests[priv->current_dest].name;

      for (i = 0; i < priv->dests[priv->current_dest].num_options; i++)
        {
          if (g_strcmp0 (priv->dests[priv->current_dest].options[i].name, "printer-state") == 0)
            paused = (g_strcmp0 (priv->dests[priv->current_dest].options[i].value, "5") == 0);
        }
    }

  if (name)
    {
      proxy = get_dbus_proxy ();

      if (!proxy)
        return;

      ret = dbus_g_proxy_call (proxy, "PrinterSetEnabled", &error,
                               G_TYPE_STRING, name,
                               G_TYPE_BOOLEAN, paused,
                               G_TYPE_INVALID,
                               G_TYPE_STRING, &ret_error,
                               G_TYPE_INVALID);

      if (error || (ret_error && ret_error[0] != '\0'))
        {
          if (error)
            g_warning ("%s", error->message);

          if (ret_error && ret_error[0] != '\0')
            g_warning ("%s", ret_error);
        }
      else
        {
          gtk_toggle_button_set_active (togglebutton, paused);
          actualize_printers_list (self);
        }
  }

  return;
}

static void
printer_delete_cb (GtkToolButton *toolbutton,
                   gpointer       user_data)
{
  CcPrintersPanelPrivate *priv;
  CcPrintersPanel        *self = (CcPrintersPanel*) user_data;
  DBusGProxy             *proxy;
  gboolean                ret = FALSE;
  GError                 *error = NULL;
  char                   *ret_error = NULL;
  char                   *name = NULL;

  priv = PRINTERS_PANEL_PRIVATE (self);

  if (priv->current_dest >= 0 &&
      priv->current_dest < priv->num_dests &&
      priv->dests != NULL)
    name = priv->dests[priv->current_dest].name;

  if (name)
    {
      proxy = get_dbus_proxy ();

      if (!proxy)
        return;

      ret = dbus_g_proxy_call (proxy, "PrinterDelete", &error,
                               G_TYPE_STRING, name,
                               G_TYPE_INVALID,
                               G_TYPE_STRING, &ret_error,
                               G_TYPE_INVALID);

      if (error || (ret_error && ret_error[0] != '\0'))
        {
          if (error)
            g_warning ("%s", error->message);

          if (ret_error && ret_error[0] != '\0')
            g_warning ("%s", ret_error);
        }
      else
        actualize_printers_list (self);
  }

  return;
}

static void
allowed_user_remove_cb (GtkToolButton *toolbutton,
                        gpointer       user_data)
{
  CcPrintersPanelPrivate *priv;
  CcPrintersPanel        *self = (CcPrintersPanel*) user_data;
  DBusGProxy             *proxy;
  gboolean                ret = FALSE;
  GError                 *error = NULL;
  char                   *ret_error = NULL;
  char                   *printer_name = NULL;
  char                   **names = NULL;
  char                   *name = NULL;
  int                     i, j;

  priv = PRINTERS_PANEL_PRIVATE (self);

  if (priv->current_allowed_user >= 0 &&
      priv->current_allowed_user < priv->num_allowed_users &&
      priv->allowed_users != NULL)
    name = priv->allowed_users[priv->current_allowed_user];

  if (priv->current_dest >= 0 &&
      priv->current_dest < priv->num_dests &&
      priv->dests != NULL)
    printer_name = priv->dests[priv->current_dest].name;

  if (name && printer_name)
    {
      proxy = get_dbus_proxy ();

      if (!proxy)
        return;

      names = g_new0 (gchar*, priv->num_allowed_users);
      j = 0;
      for (i = 0; i < (priv->num_allowed_users); i++)
        {
          if (i != priv->current_allowed_user)
            {
              names[j] = priv->allowed_users[i];
              j++;
            }
        }

      ret = dbus_g_proxy_call (proxy, "PrinterSetUsersAllowed", &error,
                               G_TYPE_STRING, printer_name,
                               G_TYPE_STRV, names,
                               G_TYPE_INVALID,
                               G_TYPE_STRING, &ret_error,
                               G_TYPE_INVALID);

      if (error || (ret_error && ret_error[0] != '\0'))
        {
          if (error)
            g_warning ("%s", error->message);

          if (ret_error && ret_error[0] != '\0')
            g_warning ("%s", ret_error);
        }
      else
        actualize_allowed_users_list (self);
  }

  return;
}

static void
cc_printers_panel_init (CcPrintersPanel *self)
{
  CcPrintersPanelPrivate *priv;
  GtkWidget              *top_widget;
  GtkWidget              *widget;
  GError                 *error = NULL;
  gchar                  *objects[] = { "main-vbox", NULL };

  priv = self->priv = PRINTERS_PANEL_PRIVATE (self);

  /* initialize main data structure */
  priv->builder = gtk_builder_new ();
  priv->dests = NULL;
  priv->num_dests = 0;
  priv->current_dest = -1;

  priv->jobs = NULL;
  priv->num_jobs = 0;
  priv->current_job = -1;

  priv->allowed_users = NULL;
  priv->num_allowed_users = 0;
  priv->current_allowed_user = -1;

  gtk_builder_add_objects_from_file (priv->builder,
                                     DATADIR"/printers.ui",
                                     objects, &error);

  if (error)
    {
      g_warning (_("Could not load ui: %s"), error->message);
      g_error_free (error);
      return;
    }

  /* add the top level widget */
  top_widget = (GtkWidget*)
    gtk_builder_get_object (priv->builder, "main-vbox");

  /* connect signals */
  widget = (GtkWidget*)
    gtk_builder_get_object (priv->builder, "job-cancel-button");
  g_signal_connect (widget, "clicked", G_CALLBACK (job_process_cb), self);

  widget = (GtkWidget*)
    gtk_builder_get_object (priv->builder, "job-delete-button");
  g_signal_connect (widget, "clicked", G_CALLBACK (job_process_cb), self);

  widget = (GtkWidget*)
    gtk_builder_get_object (priv->builder, "job-pause-button");
  g_signal_connect (widget, "clicked", G_CALLBACK (job_process_cb), self);

  widget = (GtkWidget*)
    gtk_builder_get_object (priv->builder, "job-release-button");
  g_signal_connect (widget, "clicked", G_CALLBACK (job_process_cb), self);

  widget = (GtkWidget*)
    gtk_builder_get_object (priv->builder, "printer-disable-button");
  g_signal_connect (widget, "toggled", G_CALLBACK (printer_disable_cb), self);

  widget = (GtkWidget*)
    gtk_builder_get_object (priv->builder, "printer-delete-button");
  g_signal_connect (widget, "clicked", G_CALLBACK (printer_delete_cb), self);

  widget = (GtkWidget*)
    gtk_builder_get_object (priv->builder, "allowed-user-remove-button");
  g_signal_connect (widget, "clicked", G_CALLBACK (allowed_user_remove_cb), self);


  /* make unused widgets insensitive for now */
  widget = (GtkWidget*)
    gtk_builder_get_object (priv->builder, "allowed-user-add-button");
  gtk_widget_set_sensitive (widget, FALSE);

  widget = (GtkWidget*)
    gtk_builder_get_object (priv->builder, "printer-add-button");
  gtk_widget_set_sensitive (widget, FALSE);

  widget = (GtkWidget*)
    gtk_builder_get_object (priv->builder, "print-test-page-button");
  gtk_widget_set_sensitive (widget, FALSE);

  widget = (GtkWidget*)
    gtk_builder_get_object (priv->builder, "clean-print-heads-button");
  gtk_widget_set_sensitive (widget, FALSE);

  populate_printers_list (self);
  populate_jobs_list (self);
  populate_allowed_users_list (self);

  gtk_container_add (GTK_CONTAINER (self), top_widget);
  gtk_widget_show_all (GTK_WIDGET (self));
}

void
cc_printers_panel_register (GIOModule *module)
{
  cc_printers_panel_register_type (G_TYPE_MODULE (module));
  g_io_extension_point_implement (CC_SHELL_PANEL_EXTENSION_POINT,
                                  CC_TYPE_PRINTERS_PANEL,
                                  "printers", 0);
}

