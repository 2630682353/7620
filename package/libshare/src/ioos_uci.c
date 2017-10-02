#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <uci.h>
#include "ioos_uci.h"
int do_cmd(char *cmd)
{
    int pid;
    int statbuf;
#if 1
    char cmdbuf[1024];
    int flag=0;
    if(cmd[strlen(cmd) - 1] == '&'){
	flag = 1;
	strncpy(cmdbuf,cmd,1024);
	cmdbuf[ strlen(cmdbuf)-1 ] = '\0';
    }
#endif
    int wpid = 0,  waittime = 0, timeout =600;
    pid = vfork();
    if(pid < 0)
    {
        return 0;
    }
    if(pid == 0)
    {
    	int i;
	for(i=3;i<256;i++){
		close(i);
	}
#if 1
	if(flag){
        	execl("/bin/sh", "sh", "-c", cmdbuf, (char *) 0);
	}else{
#endif
        	execl("/bin/sh", "sh", "-c", cmd, (char *) 0);
#if 1
	}
#endif
    }
#if 1
	if(flag){
		return 0;
	}
#endif
        while(1){
                wpid = waitpid(pid, &statbuf, WNOHANG);
                if (wpid == 0) {
                        if (waittime < timeout) {
                                usleep(100000);
                                waittime ++;
                        }else {
                                kill(pid, SIGKILL);
                                usleep(100000);
                                wpid = waitpid(pid, &statbuf, WNOHANG);
                                printf("Warning: do cmd is too long and kill it \n");
                                return -1;
                                                                //break;
                        }
                }else{
                        break;
                }
        }
    return 0;
}
//typedef void (uci_getlist_callback)(char*name,void*data);
int uci_list_check_value(char*file,char*section,char*list,char*value){
	struct uci_package *pkg = NULL;
	struct uci_context * ctx = NULL;
	struct uci_element *e;
	ctx = uci_alloc_context();
	if (UCI_OK != uci_load(ctx, file, &pkg)){
		uci_free_context(ctx); 
		return 0;
	}
	uci_foreach_element(&pkg->sections, e){
		struct uci_section *s = uci_to_section(e);
		if(section != NULL && (s->anonymous != 0 || strcmp(e->name,section) != 0)){
			continue;
		}
		struct uci_option *o = uci_lookup_option(ctx, s, list);
		if ( ( NULL != o) &&(UCI_TYPE_LIST == o->type)){
			
			uci_foreach_element(&o->v.list, e){
				if(strcmp(e->name,value) == 0){
					uci_unload(ctx,pkg);
					uci_free_context(ctx);
					return 1; 
				}
			}
			break;
		} 
	}
	uci_unload(ctx,pkg);
	uci_free_context(ctx); 
	return 0;
}
int uci_list_check_value2(char*file,char*section,char*list,char*value){
	struct uci_package *pkg = NULL;
	struct uci_context * ctx = NULL;
	struct uci_element *e;
	ctx = uci_alloc_context();
	if (UCI_OK != uci_load(ctx, file, &pkg)){
		uci_free_context(ctx); 
		return 0;
	}
	uci_foreach_element(&pkg->sections, e){
		struct uci_section *s = uci_to_section(e);
		if(section != NULL && (s->anonymous != 0 || strcmp(e->name,section) != 0)){
			continue;
		}
		struct uci_option *o = uci_lookup_option(ctx, s, list);
		if ( ( NULL != o) &&(UCI_TYPE_LIST == o->type)){
			
			uci_foreach_element(&o->v.list, e){
				if(strncmp(e->name,value,strlen(value)) == 0){
				//if(strcmp(e->name,value) == 0){
					uci_unload(ctx,pkg);
					uci_free_context(ctx);
					return 1; 
				}
			}
			break;
		} 
	}
	uci_unload(ctx,pkg);
	uci_free_context(ctx); 
	return 0;
}
char *uci_list_get_value(char*file,char*section,char*list,char*value){
	struct uci_package *pkg = NULL;
	struct uci_context * ctx = NULL;
	struct uci_element *e;
	char *revalue = NULL;
	ctx = uci_alloc_context();
	if (UCI_OK != uci_load(ctx, file, &pkg)){
		uci_free_context(ctx); 
		return NULL;
	}
	uci_foreach_element(&pkg->sections, e){
		struct uci_section *s = uci_to_section(e);
		if(section != NULL && (s->anonymous != 0 || strcmp(e->name,section) != 0)){
			continue;
		}
		struct uci_option *o = uci_lookup_option(ctx, s, list);
		if ( ( NULL != o) &&(UCI_TYPE_LIST == o->type)){
			
			uci_foreach_element(&o->v.list, e){
				if(strncmp(e->name,value,strlen(value)) == 0){
					printf("e->name = %s\n",e->name);
					revalue = strdup(e->name);
					printf("revalue=%s\n",revalue);
					uci_unload(ctx,pkg);
					uci_free_context(ctx);
					return revalue; 
				}
			}
			break;
		} 
	}
	uci_unload(ctx,pkg);
	uci_free_context(ctx); 
	return NULL;
}
char* uci_getval(char*file,char*section,char*option){
	struct uci_package *pkg = NULL;
	struct uci_context * ctx = NULL;
	struct uci_element *e;
	const char *value;
	char *revalue = NULL;
	ctx = uci_alloc_context();
	if (UCI_OK != uci_load(ctx, file, &pkg)){
		uci_free_context(ctx); 
		return NULL;
	}
	uci_foreach_element(&pkg->sections, e){
		struct uci_section *s = uci_to_section(e);
		if(section != NULL && (s->anonymous != 0 || strcmp(e->name,section) != 0)){
			continue;
		}
		if (NULL != (value = uci_lookup_option_string(ctx, s, option))){
			revalue = strdup(value);
			break;
		} 
	}
	uci_unload(ctx,pkg);
	uci_free_context(ctx); 
	return revalue;
}
static char * uci_show_value(struct uci_option *o)
{
	struct uci_element *e;
	char *revalue = NULL; 

	switch(o->type) {
	case UCI_TYPE_STRING:
	printf("test%d\n",__LINE__);
		revalue = strdup(o->v.string);
		printf("%s\n", o->v.string);
		break;
	case UCI_TYPE_LIST:
	printf("test%d\n",__LINE__);
		uci_foreach_element(&o->v.list, e) {
	printf("test%d\n",__LINE__);
			revalue = strdup(e->name);
		//	printf("%s", e->name);
		}
		printf("\n");
		break;
	default:
		revalue = NULL;
		printf("<unknown>\n");
		break;
	}
	printf("%s\n",revalue);
	return revalue;
}
char* uci_getval_cmd(char*cmdsrc){
	struct uci_context * ctx = NULL;
	struct uci_element *e;
	struct uci_ptr ptr;
	char *revalue = NULL;
	char cmd[1024];
	strncpy(cmd,cmdsrc,1023);
			printf("test%d\n",__LINE__);
	ctx = uci_alloc_context();
			printf("test%d\n",__LINE__);
	if (uci_lookup_ptr(ctx, &ptr, cmd, true) != UCI_OK) {
			printf("test%d\n",__LINE__);
		//cli_perror();
		return NULL;
	}
	if (!(ptr.flags & UCI_LOOKUP_COMPLETE)){
		return NULL;
	}
			printf("test%d\n",__LINE__);
	e = ptr.last;
			printf("test%d\n",__LINE__);
		switch(e->type) {
		case UCI_TYPE_SECTION:
			printf("test%d\n",__LINE__);
			revalue = strdup(ptr.s->type);
			//printf("%s\n", ptr.s->type);
			break;
		case UCI_TYPE_OPTION:
			printf("test%d\n",__LINE__);
			revalue = uci_show_value(ptr.o);
			break;
		default:
			break;
		}
	uci_free_context(ctx); 
	return revalue;
}
void uci_setval(char*file,char*section,char*option,char*value){
	char cmdbuf[2048];
	sprintf(cmdbuf,"uci set %s.%s.%s='%s'",file,section,option,value);
	do_cmd(cmdbuf);
	do_cmd("uci commit");
	return;
}
void uci_delval(char*file,char*section,char*option){
	char cmdbuf[2048];
	sprintf(cmdbuf,"uci del %s.@%s[0].%s",file,section,option);
	do_cmd(cmdbuf);
	do_cmd("uci commit");
	return;
}
void ioos_uci_add_list(char*file,char*section,char*option,char*value){
	char cmdbuf[2048];
	sprintf(cmdbuf,"uci add_list %s.%s.%s='%s'",file,section,option,value);
	do_cmd(cmdbuf);
	do_cmd("uci commit");
	return;
}
void ioos_uci_del_list(char*file,char*section,char*option,char*value){
	char cmdbuf[2048];
	sprintf(cmdbuf,"uci del_list %s.%s.%s='%s'",file,section,option,value);
	do_cmd(cmdbuf);
	do_cmd("uci commit");
	return;
}
