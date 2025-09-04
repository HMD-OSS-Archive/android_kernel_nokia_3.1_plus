/* Copyright Statement:
*
* This software/firmware and related documentation ("MediaTek Software") are
* protected under relevant copyright laws. The information contained herein
* is confidential and proprietary to MediaTek Inc. and/or its licensors.
* Without the prior written permission of MediaTek inc. and/or its licensors,
* any reproduction, modification, use or disclosure of MediaTek Software,
* and information contained herein, in whole or in part, shall be strictly prohibited.
*/
/* MediaTek Inc. (C) 2015. All rights reserved.
*
* BY OPENING THIS FILE, RECEIVER HEREBY UNEQUIVOCALLY ACKNOWLEDGES AND AGREES
* THAT THE SOFTWARE/FIRMWARE AND ITS DOCUMENTATIONS ("MEDIATEK SOFTWARE")
* RECEIVED FROM MEDIATEK AND/OR ITS REPRESENTATIVES ARE PROVIDED TO RECEIVER ON
* AN "AS-IS" BASIS ONLY. MEDIATEK EXPRESSLY DISCLAIMS ANY AND ALL WARRANTIES,
* EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE IMPLIED WARRANTIES OF
* MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE OR NONINFRINGEMENT.
* NEITHER DOES MEDIATEK PROVIDE ANY WARRANTY WHATSOEVER WITH RESPECT TO THE
* SOFTWARE OF ANY THIRD PARTY WHICH MAY BE USED BY, INCORPORATED IN, OR
* SUPPLIED WITH THE MEDIATEK SOFTWARE, AND RECEIVER AGREES TO LOOK ONLY TO SUCH
* THIRD PARTY FOR ANY WARRANTY CLAIM RELATING THERETO. RECEIVER EXPRESSLY ACKNOWLEDGES
* THAT IT IS RECEIVER'S SOLE RESPONSIBILITY TO OBTAIN FROM ANY THIRD PARTY ALL PROPER LICENSES
* CONTAINED IN MEDIATEK SOFTWARE. MEDIATEK SHALL ALSO NOT BE RESPONSIBLE FOR ANY MEDIATEK
* SOFTWARE RELEASES MADE TO RECEIVER'S SPECIFICATION OR TO CONFORM TO A PARTICULAR
* STANDARD OR OPEN FORUM. RECEIVER'S SOLE AND EXCLUSIVE REMEDY AND MEDIATEK'S ENTIRE AND
* CUMULATIVE LIABILITY WITH RESPECT TO THE MEDIATEK SOFTWARE RELEASED HEREUNDER WILL BE,
* AT MEDIATEK'S OPTION, TO REVISE OR REPLACE THE MEDIATEK SOFTWARE AT ISSUE,
* OR REFUND ANY SOFTWARE LICENSE FEES OR SERVICE CHARGE PAID BY RECEIVER TO
* MEDIATEK FOR SUCH MEDIATEK SOFTWARE AT ISSUE.
*/

#define LOG_TAG "LCM"

#ifndef BUILD_LK
#include <linux/string.h>
#include <linux/kernel.h>
#endif

#include "lcm_drv.h"
#include "lcm_define.h"
//#include <mt-plat/mt_gpio.h>
#include "lcm_gpio.h"
#include <linux/gpio.h>

#ifdef BUILD_LK
#include <platform/upmu_common.h>
#include <platform/mt_gpio.h>
#include <platform/mt_i2c.h>
#include <platform/mt_pmic.h>
#include <string.h>
#elif defined(BUILD_UBOOT)
#include <asm/arch/mt_gpio.h>
#else
#include "disp_dts_gpio.h"
#endif

extern int double_tap_work_flags;
//++++this is new way to reset tp rst gpio.maybe have bug ,will debug later
#define GTP_RST_PORT    0
extern void tpd_gpio_output(int pin, int level);

static int double_tap_work_flags_resume = 0;

//----this is new way to reset tp rst gpio.maybe have bug ,will debug later
//#define USE_CABC_FUNC 1

#ifdef BUILD_LK
#define LCM_LOGI(string, args...)  dprintf(ALWAYS, "[LK/"LOG_TAG"]"string, ##args)
#define LCM_LOGD(string, args...)  dprintf(INFO, "[LK/"LOG_TAG"]"string, ##args)
#else
#define LCM_LOGI(fmt, args...)  pr_notice("[KERNEL/"LOG_TAG"]"fmt, ##args)
#define LCM_LOGD(fmt, args...)  pr_debug("[KERNEL/"LOG_TAG"]"fmt, ##args)
#endif

#define LCM_ID (0x98)

static const unsigned int BL_MIN_LEVEL = 20;
static struct LCM_UTIL_FUNCS lcm_util;

#define SET_RESET_PIN(v)	(lcm_util.set_reset_pin((v)))
#define MDELAY(n)		(lcm_util.mdelay(n))
#define UDELAY(n)		(lcm_util.udelay(n))


#define dsi_set_cmdq_V22(cmdq, cmd, count, ppara, force_update) \
	lcm_util.dsi_set_cmdq_V22(cmdq, cmd, count, ppara, force_update)
#define dsi_set_cmdq_V2(cmd, count, ppara, force_update) \
    lcm_util.dsi_set_cmdq_V2(cmd, count, ppara, force_update)
#define dsi_set_cmdq(pdata, queue_size, force_update) \
        lcm_util.dsi_set_cmdq(pdata, queue_size, force_update)
#define wrtie_cmd(cmd) lcm_util.dsi_write_cmd(cmd)
#define write_regs(addr, pdata, byte_nums) \
        lcm_util.dsi_write_regs(addr, pdata, byte_nums)
#define read_reg(cmd) \
      lcm_util.dsi_dcs_read_lcm_reg(cmd)
#define read_reg_v2(cmd, buffer, buffer_size) \
        lcm_util.dsi_dcs_read_lcm_reg_v2(cmd, buffer, buffer_size)

#ifndef BUILD_LK
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/slab.h>
#include <linux/init.h>
#include <linux/list.h>
#include <linux/i2c.h>
#include <linux/irq.h>
/* #include <linux/jiffies.h> */
/* #include <linux/delay.h> */
#include <linux/uaccess.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/platform_device.h>
#endif

/* static unsigned char lcd_id_pins_value = 0xFF; */
static const unsigned char LCD_MODULE_ID = 0x01;
extern void core_config_sense_ctrl(bool start);
extern int mt_set_gpio_mode(unsigned long pin, unsigned long mode);
/* --------------------------------------------------------------------------- */
/* Local Constants */
/* --------------------------------------------------------------------------- */
#define LCM_DSI_CMD_MODE                                    0
#define FRAME_WIDTH                                     (720)
#define FRAME_HEIGHT                                    (1440)
#define LCM_PHYSICAL_WIDTH		(68040)
#define LCM_PHYSICAL_HEIGHT		(136080)
#define LCM_DENSITY	(320)

#define REGFLAG_DELAY		0xFFFC
#define REGFLAG_UDELAY	0xFFFB
#define REGFLAG_END_OF_TABLE	0xFFFD
#define REGFLAG_RESET_LOW	0xFFFE
#define REGFLAG_RESET_HIGH	0xFFFF

static struct LCM_DSI_MODE_SWITCH_CMD lcm_switch_mode_cmd;

#ifndef TRUE
#define TRUE 1
#endif

#ifndef FALSE
#define FALSE 0
#endif

struct LCM_setting_table {
	unsigned int cmd;
	unsigned char count;
	unsigned char para_list[64];
};

static struct LCM_setting_table lcm_suspend_setting[] = {
	{0x28, 0, {} },
	{REGFLAG_DELAY, 20, {} },
	{0x10, 0, {} },
	{REGFLAG_DELAY, 120, {} },
};

#define USE_OTP_FUN 	1
static struct LCM_setting_table init_setting_cmd[] = {
	{ 0xFF, 0x03, {0x98, 0x81, 0x03} },
};

static struct LCM_setting_table init_setting_vdo[] = {
{0xFF,0x03,{0x98,0x81,0x01}},
	{0x09,0x01,{0x01}},      
	{0x17,0x01,{0x01}},
	{0xb9,0x01,{0x08}},
	{0xc3,0x01,{0xff}},
	{0xFF,0x03,{0x98,0x81,0x05}},
	//{0x03,0x01,{0x01}}, //VCOM
	//{0x04,0x01,{0x10}}, //VCOM
	{0x63,0x01,{0x7E}}, //GVDDN
	{0x64,0x01,{0x7E}}, //GVDDP
	/*
	{0x68,0x01,{0xA1}}, //VGHO
	{0x69,0x01,{0xB1}}, //VGH
	{0x6A,0x01,{0x79}}, //VGLO
	{0x6B,0x01,{0x89}}, //VGL
	*/
	/*2018-06-12 modify from FAE ilitek*/
	
	{0x68,0x01,{0xA2}}, //VGHO
	{0x69,0x01,{0xAA}}, //VGH
	{0x6A,0x01,{0x7B}}, //VGLO
	{0x6B,0x01,{0x70}}, //VGL
	{0x32,0x01,{0x00}}, //VGHO 
	{0x33,0x01,{0x00}}, //VGHO 
	{0x36,0x01,{0x00}}, //VGH  
	{0x37,0x01,{0x01}}, //VGH  
	{0x34,0x01,{0x01}}, //VGLO 
	{0x35,0x01,{0x08}}, //VGLO 
	{0x38,0x01,{0x01}}, //VGL  
	{0x39,0x01,{0x01}}, //VGL
	{0xFF,0x03,{0x98,0x81,0x06}},
	{0xC2,0x01,{0x04}},
	{0xFF,0x03,{0x98,0x81,0x02}},
	{0x4D,0x01,{0x4E}}, // disable PC
	{0x4E,0x01,{0x00}}, // SD_SAP 2-->0
	
	{0xFF,0x03,{0x98,0x81,0x08}},
	{0xE0,0x27,{0x40,0x1E,0x9A,0xD7,0x1D,0x55,0x50,0x77,0xA3,0xC7,0xA9,0xFB,0x25,0x4A,0x6D,0xEA,0x93,0xC1,0xDF,0x06,0xFF,0x29,0x56,0x8E,0xB8,0x03,0xEC}},
	{0xE1,0x27,{0x40,0x00,0x9A,0xD7,0x1D,0x55,0x50,0x77,0xA3,0xC7,0xA9,0xFB,0x25,0x4A,0x6D,0xEA,0x93,0xC1,0xDF,0x06,0xFF,0x29,0x56,0x8E,0xB8,0x03,0xEC}},
	{0xFF,0x03,{0x98,0x81,0x06}},
	{0xD6,0x01,{0x85}}, //FTE=TSVD1, FTE1=TSHD
	{0xFF,0x03,{0x98,0x81,0x0E}},
	{0x00,0x01,{0xA0}}, //LH mode
	{0x01,0x01,{0x26}}, //DELY_VID
	{0x13,0x01,{0x0d}},
	{0x20,0x01,{0x0B}}, //17 //0B //
	{0x29,0x01,{0x78}}, //3C //78 //
	{0x25,0x01,{0x06}}, //
	{0x26,0x01,{0x29}}, //
	{0x2D,0x01,{0xA1}},
	{0x40,0x01,{0x0B}},
	{0x49,0x01,{0x78}},
	{0x45,0x01,{0x06}},
	{0x46,0x01,{0x29}},
	{0x4D,0x01,{0xA1}},
	{0xC1,0x01,{0x50}},
	{0xD0,0x01,{0x04}},
	{0xD6,0x01,{0x20}},
	{0xD7,0x01,{0x30}},
	{0xD8,0x01,{0xA0}},
	{0xD9,0x01,{0xA0}},
	{0xE2,0x01,{0x19}},
	{0xFF,0x03,{0x98,0x81,0x00}},//Page0
	{REGFLAG_UDELAY, 10, {} },
	{0x11,0x01,{0x00}},
	{REGFLAG_UDELAY, 120, {} },
	{0x29,0x01,{0x00}},
	{REGFLAG_UDELAY, 20, {} },
	{0x35,0x01,{0x00}},
};

#if 0
static struct LCM_setting_table lcm_set_window[] = {
	{0x2A, 4, {0x00, 0x00, (FRAME_WIDTH >> 8), (FRAME_WIDTH & 0xFF)} },
	{0x2B, 4, {0x00, 0x00, (FRAME_HEIGHT >> 8), (FRAME_HEIGHT & 0xFF)} },
	{REGFLAG_END_OF_TABLE, 0x00, {} }
};
#endif
#if 0
static struct LCM_setting_table lcm_sleep_out_setting[] = {
	/* Sleep Out */
	{0xFF,0x03,{0x98,0x81,0x00}},//Page0
	{0x11, 1, {0x00} },
	{REGFLAG_DELAY, 120, {} },

	/* Display ON */
	{0x29, 1, {0x00} },
	{REGFLAG_DELAY, 20, {} },
	{REGFLAG_END_OF_TABLE, 0x00, {} }
};

static struct LCM_setting_table lcm_deep_sleep_mode_in_setting[] = {
	/* Display off sequence */
	{0xFF,0x03,{0x98,0x81,0x00}},//Page0
	{0x28, 1, {0x00} },
	{REGFLAG_DELAY, 20, {} },

	/* Sleep Mode On */
	{0x10, 1, {0x00} },
	{REGFLAG_DELAY, 120, {} },
	{REGFLAG_END_OF_TABLE, 0x00, {} }
};
#endif

//this is old way for reset tp's rst gpio
#if 0
#define GPIO_TP_RST         (GPIO174 | 0x80000000)
static void set_lcm_tp_rst_pin(unsigned int output)
{
	gpio_direction_output(174,output);
	gpio_set_value(174,output);

}
#endif

static void push_table(void *cmdq, struct LCM_setting_table *table,
	unsigned int count, unsigned char force_update)
{
	unsigned int i;
	unsigned int cmd;

	for (i = 0; i < count; i++) {
		cmd = table[i].cmd;

		switch (cmd) {

		case REGFLAG_DELAY:
			if (table[i].count <= 10)
				MDELAY(table[i].count);
			else
				MDELAY(table[i].count);
			break;

		case REGFLAG_UDELAY:
			UDELAY(table[i].count);
			break;

		case REGFLAG_END_OF_TABLE:
			break;

		default:
			dsi_set_cmdq_V22(cmdq, cmd,
				table[i].count,
				table[i].para_list,
				force_update);
		}
	}
}


static void lcm_set_util_funcs(const struct LCM_UTIL_FUNCS *util)
{
	memcpy(&lcm_util, util, sizeof(struct LCM_UTIL_FUNCS));
}


static void lcm_get_params(struct LCM_PARAMS *params)
{
	memset(params, 0, sizeof(struct LCM_PARAMS));

	params->type = LCM_TYPE_DSI;

	params->width = FRAME_WIDTH;
	params->height = FRAME_HEIGHT;
	params->physical_width = LCM_PHYSICAL_WIDTH/1000;
	params->physical_height = LCM_PHYSICAL_HEIGHT/1000;
	params->physical_width_um = LCM_PHYSICAL_WIDTH;
	params->physical_height_um = LCM_PHYSICAL_HEIGHT;
	params->density            = LCM_DENSITY;

#if (LCM_DSI_CMD_MODE)
	params->dsi.mode = CMD_MODE;
	params->dsi.switch_mode = SYNC_PULSE_VDO_MODE;
	lcm_dsi_mode = CMD_MODE;
#else
	params->dsi.mode = SYNC_PULSE_VDO_MODE;
	params->dsi.switch_mode = CMD_MODE;
	lcm_dsi_mode = SYNC_PULSE_VDO_MODE;
#endif
	LCM_LOGI("lcm_get_params lcm_dsi_mode %d\n", lcm_dsi_mode);
	params->dsi.switch_mode_enable = 0;

	/* DSI */
	/* Command mode setting */
	params->dsi.LANE_NUM = LCM_FOUR_LANE;
	/* The following defined the fomat for data coming from LCD engine. */
	params->dsi.data_format.color_order = LCM_COLOR_ORDER_RGB;
	params->dsi.data_format.trans_seq = LCM_DSI_TRANS_SEQ_MSB_FIRST;
	params->dsi.data_format.padding = LCM_DSI_PADDING_ON_LSB;
	params->dsi.data_format.format = LCM_DSI_FORMAT_RGB888;

	/* Highly depends on LCD driver capability. */
	params->dsi.packet_size = 256;
	/* video mode timing */

	params->dsi.PS = LCM_PACKED_PS_24BIT_RGB888;
 //VSYNCP:2
 //VBP:8
 //VFP:145
 //HSYNCP:5
 //HBP:35
 //HFP:35
	params->dsi.vertical_sync_active = 2;
	params->dsi.vertical_backporch = 8;
	params->dsi.vertical_frontporch = 145;
	params->dsi.vertical_frontporch_for_low_power = 310; //540;
	params->dsi.vertical_active_line = FRAME_HEIGHT;

	params->dsi.horizontal_sync_active = 5;
	params->dsi.horizontal_backporch = 35;
	params->dsi.horizontal_frontporch = 35;
	params->dsi.horizontal_active_pixel = FRAME_WIDTH;
	/*params->dsi.ssc_disable = 1;*/
#ifndef CONFIG_FPGA_EARLY_PORTING
#if (LCM_DSI_CMD_MODE)
	params->dsi.PLL_CLOCK = 250;	/* this value must be in MTK suggested table */
#else
	params->dsi.PLL_CLOCK =250;	//456 /* this value must be in MTK suggested table */
#endif
	params->dsi.PLL_CK_CMD = 250;
	params->dsi.PLL_CK_VDO = 250;//456
#else
	params->dsi.pll_div1 = 0;
	params->dsi.pll_div2 = 0;
	params->dsi.fbk_div = 0x1;
#endif
	params->dsi.clk_lp_per_line_enable = 0;
	params->dsi.esd_check_enable = 1; //if use esd will set this config
	params->dsi.customization_esd_check_enable = 1;
	params->dsi.lcm_esd_check_table[0].cmd = 0x0A;
	params->dsi.lcm_esd_check_table[0].count = 1;
	params->dsi.lcm_esd_check_table[0].para_list[0] = 0x9C;

#ifdef CONFIG_MTK_ROUND_CORNER_SUPPORT
	params->round_corner_en = 1;
	params->full_content = 0;
	params->corner_pattern_width = 720;
	params->corner_pattern_height = 32;
	params->corner_pattern_height_bot = 32;
#endif
}

static void lcm_init_power(void)
{

	display_bias_enable();

}

static void lcm_suspend_power(void)
{
	/*LCM_LOGD("lcm_suspend_power\n");*/
	//printk("ili988h lcm_suspend_power\r\n");
	//duoble tap must requst not power down
	//display_bias_disable();

	if(double_tap_work_flags == 1)
		{
      double_tap_work_flags_resume = 1;
		}
	else
		{
		
		double_tap_work_flags_resume = 0;
	}
}

static void lcm_resume_power(void)
{

	/*lcm_init_power();*/
//	printk("ili988h lcm_resume_power\r\n");
	//SET_RESET_PIN(0);

	if (double_tap_work_flags_resume == 0)
		{
	display_bias_enable();
		}
		#if 0
	if(!double_tap_work_flags){
		printk("ili988h double_tap_work_flags=false\r\n");
		display_bias_enable();
		}
	else{
		 printk("ili988h double_tap_work_flags=true\r\n");
		 MDELAY(4);
		}
		#endif
}

#define ENABLE_TP_RESET 1

#ifdef ENABLE_TP_RESET
static void set_lcm_tp_rst_pin(unsigned int output)
{
    printk("ili988h set_lcm_tp_rst_pin into\r\n");
	tpd_gpio_output(GTP_RST_PORT, output);
	 printk("ili988h set_lcm_tp_rst_pin exit\r\n");
	//gpio_direction_output(174,output);
	//gpio_set_value(174,output);
}
#endif
static void lcm_init(void)
{
	int size;

	/*LCM_LOGD("lcm_init\n");*/


	  SET_RESET_PIN(1);
#ifdef ENABLE_TP_RESET

	 set_lcm_tp_rst_pin(1);
	#endif
	  MDELAY(10);
	  SET_RESET_PIN(0);
	#ifdef ENABLE_TP_RESET
	 set_lcm_tp_rst_pin(0);
	#endif
	  MDELAY(10);
	  SET_RESET_PIN(1);
#ifdef ENABLE_TP_RESET

	 set_lcm_tp_rst_pin(1);
	#endif
	  MDELAY(60);
/*
	SET_RESET_PIN(0);
	MDELAY(15);

	SET_RESET_PIN(1);
	MDELAY(1);
	SET_RESET_PIN(0);
	MDELAY(10);

	SET_RESET_PIN(1);
	MDELAY(10);
*/
	if (lcm_dsi_mode == CMD_MODE) {
		size = sizeof(init_setting_cmd) /
			sizeof(struct LCM_setting_table);
		push_table(NULL, init_setting_cmd, size, 1);
		LCM_LOGI(
			"ili9881h-----------------lcm mode = cmd mode :%d----\n",
			lcm_dsi_mode);
	} else {
		size = sizeof(init_setting_vdo) /
			sizeof(struct LCM_setting_table);
		push_table(NULL, init_setting_vdo, size, 1);
		LCM_LOGI(
			"ili9881h-----------------lcm mode = vdo mode :%d----\n",
			lcm_dsi_mode);
	}
}


static void lcm_suspend(void)
{
	/*LCM_LOGD("lcm_suspend\n");*/
	printk("DJ lcm_suspend\n");
	core_config_sense_ctrl(false);
	MDELAY(10);
	push_table(NULL, lcm_suspend_setting,
		sizeof(lcm_suspend_setting) / sizeof(struct LCM_setting_table),
		1);
	MDELAY(10);
}

static void lcm_resume(void)
{
	/*LCM_LOGD("lcm_resume\n");*/
//	printk("ili988h lcm_resume\r\n");
	lcm_init();
}



/* return TRUE: need recovery */
/* return FALSE: No need recovery */
static unsigned int lcm_esd_check(void)
{
#ifndef BUILD_LK
	char buffer[3];
	int array[4];

	array[0] = 0x00013700;
	dsi_set_cmdq(array, 1, 1);

	//read_reg_v2(0x53, buffer, 1);
	read_reg_v2(0x0A, buffer, 1);

	if (buffer[0] != 0x9C) {
	//if (buffer[0] != 0x24) {
		printk("[LCM ERROR] [0x0A]=0x%02x\n", buffer[0]);
		return TRUE;
	}
		printk("[LCM NORMAL] [0x0A]=0x%02x\n", buffer[0]);
	return FALSE;
#else
	return FALSE;
#endif

}

#if 0

static unsigned int lcm_ata_check(unsigned char *buffer)
{
#ifndef BUILD_LK
	unsigned int ret = 0;
	unsigned int x0 = FRAME_WIDTH / 4;
	unsigned int x1 = FRAME_WIDTH * 3 / 4;

	unsigned char x0_MSB = ((x0 >> 8) & 0xFF);
	unsigned char x0_LSB = (x0 & 0xFF);
	unsigned char x1_MSB = ((x1 >> 8) & 0xFF);
	unsigned char x1_LSB = (x1 & 0xFF);

	unsigned int data_array[3];
	unsigned char read_buf[4];

	LCM_LOGI("ATA check size = 0x%x,0x%x,0x%x,0x%x\n", x0_MSB, x0_LSB, x1_MSB, x1_LSB);
	data_array[0] = 0x0005390A;	/* HS packet */
	data_array[1] = (x1_MSB << 24) | (x0_LSB << 16) | (x0_MSB << 8) | 0x2a;
	data_array[2] = (x1_LSB);
	dsi_set_cmdq(data_array, 3, 1);

	data_array[0] = 0x00043700;	/* read id return two byte,version and id */
	dsi_set_cmdq(data_array, 1, 1);

	read_reg_v2(0x2A, read_buf, 4);

	if ((read_buf[0] == x0_MSB) && (read_buf[1] == x0_LSB)
	    && (read_buf[2] == x1_MSB) && (read_buf[3] == x1_LSB))
		ret = 1;
	else
		ret = 0;

	x0 = 0;
	x1 = FRAME_WIDTH - 1;

	x0_MSB = ((x0 >> 8) & 0xFF);
	x0_LSB = (x0 & 0xFF);
	x1_MSB = ((x1 >> 8) & 0xFF);
	x1_LSB = (x1 & 0xFF);

	data_array[0] = 0x0005390A;	/* HS packet */
	data_array[1] = (x1_MSB << 24) | (x0_LSB << 16) | (x0_MSB << 8) | 0x2a;
	data_array[2] = (x1_LSB);
	dsi_set_cmdq(data_array, 3, 1);
	return ret;
#else
	return 0;
#endif
}

#endif 


#ifdef USE_CABC_FUNC
static struct LCM_setting_table bl_level[] = {
	{0x51, 1, {0xFF} },
	{REGFLAG_END_OF_TABLE, 0x00, {} }
};

static void lcm_setbacklight_cmdq(void *handle, unsigned int level)
{

	LCM_LOGI("%s,ili9881h backlight: level = %d\n", __func__, level);

	bl_level[0].para_list[0] = level;

	push_table(handle,
		bl_level,
		sizeof(bl_level) / sizeof(struct LCM_setting_table),
		1);
}
#endif



static void *lcm_switch_mode(int mode)
{
#ifndef BUILD_LK
	/* customization: 1. V2C config 2 values, C2V config 1 value; 2. config mode control register */
	if (mode == 0) {    /* V2C */
		lcm_switch_mode_cmd.mode = CMD_MODE;
		lcm_switch_mode_cmd.addr = 0xBB;    /* mode control addr */
		lcm_switch_mode_cmd.val[0] = 0x13;  /* enabel GRAM firstly, ensure writing one frame to GRAM */
		lcm_switch_mode_cmd.val[1] = 0x10;  /* disable video mode secondly */
	} else {        /* C2V */
		lcm_switch_mode_cmd.mode = SYNC_PULSE_VDO_MODE;
		lcm_switch_mode_cmd.addr = 0xBB;
		lcm_switch_mode_cmd.val[0] = 0x03;  /* disable GRAM and enable video mode */
	}
	return (void *)(&lcm_switch_mode_cmd);
#else
	return NULL;
#endif
}


struct LCM_DRIVER ili9881h_hd720plus_dsi_vdo_dj_lcm_drv = {
	.name = "ili9881h_hd720plus_dsi_vdo_dj",
	.set_util_funcs = lcm_set_util_funcs,
	.get_params = lcm_get_params,
	.init = lcm_init,
	.suspend = lcm_suspend,
	.resume = lcm_resume,
	//.compare_id = lcm_compare_id,
	.init_power = lcm_init_power,
	.resume_power = lcm_resume_power,
	.suspend_power = lcm_suspend_power,
	.esd_check = lcm_esd_check,

	//.set_backlight = lcm_setbacklight,
	#ifdef USE_CABC_FUNC
	//.set_backlight_cmdq =lcm_setbacklight_cmdq,
	#endif
	//.ata_check = lcm_ata_check,
	//.update = lcm_update,
	.switch_mode = lcm_switch_mode,
};
