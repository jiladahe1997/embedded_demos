
# Add your debugging flag (or not) to CFLAGS

ifneq ($(KERNELRELEASE),)
# obj-m := lcd_control.o
obj-m := wm8960_i2c_test_2_regmap.o
obj-m := my_dht11.o

else 
KERNELDIR ?= /home/renmingrui/workspace/linux-imx
PWD := $(shell pwd)

default:
	$(MAKE) -C $(KERNELDIR) M=$(PWD) modules ARCH=arm CROSS_COMPILE=arm-linux-gnueabihf- 

endif 

clean:
	rm -rf *.o *~ core .depend .*.cmd *.ko *.mod.c .tmp_versions