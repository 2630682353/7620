
#include <stdio.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <openssl/dh.h>
//#include "ecdh128.h"
#include "base64.h"

#include "cydh.h"



#define DEBUG(fmt, args...) do{fprintf(stderr, "cydh:%s:%d "fmt, __FUNCTION__, __LINE__,##args);fprintf(stderr,"\n");}while(0)

//#define BIT_LEN_OF_PRIME			(512)
//#define BIT_LEN_OF_PRIME			(128)

#define GENERATOR				(DH_GENERATOR_2)
/*
P's binary:
0x007fffaf13e440:    95 e6 ef d5 f0 83 7e ef   05 8c 81 44 ac 38 a0 25   |  ......~....D.8.%
0x007fffaf13e450:    9f 5b 70 ef cb 93 67 ae   14 56 df 40 48 69 df c6   |  .[p...g..V.@Hi..
0x007fffaf13e460:    ba 68 8c d1 26 43 6d f3   c9 27 60 49 30 49 18 d5   |  .h..&Cm..'`I0I..
0x007fffaf13e470:    9f 6b e6 44 9f 13 0c fb   e9 3e 9a 24 77 fb 97 0b   |  .k.D.....>.$w...

*/
#define STATIC_P				"\x95\xe6\xef\xd5\xf0\x83\x7e\xef\x05\x8c\x81\x44\xac\x38\xa0\x25" \
							"\x9f\x5b\x70\xef\xcb\x93\x67\xae\x14\x56\xdf\x40\x48\x69\xdf\xc6" \
							"\xba\x68\x8c\xd1\x26\x43\x6d\xf3\xc9\x27\x60\x49\x30\x49\x18\xd5" \
							"\x9f\x6b\xe6\x44\x9f\x13\x0c\xfb\xe9\x3e\x9a\x24\x77\xfb\x97\x0b" 

enum
{
	MODE_INITIATOR = 0,
	MODE_RECEPTOR,
};

static void
print_space(int end_byte_num, void (*print)(const char*string,...))
{
	int i, num;

	num = end_byte_num % 16;
	for(i = num; i < 16; i++)
	{
		if((i % 8) == 0)
			print("  ");
		print("   ");
	}
}

static void
print_char(int end_byte_num, uint8_t *end, void (*print)(const char*string,...))
{
	int i;
	uint8_t *c;
	print("  |  ");
	for(i = 0, c = end - (end_byte_num % 16); i <= end_byte_num % 16; i++, c++)
	{
		if((*c < 0x20) || (*c > 0x7e))
			print(".");
		else
			print("%c", *c);
	}
}

static void
print_hex_8(void *buffer,void *addr,int size,void (*print)(const char*string,...))
{
	int cnt;
	uint8_t *p;

	for(cnt = 0, p = (uint8_t *)buffer; cnt < size; cnt += 1, p++, addr += 1)
	{
		if((cnt % 16) == 0)
			print("%016p:    ", addr);

		if(((cnt % 8) == 0) && ((cnt % 16) != 0))
			print("  ");

		print("%02x ", *p);

		if((cnt % 16) == 0xf)
		{
			print_char(cnt % 16, p, print);
			print("\n");
		}
	}
	if((cnt % 16) != 0)
	{
		print_space(cnt % 16, print);
		print_char((cnt % 16) - 1, p - 1, print);
		print("\n");
	}
}

static void
print_hex_16(void *buffer,void *addr,int size,void (*print)(const char*string,...))
{
	int cnt;
	uint16_t *p;

	for(cnt = 0,p = (uint16_t *)buffer;cnt < size;cnt += 2,p++,addr += 2)
	{
		if((cnt % 16) == 0)
			print("%016p:    ",addr);

		if(((cnt % 8) == 0) && ((cnt % 16) != 0))
			print("  ");

		print("%04x ",*p);

		if((cnt % 16) == 0xe)
		    print("\n");
	}
	print("\n");
}

static void
print_hex_32(void *buffer,void *addr,int size,void (*print)(const char*string,...))
{
	int cnt;
	uint32_t *p;

	for(cnt = 0,p = (uint32_t *)buffer;cnt < size;cnt += 4,p++,addr += 4)
	{
		if((cnt % 16) == 0)
			print("%016p:    ",addr);

		print("%08x ",*p);

		if((cnt % 16) == 0xc)
			print("\n");
	}
	print("\n");
}

static void
print_hex_64(void *buffer,void *addr,int size,void (*print)(const char*string,...))
{
	int cnt;
	uint64_t *p;

	for(cnt = 0,p = (uint64_t*)buffer;cnt < size;cnt += 8,p++,addr += 8)
	{
		if((cnt % 16) == 0)
			print("%016p:    ",addr);

		print("%016lx ",(uint64_t)*p);

		if((cnt % 16) == 0x8)
			print("\n");
	}
	print("\n");
}

static void
print_hex(void *addr,int len,int bitwidth ,void (*print)(const char*string,...))
{
    switch(bitwidth)
    {
    	default:
		case 8:
			print_hex_8(addr,addr,len,print);
			break;
		case 16:
			print_hex_16(addr,addr,len,print);
			break;
		case 32:
			print_hex_32(addr,addr,len,print);
			break;
		case 64:
			print_hex_64(addr,addr,len,print);
			break;	
	};
}

static void
printhex(void *s, void *p, int len)
{
	if(s)
		printf("%s", (char*)s);
	print_hex(p, len, 8, (void (*)(const char*,...))printf);
}

static int
print_BN(BIGNUM *a)
{
	int n, i, idx;
	uint64_t l;
	unsigned char buf[1024], *p = buf;

	bn_check_top(a);
	n = i = BN_num_bytes(a);
	idx = 0;
	while (i--)
	{
		l = a->d[i / BN_BYTES];
		*(p++) = (unsigned char)(l >> (8 * (i % BN_BYTES))) & 0xff;
		idx++;
	}
	*p = '\0';

	for(idx = 0; idx < n / BN_BYTES + 1; idx++)
	{
		printf("0x%016llx ", (uint64_t)(a->d[idx]));
	}
	printf("\n");
	printhex("char:\n", buf, n);
	
	return (n);
}

//0 OK 1 FAILED
int cybn_to_base64(BIGNUM *n, uint8_t *str)
{
	uint8_t bin[1024];
	//uint8_t str[1024];

	char *dec;
	int len;
	int ret=1;
	
	//print_BN(n);
	/*
	dec = BN_bn2dec(n);
	if(dec)
	{
		printf("decimal format:%s\n", dec);
		OPENSSL_free(dec);
	}
	*/
	if((len = BN_bn2bin(n, bin)) > 0)
	{
		if((len = BASE64_Encode(bin, len, str)) > 0)
		{
			str[len] = '\0';
			printf("base64 format:%s\n", str);
			return 0;
		}
	}
	return  1;
}


void disp_bn(BIGNUM *n, char *ctxt)
{
	uint8_t bin[1024], str[1024];
	char *dec;
	int len;

	if(!n)
		return;

	if(ctxt)
		printf("%s", ctxt);


	
	print_BN(n);
	
	dec = BN_bn2dec(n);
	if(dec)
	{
		printf("decimal format:%s\n", dec);
		OPENSSL_free(dec);
	}
	
	if((len = BN_bn2bin(n, bin)) > 0)
	{
		if((len = BASE64_Encode(bin, len, str)) > 0)
		{
			str[len] = '\0';
			printf("base64 format:%s\n", str);
		}
	}
}

#if 0
static void
handleErrors(const char *s, int l)
{
	printf("%s:%d Error occurred.\n", s, l);
}
#else
//#define handleErrors(s, l)	do{printf("%s:%d Error occurred.\n", s, l); goto fail;}while(0)
#define handleErrors(s, l)	do{fprintf(stderr,"\n");fprintf(stderr, "CYdh_ERROR:%s:%d ",__FUNCTION__, __LINE__);}while(0)
#endif

#if 0
//#define DHerr(x, y)
static int compute_key(unsigned char *key, const BIGNUM *pub_key, DH *dh)
{
    BN_CTX *ctx = NULL;
    BN_MONT_CTX *mont = NULL;
    BIGNUM *tmp;
    int ret = -1;
    int check_result;

    if (BN_num_bits(dh->p) > OPENSSL_DH_MAX_MODULUS_BITS) {
        DHerr(DH_F_COMPUTE_KEY, DH_R_MODULUS_TOO_LARGE);
	printf("%s:%d\n", __FUNCTION__, __LINE__);
        goto err;
    }

    ctx = BN_CTX_new();
    if (ctx == NULL)
    	{
    	printf("%s:%d\n", __FUNCTION__, __LINE__);
        goto err;
    	}
    BN_CTX_start(ctx);
    tmp = BN_CTX_get(ctx);

    if (dh->priv_key == NULL) {
        DHerr(DH_F_COMPUTE_KEY, DH_R_NO_PRIVATE_VALUE);
		printf("%s:%d\n", __FUNCTION__, __LINE__);
        goto err;
    }

    if (dh->flags & DH_FLAG_CACHE_MONT_P) {
        mont = BN_MONT_CTX_set_locked(&dh->method_mont_p,
                                      CRYPTO_LOCK_DH, dh->p, ctx);
        if ((dh->flags & DH_FLAG_NO_EXP_CONSTTIME) == 0) {
            /* XXX */
            BN_set_flags(dh->priv_key, BN_FLG_CONSTTIME);
        }
        if (!mont)
        	{
        	printf("%s:%d\n", __FUNCTION__, __LINE__);
            goto err;
        	}
    }

    if (!DH_check_pub_key(dh, pub_key, &check_result) || check_result) {
		printf("%s:%d\n", __FUNCTION__, __LINE__);
        DHerr(DH_F_COMPUTE_KEY, DH_R_INVALID_PUBKEY);
        goto err;
    }

    if (!dh->
        meth->bn_mod_exp(dh, tmp, pub_key, dh->priv_key, dh->p, ctx, mont)) {
        printf("%s:%d\n", __FUNCTION__, __LINE__);
        DHerr(DH_F_COMPUTE_KEY, ERR_R_BN_LIB);
        goto err;
    }

    ret = BN_bn2bin(tmp, key);
	printf("%s:%d\n", __FUNCTION__, __LINE__);
 err:
    if (ctx != NULL) {
        BN_CTX_end(ctx);
        BN_CTX_free(ctx);
    }
    return (ret);
}
#endif

int dh_new_key(DH **k, int prime_len, BIGNUM *p, int generator)
{
	int codes;

	/* Generate the parameters to be used */
	if(NULL == (*k = DH_new())) handleErrors(__FUNCTION__, __LINE__);

#if 0
	printf("%s:%d\n", __FUNCTION__, __LINE__);
	if((*k)->p)
		print_BN((*k)->p);
	if((*k)->g)
		print_BN((*k)->g);
#endif

	if(1 != DH_generate_parameters_ex(*k, prime_len, generator, NULL)) handleErrors(__FUNCTION__, __LINE__);

#if 0
	printf("%s:%d\n", __FUNCTION__, __LINE__);
	if((*k)->p)
		print_BN((*k)->p);
	if((*k)->g)
		print_BN((*k)->g);
#endif

	if(p)
		BN_copy((*k)->p, p);

#if 0
	printf("%s:%d\n", __FUNCTION__, __LINE__);
	if((*k)->p)
		print_BN((*k)->p);
	if((*k)->g)
		print_BN((*k)->g);
#endif

	if(1 != DH_check(*k, &codes)) handleErrors(__FUNCTION__, __LINE__);
	if(codes != 0)
	{
		/* Problems have been found with the generated parameters */
		/* Handle these here - we'll just abort for this example */
		printf("DH_check failed\n");

		if(codes & DH_CHECK_P_NOT_PRIME)
			printf("p value is not prime\n");

		if(codes & DH_CHECK_P_NOT_SAFE_PRIME)
			printf("p value is not a safe prime\n");

		if (codes & DH_UNABLE_TO_CHECK_GENERATOR)
			printf("unable to check the generator value\n");

		if (codes & DH_NOT_SUITABLE_GENERATOR)
			printf("the g value is not a generator\n");

	    goto fail;
	}

#if 0
	printf("%s:%d\n", __FUNCTION__, __LINE__);
	if((*k)->p)
		print_BN((*k)->p);
	if((*k)->g)
		print_BN((*k)->g);
#endif

	/* Generate the public and private key pair */
	if(1 != DH_generate_key(*k)) handleErrors(__FUNCTION__, __LINE__);

#if 0
	printf("%s:%d\n", __FUNCTION__, __LINE__);
	if((*k)->p)
		print_BN((*k)->p);
	if((*k)->g)
		print_BN((*k)->g);
#endif

	printf("DH ok\n");
	
	return 0;

fail:
	if(*k)
	{
		DH_free(*k);
		*k = NULL;
	}
	
	return -2;
}

#if 0
static int
get_key_public(BIGNUM **pubkey, DH *k)
{
	int ret = -1;

	if((*pubkey = BN_new()) == NULL) handleErrors(__FUNCTION__, __LINE__);

	if(BN_copy(*pubkey, k->pub_key) == NULL) handleErrors(__FUNCTION__, __LINE__);

	ret = 0;
	printf("DH get public ok\n");
	return ret;

fail:
	if(*pubkey)
	{
		BN_free(*pubkey);
		*pubkey = NULL;
	}
	return ret;
}
#endif

int  get_share(BIGNUM *pubkey, DH *k, uint8_t *key, uint32_t secret_len)
{
	/* Compute the shared secret */
	//unsigned char *secret;
	int secret_size;
	//if(NULL == (secret = OPENSSL_malloc(sizeof(unsigned char) * (DH_size(k))))) handleErrors(__FUNCTION__, __LINE__);
	if(secret_len < DH_size(k)){
	
		return -1;
	}

	if(0 > (secret_size = /*compute_key*/DH_compute_key(key, pubkey, k))) //handleErrors(__FUNCTION__, __LINE__);
	{
		printf("error size is %d\n", secret_size);
		goto fail;
	}
	
	/* Do something with the shared secret */
	/* Note secret_size may be less than DH_size(privkey) */
	printf("size is %d,The shared secret is:\n", secret_size);
	BIO_dump_fp(stdout, (const char*)key, secret_size);

fail:
	/* Clean up */
	//OPENSSL_free(secret);
	//return 0;
	return secret_size;
}

void
print_usage(char *cmd)
{
	printf("Usage:%s initiator g[2|5]\n", cmd);
	printf("Usage:%s receptor p(base64) g(base64) public(base64)\n", cmd);
}

#if 0
int ecdh_main( int argc, char** argv );
#endif

#if 1


#include <stdio.h>
#include <time.h>
#include <sys/timeb.h>

void print_time()
{
	struct timeb tp;
	struct tm * tm;

	ftime ( &tp );
	tm = localtime (   & ( tp.time )   );

	printf ("time=\n %d:%d:%d(H:M:S)\n", ( tm -> tm_hour ) , ( tm -> tm_min   ) ,
	( tm -> tm_sec   ), ( tp.millitm ) );
}


int main_test(int argc, char *argv[])
{
	print_time();
	DH *k1 = NULL;
	int mode;
	char *g_dec_p;
	uint8_t g_int;
	BIGNUM *p_bn = NULL, *g_bn = NULL;
	uint8_t p_bin[BYTE_LEN_OF_PRIME * 2], g_bin[BYTE_LEN_OF_PRIME * 2];
	uint8_t pub_bin[BYTE_LEN_OF_PRIME * 2], share_bin[BYTE_LEN_OF_PRIME * 2], str[1024];
	int p_bin_len, g_bin_len, pub_bin_len, share_bin_len;
	BIGNUM *pub_bn = NULL, *share_bn = NULL;

	if(argc < 2)
	{
help:
		print_usage(argv[0]);
		return -1;
	}
	
	if(!strcmp(argv[1], "initiator"))
		mode = MODE_INITIATOR;			/* initiator */
	else if(!strcmp(argv[1], "receptor"))
		mode = MODE_RECEPTOR;		/* receptor */
	else
		goto help;

	
	if(mode == MODE_INITIATOR)
	{
		if(argc < 3)
			goto help;

		g_int = atoi(argv[2]);
		if((g_int != DH_GENERATOR_2) && (g_int != DH_GENERATOR_5))
			goto help;

		printf("initiator:G is %d\n", g_int);

		p_bn = NULL;
	}
	else
	{
		if(argc < 5)
			goto help;
		
		p_bin_len = BASE64_Decode((uint8_t*)argv[2], strlen(argv[2]), p_bin);
		if(NULL == (p_bn = BN_bin2bn(p_bin, p_bin_len, NULL))) handleErrors(__FUNCTION__, __LINE__);
		disp_bn(p_bn, "-----pair's P:\n");

		g_bin_len = BASE64_Decode((uint8_t*)argv[3], strlen(argv[3]), g_bin);
		if(NULL == (g_bn = BN_bin2bn(g_bin, g_bin_len, NULL))) handleErrors(__FUNCTION__, __LINE__);
		disp_bn(g_bn, "-----pair's G:\n");

		pub_bin_len = BASE64_Decode((uint8_t*)argv[4], strlen(argv[4]), pub_bin);
		if(NULL == (pub_bn = BN_bin2bn(pub_bin, pub_bin_len, NULL))) handleErrors(__FUNCTION__, __LINE__);
		disp_bn(pub_bn, "-----pair's public key:\n");

		if((g_dec_p = BN_bn2dec(g_bn)) == NULL) handleErrors(__FUNCTION__, __LINE__);
		g_int = atoi(g_dec_p);
		printf("receptor:G is %d\n", g_int);
	}

	printf("========================= running =========================\n");
	if(dh_new_key(&k1, BIT_LEN_OF_PRIME, p_bn, g_int) < 0) handleErrors(__FUNCTION__, __LINE__);

	disp_bn(k1->p, "-----P:\n");
	disp_bn(k1->g, "-----G:\n");
	disp_bn(k1->pub_key, "-----Public key:\n");

	if(mode == MODE_INITIATOR)
	{
		print_time();
		printf("input pair's public key in base64 format\n");
		fgets((char*)str, sizeof(str), stdin);
		printf("pair's public key base64:%s\n", str);

		pub_bin_len = BASE64_Decode(str, strlen((char*)str), pub_bin);
		if(NULL == (pub_bn = BN_bin2bn(pub_bin, pub_bin_len, NULL))) handleErrors(__FUNCTION__, __LINE__);
		disp_bn(pub_bn, "-----pair's public key:\n");
	}

	if((share_bin_len = get_share(pub_bn, k1, share_bin, sizeof(share_bin))) < 0) handleErrors(__FUNCTION__, __LINE__);
	if(NULL == (share_bn = BN_bin2bn(share_bin, share_bin_len, NULL))) handleErrors(__FUNCTION__, __LINE__);
	disp_bn(share_bn, "-----share key\n");


	print_time();
fail:
	if(k1)
		DH_free(k1);
	if(p_bn)
		BN_free(p_bn);
	if(g_bn)
		BN_free(g_bn);
	if(pub_bn)
		BN_free(pub_bn);
	if(share_bn)
		BN_free(share_bn);

	return 0;
}
#elif 0
int
main(int argc, char *argv[])
{
	DH *k1 = NULL;
	BIGNUM *p1 = NULL;
	BIGNUM *pk2 = NULL;
	uint8_t bin[1024], str[1024];
	int len;
	uint8_t *pbin = (uint8_t *)STATIC_P;
	char *dec;
	BIGNUM *bn;

	if(argc > 10)
		return ecdh_main(argc, argv);

	if(NULL == (p1 = BN_new())) handleErrors(__FUNCTION__, __LINE__);
	if(NULL == (BN_bin2bn(pbin, strlen((char*)pbin), p1))) handleErrors(__FUNCTION__, __LINE__);
	

	if(dh_new_key(&k1, LEN_OF_PRIME, p1, GENERATOR) < 0) handleErrors(__FUNCTION__, __LINE__);
	//if(dh_new_key(&k1, LEN_OF_PRIME, NULL, GENERATOR) < 0) handleErrors(__FUNCTION__, __LINE__);

	printf("P:\n");
	print_BN(k1->p);
	dec = BN_bn2dec(k1->p);
	printf("P in decimal:%s\n", dec);
	OPENSSL_free(dec);
	len = BN_bn2bin(k1->p, bin);	
	//printhex("P's binary:\n", bin, len);
	len = BASE64_Encode(bin, len, str);
	str[len] = '\0';
	printf("P's base64 format:%s\n", str);

	printf("G:\n");
	print_BN(k1->g);
	len = BN_bn2bin(k1->g, bin);	
	dec = BN_bn2dec(k1->g);
	printf("G in decimal:%s\n", dec);
	OPENSSL_free(dec);
	//printhex("G's binary:\n", bin, len);
	len = BASE64_Encode(bin, len, str);
	str[len] = '\0';
	printf("G's base64 format:%s\n", str);
	
	printf("Public key:\n");
	print_BN(k1->pub_key);
	len = BN_bn2bin(k1->pub_key, bin);	
	dec = BN_bn2dec(k1->pub_key);
	printf("Public key in decimal:%s\n", dec);
	OPENSSL_free(dec);
	//printhex("Public key's binary:\n", bin, len);
	len = BASE64_Encode(bin, len, str);
	str[len] = '\0';
	printf("Public key's base64 format:%s\n", str);

	printf("input pair's public key in base64 format\n");
	fgets((char*)str, sizeof(str), stdin);
	printf("pair's public key base64:%s\n", str);
	
	len = BASE64_Decode(str, strlen((char*)str), bin);
	//printhex("pair's binary is:\n", bin, len);

	if(NULL == (pk2 = BN_new())) handleErrors(__FUNCTION__, __LINE__);
	//pk2 = NULL;
	if(NULL == (BN_bin2bn(bin, len, pk2))) handleErrors(__FUNCTION__, __LINE__);
	print_BN(pk2);

	if((len = get_share(pk2, k1, bin, sizeof(bin))) < 0) handleErrors(__FUNCTION__, __LINE__);

	bn = BN_bin2bn(bin, len, NULL);
	dec = BN_bn2dec(bn);
	printf("share key in decimal:%s\n", dec);
	OPENSSL_free(dec);
	BN_free(bn);
	
	len = BASE64_Encode(bin, len, str);
	str[len] = '\0';
	printf("share key's base64 format:%s\n", str);

fail:
	if(k1)
		DH_free(k1);
	if(pk2)
		BN_free(pk2);

	return 0;
}
#elif 0
int
main(int argc, char *argv[])
{
	DH *k1 = NULL, *k2 = NULL;
	BIGNUM *pk1 = NULL, *pk2 = NULL;

	if(dh_new_key(&k1, LEN_OF_PRIME, NULL, GENERATOR) < 0) handleErrors(__FUNCTION__, __LINE__);
	if(dh_new_key(&k2, LEN_OF_PRIME, k1->p, GENERATOR) < 0) handleErrors(__FUNCTION__, __LINE__);

	if(BN_cmp(k1->p, k2->p) != 0) handleErrors(__FUNCTION__, __LINE__);
	if(BN_cmp(k1->g, k2->g) != 0) handleErrors(__FUNCTION__, __LINE__);

	if(get_key_public(&pk1, k1) < 0) handleErrors(__FUNCTION__, __LINE__);
	if(get_key_public(&pk2, k2) < 0) handleErrors(__FUNCTION__, __LINE__);

	

	get_share(pk2, k1);
	get_share(pk1, k2);

fail:
	if(k1)
		DH_free(k1);
	if(k2)
		DH_free(k2);

	if(pk1)
		BN_free(pk1);
	if(pk2)
		BN_free(pk2);

	return 0;
}
#else
int
main(int argc, char *argv[])
{
	DH *privkey;
	int codes;
	int secret_size;

	/* Generate the parameters to be used */
	if(NULL == (privkey = DH_new())) handleErrors(__FUNCTION__, __LINE__);
	if(1 != DH_generate_parameters_ex(privkey, 2048, DH_GENERATOR_2, NULL)) handleErrors(__FUNCTION__, __LINE__);

	if(1 != DH_check(privkey, &codes)) handleErrors(__FUNCTION__, __LINE__);
	if(codes != 0)
	{
	    /* Problems have been found with the generated parameters */
	    /* Handle these here - we'll just abort for this example */
	    printf("DH_check failed\n");
	    return -1;
	}

	/* Generate the public and private key pair */
	if(1 != DH_generate_key(privkey)) handleErrors(__FUNCTION__, __LINE__);

	/* Send the public key to the peer.
	 * How this occurs will be specific to your situation (see main text below) */


	/* Receive the public key from the peer. In this example we're just hard coding a value */
	BIGNUM *pubkey = NULL;
	if(0 == (BN_dec2bn(&pubkey, "01234567890123456789012345678901234567890123456789"))) handleErrors(__FUNCTION__, __LINE__);

	/* Compute the shared secret */
	unsigned char *secret;
	if(NULL == (secret = OPENSSL_malloc(sizeof(unsigned char) * (DH_size(privkey))))) handleErrors(__FUNCTION__, __LINE__);

	if(0 > (secret_size = DH_compute_key(secret, pubkey, privkey))) handleErrors(__FUNCTION__, __LINE__);

	/* Do something with the shared secret */
	/* Note secret_size may be less than DH_size(privkey) */
	printf("The shared secret is:\n");
	BIO_dump_fp(stdout, secret, secret_size);

	/* Clean up */
	OPENSSL_free(secret);
	BN_free(pubkey);
	DH_free(privkey);
}
#endif

#if 0
/****************************************** ecdh ***************************************************/

static void disp( const char *str, const void *pbuf, const int size )
{
	int i=0;
	if( str != NULL )
		printf("%s:\n", str);

	if( pbuf != NULL && size > 0) {
		for( i=0; i < size; ++i )
			printf( "%02x ", *((unsigned char *)pbuf+i) );

		putchar('\n');
	}
	putchar('\n');
}

RAW_ECDH_128_KEY* create_ecdh_instance( void ) {
	RAW_ECDH_128_KEY* key = create_raw_ecdh_128_key();
	int ret = gen_raw_ecdh_128_pub_key( key );
	if( 0 != ret )  {
		printf( "gen_raw_ecdh_128_pub_key err:%d.\n", ret );
	}
	return key;
}


int
ecdh_main( int argc, char** argv )
{
	//unsigned char 			TmpPubKey[ ECDH_128_SIZE ];	
	unsigned char			tmp[512], str[512];
	int len;
	int ret = 0;
	RAW_ECDH_128_KEY* key1;
	//RAW_ECDH_128_KEY* key2;

	key1 = create_ecdh_instance();
	//key2 = create_ecdh_instance();

	//disp( "key1->RawPubKey", key1->RawPubKey, sizeof(key1->RawPubKey) );
	//disp( "key2->RawPubKey", key2->RawPubKey, sizeof(key2->RawPubKey) );
#if 0
	printf("sizeof(key1->RawPubKey):%d\n", sizeof(key1->RawPubKey));
	printhex("key1->RawPubKey\n", key1->RawPubKey, sizeof(key1->RawPubKey));

	memset(tmp, 0, sizeof(tmp));
	BASE64_Encode(key1->RawPubKey, sizeof(key1->RawPubKey) , tmp);
	printf("key1 base64:%s\n", tmp);
#endif

#if 0
	if(argc < 2)
	{
		ret = -1;
		goto end;
	}
	len = BASE64_Decode(argv[1], strlen(argv[1]), tmp);
#endif

	printf("input public key in base64 format\n");
	fgets((char*)str, sizeof(str), stdin);
	printf("public key base64:%s\n", str);
	
	len = BASE64_Decode(str, strlen((char*)str), tmp);
	//disp( "key2->RawPubKey", tmp, len);
	//printhex("key2->RawPubKey\n", tmp, len);

	if( 0 != gen_raw_ecdh_shared_key( key1, tmp ) ) 
		printf( "gen_raw_ecdh_128_pub_key err.\n" );


	//if( 0 != gen_raw_ecdh_shared_key( key2, key1->RawPubKey ) ) 
	//	printf( "gen_raw_ecdh_128_pub_key err.\n" );

	disp( "key1->RawSharedKey", key1->RawSharedKey, sizeof(key1->RawSharedKey) );
	//disp( "key2->RawSharedKey", key2->RawSharedKey, sizeof(key2->RawSharedKey) );

	BASE64_Encode(key1->RawSharedKey, sizeof(key1->RawSharedKey) , tmp);
	printf("share key base64:%s\n", tmp);

	//disp( "MD5:", MD5((const unsigned char *)key1->RawSharedKey, (size_t)sizeof(key1->RawSharedKey), NULL), MD5_DIGEST_LENGTH);

//end:
	key1->FreeSelf( (void**)&key1 );
	//key2->FreeSelf( (void**)&key2 );

	return ret;
}

#endif


