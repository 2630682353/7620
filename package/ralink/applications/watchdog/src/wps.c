#include <stdio.h>
#include "wd_com.h"

int wps_init(void)
{
return 0;

	WD_DBG("wps init\n");

	system("iwpriv ra0 set WscConfMode=0");
	system("iwpriv ra0 set WscConfMode=4");
	system("iwpriv ra0 set WscConfStatus=1");
	system("iwpriv ra0 set WscConfStatus=2");
	return 0;
}

int wps_run(void)
{
return 0;
	WD_DBG("wps start\n");

	system("iwpriv ra0 set WscMode=2");
	system("iwpriv ra0 set WscGetConf=1");
	return 0;
}
