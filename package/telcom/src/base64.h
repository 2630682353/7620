#ifndef __BASE64_H__
#define __BASE64_H__

#define	BYTE	unsigned char
#define	CHAR	unsigned char
#define	TCHAR	unsigned char
#define	INT	int
#define	TEXT(x)	x

#define B64_EOLN            0xF0    // 换行/n
#define B64_CR                0xF1    // 回车/r
#define B64_EOF                0xF2    // 连字符-
#define B64_WS                0xE0    // 跳格或者空格（/t、space）
#define B64_ERROR     0xFF    // 错误字符
#define B64_NOT_BASE64(a)    (((a)|0x13) == 0xF3)

INT BASE64_Encode( const BYTE* inputBuffer, INT inputCount, TCHAR* outputBuffer );
INT BASE64_Decode( const TCHAR* inputBuffer, INT inputCount, BYTE* outputBuffer );

#endif

