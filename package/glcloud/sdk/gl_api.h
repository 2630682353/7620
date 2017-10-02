#ifndef __GL_API_H__
#define __GL_API_H__

typedef enum {
    /* 发送透传数据到app端，参见gl_data_t */
    GL_MSG_DATA_TO_APP,
#if 0
    /* 发送透传数据到app端（通过局域网），参见gl_data_t */
    GL_MSG_DATA_TO_APP_LAN,
    /* 发送透传数据到云端，参见gl_data_t */
    GL_MSG_DATA_TO_CLOUD,
    /* 发送文件到app端，参见gl_file_t */
    GL_MSG_FILE_TO_APP,
#endif
    /* 完成文件传输信息，推送到手机APP，参考 gl_file_info_t */
    GL_SEND_FIN_FILE_INFO,
    /* 文件上传的进度，单位是百分比，推送到手机APP，参考 gl_file_percent_t */
    GL_SEND_FILE_PERCENT,
    /* 向云端发送请求token的消息, 参数为空 */
    GL_SEND_REQUEST_TOKEN,
    /* 向服务器查询最新版本固件，无需参数 */
    GL_FOTA_GET_INFO,
    /* 开始下载最新的固件，无需参数 */
    GL_FOTA_DOWNLOAD,
    /* 清空设备所有本地和云端的数据，在设备恢复出厂设置时调用此接口，无需参数 */
    GL_CTRL_RESET_ALL,
    /* 开始文件传输信息，推送到手机APP，参考 gl_file_info_t */
    GL_SEND_START_FILE_INFO,
} gl_send_type_e;

typedef enum {
    /* 收到从app端发送过来的透传数据，参见gl_data_t */
    GL_MSG_DATA_FROM_APP,
    /* 收到从app端发送过来的透传数据（通过局域网），参见gl_data_t */
    GL_MSG_DATA_FROM_APP_LAN,
#if 0
    /* 收到从云端发送过来的透传数据，参见gl_data_t */
    GL_MSG_DATA_FROM_CLOUD,
    /* 收到从app端的共享，参见gl_file_t */
    GL_MSG_FILE_FROM_APP,
#endif
    /* 接收到的文件信息，从APP推送到设备 */
    GL_RECV_FILE_INFO,
    /* 接收到的请求设备上传文件的进度，APP发起请求，不携带任何参数 */
    GL_RECV_FILE_PERCENT,
    /* 接收到上传文件的token值 */
    GL_RECV_UPLOAD_TOKEN,
    /* 固件升级信息，参见gl_fota_info_t */
    GL_FOTA_FW_INFO,
    /* 所下载的固件文件，参见gl_fota_data_t */
    GL_FOTA_FW_DATA,
    /* 所下载的固件文件的校验结果，参见gl_fota_result_t */
    GL_FOTA_RESULT,
    /* 收到此消息说明清空设备所有本地和云端的数据操作成功，无参数返回 */
    GL_CTRL_RESET_ALL_ACK,
} gl_recv_type_e;

/* SDK初始化所需信息 */
typedef struct {
    char *imei;             /* imei和pin用于唯一标识一个设备，个联提供 */
    char *pin;
    char *product_id;       /* 产品id，个联提供 */
    char *product_name;     /* 产品名称,最大长度为20个字节，即20个字符 */
#if 0
    host = 0:          dt.gl.igexin.com
    host = 0x61463B7B; 123.59.70.97
    host = 0x62463B7B; 123.59.70.98
#endif
    int host;               /* 指定远程服务器，默认填0 */
    char *sw_ver;           /* 设备的当前固件版本，如不使用个联FOTA功能不用设置 */
    int sdk_flash_addr;     /* SDK 本地数据区占用flash的起始地址, 为0则表示不使用该功能*/
    int sdk_flash_size;     /* SDK 本地数据区占用flash的大小， 为0则表示不使用该功能*/
} gl_cfg_t;

typedef struct {
    char *data;             /* 透传的数据*/
    int len;                /* 透传数据长度，长度小于等于1024*/
    int msg_id;             /* 消息id，在接收数据回调函数内发送原路返回数据使用该msg_id */
                            /* 在其他函数调用时请把msg_id置为-1, 否则发送失败 */
                            
    int user_id;            /* 发送/接收该消息的用户，大于等于0表示特定的用户 */
                            /* -1代表所有绑定该设备的用户，-2代表云端 */
} gl_data_t;


typedef enum
{
    GL_FILE_ENCRYPT_NONE,  /* 无加密*/
    GL_FILE_ENCRYPT_AES,   /* AES 加密 */
    GL_FILE_ENCRYPT_END
}gl_file_encrypt_type_e;

typedef enum
{
    GL_FILE_CHECK_NONE,    /* 无校验 */
    GL_FILE_CHECK_MD5,     /* MD5校验方式 */
    GL_FILE_CHECK_CRC32,   /* CRC32校验方式 */
    GL_FILE_CHECK_CRC16,   /* CRC8校验方式 */
    GL_FILE_CHECK_END
}gl_file_check_type_e;

typedef struct
{
    char *file_name;       /* 文件名 */
    char *file_url;        /* 文件在云端的url */
    int  file_size;        /* 文件大小，单位: Byte*/
    char encrypt_type;     /* 文件加密类型，请参考 gl_file_encrypt_type_e */
    char *encrypt_key;     /* 加密key */
    char encrypt_key_len;  /* 加密key的长度 */
    char check_type;       /* 校验类型, 请参考 gl_file_check_type_e*/
    char *check_key;       /* 校验key */
    char check_key_len;    /* 校验key的长度 */
    char *file_key;        /* 完成文件上传后生成的key */
    int  user_id;          /* 发送/接收该消息的用户，大于等于0表示特定的用户 */
                            /* -1代表所有绑定该设备的用户 */
}gl_file_info_t;


typedef struct
{
    char *file_name;       /* 请求文件名 */
    char percent;          /* 上传文件的进度，百分比 */
    int  msg_id;           /* 用于发送到特定的用户，该消息是由APP客户端发起，只发给特定的APP */        
}gl_file_percent_t;

typedef struct
{
    char *token;           /* 请求上传文件的token,以'\0'结束 */
}gl_file_token_t;


typedef struct {
    char *serv_version;     /* 云端最新固件版本 */
    char force_type;        /* 升级类型，1是强制升级，0是由用户来决定 */
    int firmware_size;      /* 固件的大小 */
} gl_fota_info_t;

typedef struct
{
    int   offset;           /* 数据的偏移地址*/
    char *data_buf;         /* 数据buf，指向升级文件的数据 */
    short len;              /* 数据的长度 */
} gl_fota_data_t;

typedef struct
{
    int result;             /* 0表示所下载的固件校验无误，-1表示下载出错 */
} gl_fota_result_t;

/*
 * 描述：用户侧回调函数，接收到app端/云端的透传数据/共享文件等消息会触发此回调
 * 参数：
 *      msg： 所接收到消息的指针，根据type值进行强制转换
 *      type：所接收到消息的类型
 * 返回：无
 */
typedef void (*gl_cb_t)(void *msg, gl_recv_type_e type);

/* 错误码 */
#define GL_OK                   0 /* 成功 */
#define GL_ERR                  1 /* 通用错误 */
#define GL_ESRVNOTCONN          2 /* 尚未连接到个联服务器 */
#define GL_ENOTREGED            3 /* 尚未在个联服务器注册成功 */
#define GL_EPARAM               4 /* 参数错误 */
#define GL_EIWAIT               5 /* igelink: 等待报文 */
#define GL_EISTART              6 /* igelink: 开始接收wifi参数 */
#define GL_EIGOT_SSID_PASSWD    7 /* igelink: 已获得wifi参数 */
#define GL_EICONNECTING         8 /* igelink: 连接目标wifi中 */
#define GL_EICONNECTED          9 /* igelink: 成功连接到目标wifi */
#define GL_EIFAILED             10/* igelink: 超时失败 */

/*
 * 描述：获取SDK版本
 * 参数：无
 * 返回：当前SDK版本
 */
char *gl_version(void);

/*
 * 描述：初始化SDK
 * 参数：
 *      cfg：参见gl_cfg_t
 *      cb：参见gl_cb_t
 * 返回：参见错误码
 */
int gl_init(gl_cfg_t *cfg, gl_cb_t cb);

/*
 * 描述：获取个联SDK的当前状态
 * 参数：无
 * 返回：参见错误码
 */
int gl_state(void);

/*
 * 描述：发送用户数据
 * 参数：
 *      msg：待发送的命令
 *      type：参见gl_send_type_e
 * 返回：参见错误码
 */
int gl_send(void *msg, gl_send_type_e type);


/*
 * 描述：开启igelink功能，请勿重复调用此接口
 *       igelink成功后，自动停止
 * 参数：
 *      aes_key：AES密钥，16bytes，NULL表示不使用AES加密
 *               长度不足16bytes自动补0，大于16截断
 *      chan_dura：信道切换间隔（单位毫秒）
 *                 0代表使用默认间隔100毫秒
 *      timeout：超时时间（单位秒），超过此时间后仍未成功，则停止igelink
 *               0代表不设置超时，即igelink会一直运行直至成功
 * 返回：参见错误码
 */
int gl_igelink_start(const char *aes_key, int chan_dura, int timeout);

/*
 * 描述：获取igelink的当前状态
 * 参数：无
 * 返回：参见错误码
 */
int gl_igelink_state(void);

/*
 * 描述：开启光波配置联网功能，请勿重复调用此接口
 *       配置成功后，自动停止
 * 参数：
 *      aes_key：AES密钥，16bytes，NULL表示不使用AES加密
 *               长度不足16bytes自动补0，大于16截断
 *      timeout：超时时间（单位秒），超过此时间后仍未成功，则停止igelink
 *               0代表不设置超时，即igelink会一直运行直至成功
 * 返回：参见错误码
 */
int gl_igelink_light_wave_start(const char *aes_key, int timeout);

/*
 * 描述：获取光波配置的当前状态
 * 参数：无
 * 返回：参见错误码
 */
int gl_igelink_light_wave_state(void);

typedef enum {
    GL_DC_CHRC_VALUE_SET        = 0, /* 设置属性值 */
    GL_DC_CHRC_VALUE_GET        = 1, /* 读取属性值*/
} chrc_valu_op_type_t;

typedef void (*gl_dev_ctrl_valu_func)(chrc_valu_op_type_t op, void *value);

typedef struct 
{
    unsigned int chrc_id;             /* 属性的id */
    gl_dev_ctrl_valu_func valu_func;  /* 设置和读取属性数据的回调函数 */
    void * const chrc_data;           /* 属性的参数 */
} user_dev_chrc_t;

/*
 * 描述：初始化设备控制
 * 参数：
 *      chrcs：设备属性参数集合
 *      count：属性的个数
 * 返回：参见错误码
 */
int gl_dev_ctrl_init(user_dev_chrc_t *chrcs, int count);

/*
 * 描述：设置属性数据的值
 * 参数：
 *      char_id：设备属性id
 *      value：设备属性数据值
 * 返回：参见错误码
 */
int gl_dev_ctrl_set_value(unsigned int char_id, void *value);

/*
 * 描述：提交设备属性数据到APP
 * 参数：
 *      无
 * 返回：参见错误码
 */
int gl_dev_ctrl_commit_all(void);

#endif /* __GL_API_H__ */

