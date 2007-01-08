/* mixer-support.h
 *
 * Copyright (C) 2007 Jan Arne Petersen <jap@gnome.org>
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


#ifndef __GNOME_SETTINGS_MIXER_SUPPORT__
#define __GNOME_SETTINGS_MIXER_SUPPORT__

#include <gtk/gtktreemodel.h>
#include <gst/interfaces/mixer.h>

G_BEGIN_DECLS

enum {
  MIXER_DEVICE_MODEL_NAME_COLUMN,
  MIXER_DEVICE_MODEL_DEVICE_COLUMN,
  MIXER_DEVICE_MODEL_MIXER_COLUMN,
  MIXER_DEVICE_MODEL_COLUMN_COUNT
};

enum {
  MIXER_TRACKS_MODEL_LABEL_COLUMN,
  MIXER_TRACKS_MODEL_COLUMN_COUNT
};

GtkTreeModel *create_mixer_device_tree_model (void);
GtkTreeModel *create_mixer_tracks_tree_model_for_mixer (GstMixer *mixer);

G_END_DECLS

#endif
