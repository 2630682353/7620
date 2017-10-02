# All rights reserved.
#
# This is free software, licensed under the GNU General Public License v2.
# See /LICENSE for more information.

include $(TOPDIR)/rules.mk
include $(INCLUDE_DIR)/kernel.mk

PKG_NAME:=mt7612e
PKG_VERSION:=mt7602e_LinuxAP_3.0.2.9_iPA_20140103

PKG_SOURCE:=$(PKG_NAME)_$(PKG_VERSION).tar.bz2
PKG_SOURCE_URL:=http://localhost/ \
    http://172.26.66.224/mtk_drivers/
PKG_BUILD_DIR:=$(KERNEL_BUILD_DIR)/$(PKG_NAME)_$(PKG_VERSION)

PKG_KCONFIG:= \
	RALINK_MT7612E RALINK_MT7620 RLT_AP_SUPPORT WDS_SUPPORT MBSS_SUPPORT ENHANCE_NEW_MBSSID_MODE \
	APCLI_SUPPORT DFS_SUPPORT LLTD_SUPPORT NINTENDO_AP COC_SUPPORT \
	DELAYED_TCP_ACK_SUPPORT RT_FIRST_CARD_EEPROM \
	RT_FIRST_IF_RF_OFFSET RT_SECOND_IF_RF_OFFSET \
	RT_FIRST_CARD RT_SECOND_CARD \
	RT_FIRST_CARD_EEPROM FIRST_IF_EEPROM_PROM FIRST_IF_EEPROM_EFUSE FIRST_IF_EEPROM_FLASH \
	SECOND_IF_EEPROM_PROM SECOND_IF_EEPROM_EFUSE SECOND_IF_EEPROM_FLASH \
	WIFI_BASIC_FUNC WSC_INCLUDED WSC_V2_SUPPORT WSC_NFC_SUPPORT DOT11N_DRAFT3 DOT11_VHT_AC \
	DOT11W_PMF_SUPPORT TXBF_SUPPORT LLTD_SUPPORT QOS_DLS_SUPPORT \
	CARRIER_DETECTION_SUPPORT IGMP_SNOOP_SUPPORT BLOCK_NET_IF \
	TXBF_SUPPORT RATE_ADAPTION NEW_RATE_ADAPT_SUPPORT AGS_SUPPORT \
	IDS_SUPPORT WIFI_WORK_QUEUE WIFI_SKB_RECYCLE RTMP_FLASH_SUPPORT \
	LED_CONTROL_SUPPORT HW_ANTENNA_DIVERSITY ATE_SUPPORT \
	RT2860V2_AP_V24_DATA_STRUCTURE RT2860V2_AP_32B_DESC MEMORY_OPTIMIZATION \
	RTMP_INTERNAL_TX_ALC RTMP_TEMPERATURE_CALIBRATION HOTSPOT \
	SINGLE_SKU_V2 RLT_MAC RLT_BBP RLT_RF RTMP_MAC RTMP_BBP RTMP_RF \
	RTMP_PCI_SUPPORT RTMP_USB_SUPPORT RTMP_RBUS_SUPPORT DISABLE_EDCCA 

PKG_CONFIG_DEPENDS:=$(foreach c, $(PKG_KCONFIG),$(if $(CONFIG_$c),CONFIG_$(c)))

include $(INCLUDE_DIR)/package.mk

define KernelPackage/mt76x2e
  SECTION:=kernel
  CATEGORY:=Ralink Properties
  TITLE:=Ralink mt76x2e wifi AP driver
  FILES:=$(PKG_BUILD_DIR)/rlt_wifi_ap/mt76x2e.ko
  AUTOLOAD:=$(call AutoLoad,91,mt76x2e)
  SUBMENU:=Drivers
  MENU:=1
endef

define KernelPackage/mt76x2e/config
	source "$(SOURCE)/Config-mt76x2e.in"
endef

define Build/Compile
	$(MAKE) -C "$(LINUX_DIR)" \
		CROSS_COMPILE="$(TARGET_CROSS)" \
		ARCH="$(LINUX_KARCH)" \
		SUBDIRS="$(PKG_BUILD_DIR)/rlt_wifi_ap/" \
		$(foreach c, $(PKG_KCONFIG),$(if $(CONFIG_$c),CONFIG_$(c)=$(CONFIG_$c)))\
		modules
endef

define KernelPackage/mt76x2e/install
		$(INSTALL_DIR) $(1)/lib/wifi
	-if [ "$(CONFIG_RT_FIRST_CARD)" == "7612" ]; then \
		$(INSTALL_DIR) $(1)/etc/Wireless/mt7602/ ; \
		$(INSTALL_BIN) ./files/mt7602.sh $(1)/lib/wifi/ ; \
		$(INSTALL_BIN) ./files/mt7602.dat $(1)/etc/Wireless/mt7602/ ; \
		$(INSTALL_BIN) ./files/mt7602.dat $(1)/etc/Wireless/mt7602/mt7602.dat.orig ; \
		$(INSTALL_BIN) ./files/mt7602.eeprom.bin $(1)/etc/Wireless/mt7602/ ; \
	fi
	-if [ "$(CONFIG_RT_SECOND_CARD)" == "7612" ]; then \
		$(INSTALL_DIR) $(1)/etc/Wireless/mt7612/ ; \
		$(INSTALL_BIN) ./files/mt7622.sh $(1)/lib/wifi/ ; \
		$(INSTALL_BIN) ./files/mt7612.dat $(1)/etc/Wireless/mt7612/ ; \
		$(INSTALL_BIN) ./files/mt7612.dat $(1)/etc/Wireless/mt7612/mt7612.dat.orig ; \
		$(INSTALL_BIN) ./files/mt7612.eeprom.bin $(1)/etc/Wireless/mt7612/ ; \
	fi
endef

$(eval $(call KernelPackage,mt76x2e))
