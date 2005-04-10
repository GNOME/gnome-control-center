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

#include "libgswitchit/gswitchit_config.h"

#include "capplet-util.h"

#include "gnome-keyboard-properties-xkb.h"
#include "libkbdraw/keyboard-drawing.h"

#ifdef HAVE_X11_EXTENSIONS_XKB_H
#include "X11/XKBlib.h"
/**
 * BAD STYLE: Taken from xklavier_private_xkb.h
 * Any ideas on architectural improvements are WELCOME
 */
extern Bool _XklXkbConfigPrepareNative( const XklConfigRecPtr data, XkbComponentNamesPtr componentNamesPtr );
extern void _XklXkbConfigCleanupNative( XkbComponentNamesPtr componentNamesPtr );
/* */
#endif

static KeyboardDrawingGroupLevel groupsLevels[] = {{0,1},{0,3},{0,0},{0,2}};
static KeyboardDrawingGroupLevel * pGroupsLevels[] = {
groupsLevels, groupsLevels+1, groupsLevels+2, groupsLevels+3 };

GtkWidget*
xkb_layout_preview_create_widget (GladeXML * chooserDialog)
{
  GtkWidget *kbdraw = keyboard_drawing_new ();
  
  keyboard_drawing_set_groups_levels (KEYBOARD_DRAWING (kbdraw), pGroupsLevels);
  return kbdraw;
}

void
xkb_layout_preview_update (GladeXML * chooserDialog)
{
#ifdef HAVE_X11_EXTENSIONS_XKB_H
  GtkWidget *chooser = CWID ( "xkb_layout_chooser");
  GtkWidget *availableLayoutsTree = CWID ("xkb_layouts_available");
  GtkTreeSelection *selection =
    gtk_tree_view_get_selection (GTK_TREE_VIEW (availableLayoutsTree));
  GtkTreeIter selectedIter;
  GtkTreeModel *model;
  GtkWidget *kbdraw = GTK_WIDGET (g_object_get_data (G_OBJECT (chooser), "kbdraw"));
  if (kbdraw != NULL &&
      gtk_tree_selection_get_selected (selection, &model, &selectedIter))
    {
      gchar *id;
      XklConfigRec data;
      char **p, *layout, *variant;
      int i;
      XkbComponentNamesRec componentNames;

      gtk_tree_model_get (model, &selectedIter, AVAIL_LAYOUT_TREE_COL_ID, &id, -1);
      XklConfigRecInit (&data);
      if (XklConfigGetFromServer (&data))
        {
          if( ( p = data.layouts ) != NULL )
          {
            for( i = data.numLayouts; --i >= 0; )
              free( *p++ );
          }

          if( ( p = data.variants ) != NULL )
          {
            for( i = data.numVariants; --i >= 0; )
              free( *p++ );
          }
          data.numLayouts =
          data.numVariants = 1;
          data.layouts = realloc (data.layouts, sizeof (char*));
          data.variants = realloc (data.variants, sizeof (char*));
          if (GSwitchItKbdConfigSplitItems (id, &layout, &variant)
              && variant != NULL)
            {
              data.layouts[0] = (layout == NULL) ? NULL : strdup (layout);
              data.variants[0] = (variant == NULL) ? NULL : strdup (variant);
            } else
            {
              data.layouts[0] = (id == NULL) ? NULL : strdup (id);
              data.variants[0] = NULL;
            }
          if (_XklXkbConfigPrepareNative (&data, &componentNames))
            {
              keyboard_drawing_set_keyboard (KEYBOARD_DRAWING (kbdraw), &componentNames);

              _XklXkbConfigCleanupNative( &componentNames );
            }
        }
      XklConfigRecDestroy (&data);
    }
#endif
}
