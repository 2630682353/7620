#
# Copyright (C) 2006-2012 OpenWrt.org
#
# This is free software, licensed under the GNU General Public License v2.
# See /LICENSE for more information.
#

include $(TOPDIR)/rules.mk
include ../../.config

PKG_NAME:=ioos
PKG_VERSION:=0.1
PKG_RELEASE:=3
APP=ioos
PKG_BUILD_DIR:=$(BUILD_DIR)/$(PKG_NAME)

ifeq ($(CONFIG_PACKAGE_diskm),y)
PKG_BUILD_DEPENDS:=diskm
endif

include $(INCLUDE_DIR)/package.mk

TARGET_CFLAGS:=-fPIC -Wall -Iinclude

ifeq ($(CONFIG_PACKAGE_diskm),y)
	TARGET_CFLAGS+=-DDISKM_SUPPORT
	TARGET_LDFLAGS+=-ldiskm
endif

define Package/ioos
  SECTION:=net
  CATEGORY:=Network
  TITLE:=ioos
  DEPENDS:=+libuci +libubox +libshare +zlib +mu
endef

define Build/Compile
	rm -rf $(PKG_BUILD_DIR)
	mkdir -p $(PKG_BUILD_DIR)
	$(CP) ./src/* $(PKG_BUILD_DIR)/
	$(MAKE) -C $(PKG_BUILD_DIR) \
		$(TARGET_CONFIGURE_OPTS) \
		LDFLAGS="$(TARGET_LDFLAGS)" \
		CFLAGS="$(TARGET_CFLAGS)"
endef

define Build/InstallDev
	$(INSTALL_DIR) $(1)/usr/include
	$(CP) $(PKG_BUILD_DIR)/image.h $(1)/usr/include/
endef

define Package/ioos/install
	$(INSTALL_DIR) $(1)/etc/init.d
	$(INSTALL_BIN) ./files/ioos.init $(1)/etc/init.d/ioos
	$(INSTALL_DIR) $(1)/etc/config
	$(INSTALL_DATA) ./files/qos_rule $(1)/etc/config
	$(INSTALL_DIR) $(1)/bin 
	$(CP) $(PKG_BUILD_DIR)/$(APP) $(1)/bin
	$(STRIP) $(1)/bin/$(APP)
	$(CP) $(PKG_BUILD_DIR)/setsysflag $(1)/bin
	$(STRIP) $(1)/bin/setsysflag
	$(CP) $(PKG_BUILD_DIR)/firmware $(1)/etc
endef

$(eval $(call BuildPackage,ioos))
