#include "igd_lib.h"
#include "igd_qos.h"

static struct qos_conf config;

static int qos_module_save_param(void)
{
	char cmd[128] = {0};

	snprintf(cmd, 127, "%s.qos.enable=%d", QOS_CONFIG_FILE, config.enable);
	uuci_set(cmd);
	snprintf(cmd, 127, "%s.qos.up=%u", QOS_CONFIG_FILE, config.up);
	uuci_set(cmd);
	snprintf(cmd, 127, "%s.qos.down=%u", QOS_CONFIG_FILE, config.down);
	uuci_set(cmd);
	return 0;
}

static int get_tspped_info(struct tspeed_info *info)
{
	FILE *fp = NULL;
	char line[1024] = {0};
	struct if_statistics statis;
	int fd = 0, i = 0;

	fp = fopen(TSPEED_STATUS_FILE, "r");
	if (NULL == fp)
		return 0;

	i = 0;
	info->delay = 0;
	fd = fileno(fp);
	flock(fd, LOCK_EX);
	while (1) {
		memset(line, 0x0, sizeof(line));
		if (!fgets(line, sizeof(line) - 1, fp))
			break;
		if (!strncmp(line, TSF_PID, strlen(TSF_PID))) {
			info->flag = 1;
		} else if (!strncmp(line, TSF_DOWN, strlen(TSF_DOWN))) {
			info->flag = 2;
			info->down = atoi(line + strlen(TSF_DOWN) + 1);
		} else if (!strncmp(line, TSF_PING, strlen(TSF_PING))) {
			info->delay += atoi(line + strlen(TSF_PING) + 1);
			i++;
		} else if (!strncmp(line, TSF_ERR, strlen(TSF_ERR))) {
			if (strcmp(line + strlen(TSF_ERR) + 1, "abort")) {
				info->flag = 2;
				info->down = 0;
				break;
			}
		}
	}
	flock(fd, LOCK_UN);
	fclose(fp);
	info->delay = (i > 0) ? info->delay/i : 0;

	if (info->flag == 1) {
		memset(&statis, 0, sizeof(statis));
		get_if_statistics(1, &statis);
		info->ispeed = (statis.in.all.speed/1024) * 8;
	} else if (info->flag == 2) {
		info->down *= 8;
		info->up = info->down;
	}
	return 0;
}

static int qos_module_init(void)
{
	char *enable_s = NULL, *up_s = NULL, *down_s = NULL;
	
	memset(&config, 0x0, sizeof(config));
	enable_s = uci_getval(QOS_CONFIG_FILE, "qos", "enable");
	up_s = uci_getval(QOS_CONFIG_FILE, "qos", "up");
	down_s = uci_getval(QOS_CONFIG_FILE, "qos", "down");
	if (enable_s) {
		config.enable = atol(enable_s);
		free(enable_s);
	} else {
		config.enable = 0;
	}
	if (up_s) {
		config.up = atol(up_s);
		free(up_s);
	} else {
		config.enable = 0;
	}
	if (down_s) {
		config.down = atol(down_s);
		free(down_s);
	} else {
		config.enable = 0;
	}
	exec_cmd("qos-start.sh restart &");
	return 0;
}

int qos_call(MSG_ID msgid, void *data, int len, void *rbuf, int rlen)
{
	int ret = -1;
	switch (msgid) {
	case QOS_MOD_INIT:		
		ret = qos_module_init();
		break;
	case QOS_PARAM_SET:
		if (!data || len != sizeof(struct qos_conf))
			return -1;
		memcpy(&config, (struct qos_conf *)data, sizeof(config));
		qos_module_save_param();
		ret = exec_cmd("qos-start.sh restart &");
		break;
	case QOS_PARAM_SHOW:
		if (!rbuf || rlen != sizeof(struct qos_conf))
			return -1;
		memcpy((struct qos_conf *)rbuf, &config, sizeof(config));
		ret = 0;
		break;
	case QOS_TEST_SPEED:
		ret = exec_cmd("tspeed %s %s", TSPEED_CONFIG_FILE, TSPEED_STATUS_FILE);
		break;
	case QOS_SPEED_SHOW:
		if (!rbuf || rlen != sizeof(struct tspeed_info))
			return -1;
		ret = get_tspped_info((struct tspeed_info *)rbuf);
		break;
	case QOS_TEST_BREAK:
		ret = exec_cmd("tspeed break "TSPEED_STATUS_FILE);
		break;
	default:
		break;
	}
	return ret;	
}
