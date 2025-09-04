/*
 * Copyright (C) 2015 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

/*
 * DW9714AF voice coil motor driver
 *
 *
 */

#include <linux/i2c.h>
#include <linux/delay.h>
#include <linux/uaccess.h>
#include <linux/fs.h>

#include "lens_info.h"
//JYLee added for slop control 20180607 [
#include <linux/hrtimer.h>
#include <linux/ktime.h>
//JYLee added for slop control 20180607 ]
#define AF_DRVNAME "DW9714AF_DRV"
#define AF_I2C_SLAVE_ADDR 0x18

#define AF_DEBUG
#ifdef AF_DEBUG
#define LOG_INF(format, args...)                                               \
	pr_debug(AF_DRVNAME " [%s] " format, __func__, ##args)
#else
#define LOG_INF(format, args...)
#endif

//[zcgadd] 20180723 add vcamaf power control function to support VCM work start[
extern int myctrlAFPower(int );
//[zcgadd] 20180723 add vcamaf power control function to support VCM work end]

static struct i2c_client *g_pstAF_I2Cclient;
static int *g_pAF_Opened;
static spinlock_t *g_pAF_SpinLock;


static unsigned long g_u4AF_INF = 0; //JYLee modified to fix AF init fail 20180409
static unsigned long g_u4AF_MACRO = 1023;
static unsigned long g_u4TargetPosition;
static unsigned long g_u4CurrPosition;

//[zcgadd] 20180723 add LSC mode function to reduce AF release impact sound start[
int DW9714AF_LSC_Mode(void);
//[zcgadd] 20180723 add LSC mode function to reduce AF release impact sound end]

static int s4AF_ReadReg(unsigned short *a_pu2Result)
{
	int i4RetValue = 0;
	char pBuff[2];

	g_pstAF_I2Cclient->addr = AF_I2C_SLAVE_ADDR;

	g_pstAF_I2Cclient->addr = g_pstAF_I2Cclient->addr >> 1;

	i4RetValue = i2c_master_recv(g_pstAF_I2Cclient, pBuff, 2);

	if (i4RetValue < 0) {
		LOG_INF("I2C read failed!!\n");
		return -1;
	}

	*a_pu2Result = (((u16) pBuff[0]) << 4) + (pBuff[1] >> 4);

	return 0;
}

static int s4AF_WriteReg(u16 a_u2Data)
{
	int i4RetValue = 0;

	char puSendCmd[2] = {(char)(a_u2Data >> 4),
			     (char)((a_u2Data & 0xF) << 4)};

	g_pstAF_I2Cclient->addr = AF_I2C_SLAVE_ADDR;

	g_pstAF_I2Cclient->addr = g_pstAF_I2Cclient->addr >> 1;

	i4RetValue = i2c_master_send(g_pstAF_I2Cclient, puSendCmd, 2);

	if (i4RetValue < 0) {
		LOG_INF("I2C send failed!!\n");
		return -1;
	}

	return 0;
}
//JYLee added for slop control 20180607 [
static int s4AF_WriteReg_16bit(u16 a_u2Data)
{
	int i4RetValue = 0;

	char puSendCmd[2] = { (char)(a_u2Data >> 8), (char)(a_u2Data & 0xFF) };

	g_pstAF_I2Cclient->addr = AF_I2C_SLAVE_ADDR;

	g_pstAF_I2Cclient->addr = g_pstAF_I2Cclient->addr >> 1;

	i4RetValue = i2c_master_send(g_pstAF_I2Cclient, puSendCmd, 2);

	if (i4RetValue < 0) {
		LOG_INF("I2C send failed!!\n");
		return -1;
	}

	return 0;
}
//JYLee added for slop control 20180607 ]
static inline int getAFInfo(__user struct stAF_MotorInfo *pstMotorInfo)
{
	struct stAF_MotorInfo stMotorInfo;

	stMotorInfo.u4MacroPosition = g_u4AF_MACRO;
	stMotorInfo.u4InfPosition = g_u4AF_INF;
	stMotorInfo.u4CurrentPosition = g_u4CurrPosition;
	stMotorInfo.bIsSupportSR = 1;

	stMotorInfo.bIsMotorMoving = 1;

	if (*g_pAF_Opened >= 1)
		stMotorInfo.bIsMotorOpen = 1;
	else
		stMotorInfo.bIsMotorOpen = 0;

	if (copy_to_user(pstMotorInfo, &stMotorInfo,
			 sizeof(struct stAF_MotorInfo)))
		LOG_INF("copy to user failed when getting motor information\n");

	return 0;
}

/* initAF include driver initialization and standby mode */
static int initAF(void)
{
	LOG_INF("+\n");

	if (*g_pAF_Opened == 1) {

		spin_lock(g_pAF_SpinLock);
		*g_pAF_Opened = 2;
		spin_unlock(g_pAF_SpinLock);
	}

	LOG_INF("-\n");

	return 0;
}

/* moveAF only use to control moving the motor */
static inline int moveAF(unsigned long a_u4Position)
{
	int ret = 0;

	if ((a_u4Position > g_u4AF_MACRO) || (a_u4Position < g_u4AF_INF)) {
		LOG_INF("out of range\n");
		return -EINVAL;
	}

	if (*g_pAF_Opened == 1) {
		unsigned short InitPos;

		ret = s4AF_ReadReg(&InitPos);

		if (ret == 0) {
			LOG_INF("Init Pos %6d\n", InitPos);

			spin_lock(g_pAF_SpinLock);
			g_u4CurrPosition = (unsigned long)InitPos;
			spin_unlock(g_pAF_SpinLock);

		} else {
			spin_lock(g_pAF_SpinLock);
			g_u4CurrPosition = 0;
			spin_unlock(g_pAF_SpinLock);
		}

		spin_lock(g_pAF_SpinLock);
		*g_pAF_Opened = 2;
		spin_unlock(g_pAF_SpinLock);
	}

	if (g_u4CurrPosition == a_u4Position)
		return 0;

	spin_lock(g_pAF_SpinLock);
	g_u4TargetPosition = a_u4Position;
	spin_unlock(g_pAF_SpinLock);

  //printk("[hanpengfei]move [curr] %lu [target] %lu\n", g_u4CurrPosition, g_u4TargetPosition); 


	if (s4AF_WriteReg((unsigned short)g_u4TargetPosition) == 0) {
		spin_lock(g_pAF_SpinLock);
		g_u4CurrPosition = (unsigned long)g_u4TargetPosition;
		spin_unlock(g_pAF_SpinLock);
	} else {
		LOG_INF("set I2C failed when moving the motor\n");
		ret = -1;
	}

	return ret;
}

static inline int setAFInf(unsigned long a_u4Position)
{
	spin_lock(g_pAF_SpinLock);
	g_u4AF_INF = a_u4Position;
	spin_unlock(g_pAF_SpinLock);
	return 0;
}

static inline int setAFMacro(unsigned long a_u4Position)
{
	spin_lock(g_pAF_SpinLock);
	g_u4AF_MACRO = a_u4Position;
	spin_unlock(g_pAF_SpinLock);
	return 0;
}

/* ////////////////////////////////////////////////////////////// */
long DW9714AF_Ioctl(struct file *a_pstFile, unsigned int a_u4Command,
		    unsigned long a_u4Param)
{
	long i4RetValue = 0;

	switch (a_u4Command) {
	case AFIOC_G_MOTORINFO:
		i4RetValue =
			getAFInfo((__user struct stAF_MotorInfo *)(a_u4Param));
		break;

	case AFIOC_T_MOVETO:
		i4RetValue = moveAF(a_u4Param);
		break;

	case AFIOC_T_SETINFPOS:
		i4RetValue = setAFInf(a_u4Param);
		break;

	case AFIOC_T_SETMACROPOS:
		i4RetValue = setAFMacro(a_u4Param);
		break;
//JYLee modified to fix AF init fail 20180409 [
	case AFIOC_S_SETPARA:
		LOG_INF("No AFIOC_S_SETPARA CMD\n");
		break;
//JYLee modified to fix AF init fail 20180409 ]
	default:
		LOG_INF("No CMD\n");
		i4RetValue = -EPERM;
		break;
	}

	return i4RetValue;
}

/* Main jobs: */
/* 1.Deallocate anything that "open" allocated in private_data. */
/* 2.Shut down the device on last close. */
/* 3.Only called once on last time. */
/* Q1 : Try release multiple times. */
//[zcgadd] 20180723 add LSC mode function to reduce AF release impact sound start[
int DW9714AF_LSC_Mode(void)
{
	LOG_INF("[zcglens]DW9714AF_LSC_Mode Start\n");

      if (s4AF_WriteReg_16bit(0xECA3) == -1)
       {
           s4AF_WriteReg_16bit(0xECA3);
           LOG_INF("retry DW9714AF_LSC_Mode 0xECA3\n");
       }
       s4AF_WriteReg_16bit(0xA104);
       s4AF_WriteReg_16bit(0xF2C0);
       s4AF_WriteReg_16bit(0xDC51);
       s4AF_WriteReg_16bit(0x0006);

	LOG_INF("[zcglens]DW9714AF_LSC_Mode End\n");

	return 1;
}
//[zcgadd] 20180723 add LSC mode function to reduce AF release impact sound end]

int DW9714AF_Release(struct inode *a_pstInode, struct file *a_pstFile)
{
//[zcgadd] 20180723 add vcamaf power control function to support VCM work start[
myctrlAFPower(1);
LOG_INF("[zcglens]Start\n");
//[zcgadd] 20180723 add vcamaf power control function to support VCM work end]
//[zcgadd] 20180723 add LSC mode function to reduce AF release impact sound start[
DW9714AF_LSC_Mode();
usleep_range(100000, 110000);
//[zcgadd] 20180723 add LSC mode function to reduce AF release impact sound end]
	if (*g_pAF_Opened == 2) {
		LOG_INF("Wait\n");
		//s4AF_WriteReg(0x80); /* Power down mode */
		s4AF_WriteReg_16bit(0x8000); /* Power down mode */ //JYLee added for slop control 20180607
	}

	if (*g_pAF_Opened) {
		LOG_INF("Free\n");

		spin_lock(g_pAF_SpinLock);
		*g_pAF_Opened = 0;
		spin_unlock(g_pAF_SpinLock);
	}

	LOG_INF("End\n");
//[zcgadd] 20180723 add vcamaf power control function to support VCM work start[	
myctrlAFPower(0 );
//[zcgadd] 20180723 add vcamaf power control function to support VCM work end]
	return 0;
}
//JYLee added for slop control 20180607 [
int DW9714AF_Init(void)
{

	LOG_INF("[zcglens]DW9714AF_Init Start\n");

       if (s4AF_WriteReg_16bit(0x8000) == -1)
       {
           s4AF_WriteReg_16bit(0x8000);
           LOG_INF("DW9714AF_Init retry WriteReg_16bit 0x8000\n");
       }

       s4AF_WriteReg_16bit(0x0000);
       usleep_range(100, 110);
       s4AF_WriteReg_16bit(0xECA3);
       s4AF_WriteReg_16bit(0xA115);
       s4AF_WriteReg_16bit(0xF228);
       s4AF_WriteReg_16bit(0xDC51);

	LOG_INF("DW9714AF_Init End\n");

	return 1;
}
int DW9714AF_SetI2Cclient(struct i2c_client *pstAF_I2Cclient,
			  spinlock_t *pAF_SpinLock, int *pAF_Opened)
{
	g_pstAF_I2Cclient = pstAF_I2Cclient;
	g_pAF_SpinLock = pAF_SpinLock;
	g_pAF_Opened = pAF_Opened;

	initAF();
	DW9714AF_Init(); //JYLee added for slop control 20180607
	LOG_INF("DW9714AF_SetI2Cclient End\n");

	return 1;
}

int DW9714AF_GetFileName(unsigned char *pFileName)
{
	#if SUPPORT_GETTING_LENS_FOLDER_NAME
	char FilePath[256];
	char *FileString;

	sprintf(FilePath, "%s", __FILE__);
	FileString = strrchr(FilePath, '/');
	*FileString = '\0';
	FileString = (strrchr(FilePath, '/') + 1);
	strncpy(pFileName, FileString, AF_MOTOR_NAME);
	LOG_INF("FileName : %s\n", pFileName);
	#else
	pFileName[0] = '\0';
	#endif
	return 1;
}
