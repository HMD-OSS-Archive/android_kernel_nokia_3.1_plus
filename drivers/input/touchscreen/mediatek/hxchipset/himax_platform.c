/*  Himax Android Driver Sample Code for MTK kernel 4.4 platform

    Copyright (C) 2018 Himax Corporation.

    This software is licensed under the terms of the GNU General Public
    License version 2, as published by the Free Software Foundation, and
    may be copied, distributed, and modified under those terms.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

*/
#include <linux/spi/spi.h>
#include <linux/spi/spidev.h>

#include "himax_platform.h"
#include "himax_common.h"

/* MTK header */
#ifndef CONFIG_SPI_MT65XX
#include "mtk_spi.h"
#include "mtk_spi_hal.h"
#endif
//#include "mtk_gpio.h"
//#include "mach/gpio_const.h"
/*#include "mt_spi.h"
#include "mt_spi_hal.h"
#include "mt_gpio.h"
#include "mach/gpio_const.h"*/

bool spi_dev_detected = false;
int i2c_error_count = 0;
int irq_enable_count = 0;
#ifdef CONFIG_SPI_MT65XX
	u32 hx_spi_speed = 1*1000000;
#endif
extern int display_bias_disable(void);
DEFINE_MUTEX(hx_wr_access);

MODULE_DEVICE_TABLE(of, himax_match_table);
struct of_device_id himax_match_table[] = {
	{.compatible = "mediatek,cap_touch_spi" },//wangbin modify
	{.compatible = "mediatek,cap_touch" },
	{},
};

//static unsigned bufsiz = (15 * 1024);
static int himax_tpd_int_gpio = 0;
unsigned int himax_touch_irq = 0;
unsigned int himax_tpd_rst_gpio_number = 174;
unsigned int himax_tpd_int_gpio_number = 0;

u8 *gpDMABuf_va = NULL;
u8 *gpDMABuf_pa = NULL;


/* Custom set some config */
static int hx_panel_coords[4] = {0,720,0,1440};//[1]=X resolution, [3]=Y resolution
static int hx_display_coords[4] = {0,720,0,1440};
static int report_type = PROTOCOL_TYPE_B;

struct i2c_client *i2c_client_point = NULL;

extern struct himax_ic_data *ic_data;
extern struct himax_ts_data *private_ts;

extern int himax_chip_common_init(void);
extern void himax_chip_common_deinit(void);

extern void himax_ts_work(struct himax_ts_data *ts);
extern enum hrtimer_restart himax_ts_timer_func(struct hrtimer *timer);

/******* SPI-start *******/
struct mutex             	hx_spi_lock;

static struct spi_device	*hx_spi;
#ifndef CONFIG_SPI_MT65XX
static struct mt_chip_conf 	hx_spi_mcc;
#endif
#if 0//Himax Test
struct mtk_chip_config 		hx_spi_mcc;
#endif
static int 				hx_irq;
//static u8 			*hx_spi_buffer;  /* only used for SPI transfer internal */
/******* SPI-end *******/

#ifndef CONFIG_SPI_MT65XX
struct mt_chip_conf spi_ctrdata = {
	.setuptime = 10,
	.holdtime = 10,
	.high_time = 50, /* 1MHz */
	.low_time = 50,
	.cs_idletime = 10,
	.ulthgh_thrsh = 0,

	.cpol = SPI_CPOL_0,
	.cpha = SPI_CPHA_0,

	.rx_mlsb = SPI_MSB,
	.tx_mlsb = SPI_MSB,

	.tx_endian = SPI_LENDIAN,
	.rx_endian = SPI_LENDIAN,

	.com_mod = FIFO_TRANSFER,
	/* .com_mod = DMA_TRANSFER, */

	.pause = 0,
	.finish_intr = 1,
	.deassert = 0,
	.ulthigh = 0,
	.tckdly = 0,
};
#endif

#if 0//Himax Test
const struct mtk_chip_config spi_ctrdata = {
    .rx_mlsb = 1,
    .tx_mlsb = 1,
    .cs_pol = 0,
};
#endif

#if defined(HX_PLATFOME_DEFINE_KEY)
/*In MT6797 need to set 1 into use-tpd-button in dts kernel-3.18\arch\arm64\boot\dts\amt6797_evb_m.dts*/
/*key_range : [keyindex][key_data] {..{x,y}..}*/
static int key_range[3][2]= {{90,883},{230,883},{370,883}};
#endif

int himax_dev_set(struct himax_ts_data *ts)
{
	ts->input_dev = tpd->dev;
	return NO_ERR;
}
int himax_input_register_device(struct input_dev *input_dev)
{
	return NO_ERR;
}

#if defined(HX_PLATFOME_DEFINE_KEY)
void himax_platform_key(void)
{
	int idx = 0;

	if (tpd_dts_data.use_tpd_button) {
		for (idx = 0; idx < tpd_dts_data.tpd_key_num; idx++) {
			input_set_capability(tpd->dev, EV_KEY, tpd_dts_data.tpd_key_local[idx]);
			I("[%d]key:%d\n", idx, tpd_dts_data.tpd_key_local[idx]);
		}
	}
}
/* report coordinates to system and system will transfer it into Key */
static void himax_vk_parser(struct himax_i2c_platform_data *pdata, int key_num)
{
	int i = 0;
	struct himax_virtual_key *vk;
	uint8_t key_index = 0;
	vk = kzalloc(key_num * (sizeof *vk), GFP_KERNEL);

	for (key_index = 0; key_index < key_num ; key_index++) {
		/* index: def in our driver */
		vk[key_index].index = key_index + 1;
		/* key size */
		vk[key_index].x_range_min = key_range[key_index][0], vk[key_index].x_range_max = key_range[key_index][0];
		vk[key_index].y_range_min = key_range[key_index][1], vk[key_index].y_range_max = key_range[key_index][1];
	}

	pdata->virtual_key = vk;

	for (i = 0 ; i < key_num; i++) {
		I(" vk[%d] idx:%d x_min:%d, y_max:%d\n", i, pdata->virtual_key[i].index, pdata->virtual_key[i].x_range_min, pdata->virtual_key[i].y_range_max);
	}
}
#else
void himax_vk_parser(struct device_node *dt,
						struct himax_i2c_platform_data *pdata)
{
	u32 data = 0;
	uint8_t cnt = 0, i = 0;
	uint32_t coords[4] = {0};
	struct device_node *node, *pp = NULL;
	struct himax_virtual_key *vk;
	node = of_parse_phandle(dt, "virtualkey", 0);

	if (node == NULL) {
		I(" DT-No vk info in DT\n");
		return;
	} else {
		while ((pp = of_get_next_child(node, pp)))
			cnt++;

		if (!cnt)
			return;

		vk = kzalloc(cnt * (sizeof *vk), GFP_KERNEL);
		pp = NULL;

		while ((pp = of_get_next_child(node, pp))) {
			if (of_property_read_u32(pp, "idx", &data) == 0)
				vk[i].index = data;

			if (of_property_read_u32_array(pp, "range", coords, 4) == 0) {
				vk[i].x_range_min = coords[0], vk[i].x_range_max = coords[1];
				vk[i].y_range_min = coords[2], vk[i].y_range_max = coords[3];
			} else {
				I(" range faile\n");
			}

			i++;
		}

		pdata->virtual_key = vk;

		for (i = 0; i < cnt; i++)
			I(" vk[%d] idx:%d x_min:%d, y_max:%d\n", i, pdata->virtual_key[i].index,
			  pdata->virtual_key[i].x_range_min, pdata->virtual_key[i].y_range_max);
	}
}
#endif
int himax_parse_dt(struct himax_ts_data *ts, struct himax_i2c_platform_data *pdata)
{
	struct device_node *dt = NULL;
	struct i2c_client *client =  NULL;
	if(spi_dev_detected)
		dt = ts->dev->of_node;
	else {
		dt = ts->client->dev.of_node;
		client = ts->client;
	}
	I("%s: Entering!\n", __func__);
	if (dt) {
		const struct of_device_id *match;
		if(spi_dev_detected)
			match = of_match_device(of_match_ptr(himax_match_table), ts->dev);
		else 
			match = of_match_device(of_match_ptr(himax_match_table), &client->dev);

		if (!match) {
			TPD_DMESG("[Himax]Error: No device match found\n");
			return -ENODEV;
		}
	}

	/*  pdata->gpio_reset = of_get_named_gpio(dev->of_node, "rst-gpio", 0);
	    pdata->gpio_irq = of_get_named_gpio(dev->of_node, "int-gpio", 0);
	    I("pdata->gpio_reset: %d\n", pdata->gpio_reset );
	    I("pdata->gpio_irq: %d\n", pdata->gpio_irq ); */
	/* himax_tpd_rst_gpio_number = of_get_named_gpio(dt, "rst-gpio", 0); */
	/* himax_tpd_int_gpio_number = of_get_named_gpio(dt, "int-gpio", 0); */
	/* It will be a non-zero and non-one value for MTK PINCTRL API */
	himax_tpd_rst_gpio_number = GTP_RST_PORT;
	himax_tpd_int_gpio_number = GTP_INT_PORT;
	pdata->gpio_reset	= himax_tpd_rst_gpio_number;
	pdata->gpio_irq		= himax_tpd_int_gpio_number;
	I("%s: int : %2.2x\n", __func__, pdata->gpio_irq);
	I("%s: rst : %2.2x\n", __func__, pdata->gpio_reset);
#if defined(HX_PLATFOME_DEFINE_KEY)
	/* now 3 keys */
	himax_vk_parser(pdata, 3);
#else
	himax_vk_parser(dt, pdata);
#endif
	/* Set device tree data */
	/* Set panel coordinates */
	pdata->abs_x_min = hx_panel_coords[0], pdata->abs_x_max = hx_panel_coords[1];
	pdata->abs_y_min = hx_panel_coords[2], pdata->abs_y_max = hx_panel_coords[3];
	I(" %s:panel-coords = %d, %d, %d, %d\n", __func__, pdata->abs_x_min,
	  pdata->abs_x_max, pdata->abs_y_min, pdata->abs_y_max);
	/* Set display coordinates */
	pdata->screenWidth  = hx_display_coords[1];
	pdata->screenHeight = hx_display_coords[3];
	I(" %s:display-coords = (%d, %d)\n", __func__, pdata->screenWidth,
	  pdata->screenHeight);
	/* report type */
	pdata->protocol_type = report_type;
	return 0;
}

static ssize_t himax_spi_sync(struct spi_message *message)
{
	int status;

	status = spi_sync(hx_spi, message);

	if (status == 0) {
		status = message->status;
		if (status == 0)
			status = message->actual_length;
	}
	return status;
}

static int himax_spi_read(uint8_t *command, uint8_t command_len, uint8_t *data, uint32_t length, uint8_t toRetry)
{
	struct spi_message message;
	struct spi_transfer xfer[2];
	int retry = 0;
	int error = -1;

	spi_message_init(&message);
	memset(xfer, 0, sizeof(xfer));

	xfer[0].tx_buf = command;
	xfer[0].len = command_len;
	spi_message_add_tail(&xfer[0], &message);

	xfer[1].tx_buf = data; 
	xfer[1].rx_buf = data;
	xfer[1].len = length;
	spi_message_add_tail(&xfer[1], &message);

	for (retry = 0; retry < toRetry; retry++) {
		error = spi_sync(hx_spi, &message);
		if (error) {
			E("SPI read error: %d\n", error);
		} else{
			break;
		}
	}
	if (retry == toRetry) {
		E("%s: SPI read error retry over %d\n",
			__func__, toRetry);
		return -EIO;
	}

	return 0;
}

static int himax_spi_write(uint8_t *buf, uint32_t length)
{

	struct spi_transfer	t = {
			.tx_buf		= buf,
			.len		= length,
	};
	struct spi_message	m;
	spi_message_init(&m);
	spi_message_add_tail(&t, &m);

	return himax_spi_sync(&m);

}

#ifdef MTK_I2C_DMA
int himax_bus_read(uint8_t command, uint8_t *data, uint8_t length, uint8_t toRetry)
{
	int ret = 0;
	s32 retry = 0;
	u8 buffer[1];
	uint8_t spi_format_buf[3];
	struct i2c_msg msg[2];
	struct i2c_client *client =NULL;
	if(spi_dev_detected){
		mutex_lock(&hx_spi_lock);
		spi_format_buf[0] = 0xF3;
		spi_format_buf[1] = command;
		spi_format_buf[2] = 0x00;
		
		ret = himax_spi_read(&spi_format_buf[0], 3, data, length, 10);
		mutex_unlock(&hx_spi_lock);

	}
	else {
		client = private_ts->client;
		msg[0].addr = (client->addr & I2C_MASK_FLAG);
		msg[0].flags = 0;
		msg[0].buf = buffer;
		msg[0].len = 1;
		msg[0].timing = 400;

		msg[1].addr = (client->addr & I2C_MASK_FLAG);
		msg[1].ext_flag = (client->ext_flag | I2C_ENEXT_FLAG | I2C_DMA_FLAG);
		msg[1].flags = I2C_M_RD;
		msg[1].buf = gpDMABuf_pa;
		msg[1].len = length;
		msg[1].timing = 400;				
		/*
		msg[0] = {
				.addr = (client->addr & I2C_MASK_FLAG),
				.flags = 0,
				.buf = buffer,
				.len = 1,
				.timing = 400
		};
		msg[1] = {
				.addr = (client->addr & I2C_MASK_FLAG),
				.ext_flag = (client->ext_flag | I2C_ENEXT_FLAG | I2C_DMA_FLAG),
				.flags = I2C_M_RD,
				.buf = gpDMABuf_pa,
				.len = length,
				.timing = 400
		};
		*/
		mutex_lock(&hx_wr_access);
		buffer[0] = command;

		if (data == NULL) {
			mutex_unlock(&hx_wr_access);
			return -EFAULT;
		}

		for (retry = 0; retry < toRetry; ++retry) {
			ret = i2c_transfer(client->adapter, &msg[0], 2);

			if (ret < 0) {
				continue;
			}

			memcpy(data, gpDMABuf_va, length);
			mutex_unlock(&hx_wr_access);
			return 0;
		}

		E("Dma I2C Read Error: %d byte(s), err-code: %d\n", length, ret);
		i2c_error_count = toRetry;
		mutex_unlock(&hx_wr_access);
		}

	return ret;
}

int himax_bus_write(uint8_t command, uint8_t *buf, uint8_t len, uint8_t toRetry)
{
	int rc = 0, retry = 0;
	int i = 0;
	u8 *pWriteData = NULL;
	struct i2c_msg msg[1];	
	struct i2c_client *client = NULL;
	uint8_t spi_format_buf[length + 2];
	if(spi_dev_detected){
		mutex_lock(&hx_spi_lock);
		spi_format_buf[0] = 0xF2;
		spi_format_buf[1] = command;

		for (i = 0; i < len; i++)
			spi_format_buf[i + 2] = buf[i];

		rc = himax_spi_write(spi_format_buf, len + 2);
		mutex_unlock(&hx_spi_lock);
	}
	else{
		pWriteData = gpDMABuf_va;
		client = private_ts->client;
		msg[0].addr = (client->addr & I2C_MASK_FLAG);
		msg[0].ext_flag =(client->ext_flag | I2C_ENEXT_FLAG | I2C_DMA_FLAG); 
		msg[0].flags = 0;
		msg[0].buf = gpDMABuf_pa;
		msg[0].len = len + 1;
		msg[0].timing =400; 		
		/*
		msg[0] = {
				(client->addr & I2C_MASK_FLAG),//msg[0].addr = 
				(client->ext_flag | I2C_ENEXT_FLAG | I2C_DMA_FLAG),//msg[0].ext_flag = 
				0,//msg[0].flags = 
				gpDMABuf_pa,//msg[0].buf = 
				len + 1,//msg[0].len = 
				400//msg[0].timing = 
		};
		*/
		mutex_lock(&hx_wr_access);

		if (!pWriteData) {
			E("dma_alloc_coherent failed!\n");
			mutex_unlock(&hx_wr_access);
			return -EFAULT;
		}

		gpDMABuf_va[0] = command;
		memcpy(gpDMABuf_va + 1, buf, len);

		for (retry = 0; retry < toRetry; ++retry) {
			rc = i2c_transfer(client->adapter, &msg[0], 1);

			if (rc < 0) {
				continue;
			}

			mutex_unlock(&hx_wr_access);
			return 0;
		}

		E("Dma I2C master write Error: %d byte(s), err-code: %d\n", len, rc);
		i2c_error_count = toRetry;
		mutex_unlock(&hx_wr_access);
	}
	return rc;
}

int himax_bus_write_command(uint8_t command, uint8_t toRetry)
{
	return himax_bus_write(command, NULL, 0, toRetry);
}

int himax_bus_master_write(uint8_t *buf, uint8_t len, uint8_t toRetry)
{
	int rc = 0, retry = 0;
	u8 *pWriteData = gpDMABuf_va;
	struct i2c_client *client = private_ts->client;
	struct i2c_msg msg[] = {
		{
			.addr = (client->addr & I2C_MASK_FLAG),
			.ext_flag = (client->ext_flag | I2C_ENEXT_FLAG | I2C_DMA_FLAG),
			.flags = 0,
			.buf = gpDMABuf_pa,
			.len = len,
			.timing = 400
		},
	};
	mutex_lock(&hx_wr_access);

	if (!pWriteData) {
		E("dma_alloc_coherent failed!\n");
		mutex_unlock(&hx_wr_access);
		return -EFAULT;
	}

	memcpy(gpDMABuf_va, buf, len);

	for (retry = 0; retry < toRetry; ++retry) {
		rc = i2c_transfer(client->adapter, &msg[0], 1);

		if (rc < 0) {
			continue;
		}

		mutex_unlock(&hx_wr_access);
		return 0;
	}

	E("Dma I2C master write Error: %d byte(s), err-code: %d\n", len, rc);
	i2c_error_count = toRetry;
	mutex_unlock(&hx_wr_access);
	return rc;
}

#else
int himax_bus_read(uint8_t command, uint8_t *data, uint8_t length, uint8_t toRetry)
{
	int retry = 0;
	int result = 0;
	uint8_t spi_format_buf[3];
	struct i2c_msg msg[2];	
	struct i2c_client *client = NULL;
	if(spi_dev_detected){
		mutex_lock(&hx_spi_lock);
		spi_format_buf[0] = 0xF3;
		spi_format_buf[1] = command;
		spi_format_buf[2] = 0x00;

		result = himax_spi_read(&spi_format_buf[0], 3, data, length, 10);
		mutex_unlock(&hx_spi_lock);
		
		return result;
	}
	else{
		client = private_ts->client;
		msg[0].addr = client->addr;
		msg[0].flags= 0; 
		msg[0].len= 1;
		msg[0].buf = &command; 
		
		msg[1] .addr =client->addr;
		msg[1] .flags = I2C_M_RD;
		msg[1] .len =length; 
		msg[1] .buf =data; 	
		/*
		msg[0] = {

				client->addr,//.addr = 
				= 0,//.flags 
				= 1,//.len 
				&command,//.buf = 
		};
		msg[1] = {
				client->addr,//.addr = 
				I2C_M_RD,//.flags = 
				length,//.len = 
				data,//.buf = 
		};
		*/
		for (retry = 0; retry < toRetry; retry++) {
			if (i2c_transfer(client->adapter, msg, 2) == 2)
				break;

			msleep(10);
		}

		if (retry == toRetry) {
			E("%s: i2c_read_block retry over %d\n",
			  __func__, toRetry);
			i2c_error_count = toRetry;
			return -EIO;
		}

		return 0;
	}
}

int himax_bus_write(uint8_t command, uint8_t *data, uint8_t length, uint8_t toRetry)
{
	int retry/*, loop_i*/;
	int i = 0;
	int result = 0;
	uint8_t buf[length + 1];
	struct i2c_msg msg[1];
	struct i2c_client *client = NULL;
	uint8_t spi_format_buf[length + 2];
	if(spi_dev_detected){
		mutex_lock(&hx_spi_lock);
		spi_format_buf[0] = 0xF2;
		spi_format_buf[1] = command;
		
		for (i = 0; i < length; i++)
			spi_format_buf[i + 2] = data[i];
		
		result = himax_spi_write(spi_format_buf, length + 2);
		mutex_unlock(&hx_spi_lock);
		return result;

	}
	else{
		client = private_ts->client;
		msg[0].addr =client->addr;
		msg[0].flags =0; 
		msg[0].len =length + 1;
		msg[0].buf = buf;		
		/*
		msg[0] = {
				client->addr,//.addr = 
				0,//.flags = 
				length + 1,//.len = 
				buf,//.buf = 
		};
		*/
		buf[0] = command;
		memcpy(buf + 1, data, length);

		for (retry = 0; retry < toRetry; retry++) {
			if (i2c_transfer(client->adapter, msg, 1) == 1)
				break;

			msleep(10);
		}

		if (retry == toRetry) {
			E("%s: i2c_write_block retry over %d\n",
			  __func__, toRetry);
			i2c_error_count = toRetry;
			return -EIO;
		}

		return 0;
	}
}

int himax_bus_write_command(uint8_t command, uint8_t toRetry)
{
	return himax_bus_write(command, NULL, 0, toRetry);
}

int himax_bus_master_write(uint8_t *data, uint8_t length, uint8_t toRetry)
{
	int retry/*, loop_i*/;
	uint8_t buf[length];
	struct i2c_client *client = private_ts->client;
	struct i2c_msg msg[] = {
		{
			.addr = client->addr,
			.flags = 0,
			.len = length,
			.buf = buf,
		}
	};
	memcpy(buf, data, length);

	for (retry = 0; retry < toRetry; retry++) {
		if (i2c_transfer(client->adapter, msg, 1) == 1)
			break;

		msleep(10);
	}

	if (retry == toRetry) {
		E("%s: i2c_write_block retry over %d\n",
		  __func__, toRetry);
		i2c_error_count = toRetry;
		return -EIO;
	}

	return 0;
}
#endif

#if 0
int himax_bus_read(uint8_t command, uint8_t *data, uint8_t length, uint8_t toRetry)
{
	int result = 0;
	uint8_t spi_format_buf[3];

	mutex_lock(&hx_spi_lock);
	spi_format_buf[0] = 0xF3;
	spi_format_buf[1] = command;
	spi_format_buf[2] = 0x00;

	result = himax_spi_read(&spi_format_buf[0], 3, data, length, 10);
	mutex_unlock(&hx_spi_lock);
	

	return result;
}

int himax_bus_write(uint8_t command, uint8_t *data, uint8_t length, uint8_t toRetry)
{
	uint8_t spi_format_buf[length + 2];
	int i = 0;
	int result = 0;

	mutex_lock(&hx_spi_lock);
	spi_format_buf[0] = 0xF2;
	spi_format_buf[1] = command;

	for (i = 0; i < length; i++)
		spi_format_buf[i + 2] = data[i];

	result = himax_spi_write(spi_format_buf, length + 2);
	mutex_unlock(&hx_spi_lock);
	
	

	return result;
}


int himax_bus_master_write(uint8_t *data, uint8_t length, uint8_t toRetry)
{
	struct spi_message msg;
	struct spi_transfer *xfer = NULL;
	u8 *tmp_buf = NULL;
	u32 package, reminder, retry;

	package = (length + 1) / 1024;
	reminder = (length + 1) % 1024;

	if ((package > 0) && (reminder != 0)) {
		xfer = kzalloc(sizeof(*xfer) * 2, GFP_KERNEL);
		retry = 1;
	} else {
		xfer = kzalloc(sizeof(*xfer), GFP_KERNEL);
		retry = 0;
	}
	if (xfer == NULL) {
		E("%s, no memory for SPI transfer\n", __func__);
		return -ENOMEM;
	}
	tmp_buf = hx_spi_buffer;

	/* switch to DMA mode if transfer length larger than 32 bytes */
#ifndef CONFIG_SPI_MT65XX
	if ((length + 1) > 32) {
		hx_spi_mcc.com_mod = DMA_TRANSFER;
		spi_setup(hx_spi);
	}
#endif
	spi_message_init(&msg);
	*tmp_buf = 0xF2;
	if (retry) {
		memcpy(tmp_buf + 1, data, (package * 1024 - 1));
		xfer[0].len = package * 1024;
	} else {
		memcpy(tmp_buf + 1, data, length);
		xfer[0].len = length + 1;
	}
#ifdef CONFIG_SPI_MT65XX
		xfer[0].speed_hz = hx_spi_speed;
#endif
	xfer[0].tx_buf = tmp_buf;
	xfer[0].delay_usecs = 5;
	spi_message_add_tail(&xfer[0], &msg);
	spi_sync(hx_spi, &msg);

	if (retry) {
		spi_message_init(&msg);
		*tmp_buf = 0xF2;
		memcpy(tmp_buf + 1, (data + package * 1024 - 1), reminder);
		xfer[1].tx_buf = tmp_buf;
		xfer[1].len = reminder + 1;
#ifdef CONFIG_SPI_MT65XX
		xfer[1].speed_hz = hx_spi_speed;
#endif
		xfer[1].delay_usecs = 5;
		spi_message_add_tail(&xfer[1], &msg);
		spi_sync(hx_spi, &msg);
	}

	/* restore to FIFO mode if has used DMA */
#ifndef CONFIG_SPI_MT65XX
	if ((length + 1) > 32) {
		hx_spi_mcc.com_mod = FIFO_TRANSFER;
		spi_setup(hx_spi);
	}
#endif
	kfree(xfer);
	if (xfer != NULL)
		xfer = NULL;

	return 0;
}
#endif

uint8_t himax_int_gpio_read(int pinnum)
{
	return  gpio_get_value(himax_tpd_int_gpio);
}

void himax_int_enable(int enable)
{
	int irqnum = 0;
	irqnum = hx_irq;
	I("%s: Entering!\n", __func__);

	if (enable == 1 && irq_enable_count == 0) {
		enable_irq(irqnum);
		irq_enable_count++;
		private_ts->irq_enabled = 1;
	} else if (enable == 0 && irq_enable_count == 1) {
		disable_irq_nosync(irqnum);
		irq_enable_count--;
		private_ts->irq_enabled = 0;
	}

	I("irq_enable_count = %d\n", irq_enable_count);
}

#ifdef HX_RST_PIN_FUNC
void himax_rst_gpio_set(int pinnum, uint8_t value)
{
	if (value)
		tpd_gpio_output(himax_tpd_rst_gpio_number, 1);
	else
		tpd_gpio_output(himax_tpd_rst_gpio_number, 0);
}
#endif

int himax_gpio_power_config(struct himax_i2c_platform_data *pdata)
{
	int error = 0;
	error = regulator_enable(tpd->reg);

	if (error != 0)
		TPD_DMESG("Failed to enable reg-vgp6: %d\n", error);

	msleep(100);
#ifdef HX_RST_PIN_FUNC
	tpd_gpio_output(himax_tpd_rst_gpio_number, 1);
	msleep(20);
	tpd_gpio_output(himax_tpd_rst_gpio_number, 0);
	msleep(20);
	tpd_gpio_output(himax_tpd_rst_gpio_number, 1);
#endif
	TPD_DMESG("mtk_tpd: himax reset over \n");
	/* set INT mode */
	tpd_gpio_as_int(himax_tpd_int_gpio_number);
	return 0;
}

static void himax_ts_isr_func(struct himax_ts_data *ts)
{
	himax_ts_work(ts);
}

irqreturn_t himax_ts_thread(int irq, void *ptr)
{

	printk("interrupt enter\r\n");
	himax_ts_isr_func((struct himax_ts_data *)ptr);

	return IRQ_HANDLED;
}

static void himax_ts_work_func(struct work_struct *work)
{
	struct himax_ts_data *ts = container_of(work, struct himax_ts_data, work);
	himax_ts_work(ts);
}

int himax_int_register_trigger(void)
{
	int ret = NO_ERR;
	struct himax_ts_data *ts = private_ts;

	if(1){
	//if (ic_data->HX_INT_IS_EDGE) {
		I("this is HX_INT_IS_EDGE\r\n");
		ret = request_threaded_irq(hx_irq, NULL, himax_ts_thread,
									IRQF_TRIGGER_FALLING | IRQF_ONESHOT, HIMAX_common_NAME, ts);
	} else {
		I("this is IRQF_TRIGGER_LOW\r\n");
		ret = request_threaded_irq(hx_irq, NULL, himax_ts_thread,
									IRQF_TRIGGER_LOW | IRQF_ONESHOT, HIMAX_common_NAME, ts);
	}

	return ret;
}

int himax_int_en_set(void)
{
	int ret = NO_ERR;
	ret = himax_int_register_trigger();
	return ret;
}

int himax_ts_register_interrupt(void)
{
	struct himax_ts_data *ts = private_ts;
	struct i2c_client *client = NULL;
	struct device_node *node = NULL;
	u32 ints[2] = {0, 0};
	int ret = 0;
	
	if(!spi_dev_detected)
		client = private_ts->client;
	
	node = of_find_matching_node(node, touch_of_match);
	if (node) {
		of_property_read_u32_array(node, "debounce", ints, ARRAY_SIZE(ints));
		gpio_set_debounce(ints[0], ints[1]);
		himax_touch_irq = irq_of_parse_and_map(node, 0);
		I("himax_touch_irq=%ud \n", himax_touch_irq);
		if(!spi_dev_detected){
			client->irq = himax_touch_irq;
			ts->client->irq = himax_touch_irq;
		}
		hx_irq = himax_touch_irq;
	} else {
		I("[%s] tpd request_irq can not find touch eint device node!\n", __func__);
	}

	ts->irq_enabled = 0;
	ts->use_irq = 0;

	/* Work functon */
	if (hx_irq) {/*INT mode*/
		ts->use_irq = 1;
		ret = himax_int_register_trigger();

		if (ret == 0) {
			ts->irq_enabled = 1;
			irq_enable_count = 1;
			I("%s: irq enabled at qpio: %d\n", __func__, hx_irq);
#ifdef HX_SMART_WAKEUP
			irq_set_irq_wake(hx_irq, 1);
#endif
		} else {
			ts->use_irq = 0;
			E("%s: request_irq failed\n", __func__);
		}
	} else {
		I("%s: hx_irq is empty, use polling mode.\n", __func__);
	}

	if (!ts->use_irq) {/*if use polling mode need to disable HX_ESD_RECOVERY function*/
		ts->himax_wq = create_singlethread_workqueue("himax_touch");
		INIT_WORK(&ts->work, himax_ts_work_func);
		hrtimer_init(&ts->timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
		ts->timer.function = himax_ts_timer_func;
		hrtimer_start(&ts->timer, ktime_set(1, 0), HRTIMER_MODE_REL);
		I("%s: polling mode enabled\n", __func__);
	}

	return ret;
}

#define GPIO_SPI_CS_PIN (GPIO26)  
#define GPIO_SPI_SCK_PIN (GPIO28)  
#define GPIO_SPI_MISO_PIN (GPIO25)  
#define GPIO_SPI_MOSI_PIN (GPIO27)  

/*
void himax_hw_init(void)  
{  
    mt_set_gpio_mode(GPIO_SPI_CS_PIN, 1);  
    mt_set_gpio_dir(GPIO_SPI_CS_PIN,GPIO_DIR_OUT);  
    mt_set_gpio_out(GPIO_SPI_CS_PIN, GPIO_OUT_ONE);  
  
    mt_set_gpio_mode(GPIO_SPI_SCK_PIN, 1);  
    mt_set_gpio_dir(GPIO_SPI_SCK_PIN,GPIO_DIR_OUT);  
    mt_set_gpio_pull_enable(GPIO_SPI_SCK_PIN, GPIO_PULL_ENABLE);  
   mt_set_gpio_pull_select(GPIO_SPI_SCK_PIN, GPIO_PULL_UP);  
  
    mt_set_gpio_mode(GPIO_SPI_MISO_PIN, 1);  
    mt_set_gpio_dir(GPIO_SPI_MISO_PIN,GPIO_DIR_IN);  
    mt_set_gpio_pull_enable(GPIO_SPI_MISO_PIN, GPIO_PULL_ENABLE);  
   mt_set_gpio_pull_select(GPIO_SPI_MISO_PIN, GPIO_PULL_UP);  
  
    mt_set_gpio_mode(GPIO_SPI_MOSI_PIN, 1);  
    mt_set_gpio_dir(GPIO_SPI_MOSI_PIN,GPIO_DIR_OUT);  
    mt_set_gpio_pull_enable(GPIO_SPI_MOSI_PIN, GPIO_PULL_ENABLE);  
   mt_set_gpio_pull_select(GPIO_SPI_MOSI_PIN, GPIO_PULL_UP);  
      
    printk("[HXTP] test Set Gpio Ok\n");  
}  
*/

static int __init himax_common_probe_spi(struct spi_device *spi)
{
	struct himax_ts_data *ts;
	//int status = -EINVAL;
	int ret = 0;
	/* Allocate driver data */
	I("%s:IN!\n", __func__);
	ts = kzalloc(sizeof(struct himax_ts_data), GFP_KERNEL);
	if (ts == NULL) {
		E("%s: allocate himax_ts_data failed\n", __func__);
		ret = -ENOMEM;
		goto err_alloc_data_failed;
	}

	//spin_lock_init(&hx_spi_lock);

	/* Initialize the driver data */
	hx_spi = spi;
	
	//himax_hw_init();

	/* setup SPI parameters */
	/* CPOL=CPHA=0, speed 1MHz */
	if (hx_spi->master->flags & SPI_MASTER_HALF_DUPLEX) {
        I("Full duplex not supported by master\n");
        ret = -EIO;
        goto err_spi_setup;
    }
	hx_spi->mode            = SPI_MODE_3;
	hx_spi->bits_per_word   = 8;
	hx_spi->max_speed_hz    = 8* 1000 * 1000;
#ifndef CONFIG_SPI_MT65XX
	memcpy(&hx_spi_mcc, &spi_ctrdata, sizeof(struct mt_chip_conf));
	hx_spi->controller_data = (void *)&hx_spi_mcc;
	I("%s %d,Old SPI,need to spi_setup()\n", __func__, __LINE__);
	spi_setup(hx_spi);
#endif

#if 0//Himax Test	
	memcpy(&hx_spi_mcc, &spi_ctrdata, sizeof(struct mtk_chip_config));
	hx_spi->controller_data = (void *)&hx_spi_mcc;
	I("%s %d,New SPI, NOT need to spi_setup()\n", __func__, __LINE__);
#endif

	hx_irq = 0;
	spi_set_drvdata(spi, ts);
	mutex_init(&hx_spi_lock);

	/* allocate buffer for SPI transfer */
	/*hx_spi_buffer = kzalloc(bufsiz, GFP_KERNEL);
	if (hx_spi_buffer == NULL) {
		status = -ENOMEM;
		goto err_check_functionality_failed;
	}*/

	//hx_spi = spi;
	ts->dev = &spi->dev;;
	private_ts = ts;

	ret = himax_chip_common_init();
	
	return ret;
//err_check_functionality_failed:
err_spi_setup:
	kfree(ts);
err_alloc_data_failed:

	return ret;
}

int himax_common_probe_i2c(struct i2c_client *client, const struct i2c_device_id *id)
{
	struct himax_ts_data *ts;
	int ret = 0;

	client->addr =0x48;
#if defined(MTK_I2C_DMA)
	client->dev.coherent_dma_mask = DMA_BIT_MASK(32);
	gpDMABuf_va = (u8 *)dma_alloc_coherent(&client->dev, 4096, (dma_addr_t *)&gpDMABuf_pa, GFP_KERNEL);

	if (!gpDMABuf_va) {
		E("Allocate DMA I2C Buffer failed\n");
		ret = -ENODEV;
		goto err_alloc_MTK_DMA_failed;
	}

	memset(gpDMABuf_va, 0, 4096);
#endif
	/* Check I2C functionality */
	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		E("%s: i2c check functionality error\n", __func__);
		ret = -ENODEV;
		goto err_check_functionality_failed;
	}

	ts = kzalloc(sizeof(struct himax_ts_data), GFP_KERNEL);
	if (ts == NULL) {
		E("%s: allocate himax_ts_data failed\n", __func__);
		ret = -ENOMEM;
		goto err_alloc_data_failed;
	}

	i2c_set_clientdata(client, ts);
	i2c_client_point = client;
	ts->client = client;
	ts->dev = &client->dev;
	private_ts = ts;

	ret = himax_chip_common_init();

err_alloc_data_failed:
err_check_functionality_failed:
#if defined(MTK_I2C_DMA)

	if (ret) {
		if (gpDMABuf_va) {
			dma_free_coherent(&client->dev, 4096, gpDMABuf_va, (dma_addr_t)gpDMABuf_pa);
			gpDMABuf_va = NULL;
			gpDMABuf_pa = NULL;
		}
	}

err_alloc_MTK_DMA_failed:
#endif
	return ret;
}


int himax_common_remove_spi(struct spi_device *spi)
{
	int ret = 0;
	hx_spi = NULL;
	spi_set_drvdata(spi, NULL);
	himax_chip_common_deinit();

	return ret;
}

int himax_common_remove_i2c(struct i2c_client *client)
{
	int ret = 0;
	himax_chip_common_deinit();
	
	if ( gpDMABuf_va) {
		dma_free_coherent(&client->dev, 4096, gpDMABuf_va, (dma_addr_t)gpDMABuf_pa);
		gpDMABuf_va = NULL;
		gpDMABuf_pa = NULL;
	}

	return ret;
}

static void himax_common_suspend(struct device *dev)
{
	struct himax_ts_data *ts = NULL;
	if (spi_dev_detected)
		ts = private_ts;
	else
		ts = dev_get_drvdata(&i2c_client_point->dev);
	
	I("%s: enter \n", __func__);
	himax_chip_common_suspend(ts);
	I("%s: END \n", __func__);
	return ;
}
static void himax_common_resume(struct device *dev)
{
	struct himax_ts_data *ts = NULL;
	
	if (spi_dev_detected)
		ts = private_ts;
	else
		ts = dev_get_drvdata(&i2c_client_point->dev);
	I("%s: enter \n", __func__);
	himax_chip_common_resume(ts);
	I("%s: END \n", __func__);
	return ;
}

#if defined(CONFIG_FB)
int fb_notifier_callback(struct notifier_block *self,
							unsigned long event, void *data)
{
	struct fb_event *evdata = data;
	int *blank;
	struct himax_ts_data *ts =
	    container_of(self, struct himax_ts_data, fb_notif);
	I(" %s\n", __func__);

	if (evdata && evdata->data && event == FB_EVENT_BLANK && ts && (hx_spi ||ts->client)) {
		blank = evdata->data;

		switch (*blank) {
		case FB_BLANK_UNBLANK:
			if (spi_dev_detected)
				himax_common_resume(ts->dev);
			else
				himax_common_resume(&ts->client->dev);
			break;

		case FB_BLANK_POWERDOWN:
		case FB_BLANK_HSYNC_SUSPEND:
		case FB_BLANK_VSYNC_SUSPEND:
		case FB_BLANK_NORMAL:
			if (spi_dev_detected)
				himax_common_suspend(ts->dev);
			else
				himax_common_suspend(&ts->client->dev);
			break;
		}
	}

	return 0;
}
#endif

static int himax_common_detect(struct i2c_client *client, struct i2c_board_info *info)
{
	strlcpy(info->type, TPD_DEVICE, sizeof(info->type));
	return 0;
}

static const struct i2c_device_id himax_common_ts_id[] = {
	{HIMAX_common_NAME, 0 },
	{}
};

static struct i2c_driver tpd_i2c_driver = {
	.probe = himax_common_probe_i2c,
	.remove = himax_common_remove_i2c,
	.detect = himax_common_detect,
	.driver	= {
		.name = HIMAX_common_NAME,
		.of_match_table = of_match_ptr(himax_match_table),
	},
	.id_table = himax_common_ts_id,
	.address_list = (const unsigned short *) forces,
};

struct spi_device_id hx_spi_id_table = {"himax-spi", 1};
static struct spi_driver himax_common_driver = {
	.driver = {
		.name = HIMAX_common_NAME,
		.bus = &spi_bus_type,
		.owner = THIS_MODULE,
#ifdef CONFIG_OF
		.of_match_table = himax_match_table,
#endif
	},
	.probe = himax_common_probe_spi,
	.remove = himax_common_remove_spi,
	.id_table = &hx_spi_id_table,
};

#if 0
static struct spi_board_info spi_board_devs[] __initdata = {
	[0] = {
		.modalias = "himax-spi",
		.bus_num = 1,
		.max_speed_hz = (6*1000000),
		.chip_select = 1,
		.mode = SPI_MODE_3,
	},
};
#endif
static int himax_common_local_init(void) {
	int retval;
	I("[Himax] wangbin Himax_ts SPI Touchscreen Driver local init\n");

	tpd->reg = regulator_get(tpd->tpd_dev, "vtouch");
	retval = regulator_set_voltage(tpd->reg, 2800000, 2800000);

	if (retval != 0) {
		E("Failed to set voltage 2V8: %d\n", retval);
	}
	
	if (spi_dev_detected) {
		//spi_register_board_info(spi_board_devs, 1);
		retval = spi_register_driver(&himax_common_driver);
		if (retval < 0) {
			E("unable to add SPI driver.\n");
			return -EFAULT;
		}
		I("[Himax] wangbin Himax_ts SPI Touchscreen Driver local init\n");
	} else {
		if (i2c_add_driver(&tpd_i2c_driver) != 0)
		{
		    E("unable to add i2c driver.\n");
		    return -1;
		}
		I("[Himax] wangbin Himax_ts I2C Touchscreen Driver local init\n");
	}
#if defined(HX_PLATFOME_DEFINE_KEY)
	if (tpd_dts_data.use_tpd_button) {
		I("tpd_dts_data.use_tpd_button %d\n", tpd_dts_data.use_tpd_button);
		tpd_button_setting(tpd_dts_data.tpd_key_num,
							tpd_dts_data.tpd_key_local,
							tpd_dts_data.tpd_key_dim_local);
	}
#endif

	tpd_type_cap = 1;
	return retval;
}

static struct tpd_driver_t tpd_device_driver = {
	.tpd_device_name = HIMAX_common_NAME,
	.tpd_local_init = himax_common_local_init,
	.suspend = himax_common_suspend,
	.resume = himax_common_resume,
#ifdef TPD_HAVE_BUTTON
	.tpd_have_button = 1,
#else
	.tpd_have_button = 0,
#endif
};


static int __init himax_common_init(void)
{
	int status = 0;

	I("himax_common_init enter\n");

	
	if(((strstr(saved_command_line, "hx83102_hd720plus_dsi_vdo_bflash_hlt") != NULL))
		|| ((strstr(saved_command_line, "hx83102_hd720plus_dsi_vdo_dflash_hlt") != NULL))
			|| ((strstr(saved_command_line, "hx83102_hd720plus_dsi_vdo_dzero_flash_hlt") != NULL))
				|| ((strstr(saved_command_line, "hx83102_hd720plus_dsi_vdo_bflash_truly") != NULL))
					||((strstr(saved_command_line, "hx83102_hd720plus_dsi_vdo_dflash_truly") != NULL))
					||((strstr(saved_command_line, "txd_hd720plus_dsi_vdo_bflash") != NULL))
						||((strstr(saved_command_line, "hx83102_hd720plus_dsi_vdo_dzero_flash_truly") != NULL))) {

		if((strstr(saved_command_line, "hx83102_hd720plus_dsi_vdo_dzero_flash_hlt") != NULL)
			|| (strstr(saved_command_line, "hx83102_hd720plus_dsi_vdo_dzero_flash_truly") != NULL)){
			spi_dev_detected = true;	
		}
		I("spi_dev_detected = %d\n", spi_dev_detected);
		tpd_get_dts_info();
		I("tpd_driver_add HXTP wangbin\n");

		if (tpd_driver_add(&tpd_device_driver) < 0){
			I("Failed to add Driver!\n");
			return -1;
		}
		return status;
	}
	else {
		I("not match lk himax lcm name\n");
		return -1;
	}
}

static void __exit himax_common_exit(void)
{	
	if (spi_dev_detected)
		spi_unregister_driver(&himax_common_driver);
	else
		tpd_driver_remove(&tpd_device_driver);
}

module_init(himax_common_init);
module_exit(himax_common_exit);

MODULE_DESCRIPTION("Himax_common driver");
MODULE_LICENSE("GPL");

