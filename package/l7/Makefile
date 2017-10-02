
include $(TOPDIR)/rules.mk

PKG_NAME:=l7
PKG_VERSION:=1.0
PKG_RELEASE:=1
include $(INCLUDE_DIR)/package.mk
include $(INCLUDE_DIR)/kernel.mk

KO=l7.ko
ifeq ($(BRIDGEMODE), 1)
        KO=l7_br.ko
endif

define Package/l7
  SECTION:=net
  CATEGORY:=Network
  TITLE:=l7
  DEPENDS:=libshare
endef

TARGET_CFLAGS += $(FPIC)

define Build/Compile
	rm -rf $(PKG_BUILD_DIR)
	mkdir -p $(PKG_BUILD_DIR)
	$(MAKE) -C src \
		$(TARGET_CONFIGURE_OPTS) \
		CFLAGS="$(TARGET_CFLAGS)" \
		LDFLAGS="$(TARGET_LDFLAGS)" \
		all
endef

define Package/l7/install
	$(INSTALL_DIR) $(1)/etc/init.d
	$(INSTALL_BIN) ./files/l7.init $(1)/etc/init.d/l7
	$(INSTALL_DIR) $(1)/bin/
	cp -rf ./src/l7_load $(1)/bin/
	$(INSTALL_DIR) $(1)/$(MODULES_SUBDIR)
	cp -rf ./src/$(KO) $(1)/$(MODULES_SUBDIR)/l7.ko
	cd rconf && chmod 777 rconf.sh && ./rconf.sh $(1)
endef

$(eval $(call BuildPackage,l7))
