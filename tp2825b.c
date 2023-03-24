/*
 * tp2825B.c
 */
#include <linux/kernel.h>
#include <linux/version.h>
#include <linux/module.h>
#include <linux/types.h>
#include <linux/errno.h>
#include <linux/fcntl.h>
#include <linux/mm.h>
#include <linux/miscdevice.h>
#include <linux/proc_fs.h>

#include <linux/fs.h>
#include <linux/slab.h>
#include <linux/init.h>
#include <asm/uaccess.h>
#include <asm/io.h>
//#include <asm/system.h>
#include <linux/interrupt.h>
#include <linux/ioport.h>
#include <linux/string.h>
#include <linux/list.h>
#include <asm/delay.h>
#include <linux/timer.h>
#include <linux/delay.h>
#include <linux/poll.h>
#include <linux/kthread.h>
#include <linux/sched.h>
#include <uapi/linux/sched/types.h>

#include <linux/i2c.h>
#include <linux/i2c-dev.h>

#include "tp2802_def.h"
#include "tp2825B.h"

static struct i2c_board_info tp2854_info =
{
    I2C_BOARD_INFO("TP2850", 0x45),
};
#define I2C_INDEX_0 3 //
static struct i2c_client* tp28xx_client0;

MODULE_DESCRIPTION("TechPoint TP2825B Linux Module");
MODULE_LICENSE("GPL");



#define DEBUG            1  //printk debug information on/off


#define DEFAULT_FORMAT      TP2802_720P30

static int HDC_enable = 1;
static int mode = DEFAULT_FORMAT;
static int chips = 1;
static int output[] = { //EMB422_16BIT, //for TP2825B
                        MIPI_2LANES,  //for TP2850
                      };

static unsigned int id[MAX_CHIPS];

#define TP2825B_I2C_A 	0x44
#define TP2825B_I2C_B 	0x45


//unsigned char tp2802_i2c_addr[] = { TP2825B_I2C_A,
//                                    TP2825B_I2C_B
//                                  };

unsigned char tp2802_i2c_addr[] = { TP2825B_I2C_B};

#define TP2802_I2C_ADDR(chip_id)    (tp2802_i2c_addr[chip_id])


typedef struct
{
    unsigned int    count[CHANNELS_PER_CHIP];
    unsigned int    mode[CHANNELS_PER_CHIP];
    unsigned int    scan[CHANNELS_PER_CHIP];
    unsigned int    gain[CHANNELS_PER_CHIP][4];
    unsigned int    std[CHANNELS_PER_CHIP];
    unsigned int    state[CHANNELS_PER_CHIP];
    unsigned int    force[CHANNELS_PER_CHIP];
    unsigned char   addr;
} tp2802wd_info;


static tp2802wd_info watchdog_info[MAX_CHIPS];
volatile static unsigned int watchdog_state = 0;
struct task_struct *task_watchdog_deamon = NULL;

//static DEFINE_SPINLOCK(watchdog_lock);
struct semaphore watchdog_lock;
#define WATCHDOG_EXIT    0
#define WATCHDOG_RUNNING 1
#define WDT              0

int  TP2802_watchdog_init(void);
void TP2802_watchdog_exit(void);
static void TP2825B_PTZ_mode(unsigned char, unsigned char, unsigned char);


unsigned int ConvertACPV1Data(unsigned char dat)
{
    unsigned int i, tmp=0;
    for(i = 0; i < 8; i++)
    {
        tmp <<= 3;

        if(0x01 & dat) tmp |= 0x06;
        else tmp |= 0x04;

        dat >>= 1;
    }
    return tmp;
}
void tp28xx_byte_write(unsigned char chip,
                       unsigned char reg_addr,
                       unsigned char value )
{
//    unsigned char chip_addr;
//    chip_addr = TP2802_I2C_ADDR(chip);
//    gpio_i2c_write(chip_addr, addr, data);
    unsigned char buf[2];
    struct i2c_client* client = tp28xx_client0;
    unsigned char chip_addr;

   // if(id[chip] == 0xffff) return;


    chip_addr = tp2802_i2c_addr[chip];

    client->addr = chip_addr;

    buf[0] = reg_addr;
    buf[1] = value;

    i2c_master_send(client, buf, 2);
    udelay(300);
}
unsigned char tp28xx_byte_read(unsigned char chip, unsigned char reg_addr)
{
//    unsigned char chip_addr;
//    chip_addr = TP2802_I2C_ADDR(chip);
//    return gpio_i2c_read(chip_addr, addr);

    unsigned char         ret_data = 0xFF;
    int                   ret;
    struct i2c_client*    client = tp28xx_client0;
    static struct i2c_msg msg[2];
    unsigned char         buffer[2];
    unsigned int          data_width = 1;

     //if(id[chip] == 0xffff) return 0xff;



    client->addr = tp2802_i2c_addr[chip] ;

    buffer[0] = reg_addr & 0xFF;

    msg[0].addr = client->addr;
    msg[0].flags = 0;
    msg[0].len = 1;
    msg[0].buf = buffer;

    msg[1].addr = client->addr;
    msg[1].flags = client->flags | I2C_M_RD;
    msg[1].len = 1;
    msg[1].buf = buffer;

    ret = i2c_transfer(client->adapter, msg, 2);
    if (ret != 2) {
        printk("[%s %d] addr 0x%x i2c_transfer error", __FUNCTION__, __LINE__, client->addr);
        return 0xff;
    }

    memcpy(&ret_data,buffer,data_width);

    return ret_data;

}

static int i2c_client_init(void)
{
    struct i2c_adapter* i2c_adap;

    i2c_adap = i2c_get_adapter(I2C_INDEX_0);
    //tp28xx_client0 = i2c_new_device(i2c_adap, &tp2854_info);
    tp28xx_client0 = i2c_new_client_device(i2c_adap, &tp2854_info);
    i2c_put_adapter(i2c_adap);

    return 0;
}

static void i2c_client_exit(void)
{
    i2c_unregister_device(tp28xx_client0);
}

static void tp2802_write_table(unsigned char chip,
                               unsigned char addr, unsigned char *tbl_ptr, unsigned char tbl_cnt)
{
    unsigned char i = 0;
    for(i = 0; i < tbl_cnt; i ++)
    {
        tp28xx_byte_write(chip, (addr + i), *(tbl_ptr + i));
    }
}

void TP2860_mipi_out(unsigned char chip, unsigned char fmt,
                    unsigned char std, unsigned char lane)
{
    char tmp;
    //mipi setting
    tp28xx_byte_write(chip, 0x40, 0x08); //select MIPI page
    tp28xx_byte_write(chip, 0x02, 0x79);
    tp28xx_byte_write(chip, 0x03, 0x71);
    tp28xx_byte_write(chip, 0x04, 0x71);
    tp28xx_byte_write(chip, 0x13, 0xef);
    tp28xx_byte_write(chip, 0x20, 0x00);
    tp28xx_byte_write(chip, 0x23, 0x9e);

    if (MIPI_1LANE == lane) {
        tp28xx_byte_write(chip, 0x21, 0x11);
        if (FHD30 == fmt || FHD25 == fmt) {
            if (STD_HDA == std) {
                tp28xx_byte_write(chip, 0x14, 0x07);
                tp28xx_byte_write(chip, 0x15, 0x05);
            } else {
                tp28xx_byte_write(chip, 0x14, 0x00);
                    tp28xx_byte_write(chip, 0x15, 0x02);
            }
            tp28xx_byte_write(chip, 0x2a, 0x08);
            tp28xx_byte_write(chip, 0x2b, 0x06);
            tp28xx_byte_write(chip, 0x2c, 0x11);
            tp28xx_byte_write(chip, 0x2e, 0x0a);
        } else if (HD30 == fmt || HD25 == fmt) {
            if(STD_HDA == std) {
                tp28xx_byte_write(chip, 0x14, 0x47);
                    tp28xx_byte_write(chip, 0x15, 0x09);
            } else {
                tp28xx_byte_write(chip, 0x14, 0x00);
                tp28xx_byte_write(chip, 0x15, 0x12);
            }
            tp28xx_byte_write(chip, 0x2a, 0x04);
            tp28xx_byte_write(chip, 0x2b, 0x03);
            tp28xx_byte_write(chip, 0x2c, 0x09);
            tp28xx_byte_write(chip, 0x2e, 0x02);
        } else if (NTSC == fmt || PAL == fmt) {
            tp28xx_byte_write(chip, 0x14, 0x51);
            tp28xx_byte_write(chip, 0x15, 0x07);
            tp28xx_byte_write(chip, 0x2a, 0x02);
            tp28xx_byte_write(chip, 0x2b, 0x01);
            tp28xx_byte_write(chip, 0x2c, 0x05);
            tp28xx_byte_write(chip, 0x2e, 0x02);
        } else if (HD60 == fmt || HD50 == fmt) {
            tp28xx_byte_write(chip, 0x14, 0x00);
            tp28xx_byte_write(chip, 0x15, 0x02);
            tp28xx_byte_write(chip, 0x2a, 0x08);
            tp28xx_byte_write(chip, 0x2b, 0x06);
            tp28xx_byte_write(chip, 0x2c, 0x11);
            tp28xx_byte_write(chip, 0x2e, 0x0a);
        }
    } else {
        /* 2lane */
        tp28xx_byte_write(chip, 0x21, 0x12);

        if (FHD30 == fmt || FHD25 == fmt) {
            if (STD_HDA == std) {
                tp28xx_byte_write(chip, 0x14, 0x40);
                tp28xx_byte_write(chip, 0x15, 0x05);
            } else {
                tp28xx_byte_write(chip, 0x14, 0x41);
                tp28xx_byte_write(chip, 0x15, 0x02);
            }
            tp28xx_byte_write(chip, 0x2a, 0x04);
            tp28xx_byte_write(chip, 0x2b, 0x03);
            tp28xx_byte_write(chip, 0x2c, 0x09);
            tp28xx_byte_write(chip, 0x2e, 0x02);
        } else if (HD30 == fmt || HD25 == fmt) {
            if(STD_HDA == std) {
                tp28xx_byte_write(chip, 0x14, 0x50);
                tp28xx_byte_write(chip, 0x15, 0x09);
            } else {
                tp28xx_byte_write(chip, 0x14, 0x41);
                tp28xx_byte_write(chip, 0x15, 0x12);
            }
            tp28xx_byte_write(chip, 0x2a, 0x02);
            tp28xx_byte_write(chip, 0x2b, 0x01);
            tp28xx_byte_write(chip, 0x2c, 0x05);
            tp28xx_byte_write(chip, 0x2e, 0x02);
        } else if (NTSC == fmt || PAL == fmt) {
            tp28xx_byte_write(chip, 0x14, 0x62);
            tp28xx_byte_write(chip, 0x15, 0x07);
            tp28xx_byte_write(chip, 0x2a, 0x02);
            tp28xx_byte_write(chip, 0x2b, 0x00);
            tp28xx_byte_write(chip, 0x2c, 0x03);
            tp28xx_byte_write(chip, 0x2e, 0x02);
        } else if (HD60 == fmt || HD50 == fmt) {
            tp28xx_byte_write(chip, 0x14, 0x41);
            tp28xx_byte_write(chip, 0x15, 0x02);
            tp28xx_byte_write(chip, 0x2a, 0x04);
            tp28xx_byte_write(chip, 0x2b, 0x03);
            tp28xx_byte_write(chip, 0x2c, 0x09);
            tp28xx_byte_write(chip, 0x2e, 0x02);
        } else if (QHD30 == fmt || QHD25 == fmt ||
                    FHD60 == fmt || FHD50 == fmt) {

            tp28xx_byte_write(chip, 0x14, 0x00);
            tp28xx_byte_write(chip, 0x15, 0x01);
            tp28xx_byte_write(chip, 0x2a, 0x08);
            tp28xx_byte_write(chip, 0x2b, 0x06);
            tp28xx_byte_write(chip, 0x2c, 0x11);
            tp28xx_byte_write(chip, 0x2e, 0x0a);
        }
    }

    tmp = tp28xx_byte_read(chip, 0x14); //PLL reset
    tp28xx_byte_write(chip, 0x14, 0x80 | tmp);
    tp28xx_byte_write(chip, 0x14, tmp);

    /* Enable MIPI CSI2 output */
    tp28xx_byte_write(chip, 0x28, 0x02);  //stream off
    tp28xx_byte_write(chip, 0x28, 0x00);  //stream on
    tp28xx_byte_write(chip, 0x40, 0x00); //back to decoder page
}

/*
 *  ch: video channel
 *  fmt: PAL/NTSC/HD25/HD30
 *  std: STD_TVI/STD_HDA
 *  lane: MIPI_2LANE/MIPI_1LANE
 *  sample: TP2860_sensor_init(VIN1,HD30,STD_TVI,MIPI_2LANE); //video is TVI 720p30 from Vin1
*/
void TP2860_sensor_init(unsigned char chip, unsigned char ch,
                        unsigned char fmt, unsigned char std,
                        unsigned char lane)
{
    unsigned char tmp;

    tp28xx_byte_write(chip, 0x40, 0x00); //select decoder page
    tp28xx_byte_write(chip, 0x06, 0x12); //default value
    tp28xx_byte_write(chip, 0x42, 0x00); //common setting for all format
    tp28xx_byte_write(chip, 0x4e, 0x00); //common setting for MIPI output
    tp28xx_byte_write(chip, 0x54, 0x00); //common setting for MIPI output
    tp28xx_byte_write(chip, 0x41, ch); //video MUX select

    if (PAL == fmt) {
#if CVBS_960H
        tp28xx_byte_write(chip, 0x02, 0x47);
        tp28xx_byte_write(chip, 0x0c, 0x13);
        tp28xx_byte_write(chip, 0x0d, 0x51);

        tp28xx_byte_write(chip, 0x15, 0x13);
        tp28xx_byte_write(chip, 0x16, 0x76);
        tp28xx_byte_write(chip, 0x17, 0x80);
        tp28xx_byte_write(chip, 0x18, 0x17);
        tp28xx_byte_write(chip, 0x19, 0x20);
        tp28xx_byte_write(chip, 0x1a, 0x17);
        tp28xx_byte_write(chip, 0x1c, 0x09);
        tp28xx_byte_write(chip, 0x1d, 0x48);

        tp28xx_byte_write(chip, 0x20, 0x48);
        tp28xx_byte_write(chip, 0x21, 0x84);
        tp28xx_byte_write(chip, 0x22, 0x37);
        tp28xx_byte_write(chip, 0x23, 0x3f);

        tp28xx_byte_write(chip, 0x2b, 0x70);
        tp28xx_byte_write(chip, 0x2c, 0x2a);
        tp28xx_byte_write(chip, 0x2d, 0x64);
        tp28xx_byte_write(chip, 0x2e, 0x56);

        tp28xx_byte_write(chip, 0x30, 0x7a);
        tp28xx_byte_write(chip, 0x31, 0x4a);
        tp28xx_byte_write(chip, 0x32, 0x4d);
        tp28xx_byte_write(chip, 0x33, 0xf0);

        tp28xx_byte_write(chip, 0x35, 0x65);
        tp28xx_byte_write(chip, 0x38, 0x00);
        tp28xx_byte_write(chip, 0x39, 0x04);

#else //PAL 720H
        tp28xx_byte_write(chip, 0x02, 0x47);
        tp28xx_byte_write(chip, 0x06, 0x32);
        tp28xx_byte_write(chip, 0x0c, 0x13);
        tp28xx_byte_write(chip, 0x0d, 0x51);

        tp28xx_byte_write(chip, 0x15, 0x03);
        tp28xx_byte_write(chip, 0x16, 0xf0);
        tp28xx_byte_write(chip, 0x17, 0xa0);
        tp28xx_byte_write(chip, 0x18, 0x17);
        tp28xx_byte_write(chip, 0x19, 0x20);
        tp28xx_byte_write(chip, 0x1a, 0x15);
        tp28xx_byte_write(chip, 0x1c, 0x06);
        tp28xx_byte_write(chip, 0x1d, 0xc0);

        tp28xx_byte_write(chip, 0x20, 0x48);
        tp28xx_byte_write(chip, 0x21, 0x84);
        tp28xx_byte_write(chip, 0x22, 0x37);
        tp28xx_byte_write(chip, 0x23, 0x3f);

        tp28xx_byte_write(chip, 0x2b, 0x70);
        tp28xx_byte_write(chip, 0x2c, 0x2a);
        tp28xx_byte_write(chip, 0x2d, 0x4b);
        tp28xx_byte_write(chip, 0x2e, 0x56);

        tp28xx_byte_write(chip, 0x30, 0x7a);
        tp28xx_byte_write(chip, 0x31, 0x4a);
        tp28xx_byte_write(chip, 0x32, 0x4d);
        tp28xx_byte_write(chip, 0x33, 0xfb);

        tp28xx_byte_write(chip, 0x35, 0x65);
        tp28xx_byte_write(chip, 0x38, 0x00);
        tp28xx_byte_write(chip, 0x39, 0x04);
#endif
	} else if(NTSC == fmt) {
#if CVBS_960H
        tp28xx_byte_write(chip, 0x02, 0x47);
        tp28xx_byte_write(chip, 0x0c, 0x13);
        tp28xx_byte_write(chip, 0x0d, 0x50);

        tp28xx_byte_write(chip, 0x15, 0x13);
        tp28xx_byte_write(chip, 0x16, 0x60);
        tp28xx_byte_write(chip, 0x17, 0x80);
        tp28xx_byte_write(chip, 0x18, 0x12);
        tp28xx_byte_write(chip, 0x19, 0xf0);
        tp28xx_byte_write(chip, 0x1a, 0x07);
        tp28xx_byte_write(chip, 0x1c, 0x09);
        tp28xx_byte_write(chip, 0x1d, 0x38);

        tp28xx_byte_write(chip, 0x20, 0x40);
        tp28xx_byte_write(chip, 0x21, 0x84);
        tp28xx_byte_write(chip, 0x22, 0x36);
        tp28xx_byte_write(chip, 0x23, 0x3c);

        tp28xx_byte_write(chip, 0x2b, 0x70);
        tp28xx_byte_write(chip, 0x2c, 0x2a);
        tp28xx_byte_write(chip, 0x2d, 0x68);
        tp28xx_byte_write(chip, 0x2e, 0x57);

        tp28xx_byte_write(chip, 0x30, 0x62);
        tp28xx_byte_write(chip, 0x31, 0xbb);
        tp28xx_byte_write(chip, 0x32, 0x96);
        tp28xx_byte_write(chip, 0x33, 0xc0);

        tp28xx_byte_write(chip, 0x35, 0x65);
        tp28xx_byte_write(chip, 0x38, 0x00);
        tp28xx_byte_write(chip, 0x39, 0x04);

#else	//NTSC 720H

        tp28xx_byte_write(chip, 0x02, 0x47);
        tp28xx_byte_write(chip, 0x0c, 0x13);
        tp28xx_byte_write(chip, 0x0d, 0x50);

        tp28xx_byte_write(chip, 0x15, 0x03);
        tp28xx_byte_write(chip, 0x16, 0xd6);
        tp28xx_byte_write(chip, 0x17, 0xa0);
        tp28xx_byte_write(chip, 0x18, 0x12);
        tp28xx_byte_write(chip, 0x19, 0xf0);
        tp28xx_byte_write(chip, 0x1a, 0x05);
        tp28xx_byte_write(chip, 0x1c, 0x06);
        tp28xx_byte_write(chip, 0x1d, 0xb4);

        tp28xx_byte_write(chip, 0x20, 0x40);
        tp28xx_byte_write(chip, 0x21, 0x84);
        tp28xx_byte_write(chip, 0x22, 0x36);
        tp28xx_byte_write(chip, 0x23, 0x3c);

        tp28xx_byte_write(chip, 0x2b, 0x70);
        tp28xx_byte_write(chip, 0x2c, 0x2a);
        tp28xx_byte_write(chip, 0x2d, 0x4b);
        tp28xx_byte_write(chip, 0x2e, 0x57);

        tp28xx_byte_write(chip, 0x30, 0x62);
        tp28xx_byte_write(chip, 0x31, 0xbb);
        tp28xx_byte_write(chip, 0x32, 0x96);
        tp28xx_byte_write(chip, 0x33, 0xcb);

        tp28xx_byte_write(chip, 0x35, 0x65);
        tp28xx_byte_write(chip, 0x38, 0x00);
        tp28xx_byte_write(chip, 0x39, 0x04);
#endif
    } else if(HD25 == fmt) {
        tp28xx_byte_write(chip, 0x02, 0x42);
        tp28xx_byte_write(chip, 0x07, 0xc0);
        tp28xx_byte_write(chip, 0x0b, 0xc0);
        tp28xx_byte_write(chip, 0x0c, 0x13);
        tp28xx_byte_write(chip, 0x0d, 0x50);
        tp28xx_byte_write(chip, 0x15, 0x13);
        tp28xx_byte_write(chip, 0x16, 0x15);
        tp28xx_byte_write(chip, 0x17, 0x00);
        tp28xx_byte_write(chip, 0x18, 0x19);
        tp28xx_byte_write(chip, 0x19, 0xd0);
        tp28xx_byte_write(chip, 0x1a, 0x25);
        tp28xx_byte_write(chip, 0x1c, 0x07);  //1280*720, 25fps
        tp28xx_byte_write(chip, 0x1d, 0xbc);  //1280*720, 25fps
        tp28xx_byte_write(chip, 0x20, 0x30);
        tp28xx_byte_write(chip, 0x21, 0x84);
        tp28xx_byte_write(chip, 0x22, 0x36);
        tp28xx_byte_write(chip, 0x23, 0x3c);
        tp28xx_byte_write(chip, 0x2b, 0x60);
        tp28xx_byte_write(chip, 0x2c, 0x0a);
        tp28xx_byte_write(chip, 0x2d, 0x30);
        tp28xx_byte_write(chip, 0x2e, 0x70);
        tp28xx_byte_write(chip, 0x30, 0x48);
        tp28xx_byte_write(chip, 0x31, 0xbb);
        tp28xx_byte_write(chip, 0x32, 0x2e);
        tp28xx_byte_write(chip, 0x33, 0x90);
        tp28xx_byte_write(chip, 0x35, 0x25);
        tp28xx_byte_write(chip, 0x38, 0x00);
        tp28xx_byte_write(chip, 0x39, 0x18);

        //AHD720p25 extra
        if (STD_HDA == std) {
            tp28xx_byte_write(chip, 0x0d, 0x70);
            tp28xx_byte_write(chip, 0x16, 0x16);
            tp28xx_byte_write(chip, 0x1c, 0x87);
            tp28xx_byte_write(chip, 0x1d, 0xba);
            tp28xx_byte_write(chip, 0x20, 0x38);
            tp28xx_byte_write(chip, 0x21, 0x46);
            tp28xx_byte_write(chip, 0x27, 0xad);
            tp28xx_byte_write(chip, 0x2c, 0x3a);
            tp28xx_byte_write(chip, 0x2d, 0x48);
            tp28xx_byte_write(chip, 0x2e, 0x40);
            tp28xx_byte_write(chip, 0x30, 0x4f);
            tp28xx_byte_write(chip, 0x31, 0x10);
            tp28xx_byte_write(chip, 0x32, 0x08);
            tp28xx_byte_write(chip, 0x33, 0x40);
        }
    } else if (HD30 == fmt) {
        tp28xx_byte_write(chip, 0x02, 0x42);
        tp28xx_byte_write(chip, 0x07, 0xc0);
        tp28xx_byte_write(chip, 0x0b, 0xc0);
        tp28xx_byte_write(chip, 0x0c, 0x13);
        tp28xx_byte_write(chip, 0x0d, 0x50);
        tp28xx_byte_write(chip, 0x15, 0x13);
        tp28xx_byte_write(chip, 0x16, 0x15);
        tp28xx_byte_write(chip, 0x17, 0x00);
        tp28xx_byte_write(chip, 0x18, 0x19);
        tp28xx_byte_write(chip, 0x19, 0xd0);
        tp28xx_byte_write(chip, 0x1a, 0x25);
        tp28xx_byte_write(chip, 0x1c, 0x06);  //1280*720, 30fps
        tp28xx_byte_write(chip, 0x1d, 0x72);  //1280*720, 30fps
        tp28xx_byte_write(chip, 0x20, 0x30);
        tp28xx_byte_write(chip, 0x21, 0x84);
        tp28xx_byte_write(chip, 0x22, 0x36);
        tp28xx_byte_write(chip, 0x23, 0x3c);
        tp28xx_byte_write(chip, 0x2b, 0x60);
        tp28xx_byte_write(chip, 0x2c, 0x0a);
        tp28xx_byte_write(chip, 0x2d, 0x30);
        tp28xx_byte_write(chip, 0x2e, 0x70);
        tp28xx_byte_write(chip, 0x30, 0x48);
        tp28xx_byte_write(chip, 0x31, 0xbb);
        tp28xx_byte_write(chip, 0x32, 0x2e);
        tp28xx_byte_write(chip, 0x33, 0x90);
        tp28xx_byte_write(chip, 0x35, 0x25);
        tp28xx_byte_write(chip, 0x38, 0x00);
        tp28xx_byte_write(chip, 0x39, 0x18);

        //AHD720p30 extra
        if (STD_HDA == std) {
            tp28xx_byte_write(chip, 0x0d, 0x70);
            tp28xx_byte_write(chip, 0x16, 0x16);
            tp28xx_byte_write(chip, 0x1c, 0x86);
            tp28xx_byte_write(chip, 0x1d, 0x70);
            tp28xx_byte_write(chip, 0x20, 0x38);
            tp28xx_byte_write(chip, 0x21, 0x46);
            tp28xx_byte_write(chip, 0x27, 0xad);
            tp28xx_byte_write(chip, 0x2c, 0x3a);
            tp28xx_byte_write(chip, 0x2d, 0x48);
            tp28xx_byte_write(chip, 0x2e, 0x40);
            tp28xx_byte_write(chip, 0x30, 0x4e);
            tp28xx_byte_write(chip, 0x31, 0xe5);
            tp28xx_byte_write(chip, 0x32, 0x00);
            tp28xx_byte_write(chip, 0x33, 0xf0);
        }
    } else if (FHD30 == fmt) {
            tp28xx_byte_write(chip, 0x02, 0x40);
            tp28xx_byte_write(chip, 0x07, 0xc0);
            tp28xx_byte_write(chip, 0x0b, 0xc0);
            tp28xx_byte_write(chip, 0x0c, 0x03);
            tp28xx_byte_write(chip, 0x0d, 0x50);
            tp28xx_byte_write(chip, 0x15, 0x03);
            tp28xx_byte_write(chip, 0x16, 0xd2);
            tp28xx_byte_write(chip, 0x17, 0x80);
            tp28xx_byte_write(chip, 0x18, 0x29);
            tp28xx_byte_write(chip, 0x19, 0x38);
            tp28xx_byte_write(chip, 0x1a, 0x47);
            tp28xx_byte_write(chip, 0x1c, 0x08);  //1920*1080, 30fps
            tp28xx_byte_write(chip, 0x1d, 0x98);
            tp28xx_byte_write(chip, 0x20, 0x30);
            tp28xx_byte_write(chip, 0x21, 0x84);
            tp28xx_byte_write(chip, 0x22, 0x36);
            tp28xx_byte_write(chip, 0x23, 0x3c);
            tp28xx_byte_write(chip, 0x2b, 0x60);
            tp28xx_byte_write(chip, 0x2c, 0x0a);
            tp28xx_byte_write(chip, 0x2d, 0x30);
            tp28xx_byte_write(chip, 0x2e, 0x70);
            tp28xx_byte_write(chip, 0x30, 0x48);
            tp28xx_byte_write(chip, 0x31, 0xbb);
            tp28xx_byte_write(chip, 0x32, 0x2e);
            tp28xx_byte_write(chip, 0x33, 0x90);
            tp28xx_byte_write(chip, 0x35, 0x05);
            tp28xx_byte_write(chip, 0x38, 0x00);
            tp28xx_byte_write(chip, 0x39, 0x1C);

            //AHD1080p30 extra
            if (STD_HDA == std) {
                tp28xx_byte_write(chip, 0x0d, 0x70);
                tp28xx_byte_write(chip, 0x15, 0x01);
                tp28xx_byte_write(chip, 0x16, 0xf0);
                tp28xx_byte_write(chip, 0x1c, 0x88);
                tp28xx_byte_write(chip, 0x1d, 0x96);
                tp28xx_byte_write(chip, 0x20, 0x38);
                tp28xx_byte_write(chip, 0x21, 0x46);
                tp28xx_byte_write(chip, 0x27, 0xad);
                tp28xx_byte_write(chip, 0x2c, 0x3a);
                tp28xx_byte_write(chip, 0x2d, 0x48);
                tp28xx_byte_write(chip, 0x2e, 0x40);
                tp28xx_byte_write(chip, 0x30, 0x52);
                tp28xx_byte_write(chip, 0x31, 0xca);
                tp28xx_byte_write(chip, 0x32, 0xf0);
                tp28xx_byte_write(chip, 0x33, 0x20);
                tp28xx_byte_write(chip, 0x35, 0x25);
            }
        } else if (FHD25 == fmt) {
            tp28xx_byte_write(chip, 0x02, 0x40);
            tp28xx_byte_write(chip, 0x07, 0xc0);
            tp28xx_byte_write(chip, 0x0b, 0xc0);
            tp28xx_byte_write(chip, 0x0c, 0x03);
            tp28xx_byte_write(chip, 0x0d, 0x50);
            tp28xx_byte_write(chip, 0x15, 0x03);
            tp28xx_byte_write(chip, 0x16, 0xd2);
            tp28xx_byte_write(chip, 0x17, 0x80);
            tp28xx_byte_write(chip, 0x18, 0x29);
            tp28xx_byte_write(chip, 0x19, 0x38);
            tp28xx_byte_write(chip, 0x1a, 0x47);
            tp28xx_byte_write(chip, 0x1c, 0x0a);  //1920*1080, 25fps
            tp28xx_byte_write(chip, 0x1d, 0x50);
            tp28xx_byte_write(chip, 0x20, 0x30);
            tp28xx_byte_write(chip, 0x21, 0x84);
            tp28xx_byte_write(chip, 0x22, 0x36);
            tp28xx_byte_write(chip, 0x23, 0x3c);
            tp28xx_byte_write(chip, 0x2b, 0x60);
            tp28xx_byte_write(chip, 0x2c, 0x0a);
            tp28xx_byte_write(chip, 0x2d, 0x30);
            tp28xx_byte_write(chip, 0x2e, 0x70);
            tp28xx_byte_write(chip, 0x30, 0x48);
            tp28xx_byte_write(chip, 0x31, 0xbb);
            tp28xx_byte_write(chip, 0x32, 0x2e);
            tp28xx_byte_write(chip, 0x33, 0x90);
            tp28xx_byte_write(chip, 0x35, 0x05);
            tp28xx_byte_write(chip, 0x38, 0x00);
            tp28xx_byte_write(chip, 0x39, 0x1C);

            //AHD1080p25 extra
            if (STD_HDA == std) {
                tp28xx_byte_write(chip, 0x0d, 0x70);
                tp28xx_byte_write(chip, 0x15, 0x01);
                tp28xx_byte_write(chip, 0x16, 0xf0);
                tp28xx_byte_write(chip, 0x1c, 0x8a);
                tp28xx_byte_write(chip, 0x1d, 0x4e);
                tp28xx_byte_write(chip, 0x20, 0x3c);
                tp28xx_byte_write(chip, 0x21, 0x46);
                tp28xx_byte_write(chip, 0x27, 0xad);
                tp28xx_byte_write(chip, 0x2c, 0x3a);
                tp28xx_byte_write(chip, 0x2d, 0x48);
                tp28xx_byte_write(chip, 0x2e, 0x40);
                tp28xx_byte_write(chip, 0x30, 0x52);
                tp28xx_byte_write(chip, 0x31, 0xc3);
                tp28xx_byte_write(chip, 0x32, 0x7d);
                tp28xx_byte_write(chip, 0x33, 0xa0);
                tp28xx_byte_write(chip, 0x35, 0x25);
            }
    } else if (HD50 == fmt) {
        tp28xx_byte_write(chip, 0x02, 0x42);
        tp28xx_byte_write(chip, 0x07, 0xc0);
        tp28xx_byte_write(chip, 0x0b, 0xc0);
        tp28xx_byte_write(chip, 0x0c, 0x03);
        tp28xx_byte_write(chip, 0x0d, 0x50);
        tp28xx_byte_write(chip, 0x15, 0x13);
        tp28xx_byte_write(chip, 0x16, 0x15);
        tp28xx_byte_write(chip, 0x17, 0x00);
        tp28xx_byte_write(chip, 0x18, 0x19);
        tp28xx_byte_write(chip, 0x19, 0xd0);
        tp28xx_byte_write(chip, 0x1a, 0x25);
        tp28xx_byte_write(chip, 0x1c, 0x07);  //1280*720, 25fps
        tp28xx_byte_write(chip, 0x1d, 0xbc);  //1280*720, 25fps
        tp28xx_byte_write(chip, 0x20, 0x30);
        tp28xx_byte_write(chip, 0x21, 0x84);
        tp28xx_byte_write(chip, 0x22, 0x36);
        tp28xx_byte_write(chip, 0x23, 0x3c);
        tp28xx_byte_write(chip, 0x2b, 0x60);
        tp28xx_byte_write(chip, 0x2c, 0x1a);
        tp28xx_byte_write(chip, 0x2d, 0x30);
        tp28xx_byte_write(chip, 0x2e, 0x70);
        tp28xx_byte_write(chip, 0x30, 0x48);
        tp28xx_byte_write(chip, 0x31, 0xbb);
        tp28xx_byte_write(chip, 0x32, 0x2e);
        tp28xx_byte_write(chip, 0x33, 0x90);
        tp28xx_byte_write(chip, 0x35, 0x05);
        tp28xx_byte_write(chip, 0x38, 0x00);
        tp28xx_byte_write(chip, 0x39, 0x1C);

        //subcarrier 22M
        if (STD_HDA == std) {
            tp28xx_byte_write(chip, 0x18, 0x1b);
            tp28xx_byte_write(chip, 0x20, 0x40);
            tp28xx_byte_write(chip, 0x21, 0x46);
            tp28xx_byte_write(chip, 0x25, 0xfe);
            tp28xx_byte_write(chip, 0x26, 0x01);
            tp28xx_byte_write(chip, 0x2c, 0x3a);
            tp28xx_byte_write(chip, 0x2d, 0x48);
            tp28xx_byte_write(chip, 0x2e, 0x40);
            tp28xx_byte_write(chip, 0x30, 0x29);
            tp28xx_byte_write(chip, 0x31, 0x67);
            tp28xx_byte_write(chip, 0x32, 0xF3);
            tp28xx_byte_write(chip, 0x33, 0x90);
        }
    } else if (HD60 == fmt) {
        tp28xx_byte_write(chip, 0x02, 0x42);
        tp28xx_byte_write(chip, 0x07, 0xc0);
        tp28xx_byte_write(chip, 0x0b, 0xc0);
        tp28xx_byte_write(chip, 0x0c, 0x03);
        tp28xx_byte_write(chip, 0x0d, 0x50);
        tp28xx_byte_write(chip, 0x15, 0x13);
        tp28xx_byte_write(chip, 0x16, 0x15);
        tp28xx_byte_write(chip, 0x17, 0x00);
        tp28xx_byte_write(chip, 0x18, 0x19);
        tp28xx_byte_write(chip, 0x19, 0xd0);
        tp28xx_byte_write(chip, 0x1a, 0x25);
        tp28xx_byte_write(chip, 0x1c, 0x06);  //1280*720, 60fps
        tp28xx_byte_write(chip, 0x1d, 0x72);  //1280*720, 60fps
        tp28xx_byte_write(chip, 0x20, 0x30);
        tp28xx_byte_write(chip, 0x21, 0x84);
        tp28xx_byte_write(chip, 0x22, 0x36);
        tp28xx_byte_write(chip, 0x23, 0x3c);
        tp28xx_byte_write(chip, 0x2b, 0x60);
        tp28xx_byte_write(chip, 0x2c, 0x1a);
        tp28xx_byte_write(chip, 0x2d, 0x30);
        tp28xx_byte_write(chip, 0x2e, 0x70);
        tp28xx_byte_write(chip, 0x30, 0x48);
        tp28xx_byte_write(chip, 0x31, 0xbb);
        tp28xx_byte_write(chip, 0x32, 0x2e);
        tp28xx_byte_write(chip, 0x33, 0x90);
        tp28xx_byte_write(chip, 0x35, 0x05);
        tp28xx_byte_write(chip, 0x38, 0x00);
        tp28xx_byte_write(chip, 0x39, 0x1C);

        //subcarrier 22M
        if (STD_HDA == std) {
            tp28xx_byte_write(chip, 0x18, 0x1b);
            tp28xx_byte_write(chip, 0x20, 0x40);
            tp28xx_byte_write(chip, 0x21, 0x46);
            tp28xx_byte_write(chip, 0x25, 0xfe);
            tp28xx_byte_write(chip, 0x26, 0x01);
            tp28xx_byte_write(chip, 0x2c, 0x3a);
            tp28xx_byte_write(chip, 0x2d, 0x48);
            tp28xx_byte_write(chip, 0x2e, 0x40);
            tp28xx_byte_write(chip, 0x30, 0x29);
            tp28xx_byte_write(chip, 0x31, 0x62);
            tp28xx_byte_write(chip, 0x32, 0xFC);
            tp28xx_byte_write(chip, 0x33, 0x96);
        }
    } else if (QHD30 == fmt) {
        tp28xx_byte_write(chip, 0x02, 0x50);
        tp28xx_byte_write(chip, 0x07, 0xc0);
        tp28xx_byte_write(chip, 0x0b, 0xc0);
        tp28xx_byte_write(chip, 0x0c, 0x03);
        tp28xx_byte_write(chip, 0x0d, 0x50);
        tp28xx_byte_write(chip, 0x15, 0x23);
        tp28xx_byte_write(chip, 0x16, 0x1b);
        tp28xx_byte_write(chip, 0x17, 0x00);
        tp28xx_byte_write(chip, 0x18, 0x38);
        tp28xx_byte_write(chip, 0x19, 0xa0);
        tp28xx_byte_write(chip, 0x1a, 0x5a);
        tp28xx_byte_write(chip, 0x1c, 0x0c);  //2560*1440, 30fps
        tp28xx_byte_write(chip, 0x1d, 0xe2);
        tp28xx_byte_write(chip, 0x20, 0x50);
        tp28xx_byte_write(chip, 0x21, 0x84);
        tp28xx_byte_write(chip, 0x22, 0x36);
        tp28xx_byte_write(chip, 0x23, 0x3c);
        tp28xx_byte_write(chip, 0x27, 0xad);
        tp28xx_byte_write(chip, 0x2b, 0x60);
        tp28xx_byte_write(chip, 0x2c, 0x2a);
        tp28xx_byte_write(chip, 0x2d, 0x58);
        tp28xx_byte_write(chip, 0x2e, 0x70);
        tp28xx_byte_write(chip, 0x30, 0x74);
        tp28xx_byte_write(chip, 0x31, 0x58);
        tp28xx_byte_write(chip, 0x32, 0x9f);
        tp28xx_byte_write(chip, 0x33, 0x60);
        tp28xx_byte_write(chip, 0x35, 0x15);
        tp28xx_byte_write(chip, 0x36, 0xdc);
        tp28xx_byte_write(chip, 0x38, 0x40);
        tp28xx_byte_write(chip, 0x39, 0x48);

        if (STD_HDA == std) {
            tmp = tp28xx_byte_read(chip, 0x14);
            tmp |= 0x40;
            tp28xx_byte_write(chip, 0x14, tmp);
            tp28xx_byte_write(chip, 0x13, 0x00);
            tp28xx_byte_write(chip, 0x15, 0x23);
            tp28xx_byte_write(chip, 0x16, 0x16);
            tp28xx_byte_write(chip, 0x18, 0x32);
            tp28xx_byte_write(chip, 0x20, 0x80);
            tp28xx_byte_write(chip, 0x21, 0x86);
            tp28xx_byte_write(chip, 0x22, 0x36);
            tp28xx_byte_write(chip, 0x2b, 0x60);
            tp28xx_byte_write(chip, 0x2d, 0xa0);
            tp28xx_byte_write(chip, 0x2e, 0x40);
            tp28xx_byte_write(chip, 0x30, 0x48);
            tp28xx_byte_write(chip, 0x31, 0x6a);
            tp28xx_byte_write(chip, 0x32, 0xbe);
            tp28xx_byte_write(chip, 0x33, 0x80);
            tp28xx_byte_write(chip, 0x39, 0x40);
        }
    } else if (QHD25 == fmt) {
        tp28xx_byte_write(chip, 0x02, 0x50);
        tp28xx_byte_write(chip, 0x07, 0xc0);
        tp28xx_byte_write(chip, 0x0b, 0xc0);
        tp28xx_byte_write(chip, 0x0c, 0x03);
        tp28xx_byte_write(chip, 0x0d, 0x50);
        tp28xx_byte_write(chip, 0x15, 0x23);
        tp28xx_byte_write(chip, 0x16, 0x1b);
        tp28xx_byte_write(chip, 0x17, 0x00);
        tp28xx_byte_write(chip, 0x18, 0x38);
        tp28xx_byte_write(chip, 0x19, 0xa0);
        tp28xx_byte_write(chip, 0x1a, 0x5a);
        tp28xx_byte_write(chip, 0x1c, 0x0f);  //2560*1440, 25fps
        tp28xx_byte_write(chip, 0x1d, 0x76);
        tp28xx_byte_write(chip, 0x20, 0x50);
        tp28xx_byte_write(chip, 0x21, 0x84);
        tp28xx_byte_write(chip, 0x22, 0x36);
        tp28xx_byte_write(chip, 0x23, 0x3c);
        tp28xx_byte_write(chip, 0x27, 0xad);
        tp28xx_byte_write(chip, 0x2b, 0x60);
        tp28xx_byte_write(chip, 0x2c, 0x2a);
        tp28xx_byte_write(chip, 0x2d, 0x58);
        tp28xx_byte_write(chip, 0x2e, 0x70);
        tp28xx_byte_write(chip, 0x30, 0x74);
        tp28xx_byte_write(chip, 0x31, 0x58);
        tp28xx_byte_write(chip, 0x32, 0x9f);
        tp28xx_byte_write(chip, 0x33, 0x60);
        tp28xx_byte_write(chip, 0x35, 0x15);
        tp28xx_byte_write(chip, 0x36, 0xdc);
        tp28xx_byte_write(chip, 0x38, 0x40);
        tp28xx_byte_write(chip, 0x39, 0x48);

        if (STD_HDA == std) {
            tmp = tp28xx_byte_read(chip, 0x14);
            tmp |= 0x40;
            tp28xx_byte_write(chip, 0x14, tmp);
            tp28xx_byte_write(chip, 0x13, 0x00);
            tp28xx_byte_write(chip, 0x15, 0x23);
            tp28xx_byte_write(chip, 0x16, 0x16);
            tp28xx_byte_write(chip, 0x18, 0x32);
            tp28xx_byte_write(chip, 0x20, 0x80);
            tp28xx_byte_write(chip, 0x21, 0x86);
            tp28xx_byte_write(chip, 0x22, 0x36);
            tp28xx_byte_write(chip, 0x2b, 0x60);
            tp28xx_byte_write(chip, 0x2d, 0xa0);
            tp28xx_byte_write(chip, 0x2e, 0x40);
            tp28xx_byte_write(chip, 0x30, 0x48);
            tp28xx_byte_write(chip, 0x31, 0x6f);
            tp28xx_byte_write(chip, 0x32, 0xb5);
            tp28xx_byte_write(chip, 0x33, 0x80);
            tp28xx_byte_write(chip, 0x39, 0x40);
        }
    } else if (FHD60 == fmt) {
        tp28xx_byte_write(chip, 0x02, 0x50);
        tp28xx_byte_write(chip, 0x07, 0xc0);
        tp28xx_byte_write(chip, 0x0b, 0xc0);
        tp28xx_byte_write(chip, 0x0c, 0x03);
        tp28xx_byte_write(chip, 0x0d, 0x50);

        tp28xx_byte_write(chip, 0x15, 0x03);
        tp28xx_byte_write(chip, 0x16, 0xf0);
        tp28xx_byte_write(chip, 0x17, 0x80);
        tp28xx_byte_write(chip, 0x18, 0x12);
        tp28xx_byte_write(chip, 0x19, 0x38);
        tp28xx_byte_write(chip, 0x1a, 0x47);
        tp28xx_byte_write(chip, 0x1c, 0x08);
        tp28xx_byte_write(chip, 0x1d, 0x96);

        tp28xx_byte_write(chip, 0x20, 0x38);
        tp28xx_byte_write(chip, 0x21, 0x84);
        tp28xx_byte_write(chip, 0x22, 0x36);
        tp28xx_byte_write(chip, 0x23, 0x3c);

        tp28xx_byte_write(chip, 0x27, 0xad);

        tp28xx_byte_write(chip, 0x2b, 0x60);
        tp28xx_byte_write(chip, 0x2c, 0x0a);
        tp28xx_byte_write(chip, 0x2d, 0x40);
        tp28xx_byte_write(chip, 0x2e, 0x70);

        tp28xx_byte_write(chip, 0x30, 0x74);
        tp28xx_byte_write(chip, 0x31, 0x9b);
        tp28xx_byte_write(chip, 0x32, 0xa5);
        tp28xx_byte_write(chip, 0x33, 0xe0);

        tp28xx_byte_write(chip, 0x35, 0x05);
        tp28xx_byte_write(chip, 0x38, 0x40);
        tp28xx_byte_write(chip, 0x39, 0x68);
    } else if (FHD50 == fmt) {
        tp28xx_byte_write(chip, 0x02, 0x50);
        tp28xx_byte_write(chip, 0x07, 0xc0);
        tp28xx_byte_write(chip, 0x0b, 0xc0);
        tp28xx_byte_write(chip, 0x0c, 0x03);
        tp28xx_byte_write(chip, 0x0d, 0x50);

        tp28xx_byte_write(chip, 0x15, 0x03);
        tp28xx_byte_write(chip, 0x16, 0xe2);
        tp28xx_byte_write(chip, 0x17, 0x80);
        tp28xx_byte_write(chip, 0x18, 0x27);
        tp28xx_byte_write(chip, 0x19, 0x38);
        tp28xx_byte_write(chip, 0x1a, 0x47);

        tp28xx_byte_write(chip, 0x1c, 0x0a);
        tp28xx_byte_write(chip, 0x1d, 0x4e);

        tp28xx_byte_write(chip, 0x20, 0x38);
        tp28xx_byte_write(chip, 0x21, 0x84);
        tp28xx_byte_write(chip, 0x22, 0x36);
        tp28xx_byte_write(chip, 0x23, 0x3c);

        tp28xx_byte_write(chip, 0x27, 0xad);

        tp28xx_byte_write(chip, 0x2b, 0x60);
        tp28xx_byte_write(chip, 0x2c, 0x0a);
        tp28xx_byte_write(chip, 0x2d, 0x40);
        tp28xx_byte_write(chip, 0x2e, 0x70);

        tp28xx_byte_write(chip, 0x30, 0x74);
        tp28xx_byte_write(chip, 0x31, 0x9b);
        tp28xx_byte_write(chip, 0x32, 0xa5);
        tp28xx_byte_write(chip, 0x33, 0xe0);

        tp28xx_byte_write(chip, 0x35, 0x05);
        tp28xx_byte_write(chip, 0x38, 0x40);
        tp28xx_byte_write(chip, 0x39, 0x68);
    }

    TP2860_mipi_out(chip, fmt, std, lane);
}

unsigned char ReverseByte(unsigned char dat)
{

    static const unsigned char BitReverseTable256[] =
    {
    0x00, 0x80, 0x40, 0xC0, 0x20, 0xA0, 0x60, 0xE0, 0x10, 0x90, 0x50, 0xD0, 0x30, 0xB0, 0x70, 0xF0,
    0x08, 0x88, 0x48, 0xC8, 0x28, 0xA8, 0x68, 0xE8, 0x18, 0x98, 0x58, 0xD8, 0x38, 0xB8, 0x78, 0xF8,
    0x04, 0x84, 0x44, 0xC4, 0x24, 0xA4, 0x64, 0xE4, 0x14, 0x94, 0x54, 0xD4, 0x34, 0xB4, 0x74, 0xF4,
    0x0C, 0x8C, 0x4C, 0xCC, 0x2C, 0xAC, 0x6C, 0xEC, 0x1C, 0x9C, 0x5C, 0xDC, 0x3C, 0xBC, 0x7C, 0xFC,
    0x02, 0x82, 0x42, 0xC2, 0x22, 0xA2, 0x62, 0xE2, 0x12, 0x92, 0x52, 0xD2, 0x32, 0xB2, 0x72, 0xF2,
    0x0A, 0x8A, 0x4A, 0xCA, 0x2A, 0xAA, 0x6A, 0xEA, 0x1A, 0x9A, 0x5A, 0xDA, 0x3A, 0xBA, 0x7A, 0xFA,
    0x06, 0x86, 0x46, 0xC6, 0x26, 0xA6, 0x66, 0xE6, 0x16, 0x96, 0x56, 0xD6, 0x36, 0xB6, 0x76, 0xF6,
    0x0E, 0x8E, 0x4E, 0xCE, 0x2E, 0xAE, 0x6E, 0xEE, 0x1E, 0x9E, 0x5E, 0xDE, 0x3E, 0xBE, 0x7E, 0xFE,
    0x01, 0x81, 0x41, 0xC1, 0x21, 0xA1, 0x61, 0xE1, 0x11, 0x91, 0x51, 0xD1, 0x31, 0xB1, 0x71, 0xF1,
    0x09, 0x89, 0x49, 0xC9, 0x29, 0xA9, 0x69, 0xE9, 0x19, 0x99, 0x59, 0xD9, 0x39, 0xB9, 0x79, 0xF9,
    0x05, 0x85, 0x45, 0xC5, 0x25, 0xA5, 0x65, 0xE5, 0x15, 0x95, 0x55, 0xD5, 0x35, 0xB5, 0x75, 0xF5,
    0x0D, 0x8D, 0x4D, 0xCD, 0x2D, 0xAD, 0x6D, 0xED, 0x1D, 0x9D, 0x5D, 0xDD, 0x3D, 0xBD, 0x7D, 0xFD,
    0x03, 0x83, 0x43, 0xC3, 0x23, 0xA3, 0x63, 0xE3, 0x13, 0x93, 0x53, 0xD3, 0x33, 0xB3, 0x73, 0xF3,
    0x0B, 0x8B, 0x4B, 0xCB, 0x2B, 0xAB, 0x6B, 0xEB, 0x1B, 0x9B, 0x5B, 0xDB, 0x3B, 0xBB, 0x7B, 0xFB,
    0x07, 0x87, 0x47, 0xC7, 0x27, 0xA7, 0x67, 0xE7, 0x17, 0x97, 0x57, 0xD7, 0x37, 0xB7, 0x77, 0xF7,
    0x0F, 0x8F, 0x4F, 0xCF, 0x2F, 0xAF, 0x6F, 0xEF, 0x1F, 0x9F, 0x5F, 0xDF, 0x3F, 0xBF, 0x7F, 0xFF
    };
    return BitReverseTable256[dat];
}
void HDC_QHD_SetData(unsigned char chip, unsigned char reg, unsigned int dat)
{

 	unsigned int i;
	unsigned char ret=0;
	unsigned char crc=0;
    if( dat > 0xff)
    {
        tp28xx_byte_write(chip, reg + 0 , 0x00);
        tp28xx_byte_write(chip, reg + 1 , 0x07);
        tp28xx_byte_write(chip, reg + 2 , 0xff);
        tp28xx_byte_write(chip, reg + 3 , 0xff);
        tp28xx_byte_write(chip, reg + 4 , 0xfc);
    }
    else
    {
        for(i = 0; i < 8; i++ )
        {
            ret >>= 1;
            if(0x80 & dat) {ret |= 0x80; crc +=0x80;}
            dat <<= 1;
        }

        tp28xx_byte_write(chip, reg + 0 , 0x00);
        tp28xx_byte_write(chip, reg + 1 , 0x06);
        tp28xx_byte_write(chip, reg + 2 , ret);
        tp28xx_byte_write(chip, reg + 3 , 0x7f|crc);
        tp28xx_byte_write(chip, reg + 4 , 0xfc);
    }

}
void HDC_SetData(unsigned char chip, unsigned char reg, unsigned int dat)
{

 	unsigned int i;
	unsigned char ret=0;
	unsigned char crc=0;
    if( dat > 0xff)
    {
        tp28xx_byte_write(chip, reg + 0 , 0x07);
        tp28xx_byte_write(chip, reg + 1 , 0xff);
        tp28xx_byte_write(chip, reg + 2 , 0xff);
        tp28xx_byte_write(chip, reg + 3 , 0xff);
        tp28xx_byte_write(chip, reg + 4 , 0xfc);
    }
    else
    {
        for(i = 0; i < 8; i++ )
        {
            ret >>= 1;
            if(0x80 & dat) {ret |= 0x80; crc +=0x80;}
            dat <<= 1;
        }

        tp28xx_byte_write(chip, reg + 0 , 0x06);
        tp28xx_byte_write(chip, reg + 1 , ret);
        tp28xx_byte_write(chip, reg + 2 , 0x7f|crc);
        tp28xx_byte_write(chip, reg + 3 , 0xff);
        tp28xx_byte_write(chip, reg + 4 , 0xfc);
    }

}
void HDA_SetACPV2Data(unsigned char chip, unsigned char reg,unsigned char dat)
{
    unsigned int i;
	unsigned int PTZ_pelco=0;

    for(i = 0; i < 8; i++)
    {
        PTZ_pelco <<= 3;

        if(0x80 & dat) PTZ_pelco |= 0x06;
        else PTZ_pelco |= 0x04;

        dat <<= 1;
    }

    tp28xx_byte_write(chip, reg + 0 , (PTZ_pelco>>16)&0xff);
    tp28xx_byte_write(chip, reg + 1 , (PTZ_pelco>>8)&0xff);
    tp28xx_byte_write(chip, reg + 2 , (PTZ_pelco)&0xff);
}
void HDA_SetACPV1Data(unsigned char chip, unsigned char reg,unsigned char dat)
{

    unsigned int i;
	unsigned int PTZ_pelco=0;

    for(i = 0; i < 8; i++)
    {
        PTZ_pelco <<= 3;

        if(0x01 & dat) PTZ_pelco |= 0x06;
        else PTZ_pelco |= 0x04;

        dat >>= 1;
    }

    tp28xx_byte_write(chip, reg + 0 , (PTZ_pelco>>16)&0xff);
    tp28xx_byte_write(chip, reg + 1 , (PTZ_pelco>>8)&0xff);
    tp28xx_byte_write(chip, reg + 2 , (PTZ_pelco)&0xff);

}

int tp2802_open(struct inode * inode, struct file * file)
{
    return SUCCESS;
}

int tp2802_close(struct inode * inode, struct file * file)
{
    return SUCCESS;
}

static void tp2802_set_work_mode_1080p25(unsigned chip)
{
    // Start address 0x15, Size = 9B
    tp2802_write_table(chip, 0x15, tbl_tp2802_1080p25_raster, 9);
}

static void tp2802_set_work_mode_1080p30(unsigned chip)
{
    // Start address 0x15, Size = 9B
    tp2802_write_table(chip, 0x15, tbl_tp2802_1080p30_raster, 9);
}

static void tp2802_set_work_mode_720p25(unsigned chip)
{
    // Start address 0x15, Size = 9B
    tp2802_write_table(chip, 0x15, tbl_tp2802_720p25_raster, 9);
}

static void tp2802_set_work_mode_720p30(unsigned chip)
{
    // Start address 0x15, Size = 9B
    tp2802_write_table(chip, 0x15, tbl_tp2802_720p30_raster, 9);
}

static void tp2802_set_work_mode_720p50(unsigned chip)
{
    // Start address 0x15, Size = 9B
    tp2802_write_table(chip, 0x15, tbl_tp2802_720p50_raster, 9);
}

static void tp2802_set_work_mode_720p60(unsigned chip)
{
    // Start address 0x15, Size = 9B
    tp2802_write_table(chip, 0x15, tbl_tp2802_720p60_raster, 9);
}

static void tp2802_set_work_mode_PAL(unsigned chip)
{
    // Start address 0x15, Size = 9B
    tp2802_write_table(chip, 0x15, tbl_tp2802_PAL_raster, 9);
}

static void tp2802_set_work_mode_NTSC(unsigned chip)
{
    // Start address 0x15, Size = 9B
    tp2802_write_table(chip, 0x15, tbl_tp2802_NTSC_raster, 9);
}
static void tp2802_set_work_mode_3M(unsigned chip)
{
    // Start address 0x15, Size = 9B
    tp2802_write_table(chip, 0x15, tbl_tp2802_3M_raster, 9);
}

static void tp2802_set_work_mode_5M(unsigned chip)
{
    // Start address 0x15, Size = 9B
    tp2802_write_table(chip, 0x15, tbl_tp2802_5M_raster, 9);
}
static void tp2802_set_work_mode_4M(unsigned chip)
{
    // Start address 0x15, Size = 9B
    tp2802_write_table(chip, 0x15, tbl_tp2802_4M_raster, 9);
}
static void tp2802_set_work_mode_3M20(unsigned chip)
{
    // Start address 0x15, Size = 9B
    tp2802_write_table(chip, 0x15, tbl_tp2802_3M20_raster, 9);
}
static void tp2802_set_work_mode_4M12(unsigned chip)
{
    // Start address 0x15, Size = 9B
    tp2802_write_table(chip, 0x15, tbl_tp2802_4M12_raster, 9);
}
static void tp2802_set_work_mode_6M10(unsigned chip)
{
    // Start address 0x15, Size = 9B
    tp2802_write_table(chip, 0x15, tbl_tp2802_6M10_raster, 9);
}
static void tp2802_set_work_mode_QHDH30(unsigned chip)
{
    // Start address 0x15, Size = 9B
    tp2802_write_table(chip, 0x15, tbl_tp2802_QHDH30_raster, 9);
}
static void tp2802_set_work_mode_QHDH25(unsigned chip)
{
    // Start address 0x15, Size = 9B
    tp2802_write_table(chip, 0x15, tbl_tp2802_QHDH25_raster, 9);
}
static void tp2802_set_work_mode_QHD15(unsigned chip)
{
    // Start address 0x15, Size = 9B
    tp2802_write_table(chip, 0x15, tbl_tp2802_QHD15_raster, 9);
}
static void tp2802_set_work_mode_QXGAH30(unsigned chip)
{
    // Start address 0x15, Size = 9B
    tp2802_write_table(chip, 0x15, tbl_tp2802_QXGAH30_raster, 9);
}
static void tp2802_set_work_mode_QXGAH25(unsigned chip)
{
    // Start address 0x15, Size = 9B
    tp2802_write_table(chip, 0x15, tbl_tp2802_QXGAH25_raster, 9);
}
static void tp2802_set_work_mode_QHD30(unsigned chip)
{
    // Start address 0x15, Size = 9B
    tp2802_write_table(chip, 0x15, tbl_tp2802_QHD30_raster, 9);
}
static void tp2802_set_work_mode_QHD25(unsigned chip)
{
    // Start address 0x15, Size = 9B
    tp2802_write_table(chip, 0x15, tbl_tp2802_QHD25_raster, 9);
}
static void tp2802_set_work_mode_QXGA30(unsigned chip)
{
    // Start address 0x15, Size = 9B
    tp2802_write_table(chip, 0x15, tbl_tp2802_QXGA30_raster, 9);
}
static void tp2802_set_work_mode_QXGA25(unsigned chip)
{
    // Start address 0x15, Size = 9B
    tp2802_write_table(chip, 0x15, tbl_tp2802_QXGA25_raster, 9);
}
/*
static void tp2802_set_work_mode_4M30(unsigned chip)
{
    // Start address 0x15, Size = 9B
    tp2802_write_table(chip, 0x15, tbl_tp2802_4M30_raster, 9);
}
static void tp2802_set_work_mode_4M25(unsigned chip)
{
    // Start address 0x15, Size = 9B
    tp2802_write_table(chip, 0x15, tbl_tp2802_4M25_raster, 9);
}
*/
static void tp2802_set_work_mode_5M20(unsigned chip)
{
    // Start address 0x15, Size = 9B
    tp2802_write_table(chip, 0x15, tbl_tp2802_5M20_raster, 9);
}
static void tp2802_set_work_mode_5MH20(unsigned chip)
{
    // Start address 0x15, Size = 9B
    tp2802_write_table(chip, 0x15, tbl_tp2802_5MH20_raster, 9);
}
/*
static void tp2802_set_work_mode_4MH30(unsigned chip)
{
    // Start address 0x15, Size = 9B
    tp2802_write_table(chip, 0x15, tbl_tp2802_4MH30_raster, 9);
}
static void tp2802_set_work_mode_4MH25(unsigned chip)
{
    // Start address 0x15, Size = 9B
    tp2802_write_table(chip, 0x15, tbl_tp2802_4MH25_raster, 9);
}
*/
static void tp2802_set_work_mode_8M15(unsigned chip)
{
    // Start address 0x15, Size = 9B
    tp2802_write_table(chip, 0x15, tbl_tp2802_8M15_raster, 9);
}
static void tp2802_set_work_mode_8MH15(unsigned chip)
{
    // Start address 0x15, Size = 9B
    tp2802_write_table(chip, 0x15, tbl_tp2802_8MH15_raster, 9);
}
static void tp2802_set_work_mode_8M12(unsigned chip)
{
    // Start address 0x15, Size = 9B
    tp2802_write_table(chip, 0x15, tbl_tp2802_8M12_raster, 9);
}
static void tp2802_set_work_mode_8MH12(unsigned chip)
{
    // Start address 0x15, Size = 9B
    tp2802_write_table(chip, 0x15, tbl_tp2802_8MH12_raster, 9);
}
static void tp2802_set_work_mode_720p30HDR(unsigned chip)
{
    // Start address 0x15, Size = 9B
    tp2802_write_table(chip, 0x15, tbl_tp2802_720p30HDR_raster, 9);
}
static void tp2802_set_work_mode_6M20(unsigned chip)
{
    // Start address 0x15, Size = 9B
    tp2802_write_table(chip, 0x15, tbl_tp2802_6M20_raster, 9);
}

//#define AMEND
#include "tp2825b_tbl.c" //

long tp2802_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
    unsigned int __user *argp = (unsigned int __user *)arg;
    unsigned int i, j, chip, tmp, ret = 0;
   // unsigned long flags;

    tp2802_register		   dev_register;
    tp2802_image_adjust    image_adjust;
    tp2802_work_mode       work_mode;
    tp2802_video_mode	   video_mode;
    tp2802_video_loss      video_loss;
    tp2802_PTZ_data        PTZ_data;

    switch (_IOC_NR(cmd))
    {

    case _IOC_NR(TP2802_READ_REG):
    {
        if (copy_from_user(&dev_register, argp, sizeof(tp2802_register)))
            return FAILURE;

        down(&watchdog_lock);

        chip = dev_register.chip;

        tp2802_set_reg_page(chip, dev_register.ch);

        dev_register.value = tp28xx_byte_read(chip, dev_register.reg_addr);

        up(&watchdog_lock);

        if (copy_to_user(argp, &dev_register, sizeof(tp2802_register)))
            return FAILURE;

        break;
    }

    case _IOC_NR(TP2802_WRITE_REG):
    {
        if (copy_from_user(&dev_register, argp, sizeof(tp2802_register)))
            return FAILURE;

        down(&watchdog_lock);

        chip = dev_register.chip;

        tp2802_set_reg_page(chip, dev_register.ch);

        tp28xx_byte_write(chip, dev_register.reg_addr, dev_register.value);

        up(&watchdog_lock);
        break;
    }

    case _IOC_NR(TP2802_SET_VIDEO_MODE):
    {
        if (copy_from_user(&video_mode, argp, sizeof(tp2802_video_mode)))
            return FAILURE;

        if(video_mode.ch >= CHANNELS_PER_CHIP)  return FAILURE;

        down(&watchdog_lock);

        ret = tp2802_set_video_mode(video_mode.chip, video_mode.mode, video_mode.ch, video_mode.std);

        up(&watchdog_lock);

        if (!(ret))
        {

            watchdog_info[video_mode.chip].mode[video_mode.ch] = video_mode.mode;
            watchdog_info[video_mode.chip].std[video_mode.ch] = video_mode.std;
            return SUCCESS;
        }
        else
        {
            printk("Invalid mode:%d\n", video_mode.mode);
            return FAILURE;
        }

        break;
    }

    case _IOC_NR(TP2802_GET_VIDEO_MODE):
    {
        if (copy_from_user(&video_mode, argp, sizeof(tp2802_video_mode)))
            return FAILURE;

        //if(video_mode.ch >= CHANNELS_PER_CHIP)  return FAILURE;
        if(video_mode.ch >= CHANNELS_PER_CHIP) video_mode.ch = 0;
#if (WDT)
        video_mode.mode = watchdog_info[video_mode.chip].mode[video_mode.ch];
        video_mode.std = watchdog_info[video_mode.chip].std[video_mode.ch];
#else
        down(&watchdog_lock);

        chip = video_mode.chip;

        tp2802_set_reg_page(chip, video_mode.ch);

        tmp = tp28xx_byte_read(chip, 0x03);
        tmp &= 0x7; /* [2:0] - CVSTD */
        video_mode.mode = tmp;

        up(&watchdog_lock);
#endif
        if (copy_to_user(argp, &video_mode, sizeof(tp2802_video_mode)))
            return FAILURE;
        break;
    }

    case _IOC_NR(TP2802_GET_VIDEO_LOSS):/* get video loss state */
    {
        if (copy_from_user(&video_loss, argp, sizeof(tp2802_video_loss)))
            return FAILURE;

        //if(video_loss.ch >= CHANNELS_PER_CHIP)  return FAILURE;
        if(video_mode.ch >= CHANNELS_PER_CHIP) video_mode.ch = 0;
#if (WDT)
        video_loss.is_lost = ( VIDEO_LOCKED == watchdog_info[video_loss.chip].state[video_loss.ch] ) ? 0:1;
        if(video_loss.is_lost) video_loss.is_lost = ( VIDEO_UNLOCK == watchdog_info[video_loss.chip].state[video_loss.ch] ) ? 0:1;
#else
        down(&watchdog_lock);

        chip = video_loss.chip;

        tp2802_set_reg_page(chip, video_loss.ch);

        tmp = tp28xx_byte_read(chip, 0x01);
        tmp = (tmp & 0x80) >> 7;
        if(!tmp)
        {
            if(0x08 == tp28xx_byte_read(chip, 0x2f))
            {
                tmp = tp28xx_byte_read(chip, 0x04);
                if(tmp < 0x30) tmp = 0;
                else tmp = 1;
            }

        }
        video_loss.is_lost = tmp;   /* [7] - VDLOSS */

        up(&watchdog_lock);
#endif
        if (copy_to_user(argp, &video_loss, sizeof(video_loss)))
            return FAILURE;

        break;
    }

    case _IOC_NR(TP2802_SET_IMAGE_ADJUST):
    {
        if (copy_from_user(&image_adjust, argp, sizeof(tp2802_image_adjust)))
        {
            return FAILURE;
        }

        if(image_adjust.ch >= CHANNELS_PER_CHIP)  return FAILURE;

        down(&watchdog_lock);

        chip = image_adjust.chip;

        tp2802_set_reg_page(chip, image_adjust.ch);

        // Set Brightness
        tp28xx_byte_write(chip, BRIGHTNESS, image_adjust.brightness);

        // Set Contrast
        tp28xx_byte_write(chip, CONTRAST, image_adjust.contrast);

        // Set Saturation
        tp28xx_byte_write(chip, SATURATION, image_adjust.saturation);

        // Set Hue
        tp28xx_byte_write(chip, HUE, image_adjust.hue);

        // Set Sharpness
        tmp = tp28xx_byte_read(chip, SHARPNESS);
        tmp &= 0xe0;
        tmp |= (image_adjust.sharpness & 0x1F);
        tp28xx_byte_write(chip, SHARPNESS, tmp);

        up(&watchdog_lock);
        break;
    }

    case _IOC_NR(TP2802_GET_IMAGE_ADJUST):
    {
        if (copy_from_user(&image_adjust, argp, sizeof(tp2802_image_adjust)))
            return FAILURE;

        if(image_adjust.ch >= CHANNELS_PER_CHIP)  return FAILURE;

        down(&watchdog_lock);

        chip = image_adjust.chip;

        tp2802_set_reg_page(chip, image_adjust.ch);

        // Get Brightness
        image_adjust.brightness = tp28xx_byte_read(chip, BRIGHTNESS);

        // Get Contrast
        image_adjust.contrast = tp28xx_byte_read(chip, CONTRAST);

        // Get Saturation
        image_adjust.saturation = tp28xx_byte_read(chip, SATURATION);

        // Get Hue
        image_adjust.hue = tp28xx_byte_read(chip, HUE);

        // Get Sharpness
        image_adjust.sharpness = 0x1F & tp28xx_byte_read(chip, SHARPNESS);

        up(&watchdog_lock);

        if (copy_to_user(argp, &image_adjust, sizeof(tp2802_image_adjust)))
            return FAILURE;

        break;
    }
    case _IOC_NR(TP2802_SET_PTZ_DATA):
    {
        if (copy_from_user(&PTZ_data, argp, sizeof(tp2802_PTZ_data)))
        {
            return FAILURE;
        }

        //if(PTZ_data.ch > PTZ_PIN_PTZ2)  return FAILURE;

        down(&watchdog_lock);

        chip = PTZ_data.chip;

        {
            tp28xx_byte_write(chip, 0x40,  0x0); //bank switch
            TP2825B_PTZ_mode(chip, PTZ_data.ch, PTZ_data.mode );

			for(i = 0; i < 24; i++)
			{
                tp28xx_byte_write(chip, 0x55+i, 0x00);
			}

			if(PTZ_HDC == PTZ_data.mode || PTZ_HDC_8M12 == PTZ_data.mode) //HDC 1080p
			{

								HDC_SetData(chip, 0x56, PTZ_data.data[0]);
								HDC_SetData(chip, 0x5c, PTZ_data.data[1]);
								HDC_SetData(chip, 0x62, PTZ_data.data[2]);
								HDC_SetData(chip, 0x68, PTZ_data.data[3]);
								TP2825B_StartTX(chip, PTZ_data.ch);
								HDC_SetData(chip, 0x56, PTZ_data.data[4]);
								HDC_SetData(chip, 0x5c, PTZ_data.data[5]);
								HDC_SetData(chip, 0x62, PTZ_data.data[6]);
								HDC_SetData(chip, 0x68, 0xffff);
								TP2825B_StartTX(chip, PTZ_data.ch);

            }
			else if(PTZ_HDC_FIFO == PTZ_data.mode ) //HDC 1080p FIFO mode
			{

                           for(i = 0; i < 7; i++)
                            {
                                tp28xx_byte_write(chip, 0x6e, PTZ_data.data[i]);
                            }

                            TP2825B_StartTX(chip, PTZ_data.ch);

            }
			else if(PTZ_HDC_QHD == PTZ_data.mode || PTZ_HDC_8M15 == PTZ_data.mode ) //HDC QHD
			{

								HDC_QHD_SetData(chip, 0x56, PTZ_data.data[0]);
								HDC_QHD_SetData(chip, 0x5c, PTZ_data.data[1]);
								HDC_QHD_SetData(chip, 0x62, PTZ_data.data[2]);
								HDC_QHD_SetData(chip, 0x68, PTZ_data.data[3]);
								TP2825B_StartTX(chip, PTZ_data.ch);
								HDC_QHD_SetData(chip, 0x56, PTZ_data.data[4]);
								HDC_QHD_SetData(chip, 0x5c, PTZ_data.data[5]);
								HDC_QHD_SetData(chip, 0x62, PTZ_data.data[6]);
								HDC_QHD_SetData(chip, 0x68, 0xffff);
								TP2825B_StartTX(chip, PTZ_data.ch);

            }
            else if(PTZ_HDA_4M25 == PTZ_data.mode || PTZ_HDA_4M15 == PTZ_data.mode) //HDA QHD
            {
                           for(i = 0; i < 8; i++)
                            {
                                tp28xx_byte_write(chip, 0x6e, 0x00);
                            }
                            TP2825B_StartTX(chip, PTZ_data.ch);

                            for(i = 0; i < 8; i++)
                            {
                                tp28xx_byte_write(chip, 0x6e, ReverseByte(PTZ_data.data[i]));
                            }
                            TP2825B_StartTX(chip, PTZ_data.ch);

            }
            else if(PTZ_HDA_1080P == PTZ_data.mode || PTZ_HDA_3M18 == PTZ_data.mode || PTZ_HDA_3M25 == PTZ_data.mode) //HDA 1080p
            {
								HDA_SetACPV2Data(chip, 0x58, 0x00);
								HDA_SetACPV2Data(chip, 0x5e, 0x00);
								HDA_SetACPV2Data(chip, 0x64, 0x00);
								HDA_SetACPV2Data(chip, 0x6a, 0x00);
								TP2825B_StartTX(chip, PTZ_data.ch);
								HDA_SetACPV2Data(chip, 0x58, PTZ_data.data[0]);
								HDA_SetACPV2Data(chip, 0x5e, PTZ_data.data[1]);
								HDA_SetACPV2Data(chip, 0x64, PTZ_data.data[2]);
								HDA_SetACPV2Data(chip, 0x6a, PTZ_data.data[3]);
								TP2825B_StartTX(chip, PTZ_data.ch);

            }
            else if( PTZ_HDA_720P == PTZ_data.mode ) //HDA 720p
            {
								HDA_SetACPV1Data(chip, 0x55, 0x00);
								HDA_SetACPV1Data(chip, 0x58, 0x00);
								HDA_SetACPV1Data(chip, 0x5b, 0x00);
								HDA_SetACPV1Data(chip, 0x5e, 0x00);
								TP2825B_StartTX(chip, PTZ_data.ch);
								HDA_SetACPV1Data(chip, 0x55, PTZ_data.data[0]);
								HDA_SetACPV1Data(chip, 0x58, PTZ_data.data[1]);
								HDA_SetACPV1Data(chip, 0x5b, PTZ_data.data[2]);
								HDA_SetACPV1Data(chip, 0x5e, PTZ_data.data[3]);
								TP2825B_StartTX(chip, PTZ_data.ch);
            }
            else if( PTZ_HDA_CVBS == PTZ_data.mode) //HDA 960H
            {
								HDA_SetACPV1Data(chip, 0x55, 0x00);
								HDA_SetACPV1Data(chip, 0x58, 0x00);
								HDA_SetACPV1Data(chip, 0x5b, 0x00);
								HDA_SetACPV1Data(chip, 0x5e, 0x00);
								//TP2825B_StartTX(chip, PTZ_data.ch);
								TP2825B_StartTX(chip, PTZ_data.ch);
								HDA_SetACPV1Data(chip, 0x55, PTZ_data.data[0]);
								HDA_SetACPV1Data(chip, 0x58, PTZ_data.data[1]);
								HDA_SetACPV1Data(chip, 0x5b, PTZ_data.data[2]);
								HDA_SetACPV1Data(chip, 0x5e, PTZ_data.data[3]);
								//TP2825B_StartTX(chip, PTZ_data.ch);
								TP2825B_StartTX(chip, PTZ_data.ch);
            }
            else //TVI
            {
                            //line1
                                tp28xx_byte_write(chip, 0x56 , 0x02);
                                tp28xx_byte_write(chip, 0x57 , PTZ_data.data[0]);
                                tp28xx_byte_write(chip, 0x58 , PTZ_data.data[1]);
                                tp28xx_byte_write(chip, 0x59 , PTZ_data.data[2]);
                                tp28xx_byte_write(chip, 0x5A , PTZ_data.data[3]);
                            //line2
                                tp28xx_byte_write(chip, 0x5C , 0x02);
                                tp28xx_byte_write(chip, 0x5D , PTZ_data.data[4]);
                                tp28xx_byte_write(chip, 0x5E , PTZ_data.data[5]);
                                tp28xx_byte_write(chip, 0x5F , PTZ_data.data[6]);
                                tp28xx_byte_write(chip, 0x60 , PTZ_data.data[7]);

                            //line3
                                tp28xx_byte_write(chip, 0x62 , 0x02);
                                tp28xx_byte_write(chip, 0x63 , PTZ_data.data[0]);
                                tp28xx_byte_write(chip, 0x64 , PTZ_data.data[1]);
                                tp28xx_byte_write(chip, 0x65 , PTZ_data.data[2]);
                                tp28xx_byte_write(chip, 0x66 , PTZ_data.data[3]);
                            //line4
                                tp28xx_byte_write(chip, 0x68 , 0x02);
                                tp28xx_byte_write(chip, 0x69 , PTZ_data.data[4]);
                                tp28xx_byte_write(chip, 0x6A , PTZ_data.data[5]);
                                tp28xx_byte_write(chip, 0x6B , PTZ_data.data[6]);
                                tp28xx_byte_write(chip, 0x6C , PTZ_data.data[7]);

                                TP2825B_StartTX(chip, PTZ_data.ch);
            }


        }
        up(&watchdog_lock);
        break;
    }
    case _IOC_NR(TP2802_GET_PTZ_DATA):
    {
        if (copy_from_user(&PTZ_data, argp, sizeof(tp2802_PTZ_data)))
        {
            return FAILURE;
        }

        if(PTZ_data.ch >= CHANNELS_PER_CHIP)  return FAILURE;

        down(&watchdog_lock);

        chip = PTZ_data.chip;

        {
            tp28xx_byte_write(chip, 0x40, 0x0); //bank switch
            // line1
                PTZ_data.data[0]=tp28xx_byte_read(chip, 0x8C );
                PTZ_data.data[1]=tp28xx_byte_read(chip, 0x8D );
                PTZ_data.data[2]=tp28xx_byte_read(chip, 0x8E );
                PTZ_data.data[3]=tp28xx_byte_read(chip, 0x8F );
            //line2
                PTZ_data.data[4]=tp28xx_byte_read(chip, 0x92 );
                PTZ_data.data[5]=tp28xx_byte_read(chip, 0x93 );
                PTZ_data.data[6]=tp28xx_byte_read(chip, 0x94 );
                PTZ_data.data[7]=tp28xx_byte_read(chip, 0x95 );
            // line3
                PTZ_data.data[8]=tp28xx_byte_read(chip, 0x98 );
                PTZ_data.data[9]=tp28xx_byte_read(chip, 0x99 );
                PTZ_data.data[10]=tp28xx_byte_read(chip, 0x9a );
                PTZ_data.data[11]=tp28xx_byte_read(chip, 0x9b );
            //line4
                PTZ_data.data[12]=tp28xx_byte_read(chip, 0x9e );
                PTZ_data.data[13]=tp28xx_byte_read(chip, 0x9f );
                PTZ_data.data[14]=tp28xx_byte_read(chip, 0xa0 );
                PTZ_data.data[15]=tp28xx_byte_read(chip, 0xa1 );
        }

        up(&watchdog_lock);

        if (copy_to_user(argp, &PTZ_data, sizeof(tp2802_PTZ_data)))
            return FAILURE;

        break;
    }
    case _IOC_NR(TP2802_SET_SCAN_MODE):
    {
        if (copy_from_user(&work_mode, argp, sizeof(tp2802_work_mode)))
            return FAILURE;

        down(&watchdog_lock);

        if(work_mode.ch >= CHANNELS_PER_CHIP)
        {
            for(i = 0; i < CHANNELS_PER_CHIP; i++)
                watchdog_info[work_mode.chip].scan[i] = work_mode.mode;
        }
        else
        {
            watchdog_info[work_mode.chip].scan[work_mode.ch] = work_mode.mode;

        }

        up(&watchdog_lock);


        break;
    }
    case _IOC_NR(TP2802_DUMP_REG):
    {
        if (copy_from_user(&dev_register, argp, sizeof(tp2802_register)))
            return FAILURE;

        down(&watchdog_lock);

        tp2802_set_reg_page(dev_register.chip, MIPI_PAGE);
        printk("MIPI\n");
        for(j = 0; j < 0x40; j++)
            {
                dev_register.value = tp28xx_byte_read(dev_register.chip, j);
                printk("%02x:%02x\n", j, dev_register.value );
            }
        printk("VIDEO\n");
        tp2802_set_reg_page(dev_register.chip, VIDEO_PAGE);
        for(j = 0x00; j < 0x100; j++)
        {
            dev_register.value = tp28xx_byte_read(dev_register.chip, j);
            printk("%02x:%02x\n", j, dev_register.value );
        }

        up(&watchdog_lock);

        if (copy_to_user(argp, &dev_register, sizeof(tp2802_register)))
            return FAILURE;

        break;
    }
    case _IOC_NR(TP2802_FORCE_DETECT):
    {
        if (copy_from_user(&work_mode, argp, sizeof(tp2802_work_mode)))
            return FAILURE;

        down(&watchdog_lock);

        if(work_mode.ch >= CHANNELS_PER_CHIP)
        {
            for(i = 0; i < CHANNELS_PER_CHIP; i++)
                watchdog_info[work_mode.chip].force[i] = 1;
        }
        else
        {
            watchdog_info[work_mode.chip].force[work_mode.ch] = 1;

        }

        up(&watchdog_lock);


        break;
    }

    case _IOC_NR(TP2802_SET_BURST_DATA):
    {
        if (copy_from_user(&PTZ_data, argp, sizeof(tp2802_PTZ_data)))
        {
            return FAILURE;
        }

        //if(PTZ_data.ch >= CHANNELS_PER_CHIP)  return FAILURE;

        down(&watchdog_lock);

        chip = PTZ_data.chip;

        {
            tp28xx_byte_write(chip, 0x40,  0x0); //bank switch
            TP2825B_PTZ_mode(chip, PTZ_data.ch, PTZ_data.mode );

                tp28xx_byte_write(chip, 0x55, 0x00);
                tp28xx_byte_write(chip, 0x5b, 0x00);

                            //line1
                                tp28xx_byte_write(chip, 0x56 , 0x03);
                                tp28xx_byte_write(chip, 0x57 , PTZ_data.data[0]);
                                tp28xx_byte_write(chip, 0x58 , PTZ_data.data[1]);
                                tp28xx_byte_write(chip, 0x59 , PTZ_data.data[2]);
                                tp28xx_byte_write(chip, 0x5A , PTZ_data.data[3]);
                            //line2
                                tp28xx_byte_write(chip, 0x5C , 0x03);
                                tp28xx_byte_write(chip, 0x5D , PTZ_data.data[4]);
                                tp28xx_byte_write(chip, 0x5E , PTZ_data.data[5]);
                                tp28xx_byte_write(chip, 0x5F , PTZ_data.data[6]);
                                tp28xx_byte_write(chip, 0x60 , PTZ_data.data[7]);
                            //line3
                                tp28xx_byte_write(chip, 0x62 , 0x03);
                                tp28xx_byte_write(chip, 0x63 , PTZ_data.data[8]);
                                tp28xx_byte_write(chip, 0x64 , PTZ_data.data[9]);
                                tp28xx_byte_write(chip, 0x65 , PTZ_data.data[10]);
                                tp28xx_byte_write(chip, 0x66 , PTZ_data.data[11]);
                            //line4
                                tp28xx_byte_write(chip, 0x68 , 0x03);
                                tp28xx_byte_write(chip, 0x69 , PTZ_data.data[12]);
                                tp28xx_byte_write(chip, 0x6A , PTZ_data.data[13]);
                                tp28xx_byte_write(chip, 0x6B , PTZ_data.data[14]);
                                tp28xx_byte_write(chip, 0x6C , PTZ_data.data[15]);

            TP2825B_StartTX(chip, PTZ_data.ch);
        }

        up(&watchdog_lock);
        break;
    }

    case _IOC_NR(TP2802_SET_VIDEO_INPUT):
    {
        if (copy_from_user(&dev_register, argp, sizeof(tp2802_register)))
            return FAILURE;

        if(dev_register.value > DIFF_VIN34)  return FAILURE;

        down(&watchdog_lock);

        chip = dev_register.chip;
        tp28xx_byte_write(chip, 0x40,  0x0); //bank switch
        tmp = tp28xx_byte_read(chip, 0x41);
        tmp &= 0xf0;
        tmp |= dev_register.value;
        tp28xx_byte_write(chip, 0x41, tmp);

        if(dev_register.value < DIFF_VIN12)
        {
            //single
            tp28xx_byte_write(chip, 0x38, 0x40);
            tp28xx_byte_write(chip, 0x3d, 0x60);
            //tp28xx_byte_write(chip, 0x3e, 0x00);
        }
        else
        {
            //differential
            tp28xx_byte_write(chip, 0x38, 0x4e);
            tp28xx_byte_write(chip, 0x3d, 0x40);
            //tp28xx_byte_write(chip, 0x3e, 0x80);
        }

        watchdog_info[chip].force[0] = 1; //reset when Vin is changed

        up(&watchdog_lock);

        if (copy_to_user(argp, &dev_register, sizeof(tp2802_register)))
            return FAILURE;

        break;
    }
    case _IOC_NR(TP2802_SET_PTZ_MODE):
    {
        if (copy_from_user(&PTZ_data, argp, sizeof(tp2802_PTZ_data)))
        {
            return FAILURE;
        }

        if(PTZ_data.ch > PTZ_PIN_PTZ2)  return FAILURE;

        down(&watchdog_lock);

        chip = PTZ_data.chip;

        //if( TP2825B == id[chip])
        {
            TP2825B_PTZ_mode(chip, PTZ_data.ch, PTZ_data.mode );
        }
        up(&watchdog_lock);
        break;
    }
    case _IOC_NR(TP2802_SET_RX_MODE):
    {
        if (copy_from_user(&PTZ_data, argp, sizeof(tp2802_PTZ_data)))
        {
            return FAILURE;
        }


        down(&watchdog_lock);

        chip = PTZ_data.chip;

        tp28xx_byte_write(chip, 0x40,  0x0); //bank switch

        TP2825B_RX_init(chip, PTZ_data.mode);

        up(&watchdog_lock);
        break;
    }

    default:
    {
        printk("Invalid tp2802 ioctl cmd!\n");
        ret = -1;
        break;
    }
    }

    return ret;
}


static void TP2825B_Set_REG0X02(unsigned char chip, unsigned char data )
{
		if(MUX656_8BIT == output[chip] || SEP656_8BIT == output[chip])
        {
            tp28xx_byte_write(chip, 0x02, data);
        }
        else
        {
            tp28xx_byte_write(chip, 0x02, data&0x7f);
        }
}
//////////////////////////////////////////////////////////////////////////////////////////////

static void TP2826_C1080P25_DataSet(unsigned char chip)
{

    tp28xx_byte_write(chip, 0x13, 0x40);

    tp28xx_byte_write(chip, 0x20, 0x50);

    tp28xx_byte_write(chip, 0x26, 0x01);
    tp28xx_byte_write(chip, 0x27, 0x5a);
    tp28xx_byte_write(chip, 0x28, 0x04);

    tp28xx_byte_write(chip, 0x2b, 0x60);

    tp28xx_byte_write(chip, 0x2d, 0x54);
    tp28xx_byte_write(chip, 0x2e, 0x40);

    tp28xx_byte_write(chip, 0x30, 0x41);
    tp28xx_byte_write(chip, 0x31, 0x82);
    tp28xx_byte_write(chip, 0x32, 0x27);
    tp28xx_byte_write(chip, 0x33, 0xa2);

}
static void TP2826_C720P25_DataSet(unsigned char chip)
{

    tp28xx_byte_write(chip, 0x13, 0x40);

    tp28xx_byte_write(chip, 0x20, 0x3a);

    tp28xx_byte_write(chip, 0x26, 0x01);
    tp28xx_byte_write(chip, 0x27, 0x5a);
    tp28xx_byte_write(chip, 0x28, 0x04);

    tp28xx_byte_write(chip, 0x2b, 0x60);
    tp28xx_byte_write(chip, 0x2d, 0x36);
    tp28xx_byte_write(chip, 0x2e, 0x40);

    tp28xx_byte_write(chip, 0x30, 0x48);
    tp28xx_byte_write(chip, 0x31, 0x67);
    tp28xx_byte_write(chip, 0x32, 0x6f);
    tp28xx_byte_write(chip, 0x33, 0x33);

}
static void TP2826_C720P50_DataSet(unsigned char chip)
{

    tp28xx_byte_write(chip, 0x13, 0x40);

    tp28xx_byte_write(chip, 0x20, 0x3a);

    tp28xx_byte_write(chip, 0x26, 0x01);
    tp28xx_byte_write(chip, 0x27, 0x5a);
    tp28xx_byte_write(chip, 0x28, 0x04);

    tp28xx_byte_write(chip, 0x2b, 0x60);

    tp28xx_byte_write(chip, 0x2d, 0x42);
    tp28xx_byte_write(chip, 0x2e, 0x40);

    tp28xx_byte_write(chip, 0x30, 0x41);
    tp28xx_byte_write(chip, 0x31, 0x82);
    tp28xx_byte_write(chip, 0x32, 0x27);
    tp28xx_byte_write(chip, 0x33, 0xa3);

}
static void TP2826_C1080P30_DataSet(unsigned char chip)
{

    tp28xx_byte_write(chip, 0x13, 0x40);

    tp28xx_byte_write(chip, 0x20, 0x3c);

    tp28xx_byte_write(chip, 0x26, 0x01);
    tp28xx_byte_write(chip, 0x27, 0x5a);
    tp28xx_byte_write(chip, 0x28, 0x04);

    tp28xx_byte_write(chip, 0x2b, 0x60);

    tp28xx_byte_write(chip, 0x2d, 0x4c);
    tp28xx_byte_write(chip, 0x2e, 0x40);

    tp28xx_byte_write(chip, 0x30, 0x41);
    tp28xx_byte_write(chip, 0x31, 0x82);
    tp28xx_byte_write(chip, 0x32, 0x27);
    tp28xx_byte_write(chip, 0x33, 0xa4);

}
static void TP2826_C720P30_DataSet(unsigned char chip)
{

    tp28xx_byte_write(chip, 0x13, 0x40);

    tp28xx_byte_write(chip, 0x20, 0x30);

    tp28xx_byte_write(chip, 0x26, 0x01);
    tp28xx_byte_write(chip, 0x27, 0x5a);
    tp28xx_byte_write(chip, 0x28, 0x04);

    tp28xx_byte_write(chip, 0x2b, 0x60);

    tp28xx_byte_write(chip, 0x2d, 0x37);
    tp28xx_byte_write(chip, 0x2e, 0x40);

    tp28xx_byte_write(chip, 0x30, 0x48);
    tp28xx_byte_write(chip, 0x31, 0x67);
    tp28xx_byte_write(chip, 0x32, 0x6f);
    tp28xx_byte_write(chip, 0x33, 0x30);

}
static void TP2826_C720P60_DataSet(unsigned char chip)
{

    tp28xx_byte_write(chip, 0x13, 0x40);

    tp28xx_byte_write(chip, 0x20, 0x30);

    tp28xx_byte_write(chip, 0x26, 0x01);
    tp28xx_byte_write(chip, 0x27, 0x5a);
    tp28xx_byte_write(chip, 0x28, 0x04);

    tp28xx_byte_write(chip, 0x2b, 0x60);

    tp28xx_byte_write(chip, 0x2d, 0x37);
    tp28xx_byte_write(chip, 0x2e, 0x40);

    tp28xx_byte_write(chip, 0x30, 0x41);
    tp28xx_byte_write(chip, 0x31, 0x82);
    tp28xx_byte_write(chip, 0x32, 0x27);
    tp28xx_byte_write(chip, 0x33, 0xa0);

}
//////////////////////////////////////////////////////////////////////////////
static int tp2802_set_video_mode(unsigned char chip, unsigned char mode, unsigned char ch, unsigned char std)
{
    int err=0;
    unsigned int tmp;


    switch(mode)
    {
    case TP2802_1080P25:
        tp2802_set_work_mode_1080p25(chip);
        TP2825B_Set_REG0X02(chip, 0xC8);
        TP2825B_V1_DataSet(chip);
        if( STD_HDA == std)
        {
            if(TP2860 == id[chip])
            {
                TP2860_A1080P25_DataSet(chip);
            }
            else
            {
                TP2825B_A1080P25_DataSet(chip);
            }

        }
        else if(STD_HDC == std || STD_HDC_DEFAULT == std)
        {
            TP2826_C1080P25_DataSet(chip);
            if(STD_HDC == std)
            {
                        tp28xx_byte_write(chip, 0x15, 0x13);
                        tp28xx_byte_write(chip, 0x16, 0x60);
                        tp28xx_byte_write(chip, 0x17, 0x80);
                        tp28xx_byte_write(chip, 0x18, 0x29);
                        tp28xx_byte_write(chip, 0x19, 0x38);
                        tp28xx_byte_write(chip, 0x1A, 0x47);
                        tp28xx_byte_write(chip, 0x1C, 0x09);
                        tp28xx_byte_write(chip, 0x1D, 0x60);
            }

        }

        if( STD_HDA == std && TP2860 == id[chip])
        {
                TP2860_SYSCLK_A1080P(chip);
        }
        else
        {
                TP2825B_SYSCLK_V1(chip);
        }
        break;

    case TP2802_1080P30:
        tp2802_set_work_mode_1080p30(chip);
        TP2825B_Set_REG0X02(chip, 0xC8);
        TP2825B_V1_DataSet(chip);
        if( STD_HDA == std)
        {
            if(TP2860 == id[chip])
            {
                TP2860_A1080P30_DataSet(chip);
            }
            else
            {
                TP2825B_A1080P30_DataSet(chip);
            }
        }
        else if(STD_HDC == std || STD_HDC_DEFAULT == std)
        {
            TP2826_C1080P30_DataSet(chip);
            if(STD_HDC == std)
            {
                        tp28xx_byte_write(chip, 0x15, 0x13);
                        tp28xx_byte_write(chip, 0x16, 0x60);
                        tp28xx_byte_write(chip, 0x17, 0x80);
                        tp28xx_byte_write(chip, 0x18, 0x29);
                        tp28xx_byte_write(chip, 0x19, 0x38);
                        tp28xx_byte_write(chip, 0x1A, 0x47);
                        tp28xx_byte_write(chip, 0x1C, 0x09);
                        tp28xx_byte_write(chip, 0x1D, 0x60);
            }

        }

        if( STD_HDA == std && TP2860 == id[chip])
        {
                TP2860_SYSCLK_A1080P(chip);
        }
        else
        {
                TP2825B_SYSCLK_V1(chip);
        }
        break;

    case TP2802_720P25:
        tp2802_set_work_mode_720p25(chip);
        TP2825B_Set_REG0X02(chip, 0xCA);
        TP2825B_V1_DataSet(chip);
        TP2825B_SYSCLK_V1(chip);
        break;

    case TP2802_720P30:
        tp2802_set_work_mode_720p30(chip);
        TP2825B_Set_REG0X02(chip, 0xCA);
        TP2825B_V1_DataSet(chip);
        TP2825B_SYSCLK_V1(chip);
        break;

    case TP2802_720P50:
        tp2802_set_work_mode_720p50(chip);
        TP2825B_Set_REG0X02(chip, 0xCA);
        TP2825B_V1_DataSet(chip);
        if( STD_HDA == std)
        {

        }
        else if(STD_HDC == std || STD_HDC_DEFAULT == std)
        {
            TP2826_C720P50_DataSet(chip);
            if(STD_HDC == std)
            {
                        tp28xx_byte_write(chip, 0x15, 0x13);
                        tp28xx_byte_write(chip, 0x16, 0x0a);
                        tp28xx_byte_write(chip, 0x17, 0x00);
                        tp28xx_byte_write(chip, 0x18, 0x19);
                        tp28xx_byte_write(chip, 0x19, 0xd0);
                        tp28xx_byte_write(chip, 0x1A, 0x25);
                        tp28xx_byte_write(chip, 0x1C, 0x06);
                        tp28xx_byte_write(chip, 0x1D, 0x7a);
            }

        }
        TP2825B_SYSCLK_V1(chip);
        break;

    case TP2802_720P60:
        tp2802_set_work_mode_720p60(chip);
        TP2825B_Set_REG0X02(chip, 0xCA);
        TP2825B_V1_DataSet(chip);
        if( STD_HDA == std)
        {
//            TP2826_A720P60_DataSet(chip);
        }
        else if(STD_HDC == std || STD_HDC_DEFAULT == std)
        {
            TP2826_C720P60_DataSet(chip);
            if(STD_HDC == std)
            {
                        tp28xx_byte_write(chip, 0x15, 0x13);
                        tp28xx_byte_write(chip, 0x16, 0x08);
                        tp28xx_byte_write(chip, 0x17, 0x00);
                        tp28xx_byte_write(chip, 0x18, 0x19);
                        tp28xx_byte_write(chip, 0x19, 0xd0);
                        tp28xx_byte_write(chip, 0x1A, 0x25);
                        tp28xx_byte_write(chip, 0x1C, 0x06);
                        tp28xx_byte_write(chip, 0x1D, 0x72);
            }

        }
        TP2825B_SYSCLK_V1(chip);
        break;

    case TP2802_720P30V2:
        tp2802_set_work_mode_720p60(chip);
        TP2825B_Set_REG0X02(chip, 0xCA);
        TP2825B_V2_DataSet(chip);
        if( STD_HDA == std)
        {
            if(TP2860 == id[chip])
            {
                TP2860_A720P30_DataSet(chip);
            }
            else
            {
                TP2825B_A720P30_DataSet(chip);
            }
        }
        else if(STD_HDC == std || STD_HDC_DEFAULT == std)
        {
            TP2826_C720P30_DataSet(chip);
            if(STD_HDC == std)
            {
                        tp28xx_byte_write(chip, 0x15, 0x13);
                        tp28xx_byte_write(chip, 0x16, 0x08);
                        tp28xx_byte_write(chip, 0x17, 0x00);
                        tp28xx_byte_write(chip, 0x18, 0x19);
                        tp28xx_byte_write(chip, 0x19, 0xd0);
                        tp28xx_byte_write(chip, 0x1A, 0x25);
                        tp28xx_byte_write(chip, 0x1C, 0x06);
                        tp28xx_byte_write(chip, 0x1D, 0x72);
            }

        }

        if( STD_HDA == std && TP2860 == id[chip])
        {
                TP2860_SYSCLK_A720P(chip);
        }
        else
        {
                TP2825B_SYSCLK_V2(chip);
        }
        break;

    case TP2802_720P25V2:
        tp2802_set_work_mode_720p50(chip);
        TP2825B_Set_REG0X02(chip, 0xCA);
        TP2825B_V2_DataSet(chip);
        if( STD_HDA == std)
        {
            if(TP2860 == id[chip])
            {
                TP2860_A720P25_DataSet(chip);
            }
            else
            {
                TP2825B_A720P25_DataSet(chip);
            }
        }
        else if(STD_HDC == std || STD_HDC_DEFAULT == std)
        {
            TP2826_C720P25_DataSet(chip);
            if(STD_HDC == std)
            {
                        tp28xx_byte_write(chip, 0x15, 0x13);
                        tp28xx_byte_write(chip, 0x16, 0x0a);
                        tp28xx_byte_write(chip, 0x17, 0x00);
                        tp28xx_byte_write(chip, 0x18, 0x19);
                        tp28xx_byte_write(chip, 0x19, 0xd0);
                        tp28xx_byte_write(chip, 0x1A, 0x25);
                        tp28xx_byte_write(chip, 0x1C, 0x06);
                        tp28xx_byte_write(chip, 0x1D, 0x7a);
            }

        }

        if( STD_HDA == std && TP2860 == id[chip])
        {
                TP2860_SYSCLK_A720P(chip);
        }
        else
        {
                TP2825B_SYSCLK_V2(chip);
        }
        break;

    case TP2802_PAL:
        tp2802_set_work_mode_PAL(chip);
        TP2825B_Set_REG0X02(chip, 0xCF);
        TP2825B_PAL_DataSet(chip);
        TP2825B_SYSCLK_CVBS(chip);
        break;

    case TP2802_NTSC:
        tp2802_set_work_mode_NTSC(chip);
        TP2825B_Set_REG0X02(chip, 0xCF);
        TP2825B_NTSC_DataSet(chip);
        TP2825B_SYSCLK_CVBS(chip);
        break;

    case TP2802_3M18:
        tp2802_set_work_mode_3M(chip);
        TP2825B_Set_REG0X02(chip, 0xC8);
        TP2825B_V1_DataSet(chip);
        tp28xx_byte_write(chip, 0x35, 0x16);
        tp28xx_byte_write(chip, 0x36, 0x30);
        TP2825B_SYSCLK_V1(chip);
        break;

    case TP2802_5M12:
        tp2802_set_work_mode_5M(chip);
        TP2825B_Set_REG0X02(chip, 0xC8);
        TP2825B_V1_DataSet(chip);
        tp28xx_byte_write(chip, 0x35, 0x17);
        tp28xx_byte_write(chip, 0x36, 0xd0);
        if( STD_HDA == std)
        {
            TP2825B_A5MP12_DataSet(chip);
        }
        else if(STD_HDC == std || STD_HDC_DEFAULT == std)
        {
            if(STD_HDC == std)
            {

            }

        }
        TP2825B_SYSCLK_V1(chip);
        break;

    case TP2802_4M15:
        tp2802_set_work_mode_4M(chip);
        TP2825B_Set_REG0X02(chip, 0xC8);
        TP2825B_V1_DataSet(chip);
        tp28xx_byte_write(chip, 0x35, 0x16);
        tp28xx_byte_write(chip, 0x36, 0x72);
        TP2825B_SYSCLK_V1(chip);
        break;

    case TP2802_3M20:
        tp2802_set_work_mode_3M20(chip);
        TP2825B_Set_REG0X02(chip, 0xC8);
        TP2825B_V1_DataSet(chip);
        tp28xx_byte_write(chip, 0x35, 0x16);
        tp28xx_byte_write(chip, 0x36, 0x72);
        tp28xx_byte_write(chip, 0x2d, 0x26);
        TP2825B_SYSCLK_V1(chip);
        break;

    case TP2802_6M10:
        tp2802_set_work_mode_6M10(chip);
        TP2825B_Set_REG0X02(chip, 0xC8);
        TP2825B_V1_DataSet(chip);
        tp28xx_byte_write(chip, 0x35, 0x17);
        tp28xx_byte_write(chip, 0x36, 0xbc);
        TP2825B_SYSCLK_V1(chip);
        break;

    case TP2802_QHD30:
        tp2802_set_work_mode_QHD30(chip);
        TP2825B_Set_REG0X02(chip, 0xD8);
        TP2825B_QHDP30_25_DataSet(chip);
        if( STD_HDA == std)
        {
            TP2825B_AQHDP30_DataSet(chip);
        }
        else if(STD_HDC == std || STD_HDC_DEFAULT == std)
        {
            TP2825B_CQHDP30_DataSet(chip);
            if(STD_HDC == std)
            {

            }

        }
        TP2825B_SYSCLK_V3(chip);
        break;

    case TP2802_QHD25:
        tp2802_set_work_mode_QHD25(chip);
        TP2825B_Set_REG0X02(chip, 0xD8);
        TP2825B_QHDP30_25_DataSet(chip);
        if( STD_HDA == std)
        {
            TP2825B_AQHDP25_DataSet(chip);
        }
        else if(STD_HDC == std || STD_HDC_DEFAULT == std)
        {
            TP2825B_CQHDP25_DataSet(chip);
            if(STD_HDC == std)
            {

            }

        }
        TP2825B_SYSCLK_V3(chip);
        break;

    case TP2802_QHD15:
        tp2802_set_work_mode_QHD15(chip);
        TP2825B_Set_REG0X02(chip, 0xC8);
        TP2825B_V1_DataSet(chip);
        tp28xx_byte_write(chip, 0x35, 0x15);
        tp28xx_byte_write(chip, 0x36, 0xdc);
        if( STD_HDA == std)
        {
            TP2825B_AQHDP15_DataSet(chip);
        }
        else if(STD_HDC == std || STD_HDC_DEFAULT == std)
        {
            if(STD_HDC == std)
            {

            }

        }
        TP2825B_SYSCLK_V1(chip);
        break;

    case TP2802_QXGA18:
        tp2802_set_work_mode_3M(chip);
        TP2825B_Set_REG0X02(chip, 0xC8);
        TP2825B_V1_DataSet(chip);
        tp28xx_byte_write(chip, 0x35, 0x16);
        tp28xx_byte_write(chip, 0x36, 0x72);
        if( STD_HDA == std)
        {
            TP2825B_AQXGAP18_DataSet(chip);
        }
        else if(STD_HDC == std || STD_HDC_DEFAULT == std)
        {
            if(STD_HDC == std)
            {

            }

        }
        TP2825B_SYSCLK_V1(chip);
        break;

    case TP2802_QXGA25:
        tp2802_set_work_mode_QXGA25(chip);
        TP2825B_Set_REG0X02(chip, 0xC8);
        tp28xx_byte_write(chip, 0x35, 0x16);
        tp28xx_byte_write(chip, 0x36, 0x72);
        if( STD_HDA == std)
        {
            TP2825B_AQXGAP25_DataSet(chip);
        }
        else if(STD_HDC == std || STD_HDC_DEFAULT == std)
        {
            if(STD_HDC == std)
            {

            }

        }
        TP2825B_SYSCLK_V3(chip);
        break;

    case TP2802_QXGA30:
        tp2802_set_work_mode_QXGA30(chip);
        TP2825B_Set_REG0X02(chip, 0xC8);
        tp28xx_byte_write(chip, 0x35, 0x16);
        tp28xx_byte_write(chip, 0x36, 0x72);
        if( STD_HDA == std)
        {
            TP2825B_AQXGAP30_DataSet(chip);
        }
        else if(STD_HDC == std || STD_HDC_DEFAULT == std)
        {
            if(STD_HDC == std)
            {

            }

        }
        TP2825B_SYSCLK_V3(chip);
        break;

    case TP2802_5M20:
        tp2802_set_work_mode_5M20(chip);
        TP2825B_Set_REG0X02(chip, 0xD8);
         TP2825B_5MP20_DataSet(chip);
        tp28xx_byte_write(chip, 0x35, 0x17);
        tp28xx_byte_write(chip, 0x36, 0xbc);
        if( STD_HDA == std)
        {
            TP2825B_A5MP20_DataSet(chip);
        }
        else if(STD_HDC == std || STD_HDC_DEFAULT == std)
        {
            TP2825B_C5MP20_DataSet(chip);
            if(STD_HDC == std)
            {

            }

        }
        TP2825B_SYSCLK_V3(chip);
        break;

     case TP2802_8M15:
        tp2802_set_work_mode_8M15(chip);
        TP2825B_Set_REG0X02(chip, 0xD8);
        TP2825B_8MP15_DataSet(chip);
        tp28xx_byte_write(chip, 0x35, 0x18);
        tp28xx_byte_write(chip, 0x36, 0xca);
        if( STD_HDA == std)
        {
            TP2825B_A8MP15_DataSet(chip);
        }
        else if(STD_HDC == std || STD_HDC_DEFAULT == std)
        {
            TP2825B_C8MP15_DataSet(chip);
            if(STD_HDC == std)
            {

            }

        }
        TP2825B_SYSCLK_V3(chip);
        break;

     case TP2802_8M12:
        tp2802_set_work_mode_8M12(chip);
        TP2825B_Set_REG0X02(chip, 0xD8);
        TP2825B_8MP15_DataSet(chip);
        tp28xx_byte_write(chip, 0x35, 0x18);
        tp28xx_byte_write(chip, 0x36, 0xca);
        if( STD_HDA == std)
        {

        }
        else if(STD_HDC == std || STD_HDC_DEFAULT == std)
        {
            TP2825B_C8MP12_DataSet(chip);
            if(STD_HDC == std)
            {
                        tp28xx_byte_write(chip, 0x1c, 0x13);
                        tp28xx_byte_write(chip, 0x1d, 0x10);
            }

        }
        TP2825B_SYSCLK_V3(chip);
        break;

    case TP2802_1080P60:
        TP2825B_Set_REG0X02(chip, 0xC8);
        TP2825B_1080P60_DataSet(chip);
        TP2825B_SYSCLK_V3(chip);
        break;

    case TP2802_1080P50:
        TP2825B_Set_REG0X02(chip, 0xC8);
        TP2825B_1080P50_DataSet(chip);
        TP2825B_SYSCLK_V3(chip);
        break;

    case TP2802_720P14:
        tp2802_set_work_mode_720p30(chip);
        TP2825B_Set_REG0X02(chip, 0xCA);
        TP2825B_V2_DataSet(chip);
        tp28xx_byte_write(chip, 0x35, 0x33);
        tp28xx_byte_write(chip, 0x36, 0x20);
        TP2825B_SYSCLK_V2(chip);
        break;

    case TP2802_720P30HDR:
        tp2802_set_work_mode_720p30HDR(chip);
        TP2825B_Set_REG0X02(chip, 0xCA);
        TP2825B_V2_DataSet(chip);
        tp28xx_byte_write(chip, 0x35, 0x33);
        tp28xx_byte_write(chip, 0x36, 0x39);
        tp28xx_byte_write(chip, 0x2d, 0x28);
        TP2825B_SYSCLK_V2(chip);
        break;

     case TP2802_6M20:
        tp2802_set_work_mode_6M20(chip);
        TP2825B_Set_REG0X02(chip, 0xC8);
        tp28xx_byte_write(chip, 0x35, 0x17);
        tp28xx_byte_write(chip, 0x36, 0xd0);
        if( STD_HDA == std)
        {

        }
        else if(STD_HDC == std || STD_HDC_DEFAULT == std)
        {
            TP2825B_C6MP20_DataSet(chip);
            if(STD_HDC == std)
            {

            }

        }
        TP2825B_SYSCLK_V3(chip);
        break;

    default:
        err = -1;
        break;
    }

    if(TP2802_PAL == mode || TP2802_NTSC == mode ) //960x480/960x576
    {
            tmp = tp28xx_byte_read(chip, 0x35);
            tmp |= 0x40;
            tp28xx_byte_write(chip, 0x35, tmp);
    }

/*
    if(MUX656_8BIT == output[chip] || SEP656_8BIT == output[chip])
    {
        if(TP2802_PAL == mode || TP2802_NTSC == mode ) //960x480/960x576
        {
#if (CVBS_960H)
            tp28xx_byte_write(chip, 0xfa, 0x0b);

            tmp = tp28xx_byte_read(chip, 0x35);
            tmp |= 0x40;
            tp28xx_byte_write(chip, 0x35, tmp);
#endif
        }
        else
        {

            tp28xx_byte_write(chip, 0xfa, 0x08);
        }
    }
    else //16bit output
    {
            tp28xx_byte_write(chip, 0xfa, 0x0b);
    }

*/
    return err;
}


static void tp2802_set_reg_page(unsigned char chip, unsigned char ch)
{
    switch(ch)
    {
    case MIPI_PAGE:
        tp28xx_byte_write(chip, 0x40, 0x08);
        break;
    default:
        tp28xx_byte_write(chip, 0x40, 0x00);
        tp28xx_byte_write(chip, 0x41, 0x03);
        break;
    }
}
/*
static void tp2802_manual_agc(unsigned char chip, unsigned char ch)
{
    unsigned int agc, tmp;

    tp28xx_byte_write(chip, 0x2F, 0x02);
    agc = tp28xx_byte_read(chip, 0x04);
    printk("AGC=0x%04x ch%02x\r\n", agc, ch);
    agc += tp28xx_byte_read(chip, 0x04);
    agc += tp28xx_byte_read(chip, 0x04);
    agc += tp28xx_byte_read(chip, 0x04);
    agc &= 0x3f0;
    agc >>=1;
    if(agc > 0x1ff) agc = 0x1ff;
#if (DEBUG)
    printk("AGC=0x%04x ch%02x\r\n", agc, ch);
#endif
    tp28xx_byte_write(chip, 0x08, agc&0xff);
    tmp = tp28xx_byte_read(chip, 0x06);
    tmp &=0xf9;
    tmp |=(agc>>7)&0x02;
    tmp |=0x04;
    tp28xx_byte_write(chip, 0x06,tmp);
}

*/


static void TP28xx_reset_default(int chip, unsigned char ch)
{

        TP2825B_reset_default(chip, ch);
}

////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////
static void tp2802_comm_init( int chip)
{
    unsigned int val;


    if(TP2825B == id[chip])
    {
        tp28xx_byte_write(chip, 0x40, 0x00); //default Vin1
        tp28xx_byte_write(chip, 0x35, 0x25);
        tp28xx_byte_write(chip, 0xfd, 0x80);
        tp28xx_byte_write(chip, 0xfa, 0x03);

        TP2825B_reset_default(chip, VIDEO_PAGE);

        tp2802_set_video_mode(chip, mode, VIDEO_PAGE, STD_TVI);

#if (WDT)

        tp28xx_byte_write(chip, 0x26, 0x04);
#endif

        //MUX output
        TP2825B_output(chip);

        TP2825B_RX_init(chip, PTZ_RX_TVI_CMD);

    }
    else if(TP2850 == id[chip])
    {
        tp28xx_byte_write(chip, 0x40, 0x00); //default Vin1
        tp28xx_byte_write(chip, 0x35, 0x25);
        tp28xx_byte_write(chip, 0xfd, 0x80);
        tp28xx_byte_write(chip, 0xf0, 0x10); //default PTZ1

        TP2825B_reset_default(chip, VIDEO_PAGE);

        tp2802_set_video_mode(chip, mode, VIDEO_PAGE, STD_TVI);

#if (WDT)

        tp28xx_byte_write(chip, 0x26, 0x04);
#endif

        //MUX output
        TP2850_output(chip);

        TP2825B_RX_init(chip, PTZ_RX_TVI_CMD);
    }
    else if(TP2860 == id[chip])
    {
        tp28xx_byte_write(chip, 0x40, 0x00); //default Vin1
        tp28xx_byte_write(chip, 0x42, 0x00);
        tp28xx_byte_write(chip, 0xfd, 0x80);
        tp28xx_byte_write(chip, 0xf0, 0x10); //default PTZ1
        tp28xx_byte_write(chip, 0x72, 0x10);

        TP2825B_reset_default(chip, VIDEO_PAGE);

        tp2802_set_video_mode(chip, mode, VIDEO_PAGE, STD_TVI);

#if (WDT)

        tp28xx_byte_write(chip, 0x26, 0x04);
#endif

        //MUX output
        TP2860_output(chip);

        TP2825B_RX_init(chip, PTZ_RX_TVI_CMD);
    }



}


static struct file_operations tp2802_fops =
{
    .owner      = THIS_MODULE,
    .unlocked_ioctl  = tp2802_ioctl,
    .open       = tp2802_open,
    .release    = tp2802_close
};

static struct miscdevice tp2802_dev =
{
    .minor		= MISC_DYNAMIC_MINOR,
    .name		= "tp2802dev",
    .fops  		= &tp2802_fops,
};


module_param(mode, uint, S_IRUGO);
module_param(chips, uint, S_IRUGO);
module_param_array(output, uint, &chips, S_IRUGO);

static int __init tp2802_module_init(void)
{
    int ret = 0, i = 0, val = 0;
    unsigned char chip;
    /*
    	// 1st check the module parameters
    	if ((mode < TP2802_1080P25) || (mode > TP2802_720P60))
    	{
    		printk("TP2802 module param 'mode' Invalid!\n");
    		return FAILURE;
    	}
    */
    printk("TP2825B driver version %d.%d.%d loaded\n",
           (TP2825_VERSION_CODE >> 16) & 0xff,
           (TP2825_VERSION_CODE >>  8) & 0xff,
           TP2825_VERSION_CODE & 0xff);

    if (chips <= 0 || chips > MAX_CHIPS)
    {
        printk("TP2825B module param 'chips' invalid value:%d\n", chips);
        return FAILURE;
    }

    /* register misc device*/
    ret = misc_register(&tp2802_dev);
    if (ret)
    {
        printk("ERROR: could not register TP2825B device module\n");
        return ret;
    }

    i2c_client_init();

    sema_init(&watchdog_lock, 1);

    chip = 0;
    TP2860_sensor_init(chip, SINGLE_VIN1, HD30, STD_HDA, MIPI_2LANES);

#if (WDT)
    ret = TP2802_watchdog_init();
    if (ret)
    {
        misc_deregister(&tp2802_dev);
        printk("ERROR: could not create watchdog\n");
        return ret;
    }
#endif

    printk("TP2825B Driver Init Successful!\n");

    return SUCCESS;
}

static void __exit tp2802_module_exit(void)
{
#if (WDT)
    TP2802_watchdog_exit();
#endif

    i2c_client_exit();

    misc_deregister(&tp2802_dev);
}


/////////////////////////////////////////////////////////////////
unsigned char TP2825B_read_egain(unsigned char chip)
{
    unsigned char gain;

            gain = tp28xx_byte_read(chip, 0x03);
            gain >>= 4;

        return gain;
}
/////////////////////////////////////////////////////////////////
unsigned char tp28xx_read_egain(unsigned char chip)
{
    unsigned char gain;


        tp28xx_byte_write(chip, 0x2f, 0x00);
        gain = tp28xx_byte_read(chip, 0x04);

        return gain;
}

//////////////////////////////////////////////////////////////////
/******************************************************************************
 *
 * TP2802_watchdog_deamon()

 *
 ******************************************************************************/
static int TP2802_watchdog_deamon(void *data)
{
    tp2802wd_info* wdi;
    struct sched_param param = {.sched_priority = 99};
    int interval, iChip, i = 0;
    unsigned char status, cvstd, gain, agc, tmp,flag_locked;
    unsigned char rx1,rx2;

    printk("TP2802_watchdog_deamon: start!\n");

    current->flags |= PF_NOFREEZE;
    set_current_state(TASK_INTERRUPTIBLE);

    while (watchdog_state != WATCHDOG_EXIT) {
        //printk("TP2802_watchdog_deamon: run\n");
        down(&watchdog_lock);

        for (iChip = 0; iChip < chips; iChip++) {
            //printk("TP2802_watchdog_deamon: run 2\n");
            wdi = &watchdog_info[iChip];

            //scan four inputs:
            for (i = 0; i < CHANNELS_PER_CHIP; i++) {
                if (SCAN_DISABLE == wdi->scan[i])
                    continue;

                tp2802_set_reg_page(iChip, i);
                status = tp28xx_byte_read(iChip, 0x01);
                //printk("status: 0x%x\n", status);

                //state machine for video checking
                if (status & FLAG_LOSS) {
                    //pr_err("%s %d\n", __func__, __LINE__);

                    //switch to no video
                    if (VIDEO_UNPLUG != wdi->state[i]) {
                        wdi->state[i] = VIDEO_UNPLUG;
                        wdi->count[i] = 0;
                        if (SCAN_MANUAL != wdi->scan[i])
                            wdi->mode[i] = INVALID_FORMAT;

                        #if (DEBUG)
                        printk("video loss ch%02x chip%2x\r\n", i, iChip );
                        #endif
                    }

                    //first time into no video
                    if (0 == wdi->count[i]) {
                        if (SCAN_MANUAL != wdi->scan[i]) {
                            //tp2802_set_video_mode(iChip, DEFAULT_FORMAT, i, STD_TVI);
                            TP28xx_reset_default(iChip, i);
                        }
                        wdi->count[i]++;
                    } else {
                        if(wdi->count[i] < MAX_COUNT)
                            wdi->count[i]++;

                        continue;
                    }
                } else {
                    //there is video
                    if (TP2802_PAL == wdi->mode[i] || TP2802_NTSC == wdi->mode[i] )
                        flag_locked = FLAG_HV_LOCKED;
                    else
                        flag_locked = FLAG_HV_LOCKED;

                    //video locked
                    if (flag_locked == (status & flag_locked)) {
                        if (VIDEO_LOCKED == wdi->state[i]) {
                            if(wdi->count[i] < MAX_COUNT)
                                wdi->count[i]++;
                        } else if (VIDEO_UNPLUG == wdi->state[i]) {
                            wdi->state[i] = VIDEO_IN;
                            wdi->count[i] = 0;

                            #if (DEBUG)
                            printk("1video in ch%02x chip%2x\r\n", i, iChip);
                            #endif
                        } else if (wdi->mode[i] != INVALID_FORMAT) {
                            //if( FLAG_HV_LOCKED == (FLAG_HV_LOCKED & status) )//H&V locked
                            {
                                wdi->state[i] = VIDEO_LOCKED;
                                wdi->count[i] = 0;
                                #if (DEBUG)
                                printk("video locked %02x ch%02x chip%2x\r\n", status, i, iChip);
                                #endif
                            }
                        }
                    } else {
                        //video in but unlocked
                        if(VIDEO_UNPLUG == wdi->state[i]) {
                            wdi->state[i] = VIDEO_IN;
                            wdi->count[i] = 0;
                            #if (DEBUG)
                            printk("2video in ch%02x chip%2x\r\n", i, iChip);
                            #endif
                        } else if (VIDEO_LOCKED == wdi->state[i]) {
                            wdi->state[i] = VIDEO_UNLOCK;
                            wdi->count[i] = 0;
                            #if (DEBUG)
                            printk("video unstable ch%02x chip%2x\r\n", i, iChip);
                            #endif
                        } else {
                            if (wdi->count[i] < MAX_COUNT)
                                wdi->count[i]++;

                            if (VIDEO_UNLOCK == wdi->state[i] && wdi->count[i] > 2) {
                                wdi->state[i] = VIDEO_IN;
                                wdi->count[i] = 0;

                                if (SCAN_MANUAL != wdi->scan[i])
                                    TP28xx_reset_default(iChip, i);

                                #if (DEBUG)
                                printk("video unlocked ch%02x chip%2x\r\n",i, iChip);
                                #endif
                            }
                        }
                    }

                    //manual reset for V1/2 switching
                    if (wdi->force[i]) {
                        wdi->state[i] = VIDEO_UNPLUG;
                        wdi->count[i] = 0;
                        wdi->mode[i] = INVALID_FORMAT;
                        wdi->force[i] = 0;
                        TP28xx_reset_default(iChip, i);
                        //tp2802_set_video_mode(iChip, DEFAULT_FORMAT, i);
                    }
                }

                //printk("video state %2x detected ch%02x count %4x\r\n",
                //          wdi->state[i], i, wdi->count[i] );
                if (VIDEO_IN == wdi->state[i]) {
                    if (SCAN_MANUAL != wdi->scan[i]) {
                        //tp28xx_byte_write(iChip, 0x40, 0x08);
                        //tp28xx_byte_write(iChip, 0x13, 0x24);
                        //tp28xx_byte_write(iChip, 0x40, 0x00);
                        //tp28xx_byte_write(iChip, 0x35, 0x25);
                        cvstd = tp28xx_byte_read(iChip, 0x03);
                        #if (DEBUG)
                        printk("video format %2x detected ch%02x chip%2x count%2x\r\n", cvstd, i, iChip, wdi->count[i]);
                        #endif

                        cvstd &= 0x0f;
                        wdi-> std[i] = STD_TVI;
                        if (SCAN_HDA == wdi->scan[i])
                            wdi-> std[i] = STD_HDA;
                        else if(SCAN_HDC == wdi->scan[i])
                            wdi-> std[i] = STD_HDC;

                        /*
                        if( TP2802_SD == (cvstd&0x07) )
                        {
                                if(wdi->count[i] & 0x01)
                                {
                                    wdi-> mode[i] = TP2802_PAL;
                                }
                                else
                                {
                                    wdi-> mode[i] = TP2802_NTSC;
                                }
                                tp2802_set_video_mode(iChip, wdi-> mode[i], i, wdi-> std[i]);

                        }
                        else if((cvstd&0x07) < 6 )
                        */

                        if ((cvstd & 0x07) < 6) {
                                if (TP2802_720P25 == (cvstd & 0x07)) {
                                    wdi-> mode[i] = TP2802_720P25V2;
                                } else if (TP2802_720P30 == (cvstd & 0x07)) {
                                    if (wdi->count[i] & 1)
                                        wdi-> mode[i] = TP2802_QHD15;
                                    else
                                        wdi-> mode[i] = TP2802_720P30V2;
                                } else if (TP2802_720P60 == (cvstd & 0x07)) {
                                    if (wdi->count[i] & 1)
                                        wdi-> mode[i] = TP2802_QHD30;
                                    else
                                        wdi-> mode[i] = TP2802_720P60;
                                } else if (TP2802_720P50 == (cvstd & 0x07)) {
                                    if (wdi->count[i] & 1)
                                        wdi-> mode[i] = TP2802_QHD25;
                                    else
                                        wdi-> mode[i] = TP2802_720P50;
                                } else if (TP2802_1080P30 == (cvstd & 0x07)) {
                                    if (wdi->count[i] & 1)
                                        wdi-> mode[i] = TP2802_8M15;
                                    else
                                        wdi-> mode[i] = TP2802_1080P30;
                                } else if (TP2802_1080P25 == (cvstd & 0x07)) {
                                    if (wdi->count[i] & 1)
                                        wdi-> mode[i] = TP2802_8M12;
                                    else
                                        wdi-> mode[i] = TP2802_1080P25;
                                }

                                tp2802_set_video_mode(iChip, wdi-> mode[i], i, wdi-> std[i]);
                        }
                        else {
                            //format is 7
                            tp28xx_byte_write(iChip, 0x2f, 0x09);
                            tmp = tp28xx_byte_read(iChip, 0x04);

                            #if (DEBUG)
                            printk("detection %02x  ch%02x chip%2x\r\n", tmp, i,iChip);
                            #endif

                            if (0x4e == tmp) {
                                if (SCAN_HDA == wdi->scan[i])
                                    wdi-> mode[i] = TP2802_QXGA18;
                                else if (SCAN_AUTO == wdi->scan[i] && wdi->count[i] < 3)
                                    wdi-> mode[i] = TP2802_QXGA18;
                                else
                                    wdi-> mode[i] = TP2802_3M18;

                                tp2802_set_video_mode(iChip, wdi-> mode[i], i, wdi-> std[i]);
                            } else if (0x5d == tmp) {
                                if ((wdi->count[i] % 3) == 0) {
                                    wdi-> mode[i] = TP2802_5M12;
                                    wdi-> std[i] = STD_HDA;
                                    tp2802_set_video_mode(iChip, wdi-> mode[i], i,  wdi-> std[i]);
                                } else if((wdi->count[i] % 3) == 1) {
                                    wdi-> mode[i] = TP2802_4M15;
                                    tp2802_set_video_mode(iChip, wdi-> mode[i], i, wdi-> std[i]);
                                } else {
                                    wdi-> mode[i] = TP2802_720P30HDR;
                                    tp2802_set_video_mode(iChip, wdi-> mode[i], i, wdi-> std[i]);
                                }
                            } else if (0x5c == tmp) {
                                wdi-> mode[i] = TP2802_5M12;
                                tp2802_set_video_mode(iChip, wdi-> mode[i], i, wdi-> std[i]);
                            } else if (0x75 == tmp) {
                                wdi-> mode[i] = TP2802_6M10;
                                tp2802_set_video_mode(iChip, wdi-> mode[i], i, wdi-> std[i]);
                            } else if (0x38 == tmp) {
                                //current only HDA
                                wdi-> mode[i] = TP2802_QXGA25;
                                wdi-> std[i] = STD_HDA;
                                tp2802_set_video_mode(iChip, wdi-> mode[i], i, wdi-> std[i]);
                            } else if (0x2e == tmp) {
                                //current only HDA
                                wdi-> mode[i] = TP2802_QXGA30;
                                wdi-> std[i] = STD_HDA;
                                tp2802_set_video_mode(iChip, wdi-> mode[i], i, wdi-> std[i]);
                            } else if (0x3a == tmp) {
                                if (TP2802_5M20 != wdi-> mode[i]) {
                                    wdi-> mode[i] = TP2802_5M20;
                                    tp2802_set_video_mode(iChip, wdi-> mode[i], i, wdi-> std[i]);
                                    //soft reset
                                    agc = tp28xx_byte_read(iChip, 0x06);
                                    agc |=0x80;
                                    tp28xx_byte_write(iChip, 0x06, agc);
                                }
                            } else if (0x39 == tmp) {
                                if (TP2802_6M20 != wdi-> mode[i]) {
                                    wdi-> mode[i] = TP2802_6M20;
                                    wdi-> std[i] = STD_HDC;
                                    tp2802_set_video_mode(iChip, wdi-> mode[i], i, wdi-> std[i]);
                                }
                            } else if (0x89 == tmp) {
                                wdi-> mode[i] = TP2802_1080P15;
                                tp2802_set_video_mode(iChip, wdi-> mode[i], i, wdi-> std[i]);
                            }

                            if (0x22 == tmp) {
                                wdi-> mode[i] = TP2802_1080P60;
                                tp2802_set_video_mode(iChip, wdi-> mode[i], i, wdi-> std[i]);
                            } else if (0x29 == tmp) {
                                wdi-> mode[i] = TP2802_1080P50;
                                tp2802_set_video_mode(iChip, wdi-> mode[i], i, wdi-> std[i]);
                            } else if (0x93 == tmp) {
                                wdi-> mode[i] = TP2802_NTSC;
                                tp2802_set_video_mode(iChip, wdi-> mode[i], i, wdi-> std[i]);
                            } else if (0x94 == tmp) {
                                wdi-> mode[i] = TP2802_PAL;
                                tp2802_set_video_mode(iChip, wdi-> mode[i], i, wdi-> std[i]);
                            }
                        }
                    }
                }

                #define EQ_COUNT 10
                if (VIDEO_LOCKED == wdi->state[i]) {
                    //check signal lock
                    if (0 == wdi->count[i]) {
                        tmp = tp28xx_byte_read(iChip, 0x26);
                        tmp |= 0x01;
                        tp28xx_byte_write(iChip, 0x26, tmp);

                        if ((SCAN_AUTO == wdi->scan[i] || SCAN_TVI == wdi->scan[i])) {
                            if ((TP2802_720P30V2 == wdi-> mode[i]) || (TP2802_720P25V2 == wdi-> mode[i])) {
                                tmp = tp28xx_byte_read(iChip, 0x03);

                                #if (DEBUG)
                                printk("CVSTD%02x  ch%02x chip%2x\r\n", tmp, i,iChip);
                                #endif

                                if (!(0x08 & tmp)) {
                                    #if (DEBUG)
                                    printk("720P V1 Detected ch%02x chip%2x\r\n",i,iChip);
                                    #endif
                                    wdi-> mode[i] &= 0xf7;
                                    //to speed the switching
                                    tp2802_set_video_mode(iChip, wdi-> mode[i], i, STD_TVI);
                                }
                            }

                            //these code need to keep bottom
                            {
                                tmp = tp28xx_byte_read(iChip, 0xa7);
                                tmp &= 0xfe;
                                tp28xx_byte_write(iChip, 0xa7, tmp);
                                /* tp28xx_byte_write(iChip, 0x2f, 0x0f); */
                                tp28xx_byte_write(iChip, 0x1f, 0x06);
                                tp28xx_byte_write(iChip, 0x1e, 0x60);
                            }
                        }
                    } else if (1 == wdi->count[i]) {
                        tmp = tp28xx_byte_read(iChip, 0xa7);
                        tmp |= 0x01;
                        tp28xx_byte_write(iChip, 0xa7, tmp);

                        #if (DEBUG)
                        tmp = tp28xx_byte_read(iChip, 0x01);
                        printk("status%02x  ch%02x\r\n", tmp, i);
                        tmp = tp28xx_byte_read(iChip, 0x03);
                        printk("CVSTD%02x  ch%02x\r\n", tmp, i);
                        #endif
                    } else if (wdi->count[i] < EQ_COUNT - 3) {
                        if (SCAN_AUTO == wdi->scan[i]) {
                            if ( STD_TVI == wdi-> std[i]) {
                                tmp = tp28xx_byte_read(iChip, 0x01);

                                if ((TP2802_PAL == wdi-> mode[i]) || (TP2802_NTSC == wdi-> mode[i])) {
                                    //nothing to do
                                } else if(TP2802_QXGA18 == wdi-> mode[i]) {
                                    if (0x60 == (tmp & 0x64)) {
                                        wdi-> std[i] = STD_HDA; //no CVI QXGA18
                                        tp2802_set_video_mode(iChip, wdi-> mode[i], i, wdi-> std[i]);
                                    }
                                } else if (TP2802_QHD15 == wdi-> mode[i] || TP2802_5M12 == wdi-> mode[i]) {
                                    if (0x60 == (tmp & 0x64)) {
                                        //no CVI QHD15/5M20/5M12.5
                                        wdi-> std[i] = STD_HDA;
                                        tp2802_set_video_mode(iChip, wdi-> mode[i], i, wdi-> std[i]);
                                    }
                                } else if (TP2802_QHD25 == wdi-> mode[i] ||
                                           TP2802_QHD30 == wdi-> mode[i] ||
                                           TP2802_8M12 == wdi-> mode[i] ||
                                           TP2802_8M15 == wdi-> mode[i] ||
                                           TP2802_5M20 == wdi-> mode[i]
                                        )
                                {
                                    agc = tp28xx_byte_read(iChip, 0x10);
                                    tp28xx_byte_write(iChip, 0x10, 0x00);

                                    tp28xx_byte_write(iChip, 0x2f, 0x0f);
                                    rx1 = tp28xx_byte_read(iChip, 0x04);

                                    tp28xx_byte_write(iChip, 0x10, agc);

                                    if (rx1 > 0x30)
                                        wdi-> std[i] = STD_HDA;
                                    else if(0x60 == (tmp&0x64))
                                        wdi-> std[i] = STD_HDC;

                                    #if (DEBUG)
                                    printk("RX1=%02x standard to %02x  ch%02x\r\n", rx1, wdi-> std[i], i);
                                    #endif

                                    if (STD_TVI != wdi->std[i])
                                        tp2802_set_video_mode(iChip, wdi-> mode[i], i, wdi-> std[i]);
                                    else if (TP2802_8M12 == wdi-> mode[i] || TP2802_8M15 == wdi-> mode[i])
                                        tp28xx_byte_write(iChip, 0x20, 0x50); //restore TVI clamping
                                } else if (0x60 == (tmp & 0x64)) {
                                    //capture line7 to match 3M/4M RT
                                    rx2 = tp28xx_byte_read(iChip, 0x94);

                                    if (HDC_enable) {
                                        if (0xff == rx2)
                                            wdi-> std[i] = STD_HDC;
                                        else if(0x00 == rx2)
                                            wdi-> std[i] = STD_HDC_DEFAULT;
                                        else
                                            wdi-> std[i] = STD_HDA;
                                    } else {
                                        wdi-> std[i] = STD_HDA;
                                    }

                                    if (STD_TVI != wdi->std[i]) {
                                        tp2802_set_video_mode(iChip, wdi-> mode[i], i, wdi-> std[i]);
                                        #if (DEBUG)
                                        printk("RX2=%02x standard to %02x  ch%02x\r\n", rx2, wdi-> std[i], i);
                                        #endif
                                    }
                                }
                            }
                        }
                    } else if (wdi->count[i] < EQ_COUNT) {

                    } else if (wdi->count[i] == EQ_COUNT) {
                        gain = tp28xx_read_egain(iChip);
                        if (STD_TVI != wdi-> std[i]) {
                            // manual mode
                            tp28xx_byte_write(iChip, 0x07, 0x80 | (gain >> 2));
                        }
                    }
                    else if (wdi->count[i] == EQ_COUNT + 1) {
                        if (SCAN_AUTO == wdi->scan[i]) {
                            if (HDC_enable) {
                                if (STD_HDC_DEFAULT == wdi->std[i]) {
                                    tp28xx_byte_write(iChip, 0x2f,0x0c);
                                    tmp = tp28xx_byte_read(iChip, 0x04);
                                    status = tp28xx_byte_read(iChip, 0x01);

                                    //if(0x10 == (0x11 & status) && (tmp < 0x18 || tmp > 0xf0))
                                    //if((tmp < 0x18 || tmp > 0xf0))
                                    if (0x10 == (0x11 & status)) {
                                        wdi-> std[i] = STD_HDC;
                                    } else {
                                        wdi-> std[i] = STD_HDA;
                                    }

                                    tp2802_set_video_mode(iChip, wdi-> mode[i], i, wdi-> std[i]);

                                    #if (DEBUG)
                                    printk("reg01=%02x reg04@2f=0c %02x std%02x ch%02x\r\n", status, tmp, wdi-> std[i], i );
                                    #endif
                                }
                            }
                        }
                    } else {
                        if (SCAN_AUTO == wdi->scan[i]) {
                                /*
                                if(wdi-> mode[i] < TP2802_3M18)
                                {
                                            tmp = tp28xx_byte_read(iChip, 0x03); //
                                            tmp &= 0x07;
                                            if(tmp != (wdi-> mode[i]&0x07) && tmp < TP2802_SD)
                                            {
                                            #if (DEBUG)
                                            printk("correct %02x from %02x ch%02x\r\n", tmp, wdi-> mode[i], i );
                                            #endif
                                                wdi->force[i] = 1;
                                            }
                                }
                                */
                        }
                    }
                }
            }
        }

        //printk("WDT elapsed time %d.%dms\n", interval/1000, interval%1000);
        up(&watchdog_lock);

        /* sleep 0.5 seconds */
        schedule_timeout_interruptible(msecs_to_jiffies(1000)+1);
    }

    set_current_state(TASK_RUNNING);
    printk("TP2802_watchdog_deamon: exit!\n");

    return 0;

}


/******************************************************************************
 *
 * TP2825_watchdog_init()

 *
 ******************************************************************************/
int __init TP2802_watchdog_init(void)
{
    struct task_struct *p_dog;
    int i, j;

    watchdog_state = WATCHDOG_RUNNING;
    memset(&watchdog_info, 0, sizeof(watchdog_info));

    for (i = 0; i < MAX_CHIPS; i++) {
        watchdog_info[i].addr = tp2802_i2c_addr[i];
        for(j = 0; j < CHANNELS_PER_CHIP; j++) {
            watchdog_info[i].count[j] = 0;
            watchdog_info[i].force[j] = 0;
            watchdog_info[i].mode[j] = INVALID_FORMAT;
            watchdog_info[i].scan[j] = SCAN_AUTO;
            watchdog_info[i].state[j] = VIDEO_UNPLUG;
            watchdog_info[i].std[j] = STD_TVI;
        }
    }

    p_dog = kthread_create(TP2802_watchdog_deamon, NULL, "WatchDog");
    if (IS_ERR(p_dog)) {
        printk("TP2802_watchdog_init: create watchdog_deamon failed!\n");
        return -1;
    }

    wake_up_process(p_dog);
    task_watchdog_deamon = p_dog;
    printk("TP2802_watchdog_init: done!\n");

    return 0;
}

/******************************************************************************
 *
 * TP2825_watchdog_exit()
 *
 ******************************************************************************/
void __exit TP2802_watchdog_exit(void)
{
    struct task_struct *p_dog;

    p_dog = task_watchdog_deamon;
    if (p_dog == NULL )
        return;

    watchdog_state = WATCHDOG_EXIT;
    wake_up_process(p_dog);
    kthread_stop(p_dog);
    yield();
    task_watchdog_deamon = NULL;

    printk("TP2802_watchdog_exit: done!\n");
}

module_init(tp2802_module_init);
module_exit(tp2802_module_exit);

