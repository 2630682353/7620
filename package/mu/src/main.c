#include "igd_lib.h"
#include "igd_interface.h"
#include "igd_host.h"
#include "igd_dnsmasq.h"
#include "igd_wifi.h"
#include "igd_qos.h"
#include "igd_url_safe.h"
#include "igd_url_log.h"
#include "igd_system.h"
#include "igd_upnp.h"
#include "igd_nat.h"
#include "igd_ali.h"
#include "igd_cloud.h"
#include "igd_advert.h"
#include "igd_vpn.h"
#include "igd_lanxun.h"
#include "igd_plug.h"
#include "igd_log.h"
#include "igd_tunnel.h"
#include "igd_route.h"
#include "aes.h"

static void register_module(MOD_ID mid, int (*module_hander)(MSG_ID msgid, void *data, int len, void *rbuf, int rlen))
{
	if (mid > MODULE_MX - 1)
		return;
	MODULE[mid].mid = mid;
	MODULE[mid].module_hander = module_hander;
}

int ipc_call(struct ipc_header *msg)
{
	char *in = msg->len ? IPC_DATA(msg) : NULL;
	char *out = msg->reply_len ? IPC_DATA(msg->reply_buf) : NULL;

	return mu_call(msg->msg, in, msg->len, out, msg->reply_len);
}

int nlk_call(struct nlk_msg_handler *nlh)
{
	char buf[4096] = {0,};
	struct nlk_cloud_config *config;
	struct nlk_msg_comm *comm;
	struct nlk_host *host = NULL;
	struct nlk_msg_l7 *app = NULL;
	struct ua_msg *ua = NULL;
	struct nlk_dhcp_msg *dhcp = NULL;
	struct nlk_alone_config *alone = NULL;
	struct nlk_switch_config *sw_st = NULL;
	struct nlk_dns_msg *ndm = NULL;
	struct nlk_sys_msg *sys = NULL;
	struct nlk_sys_msg *if_msg = NULL;

#ifdef MU_DBG_TT
	long now, pre;

	pre = sys_uptime();
	MU_DEBUG("[%ld]:nlk start\n", pre);
#endif

	if (nlk_event_recv(nlh, buf, sizeof(buf)) <= 0)
		return -1;
	comm = (struct nlk_msg_comm *)buf;

#ifdef MU_DBG_TT
	now = sys_uptime();
	MU_DEBUG("[%ld]:nlk info, %d,%d,%d\n",
		now, comm->gid, comm->action, comm->mid);
#endif

	switch (comm->gid) {
	case NLKMSG_GRP_HOST:
		host = (struct nlk_host *)buf;
		if (comm->action == NLK_ACTION_ADD) {
			mu_call(IGD_HOST_ONLINE, host, sizeof(*host), NULL, 0);
			mu_call(WIFI_MOD_VAP_HOST_ONLINE, host, sizeof(*host), NULL, 0);
			mu_call(NAT_MOD_HOSTUP, host, sizeof(*host), NULL, 0);
			ADD_SYS_LOG("终端 %02x:%02x:%02x:%02x:%02x:%02x "
						"连接路由器, ip %s", MAC_SPLIT(host->mac),
						inet_ntoa(host->addr));
		} else if (comm->action == NLK_ACTION_DEL) {
			mu_call(IGD_HOST_OFFLINE, host, sizeof(*host), NULL, 0);
			mu_call(WIFI_MOD_VAP_HOST_OFFLINE, host, sizeof(*host), NULL, 0);
			mu_call(UPNPD_HOSTOFFLINE, host, sizeof(*host), NULL, 0);
			mu_call(NAT_MOD_HOSTDOWN, host, sizeof(*host), NULL, 0);
		}
		break;
	case NLKMSG_GRP_L7:
		app = (struct nlk_msg_l7 *)buf;
		if (comm->action == NLK_ACTION_ADD) {
			mu_call(IGD_HOST_APP_ONLINE, app, sizeof(*app), NULL, 0);
		} else if (comm->action == NLK_ACTION_DEL) {
			mu_call(IGD_HOST_APP_OFFLINE, app, sizeof(*app), NULL, 0);
		}
		break;
	case NLKMSG_GRP_SYS:
		if (comm->mid == SYS_GRP_MID_ONLINE) {
			mu_call(IF_MOD_NET_STATUS, &comm->action, sizeof(comm->action), NULL, 0);
		} else if (comm->mid == SYS_GRP_MID_CONF) {
			config = (struct nlk_cloud_config *)buf;
			if (config->ver[CCT_L7])
				system("/etc/init.d/l7 restart &");
			if (config->ver[CCT_UA])
				mu_call(IGD_HOST_UA_UPDATE, NULL, 0, NULL, 0);
			if (config->ver[CCT_DOMAIN])
				mu_call(URL_LOG_MOD_SET_URL, NULL, 0, NULL, 0);
			if (config->ver[CCT_WHITE])
				mu_call(URL_SAFE_MOD_SET_WLIST, NULL, 0, NULL, 0);
			if (config->ver[CCT_VENDER])
				mu_call(IGD_HOST_VENDER_UPDATE, NULL, 0, NULL, 0);
			if (config->ver[CCT_STUDY_URL])
				mu_call(IGD_STUDY_URL_UPDATE, NULL, 0, NULL, 0);
			if (config->ver[CCT_URLSTUDY])
				mu_call(IGD_STUDY_URL_DNS_UPDATE, NULL, 0, NULL, 0);
			if (config->ver[CCT_URLLOG])
				mu_call(URL_LOG_MOD_SET_SERVER, NULL, 0, NULL, 0);
			if (config->ver[CCT_URLSAFE] || config->ver[CCT_URLREDIRECT])
				mu_call(URL_SAFE_MOD_SET_SERVER, NULL, 0, NULL, 0);
			if (config->ver[CCT_VPN_SERVEER])
				mu_call(VPN_MOD_SET_SERVER, NULL, 0, NULL, 0);
			if (config->ver[CCT_VPN_DNS])
				mu_call(VPN_MOD_SET_DNSLIST, NULL, 0, NULL, 0);
			if (config->ver[CCT_URLHOST])
				mu_call(ADVERT_MOD_SET_SERVER, NULL, 0, NULL, 0);
		} else if (comm->mid == SYS_GRP_MID_DAY) {
			mu_call(IGD_HOST_DAY_CHANGE, NULL, 0, NULL, 0);
		} else if (comm->mid == SYS_GRP_MID_DHCP) {
			dhcp = (struct nlk_dhcp_msg *)buf;
			mu_call(IGD_HOST_DHCP_INFO, dhcp->dhcp, sizeof(dhcp->dhcp), NULL, 0);
		} else if (comm->mid == SYS_GRP_MID_ALONE) {
			alone = (struct nlk_alone_config *)buf;
			if (alone->flag == CCA_JS) {
				mu_call(ADVERT_MOD_UPDATE, NULL, 0, NULL, 0);
			} else if (alone->flag == CCA_P_L7) {
				system("/etc/init.d/l7 restart &");
			} else if (alone->flag == CCA_SYSTEM) {
				#define CCASYSF "/tmp/rconf/system.txt"
				mu_call(ADVERT_MOD_SYSTEM, CCASYSF, strlen(CCASYSF) + 1, NULL, 0);
				remove(CCASYSF);
			}
		} else if (comm->mid == SYS_GRP_MID_SWITCH) {
			sw_st = (struct nlk_switch_config *)buf;
			mu_call(IGD_PLUG_SWITCH, sw_st, sizeof(*sw_st), NULL, 0);
		} else if (comm->mid == SYS_GRP_MID_OOM) {
			sys = (struct nlk_sys_msg *)buf;
			mu_call(SYSTEM_MOD_DAEMON_OOM, &sys->msg.oom, sizeof(sys->msg.oom), NULL, 0);
		}
		break;
	case NLKMSG_GRP_LOG:
		ua = (struct ua_msg *)buf;
		if (comm->mid == LOG_GRP_MID_URL) {
			mu_call(URL_LOG_MOD_DUMP_URL, NULL, 0, NULL, 0);
		} else if (comm->mid == LOG_GRP_MID_HOST) {
			mu_call(IGD_HOST_NAME_UPDATE, ua, sizeof(*ua), NULL, 0);
		} else if (comm->mid == LOG_GRP_MID_OS_TYPE) {
			mu_call(IGD_HOST_OS_TYPE_UPDATE, ua, sizeof(*ua), NULL, 0);
		} else if (comm->mid == LOG_GRP_MID_UA) {
			mu_call(IGD_HOST_UA_COLLECT, ua, sizeof(*ua), NULL, 0);
		} else if (comm->mid == LOG_GRP_MID_RULE) {
			mu_call(ADVERT_MOD_LOG_DUMP, NULL, 0, NULL, 0);
		} else if (comm->mid == LOG_GRP_MID_HTTP) {
			system("cloud_exchange u&");
		}
		break;
	case NLKMSG_GRP_DNS:
		ndm = (struct nlk_dns_msg*)buf;
		mu_call(IGD_CLOUD_DNS_UPLOAD, ndm, sizeof(*ndm), NULL, 0);
		break;
	case NLKMSG_GRP_IF:
		if_msg = (struct nlk_sys_msg *)buf;
		if ((if_msg->comm.mid != IF_GRP_MID_IPCGE) ||
			(if_msg->comm.action != NLK_ACTION_ADD))
			break;
		mu_call(ROUTE_MOD_RELOAD, NULL, 0, NULL, 0);
		break;
	default:
		MU_ERROR("gid err, %d\n", comm->gid);
		break;
	}

#ifdef MU_DBG_TT
	now = sys_uptime();
	MU_DEBUG("[%ld]:nlk end, %d,%d,%d\n",
		now, comm->gid, comm->action, comm->mid);
#endif
	return 0;
}

int  mu_init(void)
{
	init_scheduler();

	register_module(MODULE_WIFI, wifi_call);
	register_module(MODULE_INTERFACE, interface_call);
	register_module(MODULE_DNSMASQ, dnsmasq_call);
	register_module(MODULE_HOST, igd_host_call);
	register_module(MODULE_QOS, qos_call);
	register_module(MODULE_URLSAFE, urlsafe_call);
	register_module(MODULE_URLLOG, urllog_call);
	register_module(MODULE_SYSTEM, system_call);
	register_module(MODULE_UPNPD, upnpd_call);
	register_module(MODULE_NAT, nat_call);
	register_module(MODULE_ALI, ali_call);
	register_module(MODULE_CLOUD, igd_cloud_call);
	register_module(MODULE_ADVERT, advert_call);
	register_module(MODULE_VPN, vpn_call);
	register_module(MODULE_LANXUN, lanxun_call);
	register_module(MODULE_PLUG, igd_plug_call);
	register_module(MODULE_LOG, log_call);
	register_module(MODULE_TUNNEL, tunnel_call);
	register_module(MODULE_ROUTE, route_call);

	mu_call(LOG_MOD_INIT, NULL, 0, NULL, 0);

	mu_call(WIFI_MOD_INIT, NULL, 0, NULL, 0);
	
	mu_call(DNSMASQ_MOD_INIT, NULL, 0, NULL, 0);
	
	mu_call(URL_SAFE_MOD_INIT, NULL, 0, NULL, 0);
	
	mu_call(UPNPD_MOD_INIT, NULL, 0, NULL, 0);
	
	mu_call(NAT_MOD_INIT, NULL, 0, NULL, 0);
	
	mu_call(URL_LOG_MOD_INIT, NULL, 0, NULL, 0);
	
	mu_call(IGD_HOST_INIT, NULL, 0, NULL, 0);

	mu_call(IGD_CLOUD_INIT, NULL, 0, NULL, 0);

	mu_call(QOS_MOD_INIT, NULL, 0, NULL, 0);
	
	mu_call(SYSTEM_MOD_INIT, NULL, 0, NULL, 0);

	mu_call(ALI_MOD_INIT, NULL, 0, NULL, 0);

	mu_call(ADVERT_MOD_INIT, NULL, 0, NULL, 0);

	mu_call(VPN_MOD_INIT, NULL, 0, NULL, 0);
	
	mu_call(IF_MOD_INIT, NULL, 0, NULL, 0);

	mu_call(IGD_PLUG_INIT, NULL, 0, NULL, 0);

	mu_call(TUNNEL_MOD_INIT, NULL, 0, NULL, 0);

	mu_call(ROUTE_MOD_INIT, NULL, 0, NULL, 0);
	
	return 0;
}

int do_daemon(void)
{
	pid_t pid;

	pid = fork ();
	if (pid < 0)
		return -1;
	if (pid != 0)
		exit (0);
	pid = setsid();
	if (pid < -1)
		return -1;
	if (chdir("/")) 
		exit(-1);
	return 0;
}

#ifdef WIAIR_SDK
static void check_live(void *data)
{
	struct nlk_msg_comm nlk;
	unsigned char buf[AES_BLOCK_SIZE] = {0x11, 0x22, 0x33, 0x44, };
	unsigned char out[AES_BLOCK_SIZE];
	aes_encrypt_ctx en_ctx[1];
	struct timeval tv;
	uint32_t rand;
	struct timespec now;
	char pwd[16] = {0xfc, 0xec, 0x03, 0xcc,
	       	 	0x5c, 0x77, 0xde, 0x75,
			0x80, 0x35, 0x79, 0x7c};

	clock_gettime(CLOCK_MONOTONIC, &now);
	rand = (uint32_t)lrand48();
	memcpy(&buf[4], &rand, sizeof(rand));
	*((uint64_t *)&buf[8]) = now.tv_sec;
	memset(&nlk, 0, sizeof(nlk));
	nlk.key = 0;
	nlk.action = NLK_ACTION_ADD;

	aes_encrypt_key128(pwd, en_ctx);
	aes_ecb_encrypt(buf, out, AES_BLOCK_SIZE, en_ctx);
	nlk_data_send(NLK_MSG_END-1, &nlk,
		       	sizeof(nlk), out, sizeof(out));
	tv.tv_sec = 5*60;
	tv.tv_usec = 0;
	schedule(tv, check_live, NULL);
	return ;
}
#endif

int main(int argc, char *argv[])
{
	fd_set fds;
	int grp, max_fd = 0, ipc_fd;
	struct timeval tv, *ptv = NULL;
	struct nlk_msg_handler nlh;
	struct sys_waitpid daemon;

	tv.tv_sec = 1451577600;
	tv.tv_usec = 0;
	settimeofday(&tv, NULL);

	do_daemon();
	grp = 1 << (NLKMSG_GRP_SYS - 1);
	grp |= 1 << (NLKMSG_GRP_HOST - 1);
	grp |= 1 << (NLKMSG_GRP_L7 - 1);
	grp |= 1 << (NLKMSG_GRP_LOG - 1);
	grp |= 1 << (NLKMSG_GRP_DNS - 1);
	grp |= 1 << (NLKMSG_GRP_IF - 1);
	nlk_event_init(&nlh, grp);
	ipc_fd = ipc_server_init(IPC_PATH_MU);
	chmod(IPC_PATH_MU, 0777);
	mu_init();

#ifdef WIAIR_SDK
	tv.tv_sec = 60;
	tv.tv_usec = 0;
	schedule(tv, check_live, NULL);
#endif
	
	for (;;) {
		FD_ZERO(&fds);
		IGD_FD_SET(ipc_fd, &fds);
		IGD_FD_SET(nlh.sock_fd, &fds);

		ptv = process_schedule(&tv);

		if (select(max_fd + 1, &fds, NULL, NULL, ptv) < 0) {
			if (errno == EINTR || errno == EAGAIN)
				continue;
		}

		if (FD_ISSET(ipc_fd, &fds))
			ipc_server_accept(ipc_fd, ipc_call);

		if (FD_ISSET(nlh.sock_fd, &fds))
			nlk_call(&nlh);

		daemon.pid = waitpid(-1, &daemon.status, WNOHANG);
		if (daemon.pid > 0) {
			mu_call(SYSTEM_MOD_WAITPID, &daemon, sizeof(daemon), NULL, 0);
		}
	}
	return 0;
}
