#ifndef __HOST_TYPE_H__
#define __HOST_TYPE_H__

/*dhcp options*/
#define END_OP (0)
#define DIS_OPS (1<<0)
#define REQ_OPS (1<<1)
#define DIS_OP55 (1<<2)
#define REQ_OP55 (1<<3)

extern int read_vender(void);
extern unsigned short get_vender(unsigned char * mac);
extern int dbg_vender(void);
extern unsigned char get_host_os_type(struct host_dhcp_trait *dhcp);
extern char *get_vender_name(unsigned short id, char *hostname);
#endif
