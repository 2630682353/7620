#ifndef _SG_IO_H
#define _SG_IO_H

extern int sg_get_disk_vendor_serical(char *devname, char *vendor, char *serical);
extern int sg_get_disk_space(char *devname, unsigned long long *capacity);
extern int ioctl_get_disk_space(char *devname, unsigned long long *capacity);
extern int sg_sleep_disk(char *devname, int action);
#endif
