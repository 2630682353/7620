######################################################################
# Copyright (C) 2016. Shanghai Feixun Communication Co.,Ltd.
#
# DISCREPTION   : SOP比价功能
# AUTHOR        : maoyuan.li <maoyuan.li@phicomm.com.cn>
# CREATED DATE  : 2016-05
# MODIFIED DATE : 2016-06-02
######################################################################

include $(TOPDIR)/rules.mk

PKG_NAME:=jsprocess
PKG_FIXUP:=autoreconf
PKG_VERSION:=1.0
PKG_RELEASE:=1

KO=js.ko

PKG_BUILD_DIR:=$(BUILD_DIR)/$(PKG_NAME)-$(PKG_VERSION)/

include $(INCLUDE_DIR)/package.mk
include $(INCLUDE_DIR)/kernel.mk

define Package/jsprocess
  SECTION:=net
  CATEGORY:=Network
  TITLE:=js process to compare prices
  DEPENDS:=+libpthread +libuci
endef

TARGET_CFLAGS += $(FPIC)

define Package/jsprocess/description
	路由器比价功能 
endef

define Build/Compile
	rm -rf $(PKG_BUILD_DIR)
	mkdir -p $(PKG_BUILD_DIR)
	$(CP) ./src/* $(PKG_BUILD_DIR)/
	$(MAKE) -C $(PKG_BUILD_DIR) \
		$(TARGET_CONFIGURE_OPTS) \
		CFLAGS="$(TARGET_CFLAGS)" \
		LDFLAGS="$(TARGET_LDFLAGS)" \
		KDIR="$(LINUX_DIR)" \
		ARCH="$(LINUX_KARCH)"
endef

define Package/jsprocess/install
	$(INSTALL_DIR) $(1)/usr/sbin
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/$(PKG_NAME) $(1)/usr/sbin/
	$(INSTALL_DIR) $(1)/etc/init.d
	$(INSTALL_BIN) ./files/$(PKG_NAME) $(1)/etc/init.d/$(PKG_NAME)
	$(INSTALL_DIR) $(1)/$(MODULES_SUBDIR)
	$(CP) $(PKG_BUILD_DIR)/$(KO) $(1)/$(MODULES_SUBDIR)
endef

$(eval $(call BuildPackage,jsprocess))