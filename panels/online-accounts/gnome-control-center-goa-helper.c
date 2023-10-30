/*
 * Copyright (C) 2022 Endless OS Foundation, LLC
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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * Author:
 *   Georges Basile Stavracas Neto <georges.stavracas@gmail.com>
 */

#include "config.h"

#include <gtk/gtk.h>
#include <glib/gi18n.h>

#define GOA_API_IS_SUBJECT_TO_CHANGE
#define GOA_BACKEND_API_IS_SUBJECT_TO_CHANGE
#include <goabackend/goabackend.h>

#ifdef HAVE_GTK_X11
#include <gdk/gdkx.h>
#endif
#ifdef HAVE_GTK_WAYLAND
#include <gdk/gdkwayland.h>
#endif

static GdkDisplay *
get_wayland_display (void)
{
  static GdkDisplay *wayland_display = NULL;

  if (wayland_display)
    return wayland_display;

  gdk_set_allowed_backends ("wayland");
  wayland_display = gdk_display_open (NULL);
  gdk_set_allowed_backends (NULL);
  if (!wayland_display)
    g_warning ("Failed to open Wayland display");

  return wayland_display;
}

static GdkDisplay *
get_x11_display (void)
{
  static GdkDisplay *x11_display = NULL;

  if (x11_display)
    return x11_display;

  gdk_set_allowed_backends ("x11");
  x11_display = gdk_display_open (NULL);
  gdk_set_allowed_backends (NULL);
  if (!x11_display)
    g_warning ("Failed to open X11 display");

  return x11_display;
}

static void
set_external_parent_from_handle (GtkApplication *application,
                                 GtkWindow      *dialog,
                                 const char     *handle_str)
{
  GdkDisplay *display;
  GtkWindow *fake_parent;
  GdkScreen *screen;

#ifdef HAVE_GTK_X11
    {
      const char *x11_prefix = "x11:";
      if (g_str_has_prefix (handle_str, x11_prefix))
        {
          display = get_x11_display ();
          if (!display)
            {
              g_warning ("No X display connection, ignoring X11 parent");
              return;
            }
        }
    }
#endif
#ifdef HAVE_GTK_WAYLAND
    {
      const char *wayland_prefix = "wayland:";

      if (g_str_has_prefix (handle_str, wayland_prefix))
        {
          display = get_wayland_display ();
          if (!display)
            {
              g_warning ("No Wayland display connection, ignoring Wayland parent");
              return;
            }
        }
    }
#endif

  screen = gdk_display_get_default_screen (gdk_display_get_default ());
  fake_parent = g_object_new (GTK_TYPE_APPLICATION_WINDOW,
                              "application", application,
                              "type", GTK_WINDOW_TOPLEVEL,
                              "screen", screen,
                              NULL);
  g_object_ref_sink (fake_parent);

  gtk_window_set_transient_for (dialog, GTK_WINDOW (fake_parent));
  gtk_window_set_modal (dialog, TRUE);
  gtk_widget_realize (GTK_WIDGET (dialog));

#ifdef HAVE_GTK_X11
    {
      const char *x11_prefix = "x11:";
      if (g_str_has_prefix (handle_str, x11_prefix))
        {
          GdkWindow *foreign_gdk_window;
          int xid;

          errno = 0;
          xid = strtol (handle_str + strlen (x11_prefix), NULL, 16);
          if (errno != 0)
            {
              g_warning ("Failed to reference external X11 window, invalid XID %s", handle_str);
              return;
            }

          foreign_gdk_window = gdk_x11_window_foreign_new_for_display (display, xid);
          if (!foreign_gdk_window)
            {
              g_warning ("Failed to create foreign window for XID %d", xid);
              return;
            }

          gdk_window_set_transient_for (gtk_widget_get_window (GTK_WIDGET (dialog)),
                                        foreign_gdk_window);
        }
    }
#endif
#ifdef HAVE_GTK_WAYLAND
    {
      const char *wayland_prefix = "wayland:";

      if (g_str_has_prefix (handle_str, wayland_prefix))
        {
          const char *wayland_handle_str = handle_str + strlen (wayland_prefix);

          if (!gdk_wayland_window_set_transient_for_exported (gtk_widget_get_window (GTK_WIDGET (dialog)),
                                                              (char *) wayland_handle_str))
            {
              g_warning ("Failed to set window transient for external parent");
              return;
            }
        }
    }
#endif

  gtk_window_present (dialog);
}

/* create-account */

static void
on_application_activate_create_account_cb (GtkApplication  *application,
                                           char           **argv)
{
  g_autoptr(GoaProvider) provider = NULL;
  g_autoptr(GoaClient) client = NULL;
  g_autoptr(GError) error = NULL;
  GoaAccount *account;
  GtkWidget *content_area;
  GtkWidget *dialog;
  GoaObject *object;

  client = goa_client_new_sync (NULL, &error);
  if (error)
    {
      g_printerr ("Error retrieving online accounts client");
      exit (EXIT_FAILURE);
      return;
    }


  /* Find the provider with a matching type */
  provider = goa_provider_get_for_provider_type (argv[2]);
  if (!provider)
    {
      g_printerr ("Provider type not supported");
      exit (EXIT_FAILURE);
      return;
    }

  dialog = g_object_new (GTK_TYPE_DIALOG,
                         "use-header-bar", 1,
                         "default-width", 500,
                         "default-height", 350,
                         NULL);
  g_signal_connect_swapped (dialog, "response", G_CALLBACK (g_application_quit), application);
  set_external_parent_from_handle (application, GTK_WINDOW (dialog), argv[3]);

  content_area = gtk_dialog_get_content_area (GTK_DIALOG (dialog));
  gtk_container_set_border_width (GTK_CONTAINER (content_area), 0);

  object = goa_provider_add_account (provider,
                                     client,
                                     GTK_DIALOG (dialog),
                                     GTK_BOX (content_area),
                                     &error);
  if (error)
    {
      g_printerr ("Failed to create account: %s", error->message);
      exit (EXIT_FAILURE);
      return;
    }

  account = goa_object_peek_account (object);
  g_print ("%s", goa_account_get_id (account));
}

static int
create_account (int    argc,
                char **argv)
{
  g_autoptr(GtkApplication) application = NULL;

  gtk_init (&argc, &argv);

  if (argc != 4)
    {
      g_printerr ("Not enough arguments");
      return EXIT_FAILURE;
    }

  application = gtk_application_new ("org.gnome.Settings.GoaHelper",
                                     G_APPLICATION_FLAGS_NONE);
  g_signal_connect (application, "activate", G_CALLBACK (on_application_activate_create_account_cb), argv);

  return g_application_run (G_APPLICATION (application), 0, NULL);
}

/* list-providers */

typedef struct {
  GMainLoop *mainloop;
  GList *providers;
  GError *error;
} GetAllProvidersData;

static void
get_all_providers_cb (GObject      *source,
                      GAsyncResult *res,
                      gpointer      user_data)
{
  g_autolist(GoaProvider) providers = NULL;
  GetAllProvidersData *data;

  data = user_data;

  goa_provider_get_all_finish (&providers, res, &data->error);
  if (data->error)
    goto out;

  data->providers = g_steal_pointer (&providers);

out:
  g_main_loop_quit (data->mainloop);
}

static GList *
get_all_providers (GError **error)
{
  GetAllProvidersData data = (GetAllProvidersData) {
    .mainloop = g_main_loop_new (NULL, FALSE),
    .providers = NULL,
    .error = NULL,
  };

  goa_provider_get_all (get_all_providers_cb, &data);

  g_main_loop_run (data.mainloop);
  g_main_loop_unref (data.mainloop);

  if (data.error)
    g_propagate_error (error, data.error);

  return data.providers;
}

static int
list_providers (int    argc,
                char **argv)
{
  g_autofree char *serialized_result = NULL;
  g_autolist(GoaProvider) providers = NULL;
  g_autoptr(GVariant) result = NULL;
  g_autoptr(GError) error = NULL;
  GVariantBuilder b;
  GList *l;

  providers = get_all_providers (&error);

  if (error)
    {
      g_printerr ("%s", error->message);
      return EXIT_FAILURE;
    }

  g_variant_builder_init (&b, G_VARIANT_TYPE ("a(ssviu)"));
  for (l = providers; l; l = l->next)
    {
      GoaProvider *provider = l->data;
      g_autofree char *name = NULL;
      g_autoptr(GVariant) icon_variant = NULL;
      g_autoptr(GIcon) icon = NULL;

      name = goa_provider_get_provider_name (provider, NULL);
      icon = goa_provider_get_provider_icon (provider, NULL);
      icon_variant = g_icon_serialize (icon);

      g_variant_builder_add (&b, "(ssviu)",
                             goa_provider_get_provider_type (provider),
                             name,
                             icon_variant,
                             goa_provider_get_provider_features (provider),
                             goa_provider_get_credentials_generation (provider));
    }
  result = g_variant_builder_end (&b);

  serialized_result = g_variant_print (result, TRUE);
  g_print ("%s", serialized_result);

  return EXIT_SUCCESS;
}

/* show-account */

static void
on_remove_button_clicked_cb (GApplication *application)
{
  g_print ("remove");
  g_application_quit (application);
}

static void
on_application_activate_show_account_cb (GtkApplication  *application,
                                         char           **argv)
{
  g_autoptr(GoaProvider) provider = NULL;
  g_autoptr(GoaObject) object = NULL;
  g_autoptr(GoaClient) client = NULL;
  g_autoptr(GError) error = NULL;
  g_autofree char *title = NULL;
  GoaAccount *account;
  GtkWidget *content_area;
  GtkWidget *button;
  GtkWidget *dialog;
  GtkWidget *box;
  const char *provider_type;

  client = goa_client_new_sync (NULL, &error);
  if (error)
    {
      g_printerr ("Error retrieving online accounts client");
      exit (EXIT_FAILURE);
      return;
    }

  object = goa_client_lookup_by_id (client, argv[2]);
  if (!object)
    {
      g_printerr ("Online account does not exist");
      exit (EXIT_FAILURE);
      return;
    }

  /* Find the provider with a matching type */
  account = goa_object_get_account (object);
  provider_type = goa_account_get_provider_type (account);
  provider = goa_provider_get_for_provider_type (provider_type);
  if (!provider)
    {
      g_printerr ("Provider type not supported");
      exit (EXIT_FAILURE);
      return;
    }

  dialog = g_object_new (GTK_TYPE_DIALOG,
                         "use-header-bar", 1,
                         NULL);
  /* Keep account alive so that the switches are still bound to it */
  g_object_set_data_full (G_OBJECT (dialog), "goa-account", account, g_object_unref);
  g_signal_connect_swapped (dialog, "response", G_CALLBACK (g_application_quit), application);
  set_external_parent_from_handle (application, GTK_WINDOW (dialog), argv[3]);

  box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 42);
  gtk_widget_set_margin_bottom (box, 24);

  content_area = gtk_dialog_get_content_area (GTK_DIALOG (dialog));
  gtk_container_set_border_width (GTK_CONTAINER (content_area), 0);
  gtk_container_add (GTK_CONTAINER (content_area), box);

  goa_provider_show_account (provider,
                             client,
                             object,
                             GTK_BOX (box),
                             NULL,
                             NULL);

  /*
   * The above call doesn't set any widgets to visible, so we have to do that.
   * https://gitlab.gnome.org/GNOME/gnome-online-accounts/issues/56
   */
  gtk_widget_show_all (box);

  /* translators: This is the title of the "Show Account" dialog. The
   * %s is the name of the provider. e.g., 'Google'. */
  title = g_strdup_printf (_("%s Account"), goa_account_get_provider_name (account));
  gtk_window_set_title (GTK_WINDOW (dialog), title);

  button = gtk_button_new_with_label (_("Remove Account"));
  gtk_widget_set_margin_start (box, 24);
  gtk_widget_set_margin_end (box, 24);
  gtk_widget_set_halign (button, GTK_ALIGN_END);
  gtk_widget_set_valign (button, GTK_ALIGN_END);
  gtk_widget_set_visible (button, !goa_account_get_is_locked (account));
  gtk_style_context_add_class (gtk_widget_get_style_context (button), "destructive-action");
  gtk_container_add (GTK_CONTAINER (box), button);
  g_signal_connect_swapped (button, "clicked", G_CALLBACK (on_remove_button_clicked_cb), application);
}

static int
show_account (int    argc,
              char **argv)
{
  g_autoptr(GtkApplication) application = NULL;

  gtk_init (&argc, &argv);

  if (argc != 4)
    {
      g_printerr ("Not enough arguments");
      return EXIT_FAILURE;
    }

  application = gtk_application_new ("org.gnome.Settings.GoaHelper",
                                     G_APPLICATION_FLAGS_NONE);
  g_signal_connect (application, "activate", G_CALLBACK (on_application_activate_show_account_cb), argv);

  return g_application_run (G_APPLICATION (application), 0, NULL);
}

struct {
  const char *command_name;
  int (*command_func) (int    argc,
                       char **argv);
} commands[] = {
  { "create-account", create_account, },
  { "list-providers", list_providers, },
  { "show-account", show_account, },
};


static void
log_handler (const gchar    *domain,
             GLogLevelFlags  log_level,
             const gchar    *message,
             gpointer        user_data)
{
  g_printerr ("%s: %s\n", domain, message);
}

int
main (int    argc,
      char **argv)
{
  gsize i;

  bindtextdomain (GETTEXT_PACKAGE, GNOMELOCALEDIR);
  bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
  textdomain (GETTEXT_PACKAGE);

  if (argc < 2)
    return EXIT_FAILURE;

  /*
  * This helper currently communicates to the gnome-control-center parent process
  * by writing information to stdout using g_print. Therefore we need
  * a custom logging handler, so to not write logs into stdout,
  * which would confuse the parent process.
  */
  g_log_set_default_handler (log_handler, NULL);

  for (i = 0; i < G_N_ELEMENTS (commands); i++)
    {
      if (g_strcmp0 (commands[i].command_name, argv[1]) == 0)
        return commands[i].command_func (argc, argv);
    }

  return EXIT_SUCCESS;
}
