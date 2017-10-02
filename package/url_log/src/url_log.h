#ifndef _URL_COUNT_H_
#define _URL_COUNT_H_

struct url_log_cache {
	struct nf_trie_node node;
	uint16_t type;
	uint16_t id;
	uint16_t suffix_len;
	uint16_t uri_len;
	char suffix[IGD_SUFFIX_LEN];
	char uri[IGD_URI_LEN];
};

struct url_log_head {
	uint16_t mx;
	uint16_t nr;
	uint32_t obj_len;
	struct url_log_cache *data;
	struct nf_trie_root root;
};

spinlock_t url_count_lock;
#define URL_COUNT_LOCK() spin_lock_bh(&url_count_lock)
#define URL_COUNT_UNLOCK() spin_unlock_bh(&url_count_lock)

#endif
