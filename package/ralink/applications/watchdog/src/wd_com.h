#ifndef __WD_COM_H__
#define __WD_COM_H__


#define WD_DBG(fmt,args...) do { \
	watchdog_dbg("%s[%d]:"fmt, __FUNCTION__, __LINE__, ##args); \
} while(0)

extern void watchdog_dbg(const char *fmt, ...);
extern int resetd_init(int flag);
extern int resetd_loop(void);
extern int wps_init(void);
extern int wps_run(void);

#endif
