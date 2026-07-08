#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/uaccess.h>
#include <linux/slab.h>
#include <linux/gpio/consumer.h>
#include <linux/mod_devicetable.h>
#include <linux/of.h>
#include <linux/spinlock.h>

/*The char devices in driver*/
struct rti_ls_en_chrdev
{
    struct cdev dev;
    spinlock_t spinlock;
    struct gpio_desc * rti_ls_en;
};

struct rti_ls_addr_chrdev
{
    struct cdev dev;
    spinlock_t spinlock;
    struct gpio_descs * rti_ls_addr;
};

struct rti_ls_pdrv_data
{
    struct rti_ls_en_chrdev * rti_ls_en_cdev;
    struct rti_ls_addr_chrdev * rti_ls_addr_cdev;
    dev_t devno;
    struct class * class;
};

/*Operations*/
static int pdrv_rti_ls_open(struct inode *inode, struct file *filp)
{
    filp->private_data = (void *)inode->i_cdev;

    return 0;
}

static ssize_t pdrv_rti_ls_en_write(struct file *filp, const char __user *buf,
                                    size_t count, loff_t *ppos)
{
    struct rti_ls_en_chrdev * rti_ls_en_cdev = (struct rti_ls_en_chrdev *)filp->private_data;
    unsigned int val = 0;
    unsigned long flags;

    if(copy_from_user(&val, buf, 1))
        return -EFAULT;
    if(val >= 2)
        printk("RTI light source 'enable' input value error! %u\r\n", (unsigned int)val);
    else
    {
        spin_lock_irqsave(&rti_ls_en_cdev->spinlock, flags);
        gpiod_set_value(rti_ls_en_cdev->rti_ls_en, val);
        spin_unlock_irqrestore(&rti_ls_en_cdev->spinlock, flags);
    }

    return count;
}

static ssize_t pdrv_rti_ls_addr_write(struct file *filp, const char __user *buf,
                                        size_t count, loff_t *ppos)
{
    struct rti_ls_addr_chrdev * rti_ls_addr_cdev = (struct rti_ls_addr_chrdev *)filp->private_data;
    unsigned long index = 0;
    unsigned long flags;

    if(copy_from_user(&index, buf, 1))
        return -EFAULT;
    if(index >= 16)
        printk("RTI light source 'address' input value error!%u\r\n", (unsigned int)index);
    else
    {
        spin_lock_irqsave(&rti_ls_addr_cdev->spinlock, flags);
        gpiod_set_array_value(rti_ls_addr_cdev->rti_ls_addr->ndescs, 
                              rti_ls_addr_cdev->rti_ls_addr->desc,
                              NULL,
                              &index);
        spin_unlock_irqrestore(&rti_ls_addr_cdev->spinlock, flags);
    }

    return count;
}

static struct file_operations pdrv_rti_ls_en_fops = 
{
    .owner = THIS_MODULE,
    .open = pdrv_rti_ls_open,
    .write = pdrv_rti_ls_en_write,
};

static struct file_operations pdrv_rti_ls_addr_fops = 
{
    .owner = THIS_MODULE,
    .open = pdrv_rti_ls_open,
    .write = pdrv_rti_ls_addr_write,
};

/*Driver*/
static int pdrv_rti_ls_probe(struct platform_device *pdev)
{
    struct rti_ls_pdrv_data * pdrv_data = NULL;
    struct rti_ls_en_chrdev * rti_ls_en_cdev = NULL;
    struct rti_ls_addr_chrdev * rti_ls_addr_cdev = NULL;
    dev_t devno;
    dev_t major, minor;
    struct device * device;
    struct class * class;
    int ret = 0;

    /*Get the resource from platform device*/
    rti_ls_en_cdev = devm_kzalloc(&pdev->dev, sizeof(* rti_ls_en_cdev), GFP_KERNEL);
    rti_ls_addr_cdev = devm_kzalloc(&pdev->dev, sizeof(* rti_ls_addr_cdev), GFP_KERNEL);
    if((!rti_ls_en_cdev) || (!rti_ls_addr_cdev))
        return -ENOMEM;

    rti_ls_en_cdev->rti_ls_en = devm_gpiod_get(&pdev->dev, "EL", GPIOD_OUT_LOW);
    if(IS_ERR(rti_ls_en_cdev->rti_ls_en))
    {
        ret = PTR_ERR(rti_ls_en_cdev->rti_ls_en);
        goto ioremap_err;
    }
    rti_ls_addr_cdev->rti_ls_addr = devm_gpiod_get_array(&pdev->dev, "ADDR", GPIOD_OUT_HIGH);
    if(IS_ERR(rti_ls_addr_cdev->rti_ls_addr))
    {
        ret = PTR_ERR(rti_ls_addr_cdev->rti_ls_addr);
        goto ioremap_err;
    }

    /*Create char devices*/
    ret = alloc_chrdev_region(&devno, 0, 2, "RTI_light_source");
    if(ret < 0)
        goto ioremap_err;
    major = MAJOR(devno);
    minor = MINOR(devno);

    cdev_init(&rti_ls_en_cdev->dev, &pdrv_rti_ls_en_fops);
    rti_ls_en_cdev->dev.owner = THIS_MODULE;
    ret = cdev_add(&rti_ls_en_cdev->dev, MKDEV(major, minor), 1);
    if(ret < 0)
        goto add_err;
    cdev_init(&rti_ls_addr_cdev->dev, &pdrv_rti_ls_addr_fops);
    rti_ls_addr_cdev->dev.owner = THIS_MODULE;
    ret = cdev_add(&rti_ls_addr_cdev->dev, MKDEV(major, minor + 1), 1);
    if(ret < 0)
        goto add_err;
    
    class = class_create(THIS_MODULE, "RTI_light_source");
    if(IS_ERR(class))
    {
        ret = PTR_ERR(class);
        goto class_err;
    }

    device = device_create(class, &pdev->dev, MKDEV(major, minor), NULL, "rti_ls_en");
    if(IS_ERR(device))
    {
        ret = PTR_ERR(device);
        goto device_err;
    }
    device = device_create(class, &pdev->dev, MKDEV(major, minor + 1), NULL, "rti_ls_addr");
    if(IS_ERR(device))
    {
        ret = PTR_ERR(device);
        goto device_err;
    }

    /*Init spinlocks*/
    spin_lock_init(&rti_ls_en_cdev->spinlock);
    spin_lock_init(&rti_ls_addr_cdev->spinlock);

    /*Save driver data*/
    pdrv_data = kmalloc(sizeof(* pdrv_data), GFP_KERNEL);
    pdrv_data->class = class;
    pdrv_data->devno = devno;
    pdrv_data->rti_ls_addr_cdev = rti_ls_addr_cdev;
    pdrv_data->rti_ls_en_cdev = rti_ls_en_cdev;
    platform_set_drvdata(pdev, pdrv_data);

    /*Done*/
    printk("RTI_LS_DRV_DONE!\r\n");
    return 0;

/*Error*/
device_err:
    class_destroy(class);

class_err:
    cdev_del(&rti_ls_en_cdev->dev);
    cdev_del(&rti_ls_addr_cdev->dev);

add_err:
    unregister_chrdev_region(devno, 2);

ioremap_err:
    return ret;
}

static int pdrv_rti_ls_remove(struct platform_device *pdev)
{
    struct rti_ls_pdrv_data * pdrv_data = (struct rti_ls_pdrv_data *)platform_get_drvdata(pdev);
    dev_t major, minor;

    major = MAJOR(pdrv_data->devno);
    minor = MINOR(pdrv_data->devno);
    device_destroy(pdrv_data->class, MKDEV(major, minor));
    device_destroy(pdrv_data->class, MKDEV(major, minor + 1));
    cdev_del(&pdrv_data->rti_ls_en_cdev->dev);
    cdev_del(&pdrv_data->rti_ls_addr_cdev->dev);
    unregister_chrdev_region(pdrv_data->devno, 2);
    class_destroy(pdrv_data->class);
    kfree(pdrv_data);

    return 0;
}

static const struct of_device_id rti_ls_of_match[] = 
{
    {.compatible = "harry,rti_light_source"},
    {}//NULL
};
MODULE_DEVICE_TABLE(of, rti_ls_of_match);

static struct platform_driver pdrv_rti_ls = 
{
    .probe = pdrv_rti_ls_probe,
    .remove = pdrv_rti_ls_remove,
    .driver = {
        .name = "RTI_light_source",
        .of_match_table = rti_ls_of_match,
    },
};

/*Module*/
static __init int pdrv_init(void)
{
    platform_driver_register(&pdrv_rti_ls);

    return 0;
}

static __exit void pdrv_exit(void)
{
    platform_driver_unregister(&pdrv_rti_ls);
}

module_init(pdrv_init);
module_exit(pdrv_exit);

MODULE_AUTHOR("Harry");
MODULE_DESCRIPTION("RTI_light_source_driver");
MODULE_LICENSE("GPL");
