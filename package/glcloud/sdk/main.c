#include <stdio.h>
#include <netdb.h>
#include <sys/socket.h>
#include <time.h>
#include <sys/time.h>
#include <signal.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include "gl_api.h"
#include "ipc_msg.h"
#include "nlk_ipc.h"
#include "glcloud.h"

#define SUPPORT_FILE_CLOUD
#ifdef SUPPORT_FILE_CLOUD
#include "gl_file_cloud.h"
#endif

#include "nlk_ipc.h"

#if 0
#define GLCLOUD_DBG(fmt,args...) do{}while(0)
#else
#define GLCLOUD_DBG(fmt,args...) \
	igd_log("/tmp/glcloud_dbg", "DBG:%s[%d]:"fmt, __FUNCTION__, __LINE__, ##args);
#endif

extern int get_sysflag(unsigned char bit);

static char *glcloud_get_product(void)
{
	int l;
	char name[128];

	l = snprintf(name, sizeof(name), "%s", read_firmware("VENDOR"));
	l = snprintf(name + l, sizeof(name) - l, "_%s", read_firmware("PRODUCT"));

	GLCLOUD_DBG("%s\n", name);
	return strdup(name);
}

static char *glcloud_get_version(void)
{
	char *ver;

	ver = read_firmware("CURVER") ? : "1.0.0";

	GLCLOUD_DBG("%s\n", ver);
	return strdup(ver);
}

/*
 * 用户数据回调函数，请在次处理用户数据
 */
void glcloud_callback(void *msg, gl_recv_type_e type)
{
	gl_data_t *data = NULL;
	gl_file_info_t *file_info = NULL;
	gl_file_percent_t *file_percent = NULL;
	gl_file_token_t *file_token = NULL;

	GLCLOUD_DBG("type:%d\n", type);

	switch (type) {
	case GL_MSG_DATA_FROM_APP:
		data = (gl_data_t *)msg;
		GLCLOUD_DBG("data_from_app: [%s]\n", data->data);
		GLCLOUD_DBG("date len: %d\n", data->len);
		GLCLOUD_DBG("user_id: %d\n", data->msg_id);
		GLCLOUD_DBG("user_id: %d\n", data->user_id);
		break;
	case GL_MSG_DATA_FROM_APP_LAN:
		data = (gl_data_t *)msg;
		GLCLOUD_DBG("data_from_app: [%s]\n", data->data);
		GLCLOUD_DBG("date len: %d\n", data->len);
		GLCLOUD_DBG("user_id: %d\n", data->msg_id);
		GLCLOUD_DBG("user_id: %d\n", data->user_id);
		break;
#ifdef SUPPORT_FILE_CLOUD
	case GL_RECV_FILE_INFO:
		file_info = (gl_file_info_t *)msg;
		if (!file_info)
			break;
		GLCLOUD_DBG("GL_RECV_FILE_INFO, file_info->file_name:%s", file_info->file_name);
		gl_file_cloud_download("recv_data", file_info->file_name,
		file_info->file_url, file_info->check_key, file_info->encrypt_key);
		break;
	case GL_RECV_FILE_PERCENT:
		file_percent = (gl_file_percent_t *)msg;
		if (!file_percent)
			break;
		file_percent->percent = gl_file_cloud_get_upload_percent(file_percent->file_name);
		printf("my_callback(), GL_RECV_FILE_PERCENT, %d\n", file_percent->percent);
		//if (file_percent->percent >= 0 && file_percent->percent <= 100)
		{
			gl_send(file_percent, GL_SEND_FILE_PERCENT);
		}
		break;
	case GL_RECV_UPLOAD_TOKEN:
		file_token = (gl_file_token_t *)msg;
		if (!file_token)
			break;
		gl_file_cloud_set_token(file_token->token);
		break;
#endif
	default:
		printf("un-handled msg, type %d\n", type);
		break;
	}
}

static int glcloud_init(void)
{
	int ret, reset = 0;
	gl_cfg_t cfg;

	GLCLOUD_DBG("version: %s\n", gl_version());

	if ((get_sysflag(SYSFLAG_RECOVER) == 1)
		|| (get_sysflag(SYSFLAG_RESET) == 1)) {
		GLCLOUD_DBG("need reset\n");
		reset = 1;
	}

	memset(&cfg, 0, sizeof(cfg));

	/* 设备iemi、pin、产品id, 在个联开放平台申请. */
	cfg.imei = "06jhxck9epf2bt1";
	cfg.pin = "358572";
	cfg.product_id = "40001080040001";

	/* 产品名 */
	cfg.product_name = glcloud_get_product();

	/* 服务器选择，当前不支持，请置为0 */
	//cfg.host = 0x61463B7B;
	cfg.host = 0;

	/* 软件版本 */
	cfg.sw_ver = glcloud_get_version();

	/* 
	* 个联SDK数据，用于保存系统参数，大小为4K.
	* 如果不使用，请都设置为0.
	* ESP8266方案，由于需要删除整个数据块，需要传入一个扇区的地址，大小为4KB
	*/
	cfg.sdk_flash_addr = 0; //256*4096;// start at 1024k
	cfg.sdk_flash_size = 0; //0x1000;//4k

	/* 初始化个联SDK*/
	ret = gl_init(&cfg, glcloud_callback);
	if (ret != 0) {
		GLCLOUD_DBG("GL INIT FAIL, ret:%d\n", ret);
		return -1;
	}

	/* 等待个联SDK初始化完成 */
	GLCLOUD_DBG("wait init finished\n");
	do {
		ret = gl_state();
		sleep(1);
	} while (ret != GL_OK);
	GLCLOUD_DBG("gelian sdk init finished.\n");

	if (reset) {
		ret = gl_send(NULL, GL_CTRL_RESET_ALL);
		if (ret != GL_OK)
			GLCLOUD_DBG("glcloud reset fail\n");
	}

#ifdef SUPPORT_FILE_CLOUD
	gl_file_cloud_init();
#endif
	return 0;
}

int glcloud_ipc_msg(int msgid, void *data, int len, void *rbuf, int rlen)
{
	switch (msgid) {
	case 1:
		return 0;
	default:
		return -1;
	}
	return 0;
}

static int glcloud_ipc(struct ipc_header *msg)
{
	char *in = msg->len ? IPC_DATA(msg) : NULL;
	char *out = msg->reply_len ? IPC_DATA(msg->reply_buf) : NULL;

	GLCLOUD_DBG("%d, %p, %d, %p, %d\n", msg->msg, in, msg->len, out, msg->reply_len);
	return glcloud_ipc_msg(msg->msg, in, msg->len, out, msg->reply_len);
}


int main(int argc, char **argv)
{
	fd_set fds;
	struct timeval tv;
	int ipc_fd, max_fd;

	if (glcloud_init())
		return -1;

	ipc_fd = ipc_server_init(IPC_PATH_GLCLOUD);
	if (ipc_fd < 0) {
		GLCLOUD_DBG("ipc init err\n");
		return -1;
	}

	for (;;) {
		max_fd = 0;
		FD_ZERO(&fds);
		IGD_FD_SET(ipc_fd, &fds);

		tv.tv_sec = 2;
		tv.tv_usec = 0;
		if (select(max_fd + 1, &fds, NULL, NULL, &tv) < 0) {
			if (errno == EINTR || errno == EAGAIN)
				continue;
		}

		if (FD_ISSET(ipc_fd, &fds))
			ipc_server_accept(ipc_fd, glcloud_ipc);
	}
	return 0;
}
