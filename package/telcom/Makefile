include $(TOPDIR)/rules.mk

PKG_NAME:=telcom
PKG_VERSION:=1.0.0
PKG_RELEASE:=1
APP=telcom
PKG_BUILD_DIR:=$(BUILD_DIR)/$(PKG_NAME)-$(PKG_VERSION)

include $(INCLUDE_DIR)/package.mk

define Package/$(PKG_NAME)
 SECTION:=net
 CATEGORY:=Network
 TITLE:=telcom
 DEPENDS:=jansson libshare 
endef

define Build/Prepare
        mkdir -p $(PKG_BUILD_DIR)
endef

define Build/Compile
	rm -rf $(PKG_BUILD_DIR)
	mkdir -p $(PKG_BUILD_DIR)
	$(CP) ./src/* $(PKG_BUILD_DIR)/
	$(MAKE) -C $(PKG_BUILD_DIR) \
		$(TARGET_CONFIGURE_OPTS) \
		LDFLAGS="$(TARGET_LDFLAGS)" \
		all
endef

define Package/$(PKG_NAME)/install
	$(INSTALL_DIR) $(1)/bin
	$(INSTALL_DIR) $(1)/etc/init.d

	$(CP) ./telcom $(1)/etc/init.d/
	chmod 777 $(1)/etc/init.d/telcom

#	$(INSTALL_BIN) $(PKG_BUILD_DIR)/telcoms $(1)/bin/
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/telcomc $(1)/bin/
#	$(STRIP) $(1)/bin/telcoms
	$(STRIP) $(1)/bin/telcomc
endef

$(eval $(call BuildPackage,$(PKG_NAME)))
