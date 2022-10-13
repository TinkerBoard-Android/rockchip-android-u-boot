// SPDX-License-Identifier: GPL-2.0+
/*
 * (C) Copyright 2008-2018 Fuzhou Rockchip Electronics Co., Ltd
 */

#include <common.h>
#include <i2c.h>
#include <errno.h>
#include <dm.h>
#include <dm/uclass.h>
#include <dm/uclass-id.h>

#include "lt9211.h"
#include <asm/gpio.h>

#define DRIVER_NAME "lt9211"

static struct lt9211_data *g_lt9211 = NULL;
static bool connect_lt9211 = false;

void lt9211_i2c_reg_write(struct udevice *dev, uint offset, uint value)
{
	#define LT9211_I2C_WRITE_RETRY_COUNT (6)
	int ret = 0;
	int i = 0;

	do {
		ret = dm_i2c_reg_write(dev, offset, value);
		if (ret < 0) {
			printf("lt9211_i2c_reg_write, reg = %x value = %x  i = %d ret = %d\n", offset, value, i, ret);
			mdelay(20);
		}
	} while ((++i <= LT9211_I2C_WRITE_RETRY_COUNT) && (ret < 0));
}

int lt9211_i2c_reg_read(struct udevice *dev, uint offset)
{
	#define LT9211_I2C_READ_RETRY_COUNT (3)
	int ret = 0;
	int i = 0;

	do {
		ret = dm_i2c_reg_read(dev, offset);
		if (ret < 0) {
			printf("lt9211_i2c_reg_read fail, i = %d  reg = %x ret = %d\n", i, offset, ret);
			mdelay(20);
		} else
			return ret;
	} while ((++i <= LT9211_I2C_READ_RETRY_COUNT) && (ret < 0));

	return ret;
}

void lt9211_chipid(struct lt9211_data *lt9211)
{
	uint8_t getid[3] = {0};
	int i;
	lt9211_i2c_reg_write(lt9211->lt9211_i2c_dev, 0xFF, 0x81);//register bank
		for (i = 0; i < sizeof(getid) /sizeof(uint8_t); i++) {
		getid[i] = lt9211_i2c_reg_read(lt9211->lt9211_i2c_dev, i);
	}
}

void lt9211_systemint(struct lt9211_data *lt9211)
{
	/* system clock init */		   
	lt9211_i2c_reg_write(lt9211->lt9211_i2c_dev, 0xff, 0x82);
	lt9211_i2c_reg_write(lt9211->lt9211_i2c_dev, 0x01, 0x18);
	
	lt9211_i2c_reg_write(lt9211->lt9211_i2c_dev, 0xff, 0x86);
	lt9211_i2c_reg_write(lt9211->lt9211_i2c_dev, 0x06, 0x61); 	
	lt9211_i2c_reg_write(lt9211->lt9211_i2c_dev, 0x07, 0xa8); //fm for sys_clk
	  
	lt9211_i2c_reg_write(lt9211->lt9211_i2c_dev, 0xff, 0x87); //Init txpll regiseter table default value is incorrect
	lt9211_i2c_reg_write(lt9211->lt9211_i2c_dev, 0x14, 0x08); //default value
	lt9211_i2c_reg_write(lt9211->lt9211_i2c_dev, 0x15, 0x00); //default value
	lt9211_i2c_reg_write(lt9211->lt9211_i2c_dev, 0x18, 0x0f);
	lt9211_i2c_reg_write(lt9211->lt9211_i2c_dev, 0x22, 0x08); //default value
	lt9211_i2c_reg_write(lt9211->lt9211_i2c_dev, 0x23, 0x00); //default value
	lt9211_i2c_reg_write(lt9211->lt9211_i2c_dev, 0x26, 0x0f); 
}

void lt9211_mipirxphy(struct lt9211_data *lt9211)
{   
	lt9211_i2c_reg_write(lt9211->lt9211_i2c_dev, 0xff, 0x85);
	lt9211_i2c_reg_write(lt9211->lt9211_i2c_dev, 0x88, 0x50);
	lt9211_i2c_reg_write(lt9211->lt9211_i2c_dev, 0xff, 0xd0);
	/* Set Mipi Lanes */
	if(lt9211->lvds_output & OUTPUT_MIPI_1_LANE) {
		lt9211_i2c_reg_write(lt9211->lt9211_i2c_dev, 0x00, MIPI_1_LANE); // 1 : 1 Lane
	} else if(lt9211->lvds_output & OUTPUT_MIPI_2_LANE) {
		lt9211_i2c_reg_write(lt9211->lt9211_i2c_dev, 0x00, MIPI_2_LANE); // 2 : 2 Lane
	} else if(lt9211->lvds_output & OUTPUT_MIPI_3_LANE) {
		lt9211_i2c_reg_write(lt9211->lt9211_i2c_dev, 0x00, MIPI_3_LANE); // 3 : 3 Lane
	} else {
		lt9211_i2c_reg_write(lt9211->lt9211_i2c_dev, 0x00, MIPI_4_LANE); // 0 : 4 Lane 
	}
	/* Mipi rx phy */
	lt9211_i2c_reg_write(lt9211->lt9211_i2c_dev, 0xff, 0x82);
	lt9211_i2c_reg_write(lt9211->lt9211_i2c_dev, 0x02, 0x44); //port A mipi rx enable
	/*port A/B input 8205/0a bit6_4:EQ current setting*/
	lt9211_i2c_reg_write(lt9211->lt9211_i2c_dev, 0x05, 0x36); //port A CK lane swap  0x32--0x36 for WYZN Glassbit2- Port A mipi/lvds rx s2p input clk select: 1 = From outer path.
	lt9211_i2c_reg_write(lt9211->lt9211_i2c_dev, 0x0d, 0x26); //bit6_4:Port B Mipi/lvds rx abs refer current  0x26 0x76
	lt9211_i2c_reg_write(lt9211->lt9211_i2c_dev, 0x17, 0x0c);
	lt9211_i2c_reg_write(lt9211->lt9211_i2c_dev, 0x1d, 0x0c);

	lt9211_i2c_reg_write(lt9211->lt9211_i2c_dev, 0x0a, 0x81);//eq control for LIEXIN  horizon line display issue 0xf7->0x80
	lt9211_i2c_reg_write(lt9211->lt9211_i2c_dev, 0x0b, 0x00);//eq control  0x77->0x00
#ifdef _Mipi_PortA_ 
	/*port a*/
	lt9211_i2c_reg_write(lt9211->lt9211_i2c_dev, 0x07, 0x9f); //port clk enable  （if only open Portb, poart a's lane0 clk need open）
	lt9211_i2c_reg_write(lt9211->lt9211_i2c_dev, 0x08, 0xfc); //port lprx enable
#endif  
#ifdef _Mipi_PortB_	
	/*port a*/
	lt9211_i2c_reg_write(lt9211->lt9211_i2c_dev, 0x07, 0x9f); //port clk enable  （if only open Portb, poart a's lane0 clk need open）
	lt9211_i2c_reg_write(lt9211->lt9211_i2c_dev, 0x08, 0xfc); //port lprx enable	
	/*port b*/
	lt9211_i2c_reg_write(lt9211->lt9211_i2c_dev, 0x0f, 0x9F); //port clk enable
	lt9211_i2c_reg_write(lt9211->lt9211_i2c_dev, 0x10, 0xfc); //port lprx enable
	lt9211_i2c_reg_write(lt9211->lt9211_i2c_dev, 0x04, 0xa1);
#endif
	/*port diff swap*/
	lt9211_i2c_reg_write(lt9211->lt9211_i2c_dev, 0x09, 0x01); //port a diff swap
	lt9211_i2c_reg_write(lt9211->lt9211_i2c_dev, 0x11, 0x01); //port b diff swap	

	/*port lane swap*/
	lt9211_i2c_reg_write(lt9211->lt9211_i2c_dev, 0xff, 0x86);		
	lt9211_i2c_reg_write(lt9211->lt9211_i2c_dev, 0x33, 0x1b); //port a lane swap 1b:no swap	
	lt9211_i2c_reg_write(lt9211->lt9211_i2c_dev, 0x34, 0x1b); //port b lane swap 1b:no swap

}

void lt9211_mipirxdigital(struct lt9211_data *lt9211)
{	  
	lt9211_i2c_reg_write(lt9211->lt9211_i2c_dev, 0xff, 0x86);
#ifdef _Mipi_PortA_ 	
	lt9211_i2c_reg_write(lt9211->lt9211_i2c_dev, 0x30, 0x85); //mipirx HL swap	 	
#endif

#ifdef _Mipi_PortB_	
	lt9211_i2c_reg_write(lt9211->lt9211_i2c_dev, 0x30, 0x8f); //mipirx HL swap
#endif
	lt9211_i2c_reg_write(lt9211->lt9211_i2c_dev, 0xff, 0xD8);
#ifdef _Mipi_PortA_ 	
	lt9211_i2c_reg_write(lt9211->lt9211_i2c_dev, 0x16, 0x00); //mipirx HL swap bit7- 0:portAinput	
#endif

#ifdef _Mipi_PortB_	
	lt9211_i2c_reg_write(lt9211->lt9211_i2c_dev, 0x16, 0x80); //mipirx HL swap bit7- portBinput
#endif	

	lt9211_i2c_reg_write(lt9211->lt9211_i2c_dev, 0xff, 0xd0);	
	lt9211_i2c_reg_write(lt9211->lt9211_i2c_dev, 0x43, 0x12); //rpta mode enable,ensure da_mlrx_lptx_en=0

	lt9211_i2c_reg_write(lt9211->lt9211_i2c_dev, 0x02, MIPI_SETTLE_VALUE); //mipi rx controller	//settle value

}

void lt9211_setvideotiming(struct lt9211_data *lt9211, struct video_timing *video_format)
{
	mdelay(100);
	lt9211_i2c_reg_write(lt9211->lt9211_i2c_dev, 0xff, 0xd0);
	lt9211_i2c_reg_write(lt9211->lt9211_i2c_dev, 0x0d,(uint8_t)(video_format->vtotal>>8)); //vtotal[15:8]
	lt9211_i2c_reg_write(lt9211->lt9211_i2c_dev, 0x0e,(uint8_t)(video_format->vtotal)); //vtotal[7:0]
	lt9211_i2c_reg_write(lt9211->lt9211_i2c_dev, 0x0f,(uint8_t)(video_format->vact>>8)); //vactive[15:8]
	lt9211_i2c_reg_write(lt9211->lt9211_i2c_dev, 0x10,(uint8_t)(video_format->vact)); //vactive[7:0]
	lt9211_i2c_reg_write(lt9211->lt9211_i2c_dev, 0x15,(uint8_t)(video_format->vs)); //vs[7:0]
	lt9211_i2c_reg_write(lt9211->lt9211_i2c_dev, 0x17,(uint8_t)(video_format->vfp>>8)); //vfp[15:8]
	lt9211_i2c_reg_write(lt9211->lt9211_i2c_dev, 0x18,(uint8_t)(video_format->vfp)); //vfp[7:0]	

	lt9211_i2c_reg_write(lt9211->lt9211_i2c_dev, 0x11,(uint8_t)(video_format->htotal>>8)); //htotal[15:8]
	lt9211_i2c_reg_write(lt9211->lt9211_i2c_dev, 0x12,(uint8_t)(video_format->htotal)); //htotal[7:0]
	lt9211_i2c_reg_write(lt9211->lt9211_i2c_dev, 0x13,(uint8_t)(video_format->hact>>8)); //hactive[15:8]
	lt9211_i2c_reg_write(lt9211->lt9211_i2c_dev, 0x14,(uint8_t)(video_format->hact)); //hactive[7:0]
	lt9211_i2c_reg_write(lt9211->lt9211_i2c_dev, 0x16,(uint8_t)(video_format->hs)); //hs[7:0]
	lt9211_i2c_reg_write(lt9211->lt9211_i2c_dev, 0x19,(uint8_t)(video_format->hfp>>8)); //hfp[15:8]
	lt9211_i2c_reg_write(lt9211->lt9211_i2c_dev, 0x1a,(uint8_t)(video_format->hfp)); //hfp[7:0]	

}

void lt9211_timingset(struct lt9211_data *lt9211)
{
	uint16_t hact;
	uint16_t vact;
	uint8_t fmt;	
	uint8_t pa_lpn = 0;
	struct video_timing lvds_timing = {0};

	mdelay(500);//500-->100
	lt9211_i2c_reg_write(lt9211->lt9211_i2c_dev, 0xff, 0xd0);
	hact = ((lt9211_i2c_reg_read(lt9211->lt9211_i2c_dev, 0x82))<<8) + lt9211_i2c_reg_read(lt9211->lt9211_i2c_dev, 0x83);
	hact = hact/3;
	fmt = (lt9211_i2c_reg_read(lt9211->lt9211_i2c_dev, 0x84) &0x0f);
	vact = ((lt9211_i2c_reg_read(lt9211->lt9211_i2c_dev, 0x85))<<8) + lt9211_i2c_reg_read(lt9211->lt9211_i2c_dev, 0x86);
	pa_lpn = lt9211_i2c_reg_read(lt9211->lt9211_i2c_dev, 0x9c);
	printf("%s - hact: %u vact: %u fmt: %x pa_lpn: %x\n", __func__, hact, vact, fmt, pa_lpn);
	printf("%s - clk: %lu hact: %u hfp: %u hs: %u hbp: %u vact: %u  vfp: %u vs: %u vbp: %u\n", __func__,
	lt9211->vm.pixelclock,
	lt9211->vm.hactive,
	lt9211->vm.hfront_porch,
	lt9211->vm.hsync_len,
	lt9211->vm.hback_porch,
	lt9211->vm.vactive,
	lt9211->vm.vfront_porch,
	lt9211->vm.vsync_len,
	lt9211->vm.vback_porch);

	lvds_timing.hfp = lt9211->vm.hfront_porch;
	lvds_timing.hs = lt9211->vm.hsync_len;
	lvds_timing.hbp = lt9211->vm.hback_porch;
	lvds_timing.hact = lt9211->vm.hactive;
	lvds_timing.htotal = lvds_timing.hfp + lvds_timing.hs + lvds_timing.hbp + lvds_timing.hact;
	lvds_timing.vfp = lt9211->vm.vfront_porch;
	lvds_timing.vs = lt9211->vm.vsync_len;
	lvds_timing.vbp = lt9211->vm.vback_porch;
	lvds_timing.vact = lt9211->vm.vactive;
	lvds_timing.vtotal = lvds_timing.vfp + lvds_timing.vs + lvds_timing.vbp + lvds_timing.vact;
	lvds_timing.pclk_khz = (lt9211->vm.pixelclock)/1000;

	mdelay(100);

	if ((hact == lvds_timing.hact ) && ( vact == lvds_timing.vact ))
	{
		lt9211_setvideotiming(lt9211, &lvds_timing);
	} else {
		printf("%s - video_none\n", __func__); 
	}

}

void lt9211_mipirxpll(struct lt9211_data *lt9211)
{
	uint32_t  pclk_khz;
	/* dessc pll */
	lt9211_i2c_reg_write(lt9211->lt9211_i2c_dev, 0xff, 0x82);
	lt9211_i2c_reg_write(lt9211->lt9211_i2c_dev, 0x2d, 0x48);

	pclk_khz = (lt9211->vm.pixelclock)/1000;
	if(pclk_khz < 44000) {
		lt9211_i2c_reg_write(lt9211->lt9211_i2c_dev, 0x35, PIXCLK_22M_44M); /*0x83*/
	} else if(pclk_khz < 88000) {
		lt9211_i2c_reg_write(lt9211->lt9211_i2c_dev, 0x35, PIXCLK_44M_88M); /*0x82*/
	} else if(pclk_khz < 176000) {
		lt9211_i2c_reg_write(lt9211->lt9211_i2c_dev, 0x35, PIXCLK_88M_176M); /*0x81*/
	} else if(pclk_khz < 352000) {
		lt9211_i2c_reg_write(lt9211->lt9211_i2c_dev, 0x35, PIXCLK_LARGER_THAN_176M); /*0x80*/
	}

}

void lt9211_mipipcr(struct lt9211_data *lt9211)
{
	uint8_t loopx;
	uint8_t pcr_m;

	lt9211_i2c_reg_write(lt9211->lt9211_i2c_dev, 0xff, 0xd0);
	lt9211_i2c_reg_write(lt9211->lt9211_i2c_dev, 0x0c ,0x60);  //fifo position
	lt9211_i2c_reg_write(lt9211->lt9211_i2c_dev, 0x1c, 0x60);  //fifo position
	lt9211_i2c_reg_write(lt9211->lt9211_i2c_dev, 0x24, 0x70);  //pcr mode( de hs vs)

	lt9211_i2c_reg_write(lt9211->lt9211_i2c_dev, 0x2d, 0x30); //M up limit
	lt9211_i2c_reg_write(lt9211->lt9211_i2c_dev, 0x31, 0x0a); //M down limit

	/*stage1 hs mode*/
	lt9211_i2c_reg_write(lt9211->lt9211_i2c_dev, 0x25, 0xf0);  //line limit
	lt9211_i2c_reg_write(lt9211->lt9211_i2c_dev, 0x2a, 0x30);  //step in limit
	lt9211_i2c_reg_write(lt9211->lt9211_i2c_dev, 0x21, 0x4f);  //hs_step
	lt9211_i2c_reg_write(lt9211->lt9211_i2c_dev, 0x22, 0x00);

	/*stage2 hs mode*/
	lt9211_i2c_reg_write(lt9211->lt9211_i2c_dev, 0x1e, 0x01);  //RGD_DIFF_SND[7:4],RGD_DIFF_FST[3:0]
	lt9211_i2c_reg_write(lt9211->lt9211_i2c_dev, 0x23, 0x80);  //hs_step
	/*stage2 de mode*/
	lt9211_i2c_reg_write(lt9211->lt9211_i2c_dev, 0x0a, 0x02); //de adjust pre line
	lt9211_i2c_reg_write(lt9211->lt9211_i2c_dev, 0x38, 0x02); //de_threshold 1
	lt9211_i2c_reg_write(lt9211->lt9211_i2c_dev, 0x39, 0x04); //de_threshold 2
	lt9211_i2c_reg_write(lt9211->lt9211_i2c_dev, 0x3a, 0x08); //de_threshold 3
	lt9211_i2c_reg_write(lt9211->lt9211_i2c_dev, 0x3b, 0x10); //de_threshold 4

	lt9211_i2c_reg_write(lt9211->lt9211_i2c_dev, 0x3f, 0x04); //de_step 1
	lt9211_i2c_reg_write(lt9211->lt9211_i2c_dev, 0x40, 0x08); //de_step 2
	lt9211_i2c_reg_write(lt9211->lt9211_i2c_dev, 0x41, 0x10); //de_step 3
	lt9211_i2c_reg_write(lt9211->lt9211_i2c_dev, 0x42, 0x20); //de_step 4

	lt9211_i2c_reg_write(lt9211->lt9211_i2c_dev, 0x2b, 0xa0); //stable out
	//mdelay(100);
	lt9211_i2c_reg_write(lt9211->lt9211_i2c_dev, 0xff, 0xd0);   //enable HW pcr_m
	pcr_m = lt9211_i2c_reg_read(lt9211->lt9211_i2c_dev, 0x26);
	pcr_m &= 0x7f;
	lt9211_i2c_reg_write(lt9211->lt9211_i2c_dev, 0x26, pcr_m);
	lt9211_i2c_reg_write(lt9211->lt9211_i2c_dev, 0x27, 0x0f);

	lt9211_i2c_reg_write(lt9211->lt9211_i2c_dev, 0xff, 0x81);  //pcr reset
	lt9211_i2c_reg_write(lt9211->lt9211_i2c_dev, 0x20, 0xbf); // mipi portB div issue
	lt9211_i2c_reg_write(lt9211->lt9211_i2c_dev, 0x20, 0xff);
	mdelay(5);
	lt9211_i2c_reg_write(lt9211->lt9211_i2c_dev, 0x0B, 0x6F);
	lt9211_i2c_reg_write(lt9211->lt9211_i2c_dev, 0x0B, 0xFF);


	mdelay(800);//800->120
	for(loopx = 0; loopx < 10; loopx++) { //Check pcr_stable 10
		mdelay(200);
		lt9211_i2c_reg_write(lt9211->lt9211_i2c_dev, 0xff, 0xd0);
		if((lt9211_i2c_reg_read(lt9211->lt9211_i2c_dev, 0x87)) & 0x08) {
			printf("%s - LT9211 pcr stable\n", __func__);
			break;
		} else {
			printf("%s - LT9211 pcr unstable!!!!\n", __func__);
		}
	}


	lt9211_i2c_reg_write(lt9211->lt9211_i2c_dev, 0xff, 0xd0);
	printf("%s - LT9211 pcr_stable_M=%x\n", __func__, ((lt9211_i2c_reg_read(lt9211->lt9211_i2c_dev, 0x94)) & 0x7F));//print M value
}

void lt9211_txpll(struct lt9211_data *lt9211)
{
	uint8_t loopx;

	if( (lt9211->lvds_output & OUTPUT_LVDS_1_PORT ) || (lt9211->lvds_output & OUTPUT_LVDS_2_PORT) )
	{
		lt9211_i2c_reg_write(lt9211->lt9211_i2c_dev, 0xff, 0x82);
		lt9211_i2c_reg_write(lt9211->lt9211_i2c_dev, 0x36, 0x01); //b7:txpll_pd

		if( lt9211->lvds_output & OUTPUT_LVDS_1_PORT ) {
			lt9211_i2c_reg_write(lt9211->lt9211_i2c_dev, 0x37, 0x29);
		} else {
			lt9211_i2c_reg_write(lt9211->lt9211_i2c_dev, 0x37, 0x2a);
		}
		lt9211_i2c_reg_write(lt9211->lt9211_i2c_dev, 0x38, 0x06);
		lt9211_i2c_reg_write(lt9211->lt9211_i2c_dev, 0x39, 0x30);
		lt9211_i2c_reg_write(lt9211->lt9211_i2c_dev, 0x3a, 0x8e);

		lt9211_i2c_reg_write(lt9211->lt9211_i2c_dev, 0xFF, 0x81);
		lt9211_i2c_reg_write(lt9211->lt9211_i2c_dev, 0x20, 0xF7);// LVDS Txpll soft reset
		lt9211_i2c_reg_write(lt9211->lt9211_i2c_dev, 0x20, 0xFF);

		lt9211_i2c_reg_write(lt9211->lt9211_i2c_dev, 0xff, 0x87);
		lt9211_i2c_reg_write(lt9211->lt9211_i2c_dev, 0x37, 0x14);
		lt9211_i2c_reg_write(lt9211->lt9211_i2c_dev, 0x13, 0x00);
		lt9211_i2c_reg_write(lt9211->lt9211_i2c_dev, 0x13, 0x80);
		mdelay(100);
		for(loopx = 0; loopx < 10; loopx++) {  //Check Tx PLL cal

			lt9211_i2c_reg_write(lt9211->lt9211_i2c_dev, 0xff, 0x87);
			if((lt9211_i2c_reg_read(lt9211->lt9211_i2c_dev, 0x1f)) & 0x80) {
				if((lt9211_i2c_reg_read(lt9211->lt9211_i2c_dev, 0x20)) & 0x80) {
					printf("%s - pll lock\n", __func__);
				} else {
					printf("%s - pll unlocked\n", __func__);
        		}
        		printf("%s - pll cal done\n", __func__);
        		break;
        	} else {
        		printf("%s - pll unlocked\n", __func__);
        	}
		}
	}
	printf("%s - system success\n", __func__);
}

void lt9211_txphy(struct lt9211_data *lt9211)
{
	if(g_lt9211->test_pattern_en) {
		lt9211_i2c_reg_write(lt9211->lt9211_i2c_dev, 0xff, 0x82);
		lt9211_i2c_reg_write(lt9211->lt9211_i2c_dev, 0x62, 0x00); //ttl output disable
		if( lt9211->lvds_output & OUTPUT_LVDS_2_PORT ) {
			lt9211_i2c_reg_write(lt9211->lt9211_i2c_dev, 0x3b, 0xb8);
		}
		else {
			lt9211_i2c_reg_write(lt9211->lt9211_i2c_dev, 0x3b, 0x38);  //dual-port lvds tx phy
		}
		lt9211_i2c_reg_write(lt9211->lt9211_i2c_dev, 0x3e, 0x92);
		lt9211_i2c_reg_write(lt9211->lt9211_i2c_dev, 0x3f, 0x48);
		lt9211_i2c_reg_write(lt9211->lt9211_i2c_dev, 0x40, 0x31);	
		lt9211_i2c_reg_write(lt9211->lt9211_i2c_dev, 0x43, 0x80); 		
		lt9211_i2c_reg_write(lt9211->lt9211_i2c_dev, 0x44, 0x00);
		lt9211_i2c_reg_write(lt9211->lt9211_i2c_dev, 0x45, 0x00); 		
		lt9211_i2c_reg_write(lt9211->lt9211_i2c_dev, 0x49, 0x00);
		lt9211_i2c_reg_write(lt9211->lt9211_i2c_dev, 0x4a, 0x01);
		lt9211_i2c_reg_write(lt9211->lt9211_i2c_dev, 0x4e, 0x00);
		lt9211_i2c_reg_write(lt9211->lt9211_i2c_dev, 0x4f, 0x00);
		lt9211_i2c_reg_write(lt9211->lt9211_i2c_dev, 0x50, 0x00);
		lt9211_i2c_reg_write(lt9211->lt9211_i2c_dev, 0x53, 0x00);
		lt9211_i2c_reg_write(lt9211->lt9211_i2c_dev, 0x54, 0x01);
		
		lt9211_i2c_reg_write(lt9211->lt9211_i2c_dev, 0xff, 0x86);
		lt9211_i2c_reg_write(lt9211->lt9211_i2c_dev, 0x46, 0x10);
		if( lt9211->lvds_output & OUTPUT_LVDS_2_PORT ) {
			printf("%s LVDS Output Port Swap!\n", __func__);
			lt9211_i2c_reg_write(lt9211->lt9211_i2c_dev, 0x46, 0x40);
		}
		
		lt9211_i2c_reg_write(lt9211->lt9211_i2c_dev, 0xff, 0x81);
		lt9211_i2c_reg_write(lt9211->lt9211_i2c_dev, 0x20, 0x79); //ADD 3.3 TX PHY RESET & mlrx mltx calib reset
		lt9211_i2c_reg_write(lt9211->lt9211_i2c_dev, 0x20, 0xff);
	} else {
		lt9211_i2c_reg_write(lt9211->lt9211_i2c_dev, 0xff, 0x82);
		lt9211_i2c_reg_write(lt9211->lt9211_i2c_dev, 0x23, 0x02);//disable the BTA

		if( (lt9211->lvds_output & OUTPUT_LVDS_1_PORT ) || (lt9211->lvds_output & OUTPUT_LVDS_2_PORT) )
		{
			/* dual-port lvds tx phy */
			lt9211_i2c_reg_write(lt9211->lt9211_i2c_dev, 0x62, 0x00); //ttl output disable

			if( lt9211->lvds_output & OUTPUT_LVDS_2_PORT ) {
				lt9211_i2c_reg_write(lt9211->lt9211_i2c_dev, 0x3b, 0x88);//disable lvds output
			} else {
				lt9211_i2c_reg_write(lt9211->lt9211_i2c_dev, 0x3b, 0x08);//disable lvds output
				}
		}
		lt9211_i2c_reg_write(lt9211->lt9211_i2c_dev, 0x3e, 0x92);
		lt9211_i2c_reg_write(lt9211->lt9211_i2c_dev, 0x3f, 0x48);
		lt9211_i2c_reg_write(lt9211->lt9211_i2c_dev, 0x40, 0x31);	
		lt9211_i2c_reg_write(lt9211->lt9211_i2c_dev, 0x43, 0x80); 		
		lt9211_i2c_reg_write(lt9211->lt9211_i2c_dev, 0x44, 0x00);
		lt9211_i2c_reg_write(lt9211->lt9211_i2c_dev, 0x45, 0x00); 		
		lt9211_i2c_reg_write(lt9211->lt9211_i2c_dev, 0x49, 0x00);
		lt9211_i2c_reg_write(lt9211->lt9211_i2c_dev, 0x4a, 0x01);
		lt9211_i2c_reg_write(lt9211->lt9211_i2c_dev, 0x4e, 0x00);	
		lt9211_i2c_reg_write(lt9211->lt9211_i2c_dev, 0x4f, 0x00);
		lt9211_i2c_reg_write(lt9211->lt9211_i2c_dev, 0x50, 0x00);
		lt9211_i2c_reg_write(lt9211->lt9211_i2c_dev, 0x53, 0x00);
		lt9211_i2c_reg_write(lt9211->lt9211_i2c_dev, 0x54, 0x01);

		lt9211_i2c_reg_write(lt9211->lt9211_i2c_dev, 0xff, 0x81);
		lt9211_i2c_reg_write(lt9211->lt9211_i2c_dev, 0x20, 0x79); //TX PHY RESET & mlrx mltx calib reset
		lt9211_i2c_reg_write(lt9211->lt9211_i2c_dev, 0x20, 0xff);
	}


}

void lt9211_txdigital(struct lt9211_data *lt9211)
{
	if( (lt9211->lvds_output & OUTPUT_LVDS_1_PORT ) || (lt9211->lvds_output & OUTPUT_LVDS_2_PORT) ) {
		printf("%s - ----LT9211 set to OUTPUT_LVDS----\n", __func__);
		lt9211_i2c_reg_write(lt9211->lt9211_i2c_dev, 0xff, 0x85); /* lvds tx controller */
		lt9211_i2c_reg_write(lt9211->lt9211_i2c_dev, 0x59, 0x50); //bit4-LVDSTX Display color depth set 1-8bit, 0-6bit;
		if( lt9211->lvds_output & OUTPUT_FORMAT_VESA ) {
        	printf("%s - Data Format: VESA\n", __func__);
        	lt9211_i2c_reg_write(lt9211->lt9211_i2c_dev, 0x59, (lt9211_i2c_reg_read(lt9211->lt9211_i2c_dev, 0x59) & 0x7f));
    	} else if( lt9211->lvds_output & OUTPUT_FORMAT_JEIDA ) {
        	printf("%s - Data Format: JEIDA\n", __func__);
        	lt9211_i2c_reg_write(lt9211->lt9211_i2c_dev, 0x59, (lt9211_i2c_reg_read(lt9211->lt9211_i2c_dev, 0x59) | 0x80));
    	}
    	if( lt9211->lvds_output & OUTPUT_BITDEPTH_666 ) {
        	printf("%s - ColorDepth: 6Bit\n", __func__);
        	lt9211_i2c_reg_write(lt9211->lt9211_i2c_dev, 0x59, (lt9211_i2c_reg_read(lt9211->lt9211_i2c_dev, 0x59) & 0xef));
    	} else if( lt9211->lvds_output & OUTPUT_BITDEPTH_888 ) {
        	printf("%s - ColorDepth: 8Bit\n", __func__);
        	lt9211_i2c_reg_write(lt9211->lt9211_i2c_dev, 0x59, (lt9211_i2c_reg_read(lt9211->lt9211_i2c_dev, 0x59) | 0x10));
    	}
    	if( lt9211->lvds_output & OUTPUT_SYNC_MODE ) {
        	printf("%s - LVDS_MODE: Sync Mode\n", __func__);
        	lt9211_i2c_reg_write(lt9211->lt9211_i2c_dev, 0x59, (lt9211_i2c_reg_read(lt9211->lt9211_i2c_dev, 0x59) & 0xdf));
    	} else if( lt9211->lvds_output & OUTPUT_DE_MODE ) {
        	printf("%s - LVDS_MODE: De Mode\n", __func__);
        	lt9211_i2c_reg_write(lt9211->lt9211_i2c_dev, 0x59, (lt9211_i2c_reg_read(lt9211->lt9211_i2c_dev, 0x59) | 0x20));
    	}

		lt9211_i2c_reg_write(lt9211->lt9211_i2c_dev, 0x5a, 0xaa);
		lt9211_i2c_reg_write(lt9211->lt9211_i2c_dev, 0x5b, 0xaa);
		if( lt9211->lvds_output & OUTPUT_LVDS_2_PORT ) {
			lt9211_i2c_reg_write(lt9211->lt9211_i2c_dev, 0x5c, 0x03);	//lvdstx port sel 01:dual;00:single
		} else {
			lt9211_i2c_reg_write(lt9211->lt9211_i2c_dev, 0x5c, 0x00);
		}

		lt9211_i2c_reg_write(lt9211->lt9211_i2c_dev, 0xa1, 0x77);
		lt9211_i2c_reg_write(lt9211->lt9211_i2c_dev, 0xff, 0x86);
		lt9211_i2c_reg_write(lt9211->lt9211_i2c_dev, 0x40, 0x40); //tx_src_sel
		/*port src sel*/
		lt9211_i2c_reg_write(lt9211->lt9211_i2c_dev, 0x41, 0x34);
		lt9211_i2c_reg_write(lt9211->lt9211_i2c_dev, 0x42, 0x10);
		lt9211_i2c_reg_write(lt9211->lt9211_i2c_dev, 0x43, 0x23); //pt0_tx_src_sel
		lt9211_i2c_reg_write(lt9211->lt9211_i2c_dev, 0x44, 0x41);
		lt9211_i2c_reg_write(lt9211->lt9211_i2c_dev, 0x45, 0x02); //pt1_tx_src_scl
	}
}

void lt9211_clockcheckdebug(struct lt9211_data *lt9211)
{
	uint32_t  fm_value;
	lt9211_i2c_reg_write(lt9211->lt9211_i2c_dev, 0xff, 0x86);
	if(lt9211->test_pattern_en) {
		lt9211_i2c_reg_write(lt9211->lt9211_i2c_dev, 0x00,0x12);
		mdelay(100);
		fm_value = 0;
		fm_value = (lt9211_i2c_reg_read(lt9211->lt9211_i2c_dev, 0x08) &(0x0f));
		fm_value = (fm_value<<8) ;
		fm_value = fm_value + lt9211_i2c_reg_read(lt9211->lt9211_i2c_dev, 0x09);
		fm_value = (fm_value<<8) ;
		fm_value = fm_value + lt9211_i2c_reg_read(lt9211->lt9211_i2c_dev, 0x0a);
		printf("%s - lvds pixclk:  %u\n", __func__, fm_value);
	} else {
		lt9211_i2c_reg_write(lt9211->lt9211_i2c_dev, 0x00, 0x01);
		mdelay(300);
		fm_value = 0;
		fm_value = ((lt9211_i2c_reg_read(lt9211->lt9211_i2c_dev, 0x08)) & (0x0f));
		fm_value = (fm_value<<8) ;
		fm_value = fm_value + lt9211_i2c_reg_read(lt9211->lt9211_i2c_dev, 0x09);
		fm_value = (fm_value<<8) ;
		fm_value = fm_value + lt9211_i2c_reg_read(lt9211->lt9211_i2c_dev, 0x0a);
		printf("%s - mipi input byte clock: %d\n", __func__, fm_value);

		lt9211_i2c_reg_write(lt9211->lt9211_i2c_dev, 0xff, 0x86);
		lt9211_i2c_reg_write(lt9211->lt9211_i2c_dev, 0x00, 0x0a);
		mdelay(300);
		fm_value = 0;
		fm_value = ((lt9211_i2c_reg_read(lt9211->lt9211_i2c_dev, 0x08)) & (0x0f));
		fm_value = (fm_value<<8) ;
		fm_value = fm_value + lt9211_i2c_reg_read(lt9211->lt9211_i2c_dev, 0x09);
		fm_value = (fm_value<<8) ;
		fm_value = fm_value + lt9211_i2c_reg_read(lt9211->lt9211_i2c_dev, 0x0a);
		printf("%s - dessc pixel clock: %d\n", __func__, fm_value);
	}
}

void lt9211_videocheckdebug(struct lt9211_data *lt9211)
{

	uint8_t sync_polarity;
	uint16_t hact, vact;
	uint16_t hs, vs;
	uint16_t hbp, vbp;
	uint16_t htotal, vtotal;
	uint16_t hfp, vfp;

	lt9211_i2c_reg_write(lt9211->lt9211_i2c_dev, 0xff, 0x86);
	sync_polarity = lt9211_i2c_reg_read(lt9211->lt9211_i2c_dev, 0x70);
	vs = lt9211_i2c_reg_read(lt9211->lt9211_i2c_dev, 0x71);

	hs = lt9211_i2c_reg_read(lt9211->lt9211_i2c_dev, 0x72);
	hs = (hs<<8) + lt9211_i2c_reg_read(lt9211->lt9211_i2c_dev, 0x73);

	vbp = lt9211_i2c_reg_read(lt9211->lt9211_i2c_dev, 0x74);
	vfp = lt9211_i2c_reg_read(lt9211->lt9211_i2c_dev, 0x75);

	hbp = lt9211_i2c_reg_read(lt9211->lt9211_i2c_dev, 0x76);
	hbp = (hbp<<8) + lt9211_i2c_reg_read(lt9211->lt9211_i2c_dev, 0x77);

	hfp = lt9211_i2c_reg_read(lt9211->lt9211_i2c_dev, 0x78);
	hfp = (hfp<<8) + lt9211_i2c_reg_read(lt9211->lt9211_i2c_dev, 0x79);

	vtotal = lt9211_i2c_reg_read(lt9211->lt9211_i2c_dev, 0x7A);
	vtotal = (vtotal<<8) + lt9211_i2c_reg_read(lt9211->lt9211_i2c_dev, 0x7B);

	htotal = lt9211_i2c_reg_read(lt9211->lt9211_i2c_dev, 0x7C);
	htotal = (htotal<<8) + lt9211_i2c_reg_read(lt9211->lt9211_i2c_dev, 0x7D);

	vact = lt9211_i2c_reg_read(lt9211->lt9211_i2c_dev, 0x7E);
	vact = (vact<<8)+ lt9211_i2c_reg_read(lt9211->lt9211_i2c_dev, 0x7F);

	hact = lt9211_i2c_reg_read(lt9211->lt9211_i2c_dev, 0x80);
	hact = (hact<<8) + lt9211_i2c_reg_read(lt9211->lt9211_i2c_dev, 0x81);

	printf("%s - sync_polarity = %x\n", __func__, sync_polarity);

	printf("%s - hfp: %d hs: %d hbp: %d hact: %d htotal: %d\n", __func__, hfp, hs, hbp, hact, htotal);

	printf("%s - vfp: %d vs: %d vbp: %d vact: %d vtotal: %d\n", __func__,vfp, vs, vbp, vact, vtotal);

}

void lt9211_pattern(struct lt9211_data *lt9211)
{
	uint32_t  pclk_khz;
	uint8_t dessc_pll_post_div=1;
	uint32_t  pcr_m, pcr_k;
										 //hfp,hs,hbp,hact,htotal,vfp,vs,vbp,vact,vtotal,pixclk Khz
	//struct video_timing video_format  = {96, 64, 96, 1920, 2176, 8, 4, 8, 1080, 1100, 144000};//LM215WF3_SLN1_1920x1080_60Hz
	//struct video_timing video_format  = {80, 34, 80, 1366, 1560, 15, 8, 15, 768, 806, 76000};//LM215WF3_SLN1_1920x1080_60Hz

	struct video_timing video_format = {0};
	video_format.hfp = lt9211->vm.hfront_porch;
	video_format.hs = lt9211->vm.hsync_len;
	video_format.hbp = lt9211->vm.hback_porch;
	video_format.hact = lt9211->vm.hactive;
	video_format.htotal = video_format.hfp + video_format.hs + video_format.hbp + video_format.hact;
	video_format.vfp = lt9211->vm.vfront_porch;
	video_format.vs = lt9211->vm.vsync_len;
	video_format.vbp = lt9211->vm.vback_porch;
	video_format.vact = lt9211->vm.vactive;
	video_format.vtotal = video_format.vfp + video_format.vs + video_format.vbp + video_format.vact;
	video_format.pclk_khz = (lt9211->vm.pixelclock)/1000;

	printf("%s - clk: %u hact: %u hfp: %u hs: %u hbp: %u vact: %u  vfp: %u vs: %u vbp: %u\n", __func__,
		video_format.pclk_khz,
		video_format.hact,
		video_format.hfp,
		video_format.hs,
		video_format.hbp,
		video_format.vact,
		video_format.vfp,
		video_format.vs,
		video_format.vbp);


	pclk_khz = video_format.pclk_khz;

	lt9211_i2c_reg_write(lt9211->lt9211_i2c_dev, 0xff, 0xf9);
	lt9211_i2c_reg_write(lt9211->lt9211_i2c_dev, 0x3e, 0x80);

	lt9211_i2c_reg_write(lt9211->lt9211_i2c_dev, 0xff, 0x85);
	lt9211_i2c_reg_write(lt9211->lt9211_i2c_dev, 0x88, 0xc0);   //0x90:TTL RX-->Mipi TX  ; 0xd0:lvds RX->MipiTX  0xc0:Chip Video pattern gen->Lvd TX

	lt9211_i2c_reg_write(lt9211->lt9211_i2c_dev, 0xa1, 0x74);
	lt9211_i2c_reg_write(lt9211->lt9211_i2c_dev, 0xa2, 0xff);

	lt9211_i2c_reg_write(lt9211->lt9211_i2c_dev, 0xa3,(uint8_t)((video_format.hs+video_format.hbp)/256));
	lt9211_i2c_reg_write(lt9211->lt9211_i2c_dev, 0xa4,(uint8_t)((video_format.hs+video_format.hbp)%256));//h_start

	lt9211_i2c_reg_write(lt9211->lt9211_i2c_dev, 0xa5,(uint8_t)((video_format.vs+video_format.vbp)%256));//v_start

	lt9211_i2c_reg_write(lt9211->lt9211_i2c_dev, 0xa6,(uint8_t)(video_format.hact/256));
	lt9211_i2c_reg_write(lt9211->lt9211_i2c_dev, 0xa7,(uint8_t)(video_format.hact%256)); //hactive

	lt9211_i2c_reg_write(lt9211->lt9211_i2c_dev, 0xa8,(uint8_t)(video_format.vact/256));
	lt9211_i2c_reg_write(lt9211->lt9211_i2c_dev, 0xa9,(uint8_t)(video_format.vact%256));  //vactive

	lt9211_i2c_reg_write(lt9211->lt9211_i2c_dev, 0xaa,(uint8_t)(video_format.htotal/256));
	lt9211_i2c_reg_write(lt9211->lt9211_i2c_dev, 0xab,(uint8_t)(video_format.htotal%256));//htotal

	lt9211_i2c_reg_write(lt9211->lt9211_i2c_dev, 0xac,(uint8_t)(video_format.vtotal/256));
	lt9211_i2c_reg_write(lt9211->lt9211_i2c_dev, 0xad,(uint8_t)(video_format.vtotal%256));//vtotal

	lt9211_i2c_reg_write(lt9211->lt9211_i2c_dev, 0xae,(uint8_t)(video_format.hs/256));
	lt9211_i2c_reg_write(lt9211->lt9211_i2c_dev, 0xaf,(uint8_t)(video_format.hs%256));   //hsa

	lt9211_i2c_reg_write(lt9211->lt9211_i2c_dev, 0xb0,(uint8_t)(video_format.vs%256));    //vsa

	//dessc pll to generate pixel clk
	lt9211_i2c_reg_write(lt9211->lt9211_i2c_dev, 0xff, 0x82); //dessc pll
	lt9211_i2c_reg_write(lt9211->lt9211_i2c_dev, 0x2d, 0x48); //pll ref select xtal

	if(pclk_khz < 44000) {
		lt9211_i2c_reg_write(lt9211->lt9211_i2c_dev, 0x35, 0x83);
		dessc_pll_post_div = 16;
	} else if(pclk_khz < 88000) {
		lt9211_i2c_reg_write(lt9211->lt9211_i2c_dev, 0x35, 0x82);
		dessc_pll_post_div = 8;
	} else if(pclk_khz < 176000) {
		lt9211_i2c_reg_write(lt9211->lt9211_i2c_dev, 0x35, 0x81);
		dessc_pll_post_div = 4;
	} else if(pclk_khz < 352000) {
		lt9211_i2c_reg_write(lt9211->lt9211_i2c_dev, 0x35, 0x80);
		dessc_pll_post_div = 0;
	}

	pcr_m = (pclk_khz * dessc_pll_post_div) /25;
	pcr_k = pcr_m%1000;
	pcr_m = pcr_m/1000;

	pcr_k <<= 14;

	//pixel clk
	lt9211_i2c_reg_write(lt9211->lt9211_i2c_dev, 0xff, 0xd0); //pcr
	lt9211_i2c_reg_write(lt9211->lt9211_i2c_dev, 0x2d, 0x7f);
	lt9211_i2c_reg_write(lt9211->lt9211_i2c_dev, 0x31, 0x00);

	lt9211_i2c_reg_write(lt9211->lt9211_i2c_dev, 0x26, 0x80|((uint8_t)pcr_m));
	lt9211_i2c_reg_write(lt9211->lt9211_i2c_dev, 0x27,(uint8_t)((pcr_k>>16)&0xff)); //K
	lt9211_i2c_reg_write(lt9211->lt9211_i2c_dev, 0x28,(uint8_t)((pcr_k>>8)&0xff)); //K
	lt9211_i2c_reg_write(lt9211->lt9211_i2c_dev, 0x29,(uint8_t)(pcr_k&0xff)); //K
}

void lt9211_lvds_tx_logic_rst(struct lt9211_data *lt9211)
{
	lt9211_i2c_reg_write(lt9211->lt9211_i2c_dev, 0xff, 0x81);
	lt9211_i2c_reg_write(lt9211->lt9211_i2c_dev, 0x0d, 0xfb); //LVDS TX LOGIC RESET
	mdelay(10);
	lt9211_i2c_reg_write(lt9211->lt9211_i2c_dev, 0x0d, 0xff); //LVDS TX LOGIC RESET  RELEASE
}

void lt9211_lvds_tx_en(struct lt9211_data *lt9211)
{
	lt9211_i2c_reg_write(lt9211->lt9211_i2c_dev, 0xff, 0x82);

	if( lt9211->lvds_output & OUTPUT_LVDS_2_PORT ) {
	    lt9211_i2c_reg_write(lt9211->lt9211_i2c_dev, 0x3b, 0xb8);//dual-port lvds output Enable
	} else {
	    lt9211_i2c_reg_write(lt9211->lt9211_i2c_dev, 0x3b, 0x38);//signal-port lvds output Enable
	}
}

bool lt9211_is_connected(void)
{
	printf("%s  lt9211 connect = %d\n", __func__, connect_lt9211);
	return connect_lt9211;
}

void lt9211_lvds_power_on(void)
{
	printf("%s \n", __func__);
	if (dm_gpio_is_valid(&g_lt9211->lvds_vdd_en_gpio)) {
		dm_gpio_set_value(&g_lt9211->lvds_vdd_en_gpio, 1);
	}
	return;
}

void lt9211_lvds_power_off(void)
{
	printf("%s \n", __func__);
	if (&g_lt9211->lvds_vdd_en_gpio) {
		dm_gpio_set_value(&g_lt9211->lvds_vdd_en_gpio, 0);
	}
	return;
}

void lt9211_ConvertBoard_power_on(struct lt9211_data *lt9211)
{
	printf("%s \n", __func__);
	if (dm_gpio_is_valid(&lt9211->pwr_source_gpio)) {
		dm_gpio_set_value(&lt9211->pwr_source_gpio, 1);
		mdelay(10);
	}
}

void lt9211_ConvertBoard_power_off(struct lt9211_data *lt9211)
{
	printf("%s \n", __func__);
	if (dm_gpio_is_valid(&lt9211->pwr_source_gpio)) {
		dm_gpio_set_value(&lt9211->pwr_source_gpio, 0);
	}
}

void lt9211_chip_enable(struct lt9211_data *lt9211)
{
	printf("%s \n", __func__);
	if(!lt9211->powered) {
		if (dm_gpio_is_valid(&lt9211->lt9211_en_gpio)) {
			dm_gpio_set_value(&lt9211->lt9211_en_gpio, 1);
			mdelay(10);//chip enable to init lt9211
		}
	}

	lt9211->powered = true;
}

void lt9211_chip_shutdown(struct lt9211_data *lt9211)
{
	printf("%s \n", __func__);
	if(lt9211->powered) {
		if (dm_gpio_is_valid(&lt9211->lt9211_en_gpio)) {
			dm_gpio_set_value(&lt9211->lt9211_en_gpio, 0);
			mdelay(10);
		}
	}

	lt9211->powered = false;
}

void lt9211_init_seq(struct lt9211_data *lt9211)
{
	printf("%s \n", __func__);
	lt9211_chipid(lt9211);
	lt9211_systemint(lt9211);
		
	lt9211_mipirxphy(lt9211);
	lt9211_mipirxdigital(lt9211);
	lt9211_timingset(lt9211);
	lt9211_mipirxpll(lt9211);
		
	lt9211_mipipcr(lt9211);
	mdelay(10);
	lt9211_txpll(lt9211);
	lt9211_txphy(lt9211);
	lt9211_txdigital(lt9211);
		
	lt9211_lvds_tx_logic_rst(lt9211);    //LVDS TX LOGIC RESET 
	lt9211_lvds_tx_en(lt9211);           //LVDS TX output enable 
	
	lt9211_clockcheckdebug(lt9211);
	lt9211_videocheckdebug(lt9211);
}

void lt9211_lvds_pattern_config(void)
{ 
	if (!g_lt9211) {
		printf("%s g_lt9211 is null!\n", __func__);
		return;
	}
	printf("%s\n", __func__);
	if(g_lt9211->test_pattern_en) {
		lt9211_chipid(g_lt9211);
		lt9211_systemint(g_lt9211);
		//mdelay(100);
				
		lt9211_pattern(g_lt9211);
		lt9211_i2c_reg_write(g_lt9211->lt9211_i2c_dev, 0xff, 0x81);
		lt9211_i2c_reg_write(g_lt9211->lt9211_i2c_dev, 0xff, 0x81);//ADD 3.3 LVDS TX LOGIC RESET 
		//mdelay(10);
		lt9211_txpll(g_lt9211);
		lt9211_txphy(g_lt9211);	
		lt9211_txdigital(g_lt9211);
				
				
		lt9211_i2c_reg_write(g_lt9211->lt9211_i2c_dev, 0xff, 0x81);
		lt9211_i2c_reg_write(g_lt9211->lt9211_i2c_dev, 0x0D, 0xff);//ADD 3.3 LVDS TX LOGIC RESET  RELEASE

		lt9211_clockcheckdebug(g_lt9211);
		lt9211_videocheckdebug(g_lt9211);
	}
}

bool lt9211_detect(struct lt9211_data *lt9211)
{
	#define ID_REGISTERS_SZIE (3)
	uint8_t id[ID_REGISTERS_SZIE] = {0x18, 0x01, 0xe4};
	uint8_t getid[3] = {0};
	int i;

	lt9211_i2c_reg_write(lt9211->lt9211_i2c_dev, 0xFF, 0x81);
	if (lt9211->status)
		return lt9211->status;

	for (i = 0; i < sizeof(getid) /sizeof(uint8_t); i++) {
		getid[i] = lt9211_i2c_reg_read(lt9211->lt9211_i2c_dev, i);
	}

	if (!memcmp(id, getid, sizeof(id) /sizeof(uint8_t))) {
		printf("lt9211_detect successful\n");
		lt9211->status = true;
	} else {
		printf("lt9211_detect fail, 0x%x 0x%x 0x%x\n",getid[0], getid[1], getid[2]);
		lt9211->status = false;
	}

	return lt9211->status;
}

void lt9211_bridge_enable(void)
{
	if (!g_lt9211) {
		printf("%s g_lt9211 is null!\n", __func__);
		return;
	}
		
	printf("%s \n", __func__);
	if (g_lt9211->enabled)
		return;
	
	lt9211_chip_enable(g_lt9211);
	if(!g_lt9211->test_pattern_en)
		lt9211_init_seq(g_lt9211);
	else
		lt9211_lvds_pattern_config();

	g_lt9211->enabled = true;

	return;
}

void lt9211_bridge_disable(void)
{
	if (!g_lt9211) {
		printf("%s g_lt9211 is null!\n", __func__);
		return;
	}

	printf("%s \n", __func__);
	if (!g_lt9211->enabled)
		return;

	lt9211_chip_shutdown(g_lt9211);
	g_lt9211->enabled = false;
	return;
}

void lt9211_set_videomode(struct drm_display_mode *dmode)
{
	if (g_lt9211) {
		g_lt9211->vm.pixelclock = dmode->clock * 1000;
		g_lt9211->vm.hactive = dmode->hdisplay;
		g_lt9211->vm.hfront_porch = dmode->hsync_start - dmode->hdisplay;
		g_lt9211->vm.hsync_len = dmode->hsync_end - dmode->hsync_start;
		g_lt9211->vm.hback_porch = dmode->htotal - dmode->hsync_end;
		g_lt9211->vm.vactive = dmode->vdisplay;
		g_lt9211->vm.vfront_porch = dmode->vsync_start - dmode->vdisplay;
		g_lt9211->vm.vsync_len = dmode->vsync_end - dmode->vsync_start;
		g_lt9211->vm.vback_porch = dmode->vtotal - dmode->vsync_end;
		g_lt9211->vm.flags = 0;
		printf("%s - clk: %lu hact: %u hfp: %u hs: %u hbp: %u vact: %u  vfp: %u vs: %u vbp: %u\n", __func__,
			g_lt9211->vm.pixelclock,
			g_lt9211->vm.hactive,
			g_lt9211->vm.hfront_porch,
			g_lt9211->vm.hsync_len,
			g_lt9211->vm.hback_porch,
			g_lt9211->vm.vactive,
			g_lt9211->vm.vfront_porch,
			g_lt9211->vm.vsync_len,
			g_lt9211->vm.vback_porch);
	}
}

bool lt9211_test_pattern(void)
{
	printf("lt9211_test_pattern: %d\n", g_lt9211->test_pattern_en);
	return g_lt9211->test_pattern_en;
}

static int lt9211_probe(struct udevice *dev)
{
	struct lt9211_data *lt9211 = dev_get_priv(dev);
	int ret;

	printf("%s\n", __func__);
	lt9211->dev = dev;

	lt9211->i2c_id = of_alias_get_id(ofnode_to_np(dev->node)->parent, "i2c");
	if (lt9211->i2c_id < 0) {
		printf("lt9211_parse_dt: error data->i2c_id = %d, use default number 8. \n", lt9211->i2c_id);
		lt9211->i2c_id = 8;
	}
	lt9211->addr = dev_read_u32_default(dev, "reg", 0x2D);

	lt9211->enabled = false;
	lt9211->powered = false;
	lt9211->status = false;
	lt9211->test_pattern_en = false;
	lt9211->lvds_output = 0;

	lt9211->mipi_lanes = dev_read_u32_default(dev, "dsi-lanes", 2);
	if(lt9211->mipi_lanes == 1) {
		lt9211->lvds_output |= OUTPUT_MIPI_1_LANE;
	} else if(lt9211->mipi_lanes == 2) {
		lt9211->lvds_output |= OUTPUT_MIPI_2_LANE;
	} else if(lt9211->mipi_lanes == 3) {
		lt9211->lvds_output |= OUTPUT_MIPI_3_LANE;
	} else if(lt9211->mipi_lanes == 4) {
		lt9211->lvds_output |= OUTPUT_MIPI_4_LANE;
	} else {
		printf("Invalid dsi-lanes: %d\n", lt9211->mipi_lanes);
		ret = -ENODEV;
	}

	lt9211->lvds_format = dev_read_u32_default(dev, "lvds-format", 2);
	if(lt9211->lvds_format == 1) {
		lt9211->lvds_output |= OUTPUT_FORMAT_JEIDA;
	} else if(lt9211->lvds_format == 2) {
		lt9211->lvds_output |= OUTPUT_FORMAT_VESA;
	} else {
		printf("Invalid lvds-format: %d\n", lt9211->lvds_format);
		ret = -ENODEV;
	}

	lt9211->lvds_bpp = dev_read_u32_default(dev, "lvds-bpp", 24);
	if(lt9211->lvds_bpp == 24) {
		lt9211->lvds_output |= OUTPUT_BITDEPTH_888;
	} else {
		lt9211->lvds_output |= OUTPUT_BITDEPTH_666;
	}

	lt9211->dual_link = dev_read_bool(dev, "dual-link");
	if(lt9211->dual_link) {
		lt9211->lvds_output |= OUTPUT_LVDS_2_PORT;
	} else {
		lt9211->lvds_output |= OUTPUT_LVDS_1_PORT;
	}

	if(dev_read_bool(dev, "de-mode")) {
		lt9211->lvds_output |= OUTPUT_DE_MODE;
	} else {
		lt9211->lvds_output |= OUTPUT_SYNC_MODE;
	}

	lt9211->test_pattern_en = dev_read_bool(dev, "test-pattern");

	printf("lt9211_parse_dt lvds-format=%u lvds-bpp=%u test-pattern=%s\n", lt9211->lvds_format, lt9211->lvds_bpp, lt9211->test_pattern_en? "true" : "false");

	ret = gpio_request_by_name(dev, "EN-gpios", 0, &lt9211->lt9211_en_gpio , GPIOD_IS_OUT);
	if (ret) {
		printf("Cannot get EN GPIO: %d\n", ret);
	}

	ret = gpio_request_by_name(dev, "lvds_vdd_en-gpios", 0, &lt9211->lvds_vdd_en_gpio , GPIOD_IS_OUT);
	if (ret) {
		printf("Cannot get lvds_vdd_en_gpio GPIO: %d\n", ret);
	}

	ret = gpio_request_by_name(dev, "lvds_hdmi_sel-gpios", 0, &lt9211->lvds_hdmi_sel_gpio , GPIOD_IS_OUT);
	if (ret) {
		printf("Cannot get lvds_hdmi_sel_gpio GPIO: %d\n", ret);
	}

	ret = gpio_request_by_name(dev, "pwr_source-gpios", 0, &lt9211->pwr_source_gpio , GPIOD_IS_OUT);
	if (ret) {
		printf("Cannot get pwr_source GPIO: %d\n", ret);
	}

	i2c_get_chip_for_busnum(lt9211->i2c_id, lt9211->addr, 1, &lt9211->lt9211_i2c_dev);

	lt9211_ConvertBoard_power_on(lt9211);
	lt9211_chip_enable(lt9211);
	lt9211_detect(lt9211);

	if (lt9211->status) {
		printf("%s : lt9211 is connected!\n", __func__);
		connect_lt9211 = true;
	} else {
		printf("%s : lt9211 is disconnected!\n", __func__);
		if (dm_gpio_is_valid(&lt9211->lvds_vdd_en_gpio))
			gpio_free(gpio_get_number(&lt9211->lvds_vdd_en_gpio));
		if (dm_gpio_is_valid(&lt9211->pwr_source_gpio))
			gpio_free(gpio_get_number(&lt9211->pwr_source_gpio));
		if (dm_gpio_is_valid(&lt9211->lt9211_en_gpio))
			gpio_free(gpio_get_number(&lt9211->lt9211_en_gpio));

		connect_lt9211 = false;
		ret = -ENODEV;
		return ret;
	}

	g_lt9211 = lt9211;
	printf("%s -\n", __func__);

	return 0;
}

static const struct udevice_id lt9211_of_match[] = {
	{ .compatible = "lt9211" },
	{}
};

U_BOOT_DRIVER(lt9211) = {
	.name = "lt9211",
	.id = UCLASS_I2C_GENERIC,
	.of_match = lt9211_of_match,
	.probe = lt9211_probe,
	.bind = dm_scan_fdt_dev,
	.priv_auto_alloc_size = sizeof(struct lt9211_data),
};
