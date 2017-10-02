
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <errno.h>
#include "openssl/aes.h"

#define L7_FILE_LEN_MX	(500 * 1024)
int enc_safe_read(int fd, unsigned char *dst, int len)
{
	int count = 0;
	int ret;

	while (len > 0) {
		ret = read(fd, dst, len);
		if (!ret) 
			break;
		if (ret < 0) {
			if (errno == EAGAIN || errno == EWOULDBLOCK) 
				continue;
			if (errno == EINTR) 
				continue;
			return -1;
		}
		count += ret;
		len -= ret;
		dst += ret;
	}
	return count;
}

int enc_safe_write(int fd, unsigned char *src, int len)
{
	int count = 0;
	int ret;

	while (len > 0) {
		ret = write(fd, src, len);
		if (!ret) 
			continue;
		if (ret < 0) {
			if (errno == EAGAIN || errno == EWOULDBLOCK) 
				continue;
			if (errno == EINTR) 
				continue;
			return -1;
		}
		count += ret;
		len -= ret;
		src += ret;
	}
	return count;
}

static int read_file(char *path, char *out, int len)
{
	unsigned char pwd[16] = { 0xcc,0xfc,0x78,0x66,0x35,0x32,0x97,0xfc,0x78,0x99 };
	unsigned char ivec[16] = { 0x8,0xfe,0x78,0xcc,0x19,0x79,0,0xce,0xfe};
	AES_KEY aes;
	struct stat st;
	char *src = NULL;
	int size;
	int fd;
	int ret;

	fd = open(path, O_RDONLY);
	if (fd < 0) {
		printf("can't open \n");
		goto error;
	}
	if (fstat(fd, &st))
		goto error;
	size = st.st_size;
	if (size > len + 16)
		goto error;

	ret = size % 16;
	len = size;
	if (ret)
		len = len - ret + 16;
		
	src = malloc(len);
	if (!src) {
		printf("no mem \n");
		goto error;
	}
	memset(src, 0, len);
	/* change passwd */
	pwd[13] = 0x7c;
	pwd[15] = 0x1d;
	pwd[11] = 0x89;
	ret = enc_safe_read(fd, src, size);
	ivec[13] = 0x3c;
	ivec[15] = 0x7e;
	ivec[11] = 0x59;
	if (ret != size) {
		printf("read %d, need %d\n", ret, size);
		goto error;
	}
	
	if (AES_set_encrypt_key(pwd, 128, &aes) < 0)
		goto error;
	AES_cbc_encrypt(src, out, len, &aes, ivec, AES_ENCRYPT);
	free(src);
	close(fd);

	return len;
error:
    printf("read %s error\n", path);
    if (fd > 0) 
	    close(fd);
    if (src) 
	    free(src);
    return -1;
}

int main(int argc, const char *argv[])
{
	char content[L7_FILE_LEN_MX] = { 0, };
	unsigned char *in;
	unsigned char *out;
	int fd;
	int len;
	int ret;

	if (argc < 3) 
		return -1;
	in = (void *)argv[1];
	out = (void *)argv[2];

	len = read_file(in, content, sizeof(content));
	if (len < 0) {
		printf("read %s err \n", in);
		return -1;
	}
		
	fd = open(out, O_RDWR | O_CREAT | O_TRUNC, 0666);
	if (fd < 0) {
		printf("open %s err \n", out);
		return -1;
	}
	ret = enc_safe_write(fd, content, len);
	if (ret != len) {
		printf("write %d , need %d\n", ret, len);
		return -1;
	}	
	return 0;
}

