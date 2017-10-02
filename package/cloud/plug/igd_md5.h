#ifndef md5_H_0005dc37_ae94_4bf3_bff7_c8c0d121bbc3
#define md5_H_0005dc37_ae94_4bf3_bff7_c8c0d121bbc3
/* MD5.H - header file for MD5C.C
 */

/* Copyright (C) 1991-2, RSA Data Security, Inc. Created 1991. All
rights reserved.

License to copy and use this software is granted provided that it
is identified as the "RSA Data Security, Inc. MD5 Message-Digest
Algorithm" in all material mentioning or referencing this software
or this function.

License is also granted to make and use derivative works provided
that such works are identified as "derived from the RSA Data
Security, Inc. MD5 Message-Digest Algorithm" in all material
mentioning or referencing the derived work.

RSA Data Security, Inc. makes no representations concerning either
the merchantability of this software or the suitability of this
software for any particular purpose. It is provided "as is"
without express or implied warranty of any kind.
These notices must be retained in any copies of any part of this
documentation and/or software.
 */

/* MD5 context. */
#ifdef  __cplusplus
extern "C" {
#endif

#ifndef PROTOTYPES
#define PROTOTYPES 1
#endif

#define MD5_HASH_SIZE 16

/* POINTER defines a generic pointer type */
typedef unsigned char *POINTER;

/* UINT2 defines a two byte word */
typedef unsigned short int UINT2;

/* UINT4 defines a four byte word */
typedef unsigned long int UINT4;

/* PROTO_LIST is defined depending on how PROTOTYPES is defined above.
If using PROTOTYPES, then PROTO_LIST returns the list, otherwise it
  returns an empty list.
 */
#if PROTOTYPES
#define PROTO_LIST(list) list
#else
#define PROTO_LIST(list) ()
#endif

//一个结构，用于MD5存放中间结果。其作用类似于句柄，有了它，可以将数据分成多次送入，最后得到MD5 Hash。
typedef struct {
  UINT4 state[4];                                   /* state (ABCD) */
  UINT4 count[2];        /* number of bits, modulo 2^64 (lsb first) */
  unsigned char buffer[64];                         /* input buffer */
} oemMD5_CTX;

//初始化一个句柄。
void oemMD5Init PROTO_LIST ((oemMD5_CTX *));
//对某个句柄送入数据。
void oemMD5Update PROTO_LIST
  ((oemMD5_CTX *, unsigned char const*, unsigned int));
//该句柄数据送入完成，计算Hash。
void oemMD5Final PROTO_LIST ((unsigned char [MD5_HASH_SIZE], oemMD5_CTX *));
//一次计算HasH
#ifdef  __cplusplus
}
#endif

int get_md5_numbers(unsigned char *in_file,unsigned char *out_file,int len);
#endif
