#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <string.h>
#include "pdma-lib.h"
#include "../pdma-ioctl.h"

unsigned char reverse(unsigned char x){
    	unsigned char y;
	y = (x&0x01)<<7;
	y |= (x&0x02)<<5;
	y |= (x&0x04)<<3;
	y |= (x&0x08)<<1;
	y |= (x&0x10)>>1;
	y |= (x&0x20)>>3;
	y |= (x&0x40)>>5;
	y |= (x&0x80)>>7;
	return y;
}

void usage()
{
    printf("pdma-read device [-cnt value] [-dma value] [-d value] [-pt value] [-inc value]\n");
    printf("device: device name, /dev/pdma\n");
    printf("-cnt:  total read count\n");
    printf("-dma:  1: send dma-start command before read; 0: don't send\n");
    printf("-d:    delay between two read system call, measured by us\n");
    printf("-pt:   start pattern to be checked\n");
    printf("-inc:  pattern increase to be checked\n");
    printf("example: ./pdma-read /dev/pdma -cnt 1000000\n");
}

int main(int argc, char *argv[])
{
    struct pdma_rw_reg ctrl;
    struct pdma_info info;
    char *fl = argv[1];
    int fd, ret=0, check_pt=0;
    unsigned int dma_start=0, delay=0, pattern=0, inc=1;
    unsigned long long count=(unsigned long long)(-1);
    unsigned long long total_read_num=0, total_read_size=0;
    unsigned long long tm_beg, tm_end, tm_total;
    char *pval, *buf;
    char *filename = "logdecode.txt";

    /* parse argument */
    if (argc == 1 || !strcmp("-h", argv[1])) {
        usage();
        return 0;
    }

    pval = arg_get_next(argc, argv, "-cnt");
    if (pval) {
        count = memparse(pval, &pval);
    }
    pval = arg_get_next(argc, argv, "-dma");
    if (pval) {
        dma_start = memparse(pval, &pval);
    }
    pval = arg_get_next(argc, argv, "-d");
    if (pval) {
        delay = memparse(pval, &pval);
    }
    pval = arg_get_next(argc, argv, "-pt");
    if (pval) {
        pattern = memparse(pval, &pval);
        check_pt = 1;
    }
    pval = arg_get_next(argc, argv, "-inc");
    if (pval) {
        inc = memparse(pval, &pval);
    }

    /* open */
    fd = open(fl, O_RDWR);
    if (fd == -1) {
        printf("open failed for pdma device %s \n", fl);
        return -1;
    }

    /* get info */
    ret = ioctl(fd, PDMA_IOC_INFO, (unsigned long)&info);
    if (ret == -1) {
        printf("get info failed\n");
        close(fd);
        return -1;
    }

    /* malloc buf */
    buf = (char *)malloc(info.rd_block_sz);
    if (!buf) {
        printf("alloc buffer failed\n");
        close(fd);
        return -1;
    }

    /* start dma */
    if (dma_start != 0) {
        ret = ioctl(fd, PDMA_IOC_START_DMA);
        if (ret == -1) {
            printf("start dma failed\n");
            free(buf);
            close(fd);
            return -1;
        }
    }
    int ct = 0;
    /* start read */
    sys_tm_get_us(&tm_beg);

    ctrl.type = 0;
    ctrl.addr = 0;
    ret = ioctl(fd, PDMA_IOC_RW_REG, &ctrl);
    printf("read reg length: %d\n", ctrl.val);
    if(ret == -1){
        printf("ioctl rw reg failed\n");
        free(buf);
        return -1;
    }

    while (count--) {
        printf("before read\n");
        ret = read(fd, buf, info.rd_block_sz);//
        printf("this block read from %d\n",ret);
        if(ct < 500)
        {
           printf("%lx\n",((unsigned long *)buf)[0]);
           ct++;
        }
        if (ret != info.rd_block_sz) {
            printf("read failed\n");
            ret = -1;
            break;
        }
        int cnt;
        unsigned char *val32 = (unsigned char *)buf;
	if( access(filename, 0) == 0) remove(filename);
	FILE *ff = fopen(filename, "w");
        for(cnt = 0 ;  cnt < info.rd_block_sz ; cnt++){
	    if((cnt)%8 == 0) fprintf(ff, "n= %d, ", cnt);
            fprintf(ff, "%02x(%d) ", val32[cnt], reverse(val32[cnt]));
	    if((cnt+1)%8 == 0) fprintf(ff, "\n");
	}
	fclose(ff);

        if (check_pt) {
            int i;
            unsigned int start_pt = pattern + inc*(info.rd_block_sz/4)*total_read_num;
            unsigned int *val32 = (unsigned int *)buf;

            for (i = 0; i < info.rd_block_sz/4; i++) {
                if (val32[i] != (start_pt + i*inc)) {
                    printf("check pattern failed, start=0x%x inc=0x%x addr=0x%x\n", 
                           start_pt, inc, i*4);
                    hex_print(val32, info.rd_block_sz);
                    count = 0;
                    ret = -1;
                    break;
                }
            }
        }

        if (delay != 0) {
            sys_tm_wait_us(delay);
        }

        total_read_num++;
    }
    sys_tm_get_us(&tm_end); 
    tm_total = tm_end - tm_beg;
    if (tm_total == 0) tm_total = 1;
    total_read_size = total_read_num * info.rd_block_sz;

    printf("total read %lldKB, elapsed %lldus\n", total_read_size/1024, tm_total);
    printf("read performance is %lldMB/s\n", total_read_size/tm_total);

    free(buf);
    close(fd);
    return ret;
}
