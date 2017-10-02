#
# Copyright (C) 2010-2013 hua.shao@mediatek.com
#
# Ralink Property Software.
#

include $(TOPDIR)/rules.mk

PKG_NAME:=resetd
PKG_RELEASE:=1

PKG_BUILD_DIR:=$(BUILD_DIR)/$(PKG_NAME)
PKG_KCONFIG:=RALINK_MT7620 RALINK_MT7621
PKG_CONFIG_DEPENDS:=$(foreach c, $(PKG_KCONFIG),$(if $(CONFIG_$c),CONFIG_$(c)))


include $(INCLUDE_DIR)/package.mk
include $(INCLUDE_DIR)/kernel.mk

define Package/$(PKG_NAME)
  SECTION:=base
  CATEGORY:=Base system
  TITLE:=resetd
  DEPENDS:= +kmod-hw_wdg libshare
endef

define Package/$(PKG_NAME)/description
  resetd
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

TARGET_CFLAGS += \
	$(foreach c, $(PKG_KCONFIG),$(if $(CONFIG_$c),-DCONFIG_$(c)=$(CONFIG_$c))) \
	-I$(LINUX_DIR)/inclulde

define Package/$(PKG_NAME)/install
	$(INSTALL_DIR) $(1)/usr/bin
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/resetd $(1)/usr/bin
	$(INSTALL_DIR) $(1)/etc/init.d
	$(INSTALL_BIN) ./files/resetd.init $(1)/etc/init.d/resetd
endef


$(eval $(call BuildPackage,$(PKG_NAME)))

