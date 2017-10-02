# All rights reserved.
#
# This is free software, licensed under the GNU General Public License v2.
# See /LICENSE for more information.

include $(TOPDIR)/rules.mk
include $(INCLUDE_DIR)/kernel.mk

PKG_NAME:=mt7610e
PKG_VERSION:=wifi_v3006_dpa_20130613

PKG_SOURCE:=$(PKG_NAME)_$(PKG_VERSION).tar.bz2
PKG_SOURCE_URL:=http://localhost/ \
    http://172.26.66.224/mtk_drivers/
PKG_BUILD_DIR:=$(KERNEL_BUILD_DIR)/$(PKG_NAME)_$(PKG_VERSION)

PKG_KCONFIG:= \
	RALINK_MT7620 MT7610_AP MT7610_AP_V24_DATA_STRUCTURE MT7610_AP_LED MT7610_AP_WSC MT7610_AP_WSC_V2 \
	MT7610_AP_LLTD MT7610_AP_WDS MT7610_AP_MBSS NEW_MBSSID_MODE MT7610_AP_APCLI MT7610_AP_MAC_REPEATER \
	MT7610_AP_IGMP_SNOOP MT7610_AP_DFS MT7610_AP_CARRIER M7610_CON_WPS_SUPPORT MT7610_AP_80211N_DRAFT3 \
	MT7610_AP_ATE MT7610_AP_QA MT7610_AP_FLASH MT7610_AP_BIG_ENDIAN MT7610_AP_TSSI_COMPENSATION \
	RTMP_TEMPERATURE_COMPENSATION MT7610_AP_SINGLE_SKU

PKG_CONFIG_DEPENDS:=$(foreach c, $(PKG_KCONFIG),$(if $(CONFIG_$c),CONFIG_$(c)))

include $(INCLUDE_DIR)/package.mk

#define KernelPackage/mt7610e
  #CATEGORY:=Ralink Properties
  #TITLE:=Ralink mt7610e wifi AP driver
  #FILES:=$(PKG_BUILD_DIR)/mt7610.ko
  #AUTOLOAD:=$(call AutoLoad,91,mt7610)
  #SUBMENU:=Drivers
  #MENU:=1
#endef

#define KernelPackage/mt7610e/config
	#source "$(SOURCE)/config.in"
#endef

#define Build/Compile
	#$(MAKE) -C "$(LINUX_DIR)" \
		#CROSS_COMPILE="$(TARGET_CROSS)" \
		#ARCH="$(LINUX_KARCH)" \
		#SUBDIRS="$(PKG_BUILD_DIR)/" \
		#$(foreach c, $(PKG_KCONFIG),$(if $(CONFIG_$c),CONFIG_$(c)=y))\
		#modules
#endef

#define KernelPackage/mt7610e/install
	#$(INSTALL_DIR) $(1)/lib/wifi/
	#$(INSTALL_BIN) ./files/mt7610.sh $(1)/lib/wifi/
	#$(INSTALL_DIR) $(1)/etc/Wireless/mt7610/
	#$(INSTALL_BIN) ./files/mt7610.dat $(1)/etc/Wireless/mt7610/
	#$(INSTALL_BIN) ./files/mt7610.dat $(1)/etc/Wireless/mt7610/mt7610.dat.orig
	#-$(INSTALL_BIN) ./files/mt7610.eeprom.bin $(1)/etc/Wireless/mt7610/
#endef

$(eval $(call KernelPackage,mt7610e))
