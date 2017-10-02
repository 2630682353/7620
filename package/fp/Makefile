#
# Copyright (C) 2006-2012 OpenWrt.org
#
# This is free software, licensed under the GNU General Public License v2.
# See /LICENSE for more information.
#

include $(TOPDIR)/rules.mk

PKG_NAME:=fp
PKG_VERSION:=0.1
PKG_RELEASE:=1
PKG_BUILD_DIR:=$(BUILD_DIR)/$(PKG_NAME)

KO=fp.ko
INIT=fp.init

ifeq ($(BRIDGEMODE), 1)
        KO=fp_br.ko
endif

include $(INCLUDE_DIR)/package.mk
include $(INCLUDE_DIR)/kernel.mk

define Package/fp
  SECTION:=net
  CATEGORY:=Network
  TITLE:=fast path
endef

TARGET_CFLAGS += $(FPIC)

define Build/Compile
	rm -rf $(PKG_BUILD_DIR)
	mkdir -p $(PKG_BUILD_DIR)
endef

define Package/fp/install
	$(INSTALL_DIR) $(1)/$(MODULES_SUBDIR)
	$(INSTALL_DIR) $(1)/etc/init.d
	$(CP) src/$(KO) $(1)/$(MODULES_SUBDIR)/fp.ko
	$(CP) ./files/$(INIT) $(1)/etc/init.d/
endef

$(eval $(call BuildPackage,fp))
