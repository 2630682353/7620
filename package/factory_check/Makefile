include $(TOPDIR)/rules.mk

PKG_NAME:=factory_check
PKG_VERSION:=1.0.0
PKG_RELEASE:=1

include $(INCLUDE_DIR)/package.mk

define Package/$(PKG_NAME)
  SECTION:=net
  CATEGORY:=Network
  TITLE:=factory_check
  DEPENDS:=libshare libuci libubox mu
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

define Package/$(PKG_NAME)/install
	$(INSTALL_DIR) $(1)/bin
	$(CP) $(PKG_BUILD_DIR)/factory_check $(1)/bin/
	$(STRIP) $(1)/bin/factory_check
	$(INSTALL_DIR) $(1)/etc/init.d
	$(CP) ./files/factory_check.init $(1)/etc/init.d/factory_check

endef

$(eval $(call BuildPackage,$(PKG_NAME)))
