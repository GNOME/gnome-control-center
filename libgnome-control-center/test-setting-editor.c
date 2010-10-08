/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*-
 *
 * Copyright (C) 2010 Rodrigo Moya
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
 * Authors: Rodrigo Moya <rodrigo@gnome.org>
 */

#include "cc-setting-editor.h"

int
main (int argc, char *argv[])
{
  GtkWidget *dialog, *w;
  GObject *seditor;

  gtk_init (&argc, &argv);

  dialog = gtk_dialog_new_with_buttons ("Test Setting Editor", NULL, 0,
                                        GTK_STOCK_CLOSE, GTK_RESPONSE_CLOSE,
                                        NULL);
  g_signal_connect (dialog, "response", G_CALLBACK (gtk_main_quit), NULL);

  /* Create a default application selector */
  w = gtk_combo_box_new ();
  //seditor = cc_setting_editor_new_application ("x-scheme-handler/http", w);
  seditor = cc_setting_editor_new_application ("text/plain", w);
  gtk_box_pack_start (GTK_BOX (gtk_dialog_get_content_area (GTK_DIALOG (dialog))), w, TRUE, FALSE, 3);

  /* Run the dialog */
  gtk_widget_show_all (dialog);
  gtk_main ();

  return 0;
}
