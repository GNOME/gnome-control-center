/* -*-Mode: c-*- */
/* Copyright (C) 1997 Red Hat Software, Inc.
 *
 * This is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */
#include <locale.h>
#include <libintl.h>
#define _(String) gettext(String)
#include "userdialogs.h"

GtkWidget*
create_message_box(gchar* message, gchar* title)
{
  GtkWidget* message_box;
  GtkWidget* label;
  GtkWidget* hbox;
  GtkWidget* ok;

  message_box = gtk_dialog_new();
  gtk_window_position(GTK_WINDOW(message_box), GTK_WIN_POS_CENTER);
  gtk_container_set_border_width(GTK_CONTAINER(message_box), 5);
  if(title == NULL)
    gtk_window_set_title(GTK_WINDOW(message_box), _("Message"));
  else
    gtk_window_set_title(GTK_WINDOW(message_box), title);

  label = gtk_label_new(message);
  hbox = gtk_hbox_new(TRUE, 5);
  ok = gtk_button_new_with_label(_(UD_OK_TEXT));
  gtk_misc_set_padding(GTK_MISC(GTK_BIN(ok)->child), 4, 0);
  gtk_signal_connect_object(GTK_OBJECT(ok), "clicked", 
			    (GtkSignalFunc) gtk_widget_destroy,
			    (gpointer) message_box);
  gtk_widget_set_usize(ok, 50, 0);

  gtk_box_pack_start(GTK_BOX(GTK_DIALOG(message_box)->vbox), hbox,
		     FALSE, FALSE, 5);
  gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 5);
  gtk_box_pack_start(GTK_BOX(GTK_DIALOG(message_box)->action_area), ok,
		     FALSE, FALSE, 0);
  
  GTK_WIDGET_SET_FLAGS(ok, GTK_CAN_DEFAULT);
  gtk_widget_grab_default(ok);

  gtk_widget_show(ok);
  gtk_widget_show(label);
  gtk_widget_show(hbox);
  gtk_widget_show(message_box);

  return message_box;

}

/* conslidate error_box and message_box.. they're the same damn thing
 * with a different default title.
 */
GtkWidget*
create_error_box(gchar* error, gchar* title)
{
  GtkWidget* error_box;
  GtkWidget* label;
  GtkWidget* hbox;
  GtkWidget* ok;

  error_box = gtk_dialog_new();
  gtk_window_position(GTK_WINDOW(error_box), GTK_WIN_POS_CENTER);
  gtk_container_set_border_width(GTK_CONTAINER(error_box), 5);
  if(title == NULL)
    gtk_window_set_title(GTK_WINDOW(error_box), _("Error"));
  else
    gtk_window_set_title(GTK_WINDOW(error_box), title);

  label = gtk_label_new(error);
  hbox = gtk_hbox_new(TRUE, 5);
  ok = gtk_button_new_with_label(_(UD_OK_TEXT));
  gtk_misc_set_padding(GTK_MISC(GTK_BIN(ok)->child), 4, 0);
  gtk_signal_connect_object(GTK_OBJECT(ok), "clicked", 
			    (GtkSignalFunc) gtk_widget_destroy,
			    (gpointer) error_box);
  gtk_widget_set_usize(ok, 50, 0);

  gtk_box_pack_start(GTK_BOX(GTK_DIALOG(error_box)->vbox), hbox,
		     FALSE, FALSE, 5);
  gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 5);
  gtk_box_pack_start(GTK_BOX(GTK_DIALOG(error_box)->action_area), ok,
		     FALSE, FALSE, 0);
  
  GTK_WIDGET_SET_FLAGS(ok, GTK_CAN_DEFAULT);
  gtk_widget_grab_default(ok);

  gtk_widget_show(ok);
  gtk_widget_show(label);
  gtk_widget_show(hbox);
  gtk_widget_show(error_box);

  return error_box;
}

GtkWidget*
create_query_box(gchar* prompt, gchar* title, GtkSignalFunc func)
{
  GtkWidget* query_box;
  GtkWidget* label;
  GtkWidget* entry;
  GtkWidget* hbox;
  GtkWidget* ok;

  query_box = gtk_dialog_new();
  gtk_window_position(GTK_WINDOW(query_box), GTK_WIN_POS_CENTER);
  gtk_container_set_border_width(GTK_CONTAINER(query_box), 5);
  if(title == NULL)
    gtk_window_set_title(GTK_WINDOW(query_box), _("Prompt"));
  else
    gtk_window_set_title(GTK_WINDOW(query_box), _("Prompt"));
  
  label = gtk_label_new(prompt);
  entry = gtk_entry_new();
  ok = gtk_button_new_with_label(_(UD_OK_TEXT));
  gtk_misc_set_padding(GTK_MISC(GTK_BIN(ok)->child), 4, 0);
  gtk_widget_set_usize(ok, 50, 0);

  hbox = gtk_hbox_new(TRUE, 0);

  gtk_signal_connect_object(GTK_OBJECT(entry), "activate", 
			    (GtkSignalFunc) gtk_button_clicked,
			    (gpointer) GTK_BUTTON(ok));

  /* FIXME: memory leak... well, not really.  Just rely on the caller
   * to free the widget... 'cept that's not nice either. :-S 
   */
  gtk_signal_connect_object(GTK_OBJECT(ok), "clicked",
			    (GtkSignalFunc) gtk_widget_hide,
 			    (gpointer) query_box);
  if(func != NULL)
    {
      gtk_signal_connect(GTK_OBJECT(ok), "clicked", func, entry);
    }

  gtk_box_pack_start(GTK_BOX(GTK_DIALOG(query_box)->vbox), label,
		     FALSE, FALSE, 5);
  gtk_box_pack_start(GTK_BOX(GTK_DIALOG(query_box)->vbox), hbox,
		     FALSE, FALSE, 5);
  gtk_box_pack_start(GTK_BOX(hbox), entry, TRUE, TRUE, 5);
  gtk_box_pack_start(GTK_BOX(GTK_DIALOG(query_box)->action_area), ok,
		     TRUE, FALSE, 0);
  
  GTK_WIDGET_SET_FLAGS(ok, GTK_CAN_DEFAULT);
  gtk_widget_grab_default(ok);

  gtk_widget_grab_focus(entry);

  gtk_widget_show(ok);
  gtk_widget_show(label);
  gtk_widget_show(entry);
  gtk_widget_show(hbox);
  gtk_widget_show(query_box);

  return query_box;
}

GtkWidget*
create_invisible_query_box(gchar* prompt, gchar* title, GtkSignalFunc func)
{
  GtkWidget* query_box;
  GtkWidget* label;
  GtkWidget* entry;
  GtkWidget* hbox;
  GtkWidget* ok;
  
  query_box = gtk_dialog_new();
  gtk_window_position(GTK_WINDOW(query_box), GTK_WIN_POS_CENTER);
  gtk_container_set_border_width(GTK_CONTAINER(query_box), 5);
  gtk_window_set_title(GTK_WINDOW(query_box), _("Prompt"));
/*   gtk_container_border_width(GTK_CONTAINER(GTK_DIALOG(query_box)->vbox), 5); */
  label = gtk_label_new(prompt);
  entry = gtk_entry_new();
  gtk_entry_set_visibility(GTK_ENTRY(entry), FALSE);

  hbox = gtk_hbox_new(TRUE, 5);

  ok = gtk_button_new_with_label(_("OK"));
  gtk_misc_set_padding(GTK_MISC(GTK_BIN(ok)->child), 4, 0);

  gtk_signal_connect_object(GTK_OBJECT(entry), "activate", 
			    (GtkSignalFunc) gtk_button_clicked,
			    (gpointer) GTK_BUTTON(ok));
  gtk_signal_connect_object(GTK_OBJECT(ok), "clicked",
			    (GtkSignalFunc) gtk_widget_hide,
			    (gpointer) query_box);
  gtk_widget_set_usize(ok, 50, 0);

  if(func != NULL)
    {
      gtk_signal_connect(GTK_OBJECT(ok), "clicked", func, entry);
    }

  gtk_box_pack_start(GTK_BOX(GTK_DIALOG(query_box)->vbox), label,
		     FALSE, FALSE, 5);
  gtk_box_pack_start(GTK_BOX(GTK_DIALOG(query_box)->vbox), hbox,
		     FALSE, FALSE, 5);
  gtk_box_pack_start(GTK_BOX(hbox), entry, TRUE, TRUE, 5);
  gtk_box_pack_start(GTK_BOX(GTK_DIALOG(query_box)->action_area), ok,
		     TRUE, FALSE, 0);
  
  GTK_WIDGET_SET_FLAGS(ok, GTK_CAN_DEFAULT);
  gtk_widget_grab_default(ok);

  gtk_widget_grab_focus(entry);

  gtk_widget_show(ok);
  gtk_widget_show(label);
  gtk_widget_show(entry);
  gtk_widget_show(hbox);
  gtk_widget_show(query_box);

  return query_box;
}
