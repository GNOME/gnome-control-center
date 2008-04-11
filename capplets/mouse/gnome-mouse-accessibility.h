/*
 * Copyright (C) 2007 Gerd Kohlberger
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef __GNOME_MOUSE_A11Y_H
#define __GNOME_MOUSE_A11Y_H

#include <glade/glade.h>
#include <gconf/gconf-client.h>

G_BEGIN_DECLS

void setup_accessibility (GladeXML *dialog, GConfClient *client);

G_END_DECLS

#endif /* __GNOME_MOUSE_A11Y_H */
