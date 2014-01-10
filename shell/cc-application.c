/*
 * Copyright © 2013 Red Hat, Inc.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301, USA.
 */

#include "config.h"

#include <stdlib.h>

#include <glib.h>
#include <glib/gi18n.h>
#include <gio/gio.h>

#include "cc-application.h"
#include "cc-panel-loader.h"
#include "cc-shell-log.h"
#include "cc-window.h"

#if defined(HAVE_CHEESE) || defined(HAVE_WACOM)
#include <clutter-gtk/clutter-gtk.h>
#endif /* HAVE_CHEESE || HAVE_WACOM */

#ifdef HAVE_CHEESE
#include <cheese-gtk.h>
#endif /* HAVE_CHEESE */

struct _CcApplicationPrivate
{
  CcWindow *window;
};

G_DEFINE_TYPE (CcApplication, cc_application, GTK_TYPE_APPLICATION)

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
static char *search_str = NULL;
static gboolean show_overview = FALSE;
static gboolean verbose = FALSE;
static gboolean show_help = FALSE;
static gboolean show_help_gtk = FALSE;
static gboolean show_help_all = FALSE;
static gboolean list_panels = FALSE;

const GOptionEntry all_options[] = {
  { "version", 0, G_OPTION_FLAG_NO_ARG, G_OPTION_ARG_CALLBACK, option_version_cb, NULL, NULL },
  { "verbose", 'v', 0, G_OPTION_ARG_NONE, &verbose, N_("Enable verbose mode"), NULL },
  { "overview", 'o', 0, G_OPTION_ARG_NONE, &show_overview, N_("Show the overview"), NULL },
  { "search", 's', 0, G_OPTION_ARG_STRING, &search_str, N_("Search for the string"), "SEARCH" },
  { "list", 'l', 0, G_OPTION_ARG_NONE, &list_panels, N_("List possible panel names and exit"), NULL },
  { "help", 'h', G_OPTION_FLAG_HIDDEN, G_OPTION_ARG_NONE, &show_help, N_("Show help options"), NULL },
  { "help-all", 0, G_OPTION_FLAG_HIDDEN, G_OPTION_ARG_NONE, &show_help_all, N_("Show help options"), NULL },
  { "help-gtk", 0, G_OPTION_FLAG_HIDDEN, G_OPTION_ARG_NONE, &show_help_gtk, N_("Show help options"), NULL },
  { G_OPTION_REMAINING, '\0', 0, G_OPTION_ARG_FILENAME_ARRAY, &start_panels, N_("Panel to display"), N_("[PANEL] [ARGUMENT…]") },
  { NULL, 0, 0, 0, NULL, NULL, NULL } /* end the list */
};

static void
help_activated (GSimpleAction *action,
                GVariant      *parameter,
                gpointer       user_data)
{
  CcApplication *self = CC_APPLICATION (user_data);
  CcPanel *panel;
  GtkWidget *window;
  const char *uri = NULL;

  panel = cc_shell_get_active_panel (CC_SHELL (self->priv->window));
  if (panel)
    uri = cc_panel_get_help_uri (panel);

  window = cc_shell_get_toplevel (CC_SHELL (self->priv->window));
  gtk_show_uri (gtk_widget_get_screen (window),
                uri ? uri : "help:gnome-help/prefs",
                GDK_CURRENT_TIME, NULL);
}

static void
launch_panel_activated (GSimpleAction *action,
                        GVariant      *parameter,
                        gpointer       user_data)
{
  CcApplication *self = CC_APPLICATION (user_data);
  GError *error = NULL;
  gchar *panel_id;
  GVariant *parameters;

  g_variant_get (parameter, "(&s@av)", &panel_id, &parameters);
  g_debug ("gnome-control-center: 'launch-panel' activated for panel '%s' with %"G_GSIZE_FORMAT" arguments",
      panel_id, g_variant_n_children (parameters));
  if (!cc_shell_set_active_panel_from_id (CC_SHELL (self->priv->window), panel_id, parameters, &error))
    {
      g_warning ("Failed to activate the '%s' panel: %s", panel_id, error->message);
      g_error_free (error);
    }
  g_variant_unref (parameters);

  /* Now present the window */
  g_application_activate (G_APPLICATION (self));
}

static int
cc_application_command_line (GApplication *application,
                             GApplicationCommandLine *command_line)
{
  CcApplication *self = CC_APPLICATION (application);
  int argc;
  char **argv;
  int retval = 0;
  GOptionContext *context;
  GError *error = NULL;
  GVariantBuilder *flags_builder;

  verbose = FALSE;
  show_overview = FALSE;
  show_help = FALSE;
  start_panels = NULL;

  flags_builder = g_variant_builder_new (G_VARIANT_TYPE_VARDICT);

  argv = g_application_command_line_get_arguments (command_line, &argc);

  context = g_option_context_new (N_("- Settings"));
  g_option_context_add_main_entries (context, all_options, GETTEXT_PACKAGE);
  g_option_context_set_translation_domain(context, GETTEXT_PACKAGE);
  g_option_context_add_group (context, gtk_get_option_group (TRUE));
  cc_panel_loader_add_option_groups (context, flags_builder);
  g_option_context_set_help_enabled (context, FALSE);

  start_panels = NULL;
  search_str = NULL;
  show_overview = FALSE;
  verbose = FALSE;
  show_help = FALSE;
  show_help_gtk = FALSE;
  show_help_all = FALSE;
  list_panels = FALSE;

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

  if (list_panels)
    {
      GList *panels, *l;

      panels = cc_panel_loader_get_panels ();

      g_print ("%s\n", _("Available panels:"));
      for (l = panels; l != NULL; l = l->next)
        g_print ("\t%s\n", (char *) l->data);

      g_list_free (panels);

      return 0;
    }

#ifdef HAVE_CHEESE
  cheese_gtk_init (&argc, &argv);
#endif /* HAVE_CHEESE */

  cc_shell_log_set_debug (verbose);

  cc_window_show (self->priv->window);

  if (search_str)
    {
      cc_window_set_search_item (self->priv->window, search_str);
    }
  else if (show_overview)
    {
      cc_window_set_overview_page (self->priv->window);
    }
  else if (start_panels != NULL && start_panels[0] != NULL)
    {
      const char *start_id;
      GError *err = NULL;
      GVariant *parameters;
      GVariantBuilder *builder;
      int i;

      start_id = start_panels[0];

      if (start_panels[1])
        g_debug ("Extra argument: %s", start_panels[1]);
      else
        g_debug ("No extra argument");

      builder = g_variant_builder_new (G_VARIANT_TYPE ("av"));
      g_variant_builder_add (builder, "v", g_variant_builder_end (flags_builder));

      for (i = 1; start_panels[i] != NULL; i++)
        g_variant_builder_add (builder, "v", g_variant_new_string (start_panels[i]));
      parameters = g_variant_builder_end (builder);
      if (!cc_shell_set_active_panel_from_id (CC_SHELL (self->priv->window), start_id, parameters, &err))
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

  if (start_panels != NULL)
    {
      g_strfreev (start_panels);
      start_panels = NULL;
    }

  show_overview = FALSE;

  g_option_context_free (context);
  g_strfreev (argv);

  return retval;
}

static void
cc_application_quit (GSimpleAction *simple,
                     GVariant *parameter,
                     gpointer user_data)
{
  CcApplication *self = CC_APPLICATION (user_data);

  gtk_widget_destroy (GTK_WIDGET (self->priv->window));
}


static void
cc_application_activate (GApplication *application)
{
  CcApplication *self = CC_APPLICATION (application);

  cc_window_present (self->priv->window);
}

static void
cc_application_startup (GApplication *application)
{
  CcApplication *self = CC_APPLICATION (application);
  GMenu *menu;
  GMenu *section;
  GSimpleAction *action;

  G_APPLICATION_CLASS (cc_application_parent_class)->startup (application);

#if defined(HAVE_CHEESE) || defined(HAVE_WACOM)
  if (gtk_clutter_init (NULL, NULL) != CLUTTER_INIT_SUCCESS)
    {
      g_critical ("Unable to initialize Clutter");
      return;
    }
#endif /* HAVE_CHEESE || HAVE_WACOM */

  /* register a symbolic icon size for use in sidebar lists */
  gtk_icon_size_register ("cc-sidebar-list", 24, 24);

  action = g_simple_action_new ("help", NULL);
  g_action_map_add_action (G_ACTION_MAP (application), G_ACTION (action));
  g_signal_connect (action, "activate", G_CALLBACK (help_activated), self);
  g_object_unref (action);

  action = g_simple_action_new ("quit", NULL);
  g_action_map_add_action (G_ACTION_MAP (application), G_ACTION (action));
  g_signal_connect (action, "activate", G_CALLBACK (cc_application_quit), self);
  g_object_unref (action);

  /* Launch panel by id. The parameter is a (panel_id, array_of_panel_parameters)
   * tuple. The GVariant-containing array usually is just the same array of
   * strings that would be generated by passing panel-specific arguments on
   * the command line. */
  action = g_simple_action_new ("launch-panel", G_VARIANT_TYPE ("(sav)"));
  g_action_map_add_action (G_ACTION_MAP (application), G_ACTION (action));
  g_signal_connect (action, "activate", G_CALLBACK (launch_panel_activated), self);
  g_object_unref (action);

  menu = g_menu_new ();

  section = g_menu_new ();
  g_menu_append (section, _("Help"), "app.help");
  g_menu_append (section, _("Quit"), "app.quit");

  g_menu_append_section (menu, NULL, G_MENU_MODEL (section));

  gtk_application_set_app_menu (GTK_APPLICATION (application),
                                G_MENU_MODEL (menu));

  gtk_application_add_accelerator (GTK_APPLICATION (application),
                                   "F1", "app.help", NULL);

  self->priv->window = cc_window_new (GTK_APPLICATION (application));
}

static GObject *
cc_application_constructor (GType type,
                            guint n_construct_params,
                            GObjectConstructParam *construct_params)
{
  static GObject *self = NULL;

  if (self == NULL)
    {
      self = G_OBJECT_CLASS (cc_application_parent_class)->constructor (type,
                                                                        n_construct_params,
                                                                        construct_params);
      g_object_add_weak_pointer (self, (gpointer) &self);
      return self;
    }

  return g_object_ref (self);
}


static void
cc_application_dispose (GObject *object)
{
  G_OBJECT_CLASS (cc_application_parent_class)->dispose (object);
}


static void
cc_application_init (CcApplication *self)
{
  self->priv = G_TYPE_INSTANCE_GET_PRIVATE (self,
                                            CC_TYPE_APPLICATION,
                                            CcApplicationPrivate);
}


static void
cc_application_class_init (CcApplicationClass *class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (class);
  GApplicationClass *application_class = G_APPLICATION_CLASS (class);

  object_class->constructor = cc_application_constructor;
  object_class->dispose = cc_application_dispose;
  application_class->activate = cc_application_activate;
  application_class->startup = cc_application_startup;
  application_class->command_line = cc_application_command_line;

  g_type_class_add_private (class, sizeof (CcApplicationPrivate));
}


GtkApplication *
cc_application_new (void)
{
  return g_object_new (CC_TYPE_APPLICATION,
                       "application-id", "org.gnome.ControlCenter",
                       "flags", G_APPLICATION_HANDLES_COMMAND_LINE,
                       NULL);
}
