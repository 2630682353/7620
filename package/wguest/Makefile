include $(TOPDIR)/rules.mk

PKG_NAME:=wguest
PKG_VERSION:=0.1
PKG_RELEASE:=3
PKG_BUILD_DIR:=$(BUILD_DIR)/$(PKG_NAME)

INIT=wguest.init
KO=wguest.ko

include $(INCLUDE_DIR)/package.mk
include $(INCLUDE_DIR)/kernel.mk

define Package/wguest
  SECTION:=net
  CATEGORY:=Network
  TITLE:=wguest
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

define Package/wguest/install
	$(INSTALL_DIR) $(1)/etc/init.d
	$(INSTALL_DIR) $(1)/$(MODULES_SUBDIR)
	$(CP) $(PKG_BUILD_DIR)/$(KO) $(1)/$(MODULES_SUBDIR)
	$(CP) files/$(INIT) $(1)/etc/init.d/
endef

$(eval $(call BuildPackage,wguest))
