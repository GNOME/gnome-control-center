/* -*- mode: c; style: linux -*- */

/* gnome-keyboard-properties-xkb.c
 * Copyright (C) 2003 Sergey V. Oudaltsov
 *
 * Written by: Sergey V. Oudaltsov <svu@users.sourceforge.net>
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
#  include <config.h>
#endif

#include <gnome.h>
#include <glade/glade.h>

#include "capplet-util.h"

#include "gnome-keyboard-properties-xkb.h"
#include "libkbdraw/keyboard-drawing.h"

void
init_preview (GladeXML * dialog)
{
  GtkWidget *frame = WID ("preview_frame");
  GtkWidget *kbdraw = keyboard_drawing_new ();
  keyboard_drawing_set_track_group (KEYBOARD_DRAWING (kbdraw), TRUE);
  keyboard_drawing_set_track_config (KEYBOARD_DRAWING (kbdraw), TRUE);
  gtk_container_add (GTK_CONTAINER (frame), kbdraw);
  gtk_widget_show (kbdraw);
}
