#ifndef __IGD_NDISK_FILES_H__
#define __IGD_NDISK_FILES_H__

//NDFT: NDISK file type
#define NDFT_DIR       "DIR"
#define NDFT_LINK      "LINK"
#define NDFT_OTHER     "FILE/OTHER"
#define NDFT_AUDIO     "FILE/AUDIO"
#define NDFT_VIDEO     "FILE/VIDEO"
#define NDFT_PIC       "FILE/PIC"
#define NDFT_TEXT      "FILE/TEXT"
#define NDFT_PACK      "FILE/PACK"

//NDDS: NDISK dir state
#define NDDS_DEL           "DEL"
#define NDDS_KEEP          "KEEP"
#define NDDS_RELOAD        "RELOAD"
#define NDDS_DIR_RELOAD    "DIR_RELOAD"

#define NDISK_PATH_MX 4096


extern int ndisk_db_file_info(char *path, struct ndisk_file_info *info, int len);

#endif
