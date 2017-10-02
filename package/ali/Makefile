#
# Copyright (C) 2006-2012 OpenWrt.org
#
# This is free software, licensed under the GNU General Public License v2.
# See /LICENSE for more information.
#

include $(TOPDIR)/rules.mk

PKG_NAME:=ali
PKG_VERSION:=1.0
PKG_RELEASE:=1


include $(INCLUDE_DIR)/package.mk
include $(INCLUDE_DIR)/kernel.mk

define Package/ali
  SECTION:=net
  CATEGORY:=Network
  TITLE:=ali
  DEPENDS:=libshare +libpthread libjson-c mu
endef


TARGET_CFLAGS += $(FPIC)

define Build/Compile
	rm -rf $(PKG_BUILD_DIR)
	mkdir -p $(PKG_BUILD_DIR)
	$(CP) ./src/* $(PKG_BUILD_DIR)/
	$(MAKE) -C $(PKG_BUILD_DIR) \
		$(TARGET_CONFIGURE_OPTS) \
		CFLAGS="$(TARGET_CFLAGS)" \
		LDFLAGS="$(TARGET_LDFLAGS)" \
		dep all
endef


define Package/ali/install
	$(INSTALL_DIR) $(1)/bin
	$(INSTALL_DIR) $(1)/$(MODULES_SUBDIR)
	$(INSTALL_DIR) $(1)/usr/lib
	$(INSTALL_DIR) $(1)/etc/init.d
	$(INSTALL_DIR) $(1)/etc/config
	$(CP) ./files/module/* $(1)/$(MODULES_SUBDIR)
	$(CP) ./files/lib/* $(1)/usr/lib/
	$(CP) ./files/ali_cloud.init $(1)/etc/init.d/
	$(CP) ./files/aliconf $(1)/etc/config/
	$(CP) -rf ./files/htdocs $(1)/bin/
	$(CP) $(PKG_BUILD_DIR)/ali_cloud $(1)/bin/
	$(STRIP) $(1)/bin/ali_cloud
endef

$(eval $(call BuildPackage,ali))
