/* -*- mode: c; style: linux -*- */
/* -*- c-basic-offset: 2 -*- */

/* pipeline-tests.h
 * Copyright (C) 2002 Jan Schmidt
 * Copyright (C) 2006 Jürg Billeter <j@bitron.ch>
 *
 * Written by: Jan Schmidt <thaytan@mad.scientist.com>
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
#ifndef __PIPELINE_TESTS_HH__
#define __PIPELINE_TESTS_HH__

#include <gtk/gtk.h>
#include <glade/glade.h>

void user_test_pipeline(GladeXML *interface_xml,
		    GtkWindow *parent,
		    const gchar *pipeline);

			
#endif	
