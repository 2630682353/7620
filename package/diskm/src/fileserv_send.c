#define _LARGEFILE_SOURCE
#define _LARGEFILE64_SOURCE
#define _FILE_OFFSET_BITS 64
#define FILENAME "bigfile"
#include<stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/sendfile.h>
#include <stdlib.h>
#include <errno.h>
#include <time.h>
#include <string.h>
#define MAXSEND (2147483647)
#define MAXFILE 128
#define MAXAGAIN 5
#define MAXCTIME 30
#define MAX(a,b) (a) > (b)?(a):(b)
#define MIN(a,b) (a) < (b)?(a):(b)
#define BEGINSIZE 1024
#define FLAGSIZE  4
#define ENDSIZE   4096
int mark=0xccddeeff;

typedef struct rc4_key {
    unsigned char state[256];
    unsigned char x;
    unsigned char y;
} rc4_key;
rc4_key rc4key;
void rc4(unsigned char *buffer_ptr, int buffer_len, rc4_key * key);
void prepare_key(unsigned char *key_data_ptr, int key_data_len,
                 rc4_key * key);

int checkfile(off_t total){
	if(total < 4){
		return 0;
	}
	char buf[4];
	if(read(4,buf,4) !=4){
		return 0;
	}
	if(memcmp(buf,&mark,4) == 0){
		return 1;
	}else{
		return 0;
	}
	
}
void fileseek(off_t start,off_t total){
	int seek;
	seek = start;
	if(start < 4096){
		seek = 0;
	}
//	seek = MIN(seek,total -4096);
//	if(start < 4096){
//		seek = 1028;
//	}
#if 1
	
	printf("WARING==========:seek=%d\n",seek);
	lseek(4,seek,SEEK_SET);
#endif
}
off_t getmin_begin(off_t start,off_t total){
	off_t min;
	if(start >= 4096){
		return 0;
	}
	min = 4096 -start;
	min = MIN(min,total-4096);
	return min;
}
off_t getmin_mid(off_t start,off_t total,off_t file_total){
	if(start > file_total - 4096){
		return 0;
	}
	off_t max;
	if(start < 4096){
		max = file_total - 4096 - 4096;
	}else{
		max = file_total - 4096 - start;
	}
	max = MIN(total,max);
	return max;
}
off_t getmin_end(off_t start,off_t total,off_t file_total){
	off_t max;
	if(start > file_total - 4096){
		max = start + 4096 - file_total;
	}else{
		max = 4096;
	}
	max = MIN(total,max);
	return max;
}
int write_begin(off_t start,off_t total,off_t*cur){
	char buf[4096];
	//int begin = 0;
	//printf("cur=%lld\n",*cur);
	off_t send = getmin_begin(start,total - *cur);
	//printf("WARING==========:startsend=%lld,begin=%d\n",send,begin);
	//printf("WARING==========:startsend=%lld,begin=%d\n",send,begin);
	//*cur = *cur +send;
	if(send <= 0){
		return 0;
	}
	//begin = start;
	//printf("beginsend=%lld\n",send);
#if 1
	int n;
	int m=0;
	n = read(4,buf,4096);
	//printf("readn=%d\n",n);
	rc4((unsigned char *)buf, n, &rc4key);
	m = write(3,&buf[start],n);
	//n = write(3,&buf[begin],send);
	*cur = *cur +m;
	//printf("n=%d\n",m);
#endif
	return 0;
}
int write_mid(off_t start,off_t total,off_t*cur,off_t file_total){
	//printf("cur=%lld\n",*cur);
	off_t send = getmin_mid(start,total - *cur,file_total-1028);
	//*cur = *cur +send;
	//printf("midlesend=%lld\n",send);
	//printf("WARING==========:midsend=%lld\n",send);
	if(send == 0){
		return 0;
	}
#if 1
	off_t cc = 0;
	while(cc < send)
	{
		size_t send_size = MIN(send-cc,MAXSEND);
		off_t cur_size;
		cur_size=sendfile(3, 4, NULL,send_size);
		if(cur_size == -1)
		{
			
			printf("mid senndfile error");
			close(3);
			close(4);
			return -1;
			break;
		}
		*cur = *cur +cur_size;
		cc = cc + cur_size;
	}
#endif
	return 0;
}
int write_end(off_t start,off_t total,off_t*cur,off_t file_total){
	int begin = 0;
	if(start > file_total -4096){
		begin = start +4096 - file_total;
	}
	char buf[ENDSIZE];
	off_t send = getmin_end(start,total- *cur,file_total-4);
	//*cur = *cur +send;
	//printf("WARING==========:endsend=%lld,start=%d\n",send,begin);
	if(send == 0){
		return 0;
	}
#if 1
	int n;
	n = read(4,buf,4096);
	//printf("readn=%d\n",n);
	rc4((unsigned char *)buf, ENDSIZE, &rc4key);
	if(n != send){
		n =write(3,&buf[begin],send);
		return -1;
	}
	n = write(3,&buf[begin],send);
	*cur = *cur +n;
	//printf("readn=%d\n",n);
#endif
	return 0;
}
void close_fd(void)
{
	int i;
    	for(i=5;i<MAXFILE;i++)
        	close(i);
}
int parse(char* destname ,char*name);
int main(int argc,char**argv)
{
	off_t tol_size = 0;
	off_t size = 0;
	//parse(argv[2] ,argv[1]);
	//parse(argv[2] ,argv[1]);
	//return 0;
	if(argc < 2 )
	{
		printf("usage:sendfile filelen\n");
		return -1;
	}
#if 1
	int i=0;
	for(i=0;i<argc;i++){
		printf("argv[%d]=%s",i,argv[i]);
	}
#endif
//	close_fd();
	size = atoll(argv[1]);
	if(size < 0)
	{
		printf("atoll error!\n");
		printf("%s\n",argv[1]);
		return -1;
	}
		printf("size=%s\n",argv[1]);
#if 0
	switch (fork()) {
		case -1:
			return -1;
		case 0:
					//child = 1;
			break;
		default:
			return -1;
			break;
			}
#endif
	off_t start=0;
	off_t file_total=0;
	if(argc == 4){
		start = atoll(argv[2]);
		if(start < 0)
		{
			printf("atoll error!\n");
			printf("%s\n",argv[2]);
			return -1;
		}
		file_total = atoll(argv[3]);
		if(file_total < 0)
		{
			printf("atoll error!\n");
			printf("%s\n",argv[3]);
			return -1;
		}
#if 0
		if (checkfile(file_total)){
#endif
    		prepare_key((unsigned char *)"27Dvduoa9eQ1PHu1EfVGM36ZYMQ70eRk", strlen("27Dvduoa9eQ1PHu1EfVGM36ZYMQ70eRk"), &rc4key);
		fileseek(start,file_total );
		printf("FILE:%lld\n",tol_size);
		write_begin(start,size,&tol_size);
		int ret;
		printf("FILE:%lld\n",tol_size);
		ret =write_mid(start,size,&tol_size,file_total);
		if(ret !=0){
			close(3);
			close(4);
			printf("error send=%lld\n",tol_size);
		}
		printf("FILE:%lld\n",tol_size);
		write_end(start,size,&tol_size,file_total);
		printf("FILE:%lld\n",tol_size);
		close(3);
		close(4);
		printf("send=%lld\n",tol_size);
		return 0;
#if 0
		}else{
			lseek(4,start,SEEK_SET);
		}
#endif
	}
	time_t timebegin = time(NULL);
	int again = 0;
	
	while(tol_size < size)
	{
		size_t send_size = ( (size - tol_size) < MAXSEND ) ? (size - tol_size) : MAXSEND;
		off_t cur_size;
		cur_size=sendfile(3, 4, NULL,send_size);
		if(cur_size == -1)
		{
			
			printf("senndfile error");
		//	perror("sendfie error!");
			if(errno == EAGAIN)
			{
				time_t timenow = time(NULL);
				if( (again >= MAXAGAIN) ||  (timenow < timebegin) || (timenow > timebegin + MAXCTIME))
				{
					close(3);
					close(4);
					return 0;
				}else{
					again++;
					continue;
				}
			}
			close(3);
			close(4);
			printf("send error end,sendsize=%lld\n",tol_size);
			break;
			
		}
		tol_size = tol_size + cur_size;
	}
	close(3);
	close(4);
	printf("send=%lld\n",tol_size);
	return 0;
}



#define swap_byte(x,y) t = *(x); *(x) = *(y); *(y) = t

void prepare_key(unsigned char *key_data_ptr, int key_data_len,
                 rc4_key * key)
{
    unsigned char t;
    unsigned char index1;
    unsigned char index2;
    unsigned char *state;
    short counter;

    state = &key->state[0];
    for (counter = 0; counter < 256; counter++) {
        state[counter] = counter;
    }
    key->x = 0;
    key->y = 0;
    index1 = 0;
    index2 = 0;
    for (counter = 0; counter < 256; counter++) {
        index2 = (key_data_ptr[index1] + state[counter] + index2) % 256;
        swap_byte(&state[counter], &state[index2]);
        index1 = (index1 + 1) % key_data_len;
    }
}

void rc4(unsigned char *buffer_ptr, int buffer_len, rc4_key * key)
{
    unsigned char t;
    unsigned char x;
    unsigned char y;
    unsigned char *state;
    unsigned char xorIndex;
    short counter;
    rc4_key newkey;
    memcpy(&newkey,key,sizeof(rc4_key));
    //prepare_key((unsigned char *)"123456", 6, &newkey);

    x = newkey.x;
    y = newkey.y;
    state = &newkey.state[0];
    for (counter = 0; counter < buffer_len; counter++) {
        x = (x + 1) % 256;
        y = (state[x] + y) % 256;
        swap_byte(&state[x], &state[y]);
        xorIndex = (state[x] + state[y]) % 256;
        buffer_ptr[counter] ^= state[xorIndex];
    }
    newkey.x = x;
    newkey.y = y;
}
int parse(char* destname ,char*name){
	struct stat st;
	stat(name,&st);
	off_t total = st.st_size -1028;
	int fd_out;
	int fd_in;
	printf("name=%s,destname=%s\n",name,destname);
	fd_in = open(name,O_RDONLY,0777);
    if(fd_in == -1){
        return -1;
    }
	fd_out = open(destname,O_CREAT|O_TRUNC|O_RDWR,0777);
    if(fd_out == -1){
        return -1;
    }
	off_t mid;
	int end = 4096;
	int head = 4096;
	int n;
	char tmpbuf[4096];	
    lseek(fd_in,1028,SEEK_SET);
    	prepare_key((unsigned char *)"27Dvduoa9eQ1PHu1EfVGM36ZYMQ70eRk", strlen("27Dvduoa9eQ1PHu1EfVGM36ZYMQ70eRk"), &rc4key);
	n = read(fd_in,tmpbuf,4096);
	rc4((unsigned char *)tmpbuf, 4096, &rc4key);
	write(fd_out,tmpbuf,n);
	mid = total - end-head;
    while(mid >0){
	n = MIN(mid,4096);
	n = read(fd_in,tmpbuf,n);
        if(write(fd_out,tmpbuf,n) != n){
                close(fd_in);
                close(fd_out);
	    	remove(destname);
                return -1;
        }
        mid -= n;
    }
	n = read(fd_in,tmpbuf,4096);
//	memset(tmpbuf,'1',4096);
//	tmpbuf[4095]='\0';
//	printf("tmpbuf=%s\n",tmpbuf);
	rc4((unsigned char *)tmpbuf, 4096, &rc4key);
//	rc4(tmpbuf, 4096, &rc4key);
//	printf("tmpbuf=%s\n",tmpbuf);
	write(fd_out,tmpbuf,n);
	close(fd_in);
	close(fd_out);
	return 0;
	
}

int parse2(char* destname ,char*name){
	struct stat st;
	stat(name,&st);
	off_t total = st.st_size;
	int fd_out;
	int fd_in;
	printf("name=%s,destname=%s\n",name,destname);
	fd_in = open(name,O_RDONLY,0777);
    if(fd_in == -1){
        return -1;
    }
	fd_out = open(destname,O_CREAT|O_TRUNC|O_RDWR,0777);
    if(fd_out == -1){
        return -1;
    }
	off_t mid;
	int end = 4096;
	mid = total - end;
	char tmpbuf[4096];
	write(fd_out,tmpbuf,1028);
    //lseek(fd_in,1028,SEEK_SET);
	int n;
    while(mid >0){
	n = MIN(mid,4096);
	n = read(fd_in,tmpbuf,n);
        if(write(fd_out,tmpbuf,n) != n){
                close(fd_in);
                close(fd_out);
	    	remove(destname);
                return -1;
        }
        mid -= n;
    }
    	prepare_key((unsigned char *)"27Dvduoa9eQ1PHu1EfVGM36ZYMQ70eRk", strlen("27Dvduoa9eQ1PHu1EfVGM36ZYMQ70eRk"), &rc4key);
	n = read(fd_in,tmpbuf,4096);
//	memset(tmpbuf,'1',4096);
//	tmpbuf[4095]='\0';
//	printf("tmpbuf=%s\n",tmpbuf);
	rc4((unsigned char *)tmpbuf, 4096, &rc4key);
//	rc4(tmpbuf, 4096, &rc4key);
//	printf("tmpbuf=%s\n",tmpbuf);
	write(fd_out,tmpbuf,n);
	close(fd_in);
	close(fd_out);
	return 0;
	
}
