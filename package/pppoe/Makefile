#
# Copyright (C) 2006-2012 OpenWrt.org
#
# This is free software, licensed under the GNU General Public License v2.
# See /LICENSE for more information.
#

include $(TOPDIR)/rules.mk

PKG_NAME:=pppoe
PKG_VERSION:=3.6
PKG_RELEASE:=1

PAPS=pap-secrets
OPTIONS=pppoe-server-options
APP=pppoe-server

include $(INCLUDE_DIR)/package.mk

define Package/pppoe
  SECTION:=net
  CATEGORY:=Network
  TITLE:=pppoe
  DEPENDS:=ppp
endef

define Build/Prepare
	mkdir -p $(PKG_BUILD_DIR)
endef

define Build/Compile
	rm -rf $(PKG_BUILD_DIR)
	mkdir -p $(PKG_BUILD_DIR)
	$(CP) -a ./src/* $(PKG_BUILD_DIR)/
	$(MAKE) -C $(PKG_BUILD_DIR) \
		$(TARGET_CONFIGURE_OPTS) \
		CFLAGS="$(TARGET_CFLAGS)" \
		LDFLAGS="$(TARGET_LDFLAGS)" \
		all
endef


define Package/pppoe/install
	$(INSTALL_DIR) $(1)/bin
	$(INSTALL_DIR) $(1)/etc/ppp/
	$(CP) $(PKG_BUILD_DIR)/$(APP) $(1)/bin/
	$(CP) $(PKG_BUILD_DIR)/$(OPTIONS) $(1)/etc/ppp/
	$(CP) $(PKG_BUILD_DIR)/$(PAPS) $(1)/etc/ppp/
endef

$(eval $(call BuildPackage,pppoe))
