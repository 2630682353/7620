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
#include <sys/types.h>
#include <dirent.h>
#include <string.h>
#include <sys/file.h>
#include <sys/types.h>
#include <sys/vfs.h>
#include <time.h>
#define CP 0
#define MV 1
#define ERROR -1
#include <sys/wait.h>
#define MAXSEND (2147483647)
#define MAXFILE 128
//#define CP_FILE "fileserv_cp"
#define CP_FILE "/tmp/fileserv_cp"
#define CP_LOCK "/var/lock/fileserv_cp"
#define W_LOG_SIZE 1048576
void check_uri(char*dest,char*src);
pid_t pid;
int stopflag;
static int file_lock(char *file,int *fd);
static void file_unlock(int fd);
typedef int (file_loop_callback)(char*line,void*data);
static int getstopflag_callback(char*line,void*data){
    char *tmp=(char*)data;
    printf("line=%s\n",line);
    printf("tmp =%s\n",tmp);
    if((strncmp(tmp,line,strlen(tmp)) == 0)&&(line[strlen(tmp)] == '*')&&(stopflag == 0)){
        char*p;
        p = strchr(line,'*');
        if(p == NULL){
            return 0;
        }
        p++;
        printf("p=%s\n",p);
        if(*p == '1'){
            stopflag = 1;
        }
    }
    return 0;
}
static int getpid_callback(char*line,void*data){
    char *tmp=(char*)data;
    printf("line=%s\n",line);
    printf("tmp =%s\n",tmp);
    if((strncmp(tmp,line,strlen(tmp)) == 0)&&(line[strlen(tmp)] == '*')&&(pid == 0)){
    //if((strncmp(tmp,line,strlen(tmp)) == 0)&&(line[strlen(tmp)] == '*')&&(pid == 0)){
        char*p;
        char*pend;
        p = strrchr(line,'*');
        if(p == NULL){
            return 0;
        }
        p++;
        printf("%s\n",p);
        pend=strchr(p,'\n');
        if(pend == NULL){
            return 0;
        }
        *pend = '\0';
        printf("%s\n",p);
        pid=atoi(p);
        return 0;
    }
    return 0;
}
int loop_file(char*file,file_loop_callback *callback,void*data){
	FILE *fp;
    struct stat st;
    if(stat(file,&st)== -1){
        return 0;
    }
	fp = fopen(file,"r");
	if(fp == NULL){
		return -1;
	}
	char line[2048];
    memset(line,0,2048);
	while (fgets(line, 2048, fp) != NULL) {
       if(callback(line,data) == -1){
           fclose(fp);
           return -1;
       }
        memset(line,0,2048);
	}
	fclose(fp);
	return 0;	
}
int getpid_stopcp(char*name){
    int fd;
    file_lock(CP_LOCK,&fd);
    loop_file(CP_FILE,getpid_callback,name);
    printf("pid=%d\n",pid);
    if(pid > 0){
        kill(pid,SIGUSR1);
    }
    file_unlock(fd);
    return pid;
}
int getstopflag_stopcp(char*name){
    int fd;
    int i;
    for(i=0;i<3;i++){
        file_lock(CP_LOCK,&fd);
        loop_file(CP_FILE,getstopflag_callback,name);
        file_unlock(fd);
        if(stopflag == 1){
            printf("it have been stop\n");
            return 1;
        }
        sleep(1);
    }
    printf("it can't be stop\n");
    return 0;
}
static int file_lock(char *file,int *fd)
{
	printf("file lock, open,%s\n",file);
	*fd = open(file,O_CREAT|O_TRUNC|O_RDWR,0777);
	if(*fd == -1)
	{
		printf("error1\n");
		return 0;
	}
	printf("file lock,%d\n",*fd);
	if(flock(*fd,LOCK_EX) == -1)
	{
		printf("error2\n");
		return 0;
	}
	printf("file lock,end\n");
	return 1;
}

static void file_unlock(int fd)
{
	printf("file unlock,%d\n",(int)fd);
    if(fd < 0){
        return;
    }
	flock(fd,LOCK_UN);
	close(fd);
	fd = -1;
	return;
}
void check_uri(char*dest,char*src){
    int flag = 0;
    while(*src != '\0'){
        if(*src == '/'){
            if(flag == 0){
                flag = 1;
            }else{
                src++;
                continue;
            }
        }else{
            flag = 0;
        }
        *dest++=*src++;
    }
    *dest++ = '\0';
}
int main(int argc,char**argv)
{
    char tmp[1024];
    check_uri(tmp,argv[1]);
    int len;
    len = strlen(tmp);
    if(tmp[len-1] == '/'){
        tmp[len - 1 ] = '\0';
    }
    printf("tmp=%s\n",tmp);
    getpid_stopcp(tmp);
    getstopflag_stopcp(tmp);
	return 0;
}
