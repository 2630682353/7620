#ifndef _HTTP_CALL_H  
#define _HTTP_CALL_H  
  
#define MY_HTTP_DEFAULT_PORT 80  
  
char * http_get(const char *url);  
char * http_post(const char *url,const char * post_str);  
  
#endif 