/*
 * uuid.h -- private header file for uuids
 * 
 * Copyright (C) 1996, 1997 Theodore Ts'o.
 *
 * %Begin-Header%
 * This file may be redistributed under the terms of the GNU Public
 * License.
 * %End-Header%
 */

#include <sys/types.h>
#include <glib.h>

#include "uuid.h"

/*
 * Offset between 15-Oct-1582 and 1-Jan-70
 */
#define TIME_OFFSET_HIGH 0x01B21DD2
#define TIME_OFFSET_LOW  0x13814000

struct uuid {
	guint32	time_low;
	guint16	time_mid;
        guint16	time_hi_and_version;
	guint16	clock_seq;
        guint8	node[6];
};


/*
 * prototypes
 */
void uuid_pack(struct uuid *uu, uuid_t ptr);
void uuid_unpack(uuid_t in, struct uuid *uu);




