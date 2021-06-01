#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/fs.h>           // struct file_operations
#include <linux/slab.h>         // kzalloc
#include <linux/cdev.h>         // cdev
#include <linux/gpio.h>                     // gpio legency相关驱动
// #include <linux/gpio/consumer.h>         // gpio customer新驱动（需要依赖platform）
#include <asm/io.h>                    //  I/O 相关函数

#define LCD_CONTROL_MAJOR 0
static unsigned int lcd_control_major = LCD_CONTROL_MAJOR;   // 默认主设备号

#define LCD_CONTROL_GPIO_NUMBER 8     //硬件上GPIO为 GPIO1_GPIO8 (${GPIO1}-1)*32+${GPIO8}+1=9

static int lcd_control_open(struct inode * inode, struct file * filp){
    printk("lcd_control open\n");
    return 0;
}


static ssize_t lcd_control_write (struct file * filp, const char __user * buffer, size_t count, loff_t * offp) {
    int status=0;
    printk("lcd_control write\n");
    status = gpio_get_value(LCD_CONTROL_GPIO_NUMBER);
    printk("now lcd status is %d\n", status);
    if(status == 0){
        gpio_set_value(LCD_CONTROL_GPIO_NUMBER, 1);
    }else {
        gpio_set_value(LCD_CONTROL_GPIO_NUMBER, 0);
    }
    return count;
}

static ssize_t lcd_control_read (struct file * filp, char __user * buffer, size_t count, loff_t * offp) {
    printk("lcd_control read\n");
    return 1;
}


static struct file_operations lcd_control_fops = {  
    .open = lcd_control_open,
    .write = lcd_control_write,
    .read = lcd_control_read
};


static dev_t dev;           // 设备号
struct cdev * cdev;         // 字符设备结构体 cdev
static int lcd_control_init(void) {
    int ret=0;
    void __iomem * addr = NULL;
    unsigned int value = 0;
    unsigned int new_value = 0;;
    printk("lcd_control init\n");


    /**
     * 注册gpio
     */
    printk("starting register gpio %d\n", LCD_CONTROL_GPIO_NUMBER);
    ret = gpio_request_one(LCD_CONTROL_GPIO_NUMBER, GPIOF_DIR_OUT|GPIOF_INIT_LOW, "lcd_control");
    if(ret < 0){
        printk("gpio request fail\n");
        return ret;
    }

    printk("Start set IOMUXC_SW_MUX_CTL_PAD_GPIO1_IO08 register\n");
    addr = ioremap(0x020E007C, 1);
    value = ioread32(addr);
    printk("Read register value is: %#X \n", value);
    printk("Write SION bit to register ... \n");
    new_value = value | (1 << 4);
    iowrite32(new_value, addr);
    printk("Done \n");
    value = ioread32(addr);
    printk("Read register value is: %#X \n", value);
    iounmap(addr);

    /* 申请设备号 */
    if(lcd_control_major) {
        dev = MKDEV(lcd_control_major,1);
        ret = register_chrdev_region(dev, 1, "lcd_control");        
    }else {
        dev = 0;
        ret = alloc_chrdev_region(&dev, 0, 1, "lcd_control");
        lcd_control_major = MAJOR(dev);
    }

    if(ret < 0) {
        printk("device major number register fail!\n");
        return ret;
    }

    /* 注册设备 */
    /* only work for platform driver */
    // struct device * dev = NULL;
    // dev = devm_kzalloc(dev, sizeof(*dev), GFP_KERNEL);
    cdev = kzalloc(sizeof(*cdev), GFP_KERNEL);
    cdev_init(cdev, &lcd_control_fops);
    ret = cdev_add(cdev, dev, 1);
    if(ret < 0) {
        printk("cdev add fail");
        return ret;
    }

    printk("lco_control init successful!\n");

    
    return 0;
}

static void lcd_control_exit(void) {
    printk("lcd_control exit\n");
    gpio_free(LCD_CONTROL_GPIO_NUMBER);
    /* 取消注册设备 */
    cdev_del(cdev);

    /* 取消设备号 */
    unregister_chrdev_region(dev, 1);
    printk("unregister chardev ok\n");
    return;
}



module_init(lcd_control_init);
module_exit(lcd_control_exit);

MODULE_AUTHOR("RenMing rui");
MODULE_DESCRIPTION("renmingrui lcd control driver");
MODULE_LICENSE("GPL");