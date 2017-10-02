#
# Copyright (C) 2006-2012 OpenWrt.org
#
# This is free software, licensed under the GNU General Public License v2.
# See /LICENSE for more information.
#

include $(TOPDIR)/rules.mk

PKG_NAME:=nas
PKG_VERSION:=1.0.0
PKG_RELEASE:=1
APP=nas
PKG_BUILD_DIR:=$(BUILD_DIR)/$(PKG_NAME)-$(PKG_VERSION)

include $(INCLUDE_DIR)/package.mk

define Package/nas
  SECTION:=net
  CATEGORY:=Network
  TITLE:=nas
  DEPENDS:= +libshare +libopenssl +libpthread  +libstdcpp +diskm
endef

define Build/Prepare
        mkdir -p $(PKG_BUILD_DIR)
endef

define Build/Compile
	rm -rf $(PKG_BUILD_DIR)
	mkdir -p $(PKG_BUILD_DIR)
	cp -rf *  $(PKG_BUILD_DIR)/
	$(MAKE) -C $(PKG_BUILD_DIR)/KeySnLicense  			\
		$(TARGET_CONFIGURE_OPTS) \
		LDFLAGS="$(TARGET_LDFLAGS)" 
	cp $(PKG_BUILD_DIR)/KeySnLicense/liblicence.so   $(PKG_BUILD_DIR)/nas_lib
	
	$(MAKE) -C $(PKG_BUILD_DIR)/NasSDK \
		$(TARGET_CONFIGURE_OPTS) \
		LDFLAGS="$(TARGET_LDFLAGS)" 
	cp $(PKG_BUILD_DIR)/NasSDK/libnassdk.so   $(PKG_BUILD_DIR)/nas_lib
	
	$(MAKE) -C $(PKG_BUILD_DIR)/src  			\
		$(TARGET_CONFIGURE_OPTS) \
		LDFLAGS="$(TARGET_LDFLAGS)" 
endef

define Package/$(PKG_NAME)/install
	$(INSTALL_DIR) $(1)/bin
	chmod 777  $(PKG_BUILD_DIR)/src/nas
	$(CP) $(PKG_BUILD_DIR)/src/nas $(1)/bin/
	$(STRIP) $(1)/bin/nas
	
	
	$(INSTALL_DIR) $(1)/lib
	
	$(CP) $(PKG_BUILD_DIR)/nas_lib/libnassdk.so $(1)/lib/
	$(STRIP) $(1)/lib/libnassdk.so
	chmod 777  $(1)/lib/libnassdk.so
	
	$(CP) $(PKG_BUILD_DIR)/nas_lib/libtxdevicesdk.so $(1)/lib/
	$(STRIP) $(1)/lib/libtxdevicesdk.so
	chmod 777 $(1)/lib/libtxdevicesdk.so
	
	$(CP) $(PKG_BUILD_DIR)/nas_lib/liblicence.so $(1)/lib/
	$(STRIP) $(1)/lib/liblicence.so
	chmod 777 $(1)/lib/liblicence.so
	
	$(INSTALL_DIR) $(1)/etc/nas_conf
	$(CP) $(PKG_BUILD_DIR)/key/ec_key.pem $(1)/etc/nas_conf/
	$(CP) $(PKG_BUILD_DIR)/key/server.pem $(1)/etc/nas_conf/
endef

$(eval $(call BuildPackage,nas))
