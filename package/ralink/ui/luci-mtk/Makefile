#
# Copyright (C) 2010-2013 hua.shao@mediatek.com
#
# Ralink Property Software.
#

include $(TOPDIR)/rules.mk

PKG_NAME:=luci-mtk
PKG_RELEASE:=1
PKG_BUILD_DEPENDS:=$(if $(STAGING_DIR_ROOT),lua/host)
PKG_BUILD_DIR:=$(BUILD_DIR)/$(PKG_NAME)
PKG_INSTALL_DIR:=$(PKG_BUILD_DIR)/ipkg-install

LUA_TARGET:=source
LUCI_CFLAGS:=
LUCI_BUILD_PACKAGES:=
LUCI_SELECTED_MODULES:=

include $(INCLUDE_DIR)/package.mk

NIXIO_TLS:=


define Package/luci-mtk
  CATEGORY:=Ralink Properties
  TITLE:=LuCI (Customized by MediaTek)
  MAINTAINER:=Hua Shao <hua.shao@mediatek.com>
  SUBMENU:=UI
  DEPENDS:=+uhttpd +uhttpd-mod-ubus +lua +libuci-lua +libubus-lua +libiwinfo-lua 
endef

define Build/Prepare
	mkdir -p $(PKG_BUILD_DIR)
	$(TAR) c -C ./src . \
		--exclude=.pc --exclude=.svn --exclude=.git \
		--exclude='boa-0*' --exclude='*.o' --exclude='*.so' \
		--exclude=dist | \
			tar x -C $(PKG_BUILD_DIR)/
	$(call Build/Prepare/Default)
endef

define Package/luci/install/template
	$(CP) -a $(PKG_BUILD_DIR)/$(2)/dist/* $(1)/ -R
	$(CP) -a $(PKG_BUILD_DIR)/$(2)/ipkg/* $(1)/CONTROL/ 2>/dev/null || true
endef

define Package/luci-mtk/install
	touch $(1)/etc/init.d/luci_fixtime || true
	$(call Package/luci/install/template,$(1),libs/core)
	$(call Package/luci/install/template,$(1),libs/sys)
	$(call Package/luci/install/template,$(1),libs/nixio)
	$(call Package/luci/install/template,$(1),libs/sgi-cgi)
	$(call Package/luci/install/template,$(1),libs/web)
	$(call Package/luci/install/template,$(1),protocols/core)
	$(call Package/luci/install/template,$(1),protocols/ppp)
	$(call Package/luci/install/template,$(1),libs/ipkg)
	$(call Package/luci/install/template,$(1),modules/admin-core)
	$(call Package/luci/install/template,$(1),modules/admin-full)
	$(call Package/luci/install/template,$(1),themes/base)
	$(call Package/luci/install/template,$(1),themes/bootstrap)
	$(call Package/luci/install/template,$(1),i18n/english)
endef

LUCI_SELECTED_MODULES:= \
	libs/core \
	libs/sys \
	libs/nixio \
	libs/sgi-cgi \
	libs/web \
	libs/ipkg \
	protocols/core \
	protocols/ppp \
	modules/admin-core \
	modules/admin-full \
	themes/base \
	themes/bootstrap \
	i18n/english


MAKE_FLAGS += \
	MODULES="$(LUCI_SELECTED_MODULES)" \
	LUA_TARGET="$(LUA_TARGET)" \
	LUA_SHLIBS="-llua -lm -ldl -lcrypt" \
	CFLAGS="$(TARGET_CFLAGS) $(LUCI_CFLAGS) -I$(STAGING_DIR)/usr/include" \
	LDFLAGS="$(TARGET_LDFLAGS) -L$(STAGING_DIR)/usr/lib" \
	NIXIO_TLS="$(NIXIO_TLS)" OS="Linux"


$(eval $(call BuildPackage,luci-mtk))
