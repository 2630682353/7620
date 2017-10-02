#
# Copyright (C) 2009-2011 OpenWrt.org
#
# This is free software, licensed under the GNU General Public License v2.
# See /LICENSE for more information.
#

include $(TOPDIR)/rules.mk

PKG_NAME:=mu
PKG_VERSION:=1.0.0
PKG_RELEASE:=1
PKG_BUILD_DIR:=$(BUILD_DIR)/$(PKG_NAME)

include $(INCLUDE_DIR)/package.mk

define Package/$(PKG_NAME)
  SECTION:=base
  CATEGORY:=Base system
  TITLE:=mu
  DEPENDS:=+libshare +libuci +libubox +iptables
endef

define Build/Prepare

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

define Build/InstallDev
	$(INSTALL_DIR) $(1)/usr/lib  $(1)/usr/include
	$(CP) $(PKG_BUILD_DIR)/include/* $(1)/usr/include/
endef

define Package/$(PKG_NAME)/install
	$(INSTALL_DIR) $(1)/bin
	$(INSTALL_DIR) $(1)/etc/init.d
	$(INSTALL_DIR) $(1)/etc/config
	$(CP) $(PKG_BUILD_DIR)/mu $(1)/bin/
	$(CP) $(PKG_BUILD_DIR)/wisp $(1)/bin/
	$(CP) $(PKG_BUILD_DIR)/netdetect $(1)/bin/
	$(STRIP) $(1)/bin/mu
	$(STRIP) $(1)/bin/wisp
	$(STRIP) $(1)/bin/netdetect
	$(CP) ./files/mu $(1)/etc/init.d
	$(CP) ./files/command_log.sh $(1)/bin/command_log.sh
	chmod 777 $(1)/bin/command_log.sh
	$(INSTALL_DATA) ./files/host $(1)/etc/config/
endef

$(eval $(call BuildPackage,$(PKG_NAME)))
