#
# Copyright (C) 2009-2011 OpenWrt.org
#
# This is free software, licensed under the GNU General Public License v2.
# See /LICENSE for more information.
#

include $(TOPDIR)/rules.mk

PKG_NAME:=libshare
PKG_VERSION:=1.0.0
PKG_RELEASE:=1
PKG_BUILD_DIR:=$(BUILD_DIR)/$(PKG_NAME)

include $(INCLUDE_DIR)/package.mk
include $(INCLUDE_DIR)/kernel.mk

KERNEL_DIR:=$(LINUX_DIR)/include/linux/netfilter_ipv4/nf_igd

define Package/$(PKG_NAME)
  SECTION:=libs
  CATEGORY:=Libraries
  TITLE:=libshare
  DEPENDS:=+libuci +libubox
endef

define Build/Compile
	rm -rf $(PKG_BUILD_DIR)
	$(CP) $(KERNEL_DIR)/igd_share.h ./src/include
	$(CP) $(KERNEL_DIR)/product.h ./src/include
	mkdir -p $(PKG_BUILD_DIR)
	$(CP) ./src/* $(PKG_BUILD_DIR)/
	$(MAKE) -C $(PKG_BUILD_DIR) \
		$(TARGET_CONFIGURE_OPTS) \
		LDFLAGS="$(TARGET_LDFLAGS)" \
		all
endef

define Build/InstallDev
	$(INSTALL_DIR) $(1)/usr/lib  $(1)/usr/include
	$(CP) $(PKG_BUILD_DIR)/include/* $(1)/usr/include/
	$(CP) $(PKG_BUILD_DIR)/libshare.so $(1)/usr/lib/
endef

define Package/$(PKG_NAME)/install
	$(INSTALL_DIR) $(1)/lib
	$(CP) $(PKG_BUILD_DIR)/libshare.so $(1)/lib/
endef

$(eval $(call BuildPackage,$(PKG_NAME)))
