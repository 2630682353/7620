#ifndef __CT_SGW_API_H__
#define __CT_SGW_API_H__


/**
 * IP地址字符串最大长度
 */
#define CT_IP_ADDR_STR_MAX_LEN    (16)

/**
 * IPv6地址字符串最大长度
 */
#define CT_IPV6_ADDR_STR_MAX_LEN    (64)

/**
 * USB设备名称最大长度
 */
#define CT_USB_DEV_NAME_MAX_LEN    (128)

/**
 * USB存储设备mount路径最大长度
 */
#define CT_USB_DEV_MOUNT_PATH_MAX_LEN    (256)

/**
 * 无线SSID最大长度
 */
#define CT_WLAN_SSID_MAX_LEN    (32)

/**
 * 无线密码最大长度
 */
#define CT_WLAN_PASSWORD_MAX_LEN    (32)


/**
 * 设备别名最大长度
 */
#define CT_LAN_DEV_NAME_MAX_LEN    (64)

/**
 * 设备品牌名称最大长度
 */
#define CT_LAN_DEV_BRAND_NAME_MAX_LEN    (64)

/**
 * 设备操作系统名称最大长度
 */
#define CT_LAN_DEV_OS_NAME_MAX_LEN    (64)

/**
 * 事件类型
 */
typedef enum
{
	CT_EVENT_USB_DEV_ACTION,
	CT_EVENT_WAN_IP_CHANGE,
	CT_EVENT_WLAN_DEV_ONLINE
} CtEventType;


/**
 * USB设备动作类型
 */
typedef enum
{
    CT_USB_DEV_ACTION_INSERT,
    CT_USB_DEV_ACTION_PULL,
} CtUsbDevActionType;


/**
 * USB设备类型
 */
typedef enum
{
    CT_USB_DEV_SERIAL = 0x1,
    CT_USB_DEV_CDC_ACM = 0x2,
    CT_USB_DEV_HID = 0x4,
    CT_USB_DEV_STORAGE = 0x8,
} CtUsbDevType;


/**
 * LAN设备连接类型
 */
typedef enum
{
    CT_LAN_DEV_CONN_UNKNOWN,
    CT_LAN_DEV_CONN_WIRED,
    CT_LAN_DEV_CONN_WLAN,
} CtLanDevConnType;


/**
 * LAN设备类型
 */
typedef enum
{
    CT_LAN_DEV_UNKNOWN,
    CT_LAN_DEV_PHONE,
    CT_LAN_DEV_PAD,
    CT_LAN_DEV_PC,
    CT_LAN_DEV_STB,
    CT_LAN_DEV_OTHER
} CtLanDevType;


/**
 * 无线安全模式
 */
typedef enum
{
    CT_WL_AUTH_OPEN ,
    CT_WL_AUTH_SHARED,        
    CT_WL_AUTH_WPA_PSK,
    CT_WL_AUTH_WPA2_PSK,
    CT_WL_AUTH_WPA2_PSK_MIX    
} CtWlanAuthMode;


/**
 * 奇偶校验类型
 */
typedef enum
{
    CT_PARITY_NONE,  /* 没有校验位*/
    CT_PARITY_EVEN,  /* 偶校验 */
    CT_PARITY_ODD,  /* 奇校验 */
    CT_PARITY_MARK,  /* 校验位始终为1 */
    CT_PARITY_SPACE,  /* 校验位始终为0 */
} CtParityType;



/**
 * USB设备事件
 */
typedef struct
{
    CtUsbDevActionType actionType;  /* 动作类型 */
    CtUsbDevType devType;  /* 设备类型 */
    int devId;  /* 设备标识 */
    char devName[CT_USB_DEV_NAME_MAX_LEN];  /* USB存储设备名称 */
    char mountPath[CT_USB_DEV_MOUNT_PATH_MAX_LEN];  /* USB存储设备mount路径 */
} CtUsbDevEvent;


/**
 * 上网WAN连接IP改变事件
 */
typedef struct
{
    char ipAddr[CT_IP_ADDR_STR_MAX_LEN];
    char subnetMask[CT_IP_ADDR_STR_MAX_LEN];
    char ipv6Addr[CT_IPV6_ADDR_STR_MAX_LEN];
    int prefixLen;
} CtWanIpChangeEvent;


/**
 * 无线下挂设备上线事件
 */
typedef struct
{
    unsigned char macAddr[6];
} CtWlanDevOnlineEvent;


/**
 * 系统IP地址配置
 */
typedef struct
{
    char wanIpAddr[CT_IP_ADDR_STR_MAX_LEN];
    char wanSubnetMask[CT_IP_ADDR_STR_MAX_LEN];
    char ipv6WanAddr[CT_IPV6_ADDR_STR_MAX_LEN];
    int wanPrefixLen;
    char lanIpAddr[CT_IP_ADDR_STR_MAX_LEN];
    char lanSubnetMask[CT_IP_ADDR_STR_MAX_LEN];
    char ipv6LanAddr[CT_IPV6_ADDR_STR_MAX_LEN];
    int lanPrefixLen;
} CtSysIpAddrCfg;


/**
 * MAC地址
 */
typedef struct
{
    unsigned char macAddr[6];
} CtMacAddr;


/**
 * IP地址
 */
typedef struct
{
    char ipAddr[CT_IP_ADDR_STR_MAX_LEN];
} CtIpAddr;


/**
 * 设备信息
 */
typedef struct
{
    char devName[CT_LAN_DEV_NAME_MAX_LEN];  /** 网络别名 */
    CtLanDevType devType;  /** 设备类型 */
    unsigned char macAddr[6];  /** MAC地址 */
    char ipAddr[CT_IP_ADDR_STR_MAX_LEN];  /** IP地址 */
    CtLanDevConnType connType;  /** 连接形式*/
    int port;  /** 连接端口，0：未知，1：lan1口连接，2：lan2口连接，3：lan3口连接，4：lan4口连接 ，5：SSID-1，6：SSID-2，7：SSID-3，8：SSID-4，9：SSID-5，10：SSID-6，11：SSID-7，12：SSID-8*/
    char brandName[CT_LAN_DEV_BRAND_NAME_MAX_LEN];  /** 品牌名称 */
    char osName[CT_LAN_DEV_OS_NAME_MAX_LEN];  /** 操作系统名称 */
    unsigned int onlineTime;  /** 在线时长，单位为秒*/
} CtLanDevInfo;


/**
 * 无线安全配置参数
 */
typedef struct
{
    int enable;
    CtWlanAuthMode authMode;
    char ssid[CT_WLAN_SSID_MAX_LEN];
    char password[CT_WLAN_PASSWORD_MAX_LEN];
} CtWlanSecurityCfg;


/**
 * 串口配置参数
 */
typedef struct
{
    int bandrate;  /* 表示波特率 */
    CtParityType parity;  /* 奇偶校验类型 */
    int dataBits;  /* 表示数据位 */
    int stopBits;  /* 表示停止位 */
    int hwFlowCtrl;  /* 表示是否开启硬件流控，0表示关闭，1表示开启 */
    int swFlowCtrl;  /* 表示是否开启软件流控，0表示关闭，1表示开启 */
} CtUsbSerialCfg;


/*
DPI报文输入结构
*/
typedef struct CtSgwTupleInfo_{
	unsigned char direct;
	unsigned char proto;
	unsigned int sipv4;
	unsigned int dipv4;
	unsigned int sipv6[4];
	unsigned int dipv6[4];
	unsigned short sport;
	unsigned short dport;
	unsigned long in_iif;/* 报文进接口index*/
	unsigned long out_iif;/*报文出接口index*/
}CtSgwTupleInfo;

/*
DPI 引擎回调函数输入结构
*/
typedef  int  (*ctSgw_appCtxCreate)(void** p_app_ctx, unsigned char flag);
typedef  void (*ctSgw_appCtxDestroy)(void** p_app_ctx);
typedef  int (*ctSgw_appProcAppId)(unsigned char *layer2data, CtSgwTupleInfo *tupleinfo, void *p_app_ctx, unsigned int *layer7_id);

typedef struct ctSgw_dpiFuncs_
{
    ctSgw_appCtxCreate  ctSgw_appCtxCreateHook;
    ctSgw_appCtxDestroy ctSgw_appCtxDestroyHook;
	ctSgw_appProcAppId  ctSgw_appProcAppIdHook;
} ctSgw_dpiFuncs;

/**
 * 初始化模块
 * @param sock(OUT) 返回的消息监听套接字
 * @return 错误码，0表示成功，-1表示失败
 */
int ctSgw_init(int *sock);


/**
 * 清理模块
 * @return 错误码，0表示成功，-1表示失败
 */
int ctSgw_cleanup(void);


/**
 * 处理事件
 * @param eventType(OUT) 返回的事件类型
 * @param eventInfo(OUT) 返回的事件信息，动态分配内存，需要调用者释放
 * @return 错误码，0表示成功，-1表示失败
 */
int ctSgw_handleEvent(CtEventType *eventType, void **eventInfo);


/**
 * 设置无线SSID开关状态
 * @param ssidIndex(IN) SSID序号，对于单频率网关为1-4，对于双频网关为1-8，当为0时，表示整个Wifi模块
 * @param enable(IN) 0表示关闭，1表示开启
 * @return 错误码，0表示成功，-1表示失败
 */
int ctSgw_wlanSetState(int ssidIndex, int enable);


/**
 * 获取WLAN（SSID-1）的安全配置
 * @param securityCfg(OUT) 返回的无线安全配置
 * @return 错误码，0表示成功，-1表示失败
 */
int ctSgw_wlanGetSecurityCfg(CtWlanSecurityCfg *securityCfg);


/**
 * 获取LAN侧设备信息
 * @param devInfoList(OUT) 返回的设备信息列表，动态分配内存，需要调用者释放
 * @param devNum(OUT) 返回的设备信息个数
 * @return 错误码，0表示成功，-1表示失败
 */
int ctSgw_lanGetDevInfo(CtLanDevInfo **devInfoList, int *devNum);


/**
 * 更新LAN侧设备信息
 * @param devInfo(IN) 设备信息，其中
 *   devInfo->macAddr用来标识需要更新的设备，不能为空 *   devInfo->devName为空表示不需要更新
 *   devInfo->devType为CT_LAN_DEV_UNKNOWN表示不需要更新
 *   devInfo->ipAddr为空表示不需要更新
 *   devInfo->connType为CT_LAN_DEV_CONN_UNKNOWN表示不需要更新
 *   devInfo->port为0表示不需要更新
 *   devInfo->brandName为空表示不需要更新
 *   devInfo->osName为空表示不需要更新
 *   devInfo->onlineTime应忽略，不需要更新
 * @return 错误码，0表示成功，-1表示失败
 */
int ctSgw_lanUpdateDevInfo(const CtLanDevInfo *devInfo);


/**
 * 设置LAN侧设备访问权限
 * @param macAddr(IN) 设备MAC地址
 * @param internetAccess(IN) 是否允许访问Internet，0表示不允许，1表示允许
 * @param storageAccess(IN) 是否允许访问存储设备，0表示不允许，1表示允许
 * @return 错误码，0表示成功，-1表示失败
 */
int ctSgw_lanSetDevAccessPermission(const CtMacAddr *macAddr, int internetAccess, int storageAccess);


/**
 * 获取LAN侧设备Internet访问黑名单
 * @param macAddrList(OUT) 返回的设备列表，动态分配内存，需要调用者释放
 * @param macAddrNum(OUT) 返回的设备个数
 * @return 错误码，0表示成功，-1表示失败
 */
int ctSgw_lanGetDevInternetAccessBlacklist(CtMacAddr **macAddrList, int *macAddrNum);


/**
 * 获取LAN侧设备存储访问黑名单
 * @param macAddrList(OUT) 返回的设备列表，动态分配内存，需要调用者释放
 * @param macAddrNum(OUT) 返回的设备个数
 * @return 错误码，0表示成功，-1表示失败
 */
int ctSgw_lanGetDevStorageAccessBlacklist(CtMacAddr **macAddrList, int *macAddrNum);


/**
 * 获取LAN侧设备上下行带宽
 * @param macAddr(IN) 设备MAC地址
 * @param usBandwidth(OUT) 返回的上行带宽，单位kbps
 * @param dsBandwidth(OUT) 返回的下行带宽，单位kbps
 * @return 错误码，0表示成功，-1表示失败
 */
int ctSgw_lanGetDevBandwidth(const CtMacAddr *macAddr, int *usBandwidth, int *dsBandwidth);


/**
 * 设置LAN侧设备上下行最大带宽
 * @param macAddr(IN) 设备MAC地址
 * @param usBandwidth(IN) 上行带宽，单位kbps，0表示不限
 * @param dsBandwidth(IN) 下行带宽，单位kbps，0表示不限
 * @return 错误码，0表示成功，-1表示失败
 */
int ctSgw_lanSetDevMaxBandwidth(const CtMacAddr *macAddr, int usBandwidth, int dsBandwidth);


/**
 * 获取LAN侧设备上下行最大带宽
 * @param macAddr(IN) 设备MAC地址
 * @param usBandwidth(OUT) 返回的上行带宽， 单位kbps，0表示不限
 * @param dsBandwidth(OUT) 返回的下行带宽，单位kbps，0表示不限
 * @return 错误码，0表示成功，-1表示失败
 */
int ctSgw_lanGetDevMaxBandwidth(const CtMacAddr *macAddr, int *usBandwidth, int *dsBandwidth);


/**
 * 获取物理接口连接状态
 * @param portStatus(OUT) 返回的端口状态位掩码，1表示连接，0表示为连接，
                          位0-5分别表示LAN1口-LAN4口, WAN口和WiFi模块的状态
 * @return 错误码，0表示成功，-1表示失败
 */
int ctSgw_lanGetPortStatus(int *portStatus);


/**
 * 设置LAN侧设备流量统计状态
 * @param enable(IN) 启动/停止流量统计功能，0表示关闭，1表示启动
 * @return 错误码，0表示成功，-1表示失败
 */
int ctSgw_lanSetDevStatsStatus(int enable);


/**
 * 获取WAN侧接口统计
 * @param usStats(OUT) 返回的上行统计，单位Kbyte
 * @param dsStats(OUT) 返回的下行统计，单位Kbyte
 * @return 错误码，0表示成功，-1表示失败
 */
int ctSgw_wanGetIfStats(int *usStats, int *dsStats);


/**
 * 获取WAN侧PPPoE帐号
 * @param pppoeAccount(OUT) 返回的PPPoE账号
 * @param accountLen(IN) 账号存储缓冲区长度
 * @return 错误码，0表示成功，-1表示失败
 */
int ctSgw_wanGetPppoeAccount(char *pppoeAccount, int len);


/**
 * 获取CPU占用率
 * @param status(OUT) 返回的CPU占用率百分比
 * @return 错误码，0表示成功，-1表示失败
 */
int ctSgw_sysGetCpuUsage(int *percent);


/**
 * 获取内存占用率
 * @param status(OUT) 返回的内存占用率百分比
 * @return 错误码，0表示成功，-1表示失败
 */
int ctSgw_sysGetMemUsage(int *percent);


/**
 * 获取LAN/WAN的IP地址
 * @param ipAddrCfg(OUT) 返回的IP地址配置
 * @return 错误码，0表示成功，-1表示失败
 */
int ctSgw_sysGetIpAddr(CtSysIpAddrCfg *ipAddrCfg);


/**
 * 获取LOID
 * @param loid(OUT) 返回的loid
 * @param loidLen(IN) loid缓冲区长度
 * @return 错误码，0表示成功，-1表示失败
 */
int ctSgw_sysGetLoid(char *loid, int len);


/*
* 获取网关MAC地址
* @param MAC(OUT) 返回的6字节MAC  
* @return 错误码，0表示成功，-1表示失败
*/
int ctSgw_sysGetMac(unsigned char mac[6]);


/**
 * 推送页面
 * @param enable(IN)推送功能是否开启，0 表示关闭，1表示开启
 * @param width(IN) 窗体宽度
 * @param height(IN) 窗体高度
 * @param place(IN) 窗体弹出页面位置，坐标原点为屏幕左上位置，单位为像素
 * @param url(IN) 推送URL
 * @param time(IN) 尝试推送时间，当网关收到本接口时，在TIME 时间内，如果侦测到用户上网，网关将URL 推送到用户浏览器，如果没有侦测到，网关结束本次操作，单位为秒
 * @return 错误码，0表示成功，-1表示失败
 */
int ctSgw_sysPushWeb(int enable, int width, int height, int place, const char *url, int time);


/**
 * 注册感兴趣的USB设备类型
 * @param devType(IN) 感兴趣的USB设备类型
 * @return 错误码，0表示成功，-1表示失败
 */
int ctSgw_usbRegister(CtUsbDevType devType);


/**
 * 解注册所有感兴趣的设备类型
 * @return 错误码，0表示成功，-1表示失败
 */
int ctSgw_usbUnregister(void);


/**
 * 锁定USB设备
 * @param devId(IN) USB设备标识
 * @return 错误码，0表示成功，-1表示失败
 */
int ctSgw_usbLock(int devId);


 /**
 * 解锁USB设备
 * @param devId(IN) USB设备标识
 * @return 错误码，0表示成功，-1表示失败
 */
int ctSgw_usbUnlock(int devId);


/**
 * 打开USB设备
 * @param devId(IN) USB设备标识
 * @param handle(OUT) 返回的USB设备操作句柄
 * @return 错误码，0表示成功，-1表示失败
 */
int ctSgw_usbOpen(int devId, int *handle);


/**
 * 检查USB设备中是否有数据可以读取
* @param handle(IN) USB设备操作句柄
* @return 是否存在数据可读，0表示没有数据可读， 1表示存在数据可读
 */
int ctSgw_usbDataAvailable(int handle);


/**
 * 从USB设备中读数据
 * @param handle(IN) USB设备操作句柄
 * @param buf(OUT) 返回的读取数据
 * @param count(IN/OUT) 需要读取的数据长度/实际读取的数据长度
* @param timeout (IN) 等待数据可用的超时时间，单位为毫秒，0表示不等待，-1表示一直等待
 * @return 错误码，0表示成功，-1表示失败，-2表示超时
 */
int ctSgw_usbRead(int handle, void *buf, size_t *count, int timeout);


/**
 * 向USB设备中写数据
 * @param handle(IN) USB设备操作句柄
 * @param buf(IN) 写入数据
 * @param count(IN/OUT) 需要写入的数据长度/实际写入的数据长度
 * @return 错误码，0表示成功，-1表示失败
 */
int ctSgw_usbWrite(int handle, const void *buf, size_t *count);


/**
 * 配置USB串口属性
 * @param handle(IN) USB设备操作句柄
 * @param serialCfg(IN) 配置参数
 * @return 错误码，0表示成功，-1表示失败
 */
int ctSgw_usbSetSerial(int handle, const CtUsbSerialCfg *serialCfg);


/**
 * 关闭USB设备
 * @param handle(IN) USB设备操作句柄
 * @return 错误码，0表示成功，-1表示失败
 */
int ctSgw_usbClose(int handle);


/**
 * 上报事件到平台
 * @param event(IN) 需要上报的事件信息，以JSON格式表示
 * @return 错误码，0表示成功，-1表示失败
 */
int ctSgw_eventInform(const char *event);


/*
函数说明：该函数用于在每一个会话创建时挂载DPI相关的上下文信息，创建会话时调用该接口；
参数说明：p_dpi_ctx(IN)，指向挂载在会话结构体中指针的地址；
                 flag(IN)， 备用，目前必须置为0；
返回值：0    正确
              < 0 错误
*/
int ctSgw_dpiCtxCreate(void** p_dpi_ctx, unsigned char flag);
/*


函数说明：该函数用于在每一个会话销毁时释放DPI相关的上下文信息，销毁会话时调用该接口；
参数说明：p_dpi_ctx(IN)，指向挂载在会话结构体中指针的地址；
返回值：无
*/
void ctSgw_dpiCtxDestroy(void** p_dpi_ctx);
 

/*
函数说明：该函数用于在会话有新报文到达时，调用该接口，执行DPI主流程；
参数说明：layer2data,(IN) 新到达报文的Layer2以上的payload
                 tupleinfo(IN), 报文对应会话包含的五元组信息和方向，以及出入接口信息，参见结构体CtSgwTupleInfo定义
                 dpi_ctx(IN)，挂载在报文所对应会话结构体中指针；
                 layer7_id(OUT), 当返回值为MAXNET_DPI_FIN时，该值表示DPI匹配的结果
返回值： 0， 表示DPI未完成，对应会话有新报文到达时，需要继续调用本接口
               1， 表示DPI完成，对应会话后续报文无需调用本接口    
*/
int ctSgw_dpiProcAppId(unsigned char *layer2data, CtSgwTupleInfo *tupleinfo, void *dpi_ctx, unsigned int *layer7_id);
 


/*
函数说明：该函数用于在DPI引擎中调用，在报文处理主流程中注册相关的处理函数
参数说明：funcs,(IN) 注册处理函数的结构体
返回值：   0， 成功
          -1， 失败               
*/
int ctSgw_appRegisterFunc(ctSgw_dpiFuncs  *funcs);


/*
函数说明：该函数用于在DPI引擎中调用，在报文处理主流程中注销相关的处理函数
参数说明：无 
返回值：   None
*/
void ctSgw_appUnRegisterFunc(void);
#endif

