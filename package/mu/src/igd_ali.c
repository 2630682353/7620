#include "igd_lib.h"
#include "igd_ali.h"
#include "igd_interface.h"
#include "igd_system.h"

extern int get_sysflag(unsigned char bit);

static unsigned char ali_first = 0;
static int status;
void ali_restart(void *data)
{
	int uid = 1;
	struct timeval tv;
	struct iface_info info;
	struct plug_daemon_info daemon;

	tv.tv_sec = 5;
	tv.tv_usec = 0;
	mu_call(IF_MOD_IFACE_INFO, &uid, sizeof(uid), &info, sizeof(info));
	if ((info.net == NET_ONLINE) && (status == NET_OFFLINE)) {
		mu_call(SYSTEM_MOD_DAEMON_DEL, ALI_CLOUD, strlen(ALI_CLOUD) + 1, NULL, 0);

		memset(&daemon, 0, sizeof(daemon));
		arr_strcpy(daemon.name, ALI_CLOUD);
		daemon.oom = 200;
		mu_call(SYSTEM_MOD_DAEMON_PLUG_ADD, &daemon, sizeof(daemon), NULL, 0);
	}
	status = info.net;
	schedule(tv, ali_restart, NULL);
}

int ali_reset_flag(int *in, int in_len, int *out, int out_len)
{
	if (access(ALI_INIT_FILE, F_OK))
		return 0;

	if (!out && !in) {
		return ali_first ? 1 : 0;
	} else if (in) {
		ali_first = 0;
		return set_sysflag(SYSFLAG_FIRST, 0);
	}
	return -1;
}

int ali_init(void)
{
	if (access(ALI_INIT_FILE, F_OK))
		return 0;

	if (get_sysflag(SYSFLAG_FIRST) == 1)
		ali_first = 1;

	mu_call(SYSTEM_MOD_DAEMON_ADD, ALI_CLOUD, strlen(ALI_CLOUD) + 1, NULL, 0);
	ali_restart(NULL);
	return 0;
}

int ali_call(MSG_ID msgid, void *data, int len, void *rbuf, int rlen)
{
	int ret = -1;

	switch (msgid) {
	case ALI_MOD_INIT:
		ret = ali_init();
		break;
	case ALI_MOD_GET_RESET:
		ret = ali_reset_flag(data, len, rbuf, rlen);
		break;
	default:
		break;
	}
	return ret;
}
