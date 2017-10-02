#ifndef COMMON_H
#define COMMON_H

#include "TXDeviceSDK.h"


/**************************************************************************************
 * 局域网的请求和响应 结构体定义
 **************************************************************************************/
struct stop_http_server_req
{
	std::string skey;
	int port;
	stop_http_server_req() {
		port = 0;
	}
};

struct stop_http_server_rsp
{
	int port;
	stop_http_server_rsp() {
		port = 0;
	}
};

struct http_server_req
{
	int type;
	int port;
	http_server_req() {
		type = 0;
		port = 0;
	}
};

struct http_server_rsp
{
	int type;  //根据type来设置返回的json格式
};


/**************************************************************************************
 * 文件下载的请求和响应 结构体定义
 **************************************************************************************/
struct file_dl_item
{
	std::string path;
	int type;
	std::string task_key; //PC生成的task_key
	bool result;              //是否处理成功
	file_dl_item(const std::string& p, int t, std::string k) {
		path = p;
		type = t;
		task_key = k;
		result = false;
	}
	file_dl_item() {
		type = 0;
		result = false;
	}
};

struct file_dl_req
{
	std::vector<file_dl_item> dl_item;
	unsigned long long target_id;
	file_dl_req() {
		target_id = 0;
	}
};

struct file_dl_rsp
{
	unsigned long long cookie;
	file_dl_rsp() {
		cookie = 0;
	}
};


/**************************************************************************************
 * 取消文件传输的请求和响应 结构体定义
 **************************************************************************************/
struct cancel_file_transfer_pair
{
	std::string task_key;
	bool result;
	cancel_file_transfer_pair()
	{
		result = false;
	}
	cancel_file_transfer_pair(const cancel_file_transfer_pair& p)
	{
		task_key = p.task_key;
		result = p.result;
	}
};

struct cancel_file_transfer_req
{
	std::string task_key; //
	std::vector<cancel_file_transfer_pair> task_key_list;
};

struct cancel_file_transfer_rsp
{
	//std::string task_key; //
};

struct file_transfer_detail_req
{
	std::string task_key; //
};

struct file_transfer_detail_rsp
{
	//std::string task_key; //
};

/**************************************************************************************
 * 任务公共datapoint的请求和响应 结构体定义
 **************************************************************************************/

struct task_req
{
	unsigned int subid;
	task_req(){
		subid = 0;
	}
};

struct task_rsp
{
};

/**************************************************************************************
 * 账号公共datapoint的请求和响应 结构体定义
 **************************************************************************************/

struct account_req
{
	unsigned int subid;
	account_req(){
		subid = 0;
	}
};

struct account_rsp
{
};


struct login_req
{
	std::string usr;
	std::string pwd;
	std::string extra_data;
};

struct login_rsp
{
	std::string sid;
};

struct logout_req
{
	std::string extra_data;
};

struct logout_rsp
{
	// empty
};

struct check_login_req
{
	std::string extra_data;
};

struct check_login_rsp
{
	bool is_login;
	std::string usr;
	check_login_rsp() {
		is_login = false;
	}
};



/**************************************************************************************
 * 
 **************************************************************************************/
struct ftn_file_upload_buffer_with_file
{
	std::string _task_key;
	std::string _dest_path;
};

//文件类型定义
typedef enum enum_file_type_def {
	file_type_def_rich_lite = -2, // 图片，音频，视频,(常见)
	file_type_def_rich = -1, // 图片，音频，视频
	file_type_def_doc = 1, //文档
	file_type_def_pic = 2, //图片
	file_type_def_audio = 3, //音频
	file_type_def_video = 4, //视频
	file_type_def_file = 5, //文件
	file_type_def_dir = 6, //目录
	file_type_def_other = 0 //其他文件
}file_type_t;

//缩略图格式定义
typedef enum enum_thumb_type_def {
	thumb_type_def_jpg = 1, //jpg
	thumb_type_def_png = 2, //png
	thumb_type_def_bmp = 3, //bmp
	thumb_type_def_other = 0, //其他
}thumb_type_t;

//返回给H5页面的错误码
typedef enum enum_device_error_code {
	device_error_code_success = 0, //成功
	device_error_code_unknown_property_id = -1, //未知的属性id
	device_error_code_request_invalid = -2, //请求非法
	device_error_code_get_list = -3, //获取文件列表失败
	device_error_code_file_not_exist = -4, //文件不存在
	device_error_code_upload_failure = -5, //上传文件到文件通道失败
	device_error_code_login_failure = -6, //用户名或者密码错误导致登录失败
	device_error_code_logout_failure = -7, //用户未登录，无法等出用户
	device_error_code_file_exist = -8, //文件已存在

	device_error_code_part_success = -9998, //操作部分成功，例如批量下载和批量删除
	device_error_code_not_login = -9999, //用户未登陆
	device_error_code_not_impl = -10000, //未实现
	device_error_code_unknown_error = -10001, //未知错误
}device_error_code_t;


//绑定者的基本信息
typedef struct binder_info {
	tx_binder_info* _binder_list;
	int _count;
	DU_ThreadLock _lock;
	binder_info() {
		_binder_list = NULL;
		_count = 0;
	}

	void free() {
		if(_binder_list != NULL) {
			delete [] _binder_list;
			_binder_list = NULL;
			_count = 0;
		}
	}

	void dup(const tx_binder_info* bf, int count) {
		DU_ThreadLock::Lock lock(_lock);
		//先清理上一次的内容
		free();
		if(count > 0 && bf != NULL) {
			_binder_list = new tx_binder_info[count];
			memcpy(_binder_list, bf, count * sizeof(tx_binder_info));
			_count = count;
		}
	}

	//根据tinyid获取uin信息
	unsigned long long get_uin_by_tinyid(unsigned long long tinyid) {
		unsigned long long uin = 0;

		for(int i = 0; i < _count; ++i) {
			if(_binder_list[i].tinyid == tinyid) {
				uin = _binder_list[i].uin;
			}
		}

		return uin;
	}
} binder_info_t;



#endif // COMMON_H
