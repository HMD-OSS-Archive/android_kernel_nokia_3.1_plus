/*
 * Copyright (C) 2017 MediaTek Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See http://www.gnu.org/licenses/gpl-2.0.html for more details.
 */

#include "kd_camera_typedef.h"
#include "imgsensor_i2c.h"

#ifdef IMGSENSOR_LEGACY_COMPAT
void kdSetI2CSpeed(u16 i2cSpeed)
{

}

int iReadRegI2C(
	u8 *a_pSendData,
	u16 a_sizeSendData,
	u8 *a_pRecvData,
	u16 a_sizeRecvData,
	u16 i2cId)
{
	return imgsensor_i2c_read(
	    pgi2c_cfg_legacy,
	    a_pSendData,
	    a_sizeSendData,
	    a_pRecvData,
	    a_sizeRecvData,
	    i2cId,
	    IMGSENSOR_I2C_SPEED);
}

int iReadRegI2CTiming(
	u8 *a_pSendData,
	u16 a_sizeSendData,
	u8 *a_pRecvData,
	u16 a_sizeRecvData,
	u16 i2cId,
	u16 timing)
{
	return imgsensor_i2c_read(
	    pgi2c_cfg_legacy,
	    a_pSendData,
	    a_sizeSendData,
	    a_pRecvData,
	    a_sizeRecvData,
	    i2cId,
	    timing);
}

int iWriteRegI2C(u8 *a_pSendData, u16 a_sizeSendData, u16 i2cId)
{
	return imgsensor_i2c_write(
	    pgi2c_cfg_legacy,
	    a_pSendData,
	    a_sizeSendData,
	    a_sizeSendData,
	    i2cId,
	    IMGSENSOR_I2C_SPEED);
}

int iWriteRegI2CTiming(
	u8 *a_pSendData,
	u16 a_sizeSendData,
	u16 i2cId,
	u16 timing)
{
	return imgsensor_i2c_write(
	    pgi2c_cfg_legacy,
	    a_pSendData,
	    a_sizeSendData,
	    a_sizeSendData,
	    i2cId,
	    timing);
}

int iBurstWriteReg(u8 *pData, u32 bytes, u16 i2cId)
{
	return imgsensor_i2c_write(
	    pgi2c_cfg_legacy,
	    pData,
	    bytes,
	    bytes,
	    i2cId,
	    IMGSENSOR_I2C_SPEED);
}

int iBurstWriteReg_multi(
	u8 *pData,
	u32 bytes,
	u16 i2cId,
	u16 transfer_length,
	u16 timing)
{
	return imgsensor_i2c_write(
	    pgi2c_cfg_legacy,
	    pData,
	    bytes,
	    transfer_length,
	    i2cId,
	    timing);
}

int fih_read_eeprom_data(u16 ui_offset, u16 ui_length, u8 *pi_buffer, u8 slave_addr)
{
	int iRetValue = 0;
	int iResidueDataLength;
	u32 uIncOffset = 0;
	u32 uCurrentOffset;
	u8 *pBuff;

	iResidueDataLength = (int)ui_length;
	uCurrentOffset = ui_offset;
	pBuff = pi_buffer;

	do {
		if (iResidueDataLength >= 8) 
        {
            char puReadCmd[2] = { (char)(uCurrentOffset >> 8), (char)(uCurrentOffset & 0xFF) };
			iRetValue = iReadRegI2C(puReadCmd, 2, pBuff, 8, slave_addr);
			if (iRetValue != 0) {
				printk("jasondz: I2C iReadData failed1!!\n");
				return -1;
			}
			uIncOffset += 8;
			iResidueDataLength -= 8;
			uCurrentOffset = ui_offset + uIncOffset;
			pBuff = pi_buffer + uIncOffset;
		} 
        else
        {
            char puReadCmd[2] = { (char)(uCurrentOffset >> 8), (char)(uCurrentOffset & 0xFF) };
			iRetValue = iReadRegI2C(puReadCmd, 2, pBuff, iResidueDataLength, slave_addr);
			if (iRetValue != 0) {
				printk("jasondz: I2C iReadData failed2!!\n");
				return -1;
			}
			uIncOffset += 8;
			iResidueDataLength -= 8;
			uCurrentOffset = ui_offset + uIncOffset;
			pBuff = pi_buffer + uIncOffset;
		}
	} while (iResidueDataLength > 0);

	return 0;
}

#endif

