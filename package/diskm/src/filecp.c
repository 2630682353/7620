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
#include <utime.h>
#define CP 0
#define MV 1
#define ERROR -1
#include <sys/wait.h>
#define MAXSEND (2147483647)
#define MAXFILE 128
//#define CP_FILE "fileserv_cp"
#define CP_FILE "/tmp/fileserv_cp"
#define CP_LOCK "/tmp/fileserv_cp.lock"
#define W_LOG_SIZE 1048576
char *socketsenderror423="HTTP/1.1 423 Locked\r\nContent-Length: 0\r\nDate: Sat, 31 Dec 2011 16:24:49 GMT\r\nServer: lighttpd/1.4.28-devel-78M\r\n\r\n";
char *socketsend="HTTP/1.1 201 Created\r\nContent-Length: 0\r\nDate: Sat, 31 Dec 2011 16:24:49 GMT\r\nServer: lighttpd/1.4.28-devel-78M\r\n\r\n";
char *socketsenderror="HTTP/1.1 501 Not Implemented\r\nContent-Length: 0\r\nDate: Sat, 31 Dec 2011 16:24:49 GMT\r\nServer: lighttpd/1.4.28-devel-78M\r\n\r\n";
char *socketsend_nospace="HTTP/1.1 403 Forbidden\r\nContent-Length: 0\r\nDate: Sat, 31 Dec 2011 16:24:49 GMT\r\nServer: lighttpd/1.4.28-devel-78M\r\n\r\n";
char *socketsend_kill="HTTP/1.1 502 Bad Gateway\r\nContent-Length: 0\r\nDate: Sat, 31 Dec 2011 16:24:49 GMT\r\nServer: lighttpd/1.4.28-devel-78M\r\n\r\n";
typedef struct cpfile{
    char src[1024];
    char dest[1024];
    char logfile[1024];
    char logfile_tmp[1024];
    char tmp[2048];
    int src_len;
    int dest_len;
    time_t last_time;
    int speed;
    off_t delay;
    off_t total_size;
    off_t cur_size;
    off_t cur_size_tmp;
    FILE *wfp;
    int setflag;
    pid_t pid;
    int tmpcount;
}cpfile_t;
int srv_shutdown;
static void signal_handler(int sig) {
	switch (sig) {
	case SIGUSR1: srv_shutdown = 1; break;
    }
}
static int file_lock(char *file,int *fd);
static void file_unlock(int fd);
static int dir_delete(char *name);
typedef int (file_loop_callback)(char*line,void*data);
#if 0
static int checkfile_callback(char*line,void*data){
    cpfile_t *finfo = (cpfile_t *)data;
    if((strncmp(finfo->dest,line,finfo->dest_len) == 0)&&(line[finfo->dest_len] == '*')&&(line[finfo->dest_len+1] == '0')){
        printf("there is a line,dest=%s\n",finfo->dest);
        return -1;
    }
    return 0;
}
#endif
static int checkfile_callback(char*line,void*data){
    cpfile_t *finfo = (cpfile_t *)data;
    if((strncmp(finfo->dest,line,finfo->dest_len) == 0)&&(line[finfo->dest_len] == '*')&&(line[finfo->dest_len+1] == '0')){
        printf("there is a line,dest=%s\n",finfo->dest);
        return -1;
    }
    if((strncmp(finfo->dest,line,finfo->dest_len) == 0)&&(line[finfo->dest_len] == '*')){
        if(finfo->setflag == 0){
            fprintf(finfo->wfp,"%s*%d*%d*%d*%d*%d\n",finfo->dest,0,0,1,0,finfo->pid);
            finfo->setflag = 1;
        }
        return 0;
    }else{
        fwrite(line, strlen(line), 1, finfo->wfp);
    }
    return 0;
}
static int synclog_callback(char*line,void*data){
    cpfile_t *finfo = (cpfile_t *)data;
    if((strncmp(finfo->dest,line,finfo->dest_len) == 0)&&(line[finfo->dest_len] == '*')){
        time_t timenow;
        timenow=time(NULL);
        time_t timediff;
        if( (timediff =timenow - finfo->last_time) <= 0){
            finfo->speed = finfo->cur_size_tmp + finfo->delay;
            finfo->cur_size += finfo->cur_size_tmp;
            finfo->cur_size += finfo->delay;
            finfo->cur_size_tmp = 0;
            finfo->delay = 0;
        }else{
            finfo->speed = (finfo->cur_size_tmp+finfo->delay)/timediff;
            finfo->cur_size += finfo->cur_size_tmp;
            finfo->cur_size += finfo->delay;
            finfo->cur_size_tmp = 0;
            finfo->delay = 0;
        }
        finfo->last_time = timenow;
        //printf("%s*%d*%d*%lld*%lld,%u\n",finfo->dest,0,finfo->speed,finfo->total_size,finfo->cur_size,timenow);
        fprintf(finfo->wfp,"%s*%d*%d*%lld*%lld*%d\n",finfo->dest,0,finfo->speed,finfo->total_size,finfo->cur_size,finfo->pid);
        return 0;
    }else{
        fwrite(line, strlen(line), 1, finfo->wfp);
    }
    return 0;
}
static int endlog_callback(char*line,void*data){
    cpfile_t *finfo = (cpfile_t *)data;
    if((strncmp(finfo->dest,line,finfo->dest_len) == 0)&&(line[finfo->dest_len] == '*')){
        time_t timenow;
        timenow=time(NULL);
        time_t timediff;
        if( (timediff =timenow - finfo->last_time) <= 0){
            finfo->speed = finfo->cur_size_tmp+finfo->delay;
            finfo->cur_size += finfo->cur_size_tmp;
            finfo->cur_size += finfo->delay;
            finfo->cur_size_tmp = 0;
            finfo->delay = 0;
        }else{
            finfo->speed = (finfo->cur_size_tmp+finfo->delay)/timediff;
            finfo->cur_size += finfo->cur_size_tmp;
            finfo->cur_size += finfo->delay;
            finfo->cur_size_tmp = 0;
            finfo->delay = 0;
        }
        fprintf(finfo->wfp,"%s*%d*%d*%lld*%lld*%d\n",finfo->dest,1,finfo->speed,finfo->total_size,finfo->cur_size,finfo->pid);
        return 0;
    }else{
        fwrite(line, strlen(line), 1, finfo->wfp);
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
int cpfile_log_init(cpfile_t *finfo){
    int fd;
    file_lock(CP_LOCK,&fd);
	finfo->wfp = fopen(finfo->logfile_tmp,"w");
    if(loop_file(finfo->logfile,checkfile_callback,finfo) == -1){
        printf("loop file error\n");
        fclose(finfo->wfp);
        finfo->wfp = NULL;
        file_unlock(fd);
        return -1;
    }
    if(finfo->setflag == 0){
        fprintf(finfo->wfp,"%s*%d*%d*%d*%d*%d\n",finfo->dest,0,0,1,0,finfo->pid);
        finfo->setflag = 1;
    }
    fclose(finfo->wfp);
    finfo->wfp = NULL;
    rename(finfo->logfile_tmp,finfo->logfile);
    file_unlock(fd);
    return 0;
}
int cpfile_log_sync(cpfile_t *finfo){
    if (srv_shutdown){
        return -1;
    }
    int fd;
    file_lock(CP_LOCK,&fd);
	finfo->wfp = fopen(finfo->logfile_tmp,"w");
    if(finfo->wfp == NULL){
        file_unlock(fd);
        return -1;
    }
    if(loop_file(finfo->logfile,synclog_callback,finfo) == -1){
        fclose(finfo->wfp);
        finfo->wfp = NULL;
        file_unlock(fd);
        return -1;
    }
    fclose(finfo->wfp);
    finfo->wfp = NULL;
    rename(finfo->logfile_tmp,finfo->logfile);
    file_unlock(fd);
    return 0;

}
int cpfile_log_end(cpfile_t *finfo){
    int fd;
    file_lock(CP_LOCK,&fd);
	finfo->wfp = fopen(finfo->logfile_tmp,"w");
    if(finfo->wfp == NULL){
        file_unlock(fd);
        return -1;
    }
    if(loop_file(finfo->logfile,endlog_callback,finfo) == -1){
        fclose(finfo->wfp);
        finfo->wfp = NULL;
        file_unlock(fd);
        return -1;
    }
    fclose(finfo->wfp);
    finfo->wfp = NULL;
    rename(finfo->logfile_tmp,finfo->logfile);
    file_unlock(fd);
    return 0;

}
int cpfile_log_sync_check(cpfile_t *finfo){
    time_t now;
    if (srv_shutdown){
        return -1;
    }
    if(finfo->cur_size_tmp < W_LOG_SIZE){
        return 0;
    }
    //printf("go to sync\n");
    now = time(NULL);
    if(now == finfo->last_time){
        finfo->delay +=finfo->cur_size_tmp;
        finfo->cur_size_tmp = 0;
        return 0;
    }else{
        if (cpfile_log_sync(finfo)< 0){
            return -1;
        }

    }
    return 0;
}
int do_cmd(char *cmd)
{
	FILE *fp;
	int ret;

	if((fp = popen(cmd, "w")) == NULL){
		return 1;
	}
	ret = pclose(fp);
	if(ret){
		//err
		return 1;
	}else{
		//ok
		return 0;
	}
}
static int u_cpfile(char*srcname,char*destname,off_t s_size,cpfile_t* finfo){
    int fd_in;
    int fd_out;
    char tmpbuf[4096];
    int n;
    off_t d_size = 0;
	fd_in = open(srcname,O_RDONLY,0777);
    if(fd_in == -1){
        return -1;
    }
	fd_out = open(destname,O_CREAT|O_TRUNC|O_RDWR,0777);
    if(fd_out == -1){
        return -1;
    }
    while((n = read(fd_in,tmpbuf,4096)) > 0){
        if(write(fd_out,tmpbuf,n) != n){
                close(fd_in);
                close(fd_out);
	    	remove(destname);
                return -1;
        }
        finfo->cur_size_tmp += n;
        if (cpfile_log_sync_check(finfo) < 0){
            close(fd_in);
            close(fd_out);
	    remove(destname);
            return -1;
        }
        d_size = d_size + n;
    }
    close(fd_in);
    close(fd_out);
    if(d_size != s_size){
	remove(destname);
        return -1;
    }
    return 0;
}
//static  int  loop_dir(char *name,char* callback,void*data)
typedef int (loop_callback)(char*name,struct stat * st,void*data);
static int printname(char*name,struct stat * st,void*data){
    printf("%s\n",name);
    return 0;
}
static int getsize(char*name,struct stat * st,void*data){
    off_t* total = (off_t*)(data);
	if(!S_ISDIR(st->st_mode)){
        //printf("%lld,%s\n",st->st_size,name);
        *total = *total + st->st_size;
		return 0;
	}
    return 0;
}
#define WEBDAV_DIR_MODE  S_IRWXU | S_IRWXG | S_IRWXO
static int filecp(char*name,struct stat * st,void*data){
    cpfile_t* finfo = (cpfile_t*)(data);
    char tmpfile[2048];
    struct utimbuf ubuf;
    printf("set name %s\n",name);
    sprintf(tmpfile,"%s%s",finfo->dest,&name[finfo->src_len]);
    
	if(!S_ISDIR(st->st_mode)){
        printf("%lld,%s\n",st->st_size,name);
        if(u_cpfile(name,tmpfile,st->st_size,finfo) == -1){
            return -1;
        }
        ubuf.modtime=st->st_mtime;
        ubuf.actime=st->st_atime;
        //ubuf.actime=st->st_ctime;
        //ubuf.actime=st->st_mtime;
        utime(tmpfile,&ubuf);
        finfo->tmpcount = 0;
		return 0;
	}else{
        printf("tmpfile=%s\n",tmpfile);
        finfo->tmpcount++;
		if (-1 == mkdir(tmpfile, WEBDAV_DIR_MODE)) {
        printf("tmpfile=%s\n",tmpfile);
            return -1;
        }
        if(finfo->tmpcount > 50){
            finfo->tmpcount = 0;
            cpfile_log_sync(finfo);
        }
    }
    return 0;
}
static  int  loop_dir(char *name,loop_callback *fuc,void*data)
{
	struct dirent *dirp = NULL;
	struct stat st;
	DIR *dp = NULL;
    int namelen;
    int ret;
    if(name == NULL || fuc == NULL){
        printf("name||fuc error\n");
        return -1;
    }
    namelen = strlen(name);
	if(stat(name,&st) == -1)
	{
        printf("stat error file: %s\n",name);
		return -2;
	}
    if((ret = fuc(name,&st,data)) == -1){
        return -1;
    }
	if(!S_ISDIR(st.st_mode)){
		return 0;
	}
	dp = opendir(name);
	if( dp == NULL ){
        printf("opendir error file: %s\n",name);
		return -2;
	}
    name[namelen] = '/';
	while((dirp = readdir(dp)) != NULL)
	{
        if(dirp->d_name[0] == '.'){
            continue;
        }
        strcpy(&name[namelen+1],dirp->d_name);
        ret =loop_dir(name,fuc,data);
        if(ret < 0){
            closedir(dp);
            name[namelen] = '\0';
            printf("loopfile error: %s\n",name);
            return ret;
        }
	}
	closedir(dp);
    name[namelen] = '\0';
	return 0;
}

static  int  loop_dir2(char *name,loop_callback *fuc,loop_callback *fuc2,void*data)
{
	struct dirent *dirp = NULL;
	struct stat st;
	DIR *dp = NULL;
    int namelen;
    int ret;
    if(name == NULL || fuc == NULL){
        printf("name||fuc error\n");
        return -1;
    }
    namelen = strlen(name);
	if(stat(name,&st) == -1)
	{
        //printf("stat error file: %s\n",name);
		return -2;
	}
    if((ret = fuc(name,&st,data)) == -1){
        return -1;
    }
	if(!S_ISDIR(st.st_mode)){
		return 0;
	}
	dp = opendir(name);
    printf("name=%s\n",name);
	if( dp == NULL ){
        printf("opendir error file: %s\n",name);
		return -2;
	}
    name[namelen] = '/';
	while((dirp = readdir(dp)) != NULL)
	{
        if(dirp->d_name[0] == '.'){
            continue;
        }
        strcpy(&name[namelen+1],dirp->d_name);
        ret =loop_dir2(name,fuc,fuc2,data);
        if(ret < 0){
            closedir(dp);
            name[namelen] = '\0';
            printf("loopfile error: %s\n",name);
            return ret;
        }
	}
	closedir(dp);
    name[namelen] = '\0';
    if((ret = fuc2(name,&st,data)) == -1){
        return -1;
    }
	return 0;
}
static int file_delete_callback(char*name,struct stat * st,void*data){
    printf("delete file %s\n",name);
	if(!S_ISDIR(st->st_mode)){
        remove(name);
    }
    return 0;
}
static int dir_delete_callback(char*name,struct stat * st,void*data){
    printf("delete dir %s\n",name);
	if(S_ISDIR(st->st_mode)){
        remove(name);
    }
    return 0;
}
static int dir_delete(char *name){
    int ret;
    ret = loop_dir2(name,file_delete_callback,dir_delete_callback,NULL);
    if(ret < 0){
        return -1;
    }
    return ret;
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
void get_uri(char*dest,char*src){
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
int cpfile_init(cpfile_t *finfo,char*src,char*dest){
    memset(finfo,0,sizeof(cpfile_t));
    get_uri(finfo->src,src);
    get_uri(finfo->dest,dest);
    finfo->src_len = strlen(finfo->src);
    if(finfo->src[finfo->src_len -1] == '/'){
        finfo->src[finfo->src_len -1] = '\0';
        finfo->src_len--;
    }
    finfo->dest_len = strlen(finfo->dest);
    if(finfo->dest[finfo->dest_len -1] == '/'){
        finfo->dest[finfo->dest_len -1] = '\0';
        finfo->dest_len--;
    }
    strcpy(finfo->logfile,CP_FILE);
    sprintf(finfo->logfile_tmp,"%s.tmp",finfo->logfile);
    //setcpinfofile(finfo->logfile,finfo->dest);
    strcpy(finfo->tmp,finfo->src);
    finfo->pid = getpid();
    if (cpfile_log_init(finfo) < 0){
        return -1;
    }
    return 0;
    
}
int cpfile_settime_size(cpfile_t *finfo){
    if (loop_dir(finfo->tmp,getsize,&finfo->total_size) < 0){
        return -1;
    }
    finfo->last_time = time(NULL);
    if(cpfile_log_sync(finfo) == -1){
        return -1;
    }
    return 0;
}
int cpfile_cp(cpfile_t *finfo){
    int ret;
    ret  = loop_dir(finfo->tmp,filecp,finfo);
    if(ret < 0){
        printf("hehe1 error\n");
        return ret;
    }
    if ( (ret =cpfile_log_end(finfo)) < 0){
        printf("hehe2 error\n");
        return ret;
    }
    return 0;
}
int getdisklen(char*src){
    char*p;
    char*pend;
    p= src;
    int i = 0;
    for(i=0;i<4;i++){
        pend = strchr(p,'/');
        if(pend != NULL){
            p = pend +1;
        }else{
            p = NULL;
            break;
        }
    }
    if(p!= NULL){
        return p-src-1;
    }
    return -1;
}
int checkspace(cpfile_t *finfo){
    struct statfs s;
    off_t freesize = 0;
    int len;
    len=getdisklen(finfo->dest);
    if(len <= 0){
        return -1;
    }
    char tmp[1024];
    sprintf(tmp,"%.*s",len,finfo->dest);
    printf("tmp=%s\n",tmp);
    if(!statfs(tmp, &s) && (s.f_blocks > 0)){
        freesize = (unsigned long long)s.f_bsize * (unsigned long long)s.f_bavail;
    }
    printf("freesize = %lld\n",freesize);
    if(freesize < finfo->total_size){
        printf("error freesize = %lld\n",freesize);
        return -1;
    }
    printf("ok freesize = %lld\n",freesize);
    return 0;
}
int main(int argc,char**argv)
{
    cpfile_t finfo;
    int ret;
    if(argc < 3){
        printf("useage:srcname destname\n");
        return -1;
    }
	signal(SIGPIPE, SIG_IGN);
	signal(SIGCHLD, SIG_IGN);
	signal(SIGUSR1, signal_handler);
	int i;
	for(i=4;i<256;i++){
		close(i);
	}
    if( (ret = cpfile_init(&finfo,argv[1],argv[2])) == -1){
        printf("there is a cp now\n");
	    write(3,socketsenderror423,strlen(socketsenderror423));
        printf("cp error end 1\n");
	sync();
        return -1;
    }
    if ( (ret = cpfile_settime_size(&finfo))< 0){
        cpfile_log_end(&finfo);
	    write(3,socketsenderror,strlen(socketsenderror));
        printf("cp error end 2\n");
	sync();
        return -1;
    }
    if((ret = checkspace(&finfo)) < 0){
        cpfile_log_end(&finfo);
	    write(3,socketsend_nospace,strlen(socketsend_nospace));
        printf("cp error end 3\n");
	sync();
        return -1;
    }
    dir_delete(finfo.dest);
    printf("go to cp\n");
    if ((ret = cpfile_cp(&finfo)) < 0){
        cpfile_log_end(&finfo);
	    write(3,socketsenderror,strlen(socketsenderror));
        printf("cp error end 4\n");
	sync();
        return -1;
    }
    printf("cp end\n");
    if(argv[0][0] == 'm'){
        dir_delete(finfo.src);
    }
	write(3,socketsend,strlen(socketsend));
	sync();
	return 0;
}
