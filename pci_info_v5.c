#include <linux/kernel.h> /* Для работы с ядром. */ 
#include <linux/module.h> /* Для модулей. */ 
#include <linux/proc_fs.h> /* Для использования procfs.*/ 
#include <linux/uaccess.h> /* Для copy_from_user. */ 
#include <linux/version.h>
#include <linux/pci.h>
#include <linux/init.h>

#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 6, 0) 
#define HAVE_PROC_OPS 
#endif 
 
#define procfs_name "pci_info_v5"

#define PROCFS_MAX_SIZE 256 
#define DATA_MAX_SIZE 4096

static struct proc_dir_entry *our_proc_file; 
 
static char procfs_buffer[PROCFS_MAX_SIZE]; 
 
static unsigned long procfs_buffer_size = 0; 

static char proc_data[DATA_MAX_SIZE];

static int target_vendor = -1;
static int target_device = -1;
struct pci_dev *dev = NULL;

static ssize_t procfile_read(struct file *filePointer, char __user *buffer, 
                             size_t buffer_length, loff_t *offset) 
{
    dev = NULL;
    int vendor_id = target_vendor, device_id = target_device;
    int data_size = 0;
    
    if (*offset > 0) return 0;

    if (vendor_id == -1) vendor_id = PCI_ANY_ID;
    if (device_id == -1) device_id = PCI_ANY_ID;

    dev = pci_get_device(vendor_id, device_id, NULL);
    if (dev == NULL) 
    {
        data_size += snprintf(proc_data + data_size, DATA_MAX_SIZE - data_size, "Device not found\n"); 
    }
    
    while (dev != NULL) 
    {
        int bytes_written = 0;
        bytes_written = snprintf(proc_data + data_size, DATA_MAX_SIZE - data_size,
                        "Device [%04X:%02X:%02X.%d]\nClass: %04X\nVendor: %04X\nDevice ID: %04X\n\n",
                        pci_domain_nr(dev->bus), dev->bus->number, PCI_SLOT(dev->devfn),
                        PCI_FUNC(dev->devfn), dev->class, dev->vendor, dev->device);
        pci_dev_put(dev);

        if (bytes_written < 0 || bytes_written >= DATA_MAX_SIZE - data_size)
        {
            pr_info("Buffer is full or snprintf error occured. Stopping data collection.\n");
            break;
        }
        
        data_size += bytes_written;
        dev = pci_get_device(vendor_id, device_id, dev);
    }

    if (buffer_length < data_size)
        return -EFAULT;

    if (copy_to_user(buffer, proc_data, data_size))
        return -EFAULT;

    *offset += data_size;

    target_vendor = -1;
    target_device = -1;
    dev = NULL;

    return data_size;
}


static ssize_t procfile_write(struct file *file, const char __user *buff, 
                              size_t len, loff_t *off) 
{ 
    procfs_buffer_size = len; 
    if (procfs_buffer_size > PROCFS_MAX_SIZE) procfs_buffer_size = PROCFS_MAX_SIZE;
 
    if (copy_from_user(procfs_buffer, buff, procfs_buffer_size)) return -EFAULT; 
 
    procfs_buffer[procfs_buffer_size & (PROCFS_MAX_SIZE - 1)] = '\0'; 
    
    if (sscanf(procfs_buffer, "%x:%x", &target_vendor, &target_device) == 2) 
    {
        pr_info("Parsed values: vendor=%x, device=%x\n", target_vendor, target_device);
    } else 
    {
        pr_info("Parsing failed\n");
    }
    
    pr_info("procfile write %s\n", procfs_buffer); 
 
    return procfs_buffer_size; 
}


#ifdef HAVE_PROC_OPS 
static const struct proc_ops proc_file_fops = { 
    .proc_read = procfile_read, 
    .proc_write = procfile_write, 
}; 
#else 
static const struct file_operations proc_file_fops = { 
    .read = procfile_read, 
    .write = procfile_write, 
}; 
#endif 

static int __init pci_printer_init(void)
{ 
    our_proc_file = proc_create(procfs_name, 0644, NULL, &proc_file_fops); 
    if (NULL == our_proc_file) 
    { 
        proc_remove(our_proc_file); 
        pr_alert("Error:Could not initialize /proc/%s\n", procfs_name); 
        return -ENOMEM; 
    } 
 
    pr_info("/proc/%s created\n", procfs_name); 
    return 0; 
} 

static void __exit pci_printer_exit(void)
{ 
    proc_remove(our_proc_file); 
    pr_info("/proc/%s removed\n", procfs_name); 
}

module_init(pci_printer_init);
module_exit(pci_printer_exit);

MODULE_DESCRIPTION("A simple PCI devices printer using procfs");
MODULE_AUTHOR("sasaovch");
MODULE_LICENSE("GPL v2");
