#ifndef __WH_URL_SAFE__
#define __WH_URL_SAFE__

#ifdef FLASH_4_RAM_32
#define WH_URL_WLIST_MX_NR 2048
#define WH_URL_EVIL_MX_NR 512
#define WH_URL_CACHE_MX_NR 512
#else
#define WH_URL_WLIST_MX_NR 2048
#define WH_URL_EVIL_MX_NR 4096
#define WH_URL_CACHE_MX_NR 4096
#endif

#define WH_URL_REPORT_TIMOUT 5 * HZ
#define WH_URL_CHECK_TIMOUT	2 * HZ
#define WH_HTTP_PACK_TIMEOUT HZ / 5
#define WH_HTTP_TIMER_TIMEOUT HZ / 100
#define WH_CACHE_TIMER_TIMEOUT  24 * 60 * 60 * HZ
#define WH_EVIL_TIMER_TIMEOUT     24 * 60 * 60 * HZ
#define WH_URL_CHECK_TIMEOUT	HZ / 10

#define HTTP_CHECK_CMD 2002
#define HTTP_RECV_CMD 2003
#define HTTP_REPORT_CMD 2004

#define HTTP_CHECK_SRC_PORT 1000
#define HTTP_REPORT_SRC_PORT 1100

#define HTTP_CHECK_RECV_LEN 25
#define HTTP_CHECK_KEY "xlwifi305"

struct url_comm {
	uint16_t tot_len;
	uint16_t cmd;
	uint32_t devid;
};

struct wh_timer {
	long wh_timeout;
	struct timer_list timer; 
};

struct wh_url_safe_head {
	int enable;
	struct wh_timer evil_url_timer;/*check the evil url chche if > 1day free it*/
	struct wh_timer safe_url_timer;/*check the safe url chche if > 1day free it*/
	struct wh_timer http_pkg_timer;/*check the chche of http pkg, if > 200ms send it */
	struct wh_timer check_pkg_timer;/*if > 2s free the struct*/
	struct wh_url_cache_head url_cache;
	struct wh_url_cache_head url_evil;
	struct wh_url_cache_head url_vip;
	struct list_head http_skb_list;
	struct list_head check_skb_list;
	uint32_t device_id;
	uint16_t web_port;
	uint16_t url_server_port;
	uint32_t web_ip;
	uint32_t url_server_ip[WH_SERVER_IP_MX];
	char rd_url[IGD_URL_LEN];
};

spinlock_t wh_host_lock;
#define WH_HOST_LOCK() spin_lock_bh(&wh_host_lock)
#define WH_HOST_UNLOCK() spin_unlock_bh(&wh_host_lock)
#endif
