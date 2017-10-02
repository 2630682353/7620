#include "igd_lib.h"
#include "igd_lanxun.h"
#include "uci.h"

static int lanxun_close()
{
	return 0;
}

static int lanxun_open()
{
	return 0;
}

int lanxun_call(MSG_ID msgid, void *data, int len, void *rbuf, int rlen)
{
	int * retvalue;
	int ret = -1;
	switch (msgid) {
	case LANXUN_SET_CLOSE:
		ret = lanxun_close();
		break;
	case LANXUN_SET_OPEN:
		ret = lanxun_open();
		break;
	case  LANXUN_GET_STATUS:
		retvalue=(int *)rbuf;
		(*retvalue)=0;
		return 0;
	default:
		break;
	}
	return ret;
}


