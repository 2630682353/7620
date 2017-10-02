#
# Copyright (C) 2006-2012 OpenWrt.org
#
# This is free software, licensed under the GNU General Public License v2.
# See /LICENSE for more information.
#

include $(TOPDIR)/rules.mk

PKG_NAME:=ccapp_chuyun
PKG_VERSION:=1.0
PKG_RELEASE:=1


include $(INCLUDE_DIR)/package.mk
include $(INCLUDE_DIR)/kernel.mk

define Package/ccapp_chuyun
  SECTION:=net
  CATEGORY:=Network
  TITLE:=ccapp_chuyun
  DEPENDS:=libshare +libpthread libjson-c mu
endef


TARGET_CFLAGS += $(FPIC)

define Build/Compile
endef

define Package/ccapp_chuyun/install
	$(INSTALL_DIR) $(1)/etc/init.d
	cp -rf ./ccapp $(1)/etc/init.d/

	$(INSTALL_DIR) $(1)/etc/app_conf/ccapp/
	cp -rf ./files/ccapp/* 	 	$(1)/etc/app_conf/ccapp/

	$(INSTALL_DIR) $(1)/etc/nginx/
	cp -rf ./files/ccapp/nginx/*   $(1)/etc/nginx/
	cp -rf ./files/info.ccapp.conf   $(1)/etc/nginx/

	$(INSTALL_DIR) $(1)/etc/app_conf/
	cp -rf ./files/script   $(1)/etc/app_conf/
	cp -rf ./root $(1)/etc/app_conf/
	
	$(INSTALL_DIR) $(1)/usr/lib/lua/
	cp -rf ./files/lua/*   $(1)/usr/lib/lua/

	$(INSTALL_DIR) $(1)/usr/bin/
	cp -rf ./files/curl_file/curl	$(1)/usr/bin/
	chmod 777  $(1)/usr/bin/curl

	$(INSTALL_DIR) $(1)/usr/lib/
	cp -rf ./files/curl_file/libcurl.so.4.3.0   $(1)/usr/lib/
	ln -s libcurl.so.4.3.0 $(1)/usr/lib/libcurl.so.4

	cp -rf ./files/curl_file/libpolarssl.so.1.2.9   $(1)/usr/lib/
	ln -s libpolarssl.so.1.2.9   $(1)/usr/lib/libpolarssl.so.3
	ln -s libpolarssl.so.3   $(1)/usr/lib/libpolarssl.so
	
	#ngix
	$(CP) -rf ./nginx/* $(1)/
	#pdnsd
	$(CP) -rf ./pdnsd/* $(1)/

	#libopenssl
	$(CP) -rf ./libopenssl/* $(1)/
endef

$(eval $(call BuildPackage,ccapp_chuyun))
