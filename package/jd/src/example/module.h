#ifndef _IGD_MU_MODEULE_
#define _IGD_MU_MODEULE_

typedef unsigned long MSG_ID;
typedef unsigned long MOD_ID;

typedef struct mu_module {
	unsigned long mid;
	int (*module_hander)(MSG_ID msgid, void *data, int len, void *rbuf, int rlen);
}module_t;

#define DEFINE_MSG(module, code) (((module)<<16)|(code))
#define MODUEL_GET(msg) ((msg)>>16)

#define MODULE_INTERFACE 1
#define MODULE_HOST 2
#define MODULE_DNSMASQ 3
#define MODULE_WIFI 4
#define MODULE_QOS 5
#define MODULE_URLSAFE 6
#define MODULE_URLLOG 7
#define MODULE_SYSTEM 8
#define MODULE_UPNPD 9
#define MODULE_NAT 10
#define MODULE_ALI 11
#define MODULE_CLOUD 12
#define MODULE_ADVERT 13
#define MODULE_VPN 14
#define MODULE_LANXUN 15
#define MODULE_PLUG 16
#define MODULE_LOG 17

#define MODULE_MX 256

#define IPC_PATH_MU "/tmp/ipc_path_mu"
#define mu_msg(mgsid, data, len, rbuf, rlen) ipc_send(IPC_PATH_MU, mgsid, data, len, rbuf, rlen)

#ifdef FLASH_4_RAM_32
#define MU_DEBUG(fmt,args...) do{}while(0)
#else
#define MU_DBG_TT
#define MU_DEBUG(fmt, args...) igd_log("/tmp/mu.log", fmt, ##args)
#endif

#define MU_ERROR(fmt, args...) igd_log("/tmp/mu.err", fmt, ##args)

#endif
