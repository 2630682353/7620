
include $(TOPDIR)/rules.mk

PKG_NAME:=sqos
PKG_VERSION:=1.0
PKG_RELEASE:=1
include $(INCLUDE_DIR)/package.mk
include $(INCLUDE_DIR)/kernel.mk

define Package/sqos
  SECTION:=net
  CATEGORY:=Network
  TITLE:=sqos
endef

TARGET_CFLAGS += $(FPIC)

KO=sqos.ko
ifeq ($(BRIDGEMODE),1)
	KO=sqos_br.ko
endif

define Build/Compile
	rm -rf $(PKG_BUILD_DIR)
	mkdir -p $(PKG_BUILD_DIR)
endef

define Package/sqos/install
	$(INSTALL_DIR) $(1)/etc/init.d
	$(INSTALL_BIN) ./files/sqos.init $(1)/etc/init.d/sqos
	$(INSTALL_DIR) $(1)/bin/
	$(INSTALL_DIR) $(1)/$(MODULES_SUBDIR)
	$(CP) src/qos.sh $(1)/bin/
	$(CP) src/$(KO) $(1)/$(MODULES_SUBDIR)/sqos.ko
endef

$(eval $(call BuildPackage,sqos))
