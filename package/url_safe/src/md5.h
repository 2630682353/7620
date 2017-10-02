#ifndef _URL_SAFE_MD5_HASH_H_
#define _URL_SAFE_MD5_HASH_H_

/* MD5 context. */
#ifdef  __cplusplus
extern "C" {
#endif

#ifndef PROTOTYPES
#define PROTOTYPES 1
#endif

/* POINTER defines a generic pointer type */
typedef unsigned char *POINTER;

/* UINT2 defines a two byte word */
typedef unsigned short int UINT2;

/* UINT4 defines a four byte word */
typedef unsigned long UINT4;


#if PROTOTYPES
#define PROTO_LIST(list) list
#else
#define PROTO_LIST(list) ()
#endif

typedef struct {
  UINT4 state[4];                                   /* state (ABCD) */
  UINT4 count[2];        /* number of bits, modulo 2^64 (lsb first) */
  unsigned char buffer[64];                         /* input buffer */
} oemMD5_CTX;

void oemMD5Init PROTO_LIST ((oemMD5_CTX *));

void oemMD5Update PROTO_LIST
  ((oemMD5_CTX *, unsigned char const*, unsigned int));

void oemMD5Final PROTO_LIST ((unsigned char [16], oemMD5_CTX *));

#define oemMD5Hash(Hash,Data,Length) \
{\
	oemMD5_CTX context;\
	oemMD5Init(&context);\
	oemMD5Update(&context,Data,Length);\
	oemMD5Final(Hash,&context);\
}

#ifdef  __cplusplus
}
#endif
extern int get_url_md5(unsigned char *in_file, unsigned char *out_file, int len);
#endif
