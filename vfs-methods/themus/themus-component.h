/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* 
 * Copyright (C) 2002 James Willcox
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 * Author:  James Willcox  <jwillcox@gnome.org>
 */

#ifndef THEMUS_COMPONENT_H
#define THEMUS_COMPONENT_H

#include <bonobo/bonobo-object.h>

GType themus_component_get_type (void);

#define TYPE_THEMUS_COMPONENT		     (themus_component_get_type ())
#define THEMUS_COMPONENT(obj)	     	     (G_TYPE_CHECK_INSTANCE_CAST ((obj), TYPE_THEMUS_COMPONENT, ThemusComponent))
#define THEMUS_COMPONENT_CLASS(klass)        (G_TYPE_CHECK_CLASS_CAST ((klass), TYPE_THEMUS_COMPONENT, ThemusComponentClass))
#define IS_THEMUS_COMPONENT(obj)	     (G_TYPE_CHECK_INSTANCE_TYPE ((obj), TYPE_THEMUS_COMPONENT))
#define IS_THEMUS_COMPONENT_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), TYPE_THEMUS_COMPONENT))

typedef struct {
	BonoboObject parent;
} ThemusComponent;

typedef struct {
	BonoboObjectClass parent;

	POA_Bonobo_Listener__epv epv;
} ThemusComponentClass;

#endif /* THEMUS_COMPONENT_H */
