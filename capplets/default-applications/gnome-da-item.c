/*
 *  Authors: Luca Cavalli <loopback@slackit.org>
 *
 *  Copyright 2005-2006 Luca Cavalli
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of version 2 of the GNU General Public License
 *  as published by the Free Software Foundation
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Street #330, Boston, MA 02111-1307, USA.
 *
 */

#include "gnome-da-capplet.h"
#include "gnome-da-item.h"

GnomeDAWebItem*
gnome_da_web_item_new (void)
{
    GnomeDAWebItem *item = NULL;

    item = g_new0 (GnomeDAWebItem, 1);

    return item;
}

GnomeDAMailItem*
gnome_da_mail_item_new (void)
{
    GnomeDAMailItem *item = NULL;

    item = g_new0 (GnomeDAMailItem, 1);

    return item;
}

GnomeDATermItem*
gnome_da_term_item_new (void)
{
    GnomeDATermItem *item = NULL;

    item = g_new0 (GnomeDATermItem, 1);

    return item;
}

void
gnome_da_web_item_free (GnomeDAWebItem *item)
{
    g_return_if_fail (item != NULL);

    g_free (item->generic.name);
    g_free (item->generic.executable);
    g_free (item->generic.command);
    g_free (item->generic.icon_name);
    g_free (item->generic.icon_path);

    g_free (item->tab_command);
    g_free (item->win_command);

    g_free (item);
}

void
gnome_da_mail_item_free (GnomeDAMailItem *item)
{
    g_return_if_fail (item != NULL);

    g_free (item->generic.name);
    g_free (item->generic.executable);
    g_free (item->generic.command);
    g_free (item->generic.icon_name);
    g_free (item->generic.icon_path);

    g_free (item);
}

void
gnome_da_term_item_free (GnomeDATermItem *item)
{
    g_return_if_fail (item != NULL);

    g_free (item->generic.name);
    g_free (item->generic.executable);
    g_free (item->generic.command);
    g_free (item->generic.icon_name);
    g_free (item->generic.icon_path);

    g_free (item->exec_flag);

    g_free (item);
}
