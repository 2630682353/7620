#define _FILE_OFFSET_BITS 64
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <dirent.h>
#include <unistd.h>
#include <sys/stat.h>
#include <stdlib.h>
#include "nlk_ipc.h"
#include "sqlite3.h"
#include "igd_ndisk.h"
#include "igd_ndisk_db.h"

extern int ndisk_dir_check(sqlite3 *db, char *root, char *dir, char *name, int sub);

#define NDISK_CHECK_FIRST  0
#define NDISK_CHECK_SECOND  1
#define NDISK_CHECK_NOCHANGE  2

#define NDISK_DB_VER    "1"
#define NDISK_DB_DIR    ".nddb"
#define NDISK_DB_FILE   ".nddb.db"

#define NDISK_DB_INSERT_INFO(db, DIR, NAME, TYPE, SIZE, CTIME, STATE)  \
	sql_set(db, "INSERT into NDISK " \
		"("NTI_DIR", "NTI_NAME", "NTI_TYPE", "NTI_SIZE", "NTI_CTIME", "NTI_STATE")" \
		"VALUES " \ 
		"(%Q, %Q, %Q, %llu, %lu, %Q);", DIR, NAME, TYPE, SIZE, CTIME, STATE)

#define NDISK_DB_UPDATA_INFO(db, DIR, NAME, TYPE, SIZE, CTIME, STATE)  \
	sql_set(db, "UPDATE NDISK set " \
			NTI_TYPE" = %Q, " \
			NTI_SIZE" = %llu, " \
			NTI_CTIME" = %lu, " \
			NTI_STATE" = %Q " \
			"where "NTI_DIR" = %Q and "NTI_NAME" = %Q;", \
			TYPE, SIZE, CTIME, STATE, DIR, NAME);

#define NTI_ID     "ID"
#define NTI_DIR    "DIR"
#define NTI_NAME   "NAME"
#define NTI_TYPE   "TYPE"
#define NTI_SIZE   "SIZE"
#define NTI_CTIME  "CTIME"
#define NTI_STATE  "STATE"

struct ndisk_table_info {
	char *id;
	char *dir;
	char *name;
	char *type;
	char *size;
	char *ctime;
	char *state;
};

int n_find(char *path,
	int (*fun)(char *, void *), void *data)
{
	DIR *dir;
	struct dirent *d;
	int ret = 0;

	dir = opendir(path);
	if (!dir)
		return -1;

	while (1) {
		d = readdir(dir);
		if (!d)
			break;
		if (!strcmp(".", d->d_name))
			continue;
		if (!strcmp("..", d->d_name))
			continue;
		ret = fun(d->d_name, data);
		if (ret)
			break;
	}
	closedir(dir);
	return ret;
}

static char *audio_suffix[] = {".mp3", ".wav", ".cda",
	".aiff", ".au", ".mid", ".wma", ".ra", ".vqf",
	".ogg", ".aac",	".ape", ".flac", ".wv", NULL};

static char *video_suffix[] = {".avi", ".mp4", ".mov", 
	".asf", ".wma", ".3gp", ".mpg", ".swf", ".flv", 
	".rm", ".mkv", ".rmvb", ".wmv", ".mpeg", ".asf",
	".navi", NULL};

static char *pic_suffix[] = {".bmp", ".gif", ".jpeg",
	".jpg", ".png", NULL};

static char *txt_suffix[] = {".txt", ".doc", ".docx", 
	".wps", ".pdf", ".ppt", ".xls", NULL};

static char *pack_suffix[] = {".7z", ".rar", ".zip",
	".gz", ".tar", NULL};

static int file_check_type(char *arr[], char *name)
{
	int i = 0;

	while (arr[i]) {
		if(!strcasecmp(name, arr[i]))
			return 1;
		i++;
	}
	return 0;
}

static char *get_file_type(char *name)
{
	char *ptr;

	ptr = strrchr(name, '.');
	if (!ptr) {
		return NDFT_OTHER;
	} else if (file_check_type(audio_suffix, ptr)) {
		return NDFT_AUDIO;
	} else if (file_check_type(video_suffix, ptr)) {
		return NDFT_VIDEO;
	} else if (file_check_type(pic_suffix, ptr)) {
		return NDFT_PIC;
	} else if (file_check_type(txt_suffix, ptr)) {
		return NDFT_TEXT;
	} else if (file_check_type(pack_suffix, ptr)) {
		return NDFT_PACK;
	}
	return NDFT_OTHER;
}

static int sql_set(
	sqlite3 *db,
	const char *fmt, ...)
{
	int ret;
	char *sql;
	va_list ap;
	char *errMsg = NULL;

	va_start(ap, fmt);
	sql = sqlite3_vmprintf(fmt, ap);
	va_end(ap);
	ret = sqlite3_exec(db, sql, NULL, NULL, &errMsg);
	if (ret != SQLITE_OK)
		NDISK_ERR("%d [%s]\n%s\n", ret, errMsg, sql);
	if (errMsg)
		sqlite3_free(errMsg);
	//NDISK_DBG("[%s]\n", sql);
	sqlite3_free(sql);
	return ret;
}

static int sql_str_cb(
	void *data, int argc, char **argv, char **azColName)
{
	int i;
	char **str = (char **)data;

	if (*str)
		return -1;

	for (i = 0; i < argc; i++) {
		if (!argv[i])
			continue;
		*str = strdup(argv[i]);
		//NDISK_DBG("%s = %s\n", azColName[i], argv[i]);
		break;
	}
	return 0;
}

static char *sql_get_str(
	sqlite3 *db,
	char *str, int len,
	const char *fmt, ...)
{
	char *sql, *data = NULL;
	va_list ap;
	char *errMsg = NULL;

	va_start(ap, fmt);
	sql = sqlite3_vmprintf(fmt, ap);
	va_end(ap);
	if (sqlite3_exec(db, sql,
		sql_str_cb, &data, &errMsg) != SQLITE_OK) {
		NDISK_ERR("[%s]\n%s\n", errMsg, sql);
	}
	if (errMsg)
		sqlite3_free(errMsg);
	sqlite3_free(sql);
	if (!data)
		return NULL;
	strncpy(str, data, len - 1);
	str[len - 1] = '\0';
	free(data);
	return str;
}

static int sql_get_nti(
	sqlite3 *db,
	struct ndisk_table_info *nti, int nr,
	const char *fmt, ...)
{
	char *sql;
	va_list ap;
	char *errMsg = NULL;
	char **res = NULL;
	int row = 0, column = 0, num = 0;
	int r = 0, c = 0;

	va_start(ap, fmt);
	sql = sqlite3_vmprintf(fmt, ap);
	va_end(ap);
	if (sqlite3_get_table(db, sql, &res,
		&row, &column, &errMsg) != SQLITE_OK) {
		NDISK_ERR("[%s]\n%s\n", errMsg, sql);
		goto end;
	}
	//NDISK_DBG("row:%d, cloumn:%d\n", row, column);

	for (r = 0; r < row; r++) {
		for (c = 0; c < column; c++) {
			//NDISK_DBG("%s = %s\n", res[c], res[(r + 1)*column + c]);
			if (!strcmp(NTI_ID,  res[c])) {
				nti[num].id = strdup(res[(r + 1)*column + c]);
			} else if (!strcmp(NTI_DIR,  res[c])) {
				nti[num].dir = strdup(res[(r + 1)*column + c]);
			} else if (!strcmp(NTI_NAME,  res[c])) {
				nti[num].name = strdup(res[(r + 1)*column + c]);
			} else if (!strcmp(NTI_TYPE,  res[c])) {
				nti[num].type = strdup(res[(r + 1)*column + c]);
			} else if (!strcmp(NTI_SIZE,  res[c])) {
				nti[num].size = strdup(res[(r + 1)*column + c]);
			} else if (!strcmp(NTI_CTIME,  res[c])) {
				nti[num].ctime = strdup(res[(r + 1)*column + c]);
			} else if (!strcmp(NTI_STATE,  res[c])) {
				nti[num].state = strdup(res[(r + 1)*column + c]);
			} else {
				NDISK_ERR("does not support, %s = %s\n",
					res[c], res[(r + 1)*column + c]);
			}
		}
		num++;
		if (num >= nr)
			break;
	}

end:
	if (errMsg)
		sqlite3_free(errMsg);
	sqlite3_free_table(res);
	sqlite3_free(sql);
	return num;
}

static void sql_free_nti(struct ndisk_table_info *nti)
{
	if (nti->id) {
		free(nti->id);
		nti->id = NULL;
	}
	if (nti->dir) {
		free(nti->dir);
		nti->dir = NULL;
	}
	if (nti->name) {
		free(nti->name);
		nti->name = NULL;
	}
	if (nti->type) {
		free(nti->type);
		nti->type = NULL;
	}
	if (nti->size) {
		free(nti->size);
		nti->size = NULL;
	}
	if (nti->ctime) {
		free(nti->ctime);
		nti->ctime = NULL;
	}
	if (nti->state) {
		free(nti->state);
		nti->state = NULL;
	}
}

static sqlite3 *ndisk_db_check(char *db_file)
{
	sqlite3 *db = NULL;
	char ver[32] = {0,};

	if (sqlite3_open(db_file, &db) != SQLITE_OK)
		return NULL;
	sqlite3_busy_timeout(db, 5000);
	if (!sql_get_str(db, ver,
		sizeof(ver), "pragma user_version;"))
		goto err;
	if (strncmp(ver, NDISK_DB_VER, sizeof(ver))) {
		NDISK_DBG("VERSION:[%s], [%s]\n", ver, NDISK_DB_VER);
		goto err;
	}
	return db;
err:
	if (db)
		sqlite3_close(db);
	return NULL;
}

static sqlite3 *ndisk_db_create(char *db_file)
{
	sqlite3 *db = NULL;

	remove(db_file);
	if (sqlite3_open(db_file, &db) != SQLITE_OK) {
		NDISK_ERR("create db fail, %s, %s\n",
			db_file, sqlite3_errmsg(db));
		sqlite3_close(db);
		return NULL;
	}
	NDISK_DBG("%s, create success\n", db_file);
	sqlite3_busy_timeout(db, 5000);
	sql_set(db, "pragma page_size = 4096;");
	sql_set(db, "pragma journal_mode = OFF;");
	sql_set(db, "pragma synchronous = OFF;");
	sql_set(db, "pragma default_cache_size = 8192;");
	sql_set(db, "pragma auto_vacuum = 1;");
	if (sql_set(db, "CREATE TABLE NDISK( "
		"%s INTEGER PRIMARY KEY AUTOINCREMENT,"
		"%s TEXT NOT NULL,"
		"%s TEXT,"
		"%s TEXT NOT NULL, "
		"%s INTEGER, "
		"%s INTEGER, "
		"%s TEXT);", NTI_ID, NTI_DIR,
		NTI_NAME, NTI_TYPE, NTI_SIZE,
		NTI_CTIME, NTI_STATE)) {
		sqlite3_close(db);
		return NULL;
	}
	return db;
}

static int ndisk_db_del(sqlite3 *db)
{
	int i = 0;
	struct ndisk_table_info nti;

	memset(&nti, 0, sizeof(nti));
	while (1 == sql_get_nti(db, &nti, 1,
				"select * from NDISK "
				"where "NTI_STATE" = %Q and "NTI_TYPE" = %Q limit 1 offset %d;",
				NDDS_DEL, NDFT_DIR, i)) {

		NDISK_DBG("DELETE [%s] [%s]\n", nti.dir, nti.name);

		sql_set(db, "DELETE from NDISK "
					"where "NTI_DIR" glob '%s/%s*';",
					strcmp(nti.dir, "/") ? nti.dir : "", nti.name);
		sql_free_nti(&nti);
		i++;
	}
	sql_set(db, "DELETE from NDISK "
				"where "NTI_STATE" = %Q;", NDDS_DEL);
	sql_free_nti(&nti);
	return 0;
}

static int ndisk_dir_loop(
	sqlite3 *db, char *root, char *path, int sub)
{
	DIR *dir_hld;
	struct dirent *d;
	char *new_dir = path + strlen(root);

	dir_hld = opendir(path);
	if (!dir_hld) {
		NDISK_ERR("opendir fail, %s, %s\n", path, strerror(errno));
		return -1;
	}

	NDISK_DBG("LOOP: NEW_DIR:[%s] [%d]\n", new_dir, sub);

	while (1) {
		d = readdir(dir_hld);
		if (!d)
			break;
		if (!strcmp(".", d->d_name))
			continue;
		if (!strcmp("..", d->d_name))
			continue;
		if (!strcmp(NDISK_DB_DIR, d->d_name) && !strcmp(new_dir, "/"))
			continue;
		ndisk_dir_check(db, root, new_dir, d->d_name, sub);
	}
	closedir(dir_hld);
	return 0;
}

int ndisk_dir_check(
	sqlite3 *db, char *root, char *dir, char *name, int sub)
{
	struct stat st;
	char file[NDISK_PATH_MX] = {0,}, new_dir[NDISK_PATH_MX] = {0,};
	struct ndisk_table_info nti;

	memset(&nti, 0, sizeof(nti));
	snprintf(new_dir, sizeof(new_dir), "%s/%s",
		strcmp(dir, "/") ? dir : "", name);
	snprintf(file, sizeof(file), "%s%s", root, new_dir);

	if (lstat(file, &st)) {
		NDISK_ERR("lstat fail, %s, %s\n", file, strerror(errno));
		return -1;
	}

	if (1 != sql_get_nti(db, &nti, 1,
			"select * from NDISK "
			"where %s = %Q "
			"and %s = %Q;",
			NTI_DIR, dir, NTI_NAME, name)) {
		NDISK_DBG("INSERT DIR:[%s] NAME:[%s] [%d]\n", dir, name, sub);
		if (S_ISDIR(st.st_mode)) {
			st.st_ctime = 0;
			NDISK_DB_INSERT_INFO(db, dir, name,
				NDFT_DIR, st.st_size, st.st_ctime, NDDS_RELOAD);
			if (!sub)
				goto next_layer;
		} else if (S_ISLNK(st.st_mode)) {
			NDISK_DB_INSERT_INFO(db, dir, name,
				NDFT_LINK, st.st_size, st.st_ctime, NDDS_KEEP);
		} else {
			NDISK_DB_INSERT_INFO(db, dir, name,
				get_file_type(name), st.st_size, st.st_ctime, NDDS_KEEP);
		}
		goto end;
	}

	if (st.st_ctime == atol(nti.ctime) && st.st_ctime) {
		NDISK_DBG("ctime same, [%s] [%s] [%s] [%d], [%ld, %ld, %ld]\n",
			dir, name, nti.state, sub, st.st_ctime, st.st_atime, st.st_mtime);
		if (!strcmp(nti.state, NDDS_KEEP))
			goto end;
		else
			sub = NDISK_CHECK_NOCHANGE;
	}

	if (S_ISDIR(st.st_mode)) {
		NDISK_DBG("UPDATA DIR:[%s] NAME:[%s] [%d]\n", dir, name, sub);
		if (sub == NDISK_CHECK_FIRST) {
			goto next_layer;
		} else if (sub == NDISK_CHECK_SECOND) {
			st.st_ctime = 0;
			NDISK_DB_UPDATA_INFO(db, dir, name,
				NDFT_DIR, st.st_size, st.st_ctime, NDDS_RELOAD);
		} else if (sub == NDISK_CHECK_NOCHANGE) {
			NDISK_DB_UPDATA_INFO(db, dir, name,
				NDFT_DIR, st.st_size, st.st_ctime, NDDS_DIR_RELOAD);
		}
		goto end;
	} else if (S_ISLNK(st.st_mode)) {
		NDISK_DB_UPDATA_INFO(db, dir, name,
			NDFT_LINK, st.st_size, st.st_ctime, NDDS_KEEP);
		goto end;
	} else {
		NDISK_DB_UPDATA_INFO(db, dir, name,
			get_file_type(name), st.st_size, st.st_ctime, NDDS_KEEP);
		goto end;
	}

next_layer:
	sql_set(db, "UPDATE NDISK set "NTI_STATE" = %Q "
				"where "NTI_DIR" = %Q;", NDDS_DEL, new_dir);

	if (!ndisk_dir_loop(db, root, file, NDISK_CHECK_SECOND)) {
		NDISK_DB_UPDATA_INFO(db, dir, name,
			NDFT_DIR, st.st_size, st.st_ctime, NDDS_KEEP);
	}
	ndisk_db_del(db);

end:
	sql_free_nti(&nti);
	return 0;
}

static int ndisk_db_refresh(sqlite3 *db, char *root)
{
	struct ndisk_table_info nti;

	memset(&nti, 0, sizeof(nti));
	ndisk_dir_check(db, root, "/", "", NDISK_CHECK_FIRST);

	while (1) {
		if (1 != sql_get_nti(db, &nti, 1, "select * from NDISK "
				"where "NTI_STATE" = %Q limit 1;", NDDS_RELOAD)) {
			break;
		}
		NDISK_DBG("[%s], [%s], [%s], [%s], [%s], [%s]\n", nti.dir,
			nti.name, nti.type, nti.size, nti.ctime, nti.state);

		ndisk_dir_check(db, root, nti.dir, nti.name, NDISK_CHECK_FIRST);
		sql_free_nti(&nti);
	}
	sql_free_nti(&nti);
	return 0;
}

sqlite3 *ndisk_db_update(char *path)
{
	char db_file[512];
	sqlite3 *db = NULL;

	snprintf(db_file, sizeof(db_file),
		"%s/%s", path, NDISK_DB_DIR);
	if (access(db_file, F_OK))
		mkdir(db_file, 0777);

	snprintf(db_file, sizeof(db_file),
		"%s/%s/%s", path, NDISK_DB_DIR, NDISK_DB_FILE);
	db = ndisk_db_check(db_file);
	if (!db) {
		db = ndisk_db_create(db_file);
		if (!db)
			return NULL;
		sql_set(db, "pragma user_version = %s;", NDISK_DB_VER);
	}
	NDISK_DBG("check success PATH:%s\n", path);

	if (ndisk_db_refresh(db, path) < 0) {
		sqlite3_close(db);
		return NULL;
	}
	return db;
}

int ndisk_db_file_info(
	char *path, struct ndisk_file_info *info, int len)
{
	sqlite3 *db = NULL;

	db = ndisk_db_update(path);
	if (!db)
		return -1;

	return 0;
}
