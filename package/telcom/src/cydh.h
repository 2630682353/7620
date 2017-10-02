#ifndef __CY_DH_H__
#define __CY_DH_H__

#define BIT_LEN_OF_PRIME			(128)
#define BYTE_LEN_OF_PRIME			(BIT_LEN_OF_PRIME / 8)   //=64
//#define BIT_LEN_OF_PRIME			(512)
int main_test(int argc, char *argv[]);
int cybn_to_base64(BIGNUM *n, uint8_t *str);
int dh_new_key(DH **k, int prime_len, BIGNUM *p, int generator);
int  get_share(BIGNUM *pubkey, DH *k, uint8_t *key, uint32_t secret_len);
void disp_bn(BIGNUM *n, char *ctxt);

#endif

