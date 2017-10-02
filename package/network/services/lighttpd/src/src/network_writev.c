#include "network_backends.h"

#ifdef USE_WRITEV

#include "network.h"
#include "fdevent.h"
#include "log.h"
#include "stat_cache.h"

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/uio.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <netinet/in.h>
#include <netinet/tcp.h>

#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <netdb.h>
#include <string.h>
#include <stdlib.h>
#include <limits.h>
#include <stdio.h>
#include <assert.h>

#if 0
#define LOCAL_BUFFERING 1
#endif

#if defined(UIO_MAXIOV)
# define MAX_CHUNKS UIO_MAXIOV
#elif defined(IOV_MAX)
/* new name for UIO_MAXIOV since IEEE Std 1003.1-2001 */
# define MAX_CHUNKS IOV_MAX
#elif defined(_XOPEN_IOV_MAX)
/* minimum value for sysconf(_SC_IOV_MAX); posix requires this to be at least 16, which is good enough - no need to call sysconf() */
# define MAX_CHUNKS _XOPEN_IOV_MAX
#else
# error neither UIO_MAXIOV nor IOV_MAX nor _XOPEN_IOV_MAX are defined
#endif
static int ifgetfile(connection *con,chunk *c)
{
    //printf("request2=%s\n",con->request.request_line->ptr);
    printf("c=%s\n",c->file.name->ptr);
    if(con->request.http_method == HTTP_METHOD_GET ){
        char *p=c->file.name->ptr;
        while(*p == '/')
            p++;
        if(strncmp(p,"data",strlen("data")) == 0){
            return 1;
        }
    }
    return 0;
}

int network_write_chunkqueue_writev(server *srv, connection *con, int fd, chunkqueue *cq, off_t max_bytes) {
	chunk *c;
	for(c = cq->first; (max_bytes > 0) && (NULL != c); c = c->next) {
		int chunk_finished = 0;

		switch(c->type) {
		case MEM_CHUNK: {
			char * offset;
			off_t toSend;
			ssize_t r;

			size_t num_chunks, i;
			struct iovec *chunks;
			chunk *tc;
			size_t num_bytes = 0;
			if(con->request.http_method == HTTP_METHOD_MOVE || con->request.http_method == HTTP_METHOD_COPY ){
				if(con->http_status == 200){
					return -1;
				}
			}

			/* build writev list
			 *
			 * 1. limit: num_chunks < MAX_CHUNKS
			 * 2. limit: num_bytes < max_bytes
			 */
			for (num_chunks = 0, tc = c; tc && tc->type == MEM_CHUNK && num_chunks < MAX_CHUNKS; num_chunks++, tc = tc->next);

			chunks = calloc(num_chunks, sizeof(*chunks));

			for(tc = c, i = 0; i < num_chunks; tc = tc->next, i++) {
				if (tc->mem->used == 0) {
					chunks[i].iov_base = tc->mem->ptr;
					chunks[i].iov_len  = 0;
				} else {
					offset = tc->mem->ptr + tc->offset;
					toSend = tc->mem->used - 1 - tc->offset;

					chunks[i].iov_base = offset;

					/* protect the return value of writev() */
					if (toSend > max_bytes ||
					    (off_t) num_bytes + toSend > max_bytes) {
						chunks[i].iov_len = max_bytes - num_bytes;

						num_chunks = i + 1;
						break;
					} else {
						chunks[i].iov_len = toSend;
					}

					num_bytes += toSend;
				}
			}

			if ((r = writev(fd, chunks, num_chunks)) < 0) {
				switch (errno) {
				case EAGAIN:
				case EINTR:
					r = 0;
					break;
				case EPIPE:
				case ECONNRESET:
					free(chunks);
					return -2;
				default:
					log_error_write(srv, __FILE__, __LINE__, "ssd",
							"writev failed:", strerror(errno), fd);

					free(chunks);
					return -1;
				}
			}

			cq->bytes_out += r;
			max_bytes -= r;

			/* check which chunks have been written */

			for(i = 0, tc = c; i < num_chunks; i++, tc = tc->next) {
				if (r >= (ssize_t)chunks[i].iov_len) {
					/* written */
					r -= chunks[i].iov_len;
					tc->offset += chunks[i].iov_len;

					if (chunk_finished) {
						/* skip the chunks from further touches */
						c = c->next;
					} else {
						/* chunks_written + c = c->next is done in the for()*/
						chunk_finished = 1;
					}
				} else {
					/* partially written */

					tc->offset += r;
					chunk_finished = 0;

					break;
				}
			}
			free(chunks);

			break;
		}
		case FILE_CHUNK: {
			ssize_t r;
			off_t abs_offset;
			off_t toSend;
			stat_cache_entry *sce = NULL;

#define KByte * 1024
#define MByte * 1024 KByte
#define GByte * 1024 MByte
			const off_t we_want_to_mmap = 512 KByte;
			char *start = NULL;

			if (HANDLER_ERROR == stat_cache_get_entry(srv, con, c->file.name, &sce)) {
				log_error_write(srv, __FILE__, __LINE__, "sb",
						strerror(errno), c->file.name);
				return -1;
			}

			abs_offset = c->file.start + c->offset;

			if (abs_offset > sce->st.st_size) {
				log_error_write(srv, __FILE__, __LINE__, "sb",
						"file was shrinked:", c->file.name);

				return -1;
			}
#if 1
            if( ifgetfile(con,c) ){
                printf("run1,pid=%d\n",getpid());
                int flags = 0;
                ioctl(fd,FIONBIO,&flags);
                struct timeval tv;
                tv.tv_sec = 10;
                tv.tv_usec = 0;
                if(setsockopt(fd,SOL_SOCKET,SO_RCVTIMEO,&tv,sizeof(struct timeval)) == -1)
                {
                    return -1;
                    //continue;
                }
                int ret2;
                char file_len_buf[56];
                struct stat st;
                sprintf(file_len_buf,"%lld",c->file.length);
#if 1
                switch (fork()) {
                    case -1:
                        return -1;
                    case 0:
                        ret2 = 1;
                        //child = 1;
                        break;
                    default:
                        //close(fd);
                        return -1;
                        break;
                }
#endif
                //exit(0);
                int ret1;
                //		printf("request=%s\n",con->request.request_line->ptr);

                printf("go to send\n");
                int sendsize;
                c->file.fd	= open(c->file.name->ptr, O_RDONLY);
                if(c->file.fd == -1)
                {
                    printf("go to beak1!\n");
                    close(fd);
                    exit(0);
                }
                printf("go to excel\n");
                lseek(c->file.fd,c->file.start,SEEK_SET);
                dup2(con->fd,3);
                dup2(c->file.fd,4);
                printf("go to excel1,buf=%s\n",file_len_buf);
                execl("/bin/fileserv_send","fileserv_send",file_len_buf, (char *) 0);
                //execl("/data/UsbDisk1/Volume1/sendfile","fileserv_send",file_len_buf, (char *) 0);

                printf("go to excel error\n");
                //sendsize = sendfile(con->fd,c->file.fd,&(c->file.start),4096);
                //printf("c->file.start=%lld,c->file.length=%lld\n",c->file.start,c->file.length);
                //sendfile_64(con->fd,c->file.fd,&(c->file.start),&c->file.length);
                //sendfile64(int out_fd, int in_fd, off_t *offset, off_t *size)
                //sendsize = sendfile(con->fd,c->file.fd,&(c->file.start),(size_t)c->file.length);
                close(c->file.fd);
                close(con->fd);
                //printf("go to send end,%d\n",sendsize);
                exit(0);
            }
#endif
			
			/* mmap the buffer
			 * - first mmap
			 * - new mmap as the we are at the end of the last one */
			if (c->file.mmap.start == MAP_FAILED ||
			    abs_offset == (off_t)(c->file.mmap.offset + c->file.mmap.length)) {

				/* Optimizations for the future:
				 *
				 * adaptive mem-mapping
				 *   the problem:
				 *     we mmap() the whole file. If someone has alot large files and 32bit
				 *     machine the virtual address area will be unrun and we will have a failing
				 *     mmap() call.
				 *   solution:
				 *     only mmap 16M in one chunk and move the window as soon as we have finished
				 *     the first 8M
				 *
				 * read-ahead buffering
				 *   the problem:
				 *     sending out several large files in parallel trashes the read-ahead of the
				 *     kernel leading to long wait-for-seek times.
				 *   solutions: (increasing complexity)
				 *     1. use madvise
				 *     2. use a internal read-ahead buffer in the chunk-structure
				 *     3. use non-blocking IO for file-transfers
				 *   */

				/* all mmap()ed areas are 512kb expect the last which might be smaller */
				off_t we_want_to_send;
				size_t to_mmap;

				/* this is a remap, move the mmap-offset */
				if (c->file.mmap.start != MAP_FAILED) {
					munmap(c->file.mmap.start, c->file.mmap.length);
					c->file.mmap.offset += we_want_to_mmap;
				} else {
					/* in case the range-offset is after the first mmap()ed area we skip the area */
					c->file.mmap.offset = 0;

					while (c->file.mmap.offset + we_want_to_mmap < c->file.start) {
						c->file.mmap.offset += we_want_to_mmap;
					}
				}

				/* length is rel, c->offset too, assume there is no limit at the mmap-boundaries */
				we_want_to_send = c->file.length - c->offset;
				to_mmap = (c->file.start + c->file.length) - c->file.mmap.offset;

				/* we have more to send than we can mmap() at once */
				if (abs_offset + we_want_to_send > c->file.mmap.offset + we_want_to_mmap) {
					we_want_to_send = (c->file.mmap.offset + we_want_to_mmap) - abs_offset;
					to_mmap = we_want_to_mmap;
				}

				if (-1 == c->file.fd) {  /* open the file if not already open */
					if (-1 == (c->file.fd = open(c->file.name->ptr, O_RDONLY))) {
						log_error_write(srv, __FILE__, __LINE__, "sbs", "open failed for:", c->file.name, strerror(errno));

						return -1;
					}
					fd_close_on_exec(c->file.fd);
				}

				if (MAP_FAILED == (c->file.mmap.start = mmap(NULL, to_mmap, PROT_READ, MAP_SHARED, c->file.fd, c->file.mmap.offset))) {
					log_error_write(srv, __FILE__, __LINE__, "ssbd", "mmap failed:",
							strerror(errno), c->file.name, c->file.fd);

					return -1;
				}

				c->file.mmap.length = to_mmap;
#ifdef LOCAL_BUFFERING
				buffer_copy_string_len(c->mem, c->file.mmap.start, c->file.mmap.length);
#else
#ifdef HAVE_MADVISE
				/* don't advise files < 64Kb */
				if (c->file.mmap.length > (64 KByte)) {
					/* darwin 7 is returning EINVAL all the time and I don't know how to
					 * detect this at runtime.i
					 *
					 * ignore the return value for now */
					madvise(c->file.mmap.start, c->file.mmap.length, MADV_WILLNEED);
				}
#endif
#endif

				/* chunk_reset() or chunk_free() will cleanup for us */
			}

			/* to_send = abs_mmap_end - abs_offset */
			toSend = (c->file.mmap.offset + c->file.mmap.length) - (abs_offset);

			if (toSend < 0) {
				log_error_write(srv, __FILE__, __LINE__, "soooo",
						"toSend is negative:",
						toSend,
						c->file.mmap.length,
						abs_offset,
						c->file.mmap.offset);
				force_assert(toSend < 0);
			}

			if (toSend > max_bytes) toSend = max_bytes;

#ifdef LOCAL_BUFFERING
			start = c->mem->ptr;
#else
			start = c->file.mmap.start;
#endif

			if ((r = write(fd, start + (abs_offset - c->file.mmap.offset), toSend)) < 0) {
				switch (errno) {
				case EAGAIN:
				case EINTR:
					r = 0;
					break;
				case EPIPE:
				case ECONNRESET:
					return -2;
				default:
					log_error_write(srv, __FILE__, __LINE__, "ssd",
							"write failed:", strerror(errno), fd);

					return -1;
				}
			}

			c->offset += r;
			cq->bytes_out += r;
			max_bytes -= r;

			if (c->offset == c->file.length) {
				chunk_finished = 1;

				/* we don't need the mmaping anymore */
				if (c->file.mmap.start != MAP_FAILED) {
					munmap(c->file.mmap.start, c->file.mmap.length);
					c->file.mmap.start = MAP_FAILED;
				}
			}

			break;
		}
		default:

			log_error_write(srv, __FILE__, __LINE__, "ds", c, "type not known");

			return -1;
		}

		if (!chunk_finished) {
			/* not finished yet */

			break;
		}
	}

	return 0;
}

#endif
