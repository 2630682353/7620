#include "utils.h"

std::vector<std::string> split(const std::string& str, const std::string& pattern)
{
	std::string::size_type pos;
	std::vector<std::string> result;
	std::string tmp_str = str + pattern;//扩展字符串以方便操作
	int size=tmp_str.size();

	for(int i=0; i<size; i++)
	{
		pos=tmp_str.find(pattern,i);
		if(pos<size)
		{
			std::string s=tmp_str.substr(i,pos-i);
			result.push_back(s);
			i=pos+pattern.size()-1;
		}
	}
	return result;
}

std::string get_file_name(const std::string& path)
{
	std::vector<std::string> tmp = split(path, "/");	
	return *(--(tmp.end()));
}

std::string get_time()
{ 
    time_t t = time(0); 
    char tmp[64]; 
    strftime(tmp, sizeof(tmp), "%Y/%m/%d %X", localtime(&t));
    return std::string(tmp);
}
