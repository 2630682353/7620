#ifndef _IGD_LANXUN_HEAD_
#define _IGD_LANXUN_HEAD_


#define LANXUN_SET_CLOSE DEFINE_MSG(MODULE_LANXUN, 0)
enum {
	LANXUN_SET_OPEN = DEFINE_MSG(MODULE_LANXUN, 1),
	LANXUN_GET_STATUS,
};

extern int lanxun_call(MSG_ID msgid, void *data, int len, void *rbuf, int rlen);
#endif
