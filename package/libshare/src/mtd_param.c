#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdlib.h>
#include <errno.h>
#include "nlk_ipc.h"

/*
  Generate a table for a byte-wise 32-bit CRC calculation on the polynomial:
  x^32+x^26+x^23+x^22+x^16+x^12+x^11+x^10+x^8+x^7+x^5+x^4+x^2+x+1.

  Polynomials over GF(2) are represented in binary, one bit per coefficient,
  with the lowest powers in the most significant bit.  Then adding polynomials
  is just exclusive-or, and multiplying a polynomial by x is a right shift by
  one.  If we call the above polynomial p, and represent a byte as the
  polynomial q, also with the lowest power in the most significant bit (so the
  byte 0xb1 is the polynomial x^7+x^3+x+1), then the CRC is (q*x^32) mod p,
  where a mod b means the remainder after dividing a by b.

  This calculation is done using the shift-register method of multiplying and
  taking the remainder.  The register is initialized to zero, and for each
  incoming bit, x^32 is added mod p to the register if the bit is a one (where
  x^32 mod p is p+x^32 = x^26+...+1), and the register is multiplied mod p by
  x (which is shifting right by one and adding x^32 mod p if the bit shifted
  out is a one).  We start with the highest power (least significant bit) of
  q and repeat for all eight bits of q.

  The table is simply the CRC of all possible eight bit values.  This is all
  the information needed to generate CRC's on data a byte at a time for all
  combinations of CRC register values and incoming bytes.
*/
/* ========================================================================
 * Table of CRC-32's of all single-byte values (made by make_crc_table)
 */
static const unsigned long crc_table[256] = {
	0x00000000L, 0x77073096L, 0xee0e612cL, 0x990951baL, 0x076dc419L,
	0x706af48fL, 0xe963a535L, 0x9e6495a3L, 0x0edb8832L, 0x79dcb8a4L,
	0xe0d5e91eL, 0x97d2d988L, 0x09b64c2bL, 0x7eb17cbdL, 0xe7b82d07L,
	0x90bf1d91L, 0x1db71064L, 0x6ab020f2L, 0xf3b97148L, 0x84be41deL,
	0x1adad47dL, 0x6ddde4ebL, 0xf4d4b551L, 0x83d385c7L, 0x136c9856L,
	0x646ba8c0L, 0xfd62f97aL, 0x8a65c9ecL, 0x14015c4fL, 0x63066cd9L,
	0xfa0f3d63L, 0x8d080df5L, 0x3b6e20c8L, 0x4c69105eL, 0xd56041e4L,
	0xa2677172L, 0x3c03e4d1L, 0x4b04d447L, 0xd20d85fdL, 0xa50ab56bL,
	0x35b5a8faL, 0x42b2986cL, 0xdbbbc9d6L, 0xacbcf940L, 0x32d86ce3L,
	0x45df5c75L, 0xdcd60dcfL, 0xabd13d59L, 0x26d930acL, 0x51de003aL,
	0xc8d75180L, 0xbfd06116L, 0x21b4f4b5L, 0x56b3c423L, 0xcfba9599L,
	0xb8bda50fL, 0x2802b89eL, 0x5f058808L, 0xc60cd9b2L, 0xb10be924L,
	0x2f6f7c87L, 0x58684c11L, 0xc1611dabL, 0xb6662d3dL, 0x76dc4190L,
	0x01db7106L, 0x98d220bcL, 0xefd5102aL, 0x71b18589L, 0x06b6b51fL,
	0x9fbfe4a5L, 0xe8b8d433L, 0x7807c9a2L, 0x0f00f934L, 0x9609a88eL,
	0xe10e9818L, 0x7f6a0dbbL, 0x086d3d2dL, 0x91646c97L, 0xe6635c01L,
	0x6b6b51f4L, 0x1c6c6162L, 0x856530d8L, 0xf262004eL, 0x6c0695edL,
	0x1b01a57bL, 0x8208f4c1L, 0xf50fc457L, 0x65b0d9c6L, 0x12b7e950L,
	0x8bbeb8eaL, 0xfcb9887cL, 0x62dd1ddfL, 0x15da2d49L, 0x8cd37cf3L,
	0xfbd44c65L, 0x4db26158L, 0x3ab551ceL, 0xa3bc0074L, 0xd4bb30e2L,
	0x4adfa541L, 0x3dd895d7L, 0xa4d1c46dL, 0xd3d6f4fbL, 0x4369e96aL,
	0x346ed9fcL, 0xad678846L, 0xda60b8d0L, 0x44042d73L, 0x33031de5L,
	0xaa0a4c5fL, 0xdd0d7cc9L, 0x5005713cL, 0x270241aaL, 0xbe0b1010L,
	0xc90c2086L, 0x5768b525L, 0x206f85b3L, 0xb966d409L, 0xce61e49fL,
	0x5edef90eL, 0x29d9c998L, 0xb0d09822L, 0xc7d7a8b4L, 0x59b33d17L,
	0x2eb40d81L, 0xb7bd5c3bL, 0xc0ba6cadL, 0xedb88320L, 0x9abfb3b6L,
	0x03b6e20cL, 0x74b1d29aL, 0xead54739L, 0x9dd277afL, 0x04db2615L,
	0x73dc1683L, 0xe3630b12L, 0x94643b84L, 0x0d6d6a3eL, 0x7a6a5aa8L,
	0xe40ecf0bL, 0x9309ff9dL, 0x0a00ae27L, 0x7d079eb1L, 0xf00f9344L,
	0x8708a3d2L, 0x1e01f268L, 0x6906c2feL, 0xf762575dL, 0x806567cbL,
	0x196c3671L, 0x6e6b06e7L, 0xfed41b76L, 0x89d32be0L, 0x10da7a5aL,
	0x67dd4accL, 0xf9b9df6fL, 0x8ebeeff9L, 0x17b7be43L, 0x60b08ed5L,
	0xd6d6a3e8L, 0xa1d1937eL, 0x38d8c2c4L, 0x4fdff252L, 0xd1bb67f1L,
	0xa6bc5767L, 0x3fb506ddL, 0x48b2364bL, 0xd80d2bdaL, 0xaf0a1b4cL,
	0x36034af6L, 0x41047a60L, 0xdf60efc3L, 0xa867df55L, 0x316e8eefL,
	0x4669be79L, 0xcb61b38cL, 0xbc66831aL, 0x256fd2a0L, 0x5268e236L,
	0xcc0c7795L, 0xbb0b4703L, 0x220216b9L, 0x5505262fL, 0xc5ba3bbeL,
	0xb2bd0b28L, 0x2bb45a92L, 0x5cb36a04L, 0xc2d7ffa7L, 0xb5d0cf31L,
	0x2cd99e8bL, 0x5bdeae1dL, 0x9b64c2b0L, 0xec63f226L, 0x756aa39cL,
	0x026d930aL, 0x9c0906a9L, 0xeb0e363fL, 0x72076785L, 0x05005713L,
	0x95bf4a82L, 0xe2b87a14L, 0x7bb12baeL, 0x0cb61b38L, 0x92d28e9bL,
	0xe5d5be0dL, 0x7cdcefb7L, 0x0bdbdf21L, 0x86d3d2d4L, 0xf1d4e242L,
	0x68ddb3f8L, 0x1fda836eL, 0x81be16cdL, 0xf6b9265bL, 0x6fb077e1L,
	0x18b74777L, 0x88085ae6L, 0xff0f6a70L, 0x66063bcaL, 0x11010b5cL,
	0x8f659effL, 0xf862ae69L, 0x616bffd3L, 0x166ccf45L, 0xa00ae278L,
	0xd70dd2eeL, 0x4e048354L, 0x3903b3c2L, 0xa7672661L, 0xd06016f7L,
	0x4969474dL, 0x3e6e77dbL, 0xaed16a4aL, 0xd9d65adcL, 0x40df0b66L,
	0x37d83bf0L, 0xa9bcae53L, 0xdebb9ec5L, 0x47b2cf7fL, 0x30b5ffe9L,
	0xbdbdf21cL, 0xcabac28aL, 0x53b39330L, 0x24b4a3a6L, 0xbad03605L,
	0xcdd70693L, 0x54de5729L, 0x23d967bfL, 0xb3667a2eL, 0xc4614ab8L,
	0x5d681b02L, 0x2a6f2b94L, 0xb40bbe37L, 0xc30c8ea1L, 0x5a05df1bL,
	0x2d02ef8dL
};
/* =========================================================================*/ 
#define DO1(buf) crc = crc_table[((int)crc ^ (*buf++)) & 0xff] ^ (crc >> 8);
#define DO2(buf)  DO1(buf); DO1(buf);
#define DO4(buf)  DO2(buf); DO2(buf);
#define DO8(buf)  DO4(buf); DO4(buf);

/* =========================================================================*/ 
static unsigned long mtd_crc32( unsigned long crc, const unsigned char *buf, unsigned int len)
{
	if (buf == 0) {
	return 0L;
	}

	crc = crc ^ 0xffffffffL;
	while (len >= 8) {
		DO8(buf);
		len -= 8;
	}

	if (len) do {
		DO1(buf);
	} while (--len);

	return crc ^ 0xffffffffL;
}

int str2hex(char *str, int strlen, unsigned char *hex)
{
	int i = 0;
	unsigned char v = 0;

	for (i = 0; i < strlen; i++) {
		if ((str[i] >= '0') && (str[i] <= '9'))
			v = str[i] - '0';
		else if ((str[i] >= 'a') && (str[i] <= 'f'))
			v = str[i] - 'a' + 10;
		else if ((str[i] >= 'A') && (str[i] <= 'F'))
			v = str[i] - 'A' + 10;
		else 
			return -1;
		if (i%2)
			hex[i/2] += v;
		else
			hex[i/2] = v*16;
	}
	return 0;
}

#ifdef FLASH_4_RAM_32
#define MTD_PARAM_NAME "/dev/mtdblock2"
#define MTD_PARAM_SIZE 0x300
#define MTD_PARAM_OFFSET 0xfc00
#define MTD_PARAM_MAX 300
#define MTD_PARAM_PRINTF(fmt,args...) do{}while(0)
#else
#define MTD_PARAM_NAME "/dev/mtdblock4"
#define MTD_PARAM_SIZE 0x1000
#define MTD_PARAM_OFFSET 0x4000
#define MTD_PARAM_MAX 300

#define MTD_PARAM_PRINTF(fmt,args...) do{\
	igd_log("/tmp/mtd_param", "[%s():%d,%ld]:"fmt, __FUNCTION__, __LINE__, sys_uptime(), ##args);\
}while(0)
#endif

#define MTD_PARAM_ERR(fmt,args...) do{\
	igd_log("/tmp/mtd_err", "[%s():%d,%ld]:"fmt, __FUNCTION__, __LINE__, sys_uptime(), ##args);\
}while(0)

struct mtd_data {
	unsigned long crc;
	unsigned char data[MTD_PARAM_SIZE - sizeof(unsigned long)];
};

static int mtd_read_all(struct mtd_data *data)
{
	int fd = 0;
	unsigned long crc =0;

	fd = open(MTD_PARAM_NAME, O_RDONLY);
	if (fd < 0) {
		MTD_PARAM_ERR("open %s fail, %s\n", MTD_PARAM_NAME, strerror(errno));
		return -2;
	}
	lseek(fd, MTD_PARAM_OFFSET, SEEK_SET);

	if (igd_safe_read(fd, (unsigned char *)data, MTD_PARAM_SIZE) != sizeof(*data)) {
		close(fd);
		MTD_PARAM_ERR("read %s fail, %s\n", MTD_PARAM_NAME, strerror(errno));
		return -2;
	}
	close(fd);

	crc = mtd_crc32(0, data->data, sizeof(data->data));
	if((crc != data->crc) || (crc == 4294967295UL) || (crc == 0)){
		MTD_PARAM_PRINTF("crc err\n");
		return -1;
	}
	return 0;
}

static int mtd_data_group(char *group[])
{
	int i = 0, len = 0, ret = 0;
	struct mtd_data data;
	char *s = NULL, *e = NULL;

	memset(&data, 0, sizeof(data));
	ret = mtd_read_all(&data);
	if (ret < 0) {
		return ret;
	}

	s = (char *)data.data;
	e = (char *)(data.data + sizeof(data.data));
	while (s < e) {
		len = strlen(s);
		if (len <= 0)
			break;
		if (*(s + len) != '\0')
			break;
		if (!strchr(s, '='))
			break;
		group[i] = strdup(s);
		i++;
		s += (len + 1);
		if (i >= MTD_PARAM_MAX)
			break;
	}
	return i;
}

int mtd_get_val(char *name, char *val, int len)
{
	int nr = 0, i = 0, ret = -1;
	char *grp[MTD_PARAM_MAX] = {NULL,};
	char *ptr = NULL;
	
	nr = mtd_data_group(grp);
	if (nr <= 0) {
		MTD_PARAM_PRINTF("group nr is err, nr = %d\n", nr);
		return nr;
	}

	for (i = 0; i < nr; i++) {
		if (!strncmp(name, grp[i], strlen(name)))
			break;
	}
	if (i >= nr) {
		//MTD_PARAM_PRINTF("no find %s\n", name);
		goto err;
	}
	ptr = strchr(grp[i], '=');
	if (ptr == NULL) {
		MTD_PARAM_PRINTF("no find %s\n", grp[i]);
		goto err;
	}
	strncpy(val, ptr+1, len - 1);
	val[len - 1] = '\0';
	ret = 0;
err:
	for (i = 0; i < nr; i++) {
		//MTD_PARAM_PRINTF("i:%d, %s\n", i, grp[i]);
		free(grp[i]);
	}
	return ret;
}

int mtd_set_val(char *val)
{
	int nr = 0, i = 0, offset = 0, fd = 0;
	char *grp[MTD_PARAM_MAX + 1] = {NULL,};
	char *ptr = NULL;
	struct mtd_data data;

	ptr = strchr(val, '=');
	if (ptr == NULL) {
		MTD_PARAM_PRINTF("val err,%s\n", val);
		return -1;
	}

	nr = mtd_data_group(grp);
	if (nr == -2) {
		return -1;
	} else if (nr < 0) {
		MTD_PARAM_PRINTF("group nr is err, nr = %d\n", nr);
		nr = 0;
	}

	for (i = 0; i < nr; i++) {
		if (!strncmp(val, grp[i], ptr - val)) {
			free(grp[i]);
			grp[i] = strdup(val);
			break;
		}
	}
	if (i >= nr) {
		grp[nr] = strdup(val);
		nr++;
	}
	memset(&data, 0, sizeof(data));
	for (i = 0; i < nr; i++) {
		if ((strlen(grp[i]) + offset + 1) > sizeof(data.data)) {
			MTD_PARAM_PRINTF("data too large\n");
			free(grp[i]);
			continue;
		}
		offset += sprintf((char *)&data.data[offset], "%s", grp[i]);
		data.data[offset] = '\0';
		offset++;
		free(grp[i]);
	}
	data.crc = mtd_crc32(0, data.data, sizeof(data.data));

	fd = open(MTD_PARAM_NAME, O_RDWR | O_EXCL, 0444);
	if (fd < 0) {
		MTD_PARAM_ERR("open %s fail, %s\n", MTD_PARAM_NAME, strerror(errno));
		return -1;
	}
	lseek(fd, MTD_PARAM_OFFSET, SEEK_SET);
	if (igd_safe_write(fd, (unsigned char *)&data, MTD_PARAM_SIZE) <= 0){
		MTD_PARAM_ERR("write %s fail, %s\n", MTD_PARAM_NAME, strerror(errno));
		close(fd);
		return -1;
	}
	sync();
	close(fd);
	return 0;
}

int mtd_clear_data(void)
{
	int fd = 0;
	struct mtd_data data;

	memset(&data, 0, sizeof(data));
	data.crc = mtd_crc32(0, data.data, sizeof(data.data));
	fd = open(MTD_PARAM_NAME, O_RDWR | O_EXCL, 0444);
	if (fd < 0) {
		MTD_PARAM_ERR("open %s fail, %s\n", MTD_PARAM_NAME, strerror(errno));
		return -1;
	}
	lseek(fd, MTD_PARAM_OFFSET, SEEK_SET);
	if (igd_safe_write(fd, (unsigned char *)&data, MTD_PARAM_SIZE) <= 0){
		MTD_PARAM_ERR("write %s fail, %s\n", MTD_PARAM_NAME, strerror(errno));
		close(fd);
		return -1;
	}
	sync();
	close(fd);
	return 0;
}

unsigned int get_devid(void)
{
	char hwid[32];

	memset(hwid, 0, sizeof(hwid));
	if (mtd_get_val("hw_id", hwid, sizeof(hwid))) {
		return 0;
	}
	return(unsigned int)atoll(hwid);
}

int set_hw_id(unsigned int hwid)
{
	char data[128] = {0,};

	memset(data, 0, sizeof(data));
	sprintf(data, "hw_id=%u", hwid);
	return mtd_set_val(data);
}

int get_key(void)
{
	char key_s[32];
	
	memset(key_s, 0, sizeof(key_s));
	if (mtd_get_val("szKey", key_s, sizeof(key_s)))
		return -1;
	printf("key is %s\n", key_s);
	return 0;
}

int set_key(char *key)
{
	int len;
	char cmd[64];
	
	len = strlen(key);
	if (len == 0 || len > 31)
		return -1;
	snprintf(cmd, sizeof(cmd), "szKey=%s", key);
	return mtd_set_val(cmd);
}

int get_pwd(void)
{
	char pwd_s[64];
	
	memset(pwd_s, 0, sizeof(pwd_s));
	if (mtd_get_val("szSecret", pwd_s, sizeof(pwd_s)))
		return -1;
	printf("pwd is %s\n", pwd_s);
	return 0;
}

int set_pwd(char *pwd)
{
	int len;
	char cmd[128];
	
	len = strlen(pwd);
	if (len == 0 || len > 63)
		return -1;
	snprintf(cmd, sizeof(cmd), "szSecret=%s", pwd);
	return mtd_set_val(cmd);
}

int get_mod(void)
{
	char mod_s[32];
	
	memset(mod_s, 0, sizeof(mod_s));
	if (mtd_get_val("PRODUCT", mod_s, sizeof(mod_s)))
		return -1;
	printf("mod is %s\n", mod_s);
	return 0;
}

int set_mod(char *mod)
{
	char cmd[64];
	char *res = NULL;

	res = read_firmware("VENDOR");
	if (!res || !strlen(res)) {
		printf("get vendor error\n");
		return -1;
	}
	if (!strlen(mod) || ((strlen(res) + strlen(mod) + 1) > 15)) {
		printf("too long limit %d\n", (14 - strlen(res)));
		return -1;
	}
	snprintf(cmd, sizeof(cmd), "PRODUCT=%s", mod);
	return mtd_set_val(cmd);
}

int get_devname(void)
{
	char name_s[32];
	
	memset(name_s, 0, sizeof(name_s));
	if (mtd_get_val("devname", name_s, sizeof(name_s)))
		return -1;
	printf("devname is %s\n", name_s);
	return 0;
}

int set_devname(char *devname)
{
	int len;
	char cmd[64];
	
	len = strlen(devname);
	if (len == 0 || len > 31)
		return -1;
	snprintf(cmd, sizeof(cmd), "devname=%s", devname);
	return mtd_set_val(cmd);
}

int get_platinfo(unsigned char *plat)
{
	char plat_s[32];
	
	memset(plat_s, 0, sizeof(plat_s));
	if (mtd_get_val("plat", plat_s, sizeof(plat_s)))
		return -1;
	*plat = (unsigned char)atoll(plat_s);
	if (*plat > 254) {
		MTD_PARAM_PRINTF("plat %d error\n", *plat);
		return -1;
	}
	return 0;
}

int set_platinfo(unsigned char plat)
{
	char plat_s[32];
	
	if (plat > 254) {
		MTD_PARAM_PRINTF("plat %d error\n", plat);
		return -1;
	}
	memset(plat_s, 0, sizeof(plat_s));
	sprintf(plat_s, "plat=%d", plat);
	return mtd_set_val(plat_s);
}

int set_ruser(char *user)
{
	int len;
	char cmd[64];
	
	len = strlen(user);
	if (len == 0 || len > 31)
		return -1;
	snprintf(cmd, sizeof(cmd), "RUSER=%s", user);
	return mtd_set_val(cmd);
}

int get_ruser(void)
{
	char user_s[32];
	
	memset(user_s, 0, sizeof(user_s));
	if (mtd_get_val("RUSER", user_s, sizeof(user_s)))
		return -1;
	printf("RUSER is %s\n", user_s);
	return 0;
}

int set_rpwd(char *pwd)
{
	int len;
	char cmd[64];
	
	len = strlen(pwd);
	if (len == 0 || len > 31)
		return -1;
	snprintf(cmd, sizeof(cmd), "RPWD=%s", pwd);
	return mtd_set_val(cmd);
}

int get_rpwd(void)
{
	char pwd_s[32];
	
	memset(pwd_s, 0, sizeof(pwd_s));
	if (mtd_get_val("RPWD", pwd_s, sizeof(pwd_s)))
		return -1;
	printf("RPWD is %s\n", pwd_s);
	return 0;
}

int set_wuser(char *user)
{
	int len;
	char cmd[64];
	
	len = strlen(user);
	if (len == 0 || len > 31)
		return -1;
	snprintf(cmd, sizeof(cmd), "WUSER=%s", user);
	return mtd_set_val(cmd);
}

int get_wuser(void)
{
	char user_s[32];
	
	memset(user_s, 0, sizeof(user_s));
	if (mtd_get_val("WUSER", user_s, sizeof(user_s)))
		return -1;
	printf("WUSER is %s\n", user_s);
	return 0;
}

int set_wpwd(char *pwd)
{
	int len;
	char cmd[64];
	
	len = strlen(pwd);
	if (len == 0 || len > 31)
		return -1;
	snprintf(cmd, sizeof(cmd), "WPWD=%s", pwd);
	return mtd_set_val(cmd);
}

int get_wpwd(void)
{
	char pwd_s[32];
	
	memset(pwd_s, 0, sizeof(pwd_s));
	if (mtd_get_val("WPWD", pwd_s, sizeof(pwd_s)))
		return -1;
	printf("WPWD is %s\n", pwd_s);
	return 0;
}

int set_apwd(char *pwd)
{
	int len;
	char cmd[64];
	
	len = strlen(pwd);
	if (len == 0 || len > 31)
		return -1;
	snprintf(cmd, sizeof(cmd), "LOGIN_PWD=%s", pwd);
	return mtd_set_val(cmd);
}

int get_apwd(void)
{
	char pwd_s[32];
	
	memset(pwd_s, 0, sizeof(pwd_s));
	if (mtd_get_val("LOGIN_PWD", pwd_s, sizeof(pwd_s)))
		return -1;
	printf("LOGIN_PWD is %s\n", pwd_s);
	return 0;
}

int set_auser(char *user)
{
	int len;
	char cmd[64];
	
	len = strlen(user);
	if (len == 0 || len > 31)
		return -1;
	snprintf(cmd, sizeof(cmd), "LOGIN_USER=%s", user);
	return mtd_set_val(cmd);
}

int get_auser(void)
{
	char user_s[32];
	
	memset(user_s, 0, sizeof(user_s));
	if (mtd_get_val("LOGIN_USER", user_s, sizeof(user_s)))
		return -1;
	printf("LOGIN_USER is %s\n", user_s);
	return 0;
}

int get_sysflag(unsigned char bit)
{
	char flag[32];
	unsigned int intflag;

	if (bit >= SYSFLAG_MAX) {
		MTD_PARAM_PRINTF("bit:%d > %d\n", bit, SYSFLAG_MAX);
		return -1;
	}
	memset(flag, 0, sizeof(flag));
	if (mtd_get_val("sys_flag", flag, sizeof(flag))) {
		return -1;
	}
	intflag = (unsigned int)atoll(flag);
	return ((intflag>>bit)&0x1);
}

int set_sysflag(unsigned char bit, unsigned char fg)
{
	char flag[32];
	unsigned int intflag;

	if (bit >= SYSFLAG_MAX) {
		MTD_PARAM_PRINTF("bit:%d > %d\n", bit, SYSFLAG_MAX);
		return -1;
	}

	memset(flag, 0, sizeof(flag));
	if (mtd_get_val("sys_flag", flag, sizeof(flag))) {
		intflag = 0;
	} else {
		intflag = (unsigned int)atoll(flag);
	}
	if (fg) {
		intflag |= (1 << bit);
	} else {
		intflag &= ~(1 << bit);
	}
	memset(flag, 0, sizeof(flag));
	sprintf(flag, "sys_flag=%u", intflag);
	return mtd_set_val(flag);
}

int get_flashid(unsigned char *fid)
{
	char id[33];

	memset(id, 0, sizeof(id));
	if (mtd_get_val("flash_id", id, sizeof(id))) {
		return -1;
	}
	return str2hex(id, 32, fid);
}

int set_flashid(unsigned char *fid)
{
	int i = 0, offset = 0;
	char data[128] = {0,};

	memset(data, 0, sizeof(data));
	offset = sprintf(data, "flash_id=");
	for (i = 0; i < 16; i++) {
		offset += sprintf(&data[offset], "%02X", fid[i]);
	}
	return mtd_set_val(data);
}
