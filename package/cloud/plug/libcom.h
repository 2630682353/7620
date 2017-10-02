#ifndef __LIBCOM_H__
#define __LIBCOM_H__

extern void igd_log(char *file, const char *fmt, ...);
extern int get_devid(unsigned int *devid);
extern int set_devid(unsigned int hwid);
extern long sys_uptime(void);
extern int read_mac(unsigned char *mac);
extern char *read_firmware(char *key);
extern int get_cpuinfo(char *data, int data_len);
extern int get_meminfo(char *data, int data_len);
extern int igd_md5sum(char *file, void *md5);

#define IGD_BITS_LONG (sizeof(long)*8)
#ifndef BITS_TO_LONGS
#define BITS_TO_LONGS(n) ((n + (sizeof(long)*8) - 1)/ (sizeof(long)* 8))
#endif
#define MAC_SPLIT(mac) mac[0],mac[1],mac[2],mac[3],mac[4],mac[5]

static inline int igd_test_bit(int nr, unsigned long *bit)
{
	unsigned long mask = 1UL << (nr % IGD_BITS_LONG);
	unsigned long *p = bit + nr / IGD_BITS_LONG;

	return (*p & mask) != 0;
}

static inline int igd_set_bit(int nr, unsigned long *bit)
{
	unsigned long mask = 1UL << (nr % IGD_BITS_LONG);
	unsigned long *p = bit + nr / IGD_BITS_LONG;
	*p |= mask;
	return 0;
}

static inline int igd_clear_bit(int nr, unsigned long *bit)
{
	unsigned long mask = 1UL << (nr % IGD_BITS_LONG);
	unsigned long *p = bit + nr / IGD_BITS_LONG;
	*p &= ~mask;
	return 0;
}

#endif

