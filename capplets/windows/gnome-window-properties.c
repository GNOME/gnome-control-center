/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*- */

/* gnome-window-properties.c
 * Copyright (C) 2002 Seth Nickell
 * Copyright (C) 2002 Red Hat, Inc.
 *
 * Written by: Seth Nickell <snickell@stanford.edu>
 *             Havoc Pennington <hp@redhat.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
 * 02111-1307, USA.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <gconf/gconf-client.h>
#include <glade/glade.h>
#include <gnome-wm-manager.h>

#include "capplet-util.h"
#include "gconf-property-editor.h"

typedef struct
{
        int number;
        char *name;
        const char *value; /* machine-readable name for storing config */
} MouseClickModifier;

static GConfClient *gconf_client;
static GladeXML *dialog;
static GnomeWindowManager *current_wm; /* may be NULL */
static GtkWidget *dialog_win;
static GtkWidget *focus_mode_checkbutton;
static GtkWidget *autoraise_checkbutton;
static GtkWidget *autoraise_delay_spinbutton;
static GtkWidget *autoraise_delay_hbox;
static GtkWidget *double_click_titlebar_optionmenu;
static GtkWidget *double_click_titlebar_hbox;
static GtkWidget *alt_click_optionmenu;

static GnomeWMSettings settings;
static const GnomeWMDoubleClickAction *double_click_actions = NULL;
static int n_double_click_actions = 0;

static MouseClickModifier *mouse_modifiers = NULL;
static int n_mouse_modifiers = 0;

static void reload_mouse_modifiers (void);

static void
mouse_focus_toggled_callback (GtkWidget *button,
                              void      *data)
{
        GnomeWMSettings new_settings;

        new_settings.flags = GNOME_WM_SETTING_MOUSE_FOCUS;
        new_settings.focus_follows_mouse =
                gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (button));

        if (current_wm != NULL && new_settings.focus_follows_mouse != settings.focus_follows_mouse)
                gnome_window_manager_change_settings (current_wm, &new_settings);
}

static void
autoraise_toggled_callback (GtkWidget *button,
                            void      *data)
{
        GnomeWMSettings new_settings;

        new_settings.flags = GNOME_WM_SETTING_AUTORAISE;
        new_settings.autoraise =
                gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (button));

        if (current_wm != NULL && new_settings.autoraise != settings.autoraise)
                gnome_window_manager_change_settings (current_wm, &new_settings);

}

static void
autoraise_delay_value_changed_callback (GtkWidget *spinbutton,
                                        void      *data)
{
        GnomeWMSettings new_settings;

        new_settings.flags = GNOME_WM_SETTING_AUTORAISE_DELAY;
        new_settings.autoraise_delay =
                gtk_spin_button_get_value (GTK_SPIN_BUTTON (spinbutton)) * 100;

        if (current_wm != NULL && new_settings.autoraise_delay != settings.autoraise_delay)
                gnome_window_manager_change_settings (current_wm, &new_settings);
}

static void
double_click_titlebar_changed_callback (GtkWidget *optionmenu,
                                        void      *data)
{
        GnomeWMSettings new_settings;
        
        new_settings.flags = GNOME_WM_SETTING_DOUBLE_CLICK_ACTION;
        new_settings.double_click_action =
                gtk_option_menu_get_history (GTK_OPTION_MENU (optionmenu));

        if (current_wm != NULL && new_settings.double_click_action != settings.double_click_action)
                gnome_window_manager_change_settings (current_wm, &new_settings);
}

static void
alt_click_modifier_changed_callback (GtkWidget *optionmenu,
                                     void      *data)
{
        GnomeWMSettings new_settings;
        int history;
        
        new_settings.flags = GNOME_WM_SETTING_MOUSE_MOVE_MODIFIER;
        history = gtk_option_menu_get_history (GTK_OPTION_MENU (optionmenu));

        if (history >= n_mouse_modifiers) /* paranoia */
                return;

        new_settings.mouse_move_modifier = mouse_modifiers[history].value;
        
        if (current_wm != NULL &&
            strcmp (new_settings.mouse_move_modifier,
                    settings.mouse_move_modifier) != 0)
                gnome_window_manager_change_settings (current_wm, &new_settings);
}

static void
update_sensitivity (void)
{
        gtk_widget_set_sensitive (autoraise_checkbutton,
                                  settings.focus_follows_mouse);
        
        gtk_widget_set_sensitive (autoraise_delay_hbox,
                                  settings.focus_follows_mouse && settings.autoraise);

        gtk_widget_set_sensitive (double_click_titlebar_optionmenu,
                                  n_double_click_actions > 1);

        /* disable the whole dialog while no WM is running, or
         * a WM we don't understand is running. We should probably do
         * something better. I don't want to just launch the config tool
         * as we would on startup though, because then you'd get weirdness
         * in the gap time between old and new WM.
         */
        gtk_widget_set_sensitive (dialog_win, current_wm != NULL);
}

static void
init_settings_struct (GnomeWMSettings *settings)
{
        /* Init fields that weren't initialized */
        if ((settings->flags & GNOME_WM_SETTING_MOUSE_FOCUS) == 0)
                settings->focus_follows_mouse = FALSE;
        
        if ((settings->flags & GNOME_WM_SETTING_AUTORAISE) == 0)
                settings->autoraise = FALSE;
        
        if ((settings->flags & GNOME_WM_SETTING_AUTORAISE_DELAY) == 0)
                settings->autoraise_delay = 1000;
        
        if ((settings->flags & GNOME_WM_SETTING_MOUSE_MOVE_MODIFIER) == 0)
                settings->mouse_move_modifier = "Super";

        if ((settings->flags & GNOME_WM_SETTING_DOUBLE_CLICK_ACTION) == 0)
                settings->double_click_action = 0;
}

static void
set_alt_click_optionmenu_value (const GnomeWMSettings *settings)
{
        int i;
        
        i = 0;
        while (i < n_mouse_modifiers) {
                if (strcmp (mouse_modifiers[i].value,
                            settings->mouse_move_modifier) == 0)
                        break;
                ++i;
        }
        
        if (i < n_mouse_modifiers)
                gtk_option_menu_set_history (GTK_OPTION_MENU (alt_click_optionmenu),
                                             i);
}

static void
rebuild_double_click_actions_menu (void)
{
        int i;
        GtkWidget *menu;
        
        menu = gtk_menu_new ();
        i = 0;
        while (i < n_double_click_actions) {
                GtkWidget *mi;
                
                mi = gtk_menu_item_new_with_label (double_click_actions[i].human_readable_name);
                gtk_menu_shell_append (GTK_MENU_SHELL (menu),
                                       mi);
                
                gtk_widget_show (mi);
                
                ++i;
        }
        
        gtk_option_menu_set_menu (GTK_OPTION_MENU (double_click_titlebar_optionmenu),
                                  menu);
}

static void
reload_settings (void)
{
        GnomeWMSettings new_settings;

        g_assert (n_mouse_modifiers > 0);
        
        if (current_wm != NULL) {
                new_settings.flags = GNOME_WM_SETTING_MOUSE_FOCUS |
                        GNOME_WM_SETTING_AUTORAISE |
                        GNOME_WM_SETTING_AUTORAISE_DELAY |
                        GNOME_WM_SETTING_MOUSE_MOVE_MODIFIER |
                        GNOME_WM_SETTING_DOUBLE_CLICK_ACTION;

                /* this will clear any flags that don't get filled in */
                gnome_window_manager_get_settings (current_wm, &new_settings);
        } else {
                new_settings.flags = 0;
        }

        init_settings_struct (&new_settings);
        
        if (new_settings.focus_follows_mouse != settings.focus_follows_mouse)
                gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (focus_mode_checkbutton),
                                              new_settings.focus_follows_mouse);

        if (new_settings.autoraise != settings.autoraise)
                gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (autoraise_checkbutton),
                                              new_settings.autoraise);

        if (new_settings.autoraise_delay != settings.autoraise_delay)
                gtk_spin_button_set_value (GTK_SPIN_BUTTON (autoraise_delay_spinbutton),
                                           new_settings.autoraise_delay / 100);
        
        if (n_double_click_actions > 0 &&
            new_settings.double_click_action != settings.double_click_action) {
                gtk_option_menu_set_history (GTK_OPTION_MENU (double_click_titlebar_optionmenu),
                                             new_settings.double_click_action);
        }
        

        if (settings.mouse_move_modifier == NULL ||
            strcmp (settings.mouse_move_modifier,
                    new_settings.mouse_move_modifier) != 0) {
                set_alt_click_optionmenu_value (&new_settings);
        }

        settings = new_settings;

        update_sensitivity ();
}

static void
wm_settings_changed_callback (GnomeWindowManager *wm,
                              void               *data)
{
        reload_settings ();
}

static void
update_wm (GdkScreen *screen,
           gboolean   load_settings)
{
        g_assert (n_mouse_modifiers > 0);
        
        if (current_wm != NULL) {
                g_signal_handlers_disconnect_by_func (G_OBJECT (current_wm),
                                                      G_CALLBACK (wm_settings_changed_callback),
                                                      NULL);
                current_wm = NULL;
                double_click_actions = NULL;
                n_double_click_actions = 0;
        }

        current_wm = gnome_wm_manager_get_current (screen);

        if (current_wm != NULL) {
                g_signal_connect (G_OBJECT (current_wm), "settings_changed",
                                  G_CALLBACK (wm_settings_changed_callback), NULL);

                gnome_window_manager_get_double_click_actions (current_wm,
                                                               &double_click_actions,
                                                               &n_double_click_actions);
                
        }

        rebuild_double_click_actions_menu ();
        if (load_settings)
                reload_settings ();
}

static void
wm_changed_callback (GdkScreen *screen,
                     void      *data)
{
        update_wm (screen, TRUE);
}

static void
response_cb (GtkWidget *dialog_win,
             int        response_id,
             void      *data)
{

        if (response_id == GTK_RESPONSE_HELP) {

        } else {
                gtk_widget_destroy (dialog_win);
        }
}

static void
try_spawn_config_tool (GdkScreen *screen)
{
        GError *error;

        error = NULL;
        gnome_wm_manager_spawn_config_tool_for_current (screen, &error);

        if (error != NULL) {
                GtkWidget *no_tool_dialog;
                char *str;
                char *escaped;

                escaped = g_markup_escape_text (error->message, -1);
                
                str = g_strdup_printf (_("<b>Cannot start the preferences application for your window manager</b>\n\n%s"),
                                       escaped);
                g_free (escaped);
                                       
                no_tool_dialog =
                        gtk_message_dialog_new (NULL,
                                                GTK_DIALOG_DESTROY_WITH_PARENT,
                                                GTK_MESSAGE_ERROR,
                                                GTK_BUTTONS_CLOSE,
                                                " ");
                gtk_window_set_title (GTK_WINDOW (no_tool_dialog), "");
                gtk_window_set_resizable (GTK_WINDOW (no_tool_dialog), FALSE);
                
                gtk_label_set_markup (GTK_LABEL (GTK_MESSAGE_DIALOG (no_tool_dialog)->label),
                                      str);

                g_free (str);
                
                gtk_dialog_run (GTK_DIALOG (no_tool_dialog));

                gtk_widget_destroy (no_tool_dialog);
                g_error_free (error);
                
                exit (1);
        }

        /* exit, let the config tool handle it */
        exit (0);
}

int
main (int argc, char **argv)
{
        GdkScreen *screen;
        GtkSizeGroup *size_group;
        
        bindtextdomain (GETTEXT_PACKAGE, GNOMELOCALEDIR);
	bind_textdomain_codeset(GETTEXT_PACKAGE, "UTF-8");
        textdomain (GETTEXT_PACKAGE);

  	gnome_program_init ("gnome-window-properties", VERSION,
			    LIBGNOMEUI_MODULE, argc, argv,
			    NULL);

        gconf_client = gconf_client_get_default ();
        
        gnome_wm_manager_init ();

        screen = gdk_display_get_default_screen (gdk_display_get_default ());
        
        current_wm = gnome_wm_manager_get_current (screen);

        if (current_wm == NULL) {
                try_spawn_config_tool (screen);
                return 0;
        }
        
        dialog = glade_xml_new (GLADEDIR "/gnome-window-properties.glade",
                                "main-dialog", GETTEXT_PACKAGE);

        if (dialog == NULL) {
                g_warning ("Missing glade file for gnome-window-properties");
                exit (1);
        }
        
        dialog_win = WID ("main-dialog");
        focus_mode_checkbutton = WID ("focus-mode-checkbutton");
        autoraise_checkbutton = WID ("autoraise-checkbutton");
        autoraise_delay_spinbutton = WID ("autoraise-delay-spinbutton");
        autoraise_delay_hbox = WID ("autoraise-delay-hbox");
        double_click_titlebar_optionmenu = WID ("double-click-titlebar-optionmenu");
        double_click_titlebar_hbox = WID ("double-click-titlebar-hbox");
        alt_click_optionmenu = WID ("alt-click-optionmenu");

        gtk_spin_button_set_range (GTK_SPIN_BUTTON (autoraise_delay_spinbutton),
                                   0, 100);

        gtk_spin_button_set_increments (GTK_SPIN_BUTTON (autoraise_delay_spinbutton),
                                        1, 5);

        size_group = gtk_size_group_new (GTK_SIZE_GROUP_HORIZONTAL);
        gtk_size_group_add_widget (size_group, double_click_titlebar_optionmenu);
        gtk_size_group_add_widget (size_group, alt_click_optionmenu);
        g_object_unref (G_OBJECT (size_group));

        settings.flags = 0;
        init_settings_struct (&settings);

        reload_mouse_modifiers ();
        update_wm (screen, FALSE);
        
        set_alt_click_optionmenu_value (&settings);
        gtk_spin_button_set_value (GTK_SPIN_BUTTON (autoraise_delay_spinbutton),
                                   settings.autoraise_delay / 100);
        gtk_option_menu_set_history (GTK_OPTION_MENU (double_click_titlebar_optionmenu),
                                     settings.double_click_action);
        
        reload_settings (); /* must come before below signal connections */
        
        g_signal_connect (G_OBJECT (dialog_win), "response",
                          G_CALLBACK (response_cb), NULL);
        
        g_signal_connect (G_OBJECT (dialog_win), "destroy",
                          G_CALLBACK (gtk_main_quit), NULL);


        g_signal_connect (G_OBJECT (focus_mode_checkbutton), "toggled",
                          G_CALLBACK (mouse_focus_toggled_callback), NULL);

        g_signal_connect (G_OBJECT (autoraise_checkbutton), "toggled",
                          G_CALLBACK (autoraise_toggled_callback), NULL);

        g_signal_connect (G_OBJECT (autoraise_delay_spinbutton), "value_changed",
                          G_CALLBACK (autoraise_delay_value_changed_callback), NULL);

        g_signal_connect (G_OBJECT (double_click_titlebar_optionmenu), "changed",
                          G_CALLBACK (double_click_titlebar_changed_callback), NULL);
                
        g_signal_connect (G_OBJECT (alt_click_optionmenu), "changed",
                          G_CALLBACK (alt_click_modifier_changed_callback), NULL);
        
        g_signal_connect (G_OBJECT (screen), "window_manager_changed",
                          G_CALLBACK (wm_changed_callback), NULL);
        
        gtk_widget_show (dialog_win);
        
        gtk_main ();

        return 0;
}

#include <X11/Xlib.h>
#include <X11/keysym.h>
#include <gdk/gdkx.h>
static void
reload_mouse_modifiers (void)
{
  XModifierKeymap *modmap;
  KeySym *keymap;
  int keysyms_per_keycode;
  int map_size;
  int i;
  gboolean have_meta;
  gboolean have_hyper;
  gboolean have_super;
  int min_keycode, max_keycode;
  
  XDisplayKeycodes (gdk_display,
                    &min_keycode,
                    &max_keycode);

  keymap = XGetKeyboardMapping (gdk_display,
                                min_keycode,
                                max_keycode - min_keycode,
                                &keysyms_per_keycode);
  
  modmap = XGetModifierMapping (gdk_display);

  have_super = FALSE;
  have_meta = FALSE;
  have_hyper = FALSE;
  
  /* there are 8 modifiers, and the first 3 are shift, shift lock,
   * and control
   */
  map_size = 8 * modmap->max_keypermod;
  i = 3 * modmap->max_keypermod;
  while (i < map_size) {
          /* get the key code at this point in the map,
           * see if its keysym is one we're interested in
           */
          int keycode = modmap->modifiermap[i];
          
          if (keycode >= min_keycode &&
              keycode <= max_keycode) {
                  int j = 0;
                  KeySym *syms = keymap + (keycode - min_keycode) * keysyms_per_keycode;
                  
                  while (j < keysyms_per_keycode) {              
                          if (syms[j] == XK_Super_L ||
                              syms[j] == XK_Super_R)
                                  have_super = TRUE;
                          else if (syms[j] == XK_Hyper_L ||
                                   syms[j] == XK_Hyper_R)
                                  have_hyper = TRUE;                          
                          else if ((syms[j] == XK_Meta_L ||
                                    syms[j] == XK_Meta_R) &&
                                   (1 << ( i / modmap->max_keypermod)) != Mod1Mask)
                                  have_meta = TRUE;
                          ++j;
                  }
          }
          
          ++i;
  }

  XFreeModifiermap (modmap);
  XFree (keymap);

  i = 0;
  while (i < n_mouse_modifiers) {
          g_free (mouse_modifiers[i].name);
          ++i;
  }
  g_free (mouse_modifiers);
  mouse_modifiers = NULL;
                  
  
  n_mouse_modifiers = 2; /* control, alt */
  if (have_super)
          ++n_mouse_modifiers;
  if (have_hyper)
          ++n_mouse_modifiers;
  if (have_meta)
          ++n_mouse_modifiers;

  g_free (mouse_modifiers);

  mouse_modifiers = g_new0 (MouseClickModifier, n_mouse_modifiers);

  i = 0;

  mouse_modifiers[i].number = i;
  mouse_modifiers[i].name = g_strdup (_("Control"));
  mouse_modifiers[i].value = "Control";
  ++i;

  mouse_modifiers[i].number = i;
  mouse_modifiers[i].name = g_strdup (_("Alt"));
  mouse_modifiers[i].value = "Alt";
  ++i;

  if (have_hyper) {
          mouse_modifiers[i].number = i;
          mouse_modifiers[i].name = g_strdup (_("Hyper"));
          mouse_modifiers[i].value = "Hyper";
          ++i;
  }

  if (have_super) {
          mouse_modifiers[i].number = i;
          mouse_modifiers[i].name = g_strdup (_("Super"));
          mouse_modifiers[i].value = "Super";
          ++i;
  }

  if (have_meta) {
          mouse_modifiers[i].number = i;
          mouse_modifiers[i].name = g_strdup (_("Meta"));
          mouse_modifiers[i].value = "Meta";
          ++i;
  }

  g_assert (i == n_mouse_modifiers);
  
  /* Build modifier option menu */
  {
          GtkWidget *menu;
          
          menu = gtk_menu_new ();
          i = 0;
          while (i < n_mouse_modifiers) {
                  GtkWidget *mi;
                  
                  mi = gtk_menu_item_new_with_label (mouse_modifiers[i].name);
                  gtk_menu_shell_append (GTK_MENU_SHELL (menu),
                                         mi);
                  
                  gtk_widget_show (mi);
                  
                  ++i;
          }
          
          gtk_option_menu_set_menu (GTK_OPTION_MENU (alt_click_optionmenu),
                                    menu);
  }
}
