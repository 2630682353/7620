include $(TOPDIR)/rules.mk

PKG_NAME:=xl2tpd
PKG_VERSION:1.3
PKG_RELEASE:=0

include $(INCLUDE_DIR)/package.mk

define Package/xl2tpd
  SECTION:=net
  CATEGORY:=Network
  TITLE:=xl2tpd
  DEPENDS:=ppp
endef

define Build/Prepare
	mkdir -p $(PKG_BUILD_DIR)
endef

define Build/Compile
	rm -rf $(PKG_BUILD_DIR)
	mkdir -p $(PKG_BUILD_DIR)
	$(CP) -a ./src/* $(PKG_BUILD_DIR)/
	$(MAKE) -C $(PKG_BUILD_DIR) \
		$(TARGET_CONFIGURE_OPTS) \
		LDFLAGS="$(TARGET_LDFLAGS)" \
		all
endef


define Package/xl2tpd/install
	$(INSTALL_DIR) $(1)/bin
	$(INSTALL_DIR) $(1)/etc/config
	$(INSTALL_DIR) $(1)/etc/init.d
	$(CP) $(PKG_BUILD_DIR)/xl2tpd $(1)/bin/
	$(CP) ./files/xl2tpd.init $(1)/etc/init.d/
	$(CP) ./files/tunnel $(1)/etc/config/
endef

$(eval $(call BuildPackage,xl2tpd))
