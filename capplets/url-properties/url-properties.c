/* url-properties -- a capplet to configure the behaviour of gnome_url_show
 * Copyright (C) 1998  James Henstridge <james@daa.com.au>
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
 */

#ifdef HAVE_CONFIG_H
#   include <config.h>
#endif

#include <string.h>
#include <gtk/gtk.h>
#include <gnome.h>
#include <gconf/gconf-client.h>

GtkWidget *capplet, *protocol, *combo, *clist;
GSList *handlers_removed = NULL;

void url_capplet_refill_clist (void);
void url_capplet_commit(void);

void build_capplet(void);
void state_changed (void);
void response_cb (GtkDialog *dialog, GtkResponseType response, gpointer data);
void set_handler(GtkEntry *entry);
void remove_handler(GtkButton *button);
void select_clist_row(GtkCList *clist, gint row, gint column);

int
main(int argc, char *argv[]) {
  gint init_ret;

  bindtextdomain(PACKAGE, GNOMELOCALEDIR);
  textdomain(PACKAGE);

  gnome_program_init ("url-properties", VERSION,
		      LIBGNOMEUI_MODULE, argc, argv, NULL);

  build_capplet();
  url_capplet_refill_clist (); /* this will refill the clist */

  gtk_signal_connect(GTK_OBJECT(capplet), "response",
		     GTK_SIGNAL_FUNC(response_cb), NULL);

  gtk_main();
  return 0;
}

void build_capplet(void) {
  GtkWidget *vbox, *hbox, *item, *button;
  gchar *titles[] = { N_("Protocol"), N_("Command") };

  capplet = gtk_dialog_new_with_buttons (_("URL Handlers"), NULL,
		  			 -1,
					 GTK_STOCK_HELP, GTK_RESPONSE_HELP,
					 GTK_STOCK_APPLY, GTK_RESPONSE_APPLY,
					 GTK_STOCK_CLOSE, GTK_RESPONSE_CLOSE,
					 NULL);
  gtk_dialog_set_response_sensitive (GTK_DIALOG (capplet), GTK_RESPONSE_APPLY,
		  		     FALSE);

  vbox = gtk_vbox_new(FALSE, 5);
  gtk_widget_set_usize (vbox, 400, 250);
  gtk_box_pack_start (GTK_BOX (GTK_DIALOG (capplet)->vbox), vbox, TRUE, TRUE, 0);
  gtk_widget_show(vbox);

  hbox = gtk_hbox_new(FALSE, 5);
  gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, TRUE, 0);
  gtk_widget_show(hbox);

  protocol = gtk_entry_new();
  gtk_widget_set_usize(protocol, 80, -1);
  gtk_box_pack_start(GTK_BOX(hbox), protocol, FALSE, TRUE, 0);
  gtk_widget_show(protocol);

  item = gtk_label_new(_("handler:"));
  gtk_box_pack_start(GTK_BOX(hbox), item, FALSE, TRUE, 0);
  gtk_widget_show(item);

  combo = gtk_combo_new();
  gtk_combo_set_use_arrows(GTK_COMBO(combo), FALSE);
  gtk_combo_set_value_in_list(GTK_COMBO(combo), FALSE, FALSE);
  gtk_combo_disable_activate(GTK_COMBO(combo));

  /* set some commonly used handlers */
  item = gtk_list_item_new_with_label(_("Netscape"));
  gtk_combo_set_item_string(GTK_COMBO(combo), GTK_ITEM(item),
			    "gnome-moz-remote '%s'");
  gtk_container_add(GTK_CONTAINER(GTK_COMBO(combo)->list), item);
  gtk_widget_show(item);
  item = gtk_list_item_new_with_label(_("Netscape (new window)"));
  gtk_combo_set_item_string(GTK_COMBO(combo), GTK_ITEM(item),
			    "gnome-moz-remote --newwin '%s'");
  gtk_container_add(GTK_CONTAINER(GTK_COMBO(combo)->list), item);
  gtk_widget_show(item);

  item = gtk_list_item_new_with_label(_("Help browser"));
  gtk_combo_set_item_string(GTK_COMBO(combo), GTK_ITEM(item),
			    "gnome-help-browser '#%s'");
  gtk_container_add(GTK_CONTAINER(GTK_COMBO(combo)->list), item);
  gtk_widget_show(item);
  item = gtk_list_item_new_with_label(_("Help browser (new window)"));
  gtk_combo_set_item_string(GTK_COMBO(combo), GTK_ITEM(item),
			    "gnome-help-browser '%s'");
  gtk_container_add(GTK_CONTAINER(GTK_COMBO(combo)->list), item);
  gtk_widget_show(item);

  gtk_box_pack_start(GTK_BOX(hbox), combo, TRUE, TRUE, 0);
  gtk_widget_show(combo);

  gtk_signal_connect(GTK_OBJECT(GTK_COMBO(combo)->entry), "activate",
		     GTK_SIGNAL_FUNC(set_handler), NULL);

  button = gtk_button_new_with_label(_("Set"));
  gtk_box_pack_start(GTK_BOX(hbox), button, FALSE, TRUE, 0);
  gtk_widget_show(button);

  gtk_signal_connect_object(GTK_OBJECT(button), "clicked",
			    GTK_SIGNAL_FUNC(set_handler),
			    GTK_OBJECT(GTK_COMBO(combo)->entry));

  button = gtk_button_new_with_label(_("Remove"));
  gtk_box_pack_start(GTK_BOX(hbox), button, FALSE, TRUE, 0);
  gtk_widget_show(button);

  gtk_signal_connect(GTK_OBJECT(button), "clicked",
		     GTK_SIGNAL_FUNC(remove_handler), NULL);

  item = gtk_scrolled_window_new(NULL, NULL);
  gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(item),
				 GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
  gtk_box_pack_start(GTK_BOX(vbox), item, TRUE, TRUE, 0);
  gtk_widget_show(item);

  titles[0] = _(titles[0]);
  titles[1] = _(titles[1]);
  clist = gtk_clist_new_with_titles(2, titles);
  gtk_container_add(GTK_CONTAINER(item), clist);
  gtk_widget_show(clist);

  gtk_clist_set_column_width(GTK_CLIST(clist), 0, 50);
  gtk_clist_set_selection_mode(GTK_CLIST(clist), GTK_SELECTION_BROWSE);
  gtk_clist_set_sort_type(GTK_CLIST(clist), GTK_SORT_ASCENDING);
  gtk_clist_set_sort_column(GTK_CLIST(clist), 0);
  gtk_clist_set_auto_sort(GTK_CLIST(clist), TRUE);

  gtk_signal_connect(GTK_OBJECT(clist), "select_row",
		     GTK_SIGNAL_FUNC(select_clist_row), NULL);

  gtk_widget_show(capplet);
}

void url_capplet_refill_clist(void) {
  GSList *l;
  GConfClient *client;

  gtk_clist_freeze(GTK_CLIST(clist));
  gtk_clist_clear(GTK_CLIST(clist));
  
  client = gconf_client_get_default ();
  l = gconf_client_all_entries (client, "/desktop/gnome/url-handlers", NULL);
  for (; l != NULL; l = l->next)
  {
    GConfEntry *e = l->data;
    gchar *key = g_strdup (e->key);
    gchar *value = g_strdup (gconf_value_get_string (gconf_entry_get_value (e)));
    int len = strlen(key);

    if (len > 5 && !strcmp(&key[len-5], "-show")) {
      gchar *row[2];
      gint id;
      /* it is a *-show key */
      key[len-5] = '\0';
      row[0] = g_basename (key);
      row[1] = value;

      id = gtk_clist_append(GTK_CLIST(clist), row);
      if (!g_strcasecmp(key, "default"))
         gtk_clist_select_row(GTK_CLIST(clist), id, 0);
    }
    g_free(key);
    g_free(value);
    gconf_entry_free (e);
  }
  gtk_clist_thaw(GTK_CLIST(clist));

  g_slist_free (l);
  g_object_unref (G_OBJECT (client));
}

void url_capplet_commit(void) {
  gint num_rows, row;
  gchar *col1, *col2, *key;
  gchar *prefix = "/desktop/gnome/url-handlers/";
  GConfClient *client = gconf_client_get_default ();
  GSList *l;
  
  for (l = handlers_removed; l != NULL; l = l->next)
  {
	  key = g_strconcat (prefix, l->data, "-show", NULL);
	  gconf_client_unset (client, key, NULL);
	  g_free (key);
	  g_free (l->data);
  }
  
  g_slist_free (handlers_removed);
  handlers_removed = NULL;
  
  num_rows = GTK_CLIST(clist)->rows;
  for (row = 0; row < num_rows; row++) {
    gtk_clist_get_text(GTK_CLIST(clist), row, 0, &col1);
    gtk_clist_get_text(GTK_CLIST(clist), row, 1, &col2);
    key = g_strconcat (prefix, col1, "-show", NULL);
    gconf_client_set_string (client, key, col2, NULL);
    g_free(key);
  }

  g_object_unref (G_OBJECT (client));
}

void set_handler(GtkEntry *entry) {
  gint row, num_rows;
  gchar *col1, *prot, *cols[2];

  num_rows = GTK_CLIST(clist)->rows;
  prot = gtk_entry_get_text(GTK_ENTRY(protocol));
  for (row = 0; row < num_rows; row++) {
    gtk_clist_get_text(GTK_CLIST(clist), row, 0, &col1);
    if (!g_strcasecmp(prot, col1)) {
      gtk_clist_set_text(GTK_CLIST(clist), row, 1, gtk_entry_get_text(entry));
      state_changed ();
      return;
    }
  }
  /* prot not in clist */
  cols[0] = prot;
  cols[1] = gtk_entry_get_text(entry);
  gtk_clist_append(GTK_CLIST(clist), cols);
  state_changed ();
}

void remove_handler(GtkButton *button) {
  gint row, num_rows;
  gchar *col1, *prot;

  num_rows = GTK_CLIST(clist)->rows;
  prot = gtk_entry_get_text(GTK_ENTRY(protocol));
  for (row = 0; row < num_rows; row++) {
    gtk_clist_get_text(GTK_CLIST(clist), row, 0, &col1);
    if (!g_strcasecmp(prot, col1)) {
      handlers_removed = g_slist_prepend (handlers_removed, g_strdup (col1));
      gtk_clist_unselect_row(GTK_CLIST(clist), row, 0);
      gtk_clist_remove(GTK_CLIST(clist), row);
      state_changed ();
      gtk_entry_set_text(GTK_ENTRY(protocol), "");
      gtk_entry_set_text(GTK_ENTRY(GTK_COMBO(combo)->entry), "");
      if (num_rows > 1)
	gtk_clist_select_row(GTK_CLIST(clist), 0, 0);
      return;
    }
  }
}

void select_clist_row(GtkCList *clist, gint row, gint column) {
  gchar *col1, *col2;

  /* get column values */
  gtk_clist_get_text(GTK_CLIST(clist), row, 0, &col1);
  gtk_clist_get_text(GTK_CLIST(clist), row, 1, &col2);

  gtk_entry_set_text(GTK_ENTRY(protocol), col1);

  gtk_entry_set_text(GTK_ENTRY(GTK_COMBO(combo)->entry), col2);
}

void state_changed (void)
{
  gtk_dialog_set_response_sensitive (GTK_DIALOG (capplet), GTK_RESPONSE_APPLY,
		  		     TRUE);
}

void response_cb (GtkDialog *dialog, GtkResponseType response, gpointer data)
{
	switch (response)
	{
	case GTK_RESPONSE_NONE:
	case GTK_RESPONSE_CLOSE:
		gtk_main_quit ();
		break;
	case GTK_RESPONSE_APPLY:
		url_capplet_commit ();
		break;
	}
}
