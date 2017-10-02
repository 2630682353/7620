#ifndef	_DATA_POINT_PROCESSOR_H
#define _DATA_POINT_PROCESSOR_H

#include "utils.h"
#include "TXNasSDK.h"

struct file_list_req
{
	std::string path;
	int 		page;
	int 		count;
	file_list_req() {
		page = 0;
		count = 0;
	}
};

struct file_list_item{
	NasFile file;
	std::string thumb_url;  //如果file为图片文件，thumb_url 为图片的URL地址
};

struct file_list_rsp
{
	std::vector<file_list_item > buf;
	int 		count;
	bool      finish; //完成标志
	file_list_rsp() {
		count = 0;
		finish = false;
	}
};

struct file_rm_req
{
	std::vector<std::string> path_list;
	file_rm_req() {
		path_list.clear();
	}
};

struct file_rm_rsp
{
	std::vector<std::string> path_list;
	file_rm_rsp() {
		path_list.clear();
	}
};

struct type_list_req
{
	int type;
	int page;
	int count;
	type_list_req() {
		type = 0;
		page = 0;
		count = 0;
	}
};

struct type_list_rsp
{
	int 		count;
	bool		finish;
	std::vector<file_list_item > buf;
	type_list_rsp() {
		count = 0;
		finish = false;
	}
};

struct image_preview_req
{
	std::string path;
	int type;
	image_preview_req() {
		type = 2;
	}
};

struct image_preview_rsp
{
	std::string path;
	std::string url;
	image_preview_rsp() {
	}
};


//文件的时间轴
typedef enum enum_file_timeline {
	file_timeline_today = 1, //今天
	file_timeline_yesterday = 2, //昨天
	file_timeline_in_one_week = 3, //一周内
	file_timeline_before_one_week = 4, //一周前
	file_timeline_before_one_month = 5, //一个月前
	file_timeline_before_two_month = 6, //两个月前
	file_timeline_unknown = 7, //未知时间
}file_timeline_t;

#define SEC_IN_ONE_DAY 86400
#define DAY_IN_ONE_WEEK 7
#define DAY_IN_ONE_MONTH 30 //每个月按30天计算


class data_point_processor : public DU_Singleton<data_point_processor>
{
public:
	int on_process(unsigned long long from_client, tx_data_point *req_dp, tx_data_point* rsp_dp);
private:

	//文件列表
	int process_datapoint_file_list(unsigned long long from_client, tx_data_point *req_dp,tx_data_point* rsp_dp);

	//删除文件
	int process_datapoint_remove_file(unsigned long long from_client, tx_data_point *req_dp, tx_data_point* rsp_dp);

	//分类列表
	int process_datapoint_type_list(unsigned long long from_client, tx_data_point *req_dp, tx_data_point* rsp_dp);

	//图片预览
	int process_datapoint_image_preview(unsigned long long from_client, tx_data_point *req_dp, tx_data_point* rsp_dp);


protected:

	//文件列表
	int parse_file_list_request(tx_data_point* req_dp, file_list_req& req);
	int impl_file_list(file_list_req& req, file_list_rsp& rsp);
	void format_file_list_response(std::string& json_rsp, const file_list_req& req, const file_list_rsp& rsp);

	//删除文件
	int parse_remove_file_request(tx_data_point* req_dp, file_rm_req& req);
	int impl_remove_file(file_rm_req& req, file_rm_rsp& rsp);
	void format_remove_file_response(int ret, std::string& json_rsp, const file_rm_req& req, const file_rm_rsp& rsp);

	//分类列表
	int parse_type_list_request(tx_data_point* req_dp, type_list_req& req);
	int impl_type_list(type_list_req& req, type_list_rsp& rsp);
	void format_type_list_response(std::string& json_rsp, const type_list_req& req, const type_list_rsp& rsp);

	//图片预览
	int parse_image_preview_request(tx_data_point* req_dp, image_preview_req& req);
	int impl_image_preview(image_preview_req& req, image_preview_rsp& rsp);
	void format_image_preview_response(std::string& json_rsp, const image_preview_req& req, const image_preview_rsp& rsp);

private:
	int get_file_type(const std::string& ext); 
	int get_timeline(unsigned int timestamp);
	void get_type_list(const std::string path, int type, std::vector<file_list_item >& file_list,std::vector<std::string>& vecImageToUpload);
	std::string trim_seperator(std::string path); //去除路劲中冗余的分隔符 / 符号
	////////////////////////////////////////////////////////////////////
};

#endif //