#include <stdio.h>
#include <sys/utsname.h>

#include "superblocks.h"
#include "linux_version.h"

#if 0
#define DEBUG(fmt, ...) printf(fmt, __VA_ARGS__)
#else
#define DEBUG(fmt, ...)
#endif

#define isspace(x) ((x == ' ') || (x == '\t') || (x == '\r') || (x == '\n'))

int blkid_debug_mask = 0;

static unsigned char *probe_buffer;
static unsigned int probe_buffer_size = 0;
extern __off64_t lseek64 (int __fd, __off64_t __offset, int __whence) __THROW;

#define DISK_GPT_PT_SIZE	128
#define DISK_GPT_CD_LEN		72
#define DISK_GPT_CD_OFFSET		0x38

unsigned char GPT_EFI_UID[16] = {
	0x28, 0x73, 0x2A, 0xC1, 0x1F, 0xF8, 0xD2, 0x11, 0xBA, 0x4B, 0x00, 0xA0, 0xC9, 0x3E, 0xC9, 0x3B
};

unsigned char GPT_EFI_UID_INTEL[16] = {
	0xC1, 0x2A, 0x73, 0x28, 0xF8, 0X1F, 0x11, 0xD2, 0xBA, 0x4B, 0x00, 0xA0, 0xC9, 0x3E, 0xC9, 0x3B
};

unsigned char GPT_MSR_UID[16] = {
	0x16, 0xE3, 0xC9, 0xE3, 0x5C, 0x0B, 0xB8, 0x4D, 0x81, 0x7D, 0xF9, 0x2D, 0xF0, 0x02, 0x15, 0xAE
};

const unsigned char GPT_PT_MSR[DISK_GPT_CD_LEN]  = {
	0x4D, 0x00, 0x69, 0x00, 0x63, 0x00, 0x72, 0x00, 0x6F, 0x00, 0x73, 0x00, 0x6F, 0x00, 0x66, 0x00, 
	0x74, 0x00, 0x20, 0x00, 0x72, 0x00, 0x65, 0x00, 0x73, 0x00, 0x65, 0x00, 0x72, 0x00, 0x76, 0x00, 
	0x65, 0x00, 0x64, 0x00, 0x20, 0x00, 0x70, 0x00, 0x61, 0x00, 0x72, 0x00, 0x74, 0x00, 0x69, 0x00, 
	0x74, 0x00, 0x69, 0x00, 0x6F, 0x00, 0x6E, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

const unsigned char GPT_PT_EFI[DISK_GPT_CD_LEN] = {
	0x45, 0x00, 0x46, 0x00, 0x49, 0x00, 0x20, 0x00, 0x53, 0x00, 0x79, 0x00, 0x73, 0x00, 0x74, 0x00, 
	0x65, 0x00, 0x6D, 0x00, 0x20, 0x00, 0x50, 0x00, 0x61, 0x00, 0x72, 0x00, 0x74, 0x00, 0x69, 0x00, 
	0x74, 0x00, 0x69, 0x00, 0x6F, 0x00, 0x6E, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

const unsigned char GPT_PT_EFI_WIN[DISK_GPT_CD_LEN] = {
	0x45, 0x00, 0x46, 0x00, 0x49, 0x00, 0x20, 0x00, 0x73, 0x00, 0x79, 0x00, 0x73, 0x00, 0x74, 0x00, 
	0x65, 0x00, 0x6D, 0x00, 0x20, 0x00, 0x70, 0x00, 0x61, 0x00, 0x72, 0x00, 0x74, 0x00, 0x69, 0x00, 
	0x74, 0x00, 0x69, 0x00, 0x6F, 0x00, 0x6E, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

int get_linux_version (void)
{
    static int kver = -1;
    struct utsname uts;
    int major = 0;
    int minor = 0;
    int teeny = 0;
    int n;

    if (kver != -1)
        return kver;
    if (uname (&uts))
        return kver = 0;

    n = sscanf(uts.release, "%d.%d.%d", &major, &minor, &teeny);
    if (n < 1 || n > 3)
        return kver = 0;

    return kver = KERNEL_VERSION(major, minor, teeny);
}

int print_block_info(struct blkid_struct_probe *pr)
{
	printf("%s:", pr->dev);
	if (pr->uuid[0])
		printf(" UUID=\"%s\"", pr->uuid);

	if (pr->label[0])
		printf(" LABEL=\"%s\"", pr->label);

	if (pr->name[0])
		printf(" NAME=\"%s\"", pr->name);

	if (pr->version[0])
		printf(" VERSION=\"%s\"", pr->version);

	printf(" TYPE=\"%s\"\n", pr->id->name);

	return 0;
}


int blkid_probe_is_tiny(blkid_probe pr)
{
    /* never true ? */
    return 0;
}

int blkid_probe_set_value(blkid_probe pr, const char *name,
                          unsigned char *data, size_t len)
{
    /* empty stub */
    return 0;
}

int blkid_probe_set_version(blkid_probe pr, const char *version)
{
    int len = strlen(version);
    if (len > (sizeof(pr->version) - 1))
    {
        fprintf(stderr, "version buffer too small %d\n", len);
        return -1;
    }

    strncpy(pr->version, version, sizeof(pr->version));

    return 0;
}

int blkid_probe_sprintf_version(blkid_probe pr, const char *fmt, ...)
{
    va_list ap;
    int n;

    va_start(ap, fmt);
    n = vsnprintf(pr->version, sizeof(pr->version), fmt, ap);
    va_end(ap);

    if (n >= sizeof(pr->version))
        fprintf(stderr, "version buffer too small %d\n", n);

    return 0;
}

unsigned char *blkid_probe_get_buffer(blkid_probe pr,
                                      blkid_loff_t off, blkid_loff_t len)
{
    int ret;
    unsigned char *buf;

    if (len > probe_buffer_size)
    {
        buf = realloc(probe_buffer, len);

        if (!buf)
        {
            fprintf(stderr, "failed to allocate %d byte buffer\n",
                    (int)len);

            return NULL;
        }

        probe_buffer = buf;
        probe_buffer_size = len;
    }

    memset(probe_buffer, 0, probe_buffer_size);

    lseek64(pr->fd, off, SEEK_SET);
    ret = read(pr->fd, probe_buffer, len);

    if (ret != len)
        fprintf(stderr, "faile to read blkid\n");

    return probe_buffer;
}

int blkid_probe_set_label(blkid_probe pr, unsigned char *label, size_t len)
{
    if (len > (sizeof(pr->label) - 1))
    {
        fprintf(stderr, "label buffer too small %d > %d\n",
                (int) len, (int) sizeof(pr->label) - 1);
        return -1;
    }
    memcpy(pr->label, label, len + 1);

    return 0;
}

int blkid_probe_set_uuid_as(blkid_probe pr, unsigned char *uuid, const char *name)
{
    short unsigned int*u = (short unsigned int*) uuid;

    if (u[0])
        sprintf(pr->uuid,
                "%04x%04x-%04x-%04x-%04x-%04x%04x%04x",
                be16_to_cpu(u[0]), be16_to_cpu(u[1]), be16_to_cpu(u[2]), be16_to_cpu(u[3]),
                be16_to_cpu(u[4]), be16_to_cpu(u[5]), be16_to_cpu(u[6]), be16_to_cpu(u[7]));
    if (name)
        strncpy(pr->name, name, sizeof(pr->name));

    return 0;
}

int blkid_probe_set_uuid(blkid_probe pr, unsigned char *uuid)
{
    return blkid_probe_set_uuid_as(pr, uuid, NULL);
}

int blkid_probe_sprintf_uuid(blkid_probe pr, unsigned char *uuid,
                             size_t len, const char *fmt, ...)
{
    va_list ap;

    va_start(ap, fmt);
    vsnprintf(pr->uuid, sizeof(pr->uuid), fmt, ap);
    va_end(ap);

    return 0;
}

size_t blkid_encode_to_utf8(int enc, unsigned char *dest, size_t len,
                            const unsigned char *src, size_t count)
{
    size_t i, j;
    uint16_t c;

    for (j = i = 0; i + 2 <= count; i += 2)
    {
        if (enc == BLKID_ENC_UTF16LE)
            c = (src[i+1] << 8) | src[i];
        else /* BLKID_ENC_UTF16BE */
            c = (src[i] << 8) | src[i+1];
        if (c == 0)
        {
            dest[j] = '\0';
            break;
        }
        else if (c < 0x80)
        {
            if (j+1 >= len)
                break;
            dest[j++] = (uint8_t) c;
        }
        else if (c < 0x800)
        {
            if (j+2 >= len)
                break;
            dest[j++] = (uint8_t) (0xc0 | (c >> 6));
            dest[j++] = (uint8_t) (0x80 | (c & 0x3f));
        }
        else
        {
            if (j+3 >= len)
                break;
            dest[j++] = (uint8_t) (0xe0 | (c >> 12));
            dest[j++] = (uint8_t) (0x80 | ((c >> 6) & 0x3f));
            dest[j++] = (uint8_t) (0x80 | (c & 0x3f));
        }
    }
    dest[j] = '\0';
    return j;
}

size_t blkid_rtrim_whitespace(unsigned char *str)
{
    size_t i = strlen((char *) str);

    while (i--)
    {
        if (!isspace(str[i]))
            break;
    }
    str[++i] = '\0';
    return i;
}

int blkid_probe_set_utf8label(blkid_probe pr, unsigned char *label,
                              size_t len, int enc)
{
    blkid_encode_to_utf8(enc, (unsigned char *) pr->label, sizeof(pr->label), label, len);
    blkid_rtrim_whitespace((unsigned char *) pr->label);
    return 0;
}


static const struct blkid_idinfo *idinfos[] =
{
    &vfat_idinfo,
    &ntfs_idinfo,
    &exfat_idinfo,
    &hfsplus_idinfo,
    &hfs_idinfo,
};

int probe_block(char *block, struct blkid_struct_probe *pr)
{
    struct stat s;
    int i;

    if (stat(block, &s) || (!S_ISBLK(s.st_mode) && !S_ISREG(s.st_mode)))
        return -1;

    pr->err = -1;
    pr->fd = open(block, O_RDONLY);
    if (!pr->fd)
        return -1;

    for (i = 0; i < ARRAY_SIZE(idinfos); i++)
    {
        /* loop over all magic handlers */
        const struct blkid_idmag *mag;

        /* loop over all probe handlers */
        DEBUG("scanning %s\n", idinfos[i]->name);

        mag = &idinfos[i]->magics[0];

        while (mag->magic)
        {
            int off = (mag->kboff * 1024) + mag->sboff;
            char magic[32] = { 0 };

            lseek(pr->fd, off, SEEK_SET);
            read(pr->fd, magic, mag->len);
            DEBUG("magic: %s %s %d\n", mag->magic, magic, mag->len);
            if (!memcmp(mag->magic, magic, mag->len))
                break;
            mag++;
        }

        if (mag && mag->magic)
        {
            DEBUG("probing %s\n", idinfos[i]->name);
            pr->err = idinfos[i]->probefunc(pr, mag);
            pr->id = idinfos[i];
            strcpy(pr->dev, block);
            if (!pr->err){
		print_block_info(pr);
                break;
            }			
        }
    }
//	print_block_info(pr);

    close(pr->fd);

    return 0;
}

/*
*return Value:
* -1: error
* 1: gpt
* 0: not gpt
*/
int probe_ptable_gpt(char *block)
{
	char devname[256] = {0};
	int fd;	
	unsigned char sector[513] = {0};

#define DISK_FS_TYPE			0x1C2

	if(block == NULL){
		return -1;
	}
	if(strncmp(block, "/dev/", 5) == 0){
		strcpy(devname, block);
	}else{
		sprintf(devname, "/dev/%s", block);
	}
	if(access(devname, F_OK) != 0 ||
			(fd = open(devname, O_RDONLY)) < 0){
		printf("%s Not Exist OR Open %s Failed...", devname, devname);		
		close(fd);
		return -1;
	}
	/*Read Disk File Table*/
	if (512 != read(fd, sector, 512)){
		printf("Read %s first sector failed\n", devname);
		close(fd);
		return -1;
	}
	close(fd);
	if(!(sector[510] == 0x55 && 
		sector[511] == 0xaa)){
		printf("We Think %s Part Table is invaild\n", devname);
		return -1;
	}
	if(sector[DISK_FS_TYPE] == 0xEE ||
			sector[DISK_FS_TYPE] == 0xEF){
		printf("We Think %s Have Partition Talbe and It is GPT...\n", devname);
		return 1;
	}

	return 0;
}

/*
*return Value:
* 1: is efi partition
* 0: is not efi partition
* -1: error
*/
int probe_efi_partition(char *dev, char *pdev)
{
	int ptnum = 0, fd, cur;
	char devname[256] = {0};
	const unsigned char gpt_signature[] = {
					0x45, 0x46, 0x49, 0x20,
					0x50, 0x41, 0x52, 0x54};
	unsigned char sector[513] = {0};
	int siglen = sizeof(gpt_signature)/sizeof(gpt_signature[0]);
	unsigned char ptable[DISK_GPT_PT_SIZE] = {0};
	unsigned char *tpt = NULL;

	
	if(dev == NULL || pdev == NULL){
		return -1;
	}
	if(strncmp(dev, pdev, strlen(dev))){
		printf("Someone Joke me, fuck!!-->%s<=>%s\n", dev, pdev);
		return -1;
	}
	ptnum = atoi(pdev+strlen(dev));
	if(strncmp(dev, "/dev/", 5) == 0){
		strcpy(devname, dev);
	}else{
		sprintf(devname, "/dev/%s", dev);
	}
	
	if(access(devname, F_OK) != 0 ||
			(fd = open(devname, O_RDONLY)) < 0){
		printf("%s Not Exist OR Open %s Failed...", devname, devname);		
		close(fd);
		return -1;
	}
	if(lseek(fd,  512,  SEEK_SET) < 0){
		printf("Seek %s Failed...", devname);		
		close(fd);
		return -1;
	}
	if (512 != read(fd, sector, 512)){
		printf("Read %s first sector failed", devname);		
		close(fd);
		return -1;
	}	
	for(cur=0; cur < siglen; cur++){
		if(sector[cur] != gpt_signature[cur]){
			printf("GPT Signature is Not EFI PART...");			
			close(fd);
			return 0;
		}
	}
	if(lseek(fd,  (ptnum-1)*DISK_GPT_PT_SIZE,  SEEK_CUR) < 0){
		printf("Seek %s PT Failed...", devname);		
		close(fd);
		return -1;
	}
	if (DISK_GPT_PT_SIZE != read(fd, ptable, DISK_GPT_PT_SIZE)){
		printf("Read %s GPT Partition Table Failed..", devname);
		close(fd);
		return -1;
	}	
	close(fd);
	
	if(memcmp(GPT_EFI_UID, ptable, 16) == 0 ||
		memcmp(GPT_EFI_UID_INTEL, ptable, 16) == 0||
		memcmp(GPT_MSR_UID, ptable, 16) == 0){
		printf("Partition EFI System Or MSR...");
		return 1;
	}	
	tpt = ptable+DISK_GPT_CD_OFFSET;
	if(memcmp(GPT_PT_EFI, tpt, DISK_GPT_CD_LEN) == 0||
			memcmp(GPT_PT_EFI_WIN, tpt, DISK_GPT_CD_LEN) == 0){
		printf("Partition EFI System...");
		return 1;
	}
	
	if(memcmp(GPT_PT_MSR, tpt, DISK_GPT_CD_LEN) == 0){
		printf("Partition Is Mircrosoft Reserve...");
		return 1;
	}

	return 0;
}
