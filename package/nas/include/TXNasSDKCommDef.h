#ifndef NAS_SDK_COMM_DEF_H
#define NAS_SDK_COMM_DEF_H

#include <unistd.h>
#include <iostream>
#include "stdio.h"

#define SDK_API                     __attribute__((visibility("default")))
#define CXX_EXTERN_BEGIN         extern "C" {
#define CXX_EXTERN_END           }

#define NET_TYPE_SERVER 0    //服务器
#define NET_TYPE_LAN 1       //局域网


//文件类型定义
typedef enum enum_file_type_def_internal {
	file_type_rich_lite = -2, // 图片，音频，视频,(常见)
	file_type_rich = -1, // 图片，音频，视频
	file_type_doc = 1, //文档
	file_type_pic = 2, //图片
	file_type_audio = 3, //音频  (语音)
	file_type_video = 4, //视频
	file_type_file = 5, //文件
	file_type_dir = 6, //目录
	file_type_music = 7, //音乐
	file_type_other = 0 //其他文件
}nas_file_type;


struct NasFile{
	char path[1024];		 //文件路径
	char name[512];		//文件名
	int type;					//文件类型，nas_file_type
	int isdir;					//是否是目录文件, 0 不是，1 是
	uint64_t size;			//文件大小，单位byte
	uint32_t time;
};

//全局错误码表
enum nas_error_code
{
	nas_err_file_not_exist							= 0x00080001, //524289 文件不存在
	nas_err_invalid_file_transfer_tmp_path	= 0x00080002, //524290 初始化tx_init_file_transfer时，传入的临时文件夹非法
};

#endif // UTILS_H
