include $(TOPDIR)/rules.mk

PKG_NAME:=sn_blink
PKG_VERSION:=1.0.0
PKG_RELEASE:=1

include $(INCLUDE_DIR)/package.mk

define Package/$(PKG_NAME)
  SECTION:=net
  CATEGORY:=Network
  TITLE:=sn_blink
  DEPENDS:=libshare libuci libubox mu
endef

define Build/Compile
	rm -rf $(PKG_BUILD_DIR)
	mkdir -p $(PKG_BUILD_DIR)
	$(CP) ./files/* $(PKG_BUILD_DIR)/
endef

define Package/$(PKG_NAME)/install
	$(INSTALL_DIR) $(1)/bin
	$(CP) $(PKG_BUILD_DIR)/sn_blink $(1)/bin/
	$(CP) $(PKG_BUILD_DIR)/sn_blink.sh $(1)/bin/
	$(STRIP) $(1)/bin/sn_blink
	$(INSTALL_DIR) $(1)/etc/init.d
	$(CP) ./files/sn_blink.init $(1)/etc/init.d/sn_blink

endef

$(eval $(call BuildPackage,$(PKG_NAME)))
