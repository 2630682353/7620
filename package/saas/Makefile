#
# Copyright (C) 2009-2011 OpenWrt.org
#
# This is free software, licensed under the GNU General Public License v2.
# See /LICENSE for more information.
#

include $(TOPDIR)/rules.mk

PKG_NAME:=saas
PKG_VERSION:=1.0.0
PKG_RELEASE:=1
PKG_BUILD_DIR:=$(BUILD_DIR)/$(PKG_NAME)

include $(INCLUDE_DIR)/package.mk

define Package/$(PKG_NAME)
  SECTION:=base
  CATEGORY:=Base system
  TITLE:=saas
endef

define Build/Compile
	rm -rf $(PKG_BUILD_DIR)
	mkdir -p $(PKG_BUILD_DIR)
	$(CP) ./wiair/* $(PKG_BUILD_DIR)/
	$(MAKE) -C $(PKG_BUILD_DIR) \
		$(TARGET_CONFIGURE_OPTS) \
		LDFLAGS="$(TARGET_LDFLAGS)" \
		dep all
endef

define Package/$(PKG_NAME)/install
	$(INSTALL_DIR) $(1)/bin
endef

$(eval $(call BuildPackage,$(PKG_NAME)))
