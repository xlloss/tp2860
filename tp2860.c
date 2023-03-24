// SPDX-License-Identifier: GPL-2.0-or-later
/*
 *  tp2825b.c - TP2860 – 1-channel HD-TVI / CVBS / HD
 *
 *  Analog Video Decoder with MIPI-CSI2 and Bi-directional Data Back Channel
 *
 *  Slash.Huang (slash.huang@regulus.com.tw)
 */
#include <linux/kernel.h>
#include <linux/version.h>
#include <linux/module.h>
#include <linux/types.h>
#include <linux/errno.h>
#include <linux/fs.h>
#include <linux/slab.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/delay.h>
#include <linux/kthread.h>
#include <linux/sched.h>
#include <linux/i2c.h>
#include "tp2860_tbl.c"

#define TP2860_VERSION_CODE "1.0.0"

#define VIDEO_INPUT_STATUS      0X01
#define DETECTION_STATUS        0X03
#define CVSTD_MASK              0x07
#define SYWD (1 << 3)
#define SYWD_TVI_V10            0
#define SYWD_TVI_V20            1

#define EQ2_REFERENCE           0x09
#define EQ2_CTRL                0x07
#define BRIGHTNESS_CTL          0x10
#define PCLAMP_CTL              0x20
#define INTERNAL_STATUS         0x04
#define CLAMPING_CTRL           0x26
#define CLAMPING_CTRL_NORMAL    (1 << 0)
#define CLAMPING_CTRL_DETECT    (0 << 0)
#define PSP (1 << 1)
#define GTST(v) (v << 2)

#define TEST_CTL 0x2F

#define RX_CTL 0xA7
#define RXEN (1 << 0)

#define RESET_CTL           0x06
#define TXDATA2_CTL         0x60
#define RXDATA2_CTL         0x94
#define FLAG_LOSS           0x80
#define FLAG_H_LOCKED       0x20
#define FLAG_HV_LOCKED      0x60

#define FLAG_HDC_MODE       0x80
#define FLAG_HALF_MODE      0x40
#define FLAG_MEGA_MODE      0x20
#define FLAG_HDA_MODE       0x10

#define PAGE_CTRL           0x40
#define ENABLE_MIPI_PAGE    (1 << 3)
#define ENABLE_VIDEO_PAGE   (0 << 0)

#define AFE_AIN_CTRL        0x41
#define VIN(ch)             (ch << 0)

#define CHANNELS_PER_CHIP   1
#define MAX_CHIPS           1
#define SUCCESS             0
#define FAILURE             -1

#define BRIGHTNESS          0x10
#define CONTRAST            0x11
#define SATURATION          0x12
#define HUE                 0X13
#define SHARPNESS           0X14

#define MAX_COUNT           0xFFFF

#define PAL_NTSC_960H       1
#define PAL_NTSC_720H       0

#define I2C_INDEX_0         3
#define DEBUG               1
#define TP2825B_I2C_A       0x44
#define TP2825B_I2C_B       0x45
#define WATCHDOG_EXIT       0
#define WATCHDOG_RUNNING    1
#define WDT                 1

typedef struct
{
    unsigned int  count[CHANNELS_PER_CHIP];
    unsigned int  mode[CHANNELS_PER_CHIP];
    unsigned int  scan[CHANNELS_PER_CHIP];
    unsigned int  gain[CHANNELS_PER_CHIP][4];
    unsigned int  std[CHANNELS_PER_CHIP];
    unsigned int  state[CHANNELS_PER_CHIP];
    unsigned int  force[CHANNELS_PER_CHIP];
    unsigned char addr;
} tp2860wd_info;

enum {
    TP2825B = 0x2825,
    TP2850  = 0x2850,
    TP2860  = 0x2860,
};

enum {
    TP2802_1080P25      =   0x03,
    TP2802_1080P30      =   0x02,
    TP2802_720P25       =   0x05,
    TP2802_720P30       =   0x04,
    TP2802_720P50       =   0x01,
    TP2802_720P60       =   0x00,
    TP2802_SD           =   0x06,
    INVALID_FORMAT      =   0x07,
    TP2802_720P25V2     =   0x0D,
    TP2802_720P30V2     =   0x0C,
    TP2802_PAL          =   0x08,
    TP2802_NTSC         =   0x09,
    TP2802_3M18         =   0x20,   //2048x1536@18.75 for TVI
    TP2802_5M12         =   0x21,   //2592x1944@12.5 for TVI
    TP2802_4M15         =   0x22,   //2688x1520@15 for TVI
    TP2802_3M20         =   0x23,   //2048x1536@20 for TVI
    TP2802_4M12         =   0x24,   //2688x1520@12.5 for TVI
    TP2802_6M10         =   0x25,   //3200x1800@10 for TVI
    TP2802_QHD30        =   0x26,   //2560x1440@30 for TVI/HDA/HDC
    TP2802_QHD25        =   0x27,   //2560x1440@25 for TVI/HDA/HDC
    TP2802_QHD15        =   0x28,   //2560x1440@15 for HDA
    TP2802_QXGA18       =   0x29,   //2048x1536@18 for HDA/TVI
    TP2802_QXGA30       =   0x2A,   //2048x1536@30 for HDA
    TP2802_QXGA25       =   0x2B,   //2048x1536@25 for HDA
    TP2802_4M30         =   0x2C,   //2688x1520@30 for TVI(for future)
    TP2802_4M25         =   0x2D,   //2688x1520@25 for TVI(for future)
    TP2802_5M20         =   0x2E,   //2592x1944@20 for TVI/HDA
    TP2802_8M15         =   0x2f,   //3840x2160@15 for TVI
    TP2802_8M12         =   0x30,   //3840x2160@12.5 for TVI
    TP2802_1080P15      =   0x31,   //1920x1080@15 for TVI
    TP2802_1080P60      =   0x32,   //1920x1080@60 for TVI
    TP2802_960P30       =   0x33,   //1280x960@30 for TVI
    TP2802_1080P20      =   0x34,   //1920x1080@20 for TVI
    TP2802_1080P50      =   0x35,   //1920x1080@50 for TVI
    TP2802_720P14       =   0x36,   //1280x720@14 for TVI
    TP2802_720P30HDR    =   0x37,   //1280x720@30 for TVI
    TP2802_6M20         =   0x38,   //2960x1920@20 for CVI
    TP2802_8M15V2       =   0x39,   //3264x2448@15 for TVI
};

enum {
    VIDEO_UNPLUG = 0,
    VIDEO_IN,
    VIDEO_LOCKED,
    VIDEO_UNLOCK
};

enum {
    MUX656_8BIT = 0,    //Y/C-mux 4:2:2 8-bit with embedded sync
    SEP656_8BIT,        //Y/C-mux 4:2:2 8-bit with separate sync
    EMB422_16BIT,       //only TP2825B YCbCr 4:2:2 16-bit with embedded sync
    SEP422_16BIT,       //only TP2825B YCbCr 4:2:2 16-bit with separate sync
    MIPI_2LANES,        //only TP2850
    MIPI_1LANE,
};

enum {
    VIDEO_PAGE = 0,
    MIPI_PAGE = 8
};

enum {
    SCAN_DISABLE = 0,
    SCAN_AUTO,
    SCAN_TVI,
    SCAN_HDA,
    SCAN_HDC,
    SCAN_MANUAL,
    SCAN_TEST
};

enum {
    STD_TVI,
    STD_HDA,
};

enum {
    PTZ_TVI = 0,
    PTZ_HDA_1080P,
    PTZ_HDA_720P,
    PTZ_HDA_CVBS,
    PTZ_HDC,
    PTZ_HDA_3M18,   //HDA QXGA18
    PTZ_HDA_3M25,   //HDA QXGA25,QXGA30
    PTZ_HDA_4M25,   //HDA QHD25,QHD30,5M20
    PTZ_HDA_4M15,   //HDA QHD15,5M12.5
    PTZ_HDC_QHD,    //HDC QHD25,QHD30
    PTZ_HDC_FIFO,   //HDC 1080p,720p FIFO
    PTZ_HDC_8M12,   //HDC 8M12.5
    PTZ_HDC_8M15,   //HDC 8M15
    PTZ_HDC_6M20    //HDC 6M20
};

enum {
    PTZ_RX_TVI_CMD = 0,
    PTZ_RX_TVI_BURST,
    PTZ_RX_ACP1,
    PTZ_RX_ACP2,
    PTZ_RX_ACP3,
    PTZ_RX_TEST,
    PTZ_RX_HDC1,
    PTZ_RX_HDC2
};

enum {
    /* only for TP2825B */
    PTZ_PIN_TXD = 0,
    PTZ_PIN_PTZ1,
    PTZ_PIN_PTZ2,
};

enum {
    SINGLE_VIN1 = 0,
    SINGLE_VIN2,
    SINGLE_VIN3,
    SINGLE_VIN4,
    SINGLE_NA1,
    SINGLE_NA2,
    DIFF_VIN12,
    DIFF_VIN34,
    SINGLE_NA,
};

enum {
    /*0*/ PAL = 0,
    /*1*/ NTSC,
    /*2*/ HD25,
    /*3*/ HD30,
    /*4*/ FHD25,
    /*5*/ FHD30,
    /*6*/ HD50,
    /*7*/ HD60,

    /* only support with 2lane mode */
    /*8*/ QHD25,
    /*9*/ QHD30,
    /*10*/FHD50,
    /*11*/FHD60,
};

void tp28xx_byte_write(unsigned char chip, unsigned char reg_addr, unsigned char value);
unsigned char tp28xx_byte_read(unsigned char chip, unsigned char reg_addr);

struct i2c_client* tp28xx_client0;
int chips = 1;
int  tp2860_watchdog_init(void);
void tp2860_watchdog_exit(void);
unsigned char tp2860_i2c_addr[] = {TP2825B_I2C_B};

struct i2c_board_info tp2860_info =
{
    I2C_BOARD_INFO("TP2850", 0x45),
};

static tp2860wd_info watchdog_info[MAX_CHIPS];
volatile static unsigned int watchdog_state = 0;
struct task_struct *task_watchdog_deamon = NULL;
struct semaphore watchdog_lock;

void tp28xx_byte_write(unsigned char chip, unsigned char reg_addr,
                       unsigned char value)
{
    unsigned char buf[2];
    struct i2c_client* client = tp28xx_client0;
    unsigned char chip_addr;

    chip_addr = tp2860_i2c_addr[chip];
    client->addr = chip_addr;
    buf[0] = reg_addr;
    buf[1] = value;
    i2c_master_send(client, buf, 2);
    udelay(100);
}

unsigned char tp28xx_byte_read(unsigned char chip, unsigned char reg_addr)
{
    unsigned char         ret_data = 0xFF;
    int                   ret;
    struct i2c_client*    client = tp28xx_client0;
    static struct i2c_msg msg[2];
    unsigned char         buffer[2];
    unsigned int          data_width = 1;

    client->addr = tp2860_i2c_addr[chip] ;
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
        printk("[%s %d] addr 0x%x i2c_transfer error",
            __FUNCTION__, __LINE__, client->addr);
        return 0xff;
    }

    memcpy(&ret_data, buffer, data_width);

    return ret_data;

}

static int i2c_client_init(void)
{
    struct i2c_adapter* i2c_adap;

    i2c_adap = i2c_get_adapter(I2C_INDEX_0);
    tp28xx_client0 = i2c_new_client_device(i2c_adap, &tp2860_info);
    i2c_put_adapter(i2c_adap);

    return 0;
}

static void i2c_client_exit(void)
{
    i2c_unregister_device(tp28xx_client0);
}

void TP2860_mipi_out(unsigned char chip, unsigned char fmt,
                    unsigned char std, unsigned char lane)
{
    char tmp;

    /* mipi page */
    tp28xx_byte_write(chip, PAGE_CTRL, ENABLE_MIPI_PAGE);
    tp28xx_byte_write(chip, 0x02, 0x79);
    tp28xx_byte_write(chip, 0x03, 0x71);
    tp28xx_byte_write(chip, 0x04, 0x71);
    tp28xx_byte_write(chip, 0x13, 0xef);
    tp28xx_byte_write(chip, 0x20, 0x00);
    tp28xx_byte_write(chip, 0x23, 0x9e);

    if (MIPI_1LANE == lane) {
        tp28xx_byte_write(chip, 0x21, 0x11);

        switch (fmt) {
            case FHD30:
            case FHD25:
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
                break;

            case HD30:
            case HD25:
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
                break;

            case PAL:
            case NTSC:
                tp28xx_byte_write(chip, 0x14, 0x51);
                tp28xx_byte_write(chip, 0x15, 0x07);
                tp28xx_byte_write(chip, 0x2a, 0x02);
                tp28xx_byte_write(chip, 0x2b, 0x01);
                tp28xx_byte_write(chip, 0x2c, 0x05);
                tp28xx_byte_write(chip, 0x2e, 0x02);
                break;

            case HD60:
            case HD50:
                tp28xx_byte_write(chip, 0x14, 0x00);
                tp28xx_byte_write(chip, 0x15, 0x02);
                tp28xx_byte_write(chip, 0x2a, 0x08);
                tp28xx_byte_write(chip, 0x2b, 0x06);
                tp28xx_byte_write(chip, 0x2c, 0x11);
                tp28xx_byte_write(chip, 0x2e, 0x0a);
                break;

            default:
                pr_err("FMT(%d) not support", fmt);
                break;
        }
    } else {
        /* 2lane */
        tp28xx_byte_write(chip, 0x21, 0x12);

        switch (fmt) {
            case FHD30:
            case FHD25:
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
                break;

            case HD30:
            case HD25:
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
                break;

            case PAL:
            case NTSC:
                tp28xx_byte_write(chip, 0x14, 0x62);
                tp28xx_byte_write(chip, 0x15, 0x07);
                tp28xx_byte_write(chip, 0x2a, 0x02);
                tp28xx_byte_write(chip, 0x2b, 0x00);
                tp28xx_byte_write(chip, 0x2c, 0x03);
                tp28xx_byte_write(chip, 0x2e, 0x02);
                break;

            case HD60:
            case HD50:
                tp28xx_byte_write(chip, 0x14, 0x41);
                tp28xx_byte_write(chip, 0x15, 0x02);
                tp28xx_byte_write(chip, 0x2a, 0x04);
                tp28xx_byte_write(chip, 0x2b, 0x03);
                tp28xx_byte_write(chip, 0x2c, 0x09);
                tp28xx_byte_write(chip, 0x2e, 0x02);
                break;
            default:
                pr_err("FMT(%d) not support", fmt);
                break;
        }
    }

    /* PLL reset */
    tmp = tp28xx_byte_read(chip, 0x14);
    tp28xx_byte_write(chip, 0x14, 0x80 | tmp);
    tp28xx_byte_write(chip, 0x14, tmp);

    /* Enable MIPI CSI2 output */
    tp28xx_byte_write(chip, 0x28, 0x02);  //stream off
    tp28xx_byte_write(chip, 0x28, 0x00);  //stream on
    tp28xx_byte_write(chip, PAGE_CTRL, ENABLE_VIDEO_PAGE);
}

void TP2860_sensor_init(unsigned char chip, unsigned char fmt,
                        unsigned char std, unsigned char lane)
{
    tp28xx_byte_write(chip, PAGE_CTRL, ENABLE_VIDEO_PAGE);
    tp28xx_byte_write(chip, 0x06, 0x12);
    tp28xx_byte_write(chip, 0x42, 0x00);
    tp28xx_byte_write(chip, 0x4e, 0x00);
    tp28xx_byte_write(chip, 0x54, 0x00);

    /* video MUX select */
    tp28xx_byte_write(chip, AFE_AIN_CTRL, VIN(SINGLE_VIN4));

    switch (fmt) {
        case PAL:
            #if PAL_NTSC_960H
                fmt_pal_960h(chip);
            #else
                fmt_pal_720h(chip);
            #endif
            break;

        case NTSC:
            #if PAL_NTSC_960H
                fmt_ntsc_960h(chip);
            #else
                fmt_ntsc_720h
            #endif
            break;

        case HD25:
            fmt_tvi_hd_25(chip);
            if (std == STD_HDA)
                fmt_ahd_hd_25(chip);
            break;

        case HD30:
            fmt_tvi_hd_30(chip);
            if (std == STD_HDA)
                fmt_ahd_hd_30(chip);
            break;

        case FHD25:
            fmt_tvi_fhd_25(chip);
            if (std == STD_HDA)
                fmt_ahd_fhd_25(chip);
            break;

        case FHD30:
            fmt_tvi_fhd_30(chip);
            if (std == STD_HDA)
                fmt_ahd_fhd_30(chip);
            break;

        default:
            pr_err("not support format\n");
        break;
    }

    TP2860_mipi_out(chip, fmt, std, lane);
}

static void tp2860_set_reg_page(unsigned char chip, unsigned char page)
{
    switch (page) {
        case MIPI_PAGE:
            tp28xx_byte_write(chip, PAGE_CTRL, ENABLE_MIPI_PAGE);
            break;

        case VIDEO_PAGE:
        default:
            tp28xx_byte_write(chip, PAGE_CTRL, ENABLE_VIDEO_PAGE);
            break;
    }
}

static int __init tp2860_module_init(void)
{
    unsigned char chip;
    int ret;

    pr_info("TP2860 driver version %s\n", TP2860_VERSION_CODE);
    if (chips <= 0 || chips > MAX_CHIPS) {
        printk("TP2825B module param 'chips' invalid value:%d\n", chips);
        return FAILURE;
    }

    i2c_client_init();
    sema_init(&watchdog_lock, 1);

    chip = 0;
    TP2860_sensor_init(chip, HD30, STD_HDA, MIPI_2LANES);

#if WDT
    ret = tp2860_watchdog_init();
    if (ret) {
        printk("ERROR: could not create watchdog\n");
        return ret;
    }
#endif

    printk("TP2825B Driver Init Successful!\n");

    return SUCCESS;
}

static void __exit tp2860_module_exit(void)
{
#if WDT
    tp2860_watchdog_exit();
#endif
    i2c_client_exit();
}

static int tp2860_watchdog_deamon(void *data)
{
    #define STD_CONT 2
    #define FMT_CONT 6

    tp2860wd_info* wdi;
    unsigned char input_status;
    unsigned char sear_std_tab[STD_CONT] = {STD_HDA, STD_TVI};
    unsigned char sear_fmt_tab[FMT_CONT] = {PAL, NTSC, HD30, HD25, FHD30, FHD25};
    unsigned char std_tab_idx = 0, fmt_tab_idx = 1;
    int chip = 0, i = 0;
    static unsigned long int print_jiffies;

    pr_info("tp2860_watchdog_deamon: start\n");

    current->flags |= PF_NOFREEZE;
    set_current_state(TASK_INTERRUPTIBLE);
    wdi = &watchdog_info[chip];

    while (watchdog_state != WATCHDOG_EXIT) {
        down(&watchdog_lock);

        /* scan inputs status : */
        tp2860_set_reg_page(chip, VIDEO_PAGE);
        input_status = tp28xx_byte_read(chip, VIDEO_INPUT_STATUS);

        /* **********************************
         * state machine for video checking *
         * *********************************/
        if (input_status & FLAG_LOSS) {
            wdi->state[i] = VIDEO_UNPLUG;
            wdi->count[i] = 0;
            #if DEBUG
            if (printk_timed_ratelimit(&print_jiffies, 1000))
                pr_info("STD(%d) FMT(%d) VIDEO_LOSS\n",
                    wdi->std[i], wdi->mode[i]);
            #endif

            if (wdi->count[i] < MAX_COUNT)
                wdi->count[i]++;

            goto search_next_video_fmt;
        }

        /* get video in video locked */
        if (input_status & FLAG_HV_LOCKED) {
            switch (wdi->state[i]) {
                case VIDEO_LOCKED:
                    if (wdi->count[i] < MAX_COUNT)
                        wdi->count[i]++;

                    if (printk_timed_ratelimit(&print_jiffies, 1000))
                        pr_info("STD(%d) FMT(%d) VIDEO_LOCKED\n",
                            wdi->std[i], wdi->mode[i]);
                    up(&watchdog_lock);
                    continue;
                    break;

                case VIDEO_UNPLUG:
                    wdi->state[i] = VIDEO_IN;
                    wdi->count[i] = 0;
                    #if DEBUG
                    if (printk_timed_ratelimit(&print_jiffies, 500))
                        pr_info("STD(%d) FMT(%d) VIDEO_UNPLUG -> VIDEO_IN\n",
                            wdi->std[i], wdi->mode[i]);
                    #endif
                    break;

                default:
                    /* VIDEO_IN */
                    if (wdi->mode[i] != INVALID_FORMAT) {
                        wdi->state[i] = VIDEO_LOCKED;
                        wdi->count[i] = 0;
                        #if DEBUG
                        if (printk_timed_ratelimit(&print_jiffies, 1000))
                            pr_info("STD(%d) FMT(%d) VIDEO_IN -> VIDEO_LOCKED\n",
                                wdi->std[i], wdi->mode[i]);
                        #endif
                    }
                    break;
            }
        } else {
            /* video in but unlocked */
            switch (wdi->state[i]) {
                case VIDEO_UNPLUG:
                    wdi->state[i] = VIDEO_IN;
                    wdi->count[i] = 0;
                    #if DEBUG
                    if (printk_timed_ratelimit(&print_jiffies, 50))
                        pr_info("STD(%d) FMT(%d) VIDEO_UNPLUG -> VIDEO_IN\n",
                            wdi->std[i], wdi->mode[i]);
                    #endif
                    break;
                case VIDEO_LOCKED:
                    wdi->state[i] = VIDEO_UNLOCK;
                    wdi->count[i] = 0;
                    #if DEBUG
                    if (printk_timed_ratelimit(&print_jiffies, 100))
                        pr_info("STD(%d) FMT(%d) VIDEO_LOCKED -> VIDEO_UNLOCK\n",
                            wdi->std[i], wdi->mode[i]);
                    #endif
                    break;
                default:
                    /* VIDEO_IN */
                    if (wdi->count[i] < MAX_COUNT)
                        wdi->count[i]++;

                    /* VIDEO_UNLOCK */
                    if (VIDEO_UNLOCK == wdi->state[i]) {
                        wdi->state[i] = VIDEO_IN;
                        wdi->count[i] = 0;
                        #if DEBUG
                        if (printk_timed_ratelimit(&print_jiffies, 50))
                            pr_info("STD(%d) FMT(%d) VIDEO_UNLOCK -> VIDEO_IN\n",
                                wdi->std[i], wdi->mode[i]);
                        #endif
                    }
                    break;
            }
        }

search_next_video_fmt:
        if (wdi->count[i] < 50 && wdi->state[i] != VIDEO_UNLOCK) {
            up(&watchdog_lock);
            schedule_timeout_interruptible(msecs_to_jiffies(20));
            continue;
        }

        /*  next camera format init */
        std_tab_idx++;
        if (std_tab_idx >= STD_CONT)
            std_tab_idx = 0;

        fmt_tab_idx++;
        if (fmt_tab_idx >= FMT_CONT)
        fmt_tab_idx = 0;

        pr_info("TP2860_sensor_init fmt(%d) std(%d)\n",
            sear_fmt_tab[fmt_tab_idx],
            sear_std_tab[std_tab_idx]);

        TP2860_sensor_init(chip, sear_fmt_tab[fmt_tab_idx],
                sear_std_tab[std_tab_idx], MIPI_2LANES);

        wdi->std[i] = sear_std_tab[std_tab_idx];
        wdi->mode[i] = sear_fmt_tab[fmt_tab_idx];
        wdi->count[i] = 0;

        up(&watchdog_lock);
        schedule_timeout_interruptible(msecs_to_jiffies(500));
    };

    set_current_state(TASK_RUNNING);
    pr_info("tp2860_watchdog_deamon: exit\n");

    return 0;

}

int __init tp2860_watchdog_init(void)
{
    struct task_struct *p_dog;
    int i, j;

    watchdog_state = WATCHDOG_RUNNING;
    memset(&watchdog_info, 0, sizeof(watchdog_info));

    for (i = 0; i < MAX_CHIPS; i++) {
        watchdog_info[i].addr = tp2860_i2c_addr[i];
        for(j = 0; j < CHANNELS_PER_CHIP; j++) {
            watchdog_info[i].count[j] = 0;
            watchdog_info[i].force[j] = 0;
            watchdog_info[i].mode[j] = HD30;
            watchdog_info[i].scan[j] = SCAN_AUTO;
            watchdog_info[i].state[j] = VIDEO_UNPLUG;
            watchdog_info[i].std[j] = STD_HDA;
        }
    }

    p_dog = kthread_create(tp2860_watchdog_deamon, NULL, "WatchDog");
    if (IS_ERR(p_dog)) {
        printk("tp2860_watchdog_init: create watchdog_deamon failed!\n");
        return -1;
    }

    wake_up_process(p_dog);
    task_watchdog_deamon = p_dog;
    printk("tp2860_watchdog_init: done!\n");

    return 0;
}

void __exit tp2860_watchdog_exit(void)
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

    printk("tp2860_watchdog_exit: done!\n");
}

module_init(tp2860_module_init);
module_exit(tp2860_module_exit);
MODULE_AUTHOR("Slash Huang <slash.huang@regulus.com.tw>");
MODULE_DESCRIPTION("TechPoint TP2860 Linux Module");
MODULE_LICENSE("GPL");
