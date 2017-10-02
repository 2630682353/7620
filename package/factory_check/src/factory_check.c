#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include "igd_share.h"
#include "nlk_ipc.h"

extern int set_flashid(unsigned char *fid);
extern int set_hw_id(unsigned int hwid);
int str2mac(char *str, unsigned char *mac)
{
	int i = 0, j = 0;
	unsigned char v = 0;

	for (i = 0; i < 17; i++) {
		if (str[i] >= '0' && str[i] <= '9') {
			v = str[i] - '0';
		} else if (str[i] >= 'a' && str[i] <= 'f') {
			v = str[i] - 'a' + 10;
		} else if (str[i] >= 'A' && str[i] <= 'F') {
			v = str[i] - 'A' + 10;
		} else if (str[i] == ':' || str[i] == '-' ||
					str[i] == ',' || str[i] == '\r' ||
					str[i] == '\n') {
			continue;
		} else if (str[i] == '\0') {
			return 0;
		} else {
			return -1;
		}
		if (j%2)
			mac[j/2] += v;
		else
			mac[j/2] = v*16;
		j++;
		if (j/2 > 5)
			break;
	}
	return 0;
}

int factory_check_led(char *_switch)
{
	if (!strcmp(_switch, "on")) {
		set_gpio_led(NLK_ACTION_ADD);
	} else if (!strcmp(_switch, "off")) {
		set_gpio_led(NLK_ACTION_DEL);
	} else {
		return -1;
	}
	return 0;
}
int factory_check_key(void)
{
	int fd = 0;
	char buff[32];
	int count = 0;
	int bytes = 0;
	
	memset(buff, 0, sizeof(buff));
	while(1) {
		fd = open("/proc/intreg", O_RDONLY);
		if (fd < 0)
			return -1;
		bytes = read(fd, buff, sizeof(buff));
		if (bytes < 0) {
			if (fd > 0) {
				close(fd);
			}
			return -1;
		}
		if(strstr(buff, "1")) {
			printf("pass\n");
			goto pass;
		}
		close(fd);
		usleep(10000);
		count += 1;
		if (count > 1000) {
			printf("fail\n");
			return 0;
		}
	}
pass:
	close(fd);
	return 0;
}
int factory_check_reset(void)
{
	unsigned char *ptr = (unsigned char*)" ";
	if(set_hw_id(0) || set_flashid(ptr) || system("mtd erase rootfs_data")) {
		printf("fail\n");
		return 0;
	}
	printf("pass\n");
	return 0;
}
int factory_check_ver(void)
{
	printf("%s\n", read_firmware("CURVER") ? : "");
	return 0;
}
int factory_check_date(void)
{
	char *ptr = NULL;
	char *str = "DATE";
	ptr = read_firmware(str);
	if (NULL == ptr)
		return -1;
	printf("%s\n", ptr);
	return 0;	
}
int factory_check_mac_read(void)
{
	int fd = 0;
	unsigned char buff1[32];
	unsigned char buff2[32];
	unsigned char buff3[32];
	unsigned char mac1[32];
	unsigned char mac2[32];
	char *ptr = NULL;
	int bytes = 0;

	memset(buff1, 0, sizeof(buff1));
	memset(buff2, 0, sizeof(buff2));
	memset(buff3, 0, sizeof(buff3));
	ptr = "ff-ff-ff-ff-ff-ff";
	str2mac(ptr, mac1);
	ptr = "00-00-00-00-00-00";
	str2mac(ptr, mac2);
	fd = open("/dev/mtdblock3", O_RDONLY);
	if(fd < 0)
		return -1;	
	if(-1 == lseek(fd, 0x04, SEEK_SET))
		goto err;
	bytes = read(fd, buff1, 6);
	if(bytes != 6)
		goto err;
	if (-1 == lseek(fd, 0x28, SEEK_SET))
		goto err;
	bytes = read(fd, buff2, 6);
	if(bytes != 6)
		goto err;
	if (-1 == lseek(fd, 0x2E, SEEK_SET))
		goto err;
	bytes = read(fd, buff3, 6);
	if(bytes != 6)
		goto err;
	if(memcmp(buff1, buff2, 6) || memcmp(buff2, buff3, 6) || memcmp(buff1, buff3, 6)) 
		goto err;
	if(!memcmp(buff1, mac1, 6) || !memcmp(buff1, mac2, 6)) 
		goto err;	
	snprintf((char *)buff3, sizeof(buff3), "%02x-%02x-%02x-%02x-%02x-%02x",
		buff1[0], buff1[1], buff1[2], buff1[3], buff1[4], buff1[5]);
	close(fd);
	printf("%s\n", buff3);
	return 0;
err:
	if (fd > 0)
		close(fd);
	return -1;
}
int factory_check_mac_write(char *ptr)
{
	int fd = 0;
	unsigned char mac[32];
	memset(mac, 0, sizeof(mac));
	int bytes;
	
	str2mac(ptr, mac);
	fd = open ("/dev/mtdblock3", O_RDWR);
	if (fd < 0)
		return -1;
	if(-1 == lseek(fd, 0x04, SEEK_SET))
		goto err;
	bytes = igd_safe_write(fd, mac, 6);
	if(bytes != 6)
		goto err;
	if(-1 == lseek(fd, 0x28, SEEK_SET))
		goto err;
	bytes = igd_safe_write(fd, mac, 6);
	if(bytes != 6)
		goto err;
	if(-1 == lseek(fd, 0x2E, SEEK_SET))
		goto err;
	bytes = igd_safe_write(fd, mac, 6);
	if(bytes != 6)
		goto err;
	close(fd);
	return 0;
err:
	if (fd > 0)
		close(fd);
		return -1;
}
int factory_check_usb(void)
{
	FILE *stream;
	char buff[128];
	memset(buff, 0, sizeof(buff));
	stream = fopen("/proc/mounts", "r");
	if (stream == NULL) {
		return -1;
	}
	while(fgets(buff, sizeof(buff), stream) != NULL){
		if(!strncmp(buff, "/dev/sd", 7) || !strncmp(buff, "/dev/mmc", 8)) {
			printf("pass\n");
			fclose(stream);
			return 0;
		}
	}
	printf("fail\n");
	fclose(stream);
	return -1;	
}
int main(int argc, char *argv[])
{
	char *ptr;
	if (argc < 2 || argc > 3)
		return -1;
	
	ptr = argv[1];
	if (!strcmp(argv[1],"led")) {
		if (argc != 3)
			return -1;
		return factory_check_led(argv[2]);
	} else if (!strcmp(ptr,"key")) {
		return factory_check_key();
	} else if (!strcmp(ptr,"reset")) {
		return factory_check_reset();
	} else if (!strcmp(ptr,"ver")) {
		return factory_check_ver();
	} else if (!strcmp(ptr,"date")) {
		return factory_check_date();
	} else if (!strcmp(ptr,"read_mac")) {
		return factory_check_mac_read();
	} else if (!strcmp(ptr, "write_mac")){
		if (argc != 3)
			return -1;
		return factory_check_mac_write(argv[2]);
	} else if (!strcmp(ptr, "usb")) {
		return factory_check_usb();
	}
    return 0;
}
