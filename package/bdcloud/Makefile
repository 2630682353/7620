#
# Copyright (C) 2006-2012 OpenWrt.org
#
# This is free software, licensed under the GNU General Public License v2.
# See /LICENSE for more information.
#

include $(TOPDIR)/rules.mk

PKG_NAME:=bdcloud
PKG_VERSION:=1.0.0
PKG_RELEASE:=1
APP=bdcloud
PKG_BUILD_DIR:=$(BUILD_DIR)/$(PKG_NAME)-$(PKG_VERSION)

include $(INCLUDE_DIR)/package.mk

define Package/bdcloud
  SECTION:=net
  CATEGORY:=Network
  TITLE:=bdcloud
  DEPENDS:=
endef

define Build/Prepare
	
endef

define Build/Compile

endef

define Package/$(PKG_NAME)/install
	chmod 777 bdcloud
	tar -zcvf bdcloud.tar.gz cloudPlugin.ipk bdcloud
endef

$(eval $(call BuildPackage,bdcloud))
