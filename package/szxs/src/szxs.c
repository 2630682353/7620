#include "szxs.h"
#include <sys/sysinfo.h>

#if 1
#define SZXS_DEBUG(fmt,arg...) do{}while(0)
#else
#define SZXS_DEBUG(fmt,args...) do{printf("%s[%d]"fmt, __FUNCTION__, __LINE__,##args);}while(0)
#endif

#define FILE_DIR "/tmp/szxs"
/*
#ifdef FLASH_4_RAM_32
#define MAX_GZ_FILE_TOT_SIZE 500*1024
#define MAX_AR_FILE_TOT_SIZE 500*1024
#define MAX_FILE_SIZE	300*1024
#else
#define MAX_GZ_FILE_TOT_SIZE 1*1024*1024
#define MAX_AR_FILE_TOT_SIZE 1*1024*1024
#define MAX_FILE_SIZE	1024*1024
#endif
*/
#define POST_MAX_LEN 10 * 1024
#define MAX_COMPRESS_FILE_TIME 2 * 3600

/*LIST_HEAD(glb_get_lh);*/
/*LIST_HEAD(glb_post_lh);*/
LIST_HEAD(glb_id_lh);

FILE *fp = NULL;
char glb_fname[128] = {0};
time_t start_time1 = 0;
time_t start_time2 = 0;
long ftime = 0;
struct iface_info info;
extern int nlk_uk_recv(const struct nlk_msg_handler *h, void *buf, int buf_len);
uint8_t mac[6] = {0};
unsigned long max_file_size = 0;
unsigned long max_compress_file_size = 0;

static int xs_lock(void)
{
	int fd = 0;
	char file[128];

	snprintf(file, sizeof(file),
		"/tmp/xs_lock");
	fd = open(file, O_CREAT | O_WRONLY, 0777);
	if (fd < 0)
		return -1;
	if (flock(fd, LOCK_EX | LOCK_NB)) {
		close(fd);
		return -2;
	}
	return fd;
}

int is_first_package(uint32_t syn, __be32 tcp_seq)
{
	if (syn != tcp_seq)
		return 0;
	SZXS_DEBUG("is_first\n");
	return 1;
}

int is_last_package(char *data, uint16_t data_len)
{
	if (memcmp(data + data_len - 4, "\r\n\r\n", 4)) 
		return 0;
	SZXS_DEBUG("is_last\n");
	return 1;
}

int get_file_size(char *filename, unsigned long *filesize)  
{  
	struct stat statbuff;

	SZXS_DEBUG("begin get_file_size\n");
	if (stat(filename, &statbuff))
		return -1;
	*filesize = statbuff.st_size;
	return 0;
}

int gen_new_filename(unsigned char *ip_split, uint8_t *mac)
{
	time_t now = 0;

	memset(glb_fname, 0, sizeof(glb_fname));
	sprintf(glb_fname, "%d-%d-%d-%d_%02X-%02X-%02X-%02X-%02X-%02X_%lu", 
			ip_split[0], ip_split[1],
			ip_split[2], ip_split[3],
			mac[0], mac[1], mac[2],
			mac[3],	mac[4], mac[5],
			time(&now));
	return 0;
}

void pack_file(char *path)
{
	char cmd[256] = {0};

	fclose(fp);
	sprintf(cmd, "flock -xn %s tar -zcvf %s.gz %s"
			"&& flock -xn %s ln %s.gz %s.ar",
			FILE_DIR,  glb_fname, glb_fname,
			FILE_DIR, glb_fname, glb_fname);
	chdir(FILE_DIR);
	system(cmd);
	sprintf(cmd, "rm -f %s", path);
	system(cmd);
	fp = NULL;
	return;
}

static void need_pack_file(void)
{
	struct sysinfo sinfo;
	char path[128] = { 0, };

	if (!fp) 
		return ;
	sysinfo(&sinfo);
	if (sinfo.uptime - ftime < MAX_COMPRESS_FILE_TIME)
		return ;
	sprintf(path, "%s/%s", FILE_DIR, glb_fname);
	pack_file(path);
	return ;
}

int get_filename(void)
{
	uint32_t wan_ip = 0;
	char path[128];
	struct sysinfo sinfo;

	if (fp)
		return 0;

	sysinfo(&sinfo);
	ftime = sinfo.uptime;
	wan_ip = info.ip.s_addr;
	gen_new_filename((unsigned char *)&wan_ip, mac);
	sprintf(path, "%s/%s", FILE_DIR, glb_fname);
	fp = fopen(path, "ab");
	if (!fp) 
		return -1;
	return 0;
}

int write_file(struct id_list *idh)
{
	struct group_pck_info *gpi = NULL;
	struct group_pck_info *ptr = NULL;

	SZXS_DEBUG("begin write_file\n");
	fprintf(fp, "MAC: %X:%X:%X:%X:%X:%X\n",
			idh->mac[0], idh->mac[1],
			idh->mac[2], idh->mac[3],
			idh->mac[4], idh->mac[5]);
	fprintf(fp, "Time: %ld\n", idh->rcv_time);
	list_for_each_entry_safe(gpi, ptr, &idh->gpi_head, lh) {
		fwrite(gpi->data, 1, gpi->data_len, fp);
		list_del(&gpi->lh);
		free(gpi);
		gpi = NULL;
	}
	fprintf(fp, "#XT#");
	return 0;
}

int pck_filter(struct list_head *idh, uint16_t *data_len)
{
	time_t now = 0;
	struct id_list *il = NULL;
	struct id_list *il_ptr = NULL;
	struct group_pck_info *gpi = NULL;

	SZXS_DEBUG("begin pck_filter\n");
	time(&now);
	if (list_empty(&glb_id_lh))
		return 0;
	list_for_each_entry_safe(il, il_ptr, &glb_id_lh, lh) {
		gpi = list_entry(il->gpi_head.next, struct group_pck_info, lh);
		if ((strncasecmp(gpi->data, "GET", 3) == 0
					&& now - il->rcv_time > 3)
				/* || (strncasecmp(gpi->data, "POST", 4) == 0
				   && now - il->rcv_time > 6) */
				|| now - il->rcv_time > 6) {
			list_del(&il->lh);
			list_add_tail(&il->lh, idh);
			*data_len += gpi->data_len;
		}
	}
	return 0;
}

int gpi_free(struct list_head *wlh)
{
	struct group_pck_info *gpi = NULL;
	struct group_pck_info *ptr = NULL;

	list_for_each_entry_safe(gpi, ptr, wlh, lh) {
		list_del(&gpi->lh);
		free(gpi);
		gpi = NULL;
	}
	return 0;
}

int write_filter_file(struct list_head *idh)
{
	struct group_pck_info *gpi = NULL;
	struct id_list *il = NULL;
	struct id_list *id_ptr = NULL;
	SZXS_DEBUG("begin write_filter_file\n");

	list_for_each_entry_safe(il, id_ptr, idh, lh) {
		gpi = list_entry(il->gpi_head.next, struct group_pck_info, lh);
		fprintf(fp, "MAC: %X:%X:%X:%X:%X:%X\n",
				il->mac[0], il->mac[1], il->mac[2],
				il->mac[3], il->mac[4], il->mac[5]);
		fprintf(fp, "Time: %ld\n", il->rcv_time);
		fwrite(gpi->data, 1, gpi->data_len, fp);
		fprintf(fp, "#XT#");
		gpi_free(&il->gpi_head);
		list_del(&il->lh);
		free(il);
	}
	return 0;
}

struct id_list *put_into_wait(struct group_pck_info *gpi)
{
	struct id_list *il;
	struct id_list *res = NULL;
	struct group_pck_info *pkt;
	int i = 0;
	struct nlk_http_header *nlkh = NULL;

	SZXS_DEBUG("put_into_wait\n");

	/* search first */
	list_for_each_entry(il, &glb_id_lh, lh) {
		if (il->id != gpi->id) 
			continue;
		goto found;
	}

	/*  doesn't find, create it or free */
	if (gpi->seq)
		goto free;

	il = (struct id_list *)malloc(sizeof(struct id_list));
	if (!il) 
		goto free;
	il->id = gpi->id;
	il->tot_len = gpi->data_len;
	nlkh = NLMSG_DATA(gpi->buffer);
	for (i=0; i<6; i++)
		il->mac[i] = nlkh->mac[i];
	time(&il->rcv_time);
	INIT_LIST_HEAD(&il->gpi_head);
	list_add_tail(&il->lh, &glb_id_lh);
	list_add_tail(&gpi->lh, &il->gpi_head);
	return NULL;

found:
	if (gpi->seq) {
		res = il;
		goto free;
	}

	if (il->tot_len > POST_MAX_LEN)
		goto free;

	list_for_each_entry(pkt, &il->gpi_head, lh) {
		if (gpi->tcp_seq == pkt->tcp_seq)
			goto free;
		if (gpi->tcp_seq < pkt->tcp_seq)
			break;
	}

	list_add(&gpi->lh, pkt->lh.prev);
	il->tot_len += gpi->data_len;
	return NULL;

free:
	free(gpi);
	return res;
}

void cp_buffer_to_gpi(void *ptr, struct group_pck_info *gpi)
{
	struct nlk_http_header *nlkh;
	uint16_t offset = 0;
	__be16 ip_tot_len = 0;
	__u8 ip_ihl = 0;
	__u16 tcp_doff = 0;
	struct iphdr *iph;
	struct tcphdr *tcph;

	SZXS_DEBUG("begin get_each_package\n");
	nlkh = ptr;

	gpi->id = nlkh->id;

	gpi->syn = nlkh->syn;

	gpi->seq = nlkh->seq;
	if (gpi->seq)
		return ;

	iph = (void *)nlkh->data;
	ip_tot_len = ntohs(iph->tot_len);
	ip_ihl = iph->ihl;
	offset += ip_ihl * 4;

	tcph = (void *)(nlkh->data + offset);
	gpi->tcp_seq = ntohl(tcph->seq);
	tcp_doff = tcph->doff;
	offset += tcp_doff * 4;

	gpi->data_len = ip_tot_len - ip_ihl * 4 - tcp_doff * 4;
	gpi->data = nlkh->data + offset;
	return ;
}

int pck_age_timer(void)
{
	uint16_t data_len = 0;
	LIST_HEAD(idh);

	pck_filter(&idh, &data_len);
	if (list_empty(&idh))
		return 0;
	write_filter_file(&idh);
	return 0;
}

int multi_pck_proc(struct id_list *idh)
{
	if (!idh) 
		return 0;

	SZXS_DEBUG("begin multi_pck_proc\n");
	list_del(&idh->lh);
	write_file(idh);
	free(idh);
	return 0;
}

int pack_pck_file(void)
{
	unsigned long filesize = 0;
	int ret = 0;
	char path[128] = {0};

	SZXS_DEBUG("begin pack_pck_file\n");
	sprintf(path, "%s/%s", FILE_DIR, glb_fname);
	ret = get_file_size(path, &filesize);
	if (ret < 0)
		return -1;
	if (filesize > max_file_size)
		pack_file(path);
	return 0;
}

int get_all_compress_file_size(unsigned long *size, char *extension)
{
	DIR *dir = NULL;
	struct dirent *ptr = NULL;
	struct stat statbuff;
	char path[64] = {0};

	dir = opendir(FILE_DIR);
	if (dir == NULL)
		return -1;
	while ((ptr = readdir(dir)) != NULL) {
		if (memcmp(ptr->d_name + strlen(ptr->d_name) - strlen(extension),
					extension, strlen(extension)))
			continue;
		sprintf(path, "%s/%s", FILE_DIR, ptr->d_name);
		if (stat(path, &statbuff)) {
			closedir(dir);
			return -1;	
		}
		*size += statbuff.st_size;
	}
	closedir(dir);
	return 0;
}

void del_all_compress_files(char *extension)
{
	char cmd[16] = {0};

	chdir(FILE_DIR);
	sprintf(cmd, "rm -f *%s", extension);
	system(cmd);
}

int compress_file_proc(char *extension)
{
	unsigned long size = 0;
	int ret = 0;

	ret = get_all_compress_file_size(&size, extension);
	if (ret)
		return -1;
	if (size < max_compress_file_size)
		return 0;
	del_all_compress_files(extension);
	return 0;
}

/*
 * add gpi to list , or gpi need free
 */
int nlk_szxs_call(struct group_pck_info *gpi)
{
	struct id_list *idh;

	cp_buffer_to_gpi(NLMSG_DATA(gpi->buffer), gpi);
	SZXS_DEBUG("id:%u\n", gpi->id);
	SZXS_DEBUG("seq:%u\n", gpi->seq);
	SZXS_DEBUG("data:%s\n", gpi->data);
	idh = put_into_wait(gpi);
	multi_pck_proc(idh);
	/* pack oversize pck file */
	pack_pck_file();

	return 0;
}

void format_mac(unsigned char *mac, char *macptr)
{
	char buf[18] = {0};

	sprintf(buf, "%02X:%02X:%02X:%02X:%02X:%02X",
			mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
	strcpy(macptr, buf);
}

int format_ip(uint32_t *ip, char *ipstr)
{
	int i = 0;
	int count = 0;
	int offset = 0;
	struct in_addr addr;

	for (i=0; i<DNS_ADDR_CAP_MX; i++)
	{
		if ((*ip) <= 0)
			break;
		memset(&addr, 0x00, sizeof(addr));
		addr.s_addr = htonl(*ip);
		sprintf(ipstr + offset, "%s,", inet_ntoa(addr));
		ip++;
		count++;
		offset = strlen(ipstr);
	}
	ipstr[offset - 1] = '\0';
	return count;
}

void write_dns_info(void *data)
{
	struct nlk_dns_msg *ndm = NULL;
	char buf[18] = {0};
	char ipstr[80] = {0};
	time_t now = 0;

	ndm = (struct nlk_dns_msg *)data;
	fprintf(fp, "dns;");
	format_mac(ndm->mac, buf);
	fprintf(fp, "%s;", buf);
	fprintf(fp, "%s;", ndm->dns);
	format_ip(ndm->addr, ipstr);
	fprintf(fp, "%s;",  ipstr);
	time(&now);
	fprintf(fp, "%ld#XT#", now);
}

int nlk_dns_call(void *data)
{
	write_dns_info(data);
	return pack_pck_file();
}

int get_nlk_msg(struct nlk_msg_handler *nlh)
{
	uint16_t rcv_len = 0;
	uint16_t min_len = 0;
	struct nlk_msg_comm *comm = NULL;
	struct nlk_sys_msg *nlk = NULL;
	struct group_pck_info *gpi = NULL;

	SZXS_DEBUG("begin get_nlk_msg\n");
	gpi = (struct group_pck_info *)malloc(sizeof(struct group_pck_info));
	if (!gpi) 
		return -1;
	memset(gpi, 0, sizeof(struct group_pck_info));

	if ((rcv_len = nlk_uk_recv(nlh, gpi->buffer, sizeof(gpi->buffer))) <= 0)
		goto GPI_RELEASE;
	comm = (struct nlk_msg_comm *)NLMSG_DATA(gpi->buffer);
	switch (comm->gid) {
		case NLKMSG_GRP_IF:
			if (comm->mid != IF_GRP_MID_IPCGE)
				break;
			nlk = NLMSG_DATA(gpi->buffer);
			if (nlk->msg.ipcp.type != IF_TYPE_WAN)
				break;
			if (nlk->comm.action != NLK_ACTION_ADD)
				break;
			info.ip.s_addr = nlk->msg.ipcp.ip.s_addr;
			break;
		case NLKMSG_GRP_HTTP:
			min_len = sizeof(struct nlmsghdr) + sizeof(struct nlk_http_header)
				+ sizeof(struct iphdr) + sizeof(struct tcphdr);
			if (rcv_len <= min_len)
				break;
			return nlk_szxs_call(gpi);
		case NLKMSG_GRP_DNS:
			return nlk_dns_call(NLMSG_DATA(gpi->buffer));
		default:
			break;
	}

GPI_RELEASE:
	free(gpi);
	gpi = NULL;
	return 0;
}

int main(int argc, char *argv[])
{
	int grp = 0;
	int max_fd = 0;
	fd_set fds;
	struct nlk_msg_handler nlh;
	struct timeval tv;
	int uid = 0;
	time_t now = 0;

	while (1) {
		if (xs_lock() > 0)
			break;
		sleep(1);
	}

	max_file_size = 300 * 1024;
	max_compress_file_size = 500 * 1024;
	if (argc > 1)
		max_file_size = strtoul(argv[1], NULL, 10) * 1024;
	if (argc > 2)
		max_compress_file_size = strtoul(argv[2], NULL, 10) * 1024;

	signal(SIGPIPE, SIG_IGN);

	uid = 1;
	memset(&info, 0x0, sizeof(info));
	while (1) {
		mu_msg(IF_MOD_IFACE_INFO, &uid, sizeof(uid),
				&info, sizeof(info));
		if (info.ip.s_addr) 
			break;
		sleep(1);
	}
	read_mac(mac);

	if (access(FILE_DIR, 0))
		mkdir(FILE_DIR, S_IRWXU);

	grp = 1 << (NLKMSG_GRP_IF - 1);
	grp |= 1 << (NLKMSG_GRP_HTTP - 1);
	grp |= 1 << (NLKMSG_GRP_DNS - 1);
	nlk_event_init(&nlh, grp);
	time(&start_time1);
	time(&start_time2);
	while (1) {
		time(&now);
		if ((now - start_time1) > 120) {
			/* deal with oversize gz files */
			compress_file_proc(".gz");
			compress_file_proc(".ar");
			start_time1 = now;
			need_pack_file();
		}

		if (get_filename()) {
			/*  avoid cpu 100% */
			sleep(1);
			continue;
		}

		if (now - start_time2 > 3) {
			/* deal with overtime and overlength pck in list */
			pck_age_timer();
			start_time2 = now;
		}

		tv.tv_sec = 1;
		tv.tv_usec = 0;
		FD_ZERO(&fds);
		IGD_FD_SET(nlh.sock_fd, &fds);
		if (select(max_fd + 1, &fds, NULL, NULL, &tv) < 0) {
			if (errno == EINTR || errno == EAGAIN)
				continue;
		}
		if (FD_ISSET(nlh.sock_fd, &fds))
			get_nlk_msg(&nlh);
	}
	return 0;
}
