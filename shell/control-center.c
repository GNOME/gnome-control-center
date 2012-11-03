/*
 * Copyright (c) 2009, 2010 Intel, Inc.
 * Copyright (c) 2010 Red Hat, Inc.
 *
 * The Control Center is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 *
 * The Control Center is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with the Control Center; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 * Author: Thomas Wood <thos@gnome.org>
 */

#include "config.h"

#include <glib/gi18n.h>
#include <stdlib.h>

#include "gnome-control-center.h"

#include <gtk/gtk.h>
#include <string.h>
#include <libnotify/notify.h>

#ifdef GDK_WINDOWING_X11
#include <X11/Xlib.h>
#endif

#include "cc-shell-log.h"

G_GNUC_NORETURN static gboolean
option_version_cb (const gchar *option_name,
                   const gchar *value,
                   gpointer     data,
                   GError     **error)
{
  g_print ("%s %s\n", PACKAGE, VERSION);
  exit (0);
}

static char **start_panels = NULL;
static gboolean show_overview = FALSE;
static gboolean verbose = FALSE;
static gboolean show_help = FALSE;
static gboolean show_help_gtk = FALSE;
static gboolean show_help_all = FALSE;

const GOptionEntry all_options[] = {
  { "version", 0, G_OPTION_FLAG_NO_ARG, G_OPTION_ARG_CALLBACK, option_version_cb, NULL, NULL },
  { "verbose", 'v', 0, G_OPTION_ARG_NONE, &verbose, N_("Enable verbose mode"), NULL },
  { "overview", 'o', 0, G_OPTION_ARG_NONE, &show_overview, N_("Show the overview"), NULL },
  { "help", 'h', G_OPTION_FLAG_HIDDEN, G_OPTION_ARG_NONE, &show_help, N_("Show help options"), NULL },
  { "help-all", 0, G_OPTION_FLAG_HIDDEN, G_OPTION_ARG_NONE, &show_help_all, N_("Show help options"), NULL },
  { "help-gtk", 0, G_OPTION_FLAG_HIDDEN, G_OPTION_ARG_NONE, &show_help_gtk, N_("Show help options"), NULL },
  { G_OPTION_REMAINING, '\0', 0, G_OPTION_ARG_FILENAME_ARRAY, &start_panels, N_("Panel to display"), NULL },
  { NULL, 0, 0, 0, NULL, NULL, NULL } /* end the list */
};

static int
application_command_line_cb (GApplication  *application,
                             GApplicationCommandLine  *command_line,
                             GnomeControlCenter      *shell)
{
  int argc;
  char **argv;
  int retval = 0;
  GOptionContext *context;
  GError *error = NULL;

  verbose = FALSE;
  show_overview = FALSE;
  show_help = FALSE;
  start_panels = NULL;

  argv = g_application_command_line_get_arguments (command_line, &argc);

  context = g_option_context_new (N_("- System Settings"));
  g_option_context_add_main_entries (context, all_options, GETTEXT_PACKAGE);
  g_option_context_set_translation_domain(context, GETTEXT_PACKAGE);
  g_option_context_add_group (context, gtk_get_option_group (TRUE));
  g_option_context_set_help_enabled (context, FALSE);

  if (g_option_context_parse (context, &argc, &argv, &error) == FALSE)
    {
      g_print (_("%s\nRun '%s --help' to see a full list of available command line options.\n"),
               error->message, argv[0]);
      g_error_free (error);
      g_option_context_free (context);
      return 1;
    }

  if (show_help || show_help_all || show_help_gtk)
    {
      gchar *help;
      GOptionGroup *group;

      if (show_help || show_help_all)
        group = NULL;
      else
        group = gtk_get_option_group (FALSE);

      help = g_option_context_get_help (context, FALSE, group);
      g_print ("%s", help);
      g_free (help);
      g_option_context_free (context);
      return 0;
    }

  g_option_context_free (context);

  cc_shell_log_set_debug (verbose);

  gnome_control_center_show (shell, GTK_APPLICATION (application));

  if (show_overview)
    {
      gnome_control_center_set_overview_page (shell);
    }
  else if (start_panels != NULL && start_panels[0] != NULL)
    {
      const char *start_id;
      GError *err = NULL;

      start_id = start_panels[0];

      if (start_panels[1])
	g_debug ("Extra argument: %s", start_panels[1]);
      else
	g_debug ("No extra argument");

      if (!cc_shell_set_active_panel_from_id (CC_SHELL (shell), start_id, (const gchar**)start_panels+1, &err))
        {
          g_warning ("Could not load setting panel \"%s\": %s", start_id,
                     (err) ? err->message : "Unknown error");
          retval = 1;
          if (err)
            {
              g_error_free (err);
              err = NULL;
            }
        }
    }

  gnome_control_center_present (shell);
  gdk_notify_startup_complete ();

  g_strfreev (argv);
  if (start_panels != NULL)
    {
      g_strfreev (start_panels);
      start_panels = NULL;
    }
  show_overview = FALSE;

  return retval;
}

static void
help_activated (GSimpleAction *action,
                GVariant      *parameter,
                gpointer       user_data)
{
  GnomeControlCenter *shell = user_data;
  CcPanel *panel = cc_shell_get_active_panel (CC_SHELL (shell));
  GtkWidget *window = cc_shell_get_toplevel (CC_SHELL (shell));
  const char *uri = NULL;

  if (panel)
    uri = cc_panel_get_help_uri (panel);

  gtk_show_uri (gtk_widget_get_screen (window),
                uri ? uri : "help:gnome-help/prefs",
                GDK_CURRENT_TIME, NULL);
}

static void
quit_activated (GSimpleAction *action,
                GVariant      *parameter,
                gpointer       user_data)
{
  GnomeControlCenter *shell = user_data;
  g_object_unref (shell);
}

static void
application_startup_cb (GApplication       *application,
                        GnomeControlCenter *shell)
{
  GMenu *menu, *section;
  GAction *action;

  action = G_ACTION (g_simple_action_new ("help", NULL));
  g_action_map_add_action (G_ACTION_MAP (application), action);
  g_signal_connect (action, "activate", G_CALLBACK (help_activated), shell);

  action = G_ACTION (g_simple_action_new ("quit", NULL));
  g_action_map_add_action (G_ACTION_MAP (application), action);
  g_signal_connect (action, "activate", G_CALLBACK (quit_activated), shell);

  menu = g_menu_new ();

  section = g_menu_new ();
  g_menu_append (section, _("Help"), "app.help");
  g_menu_append (section, _("Quit"), "app.quit");

  g_menu_append_section (menu, NULL, G_MENU_MODEL (section));

  gtk_application_set_app_menu (GTK_APPLICATION (application),
                                G_MENU_MODEL (menu));

  gtk_application_add_accelerator (GTK_APPLICATION (application),
                                   "F1", "app.help", NULL);

  /* nothing else to do here, we don't want to show a window before
   * we've looked at the commandline
   */
}

int
main (int argc, char **argv)
{
  GnomeControlCenter *shell;
  GtkApplication *application;
  int status;

  bindtextdomain (GETTEXT_PACKAGE, GNOMELOCALEDIR);
  bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
  textdomain (GETTEXT_PACKAGE);

#ifdef GDK_WINDOWING_X11
  XInitThreads ();
#endif

  gtk_init (&argc, &argv);
  cc_shell_log_init ();

  /* register a symbolic icon size for use in sidebar lists */
  gtk_icon_size_register ("cc-sidebar-list", 24, 24);

  notify_init ("gnome-control-center");

  shell = gnome_control_center_new ();

  /* enforce single instance of this application */
  application = gtk_application_new ("org.gnome.ControlCenter", G_APPLICATION_HANDLES_COMMAND_LINE);
  g_signal_connect (application, "startup",
                    G_CALLBACK (application_startup_cb), shell);
  g_signal_connect (application, "command-line",
                    G_CALLBACK (application_command_line_cb), shell);

  status = g_application_run (G_APPLICATION (application), argc, argv);

  g_object_unref (application);

  return status;
}
