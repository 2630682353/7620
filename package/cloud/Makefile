#
# Copyright (C) 2009-2011 OpenWrt.org
#
# This is free software, licensed under the GNU General Public License v2.
# See /LICENSE for more information.
#

include $(TOPDIR)/rules.mk

PKG_NAME:=cloud
PKG_VERSION:=1.0.0
PKG_RELEASE:=1
PKG_BUILD_DIR:=$(BUILD_DIR)/$(PKG_NAME)

include $(INCLUDE_DIR)/package.mk

define Package/$(PKG_NAME)
  SECTION:=base
  CATEGORY:=Base system
  TITLE:=cloud client and cloud config prase
  DEPENDS:=libshare libuci libubox mu ioos
endef

define Build/Compile
	rm -rf $(PKG_BUILD_DIR)
	mkdir -p $(PKG_BUILD_DIR)
	$(CP) ./src/* $(PKG_BUILD_DIR)/
	$(MAKE) -C $(PKG_BUILD_DIR) \
		$(TARGET_CONFIGURE_OPTS) \
		LDFLAGS="$(TARGET_LDFLAGS)" \
		dep all
endef

define Package/$(PKG_NAME)/install
	$(INSTALL_DIR) $(1)/bin
	$(CP) $(PKG_BUILD_DIR)/cloud_client $(1)/bin/
	$(STRIP) $(1)/bin/cloud_client
	$(CP) $(PKG_BUILD_DIR)/cloud_exchange $(1)/bin/
	$(STRIP) $(1)/bin/cloud_exchange
	$(CP) $(PKG_BUILD_DIR)/tspeed $(1)/bin/
	$(STRIP) $(1)/bin/tspeed
	$(CP) $(PKG_BUILD_DIR)/tlcy $(1)/bin/
	$(STRIP) $(1)/bin/tlcy
	$(INSTALL_DIR) $(1)/etc/init.d
	$(INSTALL_BIN) ./files/cloud.init $(1)/etc/init.d/cloud
	$(CP) ./files/up_ready.sh $(1)/bin/
	chmod 777 $(1)/bin/up_ready.sh
endef

$(eval $(call BuildPackage,$(PKG_NAME)))
