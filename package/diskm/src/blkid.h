/*
 * blkid.h - Interface for libblkid, a library to identify block devices
 *
 * Copyright (C) 2001 Andreas Dilger
 * Copyright (C) 2003 Theodore Ts'o
 * Copyright (C) 2008 Karel Zak <kzak@redhat.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307  USA
 */

#ifndef _BLKID_BLKID_H
#define _BLKID_BLKID_H

#include <stdint.h>
#include <sys/types.h>

#include "libblkid-tiny.h"

#ifdef __cplusplus
extern "C" {
#endif

#define BLKID_VERSION   "2.21.0"
#define BLKID_DATE      "25-May-2012"

/**
 * blkid_dev:
 *
 * The device object keeps information about one device
 */
typedef struct blkid_struct_dev *blkid_dev;

/**
 * blkid_cache:
 *
 * information about all system devices
 */
typedef struct blkid_struct_cache *blkid_cache;

/**
 * blkid_probe:
 *
 * low-level probing setting
 */
typedef struct blkid_struct_probe *blkid_probe;

/**
 * blkid_topology:
 *
 * device topology information
 */
typedef struct blkid_struct_topology *blkid_topology;

/**
 * blkid_partlist
 *
 * list of all detected partitions and partitions tables
 */
typedef struct blkid_struct_partlist *blkid_partlist;

/**
 * blkid_partition:
 *
 * information about a partition
 */
typedef struct blkid_struct_partition *blkid_partition;

/**
 * blkid_parttable:
 *
 * information about a partition table
 */
typedef struct blkid_struct_parttable *blkid_parttable;

/**
 * blkid_loff_t:
 *
 * 64-bit signed number for offsets and sizes
 */
typedef int64_t blkid_loff_t;

/**
 * blkid_tag_iterate:
 *
 * tags iterator for high-level (blkid_cache) API
 */
typedef struct blkid_struct_tag_iterate *blkid_tag_iterate;

/**
 * blkid_dev_iterate:
 *
 * devices iterator for high-level (blkid_cache) API
 */
typedef struct blkid_struct_dev_iterate *blkid_dev_iterate;

/*
 * Flags for blkid_get_dev
 *
 * BLKID_DEV_CREATE	Create an empty device structure if not found
 *			in the cache.
 * BLKID_DEV_VERIFY	Make sure the device structure corresponds
 *			with reality.
 * BLKID_DEV_FIND	Just look up a device entry, and return NULL
 *			if it is not found.
 * BLKID_DEV_NORMAL	Get a valid device structure, either from the
 *			cache or by probing the device.
 */
#define BLKID_DEV_FIND		0x0000
#define BLKID_DEV_CREATE	0x0001
#define BLKID_DEV_VERIFY	0x0002
#define BLKID_DEV_NORMAL	(BLKID_DEV_CREATE | BLKID_DEV_VERIFY)



#define BLKID_SUBLKS_LABEL	(1 << 1) /* read LABEL from superblock */
#define BLKID_SUBLKS_LABELRAW	(1 << 2) /* read and define LABEL_RAW result value*/
#define BLKID_SUBLKS_UUID	(1 << 3) /* read UUID from superblock */
#define BLKID_SUBLKS_UUIDRAW	(1 << 4) /* read and define UUID_RAW result value */
#define BLKID_SUBLKS_TYPE	(1 << 5) /* define TYPE result value */
#define BLKID_SUBLKS_SECTYPE	(1 << 6) /* define compatible fs type (second type) */
#define BLKID_SUBLKS_USAGE	(1 << 7) /* define USAGE result value */
#define BLKID_SUBLKS_VERSION	(1 << 8) /* read FS type from superblock */
#define BLKID_SUBLKS_MAGIC	(1 << 9) /* define SBMAGIC and SBMAGIC_OFFSET */

#define BLKID_SUBLKS_DEFAULT	(BLKID_SUBLKS_LABEL | BLKID_SUBLKS_UUID | \
				 BLKID_SUBLKS_TYPE | BLKID_SUBLKS_SECTYPE)


/**
 * BLKID_FLTR_NOTIN
 */
#define BLKID_FLTR_NOTIN		1
/**
 * BLKID_FLTR_ONLYIN
 */
#define BLKID_FLTR_ONLYIN		2


/* partitions probing flags */
#define BLKID_PARTS_FORCE_GPT		(1 << 1)
#define BLKID_PARTS_ENTRY_DETAILS	(1 << 2)
#define BLKID_PARTS_MAGIC		(1 << 3)

/*
 * Deprecated functions/macros
 */

#ifdef __cplusplus
}
#endif

#endif /* _BLKID_BLKID_H */
