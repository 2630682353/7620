#include "igd_lib.h"

struct list_head event_head;
module_t MODULE[MODULE_MX];

void init_scheduler (void)
{
    INIT_LIST_HEAD(&event_head);
}

static struct list_head *get_event_head(void)
{
	return &event_head;
}

int dschedule(struct schedule_entry *event)
{
	list_del(&event->list);
	free(event);
	return 0;
}

struct schedule_entry *schedule(struct timeval tv, void (*cb)(void *data), void *data)
{
	struct timeval now;
	struct list_head *h = get_event_head();
	struct schedule_entry *p = NULL, *q = NULL;
	struct timeval tv_bak;

	if (tv.tv_usec > 0)
		return NULL;
	tv_bak = tv;
	memset(&now, 0x0, sizeof(now));
	now.tv_sec = sys_uptime();
	//gettimeofday (&now, NULL);
	tv.tv_sec += now.tv_sec;
	tv.tv_usec += now.tv_usec;

	#if 0
	if (tv.tv_usec >= 1000000) {
		tv.tv_sec++;
		tv.tv_usec -= 1000000;
	}
	#endif

	q = malloc(sizeof(struct schedule_entry));
	if (NULL == q)
		return NULL;
	memset(q, 0x0, sizeof(*q));
	q->tv = tv;
	q->tv_bak = tv_bak;
	q->cb = cb;
	q->data = data;
	
	list_for_each_entry(p, h, list) {
		if (!TVLESS(tv, p->tv)) 
			continue;
		list_add(&q->list, p->list.prev);
		return q;
	}
	list_add_tail(&q->list, h);
	return q;
}

struct timeval *process_schedule (struct timeval *ptv)
{
	struct timeval now;
	struct timeval then;
	struct schedule_entry *p = NULL;
	struct schedule_entry *q = NULL;
	struct list_head *h = get_event_head();

#ifdef MU_DBG_TT
	long pre, post;
#endif

	if (list_empty(h))
		return NULL;
	list_for_each_entry_safe(p, q, h, list) {
		memset(&now, 0x0, sizeof(now));
		now.tv_sec = sys_uptime();
		//gettimeofday (&now, NULL);
		if (!TVLESSEQ(p->tv, now))
			continue;

#ifdef MU_DBG_TT
		pre = sys_uptime();
		MU_DEBUG("[%ld]:I,[%ld,%d]\n",
			pre, p->tv_bak.tv_sec, p->tv_bak.tv_usec);
#endif

		p->cb(p->data);

#ifdef MU_DBG_TT
		post = sys_uptime();
		MU_DEBUG("[%ld]:O,[%ld,%d]\n",
			post, p->tv_bak.tv_sec, p->tv_bak.tv_usec);
#endif

		list_del(&p->list);
		free(p);
	}

	if (!list_empty(h)) {
		q = list_entry(h->next, struct schedule_entry, list);
		then.tv_sec = q->tv.tv_sec - now.tv_sec;
		then.tv_usec = q->tv.tv_usec - now.tv_usec;
		if (then.tv_usec < 0) {
			then.tv_sec -= 1;
			then.tv_usec += 1000000;
		}
		
		if ((then.tv_sec <= 0) && (then.tv_usec <= 0)) {
			then.tv_sec = 1;
			then.tv_usec = 0;
		}
		*ptv = then;
		return ptv;
	}
	return NULL;
}

int mu_call(MSG_ID mgsid, void *data, int len, void *rbuf, int rlen)
{
	int ret;
	MOD_ID mid;
#ifdef MU_DBG_TT
	long pre, post;
#endif

	mid = MODUEL_GET(mgsid);
	if (!MODULE[mid].module_hander)
		return -1;

#ifdef MU_DBG_TT
	pre = sys_uptime();
	MU_DEBUG("[%ld]:I,0x%X\n", pre, mgsid);
#endif

	ret = MODULE[mid].module_hander(mgsid, data, len, rbuf, rlen);

#ifdef MU_DBG_TT
	post = sys_uptime();
	MU_DEBUG("[%ld]:O,0x%X,%d\n", post, mgsid, ret);
#endif
	return ret;
}

int exe_default(void *cmdline)
{
	int i = 0 ;
	char *tmp = NULL;
	char *argv[1024] = {0};
	char buff[1024] = {0};
	char *param = NULL;

	strncpy(buff, cmdline, 1024);
	param = &buff[0];
	while ((tmp = strchr(param, ' ')) != NULL) {
		*tmp='\0';
		if (*(tmp-1) != '\0')
			argv[i++] = param;
		param=tmp;
		param++;
	}
	argv[i++] = param;
	argv[i] = NULL;

	if (execvp(argv[0], argv) == -1)
		exit(-1);
	return 0;
}

/*
	for example:
	1.exec_cmd("pppd"); //will wait progress exit.
	2.exec_cmd("pppd &");//do not wait progress exit.
*/
int exec_cmd(const char * fmt,...)
{
	int pid;
	int flag = 0;
	int wpid = 0;
	int waittime = 0;
	int timeout = 600;
	int statbuf;
	char tmp[1024] = {0};
	char cmdbuf[1024] = {0,};
	
	va_list ap;
	va_start(ap, fmt);
	vsprintf(cmdbuf, fmt, ap);
	va_end(ap);

	if (cmdbuf[strlen(cmdbuf) - 1] == '&') {
		strncpy(tmp, cmdbuf, strlen(cmdbuf) - 2);
		flag = 1;
	} else
		arr_strcpy(tmp, cmdbuf);
	
	pid = fork();
	if (pid < 0)
		return 0;
	if (pid == 0) {
		int i;
		for (i = 3; i < 256; i++)
			close(i);
		exe_default(tmp);
	}
	
	if (flag)
		return 0; 
	while(1) {
		wpid = waitpid(pid, &statbuf, WNOHANG);
		if (wpid != 0)
			break;
		if (waittime < timeout) {
			usleep(100000);
			waittime ++;
		} else {
			kill(pid, SIGKILL);
			usleep(100000);
			wpid = waitpid(pid, &statbuf, WNOHANG);
			return -1;
		}
	}
	return 0;
}
