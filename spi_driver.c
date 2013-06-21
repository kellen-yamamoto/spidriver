#include <linux/clk.h>
#include <linux/etherdevice.h>
#include <linux/gpio.h>
#include <linux/irq.h>
#include <linux/i2c.h>
#include <linux/i2c-gpio.h>
#include <linux/init.h>
#include <linux/linkage.h>
#include <linux/platform_device.h>
#include <linux/types.h>
#include <linux/leds.h>
#include <linux/spi/spi.h>
#include <linux/spi/spi_gpio.h>
#include <linux/spi/spi_bitbang.h>
#include <linux/gpio.h>
#include <linux/module.h>

#define PFX "halboard: "

#define SPI_BUS 1
#define SPI_BUS_CS1 0
#define SPI_BUS_CS2 1
#define SPI_BUS_SPEED 1500000

#define GPIO1 17
#define GPIO2 27
#define GPIO3 23
#define GPIO4 24

/*
 * SPI GPIO master
 */

static struct spi_gpio_platform_data spi_master_data = {
 .sck = GPIO1,
 .mosi = GPIO2,
 .miso = GPIO3,
 .num_chipselect = 1
};



static struct platform_device spi_master = {
 .name = "spi_gpio",
 .id = SPI_BUS, // bus number
 .dev = {
 .platform_data = (void *) &spi_master_data,
 }
};


static int __init add_sdcard_device_to_bus(void)
{
	struct spi_master *spi_master;
	struct spi_device *spi_device;
	struct device *pdev;
	char buff[64];
	int status = 0;

	spi_master = spi_busnum_to_master(SPI_BUS);
	if (!spi_master) {
		printk(KERN_ALERT "spi_busnum_to_master(%d) returned NULL\n",
				SPI_BUS);
		printk(KERN_ALERT "Missing modprobe spi_gpio?\n");
		return -1;
	}

	spi_device = spi_alloc_device(spi_master);
	if (!spi_device) {
		put_device(&spi_master->dev);
		printk(KERN_ALERT "spi_alloc_device() failed\n");
		return -1;
	}

	spi_device->chip_select = -EINVAL;

	/* Check whether this SPI bus.cs is already claimed */
/*
	snprintf(buff, sizeof(buff), "%s.%u", 
			dev_name(&spi_device->master->dev),
			spi_device->chip_select);

	pdev = bus_find_device_by_name(spi_device->dev.bus, NULL, buff);
	if (pdev) {
		spi_dev_put(spi_device);

		if (pdev->driver && pdev->driver->name && 
				strcmp("mmc_spi", pdev->driver->name)) {
			printk(KERN_ALERT 
					"Driver [%s] already registered for %s\n",
					pdev->driver->name, buff);
			status = -1;
		} 
	} else {
*/
		spi_device->max_speed_hz = SPI_BUS_SPEED;
		spi_device->mode = SPI_NO_CS;
		spi_device->bits_per_word = 8;
		spi_device->irq = -1;
		spi_device->controller_state = NULL;
		spi_device->controller_data = (void*) 3;
		strlcpy(spi_device->modalias, "mmc_spi", SPI_NAME_SIZE);

		status = spi_add_device(spi_device); 
		if (status < 0) { 
			spi_dev_put(spi_device);
			printk(KERN_ALERT "spi_add_device() failed: %m\n", 
					status); 
		}
 
//	}

	put_device(&spi_master->dev);

	return status;
}



static int __init halboard_modinit(void)
{
 printk(KERN_INFO PFX "init.\n");

 platform_device_register(&spi_master);
 printk(KERN_INFO PFX "platform_device_register() done. sdcard is next.\n");

 printk(KERN_INFO PFX "add_sdcard_to_bus()...\n");
 add_sdcard_device_to_bus();
 printk(KERN_INFO PFX "add_sdcard_to_bus() done.\n");


 return 0;
}
module_init(halboard_modinit);

static void __exit halboard_modexit(void)
{
 printk(KERN_INFO PFX "exit.\n");
}
module_exit(halboard_modexit);


MODULE_AUTHOR("Guenter Bartsch");
MODULE_DESCRIPTION("halboard module - spi setup for HAL 9000 board");
MODULE_LICENSE("GPL");
