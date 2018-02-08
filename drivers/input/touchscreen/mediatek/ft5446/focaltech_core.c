/*
 *
 * FocalTech ftxxxx TouchScreen driver.
 * 
 * Copyright (c) 2010-2015, Focaltech Ltd. All rights reserved.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

 /*******************************************************************************
*
* File Name: Ftxxxx_ts.c
*
*    Author: Tsai HsiangYu
*
*   Created: 2015-03-02
*
*  Abstract:
*
* Reference:
*
*******************************************************************************/

/*******************************************************************************
* 1.Included header files
*******************************************************************************/
/*
///user defined include header files
#include <linux/i2c.h>
#include <linux/input.h>
//#include <linux/earlysuspend.h>
#include <linux/delay.h>
#include <linux/slab.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/kernel.h>
#include <linux/semaphore.h>
#include <linux/mutex.h>
#include <linux/module.h>
#include <linux/syscalls.h>
#include <linux/unistd.h>
#include <linux/uaccess.h>
#include <linux/fs.h>
#include <linux/string.h>
#include <linux/timer.h>
#include <linux/input/mt.h>
#include <linux/switch.h>
#include <linux/gpio.h>
#include "focaltech_core.h"
*/

//#include "tpd.h"
//#include "tpd_custom_fts.h"
#include "cust_gpio_usage.h"
#include <cust_eint.h>
#include <linux/interrupt.h>
#include <linux/i2c.h>
#include <linux/sched.h>
#include <linux/kthread.h>
#include <linux/rtpm_prio.h>
#include <linux/wait.h>
#include <linux/time.h>
#include <linux/delay.h>
#include <linux/input.h>
#include <linux/input/mt.h>
#include <linux/dma-mapping.h>
#include <mach/mt_pm_ldo.h>
#include <mach/mt_typedefs.h>
#include <mach/mt_boot.h>
#include "focaltech_core.h"
#include <cust_i2c.h> //add by lixh10
 #ifdef FTS_INCALL_TOUCH_REJECTION
#include <mach/battery_common.h>
#include "../../../../misc/mediatek/alsps/epl8801/epl8801.h" 
#endif 
/*******************************************************************************
* 2.Private constant and macro definitions using #define
*******************************************************************************/
/*register define*/
#define FTS_RESET_PIN							GPIO_CTP_RST_PIN
#define TPD_OK 									0
#define DEVICE_MODE 							0x00
#define GEST_ID 									0x01
#define TD_STATUS 								0x02
#define TOUCH1_XH 								0x03
#define TOUCH1_XL 								0x04
#define TOUCH1_YH 								0x05
#define TOUCH1_YL 								0x06
#define TOUCH2_XH 								0x09
#define TOUCH2_XL 								0x0A
#define TOUCH2_YH 								0x0B
#define TOUCH2_YL 								0x0C
#define TOUCH3_XH 								0x0F
#define TOUCH3_XL 								0x10
#define TOUCH3_YH 								0x11
#define TOUCH3_YL 								0x12
#define TPD_MAX_RESET_COUNT 					3
/*if need these function, pls enable this MACRO*/

//#define TPD_PROXIMITY
			

#define FTS_CTL_IIC
#define SYSFS_DEBUG
//#define FTS_APK_DEBUG


#if GTP_ESD_PROTECT
	#define TPD_ESD_CHECK_CIRCLE        	200
	static struct delayed_work gtp_esd_check_work;
	static struct workqueue_struct *gtp_esd_check_workqueue = NULL;
	static void gtp_esd_check_func(struct work_struct *);
	
	//add for esd
	static int count_irq = 0;
	static unsigned long esd_check_circle = TPD_ESD_CHECK_CIRCLE;
	static u8 run_check_91_register = 0;
	u8 esd_running;
	//spinlock esd_lock;
#endif


//#ifdef FTS_CTL_IIC
//	#include "focaltech_ctl.h"
//#endif
//#ifdef SYSFS_DEBUG
//	#include "focaltech_ex_fun.h"
//#endif

/*PROXIMITY*/
#ifdef TPD_PROXIMITY
	#include <linux/hwmsensor.h>
	#include <linux/hwmsen_dev.h>
	#include <linux/sensors_io.h>
#endif

#ifdef TPD_PROXIMITY
	#define APS_ERR(fmt,arg...)           	printk("<<proximity>> "fmt"\n",##arg)
	#define TPD_PROXIMITY_DEBUG(fmt,arg...) printk("<<proximity>> "fmt"\n",##arg)
	#define TPD_PROXIMITY_DMESG(fmt,arg...) printk("<<proximity>> "fmt"\n",##arg)
	static u8 tpd_proximity_flag 			= 0;
	//add for tpd_proximity by wangdongfang
	static u8 tpd_proximity_flag_one 		= 0; 
	//0-->close ; 1--> far away
	static u8 tpd_proximity_detect 		= 1;
#endif
/*dma declare, allocate and release*/
#define __MSG_DMA_MODE__
#ifdef __MSG_DMA_MODE__
	u8 *g_dma_buff_va = NULL;
	u8 *g_dma_buff_pa = NULL;
#endif

#ifdef __MSG_DMA_MODE__

	static void msg_dma_alloct(struct i2c_client *client)
	{
		g_dma_buff_va = (u8 *)dma_alloc_coherent(&client->dev, 4096, &g_dma_buff_pa, GFP_KERNEL);//DMA size 4096 for customer
	    	if(!g_dma_buff_va)
		{
	        	TPD_DMESG("[DMA][Error] Allocate DMA I2C Buffer failed!\n");
	    	}
	}
	static void msg_dma_release(struct i2c_client *client){
		if(g_dma_buff_va)
		{
	     		dma_free_coherent(&client->dev, 4096, g_dma_buff_va, g_dma_buff_pa);
	        	g_dma_buff_va = NULL;
	        	g_dma_buff_pa = NULL;
			TPD_DMESG("[DMA][release] Allocate DMA I2C Buffer release!\n");
	    	}
	}
#endif
#ifdef TPD_HAVE_BUTTON 
	static int tpd_keys_local[TPD_KEY_COUNT] = TPD_KEYS;
	static int tpd_keys_dim_local[TPD_KEY_COUNT][4] = TPD_KEYS_DIM;
#endif
#if (defined(TPD_WARP_START) && defined(TPD_WARP_END))
	static int tpd_wb_start_local[TPD_WARP_CNT] = TPD_WARP_START;
	static int tpd_wb_end_local[TPD_WARP_CNT]   = TPD_WARP_END;
#endif
#if (defined(TPD_HAVE_CALIBRATION) && !defined(TPD_CUSTOM_CALIBRATION))
	static int tpd_calmat_local[8]     = TPD_CALIBRATION_MATRIX;
	static int tpd_def_calmat_local[8] = TPD_CALIBRATION_MATRIX;
#endif
/*******************************************************************************
* 3.Private enumerations, structures and unions using typedef
*******************************************************************************/

/*touch info*/
struct touch_info {
    int y[10];
    int x[10];
    int p[10];
    int id[10];
    int count;
};

/*register driver and device info*/ 
static const struct i2c_device_id fts_tpd_id[] = {{"fts",0},{}};

static struct i2c_board_info __initdata fts_i2c_tpd={ I2C_BOARD_INFO("fts", (0x70>>1))};
 


 
/*******************************************************************************
* 4.Static variables
*******************************************************************************/
struct i2c_client *fts_i2c_client = NULL;
struct input_dev *fts_input_dev=NULL;
struct task_struct *thread = NULL;
//static u8 last_touchpoint=0;
 //add by lixh10 start 
 #ifdef FTS_INCALL_TOUCH_REJECTION
extern kal_bool g_call_state; //lixh10 
extern int epl_sensor_show_ps_status_for_tp() ;
#endif
#if  FTS_GESTRUE_EN 
  static int fts_wakeup_flag = 0;
  static bool  fts_gesture_status = false;
   int fts_gesture_letter = 0 ;
#endif
#if  FTS_GLOVE_EN 
  static bool  fts_glove_flag = false;
static int  fts_enter_glove( bool status );
#endif
#ifdef LENOVO_CTP_EAGE_LIMIT
static int eage_x = 5;
static int eage_y = 5;	
#endif
  static bool  fts_power_down = true;

#ifdef  TPD_AUTO_UPGRADE
struct workqueue_struct *touch_wq;
struct work_struct fw_update_work;
#endif

#ifdef LENOVO_CTP_TEST_FLUENCY
#define LCD_X 1080
#define LCD_Y 1920
static struct hrtimer tpd_test_fluency_timer;
#define TIMER_MS_TO_NS(x) (x * 1000 * 1000)
  
static int test_x = 0;
static int test_y = 0;
static int test_w = 0;
static int test_id = 0;
static int coordinate_interval = 10;
static int 	delay_time = 10;
static struct completion report_point_complete;

#endif
  static bool  is_fw_upgrate = false;
  static bool  is_turnoff_checkesd = false;


 //add by lixh10 end 
int up_flag=0,up_count=0;
static int tpd_flag = 0;
static int tpd_halt=0;
static int point_num = 0;
static int p_point_num = 0;
static u8 buf_addr[2] = { 0 };
static u8 buf_value[2] = { 0 };
/*******************************************************************************
* 5.Global variable or extern global variabls/functions
*******************************************************************************/



/*******************************************************************************
* 6.Static function prototypes
*******************************************************************************/


static DECLARE_WAIT_QUEUE_HEAD(waiter);
//static DEFINE_MUTEX(i2c_access);
static DEFINE_MUTEX(i2c_rw_access);
static void tpd_eint_interrupt_handler(void);
static int __devinit tpd_probe(struct i2c_client *client, const struct i2c_device_id *id);
static int tpd_detect (struct i2c_client *client, struct i2c_board_info *info);
static int __devexit tpd_remove(struct i2c_client *client);
static int touch_event_handler(void *unused);
extern void mt_eint_mask(unsigned int eint_num);
extern void mt_eint_unmask(unsigned int eint_num);
extern void mt_eint_ack(unsigned int eint_num);
extern void mt_eint_set_hw_debounce(unsigned int eint_num, unsigned int ms);
extern void mt_eint_set_polarity(unsigned int eint_num, unsigned int pol);
extern unsigned int mt_eint_set_sens(unsigned int eint_num, unsigned int sens);
extern void mt_eint_registration(unsigned int eint_num, unsigned int flow, void (EINT_FUNC_PTR)(void), unsigned int is_auto_umask);


static struct i2c_driver tpd_i2c_driver = {
  .driver = {
  .name = "fts",
  //.owner = THIS_MODULE,
  },
  .probe = tpd_probe,
  .remove = __devexit_p(tpd_remove),
  .id_table = fts_tpd_id,
  .detect = tpd_detect,

 };



/*
*	open/release/(I/O) control tpd device
*
*/
//#define VELOCITY_CUSTOM_fts
#ifdef VELOCITY_CUSTOM_fts
#include <linux/device.h>
#include <linux/miscdevice.h>
#include <asm/uaccess.h>

/*for magnify velocity*/
#ifndef TPD_VELOCITY_CUSTOM_X
	#define TPD_VELOCITY_CUSTOM_X 				10
#endif
#ifndef TPD_VELOCITY_CUSTOM_Y
	#define TPD_VELOCITY_CUSTOM_Y 				10
#endif

#define TOUCH_IOC_MAGIC 						'A'
#define TPD_GET_VELOCITY_CUSTOM_X 			_IO(TOUCH_IOC_MAGIC,0)
#define TPD_GET_VELOCITY_CUSTOM_Y 			_IO(TOUCH_IOC_MAGIC,1)

int g_v_magnify_x =TPD_VELOCITY_CUSTOM_X;
int g_v_magnify_y =TPD_VELOCITY_CUSTOM_Y;


/************************************************************************
* Name: tpd_misc_open
* Brief: open node
* Input: node, file point
* Output: no
* Return: fail <0
***********************************************************************/
static int tpd_misc_open(struct inode *inode, struct file *file)
{
	return nonseekable_open(inode, file);
}
/************************************************************************
* Name: tpd_misc_release
* Brief: release node
* Input: node, file point
* Output: no
* Return: 0
***********************************************************************/
static int tpd_misc_release(struct inode *inode, struct file *file)
{
	return 0;
}
/************************************************************************
* Name: tpd_unlocked_ioctl
* Brief: I/O control for apk
* Input: file point, command
* Output: no
* Return: fail <0
***********************************************************************/

static long tpd_unlocked_ioctl(struct file *file, unsigned int cmd,
       unsigned long arg)
{

	void __user *data;
	
	long err = 0;
	
	if(_IOC_DIR(cmd) & _IOC_READ)
	{
		err = !access_ok(VERIFY_WRITE, (void __user *)arg, _IOC_SIZE(cmd));
	}
	else if(_IOC_DIR(cmd) & _IOC_WRITE)
	{
		err = !access_ok(VERIFY_READ, (void __user *)arg, _IOC_SIZE(cmd));
	}

	if(err)
	{
		printk("tpd: access error: %08X, (%2d, %2d)\n", cmd, _IOC_DIR(cmd), _IOC_SIZE(cmd));
		return -EFAULT;
	}

	switch(cmd)
	{
		case TPD_GET_VELOCITY_CUSTOM_X:
			data = (void __user *) arg;
			if(data == NULL)
			{
				err = -EINVAL;
				break;	  
			}			
			
			if(copy_to_user(data, &g_v_magnify_x, sizeof(g_v_magnify_x)))
			{
				err = -EFAULT;
				break;
			}				 
			break;

	   case TPD_GET_VELOCITY_CUSTOM_Y:
			data = (void __user *) arg;
			if(data == NULL)
			{
				err = -EINVAL;
				break;	  
			}			
			
			if(copy_to_user(data, &g_v_magnify_y, sizeof(g_v_magnify_y)))
			{
				err = -EFAULT;
				break;
			}				 
			break;


		default:
			printk("tpd: unknown IOCTL: 0x%08x\n", cmd);
			err = -ENOIOCTLCMD;
			break;
			
	}

	return err;
}


static struct file_operations tpd_fops = {
	//.owner = THIS_MODULE,
	.open = tpd_misc_open,
	.release = tpd_misc_release,
	.unlocked_ioctl = tpd_unlocked_ioctl,
};

static struct miscdevice tpd_misc_device = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "touch",
	.fops = &tpd_fops,
};
#endif
 //add by lixh10 start 

#ifdef LENOVO_CTP_TEST_FLUENCY
enum hrtimer_restart tpd_test_fluency_handler(struct hrtimer *timer)
{

    complete(&report_point_complete);
    return HRTIMER_NORESTART;
}

static int  fts_test_cfg( char *buf)
{
	int i;
	int error;
	int count  = 5;
	unsigned int  val[5]={0,};
	error = sscanf(buf, "%d%d%d", &val[0],&val[1],&val[2]);

	//dev_err(&fts_i2c_client->dev, "ahe fts_test_fluency para %d  %d  %d ! \n",val[0],val[1],val[2]);

	return val[0];
	
}
static ssize_t fts_test_fluency(struct device *dev, struct device_attribute *attr, char *buf)
{
	int i,j;
	int count ;
	int startpoint;
	
	unsigned int  val[5]={0,};
	int error;
	if(tpd_halt)
		return 0;
	
	//error = sscanf(buf, "%d%d", &val[0],&val[1]);
	j =  fts_test_cfg(buf);
	test_x =	LCD_X/2 ;//10 ;// LCD_Y/2 ;  //160--680
	test_y = 300 ;//LCD_Y/2;
	startpoint = test_y;
	test_w = 30;
         test_id = 1;
	count = (LCD_Y -test_y -50)/coordinate_interval ;
	mt_eint_mask(CUST_EINT_TOUCH_PANEL_NUM);  
	dev_err(&fts_i2c_client->dev, "ahe fts_test_fluency start %d ! \n",j);
	for(; j > 0; j--){
		 for(i = 0; i < 2*count; i++)
        		{   		
        			if(i<count){
				test_y +=coordinate_interval;
        			}else{ 
        				if(i==count)
					test_x +=10;
				test_y -=coordinate_interval;
			}
		
			//tpd_down(test_x, test_y, test_w);
			input_mt_slot(tpd->dev,test_id);
			input_mt_report_slot_state(tpd->dev, MT_TOOL_FINGER,true);
			input_report_key(tpd->dev, BTN_TOUCH, 1);
			
			 input_report_abs(tpd->dev, ABS_MT_PRESSURE,test_w);//0x3f  data->pressure[i]
			 input_report_abs(tpd->dev, ABS_MT_TOUCH_MAJOR,0x05);//0x05  data->area[i]
			 input_report_abs(tpd->dev, ABS_MT_POSITION_X,test_x);
			 input_report_abs(tpd->dev, ABS_MT_POSITION_Y,test_y);
			input_sync(tpd->dev);
			
            		init_completion(&report_point_complete);
	     		hrtimer_start(&tpd_test_fluency_timer, ktime_set(delay_time / 1000, (delay_time % 1000) * 1000000), HRTIMER_MODE_REL);
            		wait_for_completion(&report_point_complete);
        		}
		//tpd_up(test_x, test_y,NULL);
		input_mt_slot(tpd->dev,test_id);
		 input_mt_report_slot_state(tpd->dev, MT_TOOL_FINGER,false);
		 input_report_key(tpd->dev, BTN_TOUCH, 0);
		 input_sync(tpd->dev);
		mdelay(50);
		}
	mt_eint_unmask(CUST_EINT_TOUCH_PANEL_NUM);  
       	dev_err(&fts_i2c_client->dev,"ahe fts_test_fluency end  !!! \n");

	return 1;
}

static DEVICE_ATTR (fts_test_fluency, 0664,
			NULL,fts_test_fluency);

static ssize_t fts_test_fluency_interval_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%d\n",coordinate_interval);
}

static ssize_t fts_test_fluency_interval_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	unsigned int val;
	int error;
	//struct mms_ts_info *info = dev_get_drvdata(dev);
	error = sscanf(buf, "%d", &val);
	if(error != 1)
		return error;
	coordinate_interval = val;
	return count;
}


static DEVICE_ATTR (fts_test_interval, 0664,
			fts_test_fluency_interval_show,fts_test_fluency_interval_store);


static ssize_t fts_test_fluency_delay_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%d\n",delay_time);
}

static ssize_t fts_test_fluency_delay_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	unsigned int val;
	int error;
	//struct mms_ts_info *info = dev_get_drvdata(dev);
	error = sscanf(buf, "%d", &val);
	if(error != 1)
		return error;
	delay_time= val;
	return count;
}

static DEVICE_ATTR (fts_test_delay, 0664,
			fts_test_fluency_delay_show,fts_test_fluency_delay_store);

#endif


 #ifdef  TPD_AUTO_UPGRADE
 static void fts_fw_update_work_func(struct work_struct *work)
{
	printk("********************FTS Enter CTP Auto Upgrade ********************\n");
	mt_eint_mask(CUST_EINT_TOUCH_PANEL_NUM);
	is_fw_upgrate = true ;
#if GTP_ESD_PROTECT
	esd_switch(0);apk_debug_flag = 1;
#endif
	
	fts_ctpm_auto_upgrade(fts_i2c_client);
	mdelay(10);
	is_fw_upgrate = false;
#if GTP_ESD_PROTECT
	esd_switch(1);apk_debug_flag = 0;
#endif
#if FTS_USB_DETECT
	printk("mtk_tpd[fts] usb reg write \n");
	fts_usb_insert((bool) upmu_is_chr_det());
#endif 
	mt_eint_unmask(CUST_EINT_TOUCH_PANEL_NUM);
}
#endif
static ssize_t fts_information_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	unsigned char uc_reg_addr;
	unsigned char uc_reg_value[10]={0,};
	unsigned char fw_version = 0;
	unsigned char vendor = 0;
	unsigned char ic_version = 0;

	int retval =0;
	if(tpd_halt){
		dev_err(&fts_i2c_client->dev,"ahe cancel info reading  while suspend!\n");
		return retval;
	}
	uc_reg_addr = FTS_REG_CHIP_ID;
	retval = fts_i2c_write(fts_i2c_client, &uc_reg_addr, 1);
	 if(retval<0)
	 	goto I2C_FAIL; 
	retval = fts_i2c_read(fts_i2c_client, &uc_reg_addr, 0, &uc_reg_value, 6);
	 if(retval<0)
	 	goto I2C_FAIL; 

	  ic_version= uc_reg_value[0];
	 fw_version = uc_reg_value[3];
	 vendor =  uc_reg_value[5];
	 	
	dev_err(&fts_i2c_client->dev,"ahe [FTS] vendor = %02x \n", vendor);

	static char* vendor_id;
	switch (vendor){
	case 0x51:
	vendor_id = "ofilm";
	break;
	case 0xa0:
	vendor_id = "toptouch";
	break;
	case 0x90:
	vendor_id = "eachopto";
	break;
	case 0x3b:
	vendor_id = "biel";
	break;
	case 0x55:
	vendor_id = "laibao";
	break;
	default:
	vendor_id = "unknown";
	}	
	return snprintf(buf, PAGE_SIZE, "fts_%d_%s_%d \n",ic_version,vendor_id,fw_version);
I2C_FAIL:
	return retval ;
}
static DEVICE_ATTR (touchpanel_info, 0664,
			fts_information_show,
			NULL);


#if FTS_GLOVE_EN
static ssize_t fts_glove_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%d\n",fts_glove_flag);
}

static ssize_t fts_glove_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	unsigned long val;
	int error;
    	int ret;
	error = sstrtoul(buf, 10, &val);
	if(error)
		return error;
	if(val == 1)
	{
		if(!fts_glove_flag)
		{
            		if(tpd_halt){
	        			dev_err(&fts_i2c_client->dev, "fts Glove Send config error, power down.");
	        			return count;
           		 }
		   	 fts_glove_flag = true;
                		ret = fts_enter_glove(true);

		}else
			return count;
	}else
	{
		if(fts_glove_flag)
		{
            		if(tpd_halt){
	        			dev_err(&fts_i2c_client->dev, "fts Glove Send config error, power down.");
	        			return count;
           		 }
		    	fts_glove_flag = false;
			ret = fts_enter_glove(false);
		}else
			return count;
	}
	return count;
}

static DEVICE_ATTR (tpd_glove_status, 0664,
			fts_glove_show,
			fts_glove_store);
#endif


#if FTS_GESTRUE_EN 
static int get_array_flag(void)
{
    return fts_gesture_letter;
}
static ssize_t lenovo_gesture_flag_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%d\n",get_array_flag());
}
static DEVICE_ATTR (lenovo_tpd_info, 0664,
			lenovo_gesture_flag_show,
			NULL);
static ssize_t lenovo_gesture_wakeup_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%d\n",fts_wakeup_flag);
}

static ssize_t lenovo_gesture_wakeup_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	unsigned int val;
	int error;
	error = sstrtoul(buf, 10, &val);
	if(error)
		return error;
	if(val == 1)
	{
		if(!fts_wakeup_flag)
		{
		fts_wakeup_flag = 1;
		//enable_irq_wake(ts->client->irq);
		dev_err(&fts_i2c_client->dev,"ahe %s,gesture flag is  = %d",__func__,val);
		printk("ahe %s,gesture flag is  = %d",__func__,val);
		}else
			return count;
	}else
	{
		if(fts_wakeup_flag)
		{
		fts_wakeup_flag = 0;
		//disable_irq_wake(ts->client->irq);
		dev_err(&fts_i2c_client->dev,"ahe %s,gesture flag is  = %d",__func__,val);
		}else
			return count;
	}
	return count;
}

static DEVICE_ATTR ( tpd_suspend_status, 0664,
			lenovo_gesture_wakeup_show,
			lenovo_gesture_wakeup_store);
#endif

static struct attribute *fts_touch_attrs[] = {
	&dev_attr_touchpanel_info.attr,
#if FTS_GESTRUE_EN
	&dev_attr_tpd_suspend_status.attr,
	&dev_attr_lenovo_tpd_info.attr,
#endif
#if FTS_GLOVE_EN
	&dev_attr_tpd_glove_status.attr,
#endif
#ifdef LENOVO_CTP_TEST_FLUENCY
	&dev_attr_fts_test_fluency.attr,
	&dev_attr_fts_test_delay.attr, 
	&dev_attr_fts_test_interval.attr,		
#endif
	NULL,
};

static struct attribute_group fts_touch_group = {
	//.name = LGE_TOUCH_NAME,
	.attrs = fts_touch_attrs,
};


#ifdef LENOVO_CTP_EAGE_LIMIT
static int  tpd_edge_filter(int x, int y) {
	int ret = 0;
	if(y> TPD_RES_Y){//by pass key 
		if(x==270 ||x==540 ||x== 810){
			ret = 0;
			}else
			ret = 1;
		return ret ;
	}		
	if(x < eage_x || x> (TPD_RES_X- eage_x) || y<  eage_y  ||y> (TPD_RES_Y - eage_y))
	{
		dev_err(&fts_i2c_client->dev,"ahe  edge_filter :  (%d , %d ) dropped!\n",x,y);	
                  ret = 1;
          }
	return ret ;
}
#endif

static void fts_release_all_finger ( void )
{
	unsigned int finger_count=0;

#ifndef MT_PROTOCOL_B
	input_mt_sync ( tpd->dev );
#else
	for(finger_count = 0; finger_count < MT_MAX_TOUCH_POINTS; finger_count++)
	{
		input_mt_slot( tpd->dev, finger_count);
		input_mt_report_slot_state( tpd->dev, MT_TOOL_FINGER, false);
	}
#endif
	input_sync ( tpd->dev );

}

static int  fts_enter_gesture( void )
{
	int ret = 0;
	ret = fts_write_reg(fts_i2c_client, 0xd0, 0x01);
	if(ret < 0)
		dev_err(&fts_i2c_client->dev,"[fts][ahe] enter gesture write  fail 0xd0:  ");

	if (fts_updateinfo_curr.CHIP_ID==0x54 || fts_updateinfo_curr.CHIP_ID==0x58)
			{
	  	ret = fts_write_reg(fts_i2c_client, 0xd1, 0x33);
		if(ret < 0)
			dev_err(&fts_i2c_client->dev,"[fts][ahe] enter gesture write  fail 0xd1:  ");
		ret = fts_write_reg(fts_i2c_client, 0xd2, 0x01);
		if(ret < 0)
			dev_err(&fts_i2c_client->dev,"[fts][ahe] enter gesture write  fail 0xd2:  ");
		//fts_write_reg(fts_i2c_client, 0xd5, 0xff);
		ret = fts_write_reg(fts_i2c_client, 0xd6, 0x10);
		if(ret < 0)
			dev_err(&fts_i2c_client->dev,"[fts][ahe] enter gesture write  fail 0xd3:  ");
		//fts_write_reg(fts_i2c_client, 0xd7, 0xff);
		//fts_write_reg(fts_i2c_client, 0xd8, 0xff);
				}
	return ret ;
}

static int  fts_enter_glove( bool status )
{
	int ret = 0;
	static u8 buf_addr[2] = { 0 }; 
	static u8 buf_value[2] = { 0 };
	buf_addr[0]=0xC0; //glove control
	//buf_addr[1]=0x8B;//usb plug ?
	if(status)
		buf_value[0] = 0x01;
	else
		buf_value[0] = 0x00;
				
	ret = fts_write_reg(fts_i2c_client, buf_addr[0], buf_value[0]);
	if (ret<0) 
	{
		dev_err(&fts_i2c_client->dev, "[fts][Touch] fts_enter_glove write value fail \n");
	}
	dev_err(&fts_i2c_client->dev, "[fts][Touch] glove status :  %d \n",status);

	return ret ;

}
#if FTS_USB_DETECT
 int  fts_usb_insert( bool status )
{
	int ret = 0;
	u8 buf_addr[2] = { 0 }; 
	u8 buf_value[2] = { 0 };
	u8 tp_state = 0x03;
	//buf_addr[0]=0xC0; //glove control
	if(tpd_load_status ==0)
		return ret ;
	if((fts_i2c_client==NULL) || (tpd_halt==1)){
		//dev_err(&fts_i2c_client->dev, "[fts][Touch] fts_usb_insert return XXXX :  %d \n",status);
		return ret;
	}
	
	buf_addr[0]=0x8B;//usb plug ?
	if(status)
		buf_value[0] = 0x01;
	else
		buf_value[0] = 0x00;
	
	ret = fts_read_reg(fts_i2c_client, 0x8B,&tp_state);
	//ret = fts_read_reg(fts_i2c_client, buf_addr[0], tp_value[0]);
	//dev_err(&fts_i2c_client->dev, "[fts][Touch] fts_usb_insert status update 11111 :  (%d,%d) \n",tp_state,status);
	if(tp_state==buf_value[0]){
		//dev_err(&fts_i2c_client->dev, "[fts][Touch] fts_usb_insert status cancel update  :  (%d,%d) \n",tp_value[0],status);
		return ;
	}
			
	ret = fts_write_reg(fts_i2c_client, buf_addr[0], buf_value[0]);
	if (ret<0) 
	{
		dev_err(&fts_i2c_client->dev, "[fts][Touch] fts_usb_insert write value fail \n");
	}else
		dev_err(&fts_i2c_client->dev, "[fts][Touch] fts_usb_insert status update success  ~~~~~~ :  (%d,%d) \n",tp_state,status);

	
	return ret ;

}
#endif 

static int  fts_power_ctrl(bool en)
{
        if (en) {
		if(fts_power_down)
			hwPowerOn(TPD_POWER_SOURCE_CUSTOM, VOL_2800, "TP");
		fts_power_down = false;

	} else {
		if(!fts_power_down)
        			hwPowerDown(TPD_POWER_SOURCE_CUSTOM, "TP");
		fts_power_down = true;
	}
}

static int fts_workqueue_init ( void )
{
	//TPD_FUN ();

#ifdef TPD_AUTO_UPGRADE
	touch_wq = create_singlethread_workqueue ( "touch_wq" );
	if ( touch_wq )
	{
		INIT_WORK ( &fw_update_work, fts_fw_update_work_func );
		//wake_lock_init ( &fw_suspend_lock, WAKE_LOCK_SUSPEND, "fw_wakelock" );
	}
	else
	{
		goto err_workqueue_init;
	}
#endif

	return 0;

err_workqueue_init:
	printk ( "create_singlethread_workqueue failed\n" );
	return -1;
}
 //add by lixh10 end 

 
/************************************************************************
* Name: fts_i2c_read
* Brief: i2c read
* Input: i2c info, write buf, write len, read buf, read len
* Output: get data in the 3rd buf
* Return: fail <0
***********************************************************************/
int fts_i2c_read(struct i2c_client *client, char *writebuf,int writelen, char *readbuf, int readlen)
{
	int ret,i;
	int retry = 0 ;

	// for DMA I2c transfer
	
	mutex_lock(&i2c_rw_access);
	
	if(writelen!=0)
	{
		//DMA Write
		memcpy(g_dma_buff_va, writebuf, writelen);
		client->addr = client->addr & I2C_MASK_FLAG | I2C_DMA_FLAG;
		for(retry = 0; retry < I2C_MAX_TRY; retry++) {//add by lixh10 
			if((ret=i2c_master_send(client, (unsigned char *)g_dma_buff_pa, writelen))!=writelen){
			//dev_err(&client->dev, "###%s i2c write len=%x,buffaddr=%x\n", __func__,ret,*g_dma_buff_pa);
				dev_err(&fts_i2c_client->dev,"ahe r i2c_master_send failed retry %d\n",retry-1);
				msleep(20);
				}else
				break;
			if(tpd_halt)
			break ;
			}

		client->addr = client->addr & I2C_MASK_FLAG &(~ I2C_DMA_FLAG);
	}

	//DMA Read 
	if(readlen!=0)

	{
		client->addr = client->addr & I2C_MASK_FLAG | I2C_DMA_FLAG;
		for(retry = 0; retry < I2C_MAX_TRY; retry++) {//add by lixh10 
			if((ret = i2c_master_recv(client, (unsigned char *)g_dma_buff_pa, readlen))!=readlen){
				dev_err(&fts_i2c_client->dev,"ahe r  i2c_master_recv  failed retry %d\n",retry-1);
				msleep(20);
				}else
				break ;
			if(tpd_halt)
				break ;
			}
		memcpy(readbuf, g_dma_buff_va, readlen);
		client->addr = client->addr & I2C_MASK_FLAG &(~ I2C_DMA_FLAG);
	}
	
	mutex_unlock(&i2c_rw_access);
	
	return ret;

	/*
	int ret,i;


	if (writelen > 0) {
		struct i2c_msg msgs[] = {
			{
			 .addr = client->addr,
			 .flags = 0,
			 .len = writelen,
			 .buf = writebuf,
			 },
			{
			 .addr = client->addr,
			 .flags = I2C_M_RD,
			 .len = readlen,
			 .buf = readbuf,
			 },
		};
		ret = i2c_transfer(client->adapter, msgs, 2);
		if (ret < 0)
			dev_err(&client->dev, "f%s: i2c read error.\n",
				__func__);
	}
	else {
		struct i2c_msg msgs[] = {
			{
			 .addr = client->addr,
			 .flags = I2C_M_RD,
			 .len = readlen,
			 .buf = readbuf,
			 },
		};
		ret = i2c_transfer(client->adapter, msgs, 1);
		if (ret < 0)
			dev_err(&client->dev, "%s:i2c read error.\n", __func__);
	}
	return ret;
	*/

}

/************************************************************************
* Name: fts_i2c_write
* Brief: i2c write
* Input: i2c info, write buf, write len
* Output: no
* Return: fail <0
***********************************************************************/
int fts_i2c_write(struct i2c_client *client, char *writebuf, int writelen)
{
	int ret;
	//int i = 0;
	int retry = 0;

	mutex_lock(&i2c_rw_access);
	
 	//client->addr = client->addr & I2C_MASK_FLAG;

	//ret = i2c_master_send(client, writebuf, writelen);
	memcpy(g_dma_buff_va, writebuf, writelen);
	client->addr = client->addr & I2C_MASK_FLAG | I2C_DMA_FLAG;
	for(retry = 0; retry < I2C_MAX_TRY; retry++) {//add by lixh10 
		if((ret=i2c_master_send(client, (unsigned char *)g_dma_buff_pa, writelen))!=writelen){
			dev_err(&fts_i2c_client->dev,"ahe i2c_master_send failed retry :%d\n",retry-1);
			msleep(20);
			}else
			break;

		if(tpd_halt)
			break ;
	}
	
	client->addr = client->addr & I2C_MASK_FLAG &(~ I2C_DMA_FLAG);
		
	mutex_unlock(&i2c_rw_access);
	
	return ret;

	/*
	int ret;
	int i = 0;
	

   	client->addr = client->addr & I2C_MASK_FLAG;


	struct i2c_msg msg[] = {
		{
		 .addr = client->addr,
		 .flags = 0,
		 .len = writelen,
		 .buf = writebuf,
		 },
	};

	ret = i2c_transfer(client->adapter, msg, 1);
	if (ret < 0)
		dev_err(&client->dev, "%s i2c write error.\n", __func__);
	return ret;
	*/
}
/************************************************************************
* Name: fts_write_reg
* Brief: write register
* Input: i2c info, reg address, reg value
* Output: no
* Return: fail <0
***********************************************************************/
int fts_write_reg(struct i2c_client *client, u8 regaddr, u8 regvalue)
{
	unsigned char buf[2] = {0};

	buf[0] = regaddr;
	buf[1] = regvalue;

	return fts_i2c_write(client, buf, sizeof(buf));
}
/************************************************************************
* Name: fts_read_reg
* Brief: read register
* Input: i2c info, reg address, reg value
* Output: get reg value
* Return: fail <0
***********************************************************************/
int fts_read_reg(struct i2c_client *client, u8 regaddr, u8 *regvalue)
{

	return fts_i2c_read(client, &regaddr, 1, regvalue, 1);

}
/************************************************************************
* Name: tpd_down
* Brief: down info
* Input: x pos, y pos, id number
* Output: no
* Return: no
***********************************************************************/
static void tpd_down(int x, int y, int p) {
	
	if(x > TPD_RES_X)
	{
		TPD_DEBUG("warning: IC have sampled wrong value.\n");;
		return;
	}
	//dev_err(&fts_i2c_client->dev,"\n [zax] tpd_down (x=%d, y=%d,id=%d ) \n", x,y,p);
	input_report_key(tpd->dev, BTN_TOUCH, 1);
	input_report_abs(tpd->dev, ABS_MT_TOUCH_MAJOR, 20);
	input_report_abs(tpd->dev, ABS_MT_PRESSURE, 0x3f);
	input_report_abs(tpd->dev, ABS_MT_POSITION_X, x);
	input_report_abs(tpd->dev, ABS_MT_POSITION_Y, y);
	

	//printk("ahe tpd:D[%4d %4d %4d] ", x, y, p);
	//track id Start 0
     	//input_report_abs(tpd->dev, ABS_MT_TRACKING_ID, p); 
	input_mt_sync(tpd->dev);
     	if (FACTORY_BOOT == get_boot_mode()|| RECOVERY_BOOT == get_boot_mode())
     	{   
       	tpd_button(x, y, 1);  
     	}
	if(y > TPD_RES_Y) //virtual key debounce to avoid android ANR issue
	{
       	//msleep(50);
		//dev_err(&fts_i2c_client->dev,"fts D virtual key \n");
	 }
	 TPD_EM_PRINT(x, y, x, y, p-1, 1);
	 
 }
 /************************************************************************
* Name: tpd_up
* Brief: up info
* Input: x pos, y pos, count
* Output: no
* Return: no
***********************************************************************/
static  void tpd_up(int x, int y,int *count)
{
	 
	 input_report_key(tpd->dev, BTN_TOUCH, 0);
	 //dev_err(&fts_i2c_client->dev,"\n [zax] tpd_up (x=%d, y=%d,id=%d\n) ", x,y,*count);
	 input_mt_sync(tpd->dev);
	 TPD_EM_PRINT(x, y, x, y, 0, 0);

	if (FACTORY_BOOT == get_boot_mode()|| RECOVERY_BOOT == get_boot_mode())
	{   
		tpd_button(x, y, 0); 
	}
	if(y > TPD_RES_Y) //virtual key debounce to avoid android ANR issue
	{
       	//msleep(50);
		dev_err(&fts_i2c_client->dev,"fts U virtual key \n");
	 }
	
 }
 /************************************************************************
* Name: tpd_touchinfo
* Brief: touch info
* Input: touch info point, no use
* Output: no
* Return: success nonzero
***********************************************************************/

static int tpd_touchinfo(struct touch_info *cinfo, struct touch_info *pinfo,struct touch_info *ptest)
{
	int i = 0;
	char data[128] = {0};
       u16 high_byte,low_byte,reg;
	u8 report_rate =0;
	p_point_num = point_num;
	if (tpd_halt)
	{
		TPD_DMESG( "tpd_touchinfo return ..\n");
		return false;
	}
	//mutex_lock(&i2c_access);

       reg = 0x00;
	fts_i2c_read(fts_i2c_client, &reg, 1, data, 64);
	//mutex_unlock(&i2c_access);
	
	/*get the number of the touch points*/

	point_num= data[2] & 0x0f;
	if(up_flag==2)
	{
		up_flag=0;
		for(i = 0; i < fts_updateinfo_curr.TPD_MAX_POINTS; i++)  
		{
				cinfo->p[i] = data[3+6*i] >> 6; //event flag 
			
			cinfo->id[i] = data[3+6*i+2]>>4; //touch id
		   	/*get the X coordinate, 2 bytes*/
			high_byte = data[3+6*i];
			high_byte <<= 8;
			high_byte &= 0x0f00;
			low_byte = data[3+6*i + 1];
			cinfo->x[i] = high_byte |low_byte;	
			high_byte = data[3+6*i+2];
			high_byte <<= 8;
			high_byte &= 0x0f00;
			low_byte = data[3+6*i+3];
			cinfo->y[i] = high_byte |low_byte;

			
			if(point_num>=i+1)
				continue;
			if(up_count==0)
				continue;
			cinfo->p[i] = ptest->p[i-point_num]; //event flag 
			
			
			cinfo->id[i] = ptest->id[i-point_num]; //touch id
		   
			cinfo->x[i] = ptest->x[i-point_num];	
			
			cinfo->y[i] = ptest->y[i-point_num];
			//dev_err(&fts_i2c_client->dev," zax add two x = %d, y = %d, evt = %d,id=%d\n", cinfo->x[i], cinfo->y[i], cinfo->p[i], cinfo->id[i]);
			up_count--;
			
				
		}
		
		return true;
	}
	up_count=0;
	for(i = 0; i < fts_updateinfo_curr.TPD_MAX_POINTS; i++)  
	{
		
		cinfo->p[i] = data[3+6*i] >> 6; //event flag 
		
		if(0==cinfo->p[i])
		{
			//dev_err(&fts_i2c_client->dev,"\n  zax enter add   \n");
			up_flag=1;
     		}
		cinfo->id[i] = data[3+6*i+2]>>4; //touch id
	   	/*get the X coordinate, 2 bytes*/
		high_byte = data[3+6*i];
		high_byte <<= 8;
		high_byte &= 0x0f00;
		low_byte = data[3+6*i + 1];
		cinfo->x[i] = high_byte |low_byte;	
		high_byte = data[3+6*i+2];
		high_byte <<= 8;
		high_byte &= 0x0f00;
		low_byte = data[3+6*i+3];
		cinfo->y[i] = high_byte |low_byte;
		
		if(up_flag==1 && 1==cinfo->p[i])
		{
			up_flag=2;
			point_num++;
			ptest->x[up_count]=cinfo->x[i];
			ptest->y[up_count]=cinfo->y[i];
			ptest->id[up_count]=cinfo->id[i];
			ptest->p[up_count]=cinfo->p[i];
			//dev_err(&fts_i2c_client->dev," zax add x = %d, y = %d, evt = %d,id=%d\n", ptest->x[j], ptest->y[j], ptest->p[j], ptest->id[j]);
			cinfo->p[i]=2;
			up_count++;
		}
	}
	if(up_flag==1)
		up_flag=0;
	//printk(" tpd cinfo->x[0] = %d, cinfo->y[0] = %d, cinfo->p[0] = %d\n", cinfo->x[0], cinfo->y[0], cinfo->p[0]);
	return true;

};


 /************************************************************************
* Name: fts_read_Touchdata
* Brief: report the point information
* Input: event info
* Output: get touch data in pinfo
* Return: success is zero
***********************************************************************/
//zax static unsigned int buf_count_add=0;
//zax static unsigned int buf_count_neg=0;
//unsigned int buf_count_add1;
//unsigned int buf_count_neg1;
//u8 buf_touch_data[30*POINT_READ_BUF] = { 0 };//0xFF
static int fts_read_Touchdata(struct ts_event *data)//(struct ts_event *pinfo)
{
       u8 buf[POINT_READ_BUF] = { 0 };//0xFF
	int ret = -1;
	int i = 0;
	u8 pointid = FTS_MAX_ID;
	//u8 pt00f=0;
	if (tpd_halt)
	{
		TPD_DMESG( "fts while suspend cancel read touchdata, return ..\n");
		return ret;
	}

	//mutex_lock(&i2c_access);
	ret = fts_i2c_read(fts_i2c_client, buf, 1, buf, POINT_READ_BUF);
	if (ret < 0) 
	{
		dev_err(&fts_i2c_client->dev, "%s read touchdata failed.\n",__func__);
		//mutex_unlock(&i2c_access);
		return ret;
	}
#if 0
	for(i=0;i<POINT_READ_BUF;i++)
		{
		dev_err(&fts_i2c_client->dev,"\n [fts] zax buf %d =(0x%02x)  \n", i,buf[i]);
	}
#endif 
	//mutex_unlock(&i2c_access);
	memset(data, 0, sizeof(struct ts_event));
	data->touch_point = 0;
	
	data->touch_point_num=buf[FT_TOUCH_POINT_NUM] & 0x0F;
	//printk("tpd  fts_updateinfo_curr.TPD_MAX_POINTS=%d fts_updateinfo_curr.chihID=%d \n", fts_updateinfo_curr.TPD_MAX_POINTS,fts_updateinfo_curr.CHIP_ID);
	for (i = 0; i < fts_updateinfo_curr.TPD_MAX_POINTS; i++)
	{
		pointid = (buf[FTS_TOUCH_ID_POS + FTS_TOUCH_STEP * i]) >> 4;
		if (pointid >= FTS_MAX_ID)
			break;
		else
			data->touch_point++;
		data->au16_x[i] =
		    (s16) (buf[FTS_TOUCH_X_H_POS + FTS_TOUCH_STEP * i] & 0x0F) <<
		    8 | (s16) buf[FTS_TOUCH_X_L_POS + FTS_TOUCH_STEP * i];
		data->au16_y[i] =
		    (s16) (buf[FTS_TOUCH_Y_H_POS + FTS_TOUCH_STEP * i] & 0x0F) <<
		    8 | (s16) buf[FTS_TOUCH_Y_L_POS + FTS_TOUCH_STEP * i];
		data->au8_touch_event[i] =
		    buf[FTS_TOUCH_EVENT_POS + FTS_TOUCH_STEP * i] >> 6;
		data->au8_finger_id[i] =
		    (buf[FTS_TOUCH_ID_POS + FTS_TOUCH_STEP * i]) >> 4;

		data->pressure[i] =
			(buf[FTS_TOUCH_XY_POS + FTS_TOUCH_STEP * i]);//cannot constant value
		data->area[i] =
			(buf[FTS_TOUCH_MISC + FTS_TOUCH_STEP * i]) >> 4;
		if((data->au8_touch_event[i]==0 || data->au8_touch_event[i]==2)&&((data->touch_point_num==0)||(data->pressure[i]==0 && data->area[i]==0  )))
			return ret;
	//dev_err(&fts_i2c_client->dev,"\n [fts] zax data (id= %d ,x=%d,y= %d)\n ", data->au8_finger_id[i],data->au16_x[i],data->au16_y[i]);
		/*if(0==data->pressure[i])
		{
			data->pressure[i]=0x08;
		}
		if(0==data->area[i])
		{
			data->area[i]=0x08;
		}
		*/
		//if ( pinfo->au16_x[i]==0 && pinfo->au16_y[i] ==0)
		//	pt00f++;
	}
	ret = 0;
	//zax buf_count_add++;
	//buf_count_add1=buf_count_add;
	//zax memcpy( buf_touch_data+(((buf_count_add-1)%30)*POINT_READ_BUF), buf, sizeof(u8)*POINT_READ_BUF );
	return ret;
}

 /************************************************************************
* Name: fts_report_value
* Brief: report the point information
* Input: event info
* Output: no
* Return: success is zero
***********************************************************************/

static int fts_report_value(struct ts_event *data)
 {
	//struct ts_event *event = NULL;
	int i = 0,j=0;
	int up_point = 0;
 	int touchs = 0;
	int touchs_count = 0;


	#if GTP_ESD_PROTECT//change by fts 0708
	if(!is_turnoff_checkesd)
	{
		esd_switch(0);
		apk_debug_flag = 1;
		is_turnoff_checkesd = true;
	}
	#endif
	
	for (i = 0; i < data->touch_point; i++) 
	{
		 input_mt_slot(tpd->dev, data->au8_finger_id[i]);
 
		if (data->au8_touch_event[i]== 0 || data->au8_touch_event[i] == 2)
		{//down
		#ifdef LENOVO_CTP_EAGE_LIMIT
		if(tpd_edge_filter(data->au16_x[i],data->au16_y[i])){
			continue;	
			}
		#endif
	#ifdef FTS_INCALL_TOUCH_REJECTION
		if(g_call_state==CALL_ACTIVE){
			//dev_err(&fts_i2c_client->dev,"[fts] Incall  ~~~~~~");
			if (0==epl_sensor_show_ps_status_for_tp()) {
				dev_err(&fts_i2c_client->dev,"[fts] Incall with P-sensor  ,so cancel report ~~~~~~");
				continue;	
			}
		}
	#endif
			 input_mt_report_slot_state(tpd->dev, MT_TOOL_FINGER,true);
			 input_report_abs(tpd->dev, ABS_MT_PRESSURE,data->pressure[i]);//0x3f  data->pressure[i]
			 input_report_abs(tpd->dev, ABS_MT_TOUCH_MAJOR,data->area[i]);//0x05  data->area[i]
			 input_report_abs(tpd->dev, ABS_MT_POSITION_X,data->au16_x[i]);
			 input_report_abs(tpd->dev, ABS_MT_POSITION_Y,data->au16_y[i]);
			 touchs |= BIT(data->au8_finger_id[i]);
   			 data->touchs |= BIT(data->au8_finger_id[i]);
			//dev_err(&fts_i2c_client->dev,"[fts] D ID (%d ,%d, %d) ", data->au8_finger_id[i],data->au16_x[i],data->au16_y[i]);
		}
		else
		{//up
			//dev_err(&fts_i2c_client->dev,"[fts] U  ID: %d ", data->au8_finger_id[i]);
			 up_point++;
			 input_mt_report_slot_state(tpd->dev, MT_TOOL_FINGER,false);
			 data->touchs &= ~BIT(data->au8_finger_id[i]);
		}				 
		 
	}
 	if(unlikely(data->touchs ^ touchs)){
		for(i = 0; i < fts_updateinfo_curr.TPD_MAX_POINTS; i++){
			if(BIT(i) & (data->touchs ^ touchs)){
				up_point++; //fts change 2015-0701 for no up event.
				//dev_err(&fts_i2c_client->dev,"[fts] ~~U  ID: %d ", i);
				input_mt_slot(tpd->dev, i);
				input_mt_report_slot_state(tpd->dev, MT_TOOL_FINGER, false);
			}
		}
	}
	data->touchs = touchs;
	/*fts change 2015-0707 for no up event start */
	if((data->touch_point_num==0))
	{
		for(j = 0; j <fts_updateinfo_curr.TPD_MAX_POINTS; j++)
		{
			input_mt_slot( tpd->dev, j);
			input_mt_report_slot_state( tpd->dev, MT_TOOL_FINGER, false);
		}
		//last_touchpoint=0;
		data->touchs=0;
		up_point=fts_updateinfo_curr.TPD_MAX_POINTS;
		data->touch_point = up_point;
	}
	/*fts change 2015-0707 for no up event end */
	
	if(data->touch_point == up_point)
	{
		 input_report_key(tpd->dev, BTN_TOUCH, 0);

		 #if GTP_ESD_PROTECT  //change by fts 0708
			if(is_turnoff_checkesd)
			{
				esd_switch(1);
				apk_debug_flag = 0;
				is_turnoff_checkesd = false;
			}
		#endif
	}
	else
	{
		 input_report_key(tpd->dev, BTN_TOUCH, 1);
	}
 
	input_sync(tpd->dev);

	//last_touchpoint=data->touch_point_num;
	return 0;
    	//printk("tpd D x =%d,y= %d",event->au16_x[0],event->au16_y[0]);
 }

#ifdef TPD_PROXIMITY
 /************************************************************************
* Name: tpd_read_ps
* Brief: read proximity value
* Input: no
* Output: no
* Return: 0
***********************************************************************/
int tpd_read_ps(void)
{
	tpd_proximity_detect;
	return 0;    
}
 /************************************************************************
* Name: tpd_get_ps_value
* Brief: get proximity value
* Input: no
* Output: no
* Return: 0
***********************************************************************/
static int tpd_get_ps_value(void)
{
	return tpd_proximity_detect;
}
 /************************************************************************
* Name: tpd_enable_ps
* Brief: enable proximity
* Input: enable or not
* Output: no
* Return: 0
***********************************************************************/
static int tpd_enable_ps(int enable)
{
	u8 state;
	int ret = -1;
	
	//i2c_smbus_read_i2c_block_data(fts_i2c_client, 0xB0, 1, &state);

	ret = fts_read_reg(fts_i2c_client, 0xB0,&state);
	if (ret<0) 
	{
		printk("[Focal][Touch] read value fail");
		//return ret;
	}
	
	printk("[proxi_fts]read: 999 0xb0's value is 0x%02X\n", state);

	if (enable)
	{
		state |= 0x01;
		tpd_proximity_flag = 1;
		TPD_PROXIMITY_DEBUG("[proxi_fts]ps function is on\n");	
	}
	else
	{
		state &= 0x00;	
		tpd_proximity_flag = 0;
		TPD_PROXIMITY_DEBUG("[proxi_fts]ps function is off\n");
	}
	
	//ret = i2c_smbus_write_i2c_block_data(fts_i2c_client, 0xB0, 1, &state);
	ret = fts_write_reg(fts_i2c_client, 0xB0,state);
	if (ret<0) 
	{
		printk("[Focal][Touch] write value fail");
		//return ret;
	}
	TPD_PROXIMITY_DEBUG("[proxi_fts]write: 0xB0's value is 0x%02X\n", state);
	return 0;
}
 /************************************************************************
* Name: tpd_ps_operate
* Brief: operate function for proximity 
* Input: point, which operation, buf_in , buf_in len, buf_out , buf_out len, no use
* Output: buf_out
* Return: fail <0
***********************************************************************/
int tpd_ps_operate(void* self, uint32_t command, void* buff_in, int size_in,

		void* buff_out, int size_out, int* actualout)
{
	int err = 0;
	int value;
	hwm_sensor_data *sensor_data;
	TPD_DEBUG("[proxi_fts]command = 0x%02X\n", command);		
	
	switch (command)
	{
		case SENSOR_DELAY:
			if((buff_in == NULL) || (size_in < sizeof(int)))
			{
				APS_ERR("Set delay parameter error!\n");
				err = -EINVAL;
			}
			// Do nothing
			break;
		case SENSOR_ENABLE:
			if((buff_in == NULL) || (size_in < sizeof(int)))
			{
				APS_ERR("Enable sensor parameter error!\n");
				err = -EINVAL;
			}
			else
			{				
				value = *(int *)buff_in;
				if(value)
				{		
					if((tpd_enable_ps(1) != 0))
					{
						APS_ERR("enable ps fail: %d\n", err); 
						return -1;
					}
				}
				else
				{
					if((tpd_enable_ps(0) != 0))
					{
						APS_ERR("disable ps fail: %d\n", err); 
						return -1;
					}
				}
			}
			break;
		case SENSOR_GET_DATA:
			if((buff_out == NULL) || (size_out< sizeof(hwm_sensor_data)))
			{
				APS_ERR("get sensor data parameter error!\n");
				err = -EINVAL;
			}
			else
			{
				sensor_data = (hwm_sensor_data *)buff_out;				
				if((err = tpd_read_ps()))
				{
					err = -1;;
				}
				else
				{
					sensor_data->values[0] = tpd_get_ps_value();
					TPD_PROXIMITY_DEBUG("huang sensor_data->values[0] 1082 = %d\n", sensor_data->values[0]);
					sensor_data->value_divide = 1;
					sensor_data->status = SENSOR_STATUS_ACCURACY_MEDIUM;
				}					
			}
			break;
		default:
			APS_ERR("proxmy sensor operate function no this parameter %d!\n", command);
			err = -1;
			break;
	}
	return err;	
}
#endif

 /************************************************************************
* Name: touch_event_handler
* Brief: interrupt event from TP, and read/report data to Android system 
* Input: no use
* Output: no
* Return: 0
***********************************************************************/
 static int touch_event_handler(void *unused)
 {
	struct touch_info cinfo, pinfo,ptest;
#ifdef MT_PROTOCOL_B
	struct ts_event pevent;
#endif
	int i=0;
	int ret = 0;
	
	struct sched_param param = { .sched_priority = RTPM_PRIO_TPD };
	sched_setscheduler(current, SCHED_RR, &param);
 
	#ifdef TPD_PROXIMITY
		int err;
		hwm_sensor_data sensor_data;
		u8 proximity_status;
	#endif

	u8 state;
	do
	{
		//printk("fts  head \n");
		 mt_eint_unmask(CUST_EINT_TOUCH_PANEL_NUM); 
		 set_current_state(TASK_INTERRUPTIBLE); 
		 wait_event_interruptible(waiter,tpd_flag!=0);
						 
		 tpd_flag = 0;
			 
		 set_current_state(TASK_RUNNING);
		 //printk("tpd touch_event_handler\n");
	 	 #if FTS_GESTRUE_EN
		 if(fts_gesture_status){
			//i2c_smbus_read_i2c_block_data(fts_i2c_client, 0xd0, 1, &state);
			ret = fts_read_reg(fts_i2c_client, 0xd0,&state);
			if (ret<0) 
			{
				printk("[Focal][Touch] read value fail");
				//return ret;
			}
			//printk("tpd fts_read_Gestruedata state=%d\n",state);
		     	if(state ==1)
		     	{
			        fts_read_Gestruedata();
			        continue;
		    	}
		 }
		 #endif

		 #ifdef TPD_PROXIMITY

			 if (tpd_proximity_flag == 1)
			 {

				//i2c_smbus_read_i2c_block_data(fts_i2c_client, 0xB0, 1, &state);

				ret = fts_read_reg(fts_i2c_client, 0xB0,&state);
				if (ret<0) 
				{
					printk("[Focal][Touch] read value fail");
					//return ret;
				}
	           		TPD_PROXIMITY_DEBUG("proxi_fts 0xB0 state value is 1131 0x%02X\n", state);
				if(!(state&0x01))
				{
					tpd_enable_ps(1);
				}
				//i2c_smbus_read_i2c_block_data(fts_i2c_client, 0x01, 1, &proximity_status);
				ret = fts_read_reg(fts_i2c_client, 0x01,&proximity_status);
				if (ret<0) 
				{
					printk("[Focal][Touch] read value fail");
					//return ret;
				}
	            		TPD_PROXIMITY_DEBUG("proxi_fts 0x01 value is 1139 0x%02X\n", proximity_status);
				if (proximity_status == 0xC0)
				{
					tpd_proximity_detect = 0;	
				}
				else if(proximity_status == 0xE0)
				{
					tpd_proximity_detect = 1;
				}

				TPD_PROXIMITY_DEBUG("tpd_proximity_detect 1149 = %d\n", tpd_proximity_detect);
				if ((err = tpd_read_ps()))
				{
					TPD_PROXIMITY_DMESG("proxi_fts read ps data 1156: %d\n", err);	
				}
				sensor_data.values[0] = tpd_get_ps_value();
				sensor_data.value_divide = 1;
				sensor_data.status = SENSOR_STATUS_ACCURACY_MEDIUM;
				//if ((err = hwmsen_get_interrupt_data(ID_PROXIMITY, &sensor_data)))
				//{
				//	TPD_PROXIMITY_DMESG(" proxi_5206 call hwmsen_get_interrupt_data failed= %d\n", err);	
				//}
			}  

		#endif
                    
		#ifdef MT_PROTOCOL_B
		{
			//printk("********************111 ********************\n");
			#if GTP_ESD_PROTECT//change by fts 0708
			esd_switch(0);apk_debug_flag = 1;
			#endif
			
            		ret = fts_read_Touchdata(&pevent);
			if (ret == 0)
			fts_report_value(&pevent);

			#if GTP_ESD_PROTECT  //change by fts 0708
			esd_switch(1);apk_debug_flag = 0;
			#endif

			//printk("********************222********************\n");

		}
		#else
		{
			if (tpd_touchinfo(&cinfo, &pinfo,&ptest)) 
			{
		    		printk("tpd point_num = %d\n",point_num);
				TPD_DEBUG_SET_TIME;
				if(point_num >0) 
				{
				    for(i =0; i<point_num; i++)//only support 3 point
				    {
				    /*
					#ifdef LENOVO_CTP_EAGE_LIMIT
                                        	if(tpd_edge_filter(cinfo.x[i] ,cinfo.y[i]))
                                       	 {	
                                        		//printk("ahe edge_filter :  (%d , %d ) dropped!\n",cinfo.x[i],cinfo.y[i]);	
                                       		continue;
                                        	}
				    #endif
					*/
				         tpd_down(cinfo.x[i], cinfo.y[i], cinfo.id[i]);
				    }
				    input_sync(tpd->dev);
				}
				else  
	    			{
	              		tpd_up(cinfo.x[0], cinfo.y[0],&cinfo.id[0]);
	        	    		//TPD_DEBUG("release --->\n");         	   
	        	    		input_sync(tpd->dev);
	        		}
        		}
		}
		#endif
 	}while(!kthread_should_stop());
	return 0;
 }
  /************************************************************************
* Name: fts_reset_tp
* Brief: reset TP
* Input: pull low or high
* Output: no
* Return: 0
***********************************************************************/
void fts_reset_tp(int HighOrLow)
{
	
	if(HighOrLow)
	{
		mt_set_gpio_mode(GPIO_CTP_RST_PIN, GPIO_CTP_RST_PIN_M_GPIO);
	    	mt_set_gpio_dir(GPIO_CTP_RST_PIN, GPIO_DIR_OUT);
	    	mt_set_gpio_out(GPIO_CTP_RST_PIN, GPIO_OUT_ONE);  
	}
	else
	{
		mt_set_gpio_mode(GPIO_CTP_RST_PIN, GPIO_CTP_RST_PIN_M_GPIO);
	    	mt_set_gpio_dir(GPIO_CTP_RST_PIN, GPIO_DIR_OUT);
	    	mt_set_gpio_out(GPIO_CTP_RST_PIN, GPIO_OUT_ZERO);
	}
	
}
   /************************************************************************
* Name: tpd_detect
* Brief: copy device name
* Input: i2c info, board info
* Output: no
* Return: 0
***********************************************************************/
 static int tpd_detect (struct i2c_client *client, struct i2c_board_info *info) 
 {
	 	strcpy(info->type, TPD_DEVICE);	
	  	return 0;
 }
/************************************************************************
* Name: tpd_eint_interrupt_handler
* Brief: deal with the interrupt event
* Input: no
* Output: no
* Return: no
***********************************************************************/
 static void tpd_eint_interrupt_handler(void)
 {
	//dev_err(&fts_i2c_client->dev,"ahe fts  interrupt  %d~~~~\n",__LINE__);
	 TPD_DEBUG_PRINT_INT;
	 tpd_flag = 1;
	 #if GTP_ESD_PROTECT
		count_irq ++;
	 #endif

	 wake_up_interruptible(&waiter);
 }
/************************************************************************
* Name: fts_init_gpio_hw
* Brief: initial gpio
* Input: no
* Output: no
* Return: 0
***********************************************************************/
 static int fts_init_gpio_hw(void)
{

	int ret = 0;
	int i = 0;
	
	mt_set_gpio_mode(GPIO_CTP_RST_PIN, GPIO_CTP_RST_PIN_M_GPIO);
	mt_set_gpio_dir(GPIO_CTP_RST_PIN, GPIO_DIR_OUT);
	mt_set_gpio_out(GPIO_CTP_RST_PIN, GPIO_OUT_ONE);
	
	return ret;
}
/************************************************************************
* Name: tpd_probe
* Brief: driver entrance function for initial/power on/create channel 
* Input: i2c info, device id
* Output: no
* Return: 0
***********************************************************************/
 static int __devinit tpd_probe(struct i2c_client *client, const struct i2c_device_id *id)
 {	 
	int retval = TPD_OK;
	char data;
	u8 report_rate=0;
	int err=0;
	int reset_count = 0;
	unsigned char uc_reg_value;
	unsigned char uc_reg_addr;
	#ifdef TPD_PROXIMITY
		int err;
		struct hwmsen_object obj_ps;
	#endif

	reset_proc:   
		fts_i2c_client = client;
		fts_input_dev=tpd->dev;
#if 0
         #ifdef TPD_CLOSE_POWER_IN_SLEEP	 
		
	#else
		//reset should be pull down before power on/off    lixh10
		mt_set_gpio_mode(GPIO_CTP_RST_PIN, GPIO_CTP_RST_PIN_M_GPIO);
	    	mt_set_gpio_dir(GPIO_CTP_RST_PIN, GPIO_DIR_OUT);
	    	mt_set_gpio_out(GPIO_CTP_RST_PIN, GPIO_OUT_ZERO);  
		msleep(10);
		
	#endif	

	//power on, need confirm with SA
	#ifdef TPD_POWER_SOURCE_CUSTOM
		hwPowerOn(TPD_POWER_SOURCE_CUSTOM, VOL_2800, "TP");
	#else
		hwPowerOn(MT65XX_POWER_LDO_VGP2, VOL_2800, "TP");
	#endif
	
	#ifdef TPD_POWER_SOURCE_1800
		hwPowerOn(TPD_POWER_SOURCE_1800, VOL_1800, "TP");
	#endif 

	#ifdef TPD_CLOSE_POWER_IN_SLEEP	 
		hwPowerDown(TPD_POWER_SOURCE_CUSTOM,"TP");
		hwPowerOn(TPD_POWER_SOURCE_CUSTOM, VOL_2800, "TP");
		msleep(100);
	#else
		
		mt_set_gpio_mode(GPIO_CTP_RST_PIN, GPIO_CTP_RST_PIN_M_GPIO);
    		mt_set_gpio_dir(GPIO_CTP_RST_PIN, GPIO_DIR_OUT);
    		mt_set_gpio_out(GPIO_CTP_RST_PIN, GPIO_OUT_ZERO);  
		msleep(1);
		//TPD_DBG(" fts reset\n",__fun__);
		printk(" ahe fts reset \n");
		mt_set_gpio_mode(GPIO_CTP_RST_PIN, GPIO_CTP_RST_PIN_M_GPIO);
	    	mt_set_gpio_dir(GPIO_CTP_RST_PIN, GPIO_DIR_OUT);
	    	mt_set_gpio_out(GPIO_CTP_RST_PIN, GPIO_OUT_ONE);
	#endif	

		mt_set_gpio_mode(GPIO_CTP_EINT_PIN, GPIO_CTP_EINT_PIN_M_EINT);
    		mt_set_gpio_dir(GPIO_CTP_EINT_PIN, GPIO_DIR_IN);
    		mt_set_gpio_pull_enable(GPIO_CTP_EINT_PIN, GPIO_PULL_DISABLE);//change by lich10 
    		//mt_set_gpio_pull_select(GPIO_CTP_EINT_PIN, GPIO_PULL_UP);
#endif 
				//reset should be pull down before power on/off    lixh10
		mt_set_gpio_mode(GPIO_CTP_RST_PIN, GPIO_CTP_RST_PIN_M_GPIO);
	    	mt_set_gpio_dir(GPIO_CTP_RST_PIN, GPIO_DIR_OUT);
	    	mt_set_gpio_out(GPIO_CTP_RST_PIN, GPIO_OUT_ZERO);  
		msleep(10);
		
	//power on, need confirm with SA
	#ifdef TPD_POWER_SOURCE_CUSTOM
		//hwPowerOn(TPD_POWER_SOURCE_CUSTOM, VOL_2800, "TP");
		fts_power_ctrl(true);
	#endif
	

		mt_set_gpio_mode(GPIO_CTP_RST_PIN, GPIO_CTP_RST_PIN_M_GPIO);
    		mt_set_gpio_dir(GPIO_CTP_RST_PIN, GPIO_DIR_OUT);
    		mt_set_gpio_out(GPIO_CTP_RST_PIN, GPIO_OUT_ZERO);  
		msleep(1);
		//TPD_DBG(" fts reset\n",__fun__);
		printk("  fts do probe  reset \n");
		mt_set_gpio_mode(GPIO_CTP_RST_PIN, GPIO_CTP_RST_PIN_M_GPIO);
	    	mt_set_gpio_dir(GPIO_CTP_RST_PIN, GPIO_DIR_OUT);
	    	mt_set_gpio_out(GPIO_CTP_RST_PIN, GPIO_OUT_ONE);

		mt_set_gpio_mode(GPIO_CTP_EINT_PIN, GPIO_CTP_EINT_PIN_M_EINT);
    		mt_set_gpio_dir(GPIO_CTP_EINT_PIN, GPIO_DIR_IN);
    		mt_set_gpio_pull_enable(GPIO_CTP_EINT_PIN, GPIO_PULL_DISABLE);//change by lich10 
    		//mt_set_gpio_pull_select(GPIO_CTP_EINT_PIN, GPIO_PULL_DOWN);
    		
		msleep(200);

		err = i2c_smbus_read_i2c_block_data(fts_i2c_client, 0x00, 1, &data);
		//if auto upgrade fail, it will not read right value next upgrade.

	    	printk("fts i2c test :ret %d,data:%d\n", err,data);
   		 if(err< 0 || data!=0)// reg0 data running state is 0; other state is not 0
   		 {
        			printk("fts I2C  test transfer error, line: %d\n", __LINE__);

       			 if ( ++reset_count < TPD_MAX_RESET_COUNT )
        			{
            			goto reset_proc;
       			 }

			fts_power_ctrl(false);

        			return -1; 
   	         }


		msg_dma_alloct(client);
	
      		 fts_init_gpio_hw();

#if 0
	uc_reg_addr = FTS_REG_POINT_RATE;				
	retval = fts_i2c_write(fts_i2c_client, &uc_reg_addr, 1);
	 if(retval<0)
    	{
       	 	printk("mtk_tpd[FTS] write I2C error! driver NOt load!! .\n");
		goto I2C_FAIL; //change by lixh10 
	}
	retval = fts_i2c_read(fts_i2c_client, &uc_reg_addr, 0, &uc_reg_value, 1);
	 if(retval<0)
	 	goto I2C_FAIL; //change by lixh10 
		printk("mtk_tpd[FTS] report rate is %dHz.\n",uc_reg_value * 10);

	uc_reg_addr = FTS_REG_FW_VER;
	retval = fts_i2c_write(fts_i2c_client, &uc_reg_addr, 1);
	 if(retval<0)
	 	goto I2C_FAIL; //change by lixh10 
	retval = fts_i2c_read(fts_i2c_client, &uc_reg_addr, 0, &uc_reg_value, 1);
	 if(retval<0)
	 	goto I2C_FAIL; //change by lixh10 
		printk("mtk_tpd[FTS] Firmware version = 0x%x\n", uc_reg_value);


	uc_reg_addr = FTS_REG_CHIP_ID;
	retval = fts_i2c_write(fts_i2c_client, &uc_reg_addr, 1);
	 if(retval<0)
	 	goto I2C_FAIL; //change by lixh10 
	retval=fts_i2c_read(fts_i2c_client, &uc_reg_addr, 0, &uc_reg_value, 1);
	printk("mtk_tpd[FTS] chip id is %d.\n",uc_reg_value);
    	if(retval<0)
    	{
       	 	printk("mtk_tpd[FTS] Read I2C error! driver NOt load!! CTP chip id is %d.\n",uc_reg_value);
		goto I2C_FAIL; //change by lixh10 
	}
#endif 

	
	
	/*
	mt65xx_eint_set_sens(CUST_EINT_TOUCH_PANEL_NUM, CUST_EINT_TOUCH_PANEL_SENSITIVE);
	mt65xx_eint_set_hw_debounce(CUST_EINT_TOUCH_PANEL_NUM, CUST_EINT_TOUCH_PANEL_DEBOUNCE_CN);
	mt65xx_eint_registration(CUST_EINT_TOUCH_PANEL_NUM, CUST_EINT_TOUCH_PANEL_DEBOUNCE_EN, CUST_EINT_TOUCH_PANEL_POLARITY, tpd_eint_interrupt_handler, 1); 
	mt65xx_eint_unmask(CUST_EINT_TOUCH_PANEL_NUM);
	*/
	mt_eint_registration(CUST_EINT_TOUCH_PANEL_NUM, EINTF_TRIGGER_FALLING, tpd_eint_interrupt_handler, 1);

	
    	#ifdef VELOCITY_CUSTOM_fts
		if((err = misc_register(&tpd_misc_device)))
		{
			printk("mtk_tpd: tpd_misc_device register failed\n");
		
		}
	#endif

	#ifdef SYSFS_DEBUG
                fts_create_sysfs(fts_i2c_client);
	#endif
		HidI2c_To_StdI2c(fts_i2c_client);
		fts_get_upgrade_array();
	#ifdef FTS_CTL_IIC
		 if (fts_rw_iic_drv_init(fts_i2c_client) < 0)
			 dev_err(&client->dev, "%s:[FTS] create fts control iic driver failed\n", __func__);
	#endif
	
	#ifdef FTS_APK_DEBUG
		fts_create_apk_debug_channel(fts_i2c_client);
	#endif
	
	#ifdef TPD_PROXIMITY
		{
			obj_ps.polling = 1; //0--interrupt mode;1--polling mode;
			obj_ps.sensor_operate = tpd_ps_operate;
			if ((err = hwmsen_attach(ID_PROXIMITY, &obj_ps)))
			{
				TPD_DEBUG("hwmsen attach fail, return:%d.", err);
			}
		}
	#endif
	#if GTP_ESD_PROTECT
   		INIT_DELAYED_WORK(&gtp_esd_check_work, gtp_esd_check_func);
    		gtp_esd_check_workqueue = create_workqueue("gtp_esd_check");
    		queue_delayed_work(gtp_esd_check_workqueue, &gtp_esd_check_work, TPD_ESD_CHECK_CIRCLE);
	#endif 
	
	#ifdef LENOVO_CTP_TEST_FLUENCY
        		hrtimer_init(&tpd_test_fluency_timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
        		tpd_test_fluency_timer.function = tpd_test_fluency_handler;  
	#endif
	
	
	#if FTS_GESTRUE_EN
		fts_Gesture_init(tpd->dev);		
	#endif
	#ifdef MT_PROTOCOL_B
		//#if (LINUX_VERSION_CODE < KERNEL_VERSION(3, 8, 0))
			input_mt_init_slots(tpd->dev, MT_MAX_TOUCH_POINTS,0);
		//#endif
	#endif
		input_set_abs_params(tpd->dev, ABS_MT_TOUCH_MAJOR,0, 255, 0, 0);
		input_set_abs_params(tpd->dev, ABS_MT_WIDTH_MAJOR,	0, 255, 0, 0);
		input_set_abs_params(tpd->dev, ABS_MT_POSITION_X, 0, TPD_RES_X, 0, 0);
		input_set_abs_params(tpd->dev, ABS_MT_POSITION_Y, 0, TPD_RES_Y, 0, 0);
		input_set_abs_params(tpd->dev, ABS_MT_PRESSURE, 0, 255, 0, 0);
		//input_set_abs_params(tpd->dev, ABS_MT_TRACKING_ID, 0, 255, 0, 0);//add by lixh10 
	//add by lixh10 
	
	thread = kthread_run(touch_event_handler, 0, TPD_DEVICE);
	 if (IS_ERR(thread))
	{ 
		  retval = PTR_ERR(thread);
		  TPD_DMESG(TPD_DEVICE " failed to create kernel thread: %d\n", retval);
	}
	 #ifdef TPD_AUTO_UPGRADE
		err = fts_workqueue_init ();
		if ( err != 0 )
		{
			printk( "fts_workqueue_init failed\n" );
			//goto err_probing;
		}else
			queue_work ( touch_wq, &fw_update_work );	
	#endif
	 
	if (sysfs_create_group(&fts_i2c_client->dev.kobj , &fts_touch_group)) {
		dev_err(&client->dev, "failed to create sysfs group\n");
		return -EAGAIN;
	}
	if (sysfs_create_link(NULL, &fts_i2c_client->dev.kobj , "lenovo_tp_gestures")) {
		dev_err(&client->dev, "failed to create sysfs symlink\n");
		return -EAGAIN;
	}
	
	tpd_load_status = 1;

#ifndef TPD_AUTO_UPGRADE
#if FTS_USB_DETECT
	printk("mtk_tpd[fts] usb reg write \n");
	fts_usb_insert((bool) upmu_is_chr_det());
#endif 
	mt_eint_unmask(CUST_EINT_TOUCH_PANEL_NUM);
#endif 

   	printk("fts Touch Panel Device Probe %s\n", (retval < TPD_OK) ? "FAIL" : "PASS");
I2C_FAIL:
   	return 0;
   
 }
/************************************************************************
* Name: tpd_remove
* Brief: remove driver/channel
* Input: i2c info
* Output: no
* Return: 0
***********************************************************************/
 static int __devexit tpd_remove(struct i2c_client *client)
 
 {
     msg_dma_release(client);
if(tpd_load_status){ //add by lix10
     #ifdef FTS_CTL_IIC
     		fts_rw_iic_drv_exit();
     #endif
     #ifdef SYSFS_DEBUG
     		fts_remove_sysfs(client);
     #endif
     #if GTP_ESD_PROTECT
    		destroy_workqueue(gtp_esd_check_workqueue);
     #endif

     #ifdef FTS_APK_DEBUG
     		fts_release_apk_debug_channel();
     #endif
    #ifdef TPD_AUTO_UPGRADE
		cancel_work_sync(&fw_update_work);	
    #endif
}
	TPD_DEBUG("TPD removed\n");
 
   return 0;
 }
#if GTP_ESD_PROTECT
void esd_switch(s32 on)
{
    //spin_lock(&esd_lock); 
    if (1 == on) // switch on esd 
    {
    //lixh10 change 
	if(is_fw_upgrate==false)
            	queue_delayed_work(gtp_esd_check_workqueue, &gtp_esd_check_work, esd_check_circle);

    }
    else // switch off esd
    {
       // if (esd_running)
       // {
         //   esd_running = 0;
            //spin_unlock(&esd_lock);
            //printk("\n zax Esd cancell \n");
            cancel_delayed_work(&gtp_esd_check_work);
       // }
       // else
       // {
        //    spin_unlock(&esd_lock);
        //}
    }
}
/************************************************************************
* Name: force_reset_guitar
* Brief: reset
* Input: no
* Output: no
* Return: 0
***********************************************************************/
static void force_reset_guitar(void)
{
    	s32 i;
    	s32 ret;
	mt_eint_mask(CUST_EINT_TOUCH_PANEL_NUM);  
	mt_set_gpio_mode(GPIO_CTP_RST_PIN, GPIO_CTP_RST_PIN_M_GPIO);
	mt_set_gpio_dir(GPIO_CTP_RST_PIN, GPIO_DIR_OUT);
	mt_set_gpio_out(GPIO_CTP_RST_PIN, GPIO_OUT_ZERO);  
	msleep(5);
    	TPD_DMESG("force_reset_guitar\n");
	//hwPowerDown(MT6323_POWER_LDO_VGP1,  "TP");
	fts_power_ctrl(false);
	mdelay(100);
	//hwPowerOn(MT6323_POWER_LDO_VGP1, VOL_2800, "TP");
	fts_power_ctrl(true);


	mdelay(10);
	TPD_DMESG(" fts ic reset\n");
	mt_set_gpio_mode(GPIO_CTP_RST_PIN, GPIO_CTP_RST_PIN_M_GPIO);
	mt_set_gpio_dir(GPIO_CTP_RST_PIN, GPIO_DIR_OUT);
	mt_set_gpio_out(GPIO_CTP_RST_PIN, GPIO_OUT_ONE);
	
#ifdef TPD_PROXIMITY
	if (FT_PROXIMITY_ENABLE == tpd_proximity_flag) 
	{
		tpd_enable_ps(FT_PROXIMITY_ENABLE);
	}
#endif
	mdelay(50);
	mt_eint_unmask(CUST_EINT_TOUCH_PANEL_NUM);  
}

#define A3_REG_VALUE								0x54
#define RESET_91_REGVALUE_SAMECOUNT 				5
static u8 g_old_91_Reg_Value = 0x00;
static u8 g_first_read_91 = 0x01;
static u8 g_91value_same_count = 0;
/************************************************************************
* Name: gtp_esd_check_func
* Brief: esd check function
* Input: struct work_struct
* Output: no
* Return: 0
***********************************************************************/
static void gtp_esd_check_func(struct work_struct *work)
{
	int i;
	int ret = -1;
	u8 data, data_old;
	u8 flag_error = 0;
	int reset_flag = 0;
	u8 check_91_reg_flag = 0;
	//dev_err(&fts_i2c_client->dev,"fts esd polling ~~~~~");
	if (tpd_halt ) 
	{
		return;
	}
	if(apk_debug_flag) 
	{
		//queue_delayed_work(gtp_esd_check_workqueue, &gtp_esd_check_work, esd_check_circle);
		//printk("zax ESD  flag=%d",apk_debug_flag);
		return;
	}
	//printk("zax enetr ESD  flag=%d",apk_debug_flag);
	run_check_91_register = 0;
	for (i = 0; i < 3; i++) 
	{
		//ret = fts_i2c_smbus_read_i2c_block_data(i2c_client, 0xA3, 1, &data);
		ret = fts_read_reg(fts_i2c_client, 0xA3,&data);
		if (ret<0) 
		{
			printk("[Focal][Touch] read value fail");
			//return ret;
		}
		if (ret==1 && A3_REG_VALUE==data) 
		{
		    break;
		}
	}

	if (i >= 3) 
	{
		force_reset_guitar();
		printk("focal--tpd reset. i >= 3  ret = %d	A3_Reg_Value = 0x%02x\n ", ret, data);
		reset_flag = 1;
		goto FOCAL_RESET_A3_REGISTER;
	}

	//esd check for count
  	//ret = fts_i2c_smbus_read_i2c_block_data(i2c_client, 0x8F, 1, &data);
	ret = fts_read_reg(fts_i2c_client, 0x8F,&data);
	if (ret<0) 
	{
		printk("[Focal][Touch] read value fail");
		//return ret;
	}
	printk("0x8F:%d, count_irq is %d\n", data, count_irq);
			
	flag_error = 0;
	if((count_irq - data) > 10) 
	{
		if((data+200) > (count_irq+10) )
		{
			flag_error = 1;
		}
	}
	
	if((data - count_irq ) > 10) 
	{
		flag_error = 1;		
	}
		
	if(1 == flag_error) 
	{	
		printk("focal--tpd reset.1 == flag_error...data=%d	count_irq: %d\n ", data, count_irq);
	    	force_reset_guitar();
		reset_flag = 1;
		goto FOCAL_RESET_INT;
	}

	run_check_91_register = 1;
	//ret = fts_i2c_smbus_read_i2c_block_data(i2c_client, 0x91, 1, &data);
	ret = fts_read_reg(fts_i2c_client, 0x91,&data);
	if (ret<0) 
	{
		printk("[Focal][Touch] read value fail");
		//return ret;
	}
	printk("focal---------91 register value = 0x%02x	old value = 0x%02x\n",	data, g_old_91_Reg_Value);
	if(0x01 == g_first_read_91) 
	{
		g_old_91_Reg_Value = data;
		g_first_read_91 = 0x00;
	} 
	else 
	{
		if(g_old_91_Reg_Value == data)
		{
			g_91value_same_count++;
			printk("focal 91 value ==============, g_91value_same_count=%d\n", g_91value_same_count);
			if(RESET_91_REGVALUE_SAMECOUNT == g_91value_same_count) 
			{
				force_reset_guitar();
				printk("focal--tpd reset. g_91value_same_count = 5\n");
				g_91value_same_count = 0;
				reset_flag = 1;
			}
			
			//run_check_91_register = 1;
			esd_check_circle = TPD_ESD_CHECK_CIRCLE / 2;
			g_old_91_Reg_Value = data;
		} 
		else 
		{
			g_old_91_Reg_Value = data;
			g_91value_same_count = 0;
			//run_check_91_register = 0;
			esd_check_circle = TPD_ESD_CHECK_CIRCLE;
		}
	}
FOCAL_RESET_INT:
FOCAL_RESET_A3_REGISTER:
	count_irq=0;
	data=0;
	//fts_i2c_smbus_write_i2c_block_data(i2c_client, 0x8F, 1, &data);
	ret = fts_write_reg(fts_i2c_client, 0x8F,data);
	if (ret<0) 
	{
		printk("[Focal][Touch] write value fail");
		//return ret;
	}
	if(0 == run_check_91_register)
	{
		g_91value_same_count = 0;
	}
	#ifdef TPD_PROXIMITY
	if( (1 == reset_flag) && ( FT_PROXIMITY_ENABLE == tpd_proximity_flag) )
	{
		if((tpd_enable_ps(FT_PROXIMITY_ENABLE) != 0))
		{
			APS_ERR("enable ps fail\n"); 
			return -1;
		}
	}
	#endif
	//end esd check for count

    	if (!tpd_halt)
    	{
        	//queue_delayed_work(gtp_esd_check_workqueue, &gtp_esd_check_work, TPD_ESD_CHECK_CIRCLE);
        		queue_delayed_work(gtp_esd_check_workqueue, &gtp_esd_check_work, esd_check_circle);
    	}

    	return;
}
#endif

 /************************************************************************
* Name: tpd_local_init
* Brief: add driver info
* Input: no
* Output: no
* Return: fail <0
***********************************************************************/
 static int tpd_local_init(void)
 {
  	TPD_DMESG("Focaltech fts I2C Touchscreen Driver (Built %s @ %s)\n", __DATE__, __TIME__);
   	if(i2c_add_driver(&tpd_i2c_driver)!=0)
   	{
        	TPD_DMESG("fts unable to add i2c driver.\n");
      		return -1;
    	}
    	if(tpd_load_status == 0) 
    	{
       		TPD_DMESG("fts add error touch panel driver.\n");
    		i2c_del_driver(&tpd_i2c_driver);
    		return -1;
    	}
	//TINNO_TOUCH_TRACK_IDS <--- finger number
	//TINNO_TOUCH_TRACK_IDS	5
	#if (LINUX_VERSION_CODE > KERNEL_VERSION(3, 8, 0))
		//for linux 3.8
		input_set_abs_params(tpd->dev, ABS_MT_TRACKING_ID, 0, (TPD_MAX_POINTS_5-1), 0, 0);
	#endif
	
	
   	#ifdef TPD_HAVE_BUTTON     
		// initialize tpd button data
    	 	tpd_button_setting(TPD_KEY_COUNT, tpd_keys_local, tpd_keys_dim_local);
	#endif   
  
	#if (defined(TPD_WARP_START) && defined(TPD_WARP_END))    
    		TPD_DO_WARP = 1;
    		memcpy(tpd_wb_start, tpd_wb_start_local, TPD_WARP_CNT*4);
    		memcpy(tpd_wb_end, tpd_wb_start_local, TPD_WARP_CNT*4);
	#endif 

	#if (defined(TPD_HAVE_CALIBRATION) && !defined(TPD_CUSTOM_CALIBRATION))
    		memcpy(tpd_calmat, tpd_def_calmat_local, 8*4);
    		memcpy(tpd_def_calmat, tpd_def_calmat_local, 8*4);	
	#endif  
	TPD_DMESG("end %s, %d\n", __FUNCTION__, __LINE__);  
	tpd_type_cap = 1;
    	return 0; 
 }
 /************************************************************************
* Name: tpd_resume
* Brief: system wake up 
* Input: no use
* Output: no
* Return: no
***********************************************************************/
 static void tpd_resume( struct early_suspend *h )
 {
	//int i=0,ret = 0;
	
 	dev_err(&fts_i2c_client->dev,"fts wake up~~~~\n");
	
  	#ifdef TPD_PROXIMITY	
		if (tpd_proximity_flag == 1)
		{
			if(tpd_proximity_flag_one == 1)
			{
				tpd_proximity_flag_one = 0;	
				dev_err(&fts_i2c_client->dev," fts tpd_proximity_flag_one \n"); 
				return;
			}
		}
	#endif	
 	#if FTS_GESTRUE_EN
	if(fts_gesture_status){
    		//fts_write_reg(fts_i2c_client,0xD0,0x00); //only reset can exit 
		fts_gesture_status = false; //reset ic so exit 
		dev_err(&fts_i2c_client->dev,"[fts][ahe] exit  gesture $$$$$$$ ");
	}
	#endif
	#ifdef TPD_CLOSE_POWER_IN_SLEEP	
		mt_set_gpio_mode(GPIO_CTP_RST_PIN, GPIO_CTP_RST_PIN_M_GPIO);
		mt_set_gpio_dir(GPIO_CTP_RST_PIN, GPIO_DIR_OUT);
		mt_set_gpio_out(GPIO_CTP_RST_PIN, GPIO_OUT_ZERO);  
		mdelay(10);
		fts_power_ctrl(true);
	#else
		//just do reset change by lixh10 
		mt_set_gpio_mode(GPIO_CTP_RST_PIN, GPIO_CTP_RST_PIN_M_GPIO);
    		mt_set_gpio_dir(GPIO_CTP_RST_PIN, GPIO_DIR_OUT);
    		mt_set_gpio_out(GPIO_CTP_RST_PIN, GPIO_OUT_ZERO);  
    		mdelay(10);  
	#endif
    		mt_set_gpio_mode(GPIO_CTP_RST_PIN, GPIO_CTP_RST_PIN_M_GPIO);
    		mt_set_gpio_dir(GPIO_CTP_RST_PIN, GPIO_DIR_OUT);
    		mt_set_gpio_out(GPIO_CTP_RST_PIN, GPIO_OUT_ONE);
		mdelay(150);//why delay ??
		tpd_halt = 0;
#ifdef FTS_GLOVE_EN
		if(fts_glove_flag)
			fts_enter_glove(true);
#endif
	
#if FTS_USB_DETECT
	//printk("mtk_tpd[fts] usb reg write \n");
	fts_usb_insert((bool) upmu_is_chr_det());
#endif 
	mt_eint_ack(CUST_EINT_TOUCH_PANEL_NUM);//add by lixh10 for unhandle int 
	mt_eint_unmask(CUST_EINT_TOUCH_PANEL_NUM);  
	fts_release_all_finger();
	
#if GTP_ESD_PROTECT
                count_irq = 0;
    		queue_delayed_work(gtp_esd_check_workqueue, &gtp_esd_check_work, TPD_ESD_CHECK_CIRCLE);
#endif
	dev_err(&fts_i2c_client->dev,"fts wake up done\n");

 }
 /************************************************************************
* Name: tpd_suspend
* Brief: system sleep
* Input: no use
* Output: no
* Return: no
***********************************************************************/
 static void tpd_suspend( struct early_suspend *h )
 {
	static char data = 0x3;
	int i=0,ret = 0;
	
	dev_err(&fts_i2c_client->dev,"fts  enter sleep~~~~\n");
	#ifdef TPD_PROXIMITY
	if (tpd_proximity_flag == 1)
	{
		tpd_proximity_flag_one = 1;	
		return;
	}
	#endif

	#if GTP_ESD_PROTECT
	
    		cancel_delayed_work_sync(&gtp_esd_check_work);
	#endif
#if FTS_GESTRUE_EN
	if(fts_wakeup_flag&&(!fts_gesture_status)){
		fts_gesture_status = true ;
	}
#endif
	//mutex_lock(&i2c_access);
#if FTS_GESTRUE_EN
	if(fts_gesture_status){
		ret = fts_enter_gesture();
		if(ret>=0)
			dev_err(&fts_i2c_client->dev,"[fts][ahe] enter gesture $$$$$$$ ");
		#ifdef FTS_GLOVE_EN
		if(fts_glove_flag)
			fts_enter_glove(false);
#endif
	}else{
#endif
	dev_err(&fts_i2c_client->dev,"[fts][ahe] no gesture will enter normal sleep ~~~~ ");
	 tpd_halt = 1;
	 mt_eint_mask(CUST_EINT_TOUCH_PANEL_NUM);
	#ifdef TPD_CLOSE_POWER_IN_SLEEP
		mt_set_gpio_mode(GPIO_CTP_RST_PIN, GPIO_CTP_RST_PIN_M_GPIO);
		mt_set_gpio_dir(GPIO_CTP_RST_PIN, GPIO_DIR_OUT);
		mt_set_gpio_out(GPIO_CTP_RST_PIN, GPIO_OUT_ZERO);  
		mdelay(10);
		fts_power_ctrl(false);
	#else
		if ((fts_updateinfo_curr.CHIP_ID==0x59))
		{
			ret = fts_write_reg(fts_i2c_client, 0xA5,data);
			if (ret<0){
				printk("[Focal][Touch] write value fail");
				//return ret;
			}
		}else{
			data = 0x03;//change by lixh10 
			ret = fts_write_reg(fts_i2c_client, 0xA5,data);
			dev_err(&fts_i2c_client->dev,"[fts][ahe] suspend enter low power mode !!  ");
			if (ret<0) {
				printk("[Focal][Touch] write value fail");
				//return ret;
			}
		}
	#endif
#if FTS_GESTRUE_EN
	}
#endif
	//mutex_unlock(&i2c_access);
	fts_release_all_finger();
    	dev_err(&fts_i2c_client->dev,"fts fts enter sleep done\n");

 } 


 static struct tpd_driver_t tpd_device_driver = {
       	 .tpd_device_name = "fts",
		 .tpd_local_init = tpd_local_init,
		 .suspend = tpd_suspend,
		 .resume = tpd_resume,
	
	#ifdef TPD_HAVE_BUTTON
		 .tpd_have_button = 1,
	#else
		 .tpd_have_button = 0,
	#endif
	
 };

  /************************************************************************
* Name: tpd_suspend
* Brief:  called when loaded into kernel
* Input: no
* Output: no
* Return: 0
***********************************************************************/
 static int __init tpd_driver_init(void) {
        printk("MediaTek fts touch panel driver init\n");
        //i2c_register_board_info(I2C_CAP_TOUCH_CHANNEL, &fts_i2c_tpd, 1);//change by lixh10
	        i2c_register_board_info(IIC_PORT, &fts_i2c_tpd, 1);

if(tpd_driver_add(&tpd_device_driver) < 0)
       	TPD_DMESG("add fts driver failed\n");
	 return 0;
 }
 
 
/************************************************************************
* Name: tpd_driver_exit
* Brief:  should never be called
* Input: no
* Output: no
* Return: 0
***********************************************************************/
 static void __exit tpd_driver_exit(void) 
 {
        TPD_DMESG("MediaTek fts touch panel driver exit\n");
	 //input_unregister_device(tpd->dev);
	 tpd_driver_remove(&tpd_device_driver);
 }
 
 module_init(tpd_driver_init);
 module_exit(tpd_driver_exit);
