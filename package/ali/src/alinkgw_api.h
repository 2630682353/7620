/*
 * Copyright (c) 2014-2015 Alibaba Group. All rights reserved.
 *
 * Alibaba Group retains all right, title and interest (including all
 * intellectual property rights) in and to this computer program, which is
 * protected by applicable intellectual property laws.  Unless you have
 * obtained a separate written license from Alibaba Group., you are not
 * authorized to utilize all or a part of this computer program for any
 * purpose (including reproduction, distribution, modification, and
 * compilation into object code), and you must immediately destroy or
 * return to Alibaba Group all copies of this computer program.  If you
 * are licensed by Alibaba Group, your rights to utilize this computer
 * program are limited by the terms of that license.  To obtain a license,
 * please contact Alibaba Group.
 *
 * This computer program contains trade secrets owned by Alibaba Group.
 * and, unless unauthorized by Alibaba Group in writing, you agree to
 * maintain the confidentiality of this computer program and related
 * information and to not disclose this computer program and related
 * information to any other person or entity.
 *
 * THIS COMPUTER PROGRAM IS PROVIDED AS IS WITHOUT ANY WARRANTIES, AND
 * Alibaba Group EXPRESSLY DISCLAIMS ALL WARRANTIES, EXPRESS OR IMPLIED,
 * INCLUDING THE WARRANTIES OF MERCHANTIBILITY, FITNESS FOR A PARTICULAR
 * PURPOSE, TITLE, AND NONINFRINGEMENT.
 */

#ifndef _ALINKGW_API_H_
#define _ALINKGW_API_H_

#ifdef __cplusplus
extern "C" {
#endif

/************************************************************************/


#define ALINKGW_TRUE                1
#define ALINKGW_FALSE               0

#define ALINKGW_OK                  0
#define ALINKGW_ERR                 -1
#define ALINKGW_BUFFER_INSUFFICENT  -2

/*************************************************************************
网关设备基本属性名称定义，设备厂商可以自定义新的属性
*************************************************************************/
#define ALINKGW_ATTR_WLAN_SWITCH_STATE      "wlanSwitchState"
#define ALINKGW_ATTR_WLAN_SWITCH_SCHEDULER  "wlanSwitchScheduler"
#define ALINKGW_ATTR_ALI_SECURITY           "aliSecurity"
#define ALINKGW_ATTR_URL_PROTECT_INFO       "urlProtectInfo"
#define ALINKGW_ATTR_ADMIN_ATTACK_SWITCH_STATE 	"antiAdminCrackSwitchState"
#define ALINKGW_ATTR_ADMIN_ATTACK_NUM 			"antiAdminCrackNum"
#define ALINKGW_ATTR_ADMIN_ATTACK_INFO 			"antiAdminCrackInfo"
#define ALINKGW_ATTR_ATTACK_SWITCH_STATE 	"antiAttackSwitchState"
#define ALINKGW_ATTR_ATTACK_NUM 			"antiAttackNum"
#define ALINKGW_ATTR_ATTACK_INFO 			"antiAttackInfo"
#define ALINKGW_ATTR_PROBE_SWITCH_STATE     "antiProbedSwitchState"
#define ALINKGW_ATTR_PROBE_NUMBER           "antiProbedNum"
#define ALINKGW_ATTR_PROBE_INFO             "antiProbedInfo"
#define ALINKGW_ATTR_ACCESS_ATTACK_STATE    "antiWifiCrackSwitchState"
#define ALINKGW_ATTR_ACCESS_ATTACK_NUM		"antiWifiCrackNum"
#define ALINKGW_ATTR_ACCESS_ATTACKR_INFO	"antiWifiCrackInfo"
#define ALINKGW_ATTR_FW_DOWNLOAD_INFO       "fwDownloadInfo"
#define ALINKGW_ATTR_FW_UPGRADE_INFO        "fwUpgradeInfo"
#define ALINKGW_ATTR_WAN_DLSPEED            "wanDlSpeed"
#define ALINKGW_ATTR_WAN_ULSPEED            "wanUlSpeed"
#define ALINKGW_ATTR_WAN_DLBYTES            "wanDlbytes"
#define ALINKGW_ATTR_WAN_ULBYTES            "wanUlbytes"
#define ALINKGW_ATTR_DLBWINFO               "dlBwInfo"
#define ALINKGW_ATTR_ULBWINFO               "ulBwInfo"
#define ALINKGW_ATTR_WLAN_PA_MODE           "wlanPaMode"
#define ALINKGW_ATTR_SPEEDUP_SETTING        "speedupSetting"
#define ALINKGW_ATTR_WLAN_SETTING_24G       "wlanSetting24g"
#define ALINKGW_ATTR_WLAN_SETTING_5G        "wlanSetting5g"
#define ALINKGW_ATTR_WLAN_SECURITY_24G      "wlanSecurity24g"
#define ALINKGW_ATTR_WLAN_SECURITY_5G       "wlanSecurity5g"
#define ALINKGW_ATTR_WLAN_CHANNEL_CONDITION_24G "wlanChannelCondition24g"
#define ALINKGW_ATTR_WLAN_CHANNEL_CONDITION_5G  "wlanChannelCondition5g"
#define ALINKGW_ATTR_TPSK                   "tpsk"
#define ALINKGW_ATTR_TPSK_LIST              "tpskList"
#define ALINKGW_ATTR_BLACK_LIST_STATE              "blacklistSwitchState"
#define ALINKGW_ATTR_WHITE_LIST_STATE             "whitelistSwitchState"
#define ALINKGW_ATTR_WHITE_LIST	"whitelist"
#define ALINKGW_ATTR_BLACK_LIST	"blacklist"
#define ALINKGW_ATTR_GUEST_SETTING_24G	"guestNetworkSetting24g"
#define ALINKGW_ATTR_WAN_STAT	"wanState"
#define ALINKGW_ATTR_WAN_SETTING "wanSetting"
#define ALINKGW_ATTR_PORTMAPPINGSETTING  "portMappingSetting"

#define ALINKGW_SUBDEV_ATTR_DLSPEED         "dlSpeed"
#define ALINKGW_SUBDEV_ATTR_ULSPEED         "ulSpeed"
#define ALINKGW_SUBDEV_ATTR_DLBYTES         "dlBytes"
#define ALINKGW_SUBDEV_ATTR_ULBYTES         "ulBytes"
#define ALINKGW_SUBDEV_ATTR_INTERNETSWITCHSTATE         "internetSwitchState"
#define ALINKGW_SUBDEV_ATTR_IPADDRESS         "ipAddress"
#define ALINKGW_SUBDEV_ATTR_IPV6ADDRESS         "ipv6Address"
#define ALINKGW_SUBDEV_ATTR_BAND         "band"
#define ALINKGW_SUBDEV_ATTR_APPINFO         "appInfo"
#define ALINKGW_SUBDEV_ATTR_APPRECORD         "appRecord"
#define ALINKGW_SUBDEV_ATTR_MAXDLSPEED         "maxDlSpeed"
#define ALINKGW_SUBDEV_ATTR_MAXULSPEED         "maxUlSpeed"
#define ALINKGW_SUBDEV_ATTR_APPBLACKLIST         "appBlacklist"
#define ALINKGW_SUBDEV_ATTR_APPWHITELIST         "appWhitelist"
#define ALINKGW_SUBDEV_ATTR_WEBWHITELIST         "webWhitelist"
#define ALINKGW_SUBDEV_ATTR_INTERNETLIMITEDSWITCHSTATE         "internetLimitedSwitchState"

/*************************************************************************
网关设备基本service名称定义，设备厂商可以自定义新的service
*************************************************************************/
#define ALINKGW_SERVICE_AUTHDEVICE          "authDevice"
#define ALINKGW_SERVICE_BWCHECK             "bwCheck"
#define ALINKGW_SERVICE_FWUPGRADE           "fwUpgrade"
#define ALINKGW_SERVICE_CHANGEPASSWORD      "changePassword"
#define ALINKGW_SERVICE_REFINEWLANCHANNEL   "refineWlanChannel"
#define ALINKGW_SERVICE_REBOOT      "reboot"
#define ALINKGW_SERVICE_FACTORYRESET   "factoryReset"


/*************************************************************************
* @desc: attribute类型，分为两种：

一种是其值可以通过单个字符串/布尔值/数字表示的属性，如属性wlanSwitchState，
其值可以表示为：

"wlanSwitchState":{
    "value"："1",  //无线开关，bool类型，"1"代表打开，"0"代表关闭
    "when":"1404443369"
}；

另一种是其值需要多个<name, value>对组合表示的属性，如属性wlanSwitchScheduler，
其值为：
"wlanSetting24g":
{
    "value":{
        "enabled":"1",  //无线接口使能，bool类型，"1"代表使能，"0"代表不使能
        "mode":"bgn",  //无线模式, "b"/"g"/"n"/"bgn"/"bg"/"gn"
        "bssid":"11:22:33:44:55:66",  //无线接口mac地址
        "ssid":"abc",  //无线ssid，1-32字符
        "ssidBroadcast":"1",  //无线ssid广播使能，bool类型，"1"代表使能，"0"代表不使能
    },
    "when":"1404443369"
}


数组类型是多个集合表示的属性，如tpsklist:
"tpskList":
{
    value:[
		{
            "tpsk":"12345678",   //hash(secret(model))
            "mac":"11:22:33:44:55:66",  //station的mac地址
            "duration":"0"  //有效期,0表示长期有效
        },
        {
            "tpsk":"12345678",   //hash(secret(model))
            "mac":"12:34:56:78:90:12"
            "duration":"0"
        }
	],
	"when":"1404443369"
}


*************************************************************************/
typedef enum {
    ALINKGW_ATTRIBUTE_simple = 0,  //值可由单个字符串/布尔值/数字表示的属性
    ALINKGW_ATTRIBUTE_complex,     //值必须由多个<name, value>对组合表示的属性
    ALINKGW_ATTRIBUTE_array,       //值必须由一个或多个集合数组表示的属性
    ALINKGW_ATTRIBUTE_MAX          //属性类型分类最大数，该值不表示具体类型
}ALINKGW_ATTRIBUTE_TYPE_E;


typedef enum {
    ALINKGW_STATUS_INITAL = 0,  //初始状态，
    ALINKGW_STATUS_INITED,      //alinkgw初始化完成
    ALINKGW_STATUS_REGISTERED,  //注册成功
    ALINKGW_STATUS_LOGGED       //登录服务器成功，正常情况下处于该状态
}ALINKGW_CONN_STATUS_E;

typedef enum{
    ALINKGW_LL_NONE = 0,
    ALINKGW_LL_ERROR,
    ALINKGW_LL_WARN,
    ALINKGW_LL_INFO,
    ALINKGW_LL_DEBUG
}ALINKGW_LOGLEVEL_E;


typedef struct ALINKGW_DEVINFO {
    /* optional */
    char sn[65];
    char name[33];
    char brand[33];
    char type[33];
    char category[33];
    char manufacturer[33];
    char version[33];

    /* must follow the format xx:xx:xx:xx:xx:xx */
    char mac[18];
    char model[81];

    char cid[65];
    char key[21];
    char secret[41];
    char key_sandbox[21];
    char secret_sandbox[41];
    /*ip address of lan device*/
    char ip[16];
}ALINKGW_DEVINFO_S;


/*
 * @desc:       函数类型，表示厂商提供的alinkgw连接状态更新通知函数原型
 * @param:
 *     new_status:    更新后的链接状态
 * @return:
 *     无
 */
typedef void (*ALINKGW_STATUS_cb)(ALINKGW_CONN_STATUS_E new_status);


/*
 * @desc:       函数类型，表示厂商提供的保存<Key,Value>字符串Pair到非易失
 *              性存储器的函数原型
 * @param:
 *      key:    value值对于的字符串关键字，带结束字符'\0'
 *      value:  存放保存的字符串值，带结束字符'\0'
 * @return:
 *      ALINKGW_OK:  保存成功
 *      ALINKGW_ERR: 保存失败
 */
typedef int (*ALINKGW_KVP_save_cb)(const char *key, \
                                   const char *value);



/*
 * @desc:       函数类型，表示厂商提供的从非易失性存储器中读取<Key, Value>
 *              字符串Pair的函数原型
 * @param:
 *      key:    value值对于的字符串关键字，带结束字符'\0'
 *      value:  存放保存的字符串值，带结束字符'\0'
 *      buff_size:  value缓冲区长度
 * @return:
 *      ALINKGW_OK: 读取成功
 *      ALINKGW_BUFFER_INSUFFICENT: 缓冲区不足
 *      其他:       读取失败
 */
typedef int (*ALINKGW_KVP_load_cb)(const char *key, \
                                   char *value, \
                                   unsigned int buf_sz);


/*
 * @desc:       函数类型，表示厂商提供的获取指定属性的值的函数原型
 * @param:
 *      buf:    存放输出属性值的buf指针
 *      buf_sz:  value缓冲区长度
 * @return:
 *      ALINKGW_OK: 成功
 *      ALINKGW_BUFFER_INSUFFICENT: 缓冲区不足
 *      其他:       失败
 */
typedef int (*ALINKGW_ATTRIBUTE_get_cb)(char *buf, \
                                        unsigned int buf_sz);



/*
 * @desc:       函数类型，表示厂商提供的配置指定属性的函数原型
 * @param:
 *      json_in: 输入字符串，结束字符'\0'
 *               属性值类型为simple，json_in中存放单一值字符串,如:
 *				 1或false等
 *               属性值类型为complex，json_in中存放json格式字符串，如:
 *			     {"enabled":"1","offTime":[] "onTime":[]}
 * @return:
 *      ALINKGW_OK: 成功
 *      ALINKGW_ERR: 失败
 */
typedef int (*ALINKGW_ATTRIBUTE_set_cb)(const char *json_in);



/*
 * @desc:       函数类型，表示厂商提供的执行远端服务请求的函数原型
 * @param:
 *      json_in:       输入字符串，结束字符'\0'
 *      json_out_buf:  存放服务执行结果json串的buffer，没有结果输
 * @return:
 *      ALINKGW_OK: 成功
 *      ALINKGW_ERR: 失败
 */
typedef int (*ALINKGW_SERVICE_execute_cb)(const char *json_in, \
                                          char *json_out_buf, \
                                          unsigned int buf_sz);

/*
 * @desc:       子设备属性上报结构体定义，表示子设备的结构体实例占内存空间长度非固定，
 *                视属性个数而定
 * @menber:
 *      mac[18]:        子设备mac地址，格式为小写字母表示的17Byte长冒号隔开的字符串,
 *                      eg: "00:11:22:cc:bb:dd"
 *      attr_name[]:    属性名称字符串数组，以NULL指针结尾
 */
typedef struct subdevice_attr_
{
    char mac[18];
    const char *attr_name[];
}subdevice_attr_t;


/*
 * @desc:       函数类型，表示厂商提供的获取子设备指定属性的值的函数原型
 * @param:
 *      subdev_mac:子设备mac地址，格式:xx:xx:xx:xx:xx:xx
 *      buf:    存放输出属性值的buf指针
 *      buf_sz: 存放输出json串的buffer大小
 * @return:
 *      ALINKGW_OK: 成功
 *      ALINKGW_BUFFER_INSUFFICENT: 缓冲区不足
 *      其他:       失败
 */
typedef int (*ALINKGW_ATTRIBUTE_subdevice_get_cb)(const char *subdev_mac, \
                                                  char *buf, \
                                                  unsigned int buf_sz);


/*
 * @desc:       函数类型，表示厂商提供的配置子设备指定属性的函数原型
 * @param:
 *      subdev_mac:子设备mac地址，格式:xx:xx:xx:xx:xx:xx
 *                       格式:xx:xx:xx:xx:xx:xx
 *      json_in: 输入字符串，结束字符'\0'
 *                属性值类型为simple，json_in中存放单一值字符串,如:
 *                1或false等
 *                属性值类型为complex，json_in中存放json格式字符串，如:
 *                {"enabled":"1","offTime":[] "onTime":[]}
 * @return:
 *      ALINKGW_OK:     成功
 *      ALINKGW_ERR:    失败
 */
typedef int (*ALINKGW_ATTRIBUTE_subdevice_set_cb)(const char *subdev_mac, \
                                                  const char *json_in);



/*
 * @desc:       多个子设备的属性间接上报接口，支持多属性上报，需要注意：
 *              - 该函数需要在ALINKGW_start()后调用
 * @param:
 *      subdev_attrs:  上报的子设备和属性结构体指针数组，以NULL结束
 * @return:
 *      ALINKGW_OK:     成功
 *      ALINKGW_ERR:    失败
 */
int ALINKGW_report_attr_subdevices(subdevice_attr_t *subdev_attrs[]);



/*
 * @desc:       注册子设备属性
 * @param:
 *      name:   属性名称
 *      type:   属性值类型
 *      get_cb: 获取属性值的回调函数
 *      set_cb: 设置属性值的回调函数
 * @return:
 *      ALINKGW_OK:     成功
 *      ALINKGW_ERR:    失败
 */
int ALINKGW_register_attribute_subdevice(const char *attr_name,\
                                           ALINKGW_ATTRIBUTE_TYPE_E type,\
                                           ALINKGW_ATTRIBUTE_subdevice_get_cb get_cb,\
                                           ALINKGW_ATTRIBUTE_subdevice_set_cb set_cb);


/*
 * @desc:       注册厂商指定的设备属性
 * @param:
 *      name:   属性名称
 *      type:   属性值类型
 *      get_cb: 获取属性值的回调函数
 *      set_cb: 设置属性值的回调函数
 * @return:
 *      ALINKGW_OK:     成功
 *      ALINKGW_ERR:    失败
 */
int ALINKGW_register_attribute(const char *name,
                               ALINKGW_ATTRIBUTE_TYPE_E type,
                               ALINKGW_ATTRIBUTE_get_cb get_cb,
                               ALINKGW_ATTRIBUTE_set_cb set_cb);



/*
 * @desc:       注册厂商指定的可被远端物联平台调用的RPC服务
 * @param:
 *      name:   服务名称
 *      exec_bc:远程调用服务回调函数
 * @return:
 *      ALINKGW_OK:     成功
 *      ALINKGW_ERR:    失败
 */
int ALINKGW_register_service(const char *name,
                             ALINKGW_SERVICE_execute_cb exec_cb);



/*
 * @desc:       注销厂商指定的可被远端物联平台调用的RPC服务
 * @param:
 *      name:      服务名称
 * @return:
 *      ALINKGW_OK:     成功
 *      ALINKGW_ERR:    失败
 */
int ALINKGW_unregister_service(const char *name);



/*
 * @desc:       设置厂商提供的<Key,Value>字符串Pair的save/load的回调函数
 * @param:
 *      save_cb:    保存<Key,Value>字符串Pair的回调函数，需非空
 *      load_cb:    恢复<Key,Value>字符串Pair的回调函数，需非空
 * @return:
 */
void ALINKGW_set_kvp_cb(ALINKGW_KVP_save_cb save_cb,
                        ALINKGW_KVP_load_cb load_cb);


/*
 * @desc:       设置厂商提供的alinkgw连接server端状态变化回调函数
 * @param:
 *      new_status: 链接状态更新回调函数，必须非空
 * @return:
 */
void ALINKGW_set_conn_status_cb(ALINKGW_STATUS_cb status_cb);


/*
 * @desc:       设备属性并更，间接上报接口,需要注意：
 *              - 该函数需要在ALINKGW_start()后调用
 * @param:
 *      attr_name:  上报的属性名称
 * @return:
 *      ALINKGW_OK:     成功
 *      ALINKGW_ERR:    失败
 */
int ALINKGW_report_attr(const char *attr_name);


/*
 * @desc:       设备多个属性，间接上报接口,需要注意：
 *              - 该函数需要在ALINKGW_start()后调用
 * @param:
 *      attr_name:  上报的属性名称指针数组，以NULL结束
 * @return:
 *      ALINKGW_OK:     成功
 *      ALINKGW_ERR:    失败
 */
int ALINKGW_report_attrs(const char *attr_name[]);



/*
 * @desc:       设备属性直接上报接口,需要注意：
 *              - 该函数需要在ALINKGW_start()后调用
 * @param:
 *      attr_name:  上报的属性名称
 *      type:       上报属性的类型
 *      data:       上报属性的值
 * @return:
 *      ALINKGW_OK:     成功
 *      ALINKGW_ERR:    失败
 */
int ALINKGW_report_attr_direct(const char *attr_name,\
                               ALINKGW_ATTRIBUTE_TYPE_E type,\
                               const char *data);


/*
 * @desc:       接入网关的子设备上线上报
 * @param:
 *      name:           设备名称
 *      type:           设备类型
 *      category:       设备分类
 *      manufacturer:   设备厂商，未知厂商统一为:unknown
 *      mac:            设备mac地址，必须非空串，格式:xx:xx:xx:xx:xx:xx
 *      model:          设备model，NULL或"\0"表示未知类型
 * @return:
 *      ALINKGW_OK:     成功
 *      ALINKGW_ERR:    失败
 */
int ALINKGW_attach_sub_device(const char *name,
                              const char *type,
                              const char *category,
                              const char *manufacturer,
                              const char *mac,
                              const char *model);


/*
 * @desc:       接入网关的子设备离线上报
 * @param:
 *      mac:            设备mac地址，必须非空串，格式:xx:xx:xx:xx:xx:xx
 * @return:
 *      ALINKGW_OK:     成功
 *      ALINKGW_ERR:    失败
 */
int ALINKGW_detach_sub_device(const char *dev_mac);



/*
 * @desc:       等待alinkgw连接服务器，直到登录成功或超时返回
 * @param:
 *      timeout:    等待返回超时时间单位秒，-1表示一直等待直到成功连接到云端
 * @return:
 *      ALINKGW_OK:     成功
 *      ALINKGW_ERR:    失败
 */
int ALINKGW_wait_connect(int timeout);



/*
 * @desc:       设置物联平台连接模式(0:用户线上模式<默认配置>
                                                                             1:沙箱调试模式
                                                                             2:日常调试模式)
 *           该函数需要在ALINKGW_start()前调用
 * @param:
 * @return:
 *      ALINKGW_OK:     成功
 *      ALINKGW_ERR:    失败
 */
int ALINKGW_set_run_mode(int mode);


/*
 * @desc:       开启（或关闭）“阿里安全”插件功能，需要注意：
 *           - “阿里安全”功能默认开启
 *           - 当用户通过UI界面关闭/开启“阿里安全”时，调用本接口
 * @param:
 *      bEnabled： 1 - 开启， 0 - 关闭
 * @return:
 *      ALINKGW_OK:     成功
 *      ALINKGW_ERR:    失败
 */
int ALINKGW_enable_asec(unsigned int bEnabled);


/*
 * @desc:   设置日志信息级别（默认级别为ALINKGW_LL_ERROR）
 * @param:
 * @return:
 */
void ALINKGW_set_loglevel(ALINKGW_LOGLEVEL_E e);


/*
 * @desc:   解除路由器设备和手机APP用户的绑定关系。需要注意：
 *           - 该函数仅适用于恢复路由器出厂设置场景，请勿在其他场景下调用
 *			 - 该函数只有在设备联网环境才能成功
 *           - 该函数调用需在ALINKGW_start()之后、ALINKGW_end()之前调用
 * @param:
 * @return:
 *      ALINKGW_OK:     成功
 *      ALINKGW_ERR:    失败
 */
int ALINKGW_reset_binding();

/*
 * @desc:  启动alink服务，调用该接口前，要配置平台信息、注册属性和服务
 * @param:
 *      sn:        设备序列号, 最大64字节
 *      name:      设备名称，最大32字节
 *      brand:     设备品牌，最大32字节
 *      type:      设备类型，最大32字节
 *      category:  设备分类，最大32字节
 *      manufacturer:   设备制造商, 最大32字节
 *      version:   固件版本号,最大32字节
 *      mac:       设备mac地址，格式11:22:33:44:55:66，建议使用lan
 *			       接口mac地址,避免mac克隆引起设备mac地址变更
 *      model:     设备型号，由物联平台授权，最大80字节
 *      cid:       设备芯片ID，同类型不同设备的cid必须不同，最大64字节
 *      key:       设备链接云端key，由物联平台颁发
 *      secret:    设备链接云端秘钥，由物联平台颁发
 *      ip:        lan接口ip地址
 * @return:
 *      ALINKGW_OK:     成功
 *      ALINKGW_ERR:    失败
 */
int ALINKGW_start(ALINKGW_DEVINFO_S *device_info);

/*************************************************************************
Function:       ALINKGW_end()
Description:    停止alink服务，并释放资源
Input:          无
Output:         无
 *      ALINKGW_OK:     成功
 *      ALINKGW_ERR:    失败
 */
int ALINKGW_end();


/*
 * @desc:       云端存储数据
 * @param:
 *      name: 数据名称字符串（以'\0'结尾）
 *      val_buf: 数据值buffer
 *      val_len: 数据值buffer length
 * @return:
 *      ALINKGW_OK:     成功
 *      ALINKGW_ERR:    失败
 */
int ALINKGW_cloud_save(
    const char *name,
    unsigned char *val_buf,
    unsigned int val_len);


/*
 * @desc:       从云端恢复数据
 * @param:
 *      name: 数据名称字符串（以'\0'结尾）
 *      val_buf: 存放从云端获取的数据值的buffer
 *      val_len: value-result参数，用作value时，存放buffer的size；
 *               用作result时，存放value length
 *               用作result时，存放value length
 * @return:
 *      ALINKGW_OK:     成功
 *      ALINKGW_ERR:    失败
 */
int ALINKGW_cloud_restore(
    const char *name,
    unsigned char *val_buf,
    unsigned int *val_len);



/*
 * @desc:       从云端获取最近一次上报的属性值接口
 * @param:
 *      attr_name:  获取的属性名称数组，以NULL结束
 *      val_buf: 存放从云端获取的属性值的buffer
 *      val_len: value-result参数，用作value时，存放buffer的size；
 *               用作result时，存放value length
 * @return:
 *      ALINKGW_OK:     成功
 *      ALINKGW_ERR:    失败
 */
int ALINKGW_get_cloud_attrs(
    const char *attr_name[],
    unsigned char *val_buf,
    unsigned int *val_len);



/*
 * @desc:       wifi DOG跳转使能开关
 * @param:
 *      bEnabled:  1 - 开， 0 - 关
 * @return:
 *      ALINKGW_OK:     成功
 *      ALINKGW_ERR:    失败
 */
int ALINKGW_enable_guide_mode(unsigned int bEnabled);


/*
 * @desc:       设置WIFIDOG重定向uri地址
 * @param:
 *      uri - 重定向到的uri地址
 * @return:
 *      ALINKGW_OK:     成功
 *      ALINKGW_ERR:    失败
 */
int ALINKGW_set_guide_uri(const char *uri);

/********************************OTA START*****************************/

#define post_callback(cb, percent)   \
    if(cb)  \
    { \
        cb(percent);\
    }

/*升级参数存储接口类型定义*/
typedef int (*ALINKGW_OTA_KeyValueGet)(const char *key, char *buff, unsigned int buff_size);
typedef int (*ALINKGW_OTA_KeyValueSet)(const char *key, const char *value);

/*升级step上报接口定义*/
typedef int (*ALINKGW_OTA_StatusPost)(int percent);

/*升级step接口类型定义*/
typedef void (*ALINKGW_OTA_ProcessUnload) (ALINKGW_OTA_StatusPost cb);
typedef int (*ALINKGW_OTA_MemCheck) (int fwSize, ALINKGW_OTA_StatusPost cb);
typedef int (*ALINKGW_OTA_FwFlash) (const char *fwName, ALINKGW_OTA_StatusPost cb);
typedef int (*ALINKGW_OTA_SysReboot) (ALINKGW_OTA_StatusPost cb);

/*
 * @desc:       设置全局升级接口
 * @param:
 *      ota_processUnload - 厂家进程卸载接口
        ota_memCheck - 厂家内存检测接口
        ota_fwFlash - 厂家系统烧录接口
        ota_sysReboot - 厂家系统重启接口
 * @return:
 *      ALINKGW_OK:     成功
 *      ALINKGW_ERR:    失败
 */
int ALINKGW_OTA_Interface_set(ALINKGW_OTA_ProcessUnload ota_processUnload,
                                                            ALINKGW_OTA_MemCheck ota_memCheck,
                                                            ALINKGW_OTA_FwFlash ota_fwFlash,
                                                            ALINKGW_OTA_SysReboot ota_sysReboot);

/*
 * @desc:       设置默认下载路径及文件名
 * @param:
 *      fwName - 文件系统存放文件名(含绝对路径)
 * @return:
 *      ALINKGW_OK:     成功
 *      ALINKGW_ERR:    失败
 */
int ALINKGW_OTA_DownloadFwName_set(const char *fwName);



/*
 * @desc:       设置升级参数存储接口
 * @param:
 *      ota_keyValueGet - key/value参数获取接口
         ota_keyValueSet - key/value参数设置接口
 * @return:
 *      无
 */
void ALINKGW_OTA_KeyValue_set(ALINKGW_OTA_KeyValueGet ota_keyValueGet,
                                                            ALINKGW_OTA_KeyValueSet ota_keyValueSet);

/********************************OTA END*****************************/

/************************************************************************/

#ifdef __cplusplus
}
#endif

#endif /* _ALINKGW_API_H_ */

