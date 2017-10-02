#include <stdio.h>
#include <string.h>
#include <uci.h>
#include <stdlib.h>
#include "nlk_ipc.h"
#include "uci_fn.h"

#define UFREE(x) do{if((x)) {free((x)); (x) = NULL;}} while(0)

#define UUCI_PRINTF(fmt,args...) do{/*printf("[UUCI:%05d]DBG:"fmt, __LINE__, ##args);*/}while(0)

/*
 * export(print) a config
 */
int uuci_print_file(char *file)
{
	struct uci_context *ctx;
	struct uci_package * pkg;  

	if (!file) {
		UUCI_PRINTF("file\n");
		return -1;
	}
	ctx = uci_alloc_context();
	if (!ctx) {
		UUCI_PRINTF("alloc\n");
		return -1;
	}
	if (UCI_OK != uci_load(ctx, file, &pkg)) {
		UUCI_PRINTF("uci_load\n");
		uci_free_context(ctx);
		return -1;
	}
	uci_export(ctx, stdout, pkg, true);

	uci_unload(ctx, pkg);
	uci_free_context(ctx);
	return 0;
}
/*
 * Set the value of the given option, 
 * or add a new section with the type 
 * set to the given value.
 * 
 */
int uuci_set(char *s)
{
	
	struct uci_context *ctx;
	struct uci_ptr ptr;
	/*char str[] = "network.@interface[0].ipaddr=192.168.1.1";*/
	char *str = NULL;
	int ret = 0;
	char *dest = NULL;

	if (!s) {
		UUCI_PRINTF("s is null\n");
		return -1;
	}

	str = calloc(1, strlen(s) + 1);
	if (!str) {
		UUCI_PRINTF("calloc\n");
		return -1;
	}

	strncpy(str, s, strlen(s));

	ctx = uci_alloc_context();
	if (!ctx) {
		UUCI_PRINTF("alloc\n");
		UFREE(str);
		return -1;
	}

	if (uci_lookup_ptr(ctx, &ptr, str, true) != UCI_OK) {
		UUCI_PRINTF("uci_lookup_ptr error\n");
		uci_get_errorstr(ctx, &dest, "errmsg");
		UUCI_PRINTF("%s\n", dest);
		UFREE(str);
		UFREE(dest);
		uci_free_context(ctx);
		return -1;
	}

	ret = uci_set(ctx, &ptr);
	if (ret == UCI_OK) {
		uci_save(ctx, ptr.p);
		uci_commit(ctx, &ptr.p, false);
		UFREE(str);
		uci_free_context(ctx);
		return 0;
	} else {
		UUCI_PRINTF("cmd:%s, ret=%d\n", s, ret);
		uci_get_errorstr(ctx, &dest, "errmsg");
		UUCI_PRINTF("%s\n", dest);
		UFREE(str);
		UFREE(dest);
		uci_free_context(ctx);
		return -1;
	}

}

/*
 * Add an anonymous section of type section-type
 * to the given configuration.
 */
int uuci_add(char *file, char *st)
{
	struct uci_package *p = NULL;
	struct uci_section *s = NULL;
	struct uci_context *ctx = NULL;
	/*char str[] = "test";*/
	char *str = NULL;
	int ret = 0;
	char *dest = NULL;

	if (!file || !st) {
		UUCI_PRINTF("file or s is null\n");
		return -1;
	}

	str = calloc(1, strlen(st) + 1);
	if (!str) {
		UUCI_PRINTF("calloc\n");
		return -1;
	}

	strncpy(str, st, strlen(st));

	ctx = uci_alloc_context();
	if (!ctx) {
		UUCI_PRINTF("alloc\n");
		UFREE(str);
		return -1;
	}

	uci_load(ctx, file, &p);

	ret = uci_add_section(ctx, p, str, &s);
	if (ret == UCI_OK) {
		uci_save(ctx, p);
		uci_commit(ctx, &p, false);
		uci_unload(ctx, p);
		uci_free_context(ctx);
		UFREE(str);
		return 0;
	} else {
		UUCI_PRINTF("ret=%d\n", ret);
		uci_get_errorstr(ctx, &dest, "errmsg");
		UUCI_PRINTF("%s\n", dest);
		uci_unload(ctx, p);
		uci_free_context(ctx);
		UFREE(str);
		UFREE(dest);
		return -1;
	}
}

/*
 * Add the given string to an existing list option.
 */
int uuci_add_list(char *s)
{
	struct uci_ptr ptr;
	struct uci_context *ctx;
	/*char str[] = "network.@test[0].index=some";*/
	char *str = NULL;
	int ret = 0;
	char *dest = NULL;
	
	if (!s) {
		UUCI_PRINTF("s is null\n");
		return -1;
	}

	str = calloc(1, strlen(s) + 1);
	if (!str) {
		UUCI_PRINTF("calloc\n");
		return -1;
	}

	strncpy(str, s, strlen(s));

	ctx = uci_alloc_context();
	if (!ctx) {
		UUCI_PRINTF("alloc\n");
		UFREE(str);
		return -1;
	}

	if (uci_lookup_ptr(ctx, &ptr, str, true) != UCI_OK) {
		UUCI_PRINTF("uci_lookup_ptr error\n");
		uci_get_errorstr(ctx, &dest, "errmsg");
		UUCI_PRINTF("%s\n", dest);
		UFREE(str);
		UFREE(dest);
		uci_free_context(ctx);
		return -1;
	}
	/*UUCI_PRINTF("ptr.s->type=%s\n", ptr.s->type);*/
	/*UUCI_PRINTF("ptr.o->v.string=%s\n", ptr.o->v.string);*/
	/*UUCI_PRINTF("ptr.s->package->path=%s\n",ptr.s->package->path );*/
	/*UUCI_PRINTF("ptr.last->name=%s\n", ptr.last->name);*/

	ret = uci_add_list(ctx, &ptr);
	if (ret == UCI_OK) {
		uci_save(ctx, ptr.p);
		uci_commit(ctx, &ptr.p, false);
		UFREE(str);
		uci_free_context(ctx);
		return 0;
	} else {
		UUCI_PRINTF("ret=%d\n", ret);
		uci_get_errorstr(ctx, &dest, "errmsg");
		UUCI_PRINTF("%s\n", dest);
		uci_free_context(ctx);
		UFREE(str);
		UFREE(dest);
		return -1;
	}

}
/*
 * delete a list
 */
int uuci_del_list(char *s)
{
	
	struct uci_ptr ptr;
	struct uci_context *ctx;
	/*char str[] = "network.@test[0].index=some";*/
	int ret = 0;
	char *dest = NULL;
	char *str = NULL;

	if (!s) {
		UUCI_PRINTF("s is null\n");
		return -1;
	}

	str = calloc(1, strlen(s) + 1);
	if (!str) {
		UUCI_PRINTF("calloc\n");
		return -1;
	}

	strncpy(str, s, strlen(s));

	ctx = uci_alloc_context();
	if (!ctx) {
		UUCI_PRINTF("alloc\n");
		UFREE(str);
		return -1;
	}

	if (uci_lookup_ptr(ctx, &ptr, str, true) != UCI_OK) {
		UUCI_PRINTF("uci_lookup_ptr error\n");
		uci_get_errorstr(ctx, &dest, "errmsg");
		UUCI_PRINTF("%s\n", dest);
		UFREE(str);
		UFREE(dest);
		uci_free_context(ctx);
		return -1;
	}

	ret = uci_del_list(ctx, &ptr);
	if (ret == UCI_OK)
		ret = uci_save(ctx, ptr.p);

	if (ret != UCI_OK) {
		UUCI_PRINTF("some error\n");
		uci_get_errorstr(ctx, &dest, "errmsg");
		UUCI_PRINTF("%s\n", dest);
		UFREE(str);
		UFREE(dest);
		uci_free_context(ctx);
		return -1;

	}

	uci_save(ctx, ptr.p);
	uci_commit(ctx, &ptr.p, false);

	uci_free_context(ctx);
	UFREE(str);
	return 0;
}
/*
 * delete the given section or option
 */
int uuci_delete(char *s)
{
	
	struct uci_ptr ptr;
	struct uci_context *ctx;
	/*char str[] = "network.@test[0]";*/
	char *str = NULL;
	/*char str[] = "network.@alias[0].interface";*/
	int ret = 0;
	char *dest = NULL;

	if (!s) {
		UUCI_PRINTF("s is null\n");
		return -1;
	}

	str = calloc(1, strlen(s) + 1);
	if (!str) {
		UUCI_PRINTF("calloc\n");
		return -1;
	}

	strncpy(str, s, strlen(s));

	ctx = uci_alloc_context();
	if (!ctx) {
		UUCI_PRINTF("alloc\n");
		UFREE(str);
		return -1;
	}

	if (uci_lookup_ptr(ctx, &ptr, str, true) != UCI_OK) {
		UUCI_PRINTF("uci_lookup_ptr error\n");
		uci_get_errorstr(ctx, &dest, "errmsg");
		UUCI_PRINTF("%s\n", dest);
		UFREE(str);
		UFREE(dest);
		uci_free_context(ctx);
		return -1;
	}

	ret = uci_delete(ctx, &ptr);
	if (ret == UCI_OK) {
		uci_save(ctx, ptr.p);
		uci_commit(ctx, &ptr.p, false);
		UFREE(str);
		uci_free_context(ctx);
		return 0;
	} else {
		UUCI_PRINTF("cmd:%s, ret=%d\n", s, ret);
		uci_get_errorstr(ctx, &dest, "errmsg");
		UUCI_PRINTF("%s\n", dest);
		UFREE(str);
		UFREE(dest);
		uci_free_context(ctx);
		return -1;
	}
}
/*
 * free the return array from uuci_get()
 */
int uuci_get_free(char **result, int num)
{
	int i = 0;

	for (i = 0; i < num; ++i) {
		UFREE(result[i]);
	}
	UFREE(result);
	return 0;
}
/*
 * Get the value of the given option or
 * the type of the given section.
 * 
 * caller need free array
 */
int uuci_get(char *s, char ***array, int *num)
{
	struct uci_element *e;
	struct uci_element *elem;
	struct uci_ptr ptr;
	struct uci_context *ctx;
	/*char str[] = "network.@interface[0].ipaddr";*/
	/*char str[] = "network.@alias[0].interface";*/
	char *str = NULL;
	char *dest = NULL;

	*array = NULL;

	/*if (!s || !num || !(**array))*/
	if (!s || !num ) {
		UUCI_PRINTF("s or num or *array is null\n");
		return -1;
	}

	*num = 0;
	str = calloc(1, strlen(s) + 1);
	if (!str) {
		UUCI_PRINTF("calloc\n");
		return -1;
	}

	strncpy(str, s, strlen(s));

	UUCI_PRINTF("str=%s\n", str);

	ctx = uci_alloc_context();
	if (!ctx) {
		UUCI_PRINTF("alloc\n");
		UFREE(str);
		return -1;
	}

	if (uci_lookup_ptr(ctx, &ptr, str, true) != UCI_OK) {
		UUCI_PRINTF("uci_lookup_ptr error\n");
		uci_get_errorstr(ctx, &dest, "errmsg");
		UUCI_PRINTF("%s\n", dest);
		UFREE(str);
		UFREE(dest);
		uci_free_context(ctx);
		return -1;
	}

	e = ptr.last;
	if (!(ptr.flags & UCI_LOOKUP_COMPLETE)) {
		ctx->err = UCI_ERR_NOTFOUND;
		uci_free_context(ctx);
		UFREE(str);
		UUCI_PRINTF("Not found\n");
		return -1;
	}

	switch(e->type)
	{
		case UCI_TYPE_SECTION:
			UUCI_PRINTF("%s\n", ptr.s->type);
			++(*num);
			(*array) = realloc(*array, (*num)*4);
			(*array)[*num - 1] = strdup(ptr.s->type);
			break;
		case UCI_TYPE_OPTION:
			/*uci_show_value(ptr.o);*/
			switch(ptr.o->type)	
			{
				case UCI_TYPE_STRING:
					UUCI_PRINTF("%s\n", ptr.o->v.string);
					++(*num);
					(*array) = realloc(*array, (*num)*4);
					(*array)[*num - 1] = strdup(ptr.o->v.string);
					break;
				case UCI_TYPE_LIST:
					uci_foreach_element(&ptr.o->v.list, elem)
					{
						UUCI_PRINTF("%s\n", elem->name);
						++(*num);
						(*array) = realloc(*array, (*num)*4);
						(*array)[*num - 1] = strdup(elem->name);

					}
			}
			break;
		default:
			UUCI_PRINTF("unknown\n");
			break;
	}
	uci_free_context(ctx);
	UFREE(str);
	return 0;
}
/*
 *Writes changes of the given configuration file, 
 * or if none is given, all configuration files, 
 * to the filesystem. All "uci set", "uci add", 
 * "uci rename" and "uci delete" commands are staged 
 * into a temporary location and written to flash at 
 * once with "uci commit". This is not needed after 
 * editing configuration files with a text editor, 
 * but for scripts, GUIs and other programs working 
 * directly with UCI files.
 */
int uuci_commit(char *s)
{
	struct uci_context *ctx;
	struct uci_ptr ptr;
	char *str = NULL;
	char *dest = NULL;
	
	if (!s) {
		UUCI_PRINTF("s is null\n");
		return -1;
	}

	str = strdup(s);
	if (!str) {
		UUCI_PRINTF("str is null\n");
		return -1;
	}

	ctx = uci_alloc_context();
	if (!ctx) {
		UUCI_PRINTF("alloc\n");
		UFREE(str);
		return -1;
	}

	if (uci_lookup_ptr(ctx, &ptr, str, true) != UCI_OK) {
		UUCI_PRINTF("uci_lookup_ptr error\n");
		uci_get_errorstr(ctx, &dest, "errmsg");
		UUCI_PRINTF("%s\n", dest);
		UFREE(str);
		UFREE(dest);
		uci_free_context(ctx);
		return -1;
	}

	if (uci_commit(ctx, &ptr.p, false) != UCI_OK) {
		uci_get_errorstr(ctx, &dest, "errmsg");
		UUCI_PRINTF("%s\n", dest);
		UFREE(str);
		UFREE(dest);
		uci_free_context(ctx);
		return -1;
		
	}
	return 0;
}
