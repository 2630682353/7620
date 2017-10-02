#
# Copyright (C) 2006-2012 OpenWrt.org
#
# This is free software, licensed under the GNU General Public License v2.
# See /LICENSE for more information.
#

include $(TOPDIR)/rules.mk

PKG_NAME:=diskm
PKG_VERSION:=0.1
PKG_RELEASE:=3
APP=diskm
PKG_BUILD_DIR:=$(BUILD_DIR)/$(PKG_NAME)

include $(INCLUDE_DIR)/package.mk

define Package/diskm
  SECTION:=net
  CATEGORY:=Network
  TITLE:=diskm
  DEPENDS:=libuci libubox 
endef

define Build/Compile
	rm -rf $(PKG_BUILD_DIR)
	mkdir -p $(PKG_BUILD_DIR)
	$(CP) ./src/* $(PKG_BUILD_DIR)/
	$(MAKE) -C $(PKG_BUILD_DIR) \
		$(TARGET_CONFIGURE_OPTS) \
		LDFLAGS="$(TARGET_LDFLAGS)"
endef
define Build/InstallDev
	$(INSTALL_DIR) $(1)/usr/lib  $(1)/usr/include
	$(CP) $(PKG_BUILD_DIR)/disk_manager.h $(1)/usr/include/
	$(CP) $(PKG_BUILD_DIR)/dmodule.h $(1)/usr/include/
	$(CP) $(PKG_BUILD_DIR)/libdiskm.so $(1)/usr/lib/
endef
define Package/diskm/install
	$(INSTALL_DIR) $(1)/bin 
	$(CP) $(PKG_BUILD_DIR)/diskm $(1)/bin
	$(INSTALL_DIR) $(1)/etc/init.d
	$(INSTALL_BIN) ./files/disktag $(1)/etc/init.d/
	$(INSTALL_BIN) ./files/diskm.init $(1)/etc/init.d/
	$(INSTALL_DIR) $(1)/etc/config
	$(INSTALL_DATA) ./files/mntpara $(1)/etc/config
	$(INSTALL_DATA) ./files/mntservice $(1)/etc/config
	$(INSTALL_DATA) ./files/mnt_preffix $(1)/etc/
	$(CP) $(PKG_BUILD_DIR)/fileserv_send $(1)/bin/
	$(CP) $(PKG_BUILD_DIR)/filecp $(1)/bin/
	$(CP) $(PKG_BUILD_DIR)/stopcp $(1)/bin/
	$(INSTALL_DIR) $(1)/usr/sbin
	ln -s /bin/diskm $(1)/usr/sbin/disktriger	
	ln -s /bin/diskm $(1)/usr/sbin/umount2	
	$(INSTALL_DIR) $(1)/lib
	$(CP) $(PKG_BUILD_DIR)/libdiskm.so $(1)/lib/
endef

$(eval $(call BuildPackage,diskm))
