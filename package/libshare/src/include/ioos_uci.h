#ifndef _HTTP_IOOS_UCI_H_
#define _HTTP_IOOS_UCI_H_
char* uci_getval(char*file,char*section,char*option);
void uci_setval(char*file,char*section,char*option,char*value);
void uci_delval(char*file,char*section,char*option);
int do_cmd(char *cmd);
void ioos_uci_add_list(char*file,char*section,char*option,char*value);
void ioos_uci_del_list(char*file,char*section,char*option,char*value);
//typedef void (uci_getlist_callback)(char*name,void*data);
//int uci_getlist(char*file,char*section,char*option,uci_getlist_callback *fuc,void *data);
int uci_list_check_value(char*file,char*section,char*list,char*value);
char* uci_list_get_value(char*file,char*section,char*list,char*value);
int uci_list_check_value2(char*file,char*section,char*list,char*value);
char* uci_getval_cmd(char*cmdsrc);
#endif
