//
//  file_transfer_processor.cpp
//
//  Created by jiangtaozhu on 15/8/8.
//  Copyright (c) 2015年 jiangtaozhu. All rights reserved.
//


#include "file_transfer_processor.h"
#include "string.h"
#include "file_transfer_record_mgr.h"
#include "file_key_mgr_sdk.h"

#include "TXNasSDKImpl.h"
#include "TXNasSDK.h"


extern task_mgr g_task_mgr;
extern binder_info_t g_binder_list_internal;
extern tx_nas_device_mgr_wrapper*  g_device_mgr_wrapper;

extern void notify_file_transfer_progress(unsigned long long cookie, unsigned long long progress, unsigned long long max_progress);
extern void notify_file_transfer_complete(unsigned long long cookie, const std::string& file_key, int errcode, int status);

void file_transfer_processor::on_process(const tx_file_transfer_info* ft_info, unsigned long long cookie)
{
	if(ft_info == NULL) {
		TERROR("file_transfer_processor::on_process ft_info is null");
		return;
	}

	TNOTICE("process file transfer, transfer_type [%d], bussiness_name [%s], file path [%s], file size [%lld]", ft_info->transfer_type, ft_info->bussiness_name, ft_info->file_path, ft_info->file_size)	;
	
	switch(ft_info->transfer_type)
	{
	case transfer_type_c2c_in:
		{
			c2c_in_process(ft_info, cookie);
			break;
		}
	case transfer_type_c2c_out:
		{
			c2c_out_process(ft_info, cookie);
			break;
		}
	case transfer_type_upload:
		{
			upload_process(ft_info, cookie);
			break;
		}
	case transfer_type_download:
		{
			download_process(ft_info, cookie);
			break;
		}
	default:
		{
			TERROR("unknown transfer type=%d, cookie=%llu",ft_info->transfer_type, cookie); 
			break;
		}
	}
}


int file_transfer_processor::upload_process(const tx_file_transfer_info* ft_info, unsigned long long cookie)
{
	if(ft_info == NULL) return -1;

	TNOTICE("file_transfer_processor[upload]: bussiness_name [%s], file path [%s], file size [%lld]",  ft_info->bussiness_name, ft_info->file_path, ft_info->file_size);

	task_mgr::upload_cookie_param_t* param = g_task_mgr.get_upload_task(cookie);
	if(param == NULL)
	{
		TERROR("find param by cookie failure, cookie = %llu\n", cookie);
		return -1;
	}

	//如果不是小文件通道，那么就退出
	if(param->_transfer_type != transfer_channeltype_MINI) {
		TNOTICE("FTN transfer comolete, skip it\n");
		//需要删除掉任务信息
		g_task_mgr.rm_upload_task(cookie);
		return 0;
	}

	//根据key获取url
	char download_url[1024] = {0};
	tx_get_minidownload_url((char*)(std::string(ft_info->file_key, ft_info->key_length)).c_str(), ft_info->file_type, (char*)download_url);
	TNOTICE("Get image download url = %s\n", download_url);

	//计算文件的md5
	std::string file_md5 = DU_MD5::md5str(DU_File::load2str(param->_abs_path));
	//先将key存起来
	FILE_UPLOAD_KEY_MGR_SDK->add_file_upload_key(param->_filekey,param->_abs_path, download_url, file_md5);
	//TNOTICE("Debug: --> file_key:%s\n",param->_filekey.c_str());
	if(param->_pre_upload) {
		TNOTICE("Get uploaded thumbnail URL success ! file = %s\n", param->_abs_path.c_str());
		g_task_mgr.rm_upload_task(cookie);
		return 0;
	}

	if (param->_id > 0) {
		//然后发送DataPoint消息给设备端
		StringBuffer sb;
		Writer<StringBuffer> writer(sb);

		writer.StartObject();
		writer.String("ret");
		writer.Int(0);
		writer.String("url");
		writer.String(download_url);
		writer.String("path");
		if(param->_relative_path.empty())     writer.String("");
		else                                           		writer.String(param->_relative_path.c_str());
		writer.EndObject();

		std::string json_rsp = sb.GetString();

		TNOTICE("picture preview response = %s\n", json_rsp.c_str());

		//发送出去

		SendOperationMgr::getInstance()->AddAckDataPoint(param->_from_client,param->_id,param->_seq,0,json_rsp);

		//删除cookie list
		g_task_mgr.rm_upload_task(cookie);
	}
	return 0;
}

int file_transfer_processor::download_process(const tx_file_transfer_info* ft_info, unsigned long long cookie)
{
	if(ft_info == NULL) return -1;

	TNOTICE("file_transfer_processor[download]: bussiness_name [%s], file path [%s], file size [%lld]",  ft_info->bussiness_name, ft_info->file_path, ft_info->file_size);

	TNOTICE("call c2c_in_process");
	c2c_in_process(ft_info,cookie);

	return 0;
}

int file_transfer_processor::c2c_in_process(const tx_file_transfer_info* ft_info, unsigned long long cookie)
{
	if(ft_info == NULL) return -1;

	TNOTICE("file_transfer_processor[c2c_in]: bussiness_name [%s], file path [%s], file size [%lld]",  ft_info->bussiness_name, ft_info->file_path, ft_info->file_size);

	int ret = 0;
	file_transfer_record* record = file_transfer_record_mgr::getInstance()->get_transfer_record(cookie);
	if(record == NULL) {
		TERROR("get file transfer record failure, cookie = %llu\n", cookie);
		notify_file_transfer_complete(cookie,ft_info->file_key,transfer_error_code_not_found,transfer_status_error); // 任务结束2，出错998
		//return;   //file_in_come 中，如果是NFC模式，会没有任务task_key，所以不能直接返回
	}

	std::string buff_with_file(ft_info->buff_with_file, ft_info->buff_length);
	//TNOTICE("file_transfer_processor[c2c_in]: buff_with_file [%s]", buff_with_file.c_str()); 

	//std::string file_name = get_file_name(ft_info->file_path);

	unsigned long long sender = g_task_mgr.get_c2c_in_task(cookie);

	if(g_device_mgr_wrapper && g_device_mgr_wrapper->notify_user_check_login)	{

		char user[512] = {0};
		int  nLogin = g_device_mgr_wrapper->notify_user_check_login(sender,NULL,user,512);
		if(nLogin != 0){
			unlink(ft_info->file_path);
			TERROR("need login, file = %s, tinyid = %llu\n", ft_info->file_path, sender);
			notify_file_transfer_complete(cookie,ft_info->file_key,transfer_error_code_not_login,transfer_status_error);
			if(record) record->set_transfer_error(transfer_error_code_not_login);
			return -1;
		}

	}

	//判断请求中有没有带dest
	std::string dest_path;

	Document req;
	req.Parse(buff_with_file.c_str());
	if (!req.HasParseError()
		&& req.HasMember("dest_path")
		&& req["dest_path"].IsString())
	{
		dest_path = req["dest_path"].GetString();
	}

	int file_type = file_type_file;
	std::string strFileType;

	if (strcmp(ft_info->bussiness_name,"VideoMsg") == 0) 		        { file_type = file_type_video;  strFileType = "VideoMsg"; }			// 来自手机QQ的视频留言
	else if (strcmp(ft_info->bussiness_name,"AudioMsg") == 0) 		{ file_type = file_type_audio;  strFileType = "AudioMsg"; }			// 来自手机QQ的语音留言
	else if (strcmp(ft_info->bussiness_name,"ImgMsg") == 0) 		    { file_type = file_type_pic;      strFileType = "ImgMsg"; }			//来自手机QQ的图片消息
	else if(strcmp(ft_info->bussiness_name,"8000-NASDevMusicFile") == 0)			 { file_type = file_type_music;	strFileType = "8000-NASDevMusicFile";}			//手Q向设备发音乐      （音乐文件）
	else if(strcmp(ft_info->bussiness_name,"8001-NASDevVideoFile") == 0)			 { file_type = file_type_video;		strFileType = "8001-NASDevVideoFile";}			//手Q向设备发视频(较大的文件)
	else if(strcmp(ft_info->bussiness_name,"8002-NASDevDocumentFile") == 0)	{ file_type = file_type_doc;		strFileType = "8002-NASDevDocumentFile";}	//手Q向设备发文档
	else if(strcmp(ft_info->bussiness_name,"8003-NASDevCommonFile") == 0)	{ file_type = file_type_file;			strFileType = "8003-NASDevCommonFile";}		//手Q向设备发其他文件
	else                                                                                               { file_type = file_type_file;     strFileType = "FileMsg"; }				//其他文件

	TNOTICE("file_transfer_processor[c2c_in]: sender[%llu] src[%s] dest_path[%s] file_type[%d %s]",sender, ft_info->file_path, (dest_path.empty() ? "" : dest_path.c_str()), file_type, strFileType.c_str());

	//调用用户回调，保存文件
	int mgr_ret = 0;
	if(g_device_mgr_wrapper && g_device_mgr_wrapper->notify_file_download_complete){
		mgr_ret = (g_device_mgr_wrapper->notify_file_download_complete)(sender,ft_info->file_path,file_type,(dest_path.empty() ? NULL : dest_path.c_str()));
	}

	//删除临时文件
	{
		DU_File::removeFile(ft_info->file_path,false);
	}

	notify_file_transfer_complete(cookie,ft_info->file_key,transfer_error_code_success,transfer_status_finish);

	if(record != NULL){ if(mgr_ret) record->set_transfer_error(transfer_error_code_failed);	else record->set_transfer_end(ft_info->file_key);}
	return ret;
}

int file_transfer_processor::c2c_out_process(const tx_file_transfer_info* ft_info, unsigned long long cookie)
{
	if(ft_info == NULL) return -1;
	TNOTICE("file_transfer_processor[c2c_out]: bussiness_name [%s], file path [%s], file size [%lld]",  ft_info->bussiness_name, ft_info->file_path, ft_info->file_size);

	//如果是对外发送文件，需要删除生成的临时文件
	if(strcmp(ft_info->bussiness_name, "7001-NASDevPushThumb") == 0) {
		TNOTICE("unlink temp file, path = %s\n", ft_info->file_path);
		unlink(ft_info->file_path);
	}

	file_transfer_record* record = file_transfer_record_mgr::getInstance()->get_transfer_record(cookie);
	if(record == NULL) {
		TERROR("get file transfer record failure, cookie = %llu\n", cookie);
		notify_file_transfer_complete(cookie,ft_info->file_key,transfer_error_code_not_found,transfer_status_error); // 任务出错
		//return;
	}

	notify_file_transfer_complete(cookie,ft_info->file_key,transfer_error_code_success,transfer_status_finish);
	if(record) record->set_transfer_end(ft_info->file_key);

	return 0;
}
