//
//  data_point_processor.cpp
//
//  Created by jiangtaozhu on 15/8/8.
//  Copyright (c) 2015年 jiangtaozhu. All rights reserved.
//


#include "data_point_processor.h"
#include "string.h"
#include "TXNasSDK.h"

extern char* g_root;
extern bool g_bLogin;
extern DU_ThreadPool g_ThreadPool; 

//失败响应
void create_failed_response(int ret, const char* err_msg, tx_data_point* req_data_point, tx_data_point* rsp_data_point)
{
	if(rsp_data_point == NULL) { //如果为空，那么就不做任何处理
		return;
	}
	rsp_data_point->id = req_data_point->id;
	rsp_data_point->seq = req_data_point->seq;
	rsp_data_point->ret_code = ret;

	//value
	char buf[256] = {0};
	snprintf(buf,256, "{\"ret\":%d,\"msg\":\"%s\"}", ret, err_msg); //返回Json格式的数据

	int buf_len = strlen(buf);
	rsp_data_point->value = new char[buf_len+1];     
	memset(rsp_data_point->value,0,buf_len+1);
	memcpy(rsp_data_point->value, buf, buf_len);
	TNOTICE("FAILED: id[%d] seq[%d] value[%s]\n",rsp_data_point->id,rsp_data_point->seq,rsp_data_point->value);
}

//成功响应
void create_success_response(const char* value, tx_data_point* req_data_point, tx_data_point* rsp_data_point)
{
	if(rsp_data_point == NULL) { //如果为空，那么就不做任何处理
		return;
	}
	rsp_data_point->id = req_data_point->id;
	rsp_data_point->seq = req_data_point->seq;
	rsp_data_point->ret_code = 0;

	//value
	int len = strlen(value);
	rsp_data_point->value = new char[len+1];
	memset(rsp_data_point->value,0,len+1);
	memcpy(rsp_data_point->value, value, len);

	TNOTICE("SUCCESS: id[%d] seq[%d] value[%s]\n",rsp_data_point->id,rsp_data_point->seq,rsp_data_point->value);
}


int data_point_processor::on_process(unsigned long long from_client, tx_data_point *req_dp, tx_data_point* rsp_dp)
{
	if(req_dp == NULL || rsp_dp == NULL) {
		return -1;
	}

	TNOTICE("from_client=%lld, id=%d, seq=%d, val=%s ",from_client, req_dp->id, req_dp->seq,req_dp->value);

	int ret = 0;
	if(req_dp->id == 100001788){           //文件列表
		ret = process_datapoint_file_list(from_client,req_dp,rsp_dp);
	}
	else if (req_dp->id == 100001789){   //删除文件
		ret = process_datapoint_remove_file(from_client,req_dp,rsp_dp);
	}
	else if (req_dp->id == 100001791){		//分类列表
		ret = process_datapoint_type_list(from_client,req_dp,rsp_dp);
	}else if (req_dp->id == 100001792){		//图片预览
		ret = process_datapoint_image_preview(from_client,req_dp,rsp_dp);
	}
	else {
        TNOTICE("from_client=%lld, data_point__id=%d, data_point_value=%s, unknown datapoint id.",from_client, req_dp->id, req_dp->value);
		create_failed_response(-1,"非法的消息",req_dp,rsp_dp);
    }
	return ret;
}

//文件列表
int data_point_processor::process_datapoint_file_list(unsigned long long from_client, tx_data_point *req_dp,tx_data_point* rsp_dp)
{
	//1. 判断是否登陆
	if(!g_bLogin){
		TERROR("tinyid = %llu is not login\n",from_client);
		create_failed_response(-9999,"请重新登录",req_dp,rsp_dp);
		return -1;
	}
	
	//2. 解析请求
	file_list_req request; 
	if( parse_file_list_request(req_dp, request) != 0 ) {
		create_failed_response(-1,"解析请求失败",req_dp,rsp_dp);
		return -1;
	}

	//3. 处理： 生成文件列表
	file_list_rsp response;
	if(impl_file_list(request,response) != 0){
		create_failed_response(-1,"获取文件列表失败",req_dp,rsp_dp);
		return -1;
	}

	//4. 按照指定格式，生成并返回数据
	std::string json_rsp;
	format_file_list_response(json_rsp,request,response);
	create_success_response(json_rsp.c_str(),req_dp,rsp_dp);

	return 0;
}

//删除文件
int data_point_processor::process_datapoint_remove_file(unsigned long long from_client, tx_data_point *req_dp,tx_data_point* rsp_dp)
{
	
	//1. 判断是否登陆
	if(!g_bLogin){
		TERROR("tinyid = %llu is not login\n",from_client);
		create_failed_response(-9999,"请重新登录",req_dp,rsp_dp);
		return -1;
	}

	//2. 解析请求
	file_rm_req request; 
	if( parse_remove_file_request(req_dp, request) != 0 ) {
		create_failed_response(-1,"解析请求失败",req_dp,rsp_dp);
		return -1;
	}

	//3. 处理： 生成文件列表
	file_rm_rsp response;

	int ret = impl_remove_file(request,response);

	//4. 按照指定格式，生成并返回数据
	std::string json_rsp;
	format_remove_file_response(ret, json_rsp,request,response);
	create_success_response(json_rsp.c_str(),req_dp,rsp_dp);
	
	return 0;
}

//分类列表
int data_point_processor::process_datapoint_type_list(unsigned long long from_client, tx_data_point *req_dp,tx_data_point* rsp_dp)
{
	//1. 判断是否登陆
	if(!g_bLogin){
		TERROR("tinyid = %llu is not login\n",from_client);
		create_failed_response(-9999,"请重新登录",req_dp,rsp_dp);
		return -1;
	}
	
	//2. 解析请求
	type_list_req request; 
	if( parse_type_list_request(req_dp, request) != 0 ) {
		create_failed_response(-1,"解析请求失败",req_dp,rsp_dp);
		return -1;
	}

	//3. 处理： 生成分类列表
	type_list_rsp response;
	if(impl_type_list(request,response) != 0){
		create_failed_response(-1,"获取分类列表失败",req_dp,rsp_dp);
		return -1;
	}

	//4. 按照指定格式，生成并返回数据
	std::string json_rsp;
	format_type_list_response(json_rsp,request,response);
	create_success_response(json_rsp.c_str(),req_dp,rsp_dp);

	return 0;
}

//图片预览
int data_point_processor::process_datapoint_image_preview(unsigned long long from_client, tx_data_point *req_dp,tx_data_point* rsp_dp)
{
	//1. 判断是否登陆
	if(!g_bLogin){
		TERROR("tinyid = %llu is not login\n",from_client);
		create_failed_response(-9999,"请重新登录",req_dp,rsp_dp);
		return -1;
	}

	//2. 解析请求
	image_preview_req request; 
	if( parse_image_preview_request(req_dp, request) != 0 ) {
		create_failed_response(-1,"解析请求失败",req_dp,rsp_dp);
		return -1;
	}

	//3. 处理： 生成图片URL
	image_preview_rsp response;
	if(impl_image_preview(request,response) != 0){
		create_failed_response(-1,"获取图片URL失败，请检查文件是否存在，然后重试",req_dp,rsp_dp);
		return -1;
	}

	//4. 按照指定格式，生成并返回数据
	std::string json_rsp;
	format_image_preview_response(json_rsp,request,response);
	create_success_response(json_rsp.c_str(),req_dp,rsp_dp);

	return 0;
}


//线程函数：图片上传
void thread_func_upload_file(std::vector<std::string> imgs)
{
	for (unsigned int i=0; i<imgs.size(); ++i)	{
		tx_nas_upload_file(imgs[i].c_str(), imgs[i].c_str()); //直接以路径作为key，上传的过程是异步进行的
	}
}


/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//文件列表

int data_point_processor::parse_file_list_request(tx_data_point* req_dp, file_list_req& req)
{
/*
	请求格式示例
	{
		"path": "/",
		"page": 1,
		"count": 10
	}

	请求参数说明
	path:		要查看文件列表的目录
	page:		分页，从0开始。第0页包含了所有的文件夹和1页文件列表。
	count:		要返回的文件数量。暂时未用。
*/

	std::string req_json(req_dp->value);
	const char* json = req_json.data();

	Document req_d;
	req_d.Parse(json);
	if(req_d.HasParseError()){
		return -1;
	}

	if (!(req_d.HasMember("path") && req_d["path"].IsString()
		&& req_d.HasMember("page") && req_d["page"].IsInt())) {
			TERROR("json is not satisfied fmt. json=%s\n",json);
			return -1;
	}

	if(req_d.HasMember("count") && req_d["count"].IsInt()){
		req.count = req_d["count"].GetInt();
	}else{
		req.count = 100;
	}

	req.path = req_d["path"].GetString();
	req.page = req_d["page"].GetInt();

	return 0;
}

//目录
int select_dir(const dirent * r)
{
	int ret = 0;
	if(r->d_name[0] != '.'
		&& r->d_type == DT_DIR) {
			//非隐藏文件才显示
			ret = 1;
	}
	return ret;
}

//普通文件
int select_file(const dirent * r)
{
	int ret = 0;
	if(r->d_name[0] != '.'
		&& r->d_type == DT_REG) {
			//非隐藏文件才显示
			ret = 1;
	}
	return ret;
}

int data_point_processor::impl_file_list(file_list_req& req, file_list_rsp& rsp)
{
	//示例代码： 返回目录下所有文件夹和文件（忽略分页，您可以根据req.page进行分页返回）

	std::string abs_path = g_root + req.path + "/";
	TNOTICE("abs_path= %s",abs_path.c_str());

	//扫描目录下的所有文件夹
	{
		std::vector<std::string> dirs;
		DU_File::scanDir(abs_path,dirs,select_dir);
		for(std::vector<std::string>::iterator it = dirs.begin();	it != dirs.end();	++it) {
			NasFile item;
			std::string path =  abs_path + *it;
			std::string name = DU_File::extractFileName(*it);
			snprintf(item.path,1024,"%s",path.c_str());
			snprintf(item.name,512,"%s",name.c_str());
			item.isdir = 1;
			item.type = file_type_dir;
			item.size = 0;
			item.time = DU_File::getModifyTime(path);
			
			file_list_item list_item;
			list_item.file = item;
			list_item.thumb_url = "";

			rsp.buf.push_back(list_item);
		}
	}
	std::vector<std::string> vecImageToUpload;
	//扫描目录下的所有文件
	{
		std::vector<std::string> files;
		DU_File::scanDir(abs_path,files,select_file);
		for(std::vector<std::string>::iterator it = files.begin();	it != files.end();	++it) {
			NasFile item;
			std::string file_abs_path =  abs_path + *it;
			TNOTICE("path=%s",file_abs_path.c_str());
			std::string name = DU_File::extractFileName(*it);
			snprintf(item.path,1024,"%s",file_abs_path.c_str());
			snprintf(item.name,512,"%s",name.c_str());
			item.isdir = 0;
			item.type = get_file_type(DU_File::extractFileExt(file_abs_path));
			item.size = DU_File::getFileSize(file_abs_path);
			item.time = DU_File::getModifyTime(file_abs_path);

			file_list_item list_item;
			list_item.file = item;

			//如果是图片文件且存在缩略图，此处赋值具体的缩略图URL。
			//缩略图可通过tx_nas_upload_file上传到服务器，然后通过tx_nas_get_url获取其URL
			//此处为示例代码，直接将原图作为缩略图，原图较大，不利于页面的小图显示，实际开发中，请您使用缩略图！
			if(item.type == file_type_pic){
				std::string trimed_path = trim_seperator(file_abs_path);
				TNOTICE("trimed_path = %s",trimed_path.c_str());
				char url[2048] = {0};
				int nRet = tx_nas_get_url(trimed_path.c_str(),url,2048);
				if(nRet == 0){
					list_item.thumb_url = url;
				}else{
					vecImageToUpload.push_back(trimed_path);
					list_item.thumb_url = "";  
				}
			}else{
				list_item.thumb_url = "";  
			}	


			rsp.buf.push_back(list_item);
		}
	}
	rsp.finish = true;

	//未取到缩略图URL的图片，重新上传以获取URL
	if(!vecImageToUpload.empty()){
		//上传过程为异步操作，放到线程中处理
		DU_Functor<void, TL::TLMaker<std::vector<std::string> >::Result> tf(thread_func_upload_file);
		DU_FunctorWrapper<DU_Functor<void, TL::TLMaker<std::vector<std::string> >::Result> > tfw(tf, vecImageToUpload);
		g_ThreadPool.exec(tfw);	
	}

	return 0;
}
void data_point_processor::format_file_list_response(std::string& json_rsp, const file_list_req& req, const file_list_rsp& rsp)
{
/*
	响应格式示例
	{
		"r": 0,
		"p": "/",
		"fs": [
		{
			"n": "视频",
			"t": 6,
			"mt": 1438845878,
			"tl": 3
		},
		{
			"n": "bird.bmp",
			"t": 2,
			"s": 66616,
			"mt": 1438256413,
			"th": "http://1.2.3.4/test.com/file?abc",
			"thext": 1,
			"tl": 4
			},       
			{
				"n": "libnassdk.so",
				"t": 5,
				"s": 6893883,
				"mt": 1438911842,
				"tl": 3
			}
		]
	}

	响应参数说明
	r:	ret，错误码，0 表示成功，非0表示失败
	p:	path，目录。即请求参数中的path
	fs：	files，文件列表
	n:	name，文件名
	t:	type，文件类型，详见 TXNasSDKCommDef.h 中 nas_file_type
	s:	size，文件大小，单位byte，文件夹无size参数
	mt:	最后修改时间
	tl:	timeline，时间轴。1:今天2:昨天3:一周内4:一周前5:一月前6:两月前7:未知
	th:	thumbnail，缩略图的url。只有图片文件才有该参数
	thext: thumbnail ext，缩略图的类型。1 jpg/ 2 png/ 3 bmp
*/

	StringBuffer sb;
	Writer<StringBuffer> writer(sb);

	writer.StartObject();
	writer.String("r");
	writer.Int(0);
	writer.String("p");
	writer.String(req.path.c_str());
	writer.String("fs");
	writer.StartArray();
	for (unsigned int i = 0; i< rsp.buf.size(); ++i)
	{
		if(strlen(rsp.buf[i].file.path) == 0) continue;
		writer.StartObject();
		writer.String("n");
		writer.String(rsp.buf[i].file.name);
		writer.String("t");
		writer.Int(rsp.buf[i].file.type);
		if (rsp.buf[i].file.isdir == 0)        //非文件夹，需返回大小
		{
			writer.String("s");
			writer.Uint64(rsp.buf[i].file.size);
		}
		writer.String("mt");
		writer.Uint(rsp.buf[i].file.time);
		if(!rsp.buf[i].thumb_url.empty()){  //存在缩略图
			writer.String("th");
			writer.String(rsp.buf[i].thumb_url.c_str());
			writer.String("thext");
			writer.Int(1);  // 1 jpg, 2 png, 3 bmp
		}

		writer.String("tl");
		writer.Int(get_timeline(rsp.buf[i].file.time));

		writer.EndObject();
	}
	writer.EndArray();

	if(rsp.finish){
		writer.String("f");  // 文件列表结束
		writer.Int(1);
	}
	writer.EndObject();

	json_rsp = sb.GetString();
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//删除文件

int data_point_processor::parse_remove_file_request(tx_data_point* req_dp, file_rm_req& req)
{
/*
	请求格式示例
	{
		"files": [
			{
				"path": "/tmp1.txt"
			},
			{
				"path": "/tmp2.txt"
			}
		]
	}

	请求参数说明
	files: 要删除的文件列表
	path: 要删除的文件路径

*/

	std::string req_json(req_dp->value);
	const char* json = req_json.data();

	Document req_d;
	req_d.Parse(json);
	if(req_d.HasParseError()){
		return -1;
	}

	if (!(req_d.HasMember("files") && req_d["files"].IsArray())){
		TERROR("json format error. json=%s",json);
		return -1;
	}

	Value& s = req_d["files"];
	for(unsigned int i = 0; i < s.Size(); ++i) {
		if(s[i].HasMember("path") && s[i]["path"].IsString())
			req.path_list.push_back(s[i]["path"].GetString());
	}

	return 0;
}
int data_point_processor::impl_remove_file(file_rm_req& req, file_rm_rsp& rsp)
{
	int ret = 0;
	for(std::vector<std::string>::iterator it = req.path_list.begin();it!=req.path_list.end();++it) {
		if(*it == "/") continue;
		std::string abs_path = g_root + *it;
		TNOTICE("Remove file: %s",abs_path.c_str());

		int result = DU_File::removeFile(abs_path,true);
		if(result != 0){  //删除失败
			rsp.path_list.push_back(*it);
			int ret_code_part_success = -9998;   //有文件删除失败
			ret = ret_code_part_success;
		}else{
			//删除成功
		}
	}

	return ret;
}
void data_point_processor::format_remove_file_response(int ret, std::string& json_rsp, const file_rm_req& req, const file_rm_rsp& rsp)
{
/*	
	响应格式示例
	1. 成功，返回错误码0
	{
		"ret": 0
	}

	2. 部分或全部文件删除失败，返回错误码-9998，同时返回删除失败的文件
	{
	“ret”:-9998,
	 "failed_files": [
		{
			 "path": "/tmp1.txt"
		},
		{
		  "path": "/tmp2.txt"
		 }
	 ]
	}
*/

	StringBuffer sb;
	Writer<StringBuffer> writer(sb);
	writer.StartObject();
	writer.String("ret");
	writer.Int(ret);

	if(ret == -9998){
		writer.String("failed_files");
		writer.StartArray();
		for(std::vector<std::string>::const_iterator it = rsp.path_list.begin();	it != rsp.path_list.end();
			++it) {
				writer.StartObject();
				writer.String("path");
				writer.String((*it).c_str());
				writer.EndObject();
		}
		writer.EndArray();
	}

	writer.EndObject();

	json_rsp = sb.GetString();
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//分类列表

int data_point_processor::parse_type_list_request(tx_data_point* req_dp, type_list_req& req)
{
/*
	请求格式示例
	{
		"type": 2
		"page": 1,
		"count": 10
	}

	请求参数说明
	type:		文件类型，详见 TXNasSDKCommDef.h 中 nas_file_type
	page:		分页，从0开始。
	count:		要返回的文件数量。暂时未用。
*/

	std::string req_json(req_dp->value);
	const char* json = req_json.data();

	Document req_d;
	req_d.Parse(json);
	if(req_d.HasParseError()){
		return -1;
	}

	if (!(req_d.HasMember("type") && req_d["type"].IsInt()
		&& req_d.HasMember("page") && req_d["page"].IsInt())) {
			TERROR("json is not satisfied fmt. json=%s\n",json);
			return -1;
	}

	if(req_d.HasMember("count") && req_d["count"].IsInt()){
		req.count = req_d["count"].GetInt();
	}else{
		req.count = 100;
	}

	req.type = req_d["type"].GetInt();
	req.page = req_d["page"].GetInt();

	return 0;
}
int data_point_processor::impl_type_list(type_list_req& req, type_list_rsp& rsp)
{
	//示例代码： 返回当前目录下所有所请求类型的文件（忽略分页，您可以根据req.page进行分页返回）
	std::string path = "/";
	std::vector<std::string> vecImageToUpload;
	get_type_list(path,req.type,rsp.buf,vecImageToUpload);
	rsp.finish = true;

	//未取到缩略图URL的图片，重新上传以获取URL
	if(!vecImageToUpload.empty()){
		//上传过程为异步操作，放到线程中处理
		DU_Functor<void, TL::TLMaker<std::vector<std::string> >::Result> tf(thread_func_upload_file);
		DU_FunctorWrapper<DU_Functor<void, TL::TLMaker<std::vector<std::string> >::Result> > tfw(tf, vecImageToUpload);
		g_ThreadPool.exec(tfw);	
	}

	return 0;
}
void data_point_processor::format_type_list_response(std::string& json_rsp, const type_list_req& req, const type_list_rsp& rsp)
{
/*
	响应格式示例
	{
		"r": 0,
		"fs": [
			{
				"tl": 1,
				"item": [
					{
						"p": "/羽泉-奔跑.mp3",
						"t": 3,
						"s": 12219253,
						"mt": 1443081691
					},
					{
						"p": "/过云雨.mp3",
						"t": 3,
						"s": 9080290,
						"mt": 1442977735
					}
				]
			},
			{
				"tl": 6,
				"item": [
					{
						"p": "/图片/西湖.jpg",
						"t": 2,
						"s": 8968359,
						"mt": 1438323642,
						"th": "http://1.2.3.4/test.com/file?abc",
						"thext": 1
					}
				]
			}
		],
		"f": 1
	}

	响应参数说明
	r:	ret，错误码，0 表示成功，非0表示失败
	fs: files，文件列表
	tl:	timeline，时间轴。1:今天2:昨天3:一周内4:一周前5:一月前6:两月前7:未知
	item: 每个时间轴中包含的文件列表（数组）
	p:	path，文件路径
	t:	type，文件类型，详见 TXNasSDKCommDef.h 中 nas_file_type
	s:	size，文件大小，单位byte，文件夹无size参数
	mt:	最后修改时间
	th:	thumbnail，缩略图的url。只有图片文件才有该参数
	thext: thumbnail ext，缩略图的类型。jpg/png/bmp
	f: 用于分页，1表示该目录下所有文件/文件夹都已返回，0表示未返回完
*/

	StringBuffer sb;
	Writer<StringBuffer> writer(sb);

	writer.StartObject();
	writer.String("r");
	writer.Int(0);
	writer.String("fs");
	writer.StartArray();

	//先将文件列表按照时间轴timeline进行分类
	std::map<int,std::vector<int> > time_line_list;
	for(unsigned int i =0; i< rsp.buf.size(); ++i){
		int tl = get_timeline(rsp.buf[i].file.time);
		time_line_list[tl].push_back(i);
	}
	
	//再组合成规定格式返回
	for(std::map<int, std::vector<int> >::iterator it = time_line_list.begin();	it != time_line_list.end(); ++it){
		writer.StartObject();
		writer.String("tl");
		writer.Int(it->first);

		writer.String("item");
		writer.StartArray();
		for(std::vector<int>::iterator it_item = it->second.begin(); it_item != it->second.end();	++it_item) {
			writer.StartObject();
			writer.String("p");
			writer.String(rsp.buf[*it_item].file.path);
			writer.String("t");
			writer.Int(rsp.buf[*it_item].file.type);
			if (rsp.buf[*it_item].file.isdir == 0)        //非文件夹，需返回大小
			{
				writer.String("s");
				writer.Uint64(rsp.buf[*it_item].file.size);
			}
			writer.String("mt");
			writer.Uint(rsp.buf[*it_item].file.time);
			if(!rsp.buf[*it_item].thumb_url.empty()){  //存在缩略图
				writer.String("th");
				writer.String(rsp.buf[*it_item].thumb_url.c_str());
				writer.String("thext");
				writer.Int(1);  // 1 jpg, 2 png, 3 bmp
			}
			writer.EndObject();
		}
		writer.EndArray();

		writer.EndObject();
	}
	
	writer.EndArray();

	//判断列表是否结束
	if(rsp.finish){
		writer.String("f");  // 文件列表结束
		writer.Int(1);
	}
	
	writer.EndObject();

	json_rsp = sb.GetString();
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//图片预览
int data_point_processor::parse_image_preview_request(tx_data_point* req_dp, image_preview_req& req)
{
	/*
	请求格式示例
	{
		"path": "/a.jpg"
		"type": 2,
	}

	请求参数说明
	path:		请求的图片路径
	type:		文件类型，图片为2，详见 TXNasSDKCommDef.h 中 nas_file_type
*/

	std::string req_json(req_dp->value);
	const char* json = req_json.data();

	Document req_d;
	req_d.Parse(json);
	if(req_d.HasParseError()){
		return -1;
	}

	if (!(req_d.HasMember("path") && req_d["path"].IsString()
		&& req_d.HasMember("type") && req_d["type"].IsInt())) {
			TERROR("json is not satisfied fmt. json=%s\n",json);
			return -1;
	}

	req.path =  req_d["path"].GetString();
	req.type = req_d["type"].GetInt();

	return 0;
}
int data_point_processor::impl_image_preview(image_preview_req& req, image_preview_rsp& rsp)
{
	//示例代码： 查询的方式效率不高，只做示例用。这里展示了tx_nas_upload_file和tx_nas_get_url两个函数的用法
	
	//判断文件是否存在
	std::string abs_path = g_root + req.path;
	if(!DU_File::isFileExist(abs_path)) {
		return -1;
	}
	rsp.path = req.path;
	
	//尝试获取URL
	std::string trimed_path = trim_seperator(abs_path);
	TNOTICE("impl_image_preview: trimed_path = %s",trimed_path.c_str());
	char url[2048] = {0};
	int nRet = tx_nas_get_url(trimed_path.c_str(),url,2048);
	if(nRet == 0){
		rsp.url = url;
		return 0;
	}
	
	//尝试获取，但未获取到则重新上传。上传过程为异步操作，放到线程中处理
	std::vector<std::string> vecImageToUpload;
	vecImageToUpload.push_back(trimed_path);
	DU_Functor<void, TL::TLMaker<std::vector<std::string> >::Result> tf(thread_func_upload_file);
	DU_FunctorWrapper<DU_Functor<void, TL::TLMaker<std::vector<std::string> >::Result> > tfw(tf, vecImageToUpload);
	g_ThreadPool.exec(tfw);

	//查询
	unsigned int cnt = 0;
	do{
		DU_ThreadControl::sleep(1000);
		memset(url,0,2048);
		nRet = tx_nas_get_url(trimed_path.c_str(),url,2048);   //再次尝试获取
		if (nRet == 0){	
			rsp.url = url;			
			return 0;
		}
		cnt++;
	} while (cnt <=5);
	
	return 0;
}
void data_point_processor::format_image_preview_response(std::string& json_rsp, const image_preview_req& req, const image_preview_rsp& rsp)
{
	/*
	响应格式
	{
		"ret": 0,
		"url": " http://1.2.3.4:8080/asn.com/stddomon_file?vabcdef ",
		"path": "/tmp.jpg"
	}


	响应参数说明
	r:		ret，错误码，0 表示成功，非0表示失败
	url:	图片的服务器URL地址，用于页面显示图片
	path:	图片路径（与请求中的参数相同）
*/

	StringBuffer sb;
	Writer<StringBuffer> writer(sb);

	writer.StartObject();
	writer.String("r");
	writer.Int(0);
	writer.String("url");
	writer.String(rsp.url.c_str());
	writer.String("path");
	writer.String(rsp.path.c_str());
	writer.EndObject();

	json_rsp = sb.GetString();
}

int data_point_processor::get_file_type(const std::string& ext)
{
	//std::string type;

	static const char* pic_type[] = {
		"ico","tif","tiff","ufo","raw","arw","srf",
		"sr2","dcr","k25","kdc","cr2","crw","nef",
		"mrw","ptx","pef","raf","3fr","erf","mef",
		"mos","orf","rw2","dng","x3f","jpg","jpeg",
		"png","gif","bmp",
	};
	static const char* doc_type[] = {
		"docx","wri","rtf","xla","xlb","xlc","xld",
		"xlk","xll","xlm","xlt","xlv","xlw","xlsx",
		"xlsm","xlsb","xltm","xlam","pptx","pps",
		"ppsx","pdf","txt","doc","xls","ppt",
	};
	static const char* video_type[] = {
		"3gp","3g2","asf","dat","divx","dvr-ms","m2t",
		"m2ts","m4v","mkv","mp4","mts","mov","qt","tp",
		"trp","ts","vob","wmv","xvid","ac3","rm",
		"rmvb","ifo","mpeg","mpg","mpe","m1v","m2v",
		"mpeg1","mpeg2","mpeg4","ogv","webm","flv","avi",
		"swf","f4v",
	};
	static const char* audio_type[] = {
		"aac","flac","m4a","m4b","aif","ogg","pcm",
		"wav","cda","mid","mp2","mka","mpc","ape",
		"ra","ac3","dts","wma","mp3","amr",
	};

	for (size_t i = 0; i < sizeof(pic_type)/sizeof(char*); ++i)
	{
		if(strcasecmp(pic_type[i], ext.c_str()) == 0)
		{
			return file_type_pic;
		}
	}
	for (size_t i = 0; i < sizeof(doc_type)/sizeof(char*); ++i)
	{
		if(strcasecmp(doc_type[i], ext.c_str()) == 0)
		{
			return file_type_doc;
		}
	}
	for (size_t i = 0; i < sizeof(video_type)/sizeof(char*); ++i)
	{
		if(strcasecmp(video_type[i], ext.c_str()) == 0)
		{
			return file_type_video;
		}
	}	
	for (size_t i = 0; i < sizeof(audio_type)/sizeof(char*); ++i)
	{
		if(strcasecmp(audio_type[i], ext.c_str()) == 0)
		{
			return file_type_audio;
		}
	}
	return file_type_file;
}
int data_point_processor::get_timeline(unsigned int timestamp)
{
	int timeline = file_timeline_unknown;
	time_t now = time(NULL);
	if(now - timestamp > SEC_IN_ONE_DAY * DAY_IN_ONE_MONTH * 2) {
		timeline = file_timeline_before_two_month;
	} else if (now - timestamp  > SEC_IN_ONE_DAY * DAY_IN_ONE_MONTH * 1) {
		timeline = file_timeline_before_one_month;
	} else if (now - timestamp  > SEC_IN_ONE_DAY * DAY_IN_ONE_WEEK * 1) {
		timeline = file_timeline_before_one_week;
	} else if (now - timestamp  > SEC_IN_ONE_DAY * 2) {
		timeline = file_timeline_in_one_week;
	} else if (now - timestamp  > SEC_IN_ONE_DAY ) {
		timeline = file_timeline_yesterday;
	} else if (now - timestamp  > 0) {
		timeline = file_timeline_today;
	} else {
		timeline = file_timeline_unknown; //未知时间
	}

	return timeline;
}
void data_point_processor::get_type_list(const std::string path, int type, std::vector<file_list_item >& file_list,std::vector<std::string>& vecImageToUpload)
{
	//示例代码：递归搜索的方式效率不高，仅做示例用。

	std::string abs_path = g_root+path;
	//递归扫描目录下的所有文件夹
	{
		std::vector<std::string> dirs;
		DU_File::scanDir(abs_path,dirs,select_dir);
		for(std::vector<std::string>::iterator it = dirs.begin();	it != dirs.end();	++it) {
			get_type_list(path+*it,type,file_list,vecImageToUpload);  //递归
		}
	}

	//扫描目录下的所有文件
	{
		std::vector<std::string> files;
		DU_File::scanDir(abs_path,files,select_file);
		for(std::vector<std::string>::iterator it = files.begin();	it != files.end();	++it) {
			int file_type = get_file_type(DU_File::extractFileExt(*it));

			//判断文件类型
			if(file_type == type){
				NasFile item;
				std::string file_abs_path =  abs_path +std::string("/")+ *it;
				TNOTICE("file_path=%s",file_abs_path.c_str());
				std::string name = DU_File::extractFileName(*it);
				snprintf(item.path,1024,"%s",(path+std::string("/")+*it).c_str());
				snprintf(item.name,512,"%s",name.c_str());
				item.isdir = 0;
				item.type = file_type;
				item.size = DU_File::getFileSize(file_abs_path);
				item.time = DU_File::getModifyTime(file_abs_path);

				file_list_item list_item;
				list_item.file = item;

				//如果是图片文件且存在缩略图，此处赋值具体的缩略图URL。
				//缩略图可通过tx_nas_upload_file上传到服务器，然后通过tx_nas_get_url获取其URL
				//此处为示例代码，直接将原图作为缩略图，原图较大，不利于页面的小图显示，实际开发中，请您使用缩略图！
				if(type == file_type_pic){
					std::string trimed_path = trim_seperator(file_abs_path);
					TNOTICE("trimed_path = %s",trimed_path.c_str());
					char url[2048] = {0};
					int nRet = tx_nas_get_url(trimed_path.c_str(),url,2048);
					if(nRet == 0){
						list_item.thumb_url = url;
					}else{
						vecImageToUpload.push_back(trimed_path);
						list_item.thumb_url = "";  
					}
				}else{
					list_item.thumb_url = "";  
				}				 

				file_list.push_back(list_item);
			}
		}
	}
}
std::string data_point_processor::trim_seperator(std::string path)
{
	std::string trimed;
	int len = path.length();
	bool isPreSeperator = false;            //上一个字符是否是 /
	for (int i=0;i<len; ++i){
		if(path[i] == '/' ){
			if(!isPreSeperator){
				trimed.push_back(path[i]);
			}
			isPreSeperator = true;
		}else{
			trimed.push_back(path[i]);
			isPreSeperator = false;
		}
	}
	return trimed;	
}