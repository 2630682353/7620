#ifndef UTILS_H
#define UTILS_H

#include <stdlib.h>
#include <vector>
#include <string>
#include <stdexcept>
#include <string>
#include <time.h>
#include <string.h>
#include <stdint.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/file.h>
#include <fcntl.h>
/*
 *其他文件实用的头文件
 */
#include "du_file.h"
#include "du_config.h"
#include "du_option.h"
#include "du_thread_pool.h"
#include "du_functor.h"
#include "du_monitor.h"
#include "du_base64.h"
#include "du_md5.h"
#include "du_mmap.h"
#include "du_singleton.h"
#include "du_des.h"



//rapidjson
#include "rapidjson/document.h"
#include "rapidjson/writer.h"
#include "rapidjson/stringbuffer.h"
using namespace rapidjson;

//标准库
using namespace std;

//commdef
#include "common.h"

//日志宏
#define TLOG_NOTICE 0
#define TLOG_WARNING 1
#define TLOG_ERROR 2
#define TLOG(LEVEL, fmt, ...)\
do {\
if (TLOG_NOTICE == LEVEL)\
fprintf(stdout, "NOTICE:[%s][%d]:[%s:%d]:" fmt"\n", get_time().c_str(), getpid(), __FILE__, __LINE__ , ##__VA_ARGS__);\
else if (TLOG_WARNING == LEVEL)\
fprintf(stdout, "WARNING:[%s][%d]:[%s:%d]:" fmt"\n", get_time().c_str(), getpid(), __FILE__, __LINE__ , ##__VA_ARGS__);\
else if (TLOG_ERROR == LEVEL)\
fprintf(stdout, "ERROR:[%s][%d]:[%s:%d]:" fmt"\n", get_time().c_str(), getpid(), __FILE__, __LINE__ , ##__VA_ARGS__);\
} while(0)
#define TNOTICE(fmt, ...)\
TLOG(TLOG_NOTICE, fmt, ##__VA_ARGS__)
#define TWARNING(fmt, ...)\
TLOG(TLOG_WARNING, fmt, ##__VA_ARGS__)
#define TERROR(fmt, ...)\
TLOG(TLOG_ERROR, fmt, ##__VA_ARGS__)

std::string get_time();
std::vector<std::string> split(const std::string& str, const std::string& pattern);
std::string get_file_name(const std::string& path);


#endif // UTILS_H
