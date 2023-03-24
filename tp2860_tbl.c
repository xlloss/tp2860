extern void tp28xx_byte_write(unsigned char chip, unsigned char reg_addr, unsigned char value);
extern unsigned char tp28xx_byte_read(unsigned char chip, unsigned char reg_addr);


void fmt_pal_720h(unsigned char chip)
{
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
}

void fmt_pal_960h(unsigned char chip)
{
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
}

void fmt_ntsc_960h(unsigned char chip)
{
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
}

void fmt_ntsc_720h(unsigned char chip)
{
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
}

void fmt_tvi_hd_30(unsigned char chip)
{
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
    tp28xx_byte_write(chip, 0x27, 0x2d);
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
}

void fmt_tvi_hd_25(unsigned char chip)
{
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
    tp28xx_byte_write(chip, 0x27, 0x2d);
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
}

void fmt_ahd_hd_30(unsigned char chip)
{
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

void fmt_ahd_hd_25(unsigned char chip)
{
    /* initial fmt_tvi_hd_25 first */
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

void fmt_tvi_fhd_30(unsigned char chip)
{
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
    tp28xx_byte_write(chip, 0x27, 0x2d);
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
}

void fmt_tvi_fhd_25(unsigned char chip)
{
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
    tp28xx_byte_write(chip, 0x27, 0x2d);
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
}

void fmt_ahd_fhd_30(unsigned char chip)
{
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

void fmt_ahd_fhd_25(unsigned char chip)
{
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
