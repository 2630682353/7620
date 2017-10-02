#
# Copyright (C) 2010-2013 hua.shao@mediatek.com
#
# Ralink Property Software.
#

include $(TOPDIR)/rules.mk

PKG_NAME:=nvram
PKG_RELEASE:=1

PKG_BUILD_DIR:=$(BUILD_DIR)/$(PKG_NAME)
PKG_KCONFIG:=RALINK_MT7620 RALINK_MT7621
PKG_CONFIG_DEPENDS:=$(foreach c, $(PKG_KCONFIG),$(if $(CONFIG_$c),CONFIG_$(c)))


include $(INCLUDE_DIR)/package.mk
include $(INCLUDE_DIR)/kernel.mk

define Package/nvram
  SECTION:=Ralink Properties
  CATEGORY:=Ralink Properties
  TITLE:=nvram tool
  SUBMENU:=Applications
  DEPENDS:=+libnvram
endef

define Package/nvram/description
  nvram tool
endef

define Build/Prepare
	mkdir -p $(PKG_BUILD_DIR)
	$(CP) ./src/* $(PKG_BUILD_DIR)/
endef

TARGET_CFLAGS += \
	-I$(LINUX_DIR)/arch/mips/include\
	-I$(LINUX_DIR)/drivers/char\
	-I$(LINUX_DIR)/include\
	$(foreach c, $(PKG_KCONFIG),$(if $(CONFIG_$c),-DCONFIG_$(c)))
MAKE_FLAGS += \
        CFLAGS="$(TARGET_CFLAGS)"
define Build/Configure
endef

define Package/nvram/install
	$(INSTALL_DIR) $(1)/usr/bin
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/nvramd $(1)/usr/bin
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/ralink_init $(1)/usr/bin
	$(CP) $(PKG_BUILD_DIR)/nvram_set $(1)/usr/bin
	$(CP) $(PKG_BUILD_DIR)/nvram_get $(1)/usr/bin
	
endef


$(eval $(call BuildPackage,nvram))

