/* acme-fb-level.c

   Copyright (C) 2002, 2003 Bastien Nocera

   The Gnome Library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Library General Public License as
   published by the Free Software Foundation; either version 2 of the
   License, or (at your option) any later version.

   The Gnome Library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Library General Public License for more details.

   You should have received a copy of the GNU Library General Public
   License along with the Gnome Library; see the file COPYING.LIB.  If not,
   write to the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
   Boston, MA 02111-1307, USA.

   Author: Bastien Nocera <hadess@hadess.net>
 */

#include "config.h"
#include "acme-fb-level.h"

#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <sys/ioctl.h>
#include <linux/fb.h>
#include <linux/pmu.h>
#include <errno.h>

#ifndef FBIOBLANK
#define FBIOBLANK      0x4611          /* 0 or vesa-level+1 */
#endif
                                                                                
#ifndef PMU_IOC_GRAB_BACKLIGHT
#define PMU_IOC_GRAB_BACKLIGHT  _IOR('B', 6, 0)
#endif

G_DEFINE_TYPE (AcmeFblevel, acme_fblevel, ACME_TYPE_FBLEVEL)

struct AcmeFblevelPrivate {
	int pmu_fd;
	int saved_level;
};

static GObjectClass *parent_class = NULL;

static void
acme_fblevel_finalize (GObject *obj_self)
{
	AcmeFblevel *self = ACME_FBLEVEL (obj_self);
	gpointer priv = self->_priv;

	if (G_OBJECT_CLASS(parent_class)->finalize)
		(* G_OBJECT_CLASS(parent_class)->finalize)(obj_self);
	g_free (priv);

	return;
}

static void
acme_fblevel_class_init (AcmeFblevelClass *klass)
{
	GObjectClass *g_object_class = (GObjectClass*) klass;
	parent_class = g_type_class_ref (G_TYPE_OBJECT);
	g_object_class->finalize = acme_fblevel_finalize;

	return;
}

static void
acme_fblevel_init (AcmeFblevel *fblevel)
{
	fblevel->_priv = g_new0 (AcmeFblevelPrivate, 1);
	fblevel->level = 0;
	fblevel->dim = FALSE;
	fblevel->_priv->pmu_fd = -1;
	fblevel->_priv->saved_level = 0;

	return;
}

int
acme_fblevel_get_level (AcmeFblevel *self)
{
	int level;
	ioctl (self->_priv->pmu_fd,
			PMU_IOC_GET_BACKLIGHT, &level);
	return level;
}

void
acme_fblevel_set_level (AcmeFblevel *self, int val)
{
	int level;

	level = CLAMP (val, 0, 15);

	ioctl (self->_priv->pmu_fd,
			PMU_IOC_SET_BACKLIGHT, &level);
	self->level = level;
}

gboolean
acme_fblevel_get_dim (AcmeFblevel *self)
{
	return self->dim;
}

void
acme_fblevel_set_dim (AcmeFblevel *self, gboolean val)
{
	if (self->dim == FALSE && val == TRUE)
	{
		self->_priv->saved_level = acme_fblevel_get_level(self);
		acme_fblevel_set_level (self, 1);
		self->dim = TRUE;
	} else if (self->dim == TRUE && val == FALSE) {
		acme_fblevel_set_level (self, self->_priv->saved_level);
		self->dim = FALSE;
	}
}

AcmeFblevel *
acme_fblevel_new (void)
{
	AcmeFblevel *self;
	int fd, foo;

	if (g_file_test ("/dev/pmu", G_FILE_TEST_EXISTS) == FALSE)
		return NULL;

	if (acme_fblevel_is_powerbook () == FALSE)
		return NULL;

	self = ACME_FBLEVEL (g_object_new (ACME_TYPE_FBLEVEL, NULL));
	/* This function switches the kernel backlight control off.
	 * This is part of the PPC kernel branch since version
	 * 2.4.18-rc2-benh. It does nothing with older kernels.
	 * For those kernels a separate kernel patch is nessecary to
	 * get backlight control in user space.
	 *
	 * Notice nicked from pbbuttons*/
	fd  = open ("/dev/pmu", O_RDWR);
	/* We can't emit the signal yet, the signal isn't connected! */
	if (fd < 0)
		return NULL;

	foo = ioctl(fd, PMU_IOC_GRAB_BACKLIGHT, 0);
	self->_priv->pmu_fd = fd;
	return self;
}

gboolean
acme_fblevel_is_powerbook (void)
{
	FILE *fd;
	char str[2048];
	gboolean found = FALSE;

	fd = fopen ("/proc/cpuinfo", "r");
	while (!feof (fd) && found == FALSE)
	{
		fread (str, 1, 2048, fd);
		if (strstr (str, "PowerBook") != NULL)
			found = TRUE;
	}

	fclose (fd);

	return found;
}

