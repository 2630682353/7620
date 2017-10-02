# 
# Copyright (C) 2006-2013 OpenWrt.org
#
# This is free software, licensed under the GNU General Public License v2.
# See /LICENSE for more information.
#

include $(TOPDIR)/rules.mk

PKG_NAME:=libnvram
PKG_VERSION:=0.9.28
PKG_RELEASE:=1
PKG_BUILD_DIR:=$(BUILD_DIR)/$(PKG_NAME)-$(PKG_VERSION)

include $(INCLUDE_DIR)/package.mk
include $(INCLUDE_DIR)/kernel.mk
define Package/libnvram
  CATEGORY:=Ralink Properties
  SUBMENU:=Libraries
  TITLE:=libnvram
  DEPENDS:=+libz
endef

define Build/Prepare
	mkdir -p $(PKG_BUILD_DIR)
	$(CP) ./src/* $(PKG_BUILD_DIR)/
endef

TARGET_CFLAGS:= \
	-I$(LINUX_DIR)/include \
	-I$(LINUX_DIR)/linux/autoconf.h \
	-DUSE_HOSTCC
	

define Build/Compile
	$(MAKE) $(PKG_JOBS) -C $(PKG_BUILD_DIR) \
		$(TARGET_CONFIGURE_OPTS) \
		CFLAGS="$(TARGET_CFLAGS)" 
endef

define Build/InstallDev
	$(INSTALL_DIR) $(1)/usr/include
	$(CP)   ./src/nvram.h $(1)/usr/include/
	$(INSTALL_DIR) $(1)/usr/lib
	$(CP)	$(PKG_BUILD_DIR)/libnvram* $(1)/usr/lib/
endef

define Package/libnvram/install
	$(INSTALL_DIR) $(1)/usr/lib
	$(CP) $(PKG_BUILD_DIR)/libnvram*.so* $(1)/usr/lib/
endef

$(eval $(call BuildPackage,libnvram))
