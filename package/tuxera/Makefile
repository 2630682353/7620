#
# Copyright (C) 2006-2012 OpenWrt.org
#
# This is free software, licensed under the GNU General Public License v2.
# See /LICENSE for more information.
#

include $(TOPDIR)/rules.mk

PKG_NAME:=tuxera
PKG_VERSION:=0.1
PKG_RELEASE:=3

PKG_LICENSE:=GPLv2 LGPLv2.1
PKG_LICENSE_FILES:=

INIT=tuxera.init
KO=tntfs.ko
PKG_MAINTAINER:=Felix Fietkau <nbd@openwrt.org>

include $(INCLUDE_DIR)/package.mk
include $(INCLUDE_DIR)/kernel.mk

define Package/tuxera
  SECTION:=net
  CATEGORY:=Network
  TITLE:=tuxera
  DEPENDS:=libshare
endef

TARGET_CFLAGS += $(FPIC)

define Build/Compile
	rm -rf $(PKG_BUILD_DIR)
	mkdir -p $(PKG_BUILD_DIR)
	$(CP) ./src/* $(PKG_BUILD_DIR)
endef

#define Build/InstallDev
#	$(INSTALL_DIR) $(1)/bin
#	$(INSTALL_DIR) $(1)/lib/modules/2.6.36
#endef

define Package/tuxera/install
	$(INSTALL_DIR) $(1)/etc/init.d
	$(INSTALL_DIR) $(1)/$(MODULES_SUBDIR)
	$(CP) $(PKG_BUILD_DIR)/$(KO) $(1)/$(MODULES_SUBDIR)
	$(CP) $(PKG_BUILD_DIR)/tfat.ko $(1)/$(MODULES_SUBDIR)
	$(CP) $(PKG_BUILD_DIR)/texfat.ko $(1)/$(MODULES_SUBDIR)
	$(CP) files/$(INIT) $(1)/etc/init.d/
endef

$(eval $(call BuildPackage,tuxera))
