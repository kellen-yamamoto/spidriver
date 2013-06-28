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

#define PFX "gpio: "

#define GPIO1 17
#define GPIO2 27
#define GPIO3 23
#define GPIO4 24

#define SPI_BUS 1
#define SPI_BUS_CS1 0
#define SPI_BUS_CS2 1
#define SPI_BUS_SPEED 10000000


/*---------- Driver Info ------------*/
static struct spi_gpio_platform_data spi_master_data = {
	.sck = GPIO1,
	.mosi = GPIO2,
	.miso = GPIO3,
	.num_chipselect = 1
};

struct gpio_data {
	int cmd;
};


static struct platform_device spi_master = {
	.name = "spi_gpio",
	.id = SPI_BUS, // bus number
	.dev = {
		.platform_data = (void *) &spi_master_data,
	}
};

/*---------- Register Access ------------*/
static int gpio_read8(struct spi_device *spi, int cmd)
{
	int ret;
	ret = spi_w8r8(spi, cmd);
	return ret;
}

/*
static int gpio_read16(struct spi_device *spi, int cmd)
{
	int ret;
	ret = spi_w8r16(spi, cmd);
	return ret;
}
*/

static int gpio_write8(struct spi_device *spi, int cmd, u8 val)
{
	u8 tmp[2] = {cmd, val};
	return spi_write(spi, tmp, sizeof(tmp));
}

/*---------- SYSFS Interface ------------*/

static ssize_t show_cmd(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct gpio_data *pdata = dev_get_drvdata(dev);
	return sprintf(buf, "%d\n", pdata->cmd);
}

static ssize_t set_cmd(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	struct gpio_data *pdata = dev_get_drvdata(dev);
	
	u8 cmd;
	int error;
	
	error = kstrtou8(buf, 10, &cmd);
	if (error)
		return error;
	pdata->cmd = cmd;
	printk("Wrote %d to addr\n", cmd);
	
	return count;
}

static DEVICE_ATTR(cmd, S_IWUSR | S_IRUGO, show_cmd, set_cmd);

static ssize_t show_data(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct spi_device *spi = to_spi_device(dev);
	struct gpio_data *pdata = dev_get_drvdata(dev);
	return sprintf(buf, "%d\n", gpio_read8(spi, pdata->cmd));
}

static ssize_t set_data(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	struct spi_device *spi = to_spi_device(dev);
	struct gpio_data *pdata = dev_get_drvdata(dev);
	
	u8 val;
	int error;
		
	error = kstrtou8(buf, 10, &val);
	if (error)
		return error;
	
	gpio_write8(spi, pdata->cmd, val);
	return count;
}
	
static DEVICE_ATTR(data, S_IWUSR | S_IRUGO, show_data, set_data);
 
static struct attribute *gpio_attributes[] = {
	&dev_attr_cmd.attr,
	&dev_attr_data.attr,
	NULL
};

static const struct attribute_group gpio_group = {
	.attrs = gpio_attributes,
};

/*---------- Module Setup/Cleanup ------*/

static int __init add_gpio_device_to_bus(void)
{
	struct spi_master *spi_master;
	struct spi_device *spi_device;
	struct device *pdev;
	struct gpio_data *pdata;
	char buff[64];
	int err;
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

	spi_device->chip_select = SPI_BUS_CS1;

	/* Check whether this SPI bus.cs is already claimed */
	snprintf(buff, sizeof(buff), "%s.%u", 
			dev_name(&spi_device->master->dev),
			spi_device->chip_select);

	pdev = bus_find_device_by_name(spi_device->dev.bus, NULL, buff);
	if (pdev) {
		spi_dev_put(spi_device);

		if (pdev->driver && pdev->driver->name && 
				strcmp("gpio", pdev->driver->name)) {
			printk(KERN_ALERT 
					"Driver [%s] already registered for %s\n",
					pdev->driver->name, buff);
			status = -1;
		} 
	} 
	else {
		spi_device->max_speed_hz = SPI_BUS_SPEED;
		spi_device->mode = SPI_MODE_0;
		spi_device->bits_per_word = 8;
		spi_device->irq = -1;
		spi_device->controller_state = NULL;
		spi_device->controller_data = (void*) 2;
		strlcpy(spi_device->modalias, "gpio", SPI_NAME_SIZE);

		status = spi_add_device(spi_device); 
		if (status < 0) { 
			spi_dev_put(spi_device);
			printk(KERN_ALERT "spi_add_device() failed: %d\n", 
					status); 
		} 
		else {
			/* Add client data */
			pdata = devm_kzalloc(&spi_device->dev, sizeof(struct gpio_data), GFP_KERNEL);
			if (!pdata)
				return -ENOMEM;
			spi_set_drvdata(spi_device, pdata);
			
			/* Add sysfs files */
			err = sysfs_create_group(&spi_device->dev.kobj, &gpio_group);
			if(err < 0)
				return err;
		}
	}
	put_device(&spi_master->dev);

	return status;
}




static int __init gpio_modinit(void)
{
	printk(KERN_INFO PFX "init.\n");

	platform_device_register(&spi_master);

	printk(KERN_INFO PFX "add_gpio_to_bus()...\n");
	add_gpio_device_to_bus();
	printk(KERN_INFO PFX "add_gpio_to_bus() done.\n");

	return 0;
}
module_init(gpio_modinit);

static void __exit gpio_modexit(void)
{
	printk(KERN_INFO PFX "exit.\n");
}
module_exit(gpio_modexit);


MODULE_AUTHOR("Kellen Yamamoto");
MODULE_DESCRIPTION("Generic GPIO SPI driver with sysfs interface");
MODULE_LICENSE("GPL");
