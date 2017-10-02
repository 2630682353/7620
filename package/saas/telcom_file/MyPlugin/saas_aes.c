#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include "aes.h"

/* read file , return n bytes read, or -1 if error */
int saas_read(int fd, unsigned char *dst, int len)
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

int saas_aes(const char *path,
	unsigned char *out, int len, unsigned char *pwd)
{
	int fd;
	int ret = -1;
	struct stat st;
	int size, newlen, nr;
	unsigned char *pDataBuf = NULL;
	int i_encry = 0, i_padding = 0;
	aes_encrypt_ctx en_ctx[1];
	unsigned char buf[AES_BLOCK_SIZE] = {0};
	
	fd = open(path, O_RDONLY);
	if (fd < 0)
		return -1;
	if (fstat(fd, &st))
		goto error;
	size = st.st_size;
	i_encry = size/AES_BLOCK_SIZE + 1;
	i_padding = AES_BLOCK_SIZE - size%AES_BLOCK_SIZE;
	newlen = size + i_padding;
	if (len < newlen)
		goto error;
	memset(out, 0x0, len);
	nr = saas_read(fd, out, size);
	if (nr != size) {
		printf("igd safe read input file error\n");
		goto error;
	}
	pDataBuf = out;
	for (nr = 0; nr < i_encry; nr++) {
		memset(buf, 0x0, sizeof(buf));
		aes_encrypt_key128(pwd, en_ctx);
		aes_ecb_encrypt(pDataBuf, buf, AES_BLOCK_SIZE, en_ctx);
		memcpy(pDataBuf, buf, AES_BLOCK_SIZE);
		pDataBuf += AES_BLOCK_SIZE;
	}
	ret = newlen;
error:
	if (fd > 0)
		close(fd);
	return ret;
}
