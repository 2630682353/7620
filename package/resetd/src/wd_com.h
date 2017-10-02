#ifndef __WD_COM_H__
#define __WD_COM_H__


#define WD_DBG(fmt,args...) do { \
	wd_dbg("%s[%d]:"fmt, __FUNCTION__, __LINE__, ##args); \
} while(0)

extern void wd_dbg(const char *fmt, ...);
extern int resetd_init(int flag);
extern int resetd_loop(int rts);
extern int wps_init(void);
extern int wps_run(void);
extern int wps_loop(void);

#endif
