#include <stdio.h>
#include <stdlib.h>
#include "nlk_ipc.h"

#define TLCY_DBG printf

#define TLCY_INT(i) 

static void tlcy_usage(void)
{
	TLCY_DBG("usage:\n"
		"   1.tlcy gpio gpio_number on/off/flash/enable/disable\n"
		"     eg: tlcy gpio 11 on\n");
}

static int tlcy_gpio(int argc, char *argv[])
{
	int ret, gpio, act;

	if (argc < 5) {
		tlcy_usage();
		return -1;
	}
	gpio = atoi(argv[2]);
	if (!strcmp(argv[3], "on")) {
		act = LED_ACT_ON;
	} else if (!strcmp(argv[3], "off")) {
		act = LED_ACT_OFF;
	} else if (!strcmp(argv[3], "flash")) {
		act = LED_ACT_FLASH;
	} else if (!strcmp(argv[3], "enable")) {
		act = LED_ACT_ENABLE;
	} else if (!strcmp(argv[3], "disable")) {
		act = LED_ACT_DISABLE;
	} else {
		tlcy_usage();
		return -1;
	}
	ret = gpio_act(gpio, act, atoi(argv[4]));
	TLCY_DBG("ret:%d, gpio:%d, act:%d\n",
		ret, gpio, act);
	return 0;
}

int main(int argc, char *argv[])
{
	if (argc < 2) {
		tlcy_usage();
		return -1;
	}

	if (!strcmp(argv[1], "gpio")) {
		return tlcy_gpio(argc, argv);
	} else {
		tlcy_usage();
	}
	return 0;
}
