#ifndef NAS_SDK_H
#define NAS_SDK_H

#include "TXSDKCommonDef.h"
#include "TXDeviceSDK.h"
#include "TXDataPoint.h"
#include "TXNasSDKCommDef.h"

CXX_EXTERN_BEGIN

/**
* 注册设备相关的回调函数
*/
typedef struct tag_tx_device_mgr_wrapper
{
	// 相对路径转换为绝对路径
	// from_client：当前的用户tinyid
	// relative_path：文件相对路径
	// abs_path：用来存放转换后的绝对路径
	// len： abs_path的缓存大小（字节）
	// 返回值：成功返回0，是否返回非0
	int (* relative2abspath)(unsigned long long from_client, const char* relative_path, char* abs_path, int len);

	// 文件下载完成的回调，用户可保存文件到自定义位置
	// from_client: 用户的tinyid
	// tmp_file_path:  为原始文件缓存的相对地址
	// file_type：  文件的类型，file_type_pic、file_type_audio、file_type_video、file_type_file、file_type_other
	// specified_dst_dir:  指定该文件需要放到的目录（相对地址）；如果未指定，即该参数为NULL，用户可自由决定文件存放位置
	// 返回值： 成功返回0, 失败返回非0
	int (* notify_file_download_complete)(unsigned long long from_client, const char* tmp_file_path, int file_type , const char* specified_dst_dir);

	// 用户的登陆通知
	// tinyid：请求登录用户的tinyid
	// user ：用户名
	// pwd：密码
	// extra_data： 用户请求登陆时，发送过来的附加的数据，如果不存在extra_data，会设置为NULL
	// 返回值： 登陆成功返回0，登陆失败返回1
	int (* notify_user_login)(unsigned long long tinyid,const char* user, const char* pwd, const char* extra_data);

	// 用户的登出通知
	// tinyid： 要登出的用户的tinyid
	// extra_data：用户请求登出时，发送过来的附加的数据，如果不存在extra_data，会设置为NULL
	// 返回值：如果登出成功返回0，登出失败返回1
	int (* notify_user_logout)(unsigned long long tinyid, const char* extra_data);

	//用户的检查登陆通知
	// tinyid：用户的tinyid
	// extra_data：用户请求检查登陆状态时，发送过来的附加的数据，如果不存在extra_data，会设置为NULL
	// user : 如果已登录，user返回用户名，user缓存空间在调用该函数时已分配
	// user_len: user 缓存已分配的大小
	// 返回值：如果已登录返回0，未登录或出错返回非0
	int (* notify_user_check_login)(unsigned long long tinyid,const char* extra_data, char* user,int user_len);

}tx_nas_device_mgr_wrapper;


/**
* 初始化设备SDK
* 参数同tx_init_device
*/
SDK_API int tx_nas_init_device(tx_device_info *info, tx_device_notify *notify, tx_init_path* init_path); 


/**
* 初始化data point 处理
* 参数同tx_init_data_point
*/
int tx_nas_init_data_point(const tx_data_point_notify *notify);

/**
* 初始化文件传输
* 参数含义同tx_init_file_transfer。【！注意参数类型是指针！】
*/
SDK_API int tx_nas_init_file_transfer(tx_file_transfer_notify* notify, char * path_recv_file); 

/**
* 初始化NasSDK
*/
SDK_API int tx_nas_init(tx_nas_device_mgr_wrapper* device_mgr_wrapper); 

/**
* 退出NasSDK
*/
SDK_API int tx_nas_uninit();


/**
* 上传文件到服务器。上传成功后，可通过tx_nas_get_url获取文件的url
* abspath：文件的绝对地址
* filekey： 文件的filekey，后续可以通过该filekey来获取图片的url
*/
SDK_API int tx_nas_upload_file(const char* abspath,const char* filekey);


/**
* 获取文件的url
* filekey：上传文件时传入的filekey
* url： 用于返回url，由用户分配
* len： url缓存的大小（字节）
*/
SDK_API int tx_nas_get_url(const char* filekey, char* url, int len);

/**
* 删除filekey指向的文件，删除后，无法再通过tx_nas_get_url 获取其url
* filekey：上传文件时传入的filekey
*/
SDK_API int tx_nas_delete_uploaded_file(const char* filekey);


CXX_EXTERN_END

#endif // UTILS_H
