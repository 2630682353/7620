#include<zlib.h>
#include "zlib.h"
#include <stdio.h>
#include"crc32.h"
#  include <string.h>
#  include <stdlib.h>

#  include <sys/types.h>
#  include <sys/mman.h>
#  include <sys/stat.h>
#include <assert.h>

#define CHUNK 8192



int  inflate_read(char  *source,int  len,char  **dest,int  gzip)   
{   
    int  ret;   
    unsigned  have;   
   z_stream  strm;   
    unsigned  char  out[CHUNK];   
    int  totalsize  =  0;   
      
    /*  allocate  inflate  state  */   
    strm.zalloc  =  Z_NULL;   
    strm.zfree  =  Z_NULL;   
    strm.opaque  =  Z_NULL;   
    strm.avail_in  =  0;   
    strm.next_in  =  Z_NULL;   
      
    if (gzip)   
    ret = inflateInit2(&strm,  47);   
   else 
      ret = inflateInit(&strm);   
      
    if (ret !=  Z_OK)   
    return  ret;   
     
    strm.avail_in  =  len;   
    strm.next_in  =  (unsigned char *)source;   
         /*  run  inflate()  on  input  until  output  buffer  not  full  */   
    do {   
        strm.avail_out  =  CHUNK;   
        strm.next_out  =  out;   
        ret  =  inflate(&strm,  Z_NO_FLUSH);   
        assert(ret  !=  Z_STREAM_ERROR);    /*  state  not  clobbered  */   
          
        switch  (ret)  {   
           case  Z_NEED_DICT:   
           ret  =  Z_DATA_ERROR;          /*  and  fall  through  */   
            case  Z_DATA_ERROR:   
            case  Z_MEM_ERROR:   
                inflateEnd(&strm);   
                return  ret;           }   
                  have  =  CHUNK  -  strm.avail_out;   
        totalsize  +=  have;   
        *dest  =  realloc(*dest,totalsize);          memcpy(*dest  +  totalsize  -  have,out,have);   
    } while  (strm.avail_out  ==  0);   
	printf("total=%d\n",totalsize);
          /*  clean  up  and  return  */   
    (void)inflateEnd(&strm);         
    return  ret  ==  Z_STREAM_END  ?  Z_OK  :  Z_DATA_ERROR;   
}   





int deflate_file_to_buffer_gzip(char*dest,int len, char *start, off_t st_size, time_t mtime) {
    unsigned char *c;
    unsigned long crc;
    z_stream z;
    int c_len = 0;


    z.zalloc = Z_NULL;
    z.zfree = Z_NULL;
    z.opaque = Z_NULL;

    if (Z_OK != deflateInit2(&z,
                 Z_DEFAULT_COMPRESSION,
                 Z_DEFLATED,
                 -MAX_WBITS,  /* supress zlib-header */
                 8,
                 Z_DEFAULT_STRATEGY)) {
        return -1;
    }

    z.next_in = (unsigned char *)start;
    z.avail_in = st_size;
    z.total_in = 0;


//    buffer_prepare_copy(p->b, (z.avail_in * 1.1) + 12 + 18);

    /* write gzip header */

    //c = (unsigned char *)p->b->ptr;
    c = (unsigned char *)dest;
    c[0] = 0x1f;
    c[1] = 0x8b;


    c[2] = Z_DEFLATED;
    c[3] = 0; /* options */
    c[4] = (mtime >>  0) & 0xff;
    c[5] = (mtime >>  8) & 0xff;
    c[6] = (mtime >> 16) & 0xff;
    c[7] = (mtime >> 24) & 0xff;
    c[8] = 0x00; /* extra flags */
    c[9] = 0x03; /* UNIX */

    c_len = 10;
    z.next_out = (unsigned char *)dest+c_len;
    z.avail_out = len - c_len - 8;
    z.total_out = 0;

    if (Z_STREAM_END != deflate(&z, Z_FINISH)) {
        deflateEnd(&z);
        return -1;
    }

    /* trailer */
    c_len += z.total_out;

    crc = generate_crc32c(start, st_size);

    c = (unsigned char *)dest+c_len;

    c[0] = (crc >>  0) & 0xff;
    c[1] = (crc >>  8) & 0xff;
    c[2] = (crc >> 16) & 0xff;
    c[3] = (crc >> 24) & 0xff;
    c[4] = (z.total_in >>  0) & 0xff;
    c[5] = (z.total_in >>  8) & 0xff;
    c[6] = (z.total_in >> 16) & 0xff;
    c[7] = (z.total_in >> 24) & 0xff;
    c_len += 8;

    if (Z_OK != deflateEnd(&z)) {
        return -1;
    }

    return c_len;
}
#if 0
int main(int argc,char**argv){
	int a;
	FILE *fp;
	fp =fopen(argv[1],"r");
	if(fp == NULL){
		return -1;
	}
	char buf[8192];
	char bufs[8192];
	int n;
	memset(bufs,0,8192);
	memset(buf,0,8192);
	n = fread(bufs,1,8192,fp);
	struct stat st;
	stat(argv[1],&st);
	printf("n=%d,time=%u\n",n,st.st_mtime);
	int gn;
	gn = deflate_file_to_buffer_gzip(buf,8192, bufs, n, 0);
	//gn = deflate_file_to_buffer_gzip(buf,8192, bufs, n, st.st_mtime);
	printf("gn=%d\n",gn);
	printf("bufs=%s\n",bufs);
	printf("buf=%s\n",buf);
	int rlen;
	char*buf3= NULL;
	rlen = inflate_read(buf,gn,&buf3,1);
	printf("%s\n",buf3) ;
	printf("rlen=%d\n",rlen) ;
	if(strcmp(buf3,bufs) != 0){
		printf("very bad\n");
	}else{
		printf("it is ok\n");
	}
	return 0;
}
#endif
