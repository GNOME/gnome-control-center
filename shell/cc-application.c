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

#if defined(HAVE_WACOM) || defined(HAVE_CHEESE)
#include <clutter-gtk/clutter-gtk.h>
#endif /* HAVE_WACOM || HAVE_CHEESE */

struct _CcApplication
{
  GtkApplication  parent;

  CcWindow       *window;
};

G_DEFINE_TYPE (CcApplication, cc_application, GTK_TYPE_APPLICATION)

const GOptionEntry all_options[] = {
  { "version", 0, 0, G_OPTION_ARG_NONE, NULL, N_("Display version number"), NULL },
  { "verbose", 'v', 0, G_OPTION_ARG_NONE, NULL, N_("Enable verbose mode"), NULL },
  { "overview", 'o', 0, G_OPTION_ARG_NONE, NULL, N_("Show the overview"), NULL },
  { "search", 's', 0, G_OPTION_ARG_STRING, NULL, N_("Search for the string"), "SEARCH" },
  { "list", 'l', 0, G_OPTION_ARG_NONE, NULL, N_("List possible panel names and exit"), NULL },
  { G_OPTION_REMAINING, '\0', 0, G_OPTION_ARG_FILENAME_ARRAY, NULL, N_("Panel to display"), N_("[PANEL] [ARGUMENT…]") },
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

  panel = cc_shell_get_active_panel (CC_SHELL (self->window));
  if (panel)
    uri = cc_panel_get_help_uri (panel);

  window = cc_shell_get_toplevel (CC_SHELL (self->window));
  gtk_show_uri_on_window (GTK_WINDOW (window),
                          uri ? uri : "help:gnome-help/prefs",
                          GDK_CURRENT_TIME,
                          NULL);
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
  if (!cc_shell_set_active_panel_from_id (CC_SHELL (self->window), panel_id, parameters, &error))
    {
      g_warning ("Failed to activate the '%s' panel: %s", panel_id, error->message);
      g_error_free (error);
    }
  g_variant_unref (parameters);

  /* Now present the window */
  g_application_activate (G_APPLICATION (self));
}

static gint
cc_application_handle_local_options (GApplication *application,
                                     GVariantDict *options)
{
  if (g_variant_dict_contains (options, "version"))
    {
      g_print ("%s %s\n", PACKAGE, VERSION);
      return 0;
    }

  if (g_variant_dict_contains (options, "list"))
    {
      GList *panels, *l;

      panels = cc_panel_loader_get_panels ();

      g_print ("%s\n", _("Available panels:"));
      for (l = panels; l != NULL; l = l->next)
        g_print ("\t%s\n", (char *) l->data);

      g_list_free (panels);

      return 0;
    }

  return -1;
}

static int
cc_application_command_line (GApplication            *application,
                             GApplicationCommandLine *command_line)
{
  CcApplication *self = CC_APPLICATION (application);
  GVariantDict *options;
  int retval = 0;
  char *search_str;
  GStrv start_panels = NULL;
  gboolean debug;

  options = g_application_command_line_get_options_dict (command_line);

  debug = g_variant_dict_contains (options, "verbose");
  cc_shell_log_set_debug (debug);

  gtk_window_present (GTK_WINDOW (self->window));

  if (g_variant_dict_lookup (options, "search", "&s", &search_str))
    {
      cc_window_set_search_item (self->window, search_str);
    }
  else if (g_variant_dict_contains (options, "overview"))
    {
      cc_window_set_overview_page (self->window);
    }
  else if (g_variant_dict_lookup (options, G_OPTION_REMAINING, "^a&ay", &start_panels))
    {
      const char *start_id;
      GError *err = NULL;
      GVariant *parameters;
      GVariantBuilder builder;
      int i;

      g_return_val_if_fail (start_panels[0] != NULL, 1);
      start_id = start_panels[0];

      if (start_panels[1])
        g_debug ("Extra argument: %s", start_panels[1]);
      else
        g_debug ("No extra argument");

      g_variant_builder_init (&builder, G_VARIANT_TYPE ("av"));

      for (i = 1; start_panels[i] != NULL; i++)
        g_variant_builder_add (&builder, "v", g_variant_new_string (start_panels[i]));
      parameters = g_variant_builder_end (&builder);
      if (!cc_shell_set_active_panel_from_id (CC_SHELL (self->window), start_id, parameters, &err))
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

  g_free (start_panels);
  start_panels = NULL;

  return retval;
}

static void
cc_application_quit (GSimpleAction *simple,
                     GVariant      *parameter,
                     gpointer       user_data)
{
  CcApplication *self = CC_APPLICATION (user_data);

  gtk_widget_destroy (GTK_WIDGET (self->window));
}


static void
cc_application_activate (GApplication *application)
{
  CcApplication *self = CC_APPLICATION (application);

  gtk_window_present (GTK_WINDOW (self->window));
}

static void
cc_application_startup (GApplication *application)
{
  CcApplication *self = CC_APPLICATION (application);
  GMenu *menu;
  GMenu *section;
  GSimpleAction *action;
  const gchar *help_accels[] = { "F1", NULL };

  G_APPLICATION_CLASS (cc_application_parent_class)->startup (application);

#if defined(HAVE_WACOM) || defined(HAVE_CHEESE)
  if (gtk_clutter_init (NULL, NULL) != CLUTTER_INIT_SUCCESS)
    {
      g_critical ("Unable to initialize Clutter");
      return;
    }
#endif /* HAVE_WACOM || HAVE_CHEESE */

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
  g_menu_append (section, _("Keyboard Shortcuts"), "win.show-help-overlay");
  g_menu_append (section, _("Help"), "app.help");
  g_menu_append (section, _("Quit"), "app.quit");

  g_menu_append_section (menu, NULL, G_MENU_MODEL (section));

  gtk_application_set_app_menu (GTK_APPLICATION (application),
                                G_MENU_MODEL (menu));

  gtk_application_set_accels_for_action (GTK_APPLICATION (application),
                                         "app.help", help_accels);

  self->window = cc_window_new (GTK_APPLICATION (application));
}

static GObject *
cc_application_constructor (GType                  type,
                            guint                  n_construct_params,
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
cc_application_class_init (CcApplicationClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GApplicationClass *application_class = G_APPLICATION_CLASS (klass);

  object_class->constructor = cc_application_constructor;
  application_class->activate = cc_application_activate;
  application_class->startup = cc_application_startup;
  application_class->command_line = cc_application_command_line;
  application_class->handle_local_options = cc_application_handle_local_options;
}

static void
cc_application_init (CcApplication *self)
{
  g_application_add_main_option_entries (G_APPLICATION (self), all_options);
}

GtkApplication *
cc_application_new (void)
{
  return g_object_new (CC_TYPE_APPLICATION,
                       "application-id", "org.gnome.ControlCenter",
                       "flags", G_APPLICATION_HANDLES_COMMAND_LINE,
                       NULL);
}
