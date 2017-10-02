include $(TOPDIR)/rules.mk

PKG_NAME:=url_302
PKG_VERSION:=0.1
PKG_RELEASE:=1
PKG_BUILD_DIR:=$(BUILD_DIR)/$(PKG_NAME)

INIT=url_302.init
KO=url_302.ko

include $(INCLUDE_DIR)/package.mk
include $(INCLUDE_DIR)/kernel.mk

define Package/url_302
  SECTION:=net
  CATEGORY:=Network
  TITLE:=url_302
  DEPENDS:=libshare
endef

TARGET_CFLAGS += $(FPIC)

define Build/Compile
	rm -rf $(PKG_BUILD_DIR)
	mkdir -p $(PKG_BUILD_DIR)
	$(CP) ./src/* $(PKG_BUILD_DIR)/
	$(MAKE) -C $(PKG_BUILD_DIR) \
		$(TARGET_CONFIGURE_OPTS) \
		CFLAGS="$(TARGET_CFLAGS)" \
		LDFLAGS="$(TARGET_LDFLAGS)" \
		KDIR="$(LINUX_DIR)" \
		ARCH="$(LINUX_KARCH)"
endef

define Package/url_302/install
	$(INSTALL_DIR) $(1)/etc/init.d
	$(INSTALL_DIR) $(1)/$(MODULES_SUBDIR)
	$(CP) $(PKG_BUILD_DIR)/$(KO) $(1)/$(MODULES_SUBDIR)
	$(CP) ./files/$(INIT) $(1)/etc/init.d/$(INIT)
endef

$(eval $(call BuildPackage,url_302))
