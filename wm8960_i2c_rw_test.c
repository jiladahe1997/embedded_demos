#include <linux/kernel.h>

//struct file_operations;register_chrdev_region
#include <linux/fs.h>           

// cdev
#include <linux/cdev.h>       

// kzalloc
#include <linux/slab.h>         

// MODULE_LICENSE / MODULE_AUTHOR / MODULE_DESCRIPTION
#include <linux/module.h>

#include <linux/i2c.h>

// copy_to_user
#include <linux/uaccess.h>

#define WM8960_I2C_RW_TEST_MAJOR 0       //默认主设备号，为0代表动态申请
static unsigned int wm8960_i2c_rw_test_major = WM8960_I2C_RW_TEST_MAJOR;
#define WM8960_I2C_RW_TEST_MINOR 0       //次设备号
#define WM8960_I2C_RW_TEST_MINOR_COUNT 1 //次设备号数量

static int wm8960_i2c_rw_test_open(struct inode * inode, struct file * filp){
    printk("wm8960_i2c_rw_test open \r\n");
    return 0;
} 

static ssize_t wm8960_i2c_rw_test_write(struct file * filp, const char __user * buffer, 
    size_t count, loff_t * offp) {
    printk("wm8960_i2c_rw_test write \r\n");
    return count;
}

static ssize_t wm8960_i2c_rw_test_read(struct file * filp, char __user * buffer, 
    size_t count, loff_t * offp) {
    __u8 reg = 2;
    __u8 recv_buffer[32] = {0};
    struct i2c_adapter * adapter = NULL;
    struct i2c_msg msg[] = {{
        .addr = 0x38,
        .flags = 0,
        .buf = &reg,
        .len = 1,       
    },{
        .addr = 0x38,
        .flags = I2C_M_RD,
        .buf = recv_buffer,
        .len = ARRAY_SIZE(recv_buffer)
    }};
	int ret = 0;
    char ret_buffer[128] = {0};

    printk("wm8960_i2c_rw_test read \r\n");

    /* I2C read WM8960 registers */
    adapter = i2c_get_adapter(1);
    if(!adapter){
        printk("i2c adpter get fail \r\n");
        goto error;
    }


	ret = i2c_transfer(adapter, msg, ARRAY_SIZE(msg));
    if(ret != ARRAY_SIZE(msg)) {
        printk("i2c transfer error, ret is %d \r\n", ret);
        goto error;
    }

    ret = snprintf(ret_buffer, ARRAY_SIZE(ret_buffer), 
        "WM8960 register 0-4 is %#X ,%#X, %#X, %#X \r\n",
        recv_buffer[0], recv_buffer[1], recv_buffer[2], recv_buffer[3]);
    if(ret < 0) {
        printk("snprintf error \r\n");
        goto error;
    }

    ret = copy_to_user(buffer, ret_buffer,count);
    if(ret != 0) {
        printk("copy to user fail \r\n");
        goto error;
    }

    return count;

error:
    return -EFAULT;
}

static struct file_operations wm8960_i2c_rw_test_ops = {
    .open = wm8960_i2c_rw_test_open,
    .write = wm8960_i2c_rw_test_write,
    .read = wm8960_i2c_rw_test_read
};

/**
 * @brief  初始化驱动
 */
dev_t dev = 0;                             // 设备号
struct cdev * cdev = NULL;                 // 字符驱动

static int wm8960_i2c_rw_test_init(void) {
    int ret = 0;                               // 通用返回值
    printk("wm8960_i2c_rw_test_init \r\n");

    /* 动态申请设备号 */
    if(wm8960_i2c_rw_test_major) {
        dev = MKDEV(wm8960_i2c_rw_test_major, WM8960_I2C_RW_TEST_MINOR);
        ret = register_chrdev_region(dev, 
            WM8960_I2C_RW_TEST_MINOR_COUNT, "wm8960_i2c_rw_test");
    }else{
        ret = alloc_chrdev_region(&dev, WM8960_I2C_RW_TEST_MINOR, 
            WM8960_I2C_RW_TEST_MINOR_COUNT, "wm8960_i2c_rw_test");
        wm8960_i2c_rw_test_major = MAJOR(dev);
    }

    if(ret < 0) {
        printk("wm8960_i2c_rw_test register dev number fail \r\n");
        return ret;
    }

    /* 注册字符设备 */
    cdev = kzalloc(sizeof(struct cdev), GFP_KERNEL);
    cdev_init(cdev, &wm8960_i2c_rw_test_ops);
    ret = cdev_add(cdev, dev, WM8960_I2C_RW_TEST_MINOR_COUNT);
    if(ret < 0) {
        printk("cdev add fail \r\n");
        kfree(cdev);
        return ret;
    }
    printk("wm8960_i2c_rw_test init successful!\n");
    return 0;
}

static void wm8960_i2c_rw_test_exit(void){
    printk("wm8960_i2c_rw_test_exit \r\n");
    /* 取消注册设备 */
    cdev_del(cdev);

    /* 取消设备号 */
    unregister_chrdev_region(dev, WM8960_I2C_RW_TEST_MINOR_COUNT);
    printk("unregister chardev ok\n");
    return;

}

module_init(wm8960_i2c_rw_test_init);
module_exit(wm8960_i2c_rw_test_exit);

MODULE_AUTHOR("RenMing rui");
MODULE_DESCRIPTION("renmingrui lcd control driver");
MODULE_LICENSE("GPL");