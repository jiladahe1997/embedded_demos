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

#include <linux/regmap.h>

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

static struct device * wm8960_dev = NULL;
extern struct bus_type i2c_bus_type;
static int bus_itertor(struct device * dev, void * param){
    printk("now device name is : %s \r\n", dev_name(dev));
    if(strncmp("1-001a", dev_name(dev), strlen("1-001a")) == 0){
        wm8960_dev = dev;
        return 1;
    }
    return 0;
}

static ssize_t wm8960_i2c_rw_test_read(struct file * filp, char __user * buffer, 
    size_t count, loff_t * offp) { 
    int ret = 0;
    struct regmap * WM8960_regmap = NULL;
    unsigned int value = 0;
    struct i2c_client * i2c_client = NULL;

    /* get i2c1 device */
    ret = bus_for_each_dev(&i2c_bus_type, NULL, NULL, bus_itertor);
    if(ret == 0) {
        printk("can not find device by name \r\n");
        goto error;
    }
    if(wm8960_dev == NULL){
        printk("i2c_bus_dev is null \r\n");
        goto error;
    }

    i2c_client = kobj_to_i2c_client(&wm8960_dev->kobj);
    if(i2c_client == NULL){
        printk("kobj_to_i2c_client error \r\n");
        goto error;        
    }
    printk("get wm8960 dev ok , dev name is %s \r\n", dev_name(wm8960_dev));

    /* get remap */
    // WM8960_regmap = dev_get_regmap(wm8960_dev, NULL);
    WM8960_regmap = dev_get_regmap(&i2c_client->dev, NULL);
    if(WM8960_regmap == NULL) {
        printk("get regmap error \r\n");
        goto error;
    }
    ret = regmap_read(WM8960_regmap, 2, &value);
    if(ret != 0){
        printk("reg map read error, error code is %d \r\n", ret);
        goto error;
    }
    printk("read WM8960 register ok, value is %#X \r\n", value);

    return 0;
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