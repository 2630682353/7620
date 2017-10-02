#
# Copyright (C) 2006-2012 OpenWrt.org
#
# This is free software, licensed under the GNU General Public License v2.
# See /LICENSE for more information.
#

include $(TOPDIR)/rules.mk

PKG_NAME:=dns
PKG_VERSION:=0.1
PKG_RELEASE:=1

PKG_LICENSE:=GPLv2 LGPLv2.1
PKG_LICENSE_FILES:=
PKG_MAINTAINER:=Felix Fietkau <nbd@openwrt.org>

include $(INCLUDE_DIR)/package.mk

define Package/dns
  SECTION:=net
  CATEGORY:=Network
  TITLE:=dns
  DEPENDS:=libshare mu
endef

TARGET_CFLAGS += $(FPIC)

define Build/Compile
	rm -rf $(PKG_BUILD_DIR)
	mkdir -p $(PKG_BUILD_DIR)
	$(CP) ./src/* $(PKG_BUILD_DIR)/
	$(MAKE) -C $(PKG_BUILD_DIR) \
		$(TARGET_CONFIGURE_OPTS) \
		LDFLAGS="$(TARGET_LDFLAGS)" \
		dep all
endef

define Package/dns/install
	$(INSTALL_DIR) $(1)/bin
	$(CP) $(PKG_BUILD_DIR)/igd_dns $(1)/bin/
endef

$(eval $(call BuildPackage,dns))
