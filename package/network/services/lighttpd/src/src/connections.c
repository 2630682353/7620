#include "buffer.h"
#include "server.h"
#include "log.h"
#include "connections.h"
#include "fdevent.h"

#include "request.h"
#include "response.h"
#include "network.h"
#include "http_chunk.h"
#include "stat_cache.h"
#include "joblist.h"

#include "plugin.h"

#include "inet_ntop_cache.h"

#include <sys/stat.h>

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <fcntl.h>
#include <assert.h>

#ifdef USE_OPENSSL
# include <openssl/ssl.h>
# include <openssl/err.h>
#endif

#ifdef HAVE_SYS_FILIO_H
# include <sys/filio.h>
#endif
#include <net/if.h>
#include "sys-socket.h"
int obtain_mac(const char *ifname, unsigned char *mac, int len)  
{  
    int sk = -1, ret = 0;  
    struct ifreq ifreq;  
  
    assert(ifname);  
    assert(mac);  
//    assert(len >= 6);  
  
    sk = socket(AF_INET, SOCK_STREAM, 0);  
    if (sk < 0)  
    {  
        perror("socket");  
        ret--;  
        goto OUT;  
    }  
  
    strcpy(ifreq.ifr_name, ifname);  
    if (ioctl(sk, SIOCGIFHWADDR, &ifreq) < 0)  
    {  
        perror("ioctl");  
        ret--;  
        goto OUT;  
    }  
    memcpy(mac, (unsigned char *)ifreq.ifr_hwaddr.sa_data, 6);  
  
OUT:  
    if (sk >= 0)  
        close(sk);  
    return ret;  
}  

int net_getifaddr(const char *ifname, char *buf, int len)
{
    /* SIOCGIFADDR struct ifreq *  */
    int s;
    struct ifreq ifr;
    int ifrlen;
    struct sockaddr_in * addr;

    ifrlen = sizeof(ifr);
    s = socket(PF_INET, SOCK_DGRAM, 0);
    if (s < 0) {
	printf("error1\n");
        //          SYS_LOG(io_log, "%s\n", "net_getifaddr");
        return -1;
    }
    strncpy(ifr.ifr_name, ifname, IFNAMSIZ);
    //strncpy(ifr.ifr_name, ifname, 4096);
    if (ioctl(s, SIOCGIFADDR, &ifr, &ifrlen) < 0) {
        //          SYS_LOG(io_log, "%s\n", "ioctl");
	printf("error2\n");
        close(s);
        return -1;
    }
    addr = (struct sockaddr_in *)&ifr.ifr_addr;
    if(!inet_ntop(AF_INET, &addr->sin_addr, buf, len)) {
        //          SYS_LOG(io_log, "%s\n", "inet_ntop");
	printf("error3\n");
        close(s);
        return -1;
    }
    close(s);

    return 0;
}

typedef struct {
	        PLUGIN_DATA;
} plugin_data;




#if 1
static int getdiskpath(char*dest,const char*src)
{
        int i;
        int j;
        int k;
        for(i=0,j=0,k=0;i<4&&src[j]!='\0';j++)
        {
                if(src[j] == '/')
                {
                        if(j==0 ||dest[k-1]!='/')
                        {
                                i++;
                if(i == 4)
                {
                    break;
                }
                                dest[k++]='/';
                        }
                }else{
                        dest[k++] = src[j];
                }

        }
    dest[k]='\0';
    if(i <3 )
    {
        return 0;
    }else{
        if(i == 3 && dest[k-1] == '/')
        {
            return 0;
        }
        return k;
    }
}
static void upfile_mkdir(const char*src){
    char tmp[1024];
    memset(tmp,0,1024);
    int i=0;
    while(1){
        tmp[i] = src[i];
        if(tmp[i]  == '/'){
            mkdir(tmp, S_IRWXU | S_IRWXG | S_IRWXO);
            printf("mkdir=%s\n",tmp);
        }
        if(tmp[i] == '\0'){
            break;
        }
        i++;
    }
}
static void upfile_fordir(const char*src){
        struct stat sbuf;
    char dest[1024];
    getdiskpath(dest,src);
    strcat(dest,"/.vst");
    printf("dest=%s\n",dest);
    if(stat(dest,&sbuf) == -1){
        return;
    }
    printf("go to mkdir");
    upfile_mkdir(src);
}
#endif
#if 1//add by mengqingyong
static size_t writen(int fd, const void *vptr, size_t n)
{
    size_t nleft;
    int nwritten;
    const char *ptr;
    ptr = vptr;
    nleft = n;
    int flag =0;
    while(nleft > 0){
        if(flag > 2)
        {
            return -1;
        }
        if((nwritten = write(fd,ptr,nleft)) <= 0 ){
            if(nwritten <0 && errno == EINTR)
                nwritten = 0;
            else
                return -1;
        }
        nleft -= nwritten;
        ptr += nwritten;
        flag++;
    }
    return n;
}

#endif


static connection *connections_get_new_connection(server *srv) {
	connections *conns = srv->conns;
	size_t i;

	if (conns->size == 0) {
		conns->size = 128;
		conns->ptr = NULL;
		conns->ptr = malloc(sizeof(*conns->ptr) * conns->size);
		for (i = 0; i < conns->size; i++) {
			conns->ptr[i] = connection_init(srv);
		}
	} else if (conns->size == conns->used) {
		conns->size += 128;
		conns->ptr = realloc(conns->ptr, sizeof(*conns->ptr) * conns->size);

		for (i = conns->used; i < conns->size; i++) {
			conns->ptr[i] = connection_init(srv);
		}
	}

	connection_reset(srv, conns->ptr[conns->used]);
#if 0
	fprintf(stderr, "%s.%d: add: ", __FILE__, __LINE__);
	for (i = 0; i < conns->used + 1; i++) {
		fprintf(stderr, "%d ", conns->ptr[i]->fd);
	}
	fprintf(stderr, "\n");
#endif

	conns->ptr[conns->used]->ndx = conns->used;
	return conns->ptr[conns->used++];
}

static int connection_del(server *srv, connection *con) {
	size_t i;
	connections *conns = srv->conns;
	connection *temp;

	if (con == NULL) return -1;

	if (-1 == con->ndx) return -1;
#if 1 //add by mengqy
    if(con->filefd != -1)
    {
        close(con->filefd);
        con->filefd = -1;
    }
    if(con->headinfo.opt->used == 0 && con->headinfo.dir->used !=0)
    {
        char tmpbuf[2048];
        sprintf(tmpbuf,"%s.ittmp",con->headinfo.dir->ptr);
        struct stat sbuf;
        if(con->filefdflag != 4 && con->filefdflag != 5 && -1 != stat(tmpbuf,&sbuf))
        {
            remove(tmpbuf);
        }
        buffer_reset(con->headinfo.dir);
        //printf("go to delete tmp\n");
    }
    if(con->filefdflag != 0){
        con->filefdflag = 0;
    sync();
    }
    //con->filefdflag = 0;
    con->sendkey = 0;
	buffer_reset(con->headinfo.tail);
	buffer_reset(con->headinfo.part);
	con->headinfo.tail_len= 0;
    buffer_reset(con->headinfo.dir);
    buffer_reset(con->headinfo.file);
    buffer_reset(con->headinfo.session);
    buffer_reset(con->headinfo.status);
    buffer_reset(con->headinfo.filename);
    buffer_reset(con->headinfo.opt);
    buffer_reset(con->headinfo.length);
    con->headinfo.file_length = 0;
    con->headinfo.file_in = 0;
#endif

	buffer_reset(con->uri.authority);
	buffer_reset(con->uri.path);
	buffer_reset(con->uri.query);
	buffer_reset(con->request.orig_uri);

	i = con->ndx;

	/* not last element */

	if (i != conns->used - 1) {
		temp = conns->ptr[i];
		conns->ptr[i] = conns->ptr[conns->used - 1];
		conns->ptr[conns->used - 1] = temp;

		conns->ptr[i]->ndx = i;
		conns->ptr[conns->used - 1]->ndx = -1;
	}

	conns->used--;

	con->ndx = -1;
#if 0
	fprintf(stderr, "%s.%d: del: (%d)", __FILE__, __LINE__, conns->used);
	for (i = 0; i < conns->used; i++) {
		fprintf(stderr, "%d ", conns->ptr[i]->fd);
	}
	fprintf(stderr, "\n");
#endif
	return 0;
}

int connection_close(server *srv, connection *con) {
#ifdef USE_OPENSSL
	server_socket *srv_sock = con->srv_socket;
#endif
#if 1 // add by mengqy
    if(con->filefd != -1)
    {
        close(con->filefd);
        con->filefd = -1;
    }
    if(con->headinfo.opt->used == 0 && con->headinfo.dir->used !=0)
    {
        char tmpbuf[2048];
        sprintf(tmpbuf,"%s.ittmp",con->headinfo.dir->ptr);
        struct stat sbuf;
        if(con->filefdflag != 4 && con->filefdflag != 5 && -1 != stat(tmpbuf,&sbuf))
        {
            remove(tmpbuf);
        }
        buffer_reset(con->headinfo.dir);
        //printf("go to delete tmp\n");
    }
    if(con->filefdflag != 0){
        con->filefdflag = 0;
    sync();
    }
    //con->filefdflag = 0;
    con->sendkey = 0;
    con->headinfo.file_length = 0;
    con->headinfo.file_in = 0;
#endif


#ifdef USE_OPENSSL
	if (srv_sock->is_ssl) {
		if (con->ssl) SSL_free(con->ssl);
		con->ssl = NULL;
	}
#endif

	fdevent_event_del(srv->ev, &(con->fde_ndx), con->fd);
	fdevent_unregister(srv->ev, con->fd);
#ifdef __WIN32
	if (closesocket(con->fd)) {
		log_error_write(srv, __FILE__, __LINE__, "sds",
				"(warning) close:", con->fd, strerror(errno));
	}
#else
	if (close(con->fd)) {
		log_error_write(srv, __FILE__, __LINE__, "sds",
				"(warning) close:", con->fd, strerror(errno));
	}
#endif

	srv->cur_fds--;
#if 0
	log_error_write(srv, __FILE__, __LINE__, "sd",
			"closed()", con->fd);
#endif

	connection_del(srv, con);
	connection_set_state(srv, con, CON_STATE_CONNECT);

	return 0;
}

#if 0
static void dump_packet(const unsigned char *data, size_t len) {
	size_t i, j;

	if (len == 0) return;

	for (i = 0; i < len; i++) {
		if (i % 16 == 0) fprintf(stderr, "  ");

		fprintf(stderr, "%02x ", data[i]);

		if ((i + 1) % 16 == 0) {
			fprintf(stderr, "  ");
			for (j = 0; j <= i % 16; j++) {
				unsigned char c;

				if (i-15+j >= len) break;

				c = data[i-15+j];

				fprintf(stderr, "%c", c > 32 && c < 128 ? c : '.');
			}

			fprintf(stderr, "\n");
		}
	}

	if (len % 16 != 0) {
		for (j = i % 16; j < 16; j++) {
			fprintf(stderr, "   ");
		}

		fprintf(stderr, "  ");
		for (j = i & ~0xf; j < len; j++) {
			unsigned char c;

			c = data[j];
			fprintf(stderr, "%c", c > 32 && c < 128 ? c : '.');
		}
		fprintf(stderr, "\n");
	}
}
#endif

static int connection_handle_read_ssl(server *srv, connection *con) {
#ifdef USE_OPENSSL
	int r, ssl_err, len, count = 0, read_offset, toread;
	buffer *b = NULL;

	if (!con->srv_socket->is_ssl) return -1;

	ERR_clear_error();
	do {
		if (NULL != con->read_queue->last) {
			b = con->read_queue->last->mem;
		}

		if (NULL == b || b->size - b->used < 1024) {
			b = chunkqueue_get_append_buffer(con->read_queue);
			len = SSL_pending(con->ssl);
			if (len < 4*1024) len = 4*1024; /* always alloc >= 4k buffer */
			buffer_prepare_copy(b, len + 1);

			/* overwrite everything with 0 */
			memset(b->ptr, 0, b->size);
		}

		read_offset = (b->used > 0) ? b->used - 1 : 0;
		toread = b->size - 1 - read_offset;

		len = SSL_read(con->ssl, b->ptr + read_offset, toread);

		if (con->renegotiations > 1 && con->conf.ssl_disable_client_renegotiation) {
			log_error_write(srv, __FILE__, __LINE__, "s", "SSL: renegotiation initiated by client, killing connection");
			connection_set_state(srv, con, CON_STATE_ERROR);
			return -1;
		}

		if (len > 0) {
			if (b->used > 0) b->used--;
			b->used += len;
			b->ptr[b->used++] = '\0';

			con->bytes_read += len;

			count += len;
		}
	} while (len == toread && count < MAX_READ_LIMIT);


	if (len < 0) {
		int oerrno = errno;
		switch ((r = SSL_get_error(con->ssl, len))) {
		case SSL_ERROR_WANT_READ:
		case SSL_ERROR_WANT_WRITE:
			con->is_readable = 0;

			/* the manual says we have to call SSL_read with the same arguments next time.
			 * we ignore this restriction; no one has complained about it in 1.5 yet, so it probably works anyway.
			 */

			return 0;
		case SSL_ERROR_SYSCALL:
			/**
			 * man SSL_get_error()
			 *
			 * SSL_ERROR_SYSCALL
			 *   Some I/O error occurred.  The OpenSSL error queue may contain more
			 *   information on the error.  If the error queue is empty (i.e.
			 *   ERR_get_error() returns 0), ret can be used to find out more about
			 *   the error: If ret == 0, an EOF was observed that violates the
			 *   protocol.  If ret == -1, the underlying BIO reported an I/O error
			 *   (for socket I/O on Unix systems, consult errno for details).
			 *
			 */
			while((ssl_err = ERR_get_error())) {
				/* get all errors from the error-queue */
				log_error_write(srv, __FILE__, __LINE__, "sds", "SSL:",
						r, ERR_error_string(ssl_err, NULL));
			}

			switch(oerrno) {
			default:
				log_error_write(srv, __FILE__, __LINE__, "sddds", "SSL:",
						len, r, oerrno,
						strerror(oerrno));
				break;
			}

			break;
		case SSL_ERROR_ZERO_RETURN:
			/* clean shutdown on the remote side */

			if (r == 0) {
				/* FIXME: later */
			}

			/* fall thourgh */
		default:
			while((ssl_err = ERR_get_error())) {
				switch (ERR_GET_REASON(ssl_err)) {
				case SSL_R_SSL_HANDSHAKE_FAILURE:
				case SSL_R_TLSV1_ALERT_UNKNOWN_CA:
				case SSL_R_SSLV3_ALERT_CERTIFICATE_UNKNOWN:
				case SSL_R_SSLV3_ALERT_BAD_CERTIFICATE:
					if (!con->conf.log_ssl_noise) continue;
					break;
				default:
					break;
				}
				/* get all errors from the error-queue */
				log_error_write(srv, __FILE__, __LINE__, "sds", "SSL:",
				                r, ERR_error_string(ssl_err, NULL));
			}
			break;
		}

		connection_set_state(srv, con, CON_STATE_ERROR);

		return -1;
	} else if (len == 0) {
		con->is_readable = 0;
		/* the other end close the connection -> KEEP-ALIVE */

		return -2;
	} else {
		joblist_append(srv, con);
	}

	return 0;
#else
	UNUSED(srv);
	UNUSED(con);
	return -1;
#endif
}

/* 0: everything ok, -1: error, -2: con closed */
static int connection_handle_read(server *srv, connection *con) {
	int len;
	buffer *b;
	int toread, read_offset;

	if (con->srv_socket->is_ssl) {
		return connection_handle_read_ssl(srv, con);
	}

	b = (NULL != con->read_queue->last) ? con->read_queue->last->mem : NULL;

	/* default size for chunks is 4kb; only use bigger chunks if FIONREAD tells
	 *  us more than 4kb is available
	 * if FIONREAD doesn't signal a big chunk we fill the previous buffer
	 *  if it has >= 1kb free
	 */
#if defined(__WIN32)
	if (NULL == b || b->size - b->used < 1024) {
		b = chunkqueue_get_append_buffer(con->read_queue);
		buffer_prepare_copy(b, 4 * 1024);
	}

	read_offset = (b->used == 0) ? 0 : b->used - 1;
	len = recv(con->fd, b->ptr + read_offset, b->size - 1 - read_offset, 0);
#else
	if (ioctl(con->fd, FIONREAD, &toread) || toread == 0 || toread <= 4*1024) {
		if (NULL == b || b->size - b->used < 1024) {
			b = chunkqueue_get_append_buffer(con->read_queue);
			buffer_prepare_copy(b, 4 * 1024);
		}
	} else {
		if (toread > MAX_READ_LIMIT) toread = MAX_READ_LIMIT;
		b = chunkqueue_get_append_buffer(con->read_queue);
		buffer_prepare_copy(b, toread + 1);
	}

	read_offset = (b->used == 0) ? 0 : b->used - 1;
	len = read(con->fd, b->ptr + read_offset, b->size - 1 - read_offset);
#endif

	if (len < 0) {
		con->is_readable = 0;

		if (errno == EAGAIN) return 0;
		if (errno == EINTR) {
			/* we have been interrupted before we could read */
			con->is_readable = 1;
			return 0;
		}

		if (errno != ECONNRESET) {
			/* expected for keep-alive */
			log_error_write(srv, __FILE__, __LINE__, "ssd", "connection closed - read failed: ", strerror(errno), errno);
		}

		connection_set_state(srv, con, CON_STATE_ERROR);

		return -1;
	} else if (len == 0) {
		con->is_readable = 0;
		/* the other end close the connection -> KEEP-ALIVE */

		/* pipelining */

		return -2;
	} else if ((size_t)len < b->size - 1) {
		/* we got less then expected, wait for the next fd-event */

		con->is_readable = 0;
	}

	if (b->used > 0) b->used--;
	b->used += len;
	b->ptr[b->used++] = '\0';

	con->bytes_read += len;
#if 0
	dump_packet(b->ptr, len);
#endif

	return 0;
}

static int connection_handle_write_prepare(server *srv, connection *con) {
	if (con->mode == DIRECT) {
		/* static files */
		switch(con->request.http_method) {
		case HTTP_METHOD_GET:
		case HTTP_METHOD_POST:
		case HTTP_METHOD_HEAD:
			break;
		case HTTP_METHOD_OPTIONS:
			/*
			 * 400 is coming from the request-parser BEFORE uri.path is set
			 * 403 is from the response handler when noone else catched it
			 *
			 * */
			if ((!con->http_status || con->http_status == 200) && con->uri.path->used &&
			    con->uri.path->ptr[0] != '*') {
				response_header_insert(srv, con, CONST_STR_LEN("Allow"), CONST_STR_LEN("OPTIONS, GET, HEAD, POST"));

				con->response.transfer_encoding &= ~HTTP_TRANSFER_ENCODING_CHUNKED;
				con->parsed_response &= ~HTTP_CONTENT_LENGTH;

				con->http_status = 200;
				con->file_finished = 1;

				chunkqueue_reset(con->write_queue);
			}
			break;
		default:
			if (0 == con->http_status) {
				con->http_status = 501;
			}
			break;
		}
	}

	if (con->http_status == 0) {
		con->http_status = 403;
	}

	switch(con->http_status) {
	case 204: /* class: header only */
	case 205:
	case 304:
		/* disable chunked encoding again as we have no body */
		con->response.transfer_encoding &= ~HTTP_TRANSFER_ENCODING_CHUNKED;
		con->parsed_response &= ~HTTP_CONTENT_LENGTH;
		chunkqueue_reset(con->write_queue);

		con->file_finished = 1;
		break;
	default: /* class: header + body */
		if (con->mode != DIRECT) break;

		/* only custom body for 4xx and 5xx */
		if (con->http_status < 400 || con->http_status >= 600) break;

		con->file_finished = 0;

		buffer_reset(con->physical.path);

		/* try to send static errorfile */
		if (!buffer_is_empty(con->conf.errorfile_prefix)) {
			stat_cache_entry *sce = NULL;

			buffer_copy_string_buffer(con->physical.path, con->conf.errorfile_prefix);
			buffer_append_long(con->physical.path, con->http_status);
			buffer_append_string_len(con->physical.path, CONST_STR_LEN(".html"));

			if (HANDLER_ERROR != stat_cache_get_entry(srv, con, con->physical.path, &sce)) {
				con->file_finished = 1;

				http_chunk_append_file(srv, con, con->physical.path, 0, sce->st.st_size);
				response_header_overwrite(srv, con, CONST_STR_LEN("Content-Type"), CONST_BUF_LEN(sce->content_type));
			}
		}

		if (!con->file_finished) {
			buffer *b;

			buffer_reset(con->physical.path);

			con->file_finished = 1;
#if 1 //add by mengqy
            if(con->filefdflag == 0 || con->headinfo.file->used == 0)
            {
#endif

			b = chunkqueue_get_append_buffer(con->write_queue);

			/* build default error-page */
			buffer_copy_string_len(b, CONST_STR_LEN(
					   "<?xml version=\"1.0\" encoding=\"iso-8859-1\"?>\n"
					   "<!DOCTYPE html PUBLIC \"-//W3C//DTD XHTML 1.0 Transitional//EN\"\n"
					   "         \"http://www.w3.org/TR/xhtml1/DTD/xhtml1-transitional.dtd\">\n"
					   "<html xmlns=\"http://www.w3.org/1999/xhtml\" xml:lang=\"en\" lang=\"en\">\n"
					   " <head>\n"
					   "  <title>"));
			buffer_append_long(b, con->http_status);
			buffer_append_string_len(b, CONST_STR_LEN(" - "));
			buffer_append_string(b, get_http_status_name(con->http_status));

			buffer_append_string_len(b, CONST_STR_LEN(
					     "</title>\n"
					     " </head>\n"
					     " <body>\n"
					     "  <h1>"));
			buffer_append_long(b, con->http_status);
			buffer_append_string_len(b, CONST_STR_LEN(" - "));
			buffer_append_string(b, get_http_status_name(con->http_status));

			buffer_append_string_len(b, CONST_STR_LEN("</h1>\n"
					     " </body>\n"
					     "</html>\n"
					     ));

			response_header_overwrite(srv, con, CONST_STR_LEN("Content-Type"), CONST_STR_LEN("text/html"));
		}
#if 1 //add by mengqy
            }
#endif

		break;
	}

	if (con->file_finished) {
		/* we have all the content and chunked encoding is not used, set a content-length */

		if ((!(con->parsed_response & HTTP_CONTENT_LENGTH)) &&
		    (con->response.transfer_encoding & HTTP_TRANSFER_ENCODING_CHUNKED) == 0) {
			off_t qlen = chunkqueue_length(con->write_queue);

			/**
			 * The Content-Length header only can be sent if we have content:
			 * - HEAD doesn't have a content-body (but have a content-length)
			 * - 1xx, 204 and 304 don't have a content-body (RFC 2616 Section 4.3)
			 *
			 * Otherwise generate a Content-Length header as chunked encoding is not 
			 * available
			 */
			if ((con->http_status >= 100 && con->http_status < 200) ||
			    con->http_status == 204 ||
			    con->http_status == 304) {
				data_string *ds;
				/* no Content-Body, no Content-Length */
				if (NULL != (ds = (data_string*) array_get_element(con->response.headers, "Content-Length"))) {
					buffer_reset(ds->value); /* Headers with empty values are ignored for output */
				}
			} else if (qlen > 0 || con->request.http_method != HTTP_METHOD_HEAD) {
				/* qlen = 0 is important for Redirects (301, ...) as they MAY have
				 * a content. Browsers are waiting for a Content otherwise
				 */
				buffer_copy_off_t(srv->tmp_buf, qlen);

				response_header_overwrite(srv, con, CONST_STR_LEN("Content-Length"), CONST_BUF_LEN(srv->tmp_buf));
			}
		}
	} else {
		/**
		 * the file isn't finished yet, but we have all headers
		 *
		 * to get keep-alive we either need:
		 * - Content-Length: ... (HTTP/1.0 and HTTP/1.0) or
		 * - Transfer-Encoding: chunked (HTTP/1.1)
		 */

		if (((con->parsed_response & HTTP_CONTENT_LENGTH) == 0) &&
		    ((con->response.transfer_encoding & HTTP_TRANSFER_ENCODING_CHUNKED) == 0)) {
			con->keep_alive = 0;
		}

		/**
		 * if the backend sent a Connection: close, follow the wish
		 *
		 * NOTE: if the backend sent Connection: Keep-Alive, but no Content-Length, we
		 * will close the connection. That's fine. We can always decide the close 
		 * the connection
		 *
		 * FIXME: to be nice we should remove the Connection: ... 
		 */
		if (con->parsed_response & HTTP_CONNECTION) {
			/* a subrequest disable keep-alive although the client wanted it */
			if (con->keep_alive && !con->response.keep_alive) {
				con->keep_alive = 0;
			}
		}
	}

	if (con->request.http_method == HTTP_METHOD_HEAD) {
		/**
		 * a HEAD request has the same as a GET 
		 * without the content
		 */
		con->file_finished = 1;

		chunkqueue_reset(con->write_queue);
		con->response.transfer_encoding &= ~HTTP_TRANSFER_ENCODING_CHUNKED;
	}

	http_response_write_header(srv, con);

	return 0;
}

static int connection_handle_write(server *srv, connection *con) {
	switch(network_write_chunkqueue(srv, con, con->write_queue, MAX_WRITE_LIMIT)) {
	case 0:
		con->write_request_ts = srv->cur_ts;
		if (con->file_finished) {
			connection_set_state(srv, con, CON_STATE_RESPONSE_END);
			joblist_append(srv, con);
		}
		break;
	case -1: /* error on our side */
		log_error_write(srv, __FILE__, __LINE__, "sd",
				"connection closed: write failed on fd", con->fd);
		connection_set_state(srv, con, CON_STATE_ERROR);
		joblist_append(srv, con);
        return -1;
		break;
	case -2: /* remote close */
		connection_set_state(srv, con, CON_STATE_ERROR);
		joblist_append(srv, con);
		break;
	case 1:
		con->write_request_ts = srv->cur_ts;
		con->is_writable = 0;

		/* not finished yet -> WRITE */
		break;
	}

	return 0;
}



connection *connection_init(server *srv) {
	connection *con;

	UNUSED(srv);

	con = calloc(1, sizeof(*con));
#if 1 // add by mengqy
    con->filefd = -1;
    con->filefdflag = 0;
    con->sendkey = 0;
	con->headinfo.tail_len = 0;
	con->headinfo.part = buffer_init();
	con->headinfo.tail = buffer_init();
    con->headinfo.dir = buffer_init();
    con->headinfo.file = buffer_init();
    con->headinfo.session = buffer_init();
    con->headinfo.status = buffer_init();
    con->headinfo.filename = buffer_init();
    con->headinfo.opt = buffer_init();
    con->headinfo.length = buffer_init();
    con->headinfo.file_length = 0;
    con->headinfo.file_in = 0;
#endif


	con->fd = 0;
	con->ndx = -1;
	con->fde_ndx = -1;
	con->bytes_written = 0;
	con->bytes_read = 0;
	con->bytes_header = 0;
	con->loops_per_request = 0;

#define CLEAN(x) \
	con->x = buffer_init();

	CLEAN(request.uri);
	CLEAN(request.request_line);
	CLEAN(request.request);
	CLEAN(request.pathinfo);

	CLEAN(request.orig_uri);

	CLEAN(uri.scheme);
	CLEAN(uri.authority);
	CLEAN(uri.path);
	CLEAN(uri.path_raw);
	CLEAN(uri.query);

	CLEAN(physical.doc_root);
	CLEAN(physical.path);
	CLEAN(physical.basedir);
	CLEAN(physical.rel_path);
	CLEAN(physical.etag);
	CLEAN(parse_request);

	CLEAN(server_name);
	CLEAN(error_handler);
	CLEAN(dst_addr_buf);
#if defined USE_OPENSSL && ! defined OPENSSL_NO_TLSEXT
	CLEAN(tlsext_server_name);
#endif

#undef CLEAN
	con->write_queue = chunkqueue_init();
	con->read_queue = chunkqueue_init();
	con->request_content_queue = chunkqueue_init();
	chunkqueue_set_tempdirs(con->request_content_queue, srv->srvconf.upload_tempdirs);

	con->request.headers      = array_init();
	con->response.headers     = array_init();
	con->environment     = array_init();

	/* init plugin specific connection structures */

	con->plugin_ctx = calloc(1, (srv->plugins.used + 1) * sizeof(void *));

	con->cond_cache = calloc(srv->config_context->used, sizeof(cond_cache_t));
	config_setup_connection(srv, con);

	return con;
}

void connections_free(server *srv) {
	connections *conns = srv->conns;
	size_t i;

	for (i = 0; i < conns->size; i++) {
		connection *con = conns->ptr[i];

		connection_reset(srv, con);

		chunkqueue_free(con->write_queue);
		chunkqueue_free(con->read_queue);
		chunkqueue_free(con->request_content_queue);
		array_free(con->request.headers);
		array_free(con->response.headers);
		array_free(con->environment);

#define CLEAN(x) \
	buffer_free(con->x);
#if 1 //add by mengqy
        if(con->filefd != -1)
        {
            close(con->filefd);
            con->filefd = -1;
        }
        if(con->headinfo.opt->used == 0 && con->headinfo.dir->used !=0)
        {
            char tmpbuf[2048];
            sprintf(tmpbuf,"%s.ittmp",con->headinfo.dir->ptr);
            struct stat sbuf;
            if(con->filefdflag != 4 && con->filefdflag != 5 && -1 != stat(tmpbuf,&sbuf))
            {
                remove(tmpbuf);
            }
            buffer_reset(con->headinfo.dir);
            //printf("go to delete tmp\n");
        }
    if(con->filefdflag != 0){
        con->filefdflag = 0;
    sync();
    }
    //    con->filefdflag = 0;
        con->sendkey = 0;
        con->headinfo.file_length = 0;
        con->headinfo.file_in = 0;
		con->headinfo.tail_len = 0;
		CLEAN(headinfo.tail);
        CLEAN(headinfo.dir);
        CLEAN(headinfo.file);
        CLEAN(headinfo.session);
        CLEAN(headinfo.status);
        CLEAN(headinfo.filename);
        CLEAN(headinfo.opt);
        CLEAN(headinfo.length);

#endif


		CLEAN(request.uri);
		CLEAN(request.request_line);
		CLEAN(request.request);
		CLEAN(request.pathinfo);

		CLEAN(request.orig_uri);

		CLEAN(uri.scheme);
		CLEAN(uri.authority);
		CLEAN(uri.path);
		CLEAN(uri.path_raw);
		CLEAN(uri.query);

		CLEAN(physical.doc_root);
		CLEAN(physical.path);
		CLEAN(physical.basedir);
		CLEAN(physical.etag);
		CLEAN(physical.rel_path);
		CLEAN(parse_request);

		CLEAN(server_name);
		CLEAN(error_handler);
		CLEAN(dst_addr_buf);
#if defined USE_OPENSSL && ! defined OPENSSL_NO_TLSEXT
		CLEAN(tlsext_server_name);
#endif
#undef CLEAN
		free(con->plugin_ctx);
		free(con->cond_cache);

		free(con);
	}

	free(conns->ptr);
}


int connection_reset(server *srv, connection *con) {
	size_t i;

	plugins_call_connection_reset(srv, con);
#if 1 //add by mengqy
    if(con->filefd != -1)
    {
        close(con->filefd);
        con->filefd = -1;
    }
    if(con->headinfo.opt->used == 0 && con->headinfo.dir->used !=0)
    {
        printf("reading to reset\n");
        char tmpbuf[2048];
        sprintf(tmpbuf,"%s.ittmp",con->headinfo.dir->ptr);
        struct stat sbuf;
        if(con->filefdflag != 4 && con->filefdflag != 5 && -1 != stat(tmpbuf,&sbuf))
        {
            remove(tmpbuf);
        }
        buffer_reset(con->headinfo.dir);
    }
    if(con->filefdflag != 0){
        con->filefdflag = 0;
    sync();
    }
    con->sendkey = 0;
	con->headinfo.tail_len = 0;
	buffer_reset(con->headinfo.tail);
	buffer_reset(con->headinfo.part);
    buffer_reset(con->headinfo.dir);
    buffer_reset(con->headinfo.file);
    buffer_reset(con->headinfo.session);
    buffer_reset(con->headinfo.status);
    buffer_reset(con->headinfo.filename);
    buffer_reset(con->headinfo.opt);
    buffer_reset(con->headinfo.length);
    con->headinfo.file_length = 0;
    con->headinfo.file_in = 0;
#endif

	con->is_readable = 1;
	con->is_writable = 1;
	con->http_status = 0;
	con->file_finished = 0;
	con->file_started = 0;
	con->got_response = 0;

	con->parsed_response = 0;

	con->bytes_written = 0;
	con->bytes_written_cur_second = 0;
	con->bytes_read = 0;
	con->bytes_header = 0;
	con->loops_per_request = 0;

	con->request.http_method = HTTP_METHOD_UNSET;
	con->request.http_version = HTTP_VERSION_UNSET;

	con->request.http_if_modified_since = NULL;
	con->request.http_if_none_match = NULL;

	con->response.keep_alive = 0;
	con->response.content_length = -1;
	con->response.transfer_encoding = 0;

	con->mode = DIRECT;

#define CLEAN(x) \
	if (con->x) buffer_reset(con->x);

	CLEAN(request.uri);
	CLEAN(request.request_line);
	CLEAN(request.pathinfo);
	CLEAN(request.request);

	/* CLEAN(request.orig_uri); */

	CLEAN(uri.scheme);
	/* CLEAN(uri.authority); */
	/* CLEAN(uri.path); */
	CLEAN(uri.path_raw);
	/* CLEAN(uri.query); */

	CLEAN(physical.doc_root);
	CLEAN(physical.path);
	CLEAN(physical.basedir);
	CLEAN(physical.rel_path);
	CLEAN(physical.etag);

	CLEAN(parse_request);

	CLEAN(server_name);
	CLEAN(error_handler);
#if defined USE_OPENSSL && ! defined OPENSSL_NO_TLSEXT
	CLEAN(tlsext_server_name);
#endif
#undef CLEAN

#define CLEAN(x) \
	if (con->x) con->x->used = 0;

#undef CLEAN

#define CLEAN(x) \
		con->request.x = NULL;

	CLEAN(http_host);
	CLEAN(http_range);
	CLEAN(http_content_type);
#undef CLEAN
	con->request.content_length = 0;

	array_reset(con->request.headers);
	array_reset(con->response.headers);
	array_reset(con->environment);

	chunkqueue_reset(con->write_queue);
	chunkqueue_reset(con->request_content_queue);

	/* the plugins should cleanup themself */
	for (i = 0; i < srv->plugins.used; i++) {
		plugin *p = ((plugin **)(srv->plugins.ptr))[i];
		plugin_data *pd = p->data;

		if (!pd) continue;

		if (con->plugin_ctx[pd->id] != NULL) {
			log_error_write(srv, __FILE__, __LINE__, "sb", "missing cleanup in", p->name);
		}

		con->plugin_ctx[pd->id] = NULL;
	}

	/* The cond_cache gets reset in response.c */
	/* config_cond_cache_reset(srv, con); */

	con->header_len = 0;
	con->in_error_handler = 0;

	config_setup_connection(srv, con);

	return 0;
}
#if 1 //add by mengqy
void get_uri(char*dest,char*src)
{
    char *p = "0123456789ABCDEF";
    char *q = "0123456789abcdef";
    while(*src != '\0')
    {
        if(*src =='%')
        {
            int i;
            for(i=0;i<16;i++)
            {
                if(p[i] == *(src+1))
                {
                    break;
                }
            }
            if(i == 16)
            {
                for(i=0;i<16;i++)
                {
                    if(q[i] == *(src+1))
                    {
                        break;
                    }
                }
            }
            int j;
            for(j=0;j<16;j++)
            {
                if(p[j] == *(src+2))
                {
                    break;
                }
            }
            if(j == 16)
            {
                for(j=0;j<16;j++)
                {
                    if(q[j] == *(src+2))
                    {
                        break;
                    }
                }
            }
            *dest = (i<<4) |j;
            dest++;
            src +=3;
        }else{
            *dest++ = *src++;
        }
    }
    *dest = '\0';
}

static int set_upload_headinfo(connection *con,char*reqline,int len,server *srv)
{
    char * ptr;
    con_headinfo * info = &con->headinfo;
    char *index[]={"file=","session=","opt=","flag=",NULL};
    ptr = strchr(reqline,'&');
    if(ptr == NULL)
    {
        return -1;
    }
    *ptr++ = '\0';
    char uri[2056];
    char dst[2056];
    get_uri(uri,reqline);
    getupload(dst,uri,srv);
    info->win8flag = 0;
    buffer_copy_string(info->dir,dst);
    //printf("info->dir = %s\n",info->dir->ptr);
    while(ptr-reqline < len && ptr !=NULL)
    {
        int i = 0;
        char *start = ptr;
        ptr = strchr(start,'&');
        if(ptr != NULL)
        {
            *ptr++ = '\0';
        }else{
                ptr = strchr(start,' ');
                if(ptr != NULL)
                {
                    *ptr++ = '\0';
                }
        }
        while(index[i]!=NULL)
        {
            int n = strlen(index[i]);
            if(strncmp(start,index[i],n) == 0)
            {
                switch(i)
                {
                    case 0:
                        buffer_copy_string(info->file,start+n);
                        printf("info->file = %s\n",start+n);
                        break;
                    case 1:
                        buffer_copy_string(info->session,start+n);
                        printf("info->session = %s\n",start+n);
                        break;
                    case 2:
                        buffer_copy_string(info->opt,start+n);
                        printf("info->opt = %s\n",start+n);
            break;
            case 3:
            if(strcmp(start+n,"noweb") == 0){
                info->win8flag = 1;
            }
                        break;
                    default:
                    break;
                }

                break;
            }
            i++;
        }
    }
    return 1;

}
void getupload(char*dst,char*src,server *srv){
    if(srv->tmpupdir){
        if(strncmp(src,srv->tmpupdir,strlen(srv->tmpupdir)) == 0){
            strcpy(dst,src);
            return;
        }
    }
    if(srv->updir){
        snprintf(dst,1023,"%s%s",srv->updir,src);
        return;
    }else{
            strcpy(dst,src);
            return;
    }
}
//add 2014-04-21
static int PUT_parse_head(connection *con,server*srv){
    char reqline[1024];
    char dst[2056];
    int len;
    char *ptr;
    char *pend;
        con_headinfo * info = &con->headinfo;
    strncpy(reqline,con->request.request_line->ptr,1024);
#if 1
    if ((ptr = strstr(reqline,"/")) == NULL){
        return -1;
    }
#endif
    if( (pend = strchr(ptr,' ')) == NULL){
        return -1;
    }
    *pend = '\0';
        char uri[2056];
        get_uri(uri,ptr);
        con->headinfo.win8flag = 0;
        getupload(dst,uri,srv);
        buffer_copy_string(info->dir,dst);
    printf("PUT name=%s\n",info->dir->ptr);
    //printf("puturi=%s\n",uri);
    return 1;
}
static int parse_head( connection *con,server*srv)
{
    char reqline[1024];
    int len;
    char *ptr;
    strncpy(reqline,con->request.request_line->ptr,1024);
    if ((ptr = strchr(reqline , ' ')) == NULL)
    {
        return 0;
    }
    *ptr++ = '\0';
    len = ptr - reqline;
    if(strcmp(reqline,"POST") != 0)
    {
        return 0;
    }
    while(len != 1024 && *ptr == ' '){
        ptr++;
        len++;
    }
    if(len == 1024 )
    {
        return 0;
    }
    int n = sizeof("/upload.csp?uploadpath=")-1;
    if(strncmp(ptr,"/upload.csp?uploadpath=",n)== 0)
    {
        return set_upload_headinfo(con,ptr+n,strlen(ptr)-n,srv);
    }else{
        return 0;
    }

}

#endif
#if 1
static int is_uptmp_used(char *tmppath,connection *con,server *srv,off_t size)
{
    size_t i;
    struct stat sbuf;
    for(i=0;i < srv->conns->used;i++)
    {
        connection *ptr = srv->conns->ptr[i];
        if(ptr->fd == con->fd)
        {
            continue;
        }
        if(ptr->headinfo.dir->used == 0)
        {
            continue;
        }
        if(strcmp(ptr->headinfo.dir->ptr,con->headinfo.dir->ptr) == 0)
        {
            con->filefdflag = 4;
            return 1;
        }
    }
    sleep(2);
    if(stat(tmppath,&sbuf) == -1)
    {
        return 0;
    }
    if(size != sbuf.st_size)
    {
        con->filefdflag = 4;
        return 1;
    }else{
        return 0;
    }

}
static int getfilefd(buffer * dest,const char*src,connection *con,size_t size,server *srv)
{
    size_t tail_length = 0;
    char *ptr = NULL;
    char *type = NULL;
    size_t head_length = 0;
    size_t totalsize = 0;
    char *finalptr = NULL;
    char *firstptr = NULL;
   //add uri head
    buffer_append_string_len(dest,src,size);
    printf("dest=%s\n",dest->ptr);
    //get the total uri size.
    totalsize = dest->used - 1;

    data_string *cookie = NULL;
    if(con->request.http_method == HTTP_METHOD_PUT){
    con->headinfo.file_length = con->request.content_length;
    }else if (con->headinfo.win8flag == 1){
    con->headinfo.file_length = con->request.content_length;
	printf("win8\n");
    }else if (NULL != (cookie = (data_string *)array_get_element(con->request.headers, "win8"))) {
    if(cookie->value->used != 2 || cookie->value->ptr[0] != '1')
    {
        con->http_status = 413;
        con->keep_alive = 0;
        con->filefdflag = 6;
        return -2;
    }
    con->headinfo.file_length = con->request.content_length;
    }else{
   	printf("size =%d,totalsize =%d\n",size,totalsize);
   // printf("head=%s\n",dest->ptr);
    //complete the uri head
    if((type = strstr(dest->ptr,"\r\nContent-Type:")) == NULL)
    {
        if(totalsize > 2048)
        {
            con->http_status = 413; /* protocol error*/
            con->keep_alive = 0;
            con->filefdflag = 6;
            return -2;
        }
        return -1;//get more info
    }
    if((finalptr =strstr(type,"\r\n\r\n")) == NULL)
    {
        if(totalsize >2048)
        {
            con->http_status = 413; /* protocol error*/
            con->keep_alive = 0;
            con->filefdflag = 6;
            return -2;
        }
        return -1;//get more info
    }
    if((firstptr =strstr(dest->ptr,"\r\n\r\n")) == NULL)
    {
        con->http_status = 413; /* protocol error*/
        con->keep_alive = 0;
        con->filefdflag = 6;
        return -2;//the error
    }
    //get the head length
    *finalptr = '\0';
    head_length = strlen(dest->ptr)+4;
    //get the key length
    if ( (ptr = strstr(dest->ptr,"\r\n")) == NULL)
    {
        con->http_status = 413; /* protocol error*/
        con->keep_alive = 0;
        con->filefdflag = 6;
        return -2;
    }
    *ptr = '\0';
    //get the tail length
    if (firstptr == finalptr)
    {
        tail_length = strlen(dest->ptr)+6;
		con->headinfo.tail_len = tail_length;
		buffer_append_string_len(con->headinfo.part,dest->ptr ,strlen(dest->ptr));
		printf("con->headinfo.tail_len=%d\n",con->headinfo.tail_len);
    }else{
        tail_length = (strlen(dest->ptr)+4)*2+61;
		con->headinfo.tail_len = tail_length;
		buffer_append_string_len(con->headinfo.part,dest->ptr ,strlen(dest->ptr));
		printf("con->headinfo.tail_len2=%d\n",con->headinfo.tail_len);
        if ( (ptr = strstr(ptr+1,"\r\n\r\n")) == NULL)
        {
            con->http_status = 413; /* protocol error*/
            con->keep_alive = 0;
            con->filefdflag = 6;
            return -2;
        }
    }
    //get the file length
    con->headinfo.file_length = con->request.content_length - head_length - tail_length;
    printf("filelen=%lld\n",con->headinfo.file_length);
    printf("content_length=%lld\n",con->request.content_length);
    //get the filename
        char *filename;
        if ( (ptr = strstr(ptr+1,"Content-Disposition")) == NULL)
        {
            con->filefdflag = 6;
            return -2;
        }
        if ( (ptr = strstr(ptr+1,"filename=\"")) == NULL)
        {
            con->filefdflag = 6;
            return -2;
        }
        filename = ptr + 10;
        if ( (ptr = strstr(filename,"\"\r\n")) == NULL){
            if ( (ptr = strchr(filename,'\"')) == NULL)
            {
                    con->filefdflag = 6;
                    return -2;
            }
    }
        *ptr = '\0';
    printf("filename=%s\n",filename);
        if ( (ptr = strchr(filename,'\\')) != NULL)
        {
            filename = strrchr(filename,'\\');
            filename += 1;
        }
        if ( (ptr = strrchr(filename,'/')) != NULL)
        {
            filename = strrchr(filename,'/');
            filename += 1;
        }
        buffer_append_string(con->headinfo.dir,"/");
        //printf("filename = %s\n",filename);
        if(strncmp(con->headinfo.dir->ptr,"/tmp/ioos",strlen("/tmp/ioos")) == 0 ){
            buffer_append_string(con->headinfo.dir,"firmware.bin");
        }else{
            buffer_append_string(con->headinfo.dir,filename);
        }
    }
        printf("filelen=%lld\n",con->headinfo.file_length);
        printf("content_length=%lld\n",con->request.content_length);
        //printf("con->headinfo.dir = %s\n",con->headinfo.dir->ptr);
        //printf("filename = %s\n",con->headinfo.dir->ptr);

        //printf("head =%d,end =%d\n",head_length,tail_length);
        //printf("filesize = %d\n",(int)con->headinfo.file_length);
        char tmpbuf[2048];
        sprintf(tmpbuf,"%s.ittmp",con->headinfo.dir->ptr);
    printf("go to open tmp=%s\n",con->headinfo.dir->ptr);
#if 0
    if (con->headinfo.file_length >= (off_t)4*(off_t)1024*(off_t)1024*(off_t)1024 && \
        isfiletypeok(tmpbuf))
    {
        printf("error flag=%s\n",con->headinfo.opt->ptr);
        return -2;
    }
#endif
/*
    if(srv->sdflag){
        if(isnotsd(tmpbuf))
        {
            printf("it is not up to sd\n");
            con->headinfo.sdflag = 0;
        }else{
            printf("it is up to sd\n");
            con->headinfo.sdflag = 1;
        }
    }
*/
        struct stat sbuf;
        //printf("tmp = %s\n",tmpbuf);
        if(con->headinfo.opt->used !=0 && \
                (strcmp(con->headinfo.opt->ptr,"continue") == 0))
        {
            printf("1\n");
            if(-1 != stat(tmpbuf,&sbuf)){
                if (is_uptmp_used(tmpbuf, con, srv,sbuf.st_size))
                {
                    return -2;
                }
                con->headinfo.file_length += sbuf.st_size;
                con->headinfo.file_in += sbuf.st_size;
            }
            if(-1 != stat(con->headinfo.dir->ptr,&sbuf)){
                remove(con->headinfo.dir->ptr);
            }
            con ->filefd = open(tmpbuf,O_WRONLY|O_CREAT|O_APPEND,0777);
        if(con ->filefd == -1){
            upfile_fordir(tmpbuf);
                con ->filefd = open(tmpbuf,O_WRONLY|O_CREAT|O_APPEND,0777);
        }
        }else if(con->headinfo.opt->used !=0 && \
                (strcmp(con->headinfo.opt->ptr,"newfile") == 0)){
            printf("2\n");
            if(-1 != stat(tmpbuf,&sbuf)){
                if (is_uptmp_used(tmpbuf, con, srv,sbuf.st_size))
                {
                    return -2;
                }
                remove(tmpbuf);
            }
            con ->filefd = open(tmpbuf,O_WRONLY|O_CREAT|O_TRUNC,0777);
        if(con ->filefd == -1){
            upfile_fordir(tmpbuf);
            con ->filefd = open(tmpbuf,O_WRONLY|O_CREAT|O_TRUNC,0777);
        }
        }else{
            printf("?????,tmpbuf = %s\n",tmpbuf);
            if(-1 != stat(tmpbuf,&sbuf)){
                if (is_uptmp_used(tmpbuf, con, srv,sbuf.st_size))
                {
                    return -2;
                }
                remove(tmpbuf);
            }
            if(-1 != stat(con->headinfo.dir->ptr,&sbuf)){
                remove(con->headinfo.dir->ptr);
            }
            con ->filefd = open(tmpbuf,O_WRONLY|O_CREAT|O_TRUNC,0777);
        if(con ->filefd == -1){
            upfile_fordir(tmpbuf);
            con ->filefd = open(tmpbuf,O_WRONLY|O_CREAT|O_TRUNC,0777);
        }
        }
        if(con->filefd == -1)
        {
	    printf("can't open\n");
            con->http_status = 413; /* protocol error*/
            con->keep_alive = 0;
            con->filefdflag = 2;
            return -2;
        }
		if(con->headinfo.file_length < 0){
			con->headinfo.file_length = 0;
		}
        if(totalsize -head_length <con->headinfo.file_length)
        {
            //printf("totalsize -head_length = %d\n",(int)(totalsize -head_length));

        if(writen(con->filefd,&(dest->ptr[head_length]),totalsize -head_length)\
                != (int)(totalsize -head_length)){
            con->http_status = 413; /* protocol error*/
            con->keep_alive = 0;
            con->filefdflag = 3;
            return -2;

        }
        con->headinfo.file_in += totalsize - head_length;
        }else{
        if(con->headinfo.file_length > 0){
        if(writen(con->filefd,&(dest->ptr[head_length]), \
                con->headinfo.file_length) != (int)(con->headinfo.file_length)){
            con->http_status = 413; /* protocol error*/
            con->keep_alive = 0;
            con->filefdflag = 3;
            return -2;
        }
        con->headinfo.file_in += con->headinfo.file_length;
		buffer_append_string_len(con->headinfo.tail,&dest->ptr[head_length]+con->headinfo.file_length ,totalsize -head_length-con->headinfo.file_length);
		printf("tail4=%s\n",con->headinfo.tail->ptr);
        }else{
        	buffer_append_string_len(con->headinfo.tail,&dest->ptr[head_length],totalsize -head_length);
			printf("tail5=%s\n",con->headinfo.tail->ptr);
        }

        }
    return 0;
}
#endif

int checkfileend(connection *con)
{
	int len;
	if(con->headinfo.tail_len == (strlen(con->headinfo.part->ptr)+4)*2+61){
		printf("test1\n");
		if( (strncmp(con->headinfo.tail->ptr,"\r\n",2)== 0) &&
			!strncmp(con->headinfo.part->ptr,con->headinfo.tail->ptr +2,
			strlen(con->headinfo.part->ptr)))
		{
			printf("test3\n");
			return 0;
		
		}else{
			printf("test2\n");
			printf("con->headinfo.tail->used =%d\n",con->headinfo.tail->used);
			printf("con->headinfo.part->used=%d\n",con->headinfo.part->used);
			len = con->headinfo.tail->used -1 - (con->headinfo.part->used -1) - 6;
			

			con->headinfo.file_length +=  len;
			
			if (len != writen(con->filefd,con->headinfo.tail->ptr,len))
            {
					return -1;

            }
			con->headinfo.file_in += len;
			
			con->headinfo.tail_len -= len;
			
		
		}
	}
	return 0;
}

/**
 * handle all header and content read
 *
 * we get called by the state-engine and by the fdevent-handler
 */
static int connection_handle_read_state(server *srv, connection *con)  {
	connection_state_t ostate = con->state;
	chunk *c, *last_chunk;
	off_t last_offset;
	chunkqueue *cq = con->read_queue;
	chunkqueue *dst_cq = con->request_content_queue;
	int is_closed = 0; /* the connection got closed, if we don't have a complete header, -> error */

	if (con->is_readable) {
		con->read_idle_ts = srv->cur_ts;

		switch(connection_handle_read(srv, con)) {
		case -1:
			return -1;
		case -2:
			is_closed = 1;
			break;
		default:
			break;
		}
	}

	/* the last chunk might be empty */
	for (c = cq->first; c;) {
		if (cq->first == c && c->mem->used == 0) {
			/* the first node is empty */
			/* ... and it is empty, move it to unused */

			cq->first = c->next;
			if (cq->first == NULL) cq->last = NULL;

			c->next = cq->unused;
			cq->unused = c;
			cq->unused_chunks++;

			c = cq->first;
		} else if (c->next && c->next->mem->used == 0) {
			chunk *fc;
			/* next node is the last one */
			/* ... and it is empty, move it to unused */

			fc = c->next;
			c->next = fc->next;

			fc->next = cq->unused;
			cq->unused = fc;
			cq->unused_chunks++;

			/* the last node was empty */
			if (c->next == NULL) {
				cq->last = c;
			}

			c = c->next;
		} else {
			c = c->next;
		}
	}

	/* we might have got several packets at once
	 */

	switch(ostate) {
	case CON_STATE_READ:
		/* if there is a \r\n\r\n in the chunkqueue
		 *
		 * scan the chunk-queue twice
		 * 1. to find the \r\n\r\n
		 * 2. to copy the header-packet
		 *
		 */

		last_chunk = NULL;
		last_offset = 0;

		for (c = cq->first; c; c = c->next) {
			buffer b;
			size_t i;

			b.ptr = c->mem->ptr + c->offset;
			b.used = c->mem->used - c->offset;
			if (b.used > 0) b.used--; /* buffer "used" includes terminating zero */

			for (i = 0; i < b.used; i++) {
				char ch = b.ptr[i];

				if ('\r' == ch) {
					/* chec if \n\r\n follows */
					size_t j = i+1;
					chunk *cc = c;
					const char header_end[] = "\r\n\r\n";
					int header_end_match_pos = 1;

					for ( ; cc; cc = cc->next, j = 0 ) {
						buffer bb;
						bb.ptr = cc->mem->ptr + cc->offset;
						bb.used = cc->mem->used - cc->offset;
						if (bb.used > 0) bb.used--; /* buffer "used" includes terminating zero */

						for ( ; j < bb.used; j++) {
							ch = bb.ptr[j];

							if (ch == header_end[header_end_match_pos]) {
								header_end_match_pos++;
								if (4 == header_end_match_pos) {
									last_chunk = cc;
									last_offset = j+1;
									goto found_header_end;
								}
							} else {
								goto reset_search;
							}
						}
					}
				}
reset_search: ;
			}
		}
found_header_end:

		/* found */
		if (last_chunk) {
			buffer_reset(con->request.request);

			for (c = cq->first; c; c = c->next) {
				buffer b;

				b.ptr = c->mem->ptr + c->offset;
				b.used = c->mem->used - c->offset;

				if (c == last_chunk) {
					b.used = last_offset + 1;
				}

				buffer_append_string_buffer(con->request.request, &b);

				if (c == last_chunk) {
					c->offset += last_offset;

					break;
				} else {
					/* the whole packet was copied */
					c->offset = c->mem->used - 1;
				}
			}

			connection_set_state(srv, con, CON_STATE_REQUEST_END);
		} else if (chunkqueue_length(cq) > 64 * 1024) {
			log_error_write(srv, __FILE__, __LINE__, "s", "oversized request-header -> sending Status 414");

			con->http_status = 414; /* Request-URI too large */
			con->keep_alive = 0;
			connection_set_state(srv, con, CON_STATE_HANDLE_REQUEST);
		}
		break;
	case CON_STATE_READ_POST:
#if 1 //add by mengqy
    //con->request.http_method == HTTP_METHOD_PUT
#if 1
        if(con->request.http_method == HTTP_METHOD_PUT || (con->request.request_line->ptr &&     \
                (strstr(con->request.request_line->ptr,"/upload.csp?")) != NULL))
#endif
#if 0
        if(con->request.request_line->ptr &&     \
                (strstr(con->request.request_line->ptr,"/upload.csp?")) != NULL)
#endif
        {
	
       // printf("go to up %s\n",con->request.request_line->ptr);
            if(con->filefdflag == 0)
            {
        if(con->request.http_method == HTTP_METHOD_PUT){
                    con->filefdflag = PUT_parse_head(con,srv);

        }else{
                    con->filefdflag = parse_head(con,srv);
        }
            }
            if(con->filefdflag == -1)
            {
                con->http_status = 413; /* protocol error*/
                con->keep_alive = 0;
                con->filefdflag = 6;
                connection_set_state(srv, con, CON_STATE_HANDLE_REQUEST);
                //printf("buhuiba\n");
                break;

            }
            if(con->filefdflag != 0){


            for (c = cq->first; c && (dst_cq->bytes_in != (off_t)con->request.content_length); c = c->next) {
                off_t  weWant, weHave, toRead;
                weWant = con->request.content_length;
                assert(c->mem->used);
                weHave = c->mem->used - c->offset - 1;
                toRead = weHave > weWant ? weWant : weHave;
                if(con->filefd == -1)
                {

                    int flag;
                    //printf("go to get filefd\n");
                    //printf("toread = %d\n",(int)toRead);
                    flag = getfilefd(con->headinfo.status,c->mem->ptr + c->offset,con,(size_t)toRead,srv);
            if( flag != -1 )
            {
            buffer_free(con->headinfo.status);
                con->headinfo.status = buffer_init();
            }
                    if(flag == -2 ){
                        con->http_status = 413; /* protocol error*/
                        con->keep_alive = 0;
                        connection_set_state(srv, con, CON_STATE_HANDLE_REQUEST);
                        if(con->filefd != -1)
                        {
                            close(con->filefd);
                            con->filefd = -1;
                        }
                        break;
                    }
                }else

                {
                //printf("weHave = %d,weWant =%d\n", (int)weHave,(int)weWant);
                //printf("c->mem->used = %d,\n", (int)c->mem->used);
                //printf("dst_cq->bytes_in = %d,toread =%d,c->offset = %d\n", (int)dst_cq->bytes_in,(int)toRead,(int)c->offset);
                //printf("con->request.content_length = %d\n",(int)con->request.content_length);
                //printf("write-----------------fd = %d\n",con->filefd);
    off_t toRead2 = (off_t)(con->headinfo.file_length - con->headinfo.file_in);
//      printf("toRead=%d,toRead2=%d\n",(int)toRead,(int)toRead2);

        if(toRead < toRead2)
        {
#if 0
        if(srv->sdflag){
            if( (con->headinfo.count & 63) == 32)
            {
                fsync(con->filefd);
                printf("con->headinfo.count=%d\n",con->headinfo.count);

            }
#if 0
            if(con->headinfo.count > 30)
            {
                printf("go to fsync\n");
                fsync(con->filefd);
                con->headinfo.count=0;
            }
#endif
        }
#endif
            con->headinfo.count++;
            if ((con->headinfo.count >> 6) >= 20){
                        struct stat sbuf;
                        char tmpbuf[1024];
                        sprintf(tmpbuf,"%s.ittmp",con->headinfo.dir->ptr);
                con->headinfo.count=0;
                        if(-1 == stat(tmpbuf,&sbuf) ){
                    printf("file error\n");
                    con->http_status = 413; /* Request-Entity too large */
                    con->keep_alive = 0;
                                con->filefdflag = 3;
                    connection_set_state(srv, con, CON_STATE_HANDLE_REQUEST);
                                if(con->filefd != -1)
                                {
                        close(con->filefd);
                        con->filefd = -1;
                                }
                    printf("file error end\n");
                    break;
                }
            }
            //printf("write one size = %d,file_in =%d,toread =%d\n",(int)con->headinfo.file_length,(int)con->headinfo.file_in,(int)toRead);
            if (toRead != writen(con->filefd,c->mem->ptr + c->offset, (int)toRead))
            {
         printf("write error\n");
                    con->http_status = 413; /* Request-Entity too large */
                    con->keep_alive = 0;
                    con->filefdflag = 3;
                    connection_set_state(srv, con, CON_STATE_HANDLE_REQUEST);
                    if(con->filefd != -1)
                    {
                        close(con->filefd);
                        con->filefd = -1;
                    }
                    break;
            }
                con->headinfo.file_in +=toRead;
        }else {
                //printf("write two size = %d,file_in =%d,toread =%d\n",(int)con->headinfo.file_length,(int)con->headinfo.file_in,(int)toRead);
                //printf("toread1 = %d\n",(int)(con->headinfo.file_length - con->headinfo.file_in));
                //if(con->headinfo.file_length - con->headinfo.file_in != 0)
				if(toRead2 > 0)
				{
                //printf("toread2 = %d\n",(int)(con->headinfo.file_length - con->headinfo.file_in));
                if (toRead2 != writen(con->filefd,c->mem->ptr + c->offset,(int)toRead2))
            {
                    con->http_status = 413; /* Request-Entity too large */
                    con->filefdflag = 3;
                    con->keep_alive = 0;
                    connection_set_state(srv, con, CON_STATE_HANDLE_REQUEST);
                    if(con->filefd != -1)
                    {
                        close(con->filefd);
                        con->filefd = -1;
                    }
                    break;

            }
				con->headinfo.file_in += toRead2;
				buffer_append_string_len(con->headinfo.tail,c->mem->ptr + c->offset + (int)toRead2 ,(int)toRead - (int)toRead2);
				printf("tail1=%s\n",con->headinfo.tail->ptr);
				}else{
                	buffer_append_string_len(con->headinfo.tail,c->mem->ptr + c->offset,(int)toRead);
					printf("tail2=%s\n",con->headinfo.tail->ptr);
                }
				
				
                //con->headinfo.file_in += con->headinfo.file_length - con->headinfo.file_in;
        }
                }
                c->offset += toRead;
                dst_cq->bytes_in += toRead;
                //printf("con->headinfo.file_in =%d\n",(int)con->headinfo.file_in);
                //printf("dst_cq->bytes_in = %d,toread =%d,c->offset = %d\n", (int)dst_cq->bytes_in,(int)toRead,(int)c->offset);
            }
            //printf("writeend-----------------fd = %d\n",con->filefd);
            if (dst_cq->bytes_in == con->request.content_length) {
                struct stat sbuf;
                char tmpbuf[1024];
                sprintf(tmpbuf,"%s.ittmp",con->headinfo.dir->ptr);
        printf("go to end---\n");
		printf("tail3=%s\n",con->headinfo.tail->ptr);
		printf("part=%s\n",con->headinfo.part->ptr);
				checkfileend(con);
                if(-1 == stat(tmpbuf,&sbuf) ||  \
                        con->headinfo.file_length != sbuf.st_size || \
                        con->headinfo.file_in != sbuf.st_size){
                printf("file is not ok\n");
		    //printf("sbuf.st_size=%d, con->headinfo.file_length=%d,con->headinfo.file_in=%d\n",sbuf.st_size, con->headinfo.file_length,con->headinfo.file_in);
                    con->filefdflag = 5;
                    con->http_status = 413; /* Request-Entity too large */
                    con->keep_alive = 0;
                }
        printf("file is ok\n");

                connection_set_state(srv, con, CON_STATE_HANDLE_REQUEST);

                if(con->filefd != -1)
                {
                    close(con->filefd);
                    con->filefd = -1;
                }
            }
            break;
        }

        }
	printf("put2\n");
#endif

		for (c = cq->first; c && (dst_cq->bytes_in != (off_t)con->request.content_length); c = c->next) {
			off_t weWant, weHave, toRead;

			weWant = con->request.content_length - dst_cq->bytes_in;

			force_assert(c->mem->used);

			weHave = c->mem->used - c->offset - 1;

			toRead = weHave > weWant ? weWant : weHave;

			/* the new way, copy everything into a chunkqueue whcih might use tempfiles */
			if (con->request.content_length > 64 * 1024) {
				chunk *dst_c = NULL;
				/* copy everything to max 1Mb sized tempfiles */

				/*
				 * if the last chunk is
				 * - smaller than 1Mb (size < 1Mb)
				 * - not read yet (offset == 0)
				 * -> append to it
				 * otherwise
				 * -> create a new chunk
				 *
				 * */

				if (dst_cq->last &&
				    dst_cq->last->type == FILE_CHUNK &&
				    dst_cq->last->file.is_temp &&
				    dst_cq->last->offset == 0) {
					/* ok, take the last chunk for our job */

			 		if (dst_cq->last->file.length < 1 * 1024 * 1024) {
						dst_c = dst_cq->last;

						if (dst_c->file.fd == -1) {
							/* this should not happen as we cache the fd, but you never know */
							dst_c->file.fd = open(dst_c->file.name->ptr, O_WRONLY | O_APPEND);
							fd_close_on_exec(dst_c->file.fd);
						}
					} else {
						/* the chunk is too large now, close it */
						dst_c = dst_cq->last;

						if (dst_c->file.fd != -1) {
							close(dst_c->file.fd);
							dst_c->file.fd = -1;
						}
						dst_c = chunkqueue_get_append_tempfile(dst_cq);
					}
				} else {
					dst_c = chunkqueue_get_append_tempfile(dst_cq);
				}

				/* we have a chunk, let's write to it */

				if (dst_c->file.fd == -1) {
					/* we don't have file to write to,
					 * EACCES might be one reason.
					 *
					 * Instead of sending 500 we send 413 and say the request is too large
					 *  */

					log_error_write(srv, __FILE__, __LINE__, "sbs",
							"denying upload as opening to temp-file for upload failed:",
							dst_c->file.name, strerror(errno));

					con->http_status = 413; /* Request-Entity too large */
					con->keep_alive = 0;
					connection_set_state(srv, con, CON_STATE_HANDLE_REQUEST);

					break;
				}

				if (toRead != write(dst_c->file.fd, c->mem->ptr + c->offset, toRead)) {
					/* write failed for some reason ... disk full ? */
					log_error_write(srv, __FILE__, __LINE__, "sbs",
							"denying upload as writing to file failed:",
							dst_c->file.name, strerror(errno));

					con->http_status = 413; /* Request-Entity too large */
					con->keep_alive = 0;
					connection_set_state(srv, con, CON_STATE_HANDLE_REQUEST);

					close(dst_c->file.fd);
					dst_c->file.fd = -1;

					break;
				}

				dst_c->file.length += toRead;

				if (dst_cq->bytes_in + toRead == (off_t)con->request.content_length) {
					/* we read everything, close the chunk */
					close(dst_c->file.fd);
					dst_c->file.fd = -1;
				}
			} else {
				buffer *b;

				if (dst_cq->last &&
				    dst_cq->last->type == MEM_CHUNK) {
					b = dst_cq->last->mem;
				} else {
					b = chunkqueue_get_append_buffer(dst_cq);
					/* prepare buffer size for remaining POST data; is < 64kb */
					buffer_prepare_copy(b, con->request.content_length - dst_cq->bytes_in + 1);
				}
				buffer_append_string_len(b, c->mem->ptr + c->offset, toRead);
			}

			c->offset += toRead;
			dst_cq->bytes_in += toRead;
		}

		/* Content is ready */
		if (dst_cq->bytes_in == (off_t)con->request.content_length) {
			connection_set_state(srv, con, CON_STATE_HANDLE_REQUEST);
		}

		break;
	default: break;
	}

	/* the connection got closed and we didn't got enough data to leave one of the READ states
	 * the only way is to leave here */
	if (is_closed && ostate == con->state) {
		connection_set_state(srv, con, CON_STATE_ERROR);
	}

	chunkqueue_remove_finished_chunks(cq);

	return 0;
}

static handler_t connection_handle_fdevent(server *srv, void *context, int revents) {
	connection *con = context;

	joblist_append(srv, con);

	if (con->srv_socket->is_ssl) {
		/* ssl may read and write for both reads and writes */
		if (revents & (FDEVENT_IN | FDEVENT_OUT)) {
			con->is_readable = 1;
			con->is_writable = 1;
		}
	} else {
		if (revents & FDEVENT_IN) {
			con->is_readable = 1;
		}
		if (revents & FDEVENT_OUT) {
			con->is_writable = 1;
			/* we don't need the event twice */
		}
	}


	if (revents & ~(FDEVENT_IN | FDEVENT_OUT)) {
		/* looks like an error */

		/* FIXME: revents = 0x19 still means that we should read from the queue */
		if (revents & FDEVENT_HUP) {
			if (con->state == CON_STATE_CLOSE) {
				con->close_timeout_ts = srv->cur_ts - (HTTP_LINGER_TIMEOUT+1);
			} else {
				/* sigio reports the wrong event here
				 *
				 * there was no HUP at all
				 */
#ifdef USE_LINUX_SIGIO
				if (srv->ev->in_sigio == 1) {
					log_error_write(srv, __FILE__, __LINE__, "sd",
						"connection closed: poll() -> HUP", con->fd);
				} else {
					connection_set_state(srv, con, CON_STATE_ERROR);
				}
#else
				connection_set_state(srv, con, CON_STATE_ERROR);
#endif

			}
		} else if (revents & FDEVENT_ERR) {
			/* error, connection reset, whatever... we don't want to spam the logfile */
#if 0
			log_error_write(srv, __FILE__, __LINE__, "sd",
					"connection closed: poll() -> ERR", con->fd);
#endif
			connection_set_state(srv, con, CON_STATE_ERROR);
		} else {
			log_error_write(srv, __FILE__, __LINE__, "sd",
					"connection closed: poll() -> ???", revents);
		}
	}

	if (con->state == CON_STATE_READ ||
	    con->state == CON_STATE_READ_POST) {
		connection_handle_read_state(srv, con);
	}

	if (con->state == CON_STATE_WRITE &&
	    !chunkqueue_is_empty(con->write_queue) &&
	    con->is_writable) {

		if (-1 == connection_handle_write(srv, con)) {
			connection_set_state(srv, con, CON_STATE_ERROR);

			log_error_write(srv, __FILE__, __LINE__, "ds",
					con->fd,
					"handle write failed.");
		}
	}

	if (con->state == CON_STATE_CLOSE) {
		/* flush the read buffers */
		int len;
		char buf[1024];

		len = read(con->fd, buf, sizeof(buf));
		if (len == 0 || (len < 0 && errno != EAGAIN && errno != EINTR) ) {
			con->close_timeout_ts = srv->cur_ts - (HTTP_LINGER_TIMEOUT+1);
		}
	}

	return HANDLER_FINISHED;
}


connection *connection_accept(server *srv, server_socket *srv_socket) {
	/* accept everything */

	/* search an empty place */
	int cnt;
	sock_addr cnt_addr;
	socklen_t cnt_len;
	/* accept it and register the fd */

	/**
	 * check if we can still open a new connections
	 *
	 * see #1216
	 */

	if (srv->conns->used >= srv->max_conns) {
		return NULL;
	}

	cnt_len = sizeof(cnt_addr);

	if (-1 == (cnt = accept(srv_socket->fd, (struct sockaddr *) &cnt_addr, &cnt_len))) {
		switch (errno) {
		case EAGAIN:
#if EWOULDBLOCK != EAGAIN
		case EWOULDBLOCK:
#endif
		case EINTR:
			/* we were stopped _before_ we had a connection */
		case ECONNABORTED: /* this is a FreeBSD thingy */
			/* we were stopped _after_ we had a connection */
			break;
		case EMFILE:
			/* out of fds */
			break;
		default:
			log_error_write(srv, __FILE__, __LINE__, "ssd", "accept failed:", strerror(errno), errno);
		}
		return NULL;
	} else {
		connection *con;

		srv->cur_fds++;

		/* ok, we have the connection, register it */
#if 0
		log_error_write(srv, __FILE__, __LINE__, "sd",
				"appected()", cnt);
#endif
		srv->con_opened++;

		con = connections_get_new_connection(srv);

		con->fd = cnt;
		con->fde_ndx = -1;
#if 0
		gettimeofday(&(con->start_tv), NULL);
#endif
		fdevent_register(srv->ev, con->fd, connection_handle_fdevent, con);

		connection_set_state(srv, con, CON_STATE_REQUEST_START);

		con->connection_start = srv->cur_ts;
		con->dst_addr = cnt_addr;
		buffer_copy_string(con->dst_addr_buf, inet_ntop_cache_get_ip(srv, &(con->dst_addr)));
		con->srv_socket = srv_socket;

		if (-1 == (fdevent_fcntl_set(srv->ev, con->fd))) {
			log_error_write(srv, __FILE__, __LINE__, "ss", "fcntl failed: ", strerror(errno));
			return NULL;
		}
#ifdef USE_OPENSSL
		/* connect FD to SSL */
		if (srv_socket->is_ssl) {
			if (NULL == (con->ssl = SSL_new(srv_socket->ssl_ctx))) {
				log_error_write(srv, __FILE__, __LINE__, "ss", "SSL:",
						ERR_error_string(ERR_get_error(), NULL));

				return NULL;
			}

			con->renegotiations = 0;
			SSL_set_app_data(con->ssl, con);
			SSL_set_accept_state(con->ssl);

			if (1 != (SSL_set_fd(con->ssl, cnt))) {
				log_error_write(srv, __FILE__, __LINE__, "ss", "SSL:",
						ERR_error_string(ERR_get_error(), NULL));
				return NULL;
			}
		}
#endif
		return con;
	}
}


int connection_state_machine(server *srv, connection *con) {
	int done = 0, r;
#ifdef USE_OPENSSL
	server_socket *srv_sock = con->srv_socket;
#endif

	if (srv->srvconf.log_state_handling) {
		log_error_write(srv, __FILE__, __LINE__, "sds",
				"state at start",
				con->fd,
				connection_get_state(con->state));
	}

	while (done == 0) {
		size_t ostate = con->state;

		switch (con->state) {
		case CON_STATE_REQUEST_START: /* transient */
			if (srv->srvconf.log_state_handling) {
				log_error_write(srv, __FILE__, __LINE__, "sds",
						"state for fd", con->fd, connection_get_state(con->state));
			}

			con->request_start = srv->cur_ts;
			con->read_idle_ts = srv->cur_ts;

			con->request_count++;
			con->loops_per_request = 0;

			connection_set_state(srv, con, CON_STATE_READ);

			break;
		case CON_STATE_REQUEST_END: /* transient */
			if (srv->srvconf.log_state_handling) {
				log_error_write(srv, __FILE__, __LINE__, "sds",
						"state for fd", con->fd, connection_get_state(con->state));
			}

			buffer_reset(con->uri.authority);
			buffer_reset(con->uri.path);
			buffer_reset(con->uri.query);
			buffer_reset(con->request.orig_uri);

			if (http_request_parse(srv, con)) {
				 if(con->request.request_line->ptr != NULL){
                                printf("request=%s\n",con->request.request_line->ptr);
                        }

				/* we have to read some data from the POST request */

				connection_set_state(srv, con, CON_STATE_READ_POST);

				break;
			}
			 if(con->request.request_line->ptr != NULL){
                                printf("request=%s\n",con->request.request_line->ptr);
                        }

			connection_set_state(srv, con, CON_STATE_HANDLE_REQUEST);

			break;
		case CON_STATE_HANDLE_REQUEST:
			/*
			 * the request is parsed
			 *
			 * decided what to do with the request
			 * -
			 *
			 *
			 */
#if 1
			if(con->request.request_line->ptr != NULL){
				printf("%s\n",con->request.request_line->ptr);
			}
			 if(srv->reflag){
                         	data_string *host = NULL;
				printf("test1\n");
                         	if ((con->request.request_line->ptr != NULL)&& \
					(strncmp(con->request.request_line->ptr,"GET ", 4)== 0)&& \
					(NULL != (host = (data_string *)array_get_element(con->request.headers, "Host")))) {
					printf("test2\n");
                               		if(strcmp(host->value->ptr,srv->src_host) == 0){
						char oldbuf[1024];
						strncpy(oldbuf,con->request.request_line->ptr+4,1023);
						char *endp;
						char *ep;
						printf("req=%s\n",con->request.request_line->ptr);
						endp = strchr(oldbuf,' ');
						ep = strchr(oldbuf,'?');
						if((endp == NULL)){
							printf("it is error2\n");
							con->file_finished = 1;
                                                        con->http_status = 500;
                                                        connection_set_state(srv, con, CON_STATE_RESPONSE_START);
                                                        break;
						}
						*endp = '\0';
						if(ep != NULL){
							*ep++ = '\0';
						}else{
							ep = "";
						}
						printf("before=%s\n",oldbuf);
						printf("end=%s\n",ep);
						
                                        	char buf[1024];
					 	char *ip = NULL;
						char mac[32];
						char gw[32];
						char gw_id[32];
						struct sockaddr_in addr;
            					socklen_t len = sizeof(struct sockaddr_in);
            					if(getpeername(con->fd,(struct sockaddr *)&addr,&len) != 0){
							con->http_status = 500;
                					connection_set_state(srv, con, CON_STATE_RESPONSE_START);
							break;
						}
						int ret;
						unsigned char macint[6];
						char ifname[64];
						ret = get_val("/etc/redirect","interface",ifname,64);
						if(ret < 0){
							strcpy(ifname,"br-lan");
						}
						ret = obtain_mac(ifname, macint, 6);
						if(ret < 0){
							memset(macint,0,6);
						}
						sprintf(gw_id,"%02x:%02x:%02x:%02x:%02x:%02x", macint[0], macint[1], \
                                macint[2], macint[3], macint[4], macint[5]);  
						//ret = get_val("/tmp/ioosmac", "mac",gw_id,32);
					//	if(ret < 0){
					//		gw_id[0] = '\0';
					//	}
						printf("gw_id=%s\n",gw_id);
            					ip = inet_ntoa(addr.sin_addr);
						printf("ip = %s\n",ip);
						ret = arp_get_mac("/proc/net/arp", ip,mac,32);
						if(ret < 0){
							mac[0] = '\0';
						}
						printf("mac = %s\n",mac);
						if(net_getifaddr("br-lan", gw, 32) != 0){
							printf("it is error\n");
							con->http_status = 500;
                                                	connection_set_state(srv, con, CON_STATE_RESPONSE_START);
                                                	break;

						}
						printf("gw_ip =%s\n",gw);
                                        	//sprintf(buf,"%s/?g=%s&t=2060&d=%s&i%s&m%s&u%s&eu%s",\
						srv->dst_host,gw,gw_id,ip,mac,oldbuf,ep);
						//sprintf(buf,"http://%s/",srv->dst_host);
                                        	sprintf(buf,"http://%s%s?%s&gw_address=%s&gw_port=2060&gw_id=%s&ip=%s&mac=%s",\
						srv->dst_host,oldbuf,ep,gw,gw_id,ip,mac);
                                        	//sprintf(buf,"http://%s/?gw_address=%s&gw_port=2060&gw_id=%s&ip=%s&mac=%s&uri=%s&quri=%s",\
						srv->dst_host,gw,gw_id,ip,mac,oldbuf,ep);
                                        	//sprintf(buf,"%s/%s?gw_address=%s&gw_port=2060&gw_id=%s&ip=%s&mac=%s&uri=%s&quri=%s",\
						srv->dst_host,gw,gw_id,ip,mac,oldbuf,ep);
						printf("buf=%s\n",buf);
                                        	response_header_overwrite(srv, con, CONST_STR_LEN("Location"), buf, strlen(buf));
                                        	//Location: http://%s:80/
				      		con->http_status = 307;
						con->file_finished = 1;
                					/* we have something to send, go on */
                		      		connection_set_state(srv, con, CON_STATE_RESPONSE_START);
						buffer *bbb;
						bbb = chunkqueue_get_append_buffer(con->write_queue);
						break;
                                	}
		
                         	}
			 }
			//if(con->request.request_line->ptr != NULL){
			//	printf("%s\n",con->request.request_line->ptr);
			//}
#endif

#if 1
        //if(1){
        if(con->addrflag !=0){
            struct sockaddr_in addr;
            socklen_t len = sizeof(struct sockaddr_in);
            char *ip = NULL;
            int port;
            if(getpeername(con->fd,(struct sockaddr *)&addr,&len) == 0){
            ip = inet_ntoa(addr.sin_addr);
            port = ntohs(addr.sin_port);
            printf("addr=%s,port=%d\n",ip,port);
                        buffer *b;
                        response_header_overwrite(srv, con, CONST_STR_LEN("Content-Type"), CONST_STR_LEN("text/xml;charset=UTF-8"));
                        b = chunkqueue_get_append_buffer(con->write_queue);
            char str[1024];
            sprintf(str,"<?xml version=\"1.0\" ?><root><network><srcip><ip>%s</ip><port>%d</port><errno>0</errno></srcip></network></root>",ip,port);
                        buffer_append_string(b, str);
    }
        }
#endif
#if 1

                if (con->filefdflag != 0)
                {
            printf("upload end\n");
            printf("con->http_status=%d\n",con->http_status);
            printf("con->filefdflag=%d\n",con->filefdflag);
                    if(con->http_status == 0)
                    {//safe rename
            printf("con->http_status=%d\n",con->http_status);
                        char tmpbuf[1024];
                        int ret;
                        sprintf(tmpbuf,"%s.ittmp",con->headinfo.dir->ptr);
                        if(con->headinfo.opt->used !=0 && \
                            (strcmp(con->headinfo.opt->ptr,"newfile") == 0)){
                                    struct stat sbuf;
                                    if(-1 == stat(con->headinfo.dir->ptr,&sbuf))
                                    {
                                    ret = rename(tmpbuf,con->headinfo.dir->ptr);
                    printf("rename5\n");
                                        //continue;
                                    }else{
                            char destbuf[1024];
                            char *tail;
                            strcpy(destbuf,con->headinfo.dir->ptr);
                            tail = strrchr(destbuf,'.');
                            if (tail != NULL)
                            {
                                *tail = '\0';
                                tail++;
                            }
                            int i;
                            for(i=1; i < 100 ; i++)
                            {
                                char destname[1024];
                                if(tail == NULL)
                                {
                                    sprintf(destname,"%s(%d)",destbuf,i);
                                }else{
                                    sprintf(destname,"%s(%d).%s",destbuf,i,tail);
                                }
                                //if(con->headinfo.opt->used !=0 && \
                                    (strcmp(con->headinfo.opt->ptr,"newfile") == 0)){
                                    //struct stat sbuf;
                                    if(-1 != stat(destname,&sbuf))
                                    {
                                        continue;
                                    }

                                //}
                                if((ret=rename(tmpbuf,destname)) == 0)
                                {
                    printf("rename3\n");
                                    break;
                                }
                    printf("rename4\n");
                            }
                }
                            //struct stat sbuf;
                            //if(0 == stat(tmpbuf,&sbuf))
                            //{
                            //    ret = rename(tmpbuf,con->headinfo.dir->ptr);
                            //}else{
                            //    ret = -1;
                            //}
                        }else{
                                ret = rename(tmpbuf,con->headinfo.dir->ptr);
                printf("rename2\n");
                        }

                        if(ret != 0)
                        {
                            char destbuf[1024];
                            char *tail;
                            strcpy(destbuf,con->headinfo.dir->ptr);
                            tail = strrchr(destbuf,'.');
                            if (tail != NULL)
                            {
                                *tail = '\0';
                                tail++;
                            }
                            int i;
                            for(i=1; i < 100 ; i++)
                            {
                                char destname[1024];
                                if(tail == NULL)
                                {
                                    sprintf(destname,"%s(%d)",destbuf,i);
                                }else{
                                    sprintf(destname,"%s(%d).%s",destbuf,i,tail);
                                }
                                if(con->headinfo.opt->used !=0 && \
                                    (strcmp(con->headinfo.opt->ptr,"newfile") == 0)){
                                    struct stat sbuf;
                                    if(-1 != stat(destname,&sbuf))
                                    {
                                        continue;
                                    }

                                }
                                if(rename(tmpbuf,destname) == 0)
                                {
                                    break;
                                }
                            }

                        }
                    }
                    int flag;
                    char str[1024];
                    if(con->filefdflag == 1)
                    {
                        flag = 0;
                    }else{
                        flag = con->filefdflag;
                        switch(flag)
                        {
                            case 2:
                                con->http_status = 412;
                printf("error,%d\n",con->http_status);
                                break;
                            case 3:
                                con->http_status = 403;
                printf("error,%d\n",con->http_status);
                                break;
                            case 4:
                                con->http_status = 423;
                printf("error,%d\n",con->http_status);
                                break;
                            case 5:
                                con->http_status = 417;
                printf("error,%d\n",con->http_status);
                                break;
                            case 6:
                                con->http_status = 400;
                printf("error,%d\n",con->http_status);
                                break;
                            default:
                                con->http_status = 417;
                printf("error,%d\n",con->http_status);
                                flag = 6;
                                break;

                        }
                    }
                    if(con->headinfo.file->used != 0)
                    {
                    //printf("will do\n");
                        sprintf(str,"<!DOCTYPE html PUBLIC \"-//W3C//DTD XHTML 1.0 Strict//EN\" "
                            "\" http://www.w3.org/TR/xhtml1/DTD/xhtml1-strict.dtd\">"
                            "<html xmlns=\" http://www.w3.org/1999/xhtml\" class=\"I\">"
                            "<head><meta http-equiv=\"Content-Type\" content=\"text/html; charset=utf-8\" />"
                            "</head><body><script type=\"text/javascript\">"
                            "parent.window.uploadComplete(\"%15s\",%d);"
                            "</script></body></html>",con->headinfo.file->ptr,flag);
                        buffer *b;
                     //printf("end of doing\n");
                        b = chunkqueue_get_append_buffer(con->write_queue);
                        buffer_append_string(b, str);
                    }
             sync();
                     response_header_overwrite(srv, con, CONST_STR_LEN("Cache-Control"), CONST_STR_LEN("no-cache"));
                     response_header_overwrite(srv, con, CONST_STR_LEN("Pragma"), CONST_STR_LEN("no-cache"));
                     response_header_overwrite(srv, con, CONST_STR_LEN("Expires"), CONST_STR_LEN("0"));
                     response_header_overwrite(srv, con, CONST_STR_LEN("Content-Type"), CONST_STR_LEN("text/html"));
						if (con->http_status == 0) con->http_status = 201;
						con->file_finished = 1;
                					/* we have something to send, go on */
                		      		connection_set_state(srv, con, CON_STATE_RESPONSE_START);
						break;
						//buffer *bbb;
						//bbb = chunkqueue_get_append_buffer(con->write_queue);
#if 0
                     if(con->headinfo.session->used != 0)
                     {
                        sprintf(str,"SESSID=%s;",con->headinfo.session->ptr);
                        response_header_overwrite(srv, con, CONST_STR_LEN("Set-cookie"), str, strlen(str));
                     }
#endif
            printf("upload end end\n");


                }
#endif

			if (srv->srvconf.log_state_handling) {
				log_error_write(srv, __FILE__, __LINE__, "sds",
						"state for fd", con->fd, connection_get_state(con->state));
			}

			switch (r = http_response_prepare(srv, con)) {
			case HANDLER_FINISHED:
				if (con->mode == DIRECT) {
					if (con->http_status == 404 ||
					    con->http_status == 403) {
						/* 404 error-handler */

						if (con->in_error_handler == 0 &&
						    (!buffer_is_empty(con->conf.error_handler) ||
						     !buffer_is_empty(con->error_handler))) {
							/* call error-handler */

							con->error_handler_saved_status = con->http_status;
							con->http_status = 0;

							if (buffer_is_empty(con->error_handler)) {
								buffer_copy_string_buffer(con->request.uri, con->conf.error_handler);
							} else {
								buffer_copy_string_buffer(con->request.uri, con->error_handler);
							}
							buffer_reset(con->physical.path);

							con->in_error_handler = 1;

							connection_set_state(srv, con, CON_STATE_HANDLE_REQUEST);

							done = -1;
							break;
						} else if (con->in_error_handler) {
							/* error-handler is a 404 */

							con->http_status = con->error_handler_saved_status;
						}
					} else if (con->in_error_handler) {
						/* error-handler is back and has generated content */
						/* if Status: was set, take it otherwise use 200 */
					}
				}

				if (con->http_status == 0) con->http_status = 200;

				/* we have something to send, go on */
				connection_set_state(srv, con, CON_STATE_RESPONSE_START);
				break;
			case HANDLER_WAIT_FOR_FD:
				srv->want_fds++;

				fdwaitqueue_append(srv, con);

				connection_set_state(srv, con, CON_STATE_HANDLE_REQUEST);

				break;
			case HANDLER_COMEBACK:
				done = -1;
				/* fallthrough */
			case HANDLER_WAIT_FOR_EVENT:
				/* come back here */
				connection_set_state(srv, con, CON_STATE_HANDLE_REQUEST);

				break;
			case HANDLER_ERROR:
				/* something went wrong */
				connection_set_state(srv, con, CON_STATE_ERROR);
				break;
			default:
				log_error_write(srv, __FILE__, __LINE__, "sdd", "unknown ret-value: ", con->fd, r);
				break;
			}

			break;
		case CON_STATE_RESPONSE_START:
			/*
			 * the decision is done
			 * - create the HTTP-Response-Header
			 *
			 */

			if (srv->srvconf.log_state_handling) {
				log_error_write(srv, __FILE__, __LINE__, "sds",
						"state for fd", con->fd, connection_get_state(con->state));
			}

			if (-1 == connection_handle_write_prepare(srv, con)) {
				connection_set_state(srv, con, CON_STATE_ERROR);

				break;
			}

			connection_set_state(srv, con, CON_STATE_WRITE);
			break;
		case CON_STATE_RESPONSE_END: /* transient */
			/* log the request */

			if (srv->srvconf.log_state_handling) {
				log_error_write(srv, __FILE__, __LINE__, "sds",
						"state for fd", con->fd, connection_get_state(con->state));
			}

			plugins_call_handle_request_done(srv, con);

			srv->con_written++;

			if (con->keep_alive) {
				connection_set_state(srv, con, CON_STATE_REQUEST_START);

#if 0
				con->request_start = srv->cur_ts;
				con->read_idle_ts = srv->cur_ts;
#endif
			} else {
				switch(r = plugins_call_handle_connection_close(srv, con)) {
				case HANDLER_GO_ON:
				case HANDLER_FINISHED:
					break;
				default:
					log_error_write(srv, __FILE__, __LINE__, "sd", "unhandling return value", r);
					break;
				}

#ifdef USE_OPENSSL
				if (srv_sock->is_ssl) {
					switch (SSL_shutdown(con->ssl)) {
					case 1:
						/* done */
						break;
					case 0:
						/* wait for fd-event
						 *
						 * FIXME: wait for fdevent and call SSL_shutdown again
						 *
						 */

						break;
					default:
						log_error_write(srv, __FILE__, __LINE__, "ss", "SSL:",
								ERR_error_string(ERR_get_error(), NULL));
					}
				}
#endif
				if ((0 == shutdown(con->fd, SHUT_WR))) {
					con->close_timeout_ts = srv->cur_ts;
					connection_set_state(srv, con, CON_STATE_CLOSE);
				} else {
					connection_close(srv, con);
				}

				srv->con_closed++;
			}

			connection_reset(srv, con);

			break;
		case CON_STATE_CONNECT:
			if (srv->srvconf.log_state_handling) {
				log_error_write(srv, __FILE__, __LINE__, "sds",
						"state for fd", con->fd, connection_get_state(con->state));
			}

			chunkqueue_reset(con->read_queue);

			con->request_count = 0;

			break;
		case CON_STATE_CLOSE:
			if (srv->srvconf.log_state_handling) {
				log_error_write(srv, __FILE__, __LINE__, "sds",
						"state for fd", con->fd, connection_get_state(con->state));
			}

			/* we have to do the linger_on_close stuff regardless
			 * of con->keep_alive; even non-keepalive sockets may
			 * still have unread data, and closing before reading
			 * it will make the client not see all our output.
			 */
			{
				int len;
				char buf[1024];

				len = read(con->fd, buf, sizeof(buf));
				if (len == 0 || (len < 0 && errno != EAGAIN && errno != EINTR) ) {
					con->close_timeout_ts = srv->cur_ts - (HTTP_LINGER_TIMEOUT+1);
				}
			}

			if (srv->cur_ts - con->close_timeout_ts > HTTP_LINGER_TIMEOUT) {
				connection_close(srv, con);

				if (srv->srvconf.log_state_handling) {
					log_error_write(srv, __FILE__, __LINE__, "sd",
							"connection closed for fd", con->fd);
				}
			}

			break;
		case CON_STATE_READ_POST:
		case CON_STATE_READ:
			if (srv->srvconf.log_state_handling) {
				log_error_write(srv, __FILE__, __LINE__, "sds",
						"state for fd", con->fd, connection_get_state(con->state));
			}

			connection_handle_read_state(srv, con);
			break;
		case CON_STATE_WRITE:
			if (srv->srvconf.log_state_handling) {
				log_error_write(srv, __FILE__, __LINE__, "sds",
						"state for fd", con->fd, connection_get_state(con->state));
			}

			/* only try to write if we have something in the queue */
			if (!chunkqueue_is_empty(con->write_queue)) {
#if 0
				log_error_write(srv, __FILE__, __LINE__, "dsd",
						con->fd,
						"packets to write:",
						con->write_queue->used);
#endif
			}
			if (!chunkqueue_is_empty(con->write_queue) && con->is_writable) {
				if (-1 == connection_handle_write(srv, con)) {
					log_error_write(srv, __FILE__, __LINE__, "ds",
							con->fd,
							"handle write failed.");
					connection_set_state(srv, con, CON_STATE_ERROR);
				}
			}

			break;
		case CON_STATE_ERROR: /* transient */

			/* even if the connection was drop we still have to write it to the access log */
			if (con->http_status) {
				plugins_call_handle_request_done(srv, con);
			}
#ifdef USE_OPENSSL
			if (srv_sock->is_ssl) {
				int ret, ssl_r;
				unsigned long err;
				ERR_clear_error();
				switch ((ret = SSL_shutdown(con->ssl))) {
				case 1:
					/* ok */
					break;
				case 0:
					ERR_clear_error();
					if (-1 != (ret = SSL_shutdown(con->ssl))) break;

					/* fall through */
				default:

					switch ((ssl_r = SSL_get_error(con->ssl, ret))) {
					case SSL_ERROR_WANT_WRITE:
					case SSL_ERROR_WANT_READ:
						break;
					case SSL_ERROR_SYSCALL:
						/* perhaps we have error waiting in our error-queue */
						if (0 != (err = ERR_get_error())) {
							do {
								log_error_write(srv, __FILE__, __LINE__, "sdds", "SSL:",
										ssl_r, ret,
										ERR_error_string(err, NULL));
							} while((err = ERR_get_error()));
						} else if (errno != 0) { /* ssl bug (see lighttpd ticket #2213): sometimes errno == 0 */
							switch(errno) {
							case EPIPE:
							case ECONNRESET:
								break;
							default:
								log_error_write(srv, __FILE__, __LINE__, "sddds", "SSL (error):",
									ssl_r, ret, errno,
									strerror(errno));
								break;
							}
						}

						break;
					default:
						while((err = ERR_get_error())) {
							log_error_write(srv, __FILE__, __LINE__, "sdds", "SSL:",
									ssl_r, ret,
									ERR_error_string(err, NULL));
						}

						break;
					}
				}
				ERR_clear_error();
			}
#endif

			switch(con->mode) {
			case DIRECT:
#if 0
				log_error_write(srv, __FILE__, __LINE__, "sd",
						"emergency exit: direct",
						con->fd);
#endif
				break;
			default:
				switch(r = plugins_call_handle_connection_close(srv, con)) {
				case HANDLER_GO_ON:
				case HANDLER_FINISHED:
					break;
				default:
					log_error_write(srv, __FILE__, __LINE__, "sd", "unhandling return value", r);
					break;
				}
				break;
			}

			connection_reset(srv, con);
#if 0
			/* close the connection */
			if ((0 == shutdown(con->fd, SHUT_WR))) {
				con->close_timeout_ts = srv->cur_ts;
				connection_set_state(srv, con, CON_STATE_CLOSE);

				if (srv->srvconf.log_state_handling) {
					log_error_write(srv, __FILE__, __LINE__, "sd",
							"shutdown for fd", con->fd);
				}
			} else {
				connection_close(srv, con);
			}
#endif
			connection_close(srv, con);

			con->keep_alive = 0;

			srv->con_closed++;

			break;
		default:
			log_error_write(srv, __FILE__, __LINE__, "sdd",
					"unknown state:", con->fd, con->state);

			break;
		}

		if (done == -1) {
			done = 0;
		} else if (ostate == con->state) {
			done = 1;
		}
	}

	if (srv->srvconf.log_state_handling) {
		log_error_write(srv, __FILE__, __LINE__, "sds",
				"state at exit:",
				con->fd,
				connection_get_state(con->state));
	}

	switch(con->state) {
	case CON_STATE_READ_POST:
	case CON_STATE_READ:
	case CON_STATE_CLOSE:
		fdevent_event_set(srv->ev, &(con->fde_ndx), con->fd, FDEVENT_IN);
		break;
	case CON_STATE_WRITE:
		/* request write-fdevent only if we really need it
		 * - if we have data to write
		 * - if the socket is not writable yet
		 */
		if (!chunkqueue_is_empty(con->write_queue) &&
		    (con->is_writable == 0) &&
		    (con->traffic_limit_reached == 0)) {
			fdevent_event_set(srv->ev, &(con->fde_ndx), con->fd, FDEVENT_OUT);
		} else {
			fdevent_event_del(srv->ev, &(con->fde_ndx), con->fd);
		}
		break;
	default:
		fdevent_event_del(srv->ev, &(con->fde_ndx), con->fd);
		break;
	}

	return 0;
}
