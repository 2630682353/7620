#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include "aes.h"

int n_size(int fd)
{
	struct stat st;

	if (fstat(fd, &st) < 0)
		return -1;
	return (int)st.st_size;
}

int n2_size(FILE *fp)
{
	long cur, len;

	cur = ftell(fp);
	fseek(fp, 0L, SEEK_END);
	len = ftell(fp);
	fseek(fp, cur, SEEK_SET);
	return (int)len;
}

int n_read(int fd, void *dst, int len)
{
	int r;
	unsigned char *d = dst;

	while (len > 0) {
		r = read(fd, d, len);
		if (r > 0) {
			len -= r;
			d += r;
		} else if (r < 0) {
			if (errno == EINTR
				|| errno == EAGAIN
				|| errno == EWOULDBLOCK) 
				continue;
			return -1;
		} else {
			break;
		}
	}
	return (int)(d - (unsigned char *)dst);
}

int n_write(int fd, void *src, int len)
{
	int w;
	unsigned char *s = src;

	while (len > 0) {
		w = write(fd, s, len);
		if (w > 0) {
			len -= w;
			s += w;
		} else if (w < 0) {
			if (errno == EINTR
				|| errno == EAGAIN
				|| errno == EWOULDBLOCK) 
				continue;
			return -1;
		} else {
			break;
		}
	}
	return (int)(s - (unsigned char *)src);
}

#define ADD_HEAD_LEN 8
int gz_encrypt(char *in, char *out, unsigned char *pwd)
{
	int in_fd, out_fd;
	int in_size;
	int i, nr, ret, head_len = ADD_HEAD_LEN;
	unsigned char in_buf[AES_BLOCK_SIZE], out_buf[AES_BLOCK_SIZE];
	aes_encrypt_ctx en_ctx[1];

	in_fd = open(in, O_RDONLY);
	out_fd = open(out, O_TRUNC | O_CREAT | O_WRONLY, 0777);
	if ((in_fd < 0) || (out_fd < 0)) {
		printf("open fail\n");
		goto err;
	}

	in_size = n_size(in_fd);
	if (in_size < 0) {
		printf("size fail\n");
		goto err;
	}
	nr = (in_size + 8)/AES_BLOCK_SIZE \
		+ ((in_size + 8)%AES_BLOCK_SIZE ? 1 : 0);
	printf("in file size: %d(B), nr:%d\n", in_size, nr);

	for (i = 0; i < nr; i++) {
		memset(in_buf, 0x0, sizeof(in_buf));
		memset(out_buf, 0x0, sizeof(out_buf));

		if (head_len) {
			snprintf((char *)in_buf, ADD_HEAD_LEN, "%d", in_size);
			ret = n_read(in_fd, in_buf + ADD_HEAD_LEN,\
				AES_BLOCK_SIZE - ADD_HEAD_LEN);
			head_len = 0;
		} else {
			ret = n_read(in_fd, in_buf, AES_BLOCK_SIZE);
		}
		if (ret <= 0) {
			printf("read fail\n");
			goto err;
		}
		in_size -= ret;

		aes_encrypt_key128(pwd, en_ctx);
		if (aes_ecb_encrypt(in_buf, out_buf, AES_BLOCK_SIZE, en_ctx)) {
			printf("aes fail\n");
			goto err;
		}
		ret = n_write(out_fd, out_buf, AES_BLOCK_SIZE);
		if (ret != AES_BLOCK_SIZE) {
			printf("write fail, %d\n", ret);
			goto err;
		}
		if (!in_size)
			break;
	}

	close(in_fd);
	close(out_fd);
	return 0;
err:
	if (in_fd > 0)
		close(in_fd);
	if (out_fd > 0)
		close(out_fd);
	return -1;
}

int main(int argc, char *argv[])
{
	unsigned char pwd[16] = {0xcc, 0xfc, 0x78, 0x66, 0x35, 0x32, 0x97, 0xfc, 0x78, 0x99};

	if (argc < 3) {
		printf("./gz_encrypt [in_file] [out_file]");
		return -1;
	}

	return gz_encrypt(argv[1], argv[2], pwd);
}
