include $(TOPDIR)/rules.mk

PKG_NAME:=glvpn
PKG_VERSION:=0.0.1
PKG_RELEASE:=3
PKG_BUILD_DIR:=$(BUILD_DIR)/$(PKG_NAME)

include $(INCLUDE_DIR)/package.mk
include $(INCLUDE_DIR)/kernel.mk

define Package/$(PKG_NAME)
	SECTION:=net
	CATEGORY:=Network
	TITLE:=glvpn
	URL:=http://www.wiair.com/
	DEPENDS:=ppp mu libshare +libpthread
endef

define Build/Compile
	rm -rf $(PKG_BUILD_DIR)
	mkdir -p $(PKG_BUILD_DIR)
	$(CP) ./src/* $(PKG_BUILD_DIR)/
	$(MAKE) -C $(PKG_BUILD_DIR) \
		$(TARGET_CONFIGURE_OPTS) \
		KDIR="$(LINUX_DIR)" \
		LDFLAGS="$(TARGET_LDFLAGS)" \
		ARCH="$(LINUX_KARCH)"
endef

define Package/$(PKG_NAME)/install
	$(INSTALL_DIR) $(1)/etc/config/
	$(INSTALL_DIR) $(1)/bin/
	$(INSTALL_DIR) $(1)/etc/init.d/
	$(INSTALL_BIN) ./files/vpnconfig $(1)/etc/config/
	$(INSTALL_BIN) ./files/gl.init $(1)/etc/init.d/
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/glclient/glclient $(1)/bin/
	$(INSTALL_DIR) $(1)/$(MODULES_SUBDIR)
	#$(INSTALL_BIN) $(PKG_BUILD_DIR)/kernel/driver/pptp.ko $(1)/$(MODULES_SUBDIR)
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/kernel/igd_pptp/vpn.ko $(1)/$(MODULES_SUBDIR)
endef

$(eval $(call BuildPackage,$(PKG_NAME)))

