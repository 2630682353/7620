#ifndef __URL_SAFE_COMMON__
#define __URL_SAFE_COMMON__

#define WH_REDIRECT_URL_LEN 512
#define WH_SERVER_IP_MX 2
#define WH_WHITE_LIST_MX_NR 100

#define WH_HTTP_HEAD "HTTP/1.1 200 OK\r\nServer: HTTP Software 1.1\r\nDate: \
	Sat, 21 Dec 2009 12:00:00 GMT\r\nConnection: close\r\nContent-type: text/html\r\n\r\n"
#define WH_HTTP_WARN_SIZE_MX 1300
#define WH_HTTP_HEAD_SIZE (sizeof(WH_HTTP_HEAD) - 1)
#define WH_HTML_HEAD_BEGIN  	"<html><head><meta http-equiv=\"Content-Type\" content=\"text/html; charset=utf-8\">\n"
#define WH_HTML_HEAD_MID_2	"<meta http-equiv=\"refresh\" content=\"1; url=http://%s\" />"
#define WH_HTML_BODY  "</head><body >\n<div id=\"bg\"><div id=\"info\">\n"
#define WH_HTML_BR	"<br><br><br><br><br><br><br><br><br><br>"
#define WH_HTML_END  "</span></div></div></body></html>\n"
#define REDIRECT_URL "192.168.2.1?name=%s&a=%s&"

struct wh_safe_ip {
	struct list_head list;
	uint32_t ip;
};

struct wh_skb_list {
	uint32_t check_id;
	struct nf_http *http_info;
	struct list_head http_list;
	struct list_head check_list;
	struct sk_buff_head skb_queue;//http请求包缓存队列
	struct sk_buff *check_skb;//URL查询包
	unsigned long jiffies;
	__be32 sip;
	__be32 dip;
	uint16_t url_type;
	uint8_t wh_type;
	uint8_t send_times;//重发URL查询包次数
};

struct wh_url_cache {
	struct list_head list;//老化表
	struct list_head list_ip;//安全IP列表
	unsigned long jiffies;
	unsigned long last_time;
	struct nf_trie_node node;
	struct nf_trie_root *root;
	uint16_t url_type;
	uint16_t wh_type;
};

struct wh_url_cache_head {
	uint16_t nr;
	uint16_t mx;
	struct list_head list;//老化表
	struct nf_trie_root root;
};

extern int wh_url_del(struct wh_url_cache_head *h, unsigned char *host);
extern void wh_url_cache_free(struct wh_url_cache_head *h, struct wh_url_cache *url);
extern struct wh_url_cache * wh_add_url_cache(struct wh_url_cache_head *h, struct wh_skb_list *http);
extern struct wh_url_cache * wh_url_add(struct wh_url_cache_head *h, unsigned char *host, struct wh_skb_list *http);
extern struct wh_url_cache * _wh_add_url_cache(struct wh_url_cache_head *h, unsigned char *name, struct wh_skb_list *http);

#endif
