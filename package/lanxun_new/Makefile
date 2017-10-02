#
# Copyright (C) 2006-2012 OpenWrt.org
#
# This is free software, licensed under the GNU General Public License v2.
# See /LICENSE for more information.
#

include $(TOPDIR)/rules.mk

PKG_NAME:=lanxun_new
PKG_VERSION:=1.0.0
PKG_RELEASE:=1
APP=lanxun_new
PKG_BUILD_DIR:=$(BUILD_DIR)/$(PKG_NAME)-$(PKG_VERSION)
VERSON = $(shell cat src/include/ver.h  |grep LANXUN_VER | cut -d ' ' -f 3 |sed 's/\"//g')
PLATFORM = $(shell cat src/include/ver.h  |grep LANXUN_PLATFORM | cut -d ' ' -f 3 |sed 's/\"//g')
include $(INCLUDE_DIR)/package.mk

define Package/lanxun_new
  SECTION:=net
  CATEGORY:=Network
  TITLE:=lanxun_new
  DEPENDS:=libshare
endef

define Build/Prepare
        mkdir -p $(PKG_BUILD_DIR)
endef

define Build/Compile
	rm -rf $(PKG_BUILD_DIR)
	mkdir -p $(PKG_BUILD_DIR)
	$(CP) ./src/* $(PKG_BUILD_DIR)/
	$(CP) ./lanxun_lib/* $(PKG_BUILD_DIR)/
	$(MAKE) -C $(PKG_BUILD_DIR) \
		$(TARGET_CONFIGURE_OPTS) \
		LDFLAGS="$(TARGET_LDFLAGS)" \
		all
endef

define Package/$(PKG_NAME)/install
	

	rm -rf __lanxun__plugin__dir__temp__
	rm -rf lanxun.tar.gz
	mkdir __lanxun__plugin__dir__temp__
	mkdir __lanxun__plugin__dir__temp__/lib
	mkdir __lanxun__plugin__dir__temp__/bin
	cp -rf lanxun_lib/libccapp.so __lanxun__plugin__dir__temp__/lib
	cp -rf lanxun_conf __lanxun__plugin__dir__temp__/
	cp init.sh __lanxun__plugin__dir__temp__/
	cp $(PKG_BUILD_DIR)/lanxun __lanxun__plugin__dir__temp__/bin
	chmod 777 __lanxun__plugin__dir__temp__/init.sh
	chmod 777 __lanxun__plugin__dir__temp__/bin/lanxun
	chmod 777 __lanxun__plugin__dir__temp__/lib/*
	$(STRIP) __lanxun__plugin__dir__temp__/bin/lanxun
	tar -zcvf lanxun_$(PLATFORM)_$(VERSON).tar.gz  -C __lanxun__plugin__dir__temp__ lib bin init.sh lanxun_conf
	rm -rf __lanxun__plugin__dir__temp__


endef

$(eval $(call BuildPackage,lanxun_new))
