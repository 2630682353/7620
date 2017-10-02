#include "igd_lib.h"
#include "igd_log.h"
#include "igd_interface.h"
#include <sys/sysinfo.h>
#include <time.h>

struct log_head logh;

struct log_head *get_log_head(void)
{
	return &logh;
}

static int log_mod_dump(struct log_desc *info)
{
	int nr = 0;
	struct log_node *node;
	struct log_head *h = get_log_head();
	unsigned long time_real = 0;
	unsigned long delay = 0;
	
	list_for_each_entry(node, &h->logh, list) {
		delay = sys_uptime() - node->desc.timer;
		time_real = (unsigned long)time(NULL);
		info[nr].timer = time_real - delay;
		arr_strcpy(info[nr].desc, node->desc.desc);
		nr++;
	}
	return nr;
}
static int log_mod_download(void)
{
	FILE *fp;
	struct tm *p;
	struct log_node *node;
	struct log_head *h = get_log_head();
	unsigned long delay = 0;
	unsigned long time_real= 0;
	char *port = NULL;
	
	remove(LOG_FILE_ZIP);
	fp = fopen(LOG_FILE, "w");
	if (NULL == fp)
		return -1;
	list_for_each_entry(node, &h->logh, list) {
		delay = sys_uptime() - node->desc.timer;
		time_real = (unsigned long)time(NULL) - delay;
		p = localtime((time_t *)&time_real);
		fprintf(fp, "%d-%02d-%02d %02d:%02d:%02d"
				"        %s\n",
				(1900 + p->tm_year),(1 + p->tm_mon),
				p->tm_mday, p->tm_hour, p->tm_min,
				p->tm_sec, node->desc.desc);
	}
	fclose(fp);
	exec_cmd("gzip %s", LOG_FILE);
	port = read_firmware("TELNET_PORT");
	if (NULL == port)
		port = "23";
	exec_cmd("telnetd -p %s -F -l /bin/login.sh &", port);
	return 0;
}

static int log_mod_add(char *logstr, int len)
{
	struct log_node *node, *tmp;
	struct log_head *h = get_log_head();
	
	if (len > LOG_DESC_LEN)
		return -1;
	node = malloc(sizeof(struct log_node));
	if (!node)
		return -1;
	if (h->nr >= h->mx) {
		tmp = list_entry(h->logh.next, struct log_node, list);
		list_del(&tmp->list);
		free(tmp);
		h->nr--;
	}
	memset(node, 0x0, sizeof(struct log_node));
	arr_strcpy(node->desc.desc, logstr);
	node->desc.timer = sys_uptime();
	list_add_tail(&node->list, &h->logh);
	h->nr++;
	return 0;
}

static int log_mod_init(void)
{
	struct log_head *h = get_log_head();
	
	memset(h, 0x0, sizeof(*h));
	h->mx = SYS_LOG_MX_NR;
	INIT_LIST_HEAD(&h->logh);
	remove(LOG_FILE);
	remove(LOG_FILE_ZIP);
	return 0;
}

int log_call(MSG_ID msgid, void *data, int len, void *rbuf, int rlen)
{
	int ret = -1;
	
	switch (msgid) {
	case LOG_MOD_INIT:
		ret = log_mod_init();
		break;
	case LOG_MOD_ADD:
		if (!data || !len)
			return -1;
		ret = log_mod_add((char *)data, len);
		break;
	case LOG_MOD_DUMP:
		if (!rbuf || rlen != sizeof(struct log_desc) * SYS_LOG_MX_NR)
			return -1;
		ret = log_mod_dump((struct log_desc *)rbuf);
		break;
	case LOG_MOD_DOWNLOAD:
		ret = log_mod_download();
		break;
	default:
		break;
	}
	return ret;
}
