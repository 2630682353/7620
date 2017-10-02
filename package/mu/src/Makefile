CFLAGS= -Wall -Iinclude -Os
SRCS=$(wildcard *.c)
OBJS=$(SRCS:.c=.o)

all: $(OBJS)
	$(CC) -o mu  main.o igd_wifi.o igd_interface.o igd_system.o igd_upnp.o igd_nat.o igd_lanxun.o \
	igd_url_safe.o igd_url_log.o igd_qos.o igd_lib.o igd_host.o igd_cloud.o host_os_type.o igd_ali.o igd_tunnel.o \
	igd_dnsmasq.o host_mac.o igd_advert.o igd_vpn.o igd_plug.o igd_log.o igd_route.o $(CFLAGS) $(LDFLAGS) $(GCC_DEFINE) -lshare -luci -lubox -lip4tc
	$(CC) -o wisp wisp.o igd_lib.o $(CFLAGS) $(LDFLAGS) -lshare -luci -lubox
	$(CC) -o netdetect netdetect.o igd_lib.c $(CFLAGS) $(LDFLAGS) -lshare -luci -lubox

sinclude .depfile
dep:
	$(CC) -MM $(CFLAGS) $(SRCS) > .depfile

%.o: %.c
	$(CC) $(CFLAGS) $(GCC_DEFINE) -c -o $@ $<

clean:
	rm -rf .depfile *.o *.so mu
