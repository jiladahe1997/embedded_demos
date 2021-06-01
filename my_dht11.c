/**
 * @brief dht11驱动，自己编写，采用device tree + 驱动的形式.
 *        配合tfpt加载kernel+dht
 *        配合nfs加载rootfs
 *
 * @link https://www.yuque.com/jiladahe1997/kk0t7q/qhw2mg
 * 
 * @author jiladahe1997 (https://github.com/jiladahe1997)
 * @date   2021/04/25
 */ 
#include <linux/module.h>
MODULE_AUTHOR("Ren Mingrui <972931182@qq.com>");
MODULE_DESCRIPTION("DHT11 driver Re-write");
MODULE_LICENSE("GPL");

#include <linux/platform_device.h> // platform_driver
#include <linux/of_gpio.h>         // of_xxx API
#include <linux/gpio.h>            // gpio_request API
#include <linux/kthread.h>         // kthread_run API
#include <linux/delay.h>           // udelay      API
#include <linux/interrupt.h>       // request_irq API
#include <linux/timekeeping.h>     // ktime_get_ns API

struct my_dht11_info
{
    int gpio_index;
    int irq;
    struct device *dev;
    struct task_struct * thread_handle;
    u8 dht11_data[5];  //dht11数据 5*8=40共40位
    u8 dht11_data_trigger_count;
    u64 dht11_data_last_trigger_jiffies;
    u64 debug_time[41];
    u64 debug_jiffies[41];
};

/* TODO 不要在bss段申请，改为堆上 */
static struct my_dht11_info m_dht11_info = {0};


static const struct of_device_id dht11_dt_ids[] = {
    { .compatible = "my_dht11" }
};

static irqreturn_t dht11_irq_handle(int irq, void *data){
    //printk("dht11_irq_handle trigger\r\n");
    u64 now = ktime_get_ns();
    u8 dht11_data = 0;
    unsigned char dht11_byte = 0;
    unsigned char dht11_bit = 0;
    u64 duration_us = 0;
    unsigned int trigger_count = 0;

    duration_us = now - m_dht11_info.dht11_data_last_trigger_jiffies;
    m_dht11_info.dht11_data_last_trigger_jiffies = now;
    m_dht11_info.debug_time[m_dht11_info.dht11_data_trigger_count] = duration_us;
    m_dht11_info.debug_jiffies[m_dht11_info.dht11_data_trigger_count] = now;

    m_dht11_info.dht11_data_trigger_count++;
    /* 第一位是响应信号 */
    if(m_dht11_info.dht11_data_trigger_count == 1)
        return IRQ_HANDLED;

    /* trigger_count: 1..2..3..4..5..6..7..8 ...40*/
    trigger_count = m_dht11_info.dht11_data_trigger_count - 1;
    dht11_byte = (trigger_count-1) / 8;
    dht11_bit = 7 - (trigger_count-1)  % 8;
    dht11_data = (duration_us > 100*1000) ? 1 : 0;
    dht11_data <<= dht11_bit;
    m_dht11_info.dht11_data[dht11_byte] |= dht11_data;

    return IRQ_HANDLED;
}

/**
 * @brief 读取dht11的数据，阻塞式读取使用固定时间作为延时
 */
static int dht11_read_thread_entry(void *data){
    int ret = 0;
    int humidity_integer = 0;
    int humidity_decimal = 0;
    int temperature_integer = 0;
    int temperature_decimal = 0;
    int i=0;
    u64 t1;
    u64 t2;
    u64 t3;
    dev_info(m_dht11_info.dev, "dht11 start");

    /* 等待1s越过DHT11不稳定状态 */
    mdelay(1000);

    do
    {
        ret = gpio_direction_output(m_dht11_info.gpio_index,0);

        if(ret < 0) 
            dev_err(m_dht11_info.dev, "gpio_direction_output error, error code is: [%d]", ret);

        t1 = ktime_get_ns();
        mdelay(25); //dht11启动延时18ms
        t2 = ktime_get_ns();
        
        ret = gpio_direction_input(m_dht11_info.gpio_index);
        if(ret < 0) 
            dev_err(m_dht11_info.dev, "gpio_direction_input error, error code is: [%d]", ret);
        
        ret = devm_request_irq(m_dht11_info.dev,m_dht11_info.irq, 
                            dht11_irq_handle, IRQF_TRIGGER_FALLING, "dht11_iqr", NULL);
        if(ret < 0){
            dev_err(m_dht11_info.dev, "request_irqerror, error code is: [%d]", ret);
        }   
        t3 = ktime_get_ns();
        mdelay(20); //dht11传输40位数据 40×100us

        devm_free_irq(m_dht11_info.dev, m_dht11_info.irq, NULL);

        /* 打印数据 */
        humidity_integer = 1* m_dht11_info.dht11_data[0] ;
        humidity_decimal = m_dht11_info.dht11_data[1] & (~0x80) ;
        humidity_integer *= (m_dht11_info.dht11_data[1] & 0x80) ? -1:1;
        temperature_integer = 1* m_dht11_info.dht11_data[2] ;
        temperature_decimal = m_dht11_info.dht11_data[3] & (~0x80) ;
        temperature_integer *= (m_dht11_info.dht11_data[3] & 0x80) ? -1:1;

        for(i=0;i<41;i++) {
            dev_info(m_dht11_info.dev, "jiffies[%d]:[%lld],duration[%d]:[%lld]",i, m_dht11_info.debug_jiffies[i],
                i, m_dht11_info.debug_time[i]);
        }
        for(i=0;i<4;i++){
            dev_info(m_dht11_info.dev, "dht11_data[%d]:[%Xd]",i, m_dht11_info.dht11_data[i]);
        }
        dev_info(m_dht11_info.dev,"trigger_count: [%d]",m_dht11_info.dht11_data_trigger_count);

        dev_info(m_dht11_info.dev, "t1:[%lld],t2:[%lld],t3:[%lld]",t1,t2,t3);
        dev_info(m_dht11_info.dev, "humidity: [%d.%d], temperature: [%d.%d]", 
            humidity_integer, humidity_decimal, temperature_integer, temperature_decimal);
        /* clear */
        m_dht11_info.dht11_data_last_trigger_jiffies = 0;
        m_dht11_info.dht11_data_trigger_count = 0;
        for(i=0;i<4;i++){
            m_dht11_info.dht11_data[i]=0;
        }

        /* 5s一次 */
        dev_info(m_dht11_info.dev, "dht11 done");
        mdelay(5000);
    } while (!kthread_should_stop());

    return 0;   
}

/**
 * @brief 驱动注册函数
 */ 
static int dht11_probe(struct platform_device *pdev){
    int ret = 0;
    /* 这里必须使用指针，不能使用结构体赋值 */
    // m_dht11_info.dev = pdev->dev;
    m_dht11_info.dev = &pdev->dev;

    /* 从设备树拿GPIO */
    ret = of_get_gpio(m_dht11_info.dev->of_node, 0);
    if(ret < 0) {
        dev_err(m_dht11_info.dev, "gpio get error, error code is: [%d]", ret);
        return ret;
    } else {
        m_dht11_info.gpio_index = ret;
        dev_info(m_dht11_info.dev, "gpio get ok, index: [%d]", m_dht11_info.gpio_index);
    }

    /* 注册GPIO，并设置为开漏模式(上拉输入) */
    ret = devm_gpio_request_one(m_dht11_info.dev, m_dht11_info.gpio_index, GPIOF_OUT_INIT_HIGH, "dht11");
    // ret = gpio_request_one(m_dht11_info.gpio_index, GPIOF_OUT_INIT_HIGH, "dht11");
    if(ret < 0){
        dev_err(m_dht11_info.dev, "gpio request error, error code is: [%d]", ret);
        return ret;
    }

    m_dht11_info.irq = gpio_to_irq(m_dht11_info.gpio_index);
    if(m_dht11_info.irq < 0){
        dev_err(m_dht11_info.dev, "gpio to irq error, error code is: [%d]", m_dht11_info.irq);
        return m_dht11_info.irq;
    }                    

    /* 启动一个线程，每隔5秒打印一次温湿度信息 */
    m_dht11_info.thread_handle = kthread_run(dht11_read_thread_entry, NULL, "dht11_thread");

    return 0;
}

static int dht11_remove(struct platform_device *pdev){
    int ret = 0;

    ret = kthread_stop(m_dht11_info.thread_handle);
    if(ret < 0){
        dev_err(m_dht11_info.dev, "kthread stop error, error code is: [%d]", ret);
        return ret;
    }

    return 0;
}

static struct platform_driver dht11_driver = {
    .driver = {
        .name = "my_dht11",
        .of_match_table = dht11_dt_ids
    },
    .probe = dht11_probe,
    .remove = dht11_remove
};

module_platform_driver(dht11_driver)