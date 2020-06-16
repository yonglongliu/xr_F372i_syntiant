/*
 * Copyright (c) 2017 - 2020 Syntiant Corp.
 *
 * This program is free software for testing; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See http://www.gnu.org/licenses/gpl-2.0.html for more details.
 */

#if 0
#define DEBUG
#endif
#define  USE_VMALLOC
#include <linux/init.h>
#include <linux/module.h>
#include <linux/device.h>
#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/version.h>
#include <linux/slab.h>
#include <linux/string.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/clk.h>
#include <linux/spi/spi.h>
#include <linux/cdev.h>
#include <linux/errno.h>
#include <linux/gpio.h>
#include <linux/wait.h>
#include <linux/kthread.h>
#include <linux/time.h>
#include <linux/proc_fs.h>
#include <linux/of_irq.h>
#include <linux/of_gpio.h>
#include <linux/input.h>
#include <linux/gpio_keys.h>

#include "linux_backports.h"

#include "ndp10x_ioctl.h"
#include "ndp10x_config.h"
#include "syntiant_ilib/syntiant_ndp.h"
#include "syntiant_ilib/syntiant_ndp_error.h"
#include "syntiant_ilib/syntiant_ndp10x.h"
#include "syntiant_ilib/ndp10x_spi_regs.h"
#include "syntiant_ilib/syntiant_ndp_driver.h"
#include "syntiant_ilib/syntiant_ndp_ilib_version.h"
#include "syntiant_packager/syntiant_package.h"
#include "syntiant-firmware/ndp10x_ph.h"

#include "ndp10x_ioctl.h"
#include "es_conversion.h"

#define BYTES_PER_AUDIO_SAMPLE (2)
#define NDP10X_BYTES_PER_MILLISECOND (16000 * BYTES_PER_AUDIO_SAMPLE / 1000)
#define RESULT_RING_SIZE (16)
#define NDP10X_MATCH_INFO_SHIFT 8

/* circular ring metadata */
struct ndp_ring_s {
    int producer;
    int consumer;
    int size;
    int element_size;
    int quantum;
    uint8_t *buf;
};

/* watch data */
struct ndp_result_s {
    struct timespec ts;
    uint32_t summary;
    uint32_t extract_len_avail;
};

struct ndp10x_spi_dev_s {
    /* TODO: maybe collapse/untangle some of this? */
    dev_t spi_devt;
    int registered;
    struct cdev cdev;
    struct class *spi_cls;
    struct device *spi_dev;
    struct spi_device *spi;
    struct mutex lock;
    struct ndp10x_s *ndp10x;
    int spi_split;
    int spi_read_delay;
    int spi_pcm_input_rate;
};

struct ndp10x_s {
    int minor;
    int opens;
    struct device *device;
    struct ndp10x_spi_dev_s spi_dev;
    struct syntiant_ndp_device_s *ndp;
    struct mutex ndp_mutex;
    wait_queue_head_t mbwait_waitq;
    int mbwait_condition;
    int sends_max_outstanding;
    spinlock_t send_ring_lock;
    struct ndp_ring_s send_ring;
    wait_queue_head_t send_waitq;
    int send_waiting;
    int sends_outstanding;
    struct mutex send_ioctl_mutex;    /* ensures only 1 send ioctl running */
    int extract_scratch_size;
    uint8_t *extract_scratch;
    spinlock_t extract_ring_lock;
    struct ndp_ring_s extract_ring;
    int extracts_left;
    int extract_waiting;
    wait_queue_head_t extract_waitq;
    struct mutex extract_ioctl_mutex; /* ensures only 1 extract ioctl running */

    /* ensures only 1 ndp10x_config ioctl running */
    struct mutex ndp10x_config_ioctl_mutex;
    
    spinlock_t result_ring_lock;
    struct ndp_ring_s result_ring;
    wait_queue_head_t result_waitq;
#ifdef CONFIG_PM_SLEEP
    int suspended;
#endif
    uint64_t isrs;
    uint64_t polls;
    uint64_t frames;
    uint64_t results;
    uint64_t results_dropped;
    uint64_t extracts;
    uint64_t extract_bytes;
    uint64_t extract_bytes_dropped;
    uint64_t sends;
    uint64_t send_bytes;
    int audio_frame_step;     /* 0 -> no package loaded */
    int extract_buffer_size;
    int send_buffer_size;
    int result_per_frame;
    int armed;
    int armed_watch_active;
    int pcm_input;
    int extract_before_match;

    struct proc_dir_entry *procfs_dir_ent;
    struct proc_dir_entry *procfs_mic_ctl_file;
    struct proc_dir_entry *procfs_info_file;
/*ndp10x mmitest flag */
	struct input_dev *input;
};
int ndp10x_irq_pin = 0;
void *ndp10x_malloc(int size);
void ndp10x_free(void *p);

static int ndp10x_spi_probe(struct spi_device *spi);
static void ndp10x_ndp_uninit(struct ndp10x_s *ndp10x);
static int ndp10x_spi_remove(struct spi_device *spi);
static void ndp10x_uninit(struct ndp10x_s *ndp10x);

static int ndp10x_spi_transfer(struct ndp10x_s *ndp10x, int mcu, uint32_t addr,
                               void *out, void *in, unsigned int count);
static int ndp10x_open(struct inode *inode, struct file *file);
static long ndp10x_ioctl(struct file *file, unsigned int cmd,
                         unsigned long arg);
#ifdef CONFIG_COMPAT
static long ndp10x_compat_ioctl(struct file *f, unsigned int cmd,
                                unsigned long arg);
#endif
static int ndp10x_release(struct inode *inode, struct file *file);

static ssize_t ndp10x_procfs_read_mic_ctl(struct file *file,
                                          char __user *ubuf,
                                          size_t count,
                                          loff_t *ppos);
static ssize_t ndp10x_procfs_write_mic_ctl(struct file *file,
                                           const char __user *ubuf,
                                           size_t count,
                                           loff_t *ppos);

//static void ndp10x_config_ext_clk(void);
static void ndp10x_config_ext_pwr(void);

#ifdef CONFIG_PM_SLEEP
static int ndp10x_driver_suspend(struct device *spi);
static int ndp10x_driver_resume(struct device *spi);
#else
#define ndp10x_driver_suspend NULL
#define ndp10x_driver_resume NULL
#endif

#define CLASS_NAME "ndp10x"
#define DEVICE_NAME "ndp10x"
#define MAX_DEV  16

static int ndp10x_major = -1;
static struct class *ndp10x_class = NULL;
static struct ndp10x_s ndp10xs[MAX_DEV];

static struct file_operations fops = {
    .owner = THIS_MODULE,
    .open = ndp10x_open,
#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,6,35))
    .ioctl = my_ioctl
#else
    .unlocked_ioctl
#endif
    = ndp10x_ioctl,
#ifdef CONFIG_COMPAT
    .compat_ioctl = ndp10x_compat_ioctl,
#endif
    .release = ndp10x_release,
};

static int ndp10x_procfs_info_show(struct seq_file *m, void *data);
static int ndp10x_procfs_info_open(struct inode *inode, struct file *file)
{
    return single_open(file, ndp10x_procfs_info_show, inode->i_private);
}

static const struct file_operations ndp10x_procfs_info_fops = {
    .owner = THIS_MODULE,
    .open = ndp10x_procfs_info_open,
    .read = seq_read,
    .llseek = seq_lseek,
    .release = single_release,
};

static struct file_operations procfs_mic_ctl_fops = {
    .owner = THIS_MODULE,
    .read = ndp10x_procfs_read_mic_ctl,
    .write = ndp10x_procfs_write_mic_ctl,
};

static const struct of_device_id spi_match[] = {
    { .compatible = SPI_DEVICE_NAME, },
    {}
};
MODULE_DEVICE_TABLE(of, spi_match);

#ifdef CONFIG_PM_SLEEP
static SIMPLE_DEV_PM_OPS(ndp10x_spi_driver_pm_ops, ndp10x_driver_suspend,
                         ndp10x_driver_resume);
#endif

static struct spi_driver ndp10x_spi_driver = {
    .driver = {
        .name =	SPI_DEVICE_NAME,
        .owner = THIS_MODULE,
        .of_match_table	= spi_match,
#ifdef CONFIG_PM_SLEEP
        .pm = &ndp10x_spi_driver_pm_ops,
#endif
    },
    .probe = ndp10x_spi_probe,
    .remove = ndp10x_spi_remove,
};

static const struct file_operations ndp10x_spi_fops = {
    .owner =	THIS_MODULE,
};

#ifdef CONFIG_PM_SLEEP
/* PM level functions */
static int ndp10x_driver_suspend(struct device *spi)
{
    struct ndp10x_spi_dev_s *spi_dev =
        spi_get_drvdata((struct spi_device *)spi);
    pr_debug("%s irq=%d\n", __func__, spi_dev->spi->irq);
    WRITE_ONCE(spi_dev->ndp10x->suspended, 1);
    synchronize_irq(spi_dev->spi->irq);
    enable_irq_wake(spi_dev->spi->irq);
    return 0;
}

static int ndp10x_driver_resume(struct device *spi)
{
    struct ndp10x_spi_dev_s *spi_dev =
        spi_get_drvdata((struct spi_device *)spi);
    pr_debug("%s irq=%d\n", __func__, spi_dev->spi->irq);
    disable_irq_wake(spi_dev->spi->irq);
    WRITE_ONCE(spi_dev->ndp10x->suspended, 0);
    /* there may have been an IRQ during our suspend */
    irq_wake_thread(spi_dev->spi->irq, spi_dev);
    return 0;
}
#endif


/*
 * these rings:
 * - are suitable for objects of any size (`element_size`)
 * - implement a tail-drop behavior when doing ring_add()
 * - ensure there is always at least 1 `quantum` of space free after
 *   a ring_add()
 *
 * to avoid tail-dropping, simply do not ring_add() more than is reported
 * available by ring_space()
 */

static void
ring_reset(struct ndp_ring_s *ring)
{
    ring->producer = 0;
    ring->consumer = 0;
}

static void
ring_free(struct ndp_ring_s *ring)
{
    if (ring->buf) {
        ndp10x_free(ring->buf);
        ring->buf = NULL;
    }
    ring->size = 0;
}

static int
ring_allocate(struct ndp_ring_s *ring, int size, int element_size, int quantum)
{
    int new_bytes = size * element_size;
    int current_bytes = ring->size * ring->element_size;
    int s = 0;

    ring_reset(ring);
    if (current_bytes != new_bytes || !ring->buf) {
        ring_free(ring);
        if (new_bytes) {
            ring->buf = ndp10x_malloc(new_bytes);
            if (!ring->buf) {
                size = 0;
                s = -ENOMEM;
            }
        }
    }
    ring->size = size;
    ring->element_size = element_size;
    ring->quantum = quantum;

    return s;
}

void *
ring_producer(struct ndp_ring_s *ring)
{
    int es = ring->element_size;
    int p = READ_ONCE(ring->producer);
    uint8_t *b = ring->buf;

    return (void *) (b + p * es);
}

void *
ring_consumer(struct ndp_ring_s *ring)
{
    int es = ring->element_size;
    int c = READ_ONCE(ring->consumer);
    uint8_t *b = ring->buf;

    return (void *) (b + c * es);
}

int
ring_empty(struct ndp_ring_s *ring)
{
    int p = READ_ONCE(ring->producer);
    int c = READ_ONCE(ring->consumer);
    return p == c;
}

/* this is for debugging only -- free space including reserved quantum */
int
ring_unused(struct ndp_ring_s *ring)
{
    int p = READ_ONCE(ring->producer);
    int c = READ_ONCE(ring->consumer);
    int s = ring->size;
    int space = p == c ? s : (c + s - p) % s;
    return space;
}

int
ring_space(struct ndp_ring_s *ring)
{
    int p = READ_ONCE(ring->producer);
    int c = READ_ONCE(ring->consumer);
    int s = ring->size;
    int q = ring->quantum;
    int space = (c + s - p - q) % s;
    return space;
}

int
ring_space_to_end(struct ndp_ring_s *ring)
{
    int p = READ_ONCE(ring->producer);
    int s = ring->size;
    int space = ring_space(ring);
    int to_end = s - p;
    int space_to_end = min(to_end, space);
    return space_to_end;
}

int
ring_cnt(struct ndp_ring_s *ring)
{
    int p = READ_ONCE(ring->producer);
    int c = READ_ONCE(ring->consumer);
    int s = ring->size;
    int cnt = s ? (p + s - c) % s : 0;
    return cnt;
}

int
ring_cnt_to_end(struct ndp_ring_s *ring)
{
    int c = READ_ONCE(ring->consumer);
    int s = ring->size;
    int cnt = ring_cnt(ring);
    int to_end = s - c;
    int cnt_to_end = min(cnt, to_end);
    return cnt_to_end;
}

void
ring_remove(struct ndp_ring_s *ring, int n)
{
    int c = READ_ONCE(ring->consumer);
    int s = ring->size;
    WRITE_ONCE(ring->consumer, (c + n) % s);
}

int
ring_add(struct ndp_ring_s *ring, int n)
{
    int rs = ring_space(ring);
    int tail_drop = rs < n;
    int p = READ_ONCE(ring->producer);
    int s = ring->size;
    int dropped = 0;
    int c0, c, q;

    p = (p + n) % s;
    WRITE_ONCE(ring->producer, p);
    if (tail_drop) {
        q = ring->quantum;
        c0 = READ_ONCE(ring->consumer);
        c = p + s + q;
        dropped = (c - c0) % s;
        if (dropped) {
            WRITE_ONCE(ring->consumer, c % s);
        }
    }
    return dropped;
}

int
ndp10x_translate_error(int e)
{
    int s = 0;

    switch (e) {
    case SYNTIANT_NDP_ERROR_NONE:
        break;
    case SYNTIANT_NDP_ERROR_FAIL:
        s = -EIO;
        break;
    case SYNTIANT_NDP_ERROR_ARG:
    case SYNTIANT_NDP_ERROR_UNINIT:
        s = -EINVAL;
        break;
    case SYNTIANT_NDP_ERROR_UNSUP:
        s = -ENOSYS;
        break;
    case SYNTIANT_NDP_ERROR_NOMEM:
        s = -ENOMEM;
        break;
    case SYNTIANT_NDP_ERROR_BUSY:
        s = -EBUSY;
        break;
    case SYNTIANT_NDP_ERROR_TIMEOUT:
        s = -ETIME;
        break;
    case SYNTIANT_NDP_ERROR_MORE:
        break;
    }
    return s;
}


void *
ndp10x_malloc(int size)
{
    void *p;
#ifdef USE_VMALLOC
    p = vmalloc(size);
#else
    p = kmalloc(size, GFP_KERNEL);
#endif
    return p;
}

void
ndp10x_free(void *p)
{
#ifdef USE_VMALLOC
    vfree(p);
#else
    kfree(p);
#endif
}

int
ndp10x_mbwait(void *d)
{
    struct ndp10x_s *ndp10x = d;
    long s0;
    int s = SYNTIANT_NDP_ERROR_NONE;

    while (!(READ_ONCE(ndp10x->mbwait_condition))) {
        mutex_unlock(&ndp10x->ndp_mutex);
        s0 = wait_event_interruptible_timeout
            (ndp10x->mbwait_waitq, READ_ONCE(ndp10x->mbwait_condition), HZ);
        if (!s0) {
            pr_info("%s: timed out\n", __func__);
            s = SYNTIANT_NDP_ERROR_TIMEOUT;
            goto out;
        } else if (s0 < 0) {
            pr_err("%s: error: %d\n", __func__, -s);
            s = SYNTIANT_NDP_ERROR_FAIL;
            goto out;
        }

        s0 = mutex_lock_interruptible(&ndp10x->ndp_mutex);
        if (s0) {
            s = SYNTIANT_NDP_ERROR_FAIL;
            goto out;
        }
    }

 out:
    WRITE_ONCE(ndp10x->mbwait_condition, 0);
    return s;
}

int
ndp10x_get_type(void *d, unsigned int *type)
{

    struct ndp10x_s *ndp10x = d;
    int s = SYNTIANT_NDP_ERROR_NONE;
    uint8_t in;

    pr_debug("%s: reading device id\n", __func__);

    s = ndp10x_spi_transfer(ndp10x, 0, NDP10X_SPI_ID0, NULL, &in, 1);
    if (s) return s;

    pr_debug("%s: device id read: 0x%02x\n", __func__, in);
    if (!in) {
        in = 0x18;
        pr_crit("%s: device id overriden to %d\n", __func__, in);
    }

    *type = in;

    return SYNTIANT_NDP_ERROR_NONE;
}

int
ndp10x_sync(void *d)
{
    struct ndp10x_s *ndp10x;
    int s;

    ndp10x = d;

    s = mutex_lock_interruptible(&ndp10x->ndp_mutex);

    return s ? SYNTIANT_NDP_ERROR_FAIL : SYNTIANT_NDP_ERROR_NONE;
}

int
ndp10x_unsync(void *d)
{
    struct ndp10x_s *ndp10x;

    ndp10x = d;

    mutex_unlock(&ndp10x->ndp_mutex);

    return SYNTIANT_NDP_ERROR_NONE;
}

int
ndp10x_transfer(void *d, int mcu, uint32_t addr, void *out, void *in,
                unsigned int count)
{
    struct ndp10x_s *ndp10x = d;
    int s = SYNTIANT_NDP_ERROR_NONE;

#if 0
    pr_debug("%s: mcu %d, addr 0x%08x, out %p, in %p, count %dB\n", __func__,
             mcu, addr, out, in, count);
#endif

    if(out && in) {
        pr_err("%s: error in transfer parameters\n", __func__);
    }

    s = ndp10x_spi_transfer(ndp10x, mcu, addr, out, in, count);
    if (s < 0) {
        pr_crit("%s: unable to transfer to the SPI device\n", __func__);
        return SYNTIANT_NDP_ERROR_FAIL;
    }

    return SYNTIANT_NDP_ERROR_NONE;
}

static int
ndp10x_enable(struct ndp10x_s *ndp10x)
{
    uint32_t causes;
    int s;
    
    causes = SYNTIANT_NDP_INTERRUPT_DEFAULT;
    s = syntiant_ndp_interrupts(ndp10x->ndp, &causes);
    if (s) {
        pr_alert("%s: unable to enable interrupt: %s\n", __func__,
                 syntiant_ndp_error_name(s));
    }
    return ndp10x_translate_error(s);
}

static int
ndp10x_quiesce(struct ndp10x_s *ndp10x)
{
    uint32_t interrupts;
    int s;

    interrupts = 0;
    s = syntiant_ndp_interrupts(ndp10x->ndp, &interrupts);
    if (s) {
        pr_err("%s: unable to disable interrupts: %s\n", __func__,
               syntiant_ndp_error_name(s));
    }

    synchronize_irq(ndp10x->spi_dev.spi->irq);
    
    s = ndp10x_translate_error(s);
    return s;
}

static ssize_t ndp10x_procfs_read_mic_ctl(struct file *file,
                                          char __user *ubuf,
                                          size_t count,
                                          loff_t *ppos)
{
    char tmp[10];

    struct ndp10x_s *ndp10x = PDE_DATA(file_inode(file));
    struct syntiant_ndp10x_config_s ndp10x_config;
    int s;

    /* bail out on zero reads & successive reads */
    if ((count == 0) || (*ppos > 0)) {
        return 0;
    }

    count = 0;

    memset(&ndp10x_config, 0, sizeof(ndp10x_config));
    ndp10x_config.get_all = 1;
    s = syntiant_ndp10x_config(ndp10x->ndp, &ndp10x_config);
    if (!s) {
        sprintf(tmp, "%d\n",
                ndp10x_config.pdm_clock_ndp);
        /* +1 for trailing 0 */
        count = 1 + strlen(tmp);
        s = copy_to_user(ubuf, tmp, count);
        *ppos += count;
    }

    return count;
}


static ssize_t ndp10x_procfs_write_mic_ctl(struct file *file,
                                           const char __user *ubuf,
                                           size_t count,
                                           loff_t *ppos)
{
    struct ndp10x_s *ndp10x = PDE_DATA(file_inode(file));
    struct syntiant_ndp10x_config_s ndp10x_config;
    char tmp[10];
    int s;
    int disable;

    /* bail out on zero reads & successive reads */
    if ((count == 0) || (*ppos > 0)) {
        return 0;
    }

    s = copy_from_user(tmp, ubuf, 1);
    if (s) {
        return -EFAULT;
    }

    /* only use first byte of data, but pretend we used it all */
    memset(&ndp10x_config, 0, sizeof(ndp10x_config));

    ndp10x_config.get_all = 1;
    s = syntiant_ndp10x_config(ndp10x->ndp, &ndp10x_config);
    if (s) {
        pr_err("%s: Error getting NDP10x config %d\n", __func__, s);
        *ppos += count;
        return count;
    }

    disable = tmp[0] == '0';
    pr_info("%s: %sabling NDP mic clock & input\n",
            __func__, disable ? "Dis" : "En");

    ndp10x_config.set = SYNTIANT_NDP10X_CONFIG_SET_PDM_CLOCK_NDP
        | SYNTIANT_NDP10X_CONFIG_SET_DNN_INPUT
        | SYNTIANT_NDP10X_CONFIG_SET_TANK_INPUT;
    ndp10x_config.set1 = 0;
    ndp10x_config.get_all = 0;
    ndp10x_config.pdm_clock_ndp = !disable;
    ndp10x_config.dnn_input =
        disable ?
        SYNTIANT_NDP10X_CONFIG_DNN_INPUT_NONE:
        SYNTIANT_NDP10X_CONFIG_DNN_INPUT_PDM0;
    ndp10x_config.tank_input =
        disable ?
        SYNTIANT_NDP10X_CONFIG_TANK_INPUT_NONE:
        SYNTIANT_NDP10X_CONFIG_TANK_INPUT_DNN;
    s = syntiant_ndp10x_config(ndp10x->ndp, &ndp10x_config);
    if (s) {
        pr_err("%s: Error setting NDP10x config %d\n", __func__, s);
    }
    *ppos += count;

    return count;
}

int freq_active(int dnn_input, int tank_input)
{
    return (((dnn_input != SYNTIANT_NDP10X_CONFIG_DNN_INPUT_NONE)
        &&  (dnn_input != SYNTIANT_NDP10X_CONFIG_DNN_INPUT_I2S_DIRECT)
        &&  (dnn_input != SYNTIANT_NDP10X_CONFIG_DNN_INPUT_SPI_DIRECT))
        ||  (tank_input != SYNTIANT_NDP10X_CONFIG_TANK_INPUT_FILTER_BANK));
}

int pdm_active(int dnn_input, int tank_input, int n)
{
    return  (((!n) && (dnn_input == SYNTIANT_NDP10X_CONFIG_DNN_INPUT_PDM0))
        ||  ((n) && (dnn_input == SYNTIANT_NDP10X_CONFIG_DNN_INPUT_PDM1))
        ||  (dnn_input == SYNTIANT_NDP10X_CONFIG_DNN_INPUT_PDM_SUM)
        ||  ((!n) && (tank_input == SYNTIANT_NDP10X_CONFIG_TANK_INPUT_PDM0))
        ||  ((n) && (tank_input == SYNTIANT_NDP10X_CONFIG_TANK_INPUT_PDM1))
        ||  (tank_input == SYNTIANT_NDP10X_CONFIG_TANK_INPUT_PDM_SUM)
        ||  (tank_input == SYNTIANT_NDP10X_CONFIG_TANK_INPUT_PDM_BOTH)
        ||  ((!n) && (tank_input == SYNTIANT_NDP10X_CONFIG_TANK_INPUT_PDM0_RAW))
        ||  ((n) && (tank_input == SYNTIANT_NDP10X_CONFIG_TANK_INPUT_PDM1_RAW))
        ||  (tank_input == SYNTIANT_NDP10X_CONFIG_TANK_INPUT_PDM_BOTH_RAW));
}

int i2s_active(int dnn_input, int tank_input)
{
    return ((dnn_input == SYNTIANT_NDP10X_CONFIG_DNN_INPUT_I2S_LEFT)
        || (dnn_input == SYNTIANT_NDP10X_CONFIG_DNN_INPUT_I2S_RIGHT)
        || (dnn_input == SYNTIANT_NDP10X_CONFIG_DNN_INPUT_I2S_SUM)
        || (dnn_input == SYNTIANT_NDP10X_CONFIG_DNN_INPUT_I2S_MONO)
        || (dnn_input == SYNTIANT_NDP10X_CONFIG_DNN_INPUT_I2S_DIRECT)
        || (tank_input == SYNTIANT_NDP10X_CONFIG_TANK_INPUT_I2S_LEFT)
        || (tank_input == SYNTIANT_NDP10X_CONFIG_TANK_INPUT_I2S_RIGHT)
        || (tank_input == SYNTIANT_NDP10X_CONFIG_TANK_INPUT_I2S_MONO)
        || (tank_input == SYNTIANT_NDP10X_CONFIG_TANK_INPUT_I2S_SUM)
        || (tank_input == SYNTIANT_NDP10X_CONFIG_TANK_INPUT_I2S_BOTH));
}

#define CONFIG_STRING_LEN 64
#define MAX_LABELS 64
static int ndp10x_procfs_info_show(struct seq_file *m, void *data)
{
    struct ndp10x_s *ndp10x = PDE_DATA(file_inode(m->file));

    int dnn_input;
    int tank_input;
    int have_input;
    int have_freq;
    int have_pdm;
    int have_i2s;
    char *mp;
    int freq;
    int eq;

    struct syntiant_ndp_config_s config;
    struct syntiant_ndp10x_config_s ndp10x_config;
    int s;

    char fwver[CONFIG_STRING_LEN] = "";
    char paramver[CONFIG_STRING_LEN] =  "";
    char pkgver[CONFIG_STRING_LEN] = "";
    char label_data[CONFIG_STRING_LEN] = "";

    memset(&config, 0, sizeof(struct syntiant_ndp_config_s));
    config.firmware_version = fwver;
    config.firmware_version_len = STRING_LEN;
    config.parameters_version = paramver;
    config.parameters_version_len = STRING_LEN;
    config.pkg_version = pkgver;
    config.pkg_version_len = STRING_LEN;
    config.labels = label_data;
    config.labels_len = STRING_LEN;

    s = syntiant_ndp_get_config(ndp10x->ndp, &config);
    if (s) {
        if (s == SYNTIANT_NDP_ERROR_UNINIT) {
            seq_printf(m, "NDP Device not initialized\n");
        } else {
            
            seq_printf(m, "NDP Get config error %d\n", s);
        }
        return 0;
    }

    memset(&ndp10x_config, 0, sizeof(ndp10x_config));
    ndp10x_config.get_all = 1;
    s = syntiant_ndp10x_config(ndp10x->ndp, &ndp10x_config);
                 
    seq_printf(m, "NDP10x Driver Version: %s\nFirmware: %s\nParameters: %s\n",
               SYNTIANT_NDP_ILIB_VERSION,fwver, paramver);
    
    seq_printf(m, "NDP MIC Output: %d [%d Hz]\n",
               ndp10x_config.pdm_clock_ndp,
               ndp10x_config.pdm_clock_rate);
    
    dnn_input = ndp10x_config.dnn_input;
    tank_input = ndp10x_config.tank_input;
    
    have_input = (dnn_input != SYNTIANT_NDP10X_CONFIG_DNN_INPUT_NONE);
    
    have_freq = freq_active(dnn_input, tank_input);
    
    have_pdm = pdm_active(dnn_input, tank_input, 0) 
        || pdm_active(dnn_input, tank_input, 1);
    
    have_i2s = i2s_active(dnn_input, tank_input); 
        
    seq_printf(m, "input clock rate: %d\n", 
               ndp10x_config.input_clock_rate);

    seq_printf(m, "core clock rate: %d\n", 
               ndp10x_config.core_clock_rate);
    seq_printf(m, "mcu clock rate: %d\n", 
               ndp10x_config.mcu_clock_rate);
    seq_printf(m, "holding tank input: %s\n", 
               syntiant_ndp10x_config_tank_input_s(ndp10x_config.tank_input));
    if (have_freq) {
        seq_printf(m, "holding tank size: %d\n", 
                   ndp10x_config.tank_size);
        seq_printf(m, "holding tank maximum size: %d\n", 
                   ndp10x_config.tank_max_size);
        seq_printf(m, "holding tank sample width: %d bits\n", 
                   ndp10x_config.tank_bits);
    }

    if (dnn_input == SYNTIANT_NDP10X_CONFIG_DNN_INPUT_SPI) {
        seq_printf(m, "spi max PCM input rate: %d\n", 
                   ndp10x_config.spi_max_pcm_input_rate);
        seq_printf(m, "spi word bits: %d\n", 
                   ndp10x_config.spi_word_bits);
    }

    if(have_pdm) {
        seq_printf(m, "pdm clock frequency: %d\n", 
                   ndp10x_config.pdm_clock_rate);
        seq_printf(m, "pdm clock source: %s\n",
                   (ndp10x_config.pdm_clock_ndp) ?
                   "ndp": "external");
        for(s=0; s<2; s++) {
            if(pdm_active(dnn_input, tank_input, s)) {
                seq_printf(m, "pdm%d in shift: %d\n",
                           s, ndp10x_config.pdm_in_shift[s]);
                seq_printf(m, "pdm%d out shift: %d\n",
                           s, ndp10x_config.pdm_out_shift[s]);
                seq_printf(m, "pdm%d DC offset: %d\n",
                           s, ndp10x_config.pdm_dc_offset[s]);
            }
        }
    }

    if(have_i2s) {
        seq_printf(m, "i2s frame size: %d\n",
                   ndp10x_config.i2s_frame_size);
        seq_printf(m, "i2s sample size: %d\n",
                   ndp10x_config.i2s_sample_size);
        seq_printf(m, "i2s sample msbit: %d\n",
                   ndp10x_config.i2s_sample_msbit);
    }

    if(have_freq) {
        seq_printf(m, "frequency domain clock rate: %d\n", 
                   ndp10x_config.freq_clock_rate);
        seq_printf(m, "audio frame size: %d\n", 
                   ndp10x_config.audio_frame_size);
        seq_printf(m, "audio frame step: %d\n", 
                   ndp10x_config.audio_frame_step);
        seq_printf(m, "audio buffer used: %d\n", 
                   ndp10x_config.audio_buffer_used);
        seq_printf(m, "audio buffer low water mark notification: %s\n", 
                   (ndp10x_config.water_mark_on) ?
                   "on" : "off");
        seq_printf(m, "filter bank feature extractor bin count: %d\n", 
                   ndp10x_config.freq_frame_size);
        seq_printf(m, "preemphasis filter decay exponent: %d\n", 
                   ndp10x_config.preemphasis_exponent);
        seq_printf(m, "dsp power offset: %d\n", ndp10x_config.power_offset);
        seq_printf(m, "dsp power scale exponent: %d\n", 
                   ndp10x_config.power_scale_exponent);
    }
        
    seq_printf(m, "dnn input: %s\n", 
               syntiant_ndp10x_config_dnn_input_s(ndp10x_config.dnn_input));
    seq_printf(m, "dnn clock rate: %d\n", ndp10x_config.dnn_clock_rate);
        
    if(have_input) {
        seq_printf(m, "dnn frame size: %d\n", 
                   ndp10x_config.dnn_frame_size);
    }

    if((dnn_input == SYNTIANT_NDP10X_CONFIG_DNN_INPUT_SPI_DIRECT)
       || (dnn_input == SYNTIANT_NDP10X_CONFIG_DNN_INPUT_I2S_DIRECT)) {
        seq_printf(m, "dnn signed input: %s\n", 
                   (ndp10x_config.dnn_signed) ?
                   "on" : "off");
    }

    if (have_input) {
        seq_printf(m, "dnn minimum input threshold: %d\n", 
                   ndp10x_config.dnn_minimum_threshold);
        seq_printf(m, "dnn run input threshold: %d\n", 
                   ndp10x_config.dnn_run_threshold);
        seq_printf(m, "dnn inputs: %d\n", ndp10x_config.dnn_inputs);
        seq_printf(m, "dnn static inputs: %d\n", 
                   ndp10x_config.dnn_static_inputs);
        seq_printf(m, "dnn outputs: %d\n", ndp10x_config.dnn_outputs);
    }
        
    if(have_pdm && ndp10x_config.agc_on) {
        seq_printf(m, "agc: %s\n", (ndp10x_config.agc_on)?"on":"off");
        seq_printf(m, "agc_max_adj: %d, %d\n", 
                   ndp10x_config.agc_max_adj[0],
                   ndp10x_config.agc_max_adj[1]);
        seq_printf(m, "agc_nom_speech_quiet: %d\n", 
                   ndp10x_config.agc_nom_speech_quiet);
    }
        
    if(have_input && ndp10x_config.fw_pointers_addr) {
        seq_printf(m, "match per frame: %s\n", 
                   (ndp10x_config.match_per_frame_on) ?
                   "on" : "off");
    }

    if(ndp10x_config.fw_pointers_addr) {
        seq_printf(m, "fw pointers address: %08x\n", 
                   ndp10x_config.fw_pointers_addr);
    }
        
    mp = ndp10x_config.memory_power;
    for(s = 0; s <= SYNTIANT_NDP10X_CONFIG_MEMORY_MAX; s++) {
        if(mp[s] != SYNTIANT_NDP10X_CONFIG_MEMORY_POWER_ON) {
            seq_printf(m, "memory_power of %s : %s\n", 
                       syntiant_ndp10x_memory_s(s), 
                       syntiant_ndp10x_memory_power_s(mp[s]));
        }
    }

    if (have_freq) {
        for(s = 0; s < SYNTIANT_NDP10X_MAX_FREQUENCY_BINS; s++) {
            freq = (int)(ndp10x_config.filter_frequency[s]
                         * SYNTIANT_NDP10X_AUDIO_FREQUENCY/512/2);
            eq = ndp10x_config.filter_eq[s] / 2;
            seq_printf(m, "filter bin %d: %d hz,"
                       " eq %d dB\n",
                       s, freq, eq);
        }
    }
    return 0;    
}

int
ndp10x_spi_transfer(struct ndp10x_s *ndp10x, int mcu, uint32_t addr,
                    void *out, void *in, unsigned int count)
{
    int s = SYNTIANT_NDP_ERROR_NONE;
    struct ndp10x_spi_dev_s *d = &ndp10x->spi_dev;
    struct spi_device *spi = ndp10x->spi_dev.spi;
    struct spi_transfer tr[3];
    uint8_t spi_cmd[4];
    uint8_t spi_cmd_addr[5];
    int s0;
    int i;
    struct spi_message m;
    int MAX_SPI_XFER_LEN = 32; /* assume max transfers of 32 bytes on given platform */
    unsigned int cur_len = 0;
    struct spi_transfer *tr_data = NULL;
    int j;
    unsigned int count_left = count;

    if (!spi) {
        pr_crit("%s: SPI is not configured properly\n", __func__);
        return -ENODEV;
    }
    if (count > MAX_BUFFER_SIZE) {
        pr_err("%s: transfer size is greater than MAX_BUFFER_SIZE\n", __func__);
        return -ENOMEM;
    }

    if (count > MAX_SPI_XFER_LEN) {
        tr_data = vmalloc(count / MAX_SPI_XFER_LEN *
                        sizeof(struct spi_transfer));
        if (!tr_data) {
            pr_err("%s: Error allocating memory for SPI transfer\n", __func__);
            goto error;
        }
        memset(tr_data, 0, count / MAX_SPI_XFER_LEN * sizeof(struct spi_transfer));
    }

    i = 0;
    spi_message_init(&m);
    if (mcu) {
        if ((count & 0x3) != 0) {
            pr_err("%s: SYNTIANT_NDP_ERROR_ARG\n", __func__);
        }
        memset(tr, 0, (2 + (in ? 1 : 0)) * sizeof(struct spi_transfer));

        /* Loads cmd + addr byte in 1 transaction */
        spi_cmd_addr[0] = NDP10X_SPI_MADDR(0);
        memcpy(&spi_cmd_addr[1], &addr, sizeof(addr));
        tr[i].tx_buf = (char*)&spi_cmd_addr[0];
        tr[i].bits_per_word = 8;
        tr[i].len = 5;
        /* for reads, need to restart a read command packet */
        if (in) {
            if (d->spi_split) {
                /* some OS/chip combinations don't cs_change signal so flush */
                spi_message_add_tail(&tr[i], &m);
                mutex_lock(&d->lock);
                s0 = spi_sync(spi, &m);
                mutex_unlock(&d->lock);
                if (s0 < 0) {
                    pr_err("%s: unable to transfer first split on SPI device\n",
                        __func__);
                    s = SYNTIANT_NDP_ERROR_FAIL;
                    goto error;
                }
            } else {
                tr[i].cs_change = 1;
                spi_message_add_tail(&tr[i], &m);
                i += 1;
            }
            /*
            * this adds time between the command+address write
            * and the read of the first MCU data, during which
            * time the hardware is fetching data in the chip.
            * spi_read_delay > 0 enables SPI clock rates > 1 mbps
            * we read from bytes 1-3 of MADDR which are located before
            * MDATA.
            */
            memset(spi_cmd, 0, sizeof(spi_cmd));
            spi_cmd[0] = 0x80 | (NDP10X_SPI_MDATA(0) - d->spi_read_delay);
            tr[i].tx_buf = (char*) &spi_cmd[0];
            tr[i].len = 1 + d->spi_read_delay;
        }
    } else {
        if (0xff < addr) {
            pr_err("%s: SYNTIANT_NDP_ERROR_ARG\n", __func__);
            s = SYNTIANT_NDP_ERROR_ARG;
            goto error;
        }
        memset(tr, 0, 2 * sizeof(*tr));
        spi_cmd[0] = (in ? 0x80 : 0) | addr;
        tr[i].tx_buf = (char*) &spi_cmd[0];
        tr[i].bits_per_word = 8;
        tr[i].len = 1;
    }
    spi_message_add_tail(&tr[i], &m);
    i += 1;
    tr[i].tx_buf = out;
    tr[i].rx_buf = in;
    tr[i].bits_per_word = 8;

    if (count > MAX_SPI_XFER_LEN) {
        tr[i].len = MAX_SPI_XFER_LEN;
    } else {
        tr[i].len = count;
    }
    cur_len += tr[i].len;
    count_left -= tr[i].len;
    /* if (mcu) {
        pr_info("%s: transfer addr=%x len=%d in=%p out=%p cur_len=%d left=%d\n",
                __func__, addr, tr_data[j].len,
                in, out, cur_len, count_left);
    } */
    if (!mcu && !in && addr == NDP10X_SPI_SAMPLE) {
        tr[i].speed_hz = d->spi_pcm_input_rate;
    }
    spi_message_add_tail(&tr[i], &m);

    j = 0;
    while (count_left) {
        if (out) {
            tr_data[j].tx_buf = (uint8_t*) out + cur_len;
        } else {
            tr_data[j].rx_buf = (uint8_t*) in + cur_len;
        }
        tr_data[j].bits_per_word = 8;
        if (count_left > MAX_SPI_XFER_LEN) {
            tr_data[j].len = MAX_SPI_XFER_LEN;
        } else {
            tr_data[j].len = count_left;
        }
        if (!mcu && !in && addr == NDP10X_SPI_SAMPLE) {
            tr_data[j].speed_hz = d->spi_pcm_input_rate;
        }
        cur_len += tr_data[j].len;
        count_left -= tr_data[j].len;
        /* if (mcu) {
            pr_info("%s: transfer addr=%x len=%d in=%p out=%p cur_len=%d left=%d\n",
                    __func__, addr, tr_data[j].len,
                    in, out, cur_len, count_left);
        } */
        spi_message_add_tail(&tr_data[j], &m);
        j++;
    }

    mutex_lock(&d->lock);
    s0 = spi_sync(spi, &m);
    mutex_unlock(&d->lock);
    if (s0 < 0) {
        pr_err("%s: unable to transfer on SPI device\n", __func__);
        s = SYNTIANT_NDP_ERROR_FAIL;
        goto error;
    }

 error:
    if (tr_data) {
        vfree(tr_data);
    }
    return s;
}

static void
ndp10x_poll_one(struct ndp10x_s *ndp10x)
{
    int s, frame;
    uint32_t notifications, summary;
    struct ndp_result_s *result;
    struct ndp_ring_s *result_ring;
    int len;
    struct syntiant_ndp10x_config_s ndp10x_config = {0};

    ndp10x->polls++;

    s = syntiant_ndp_poll(ndp10x->ndp, &notifications, 1);
    if (s) {
        pr_err("%s: poll failed: %s\n", __func__, syntiant_ndp_error_name(s));
    }
    
    pr_debug("%s: poll, notifications 0x%x\n", __func__, notifications);

    frame = notifications & SYNTIANT_NDP_NOTIFICATION_DNN;
    if (frame) {
        ndp10x->frames++;

        summary = 0;
        if (READ_ONCE(ndp10x->audio_frame_step)) {
            s = syntiant_ndp_get_match_summary(ndp10x->ndp, &summary);
            if (s) {
                pr_err("%s: get_match_summary failed: %s\n", __func__,
                       syntiant_ndp_error_name(s));
                return;
            }
        }

        if (ndp10x->result_per_frame
            || (NDP10X_SPI_MATCH_MATCH_MASK & summary)) {

            if (READ_ONCE(ndp10x->armed)) {
                spin_lock(&ndp10x->extract_ring_lock);
                ring_reset(&ndp10x->extract_ring);
                spin_unlock(&ndp10x->extract_ring_lock);

                len = ndp10x->extract_before_match;
                s = syntiant_ndp_extract_data(ndp10x->ndp,
                                              SYNTIANT_NDP_EXTRACT_TYPE_INPUT,
                                              SYNTIANT_NDP_EXTRACT_FROM_MATCH,
                                              NULL,
                                              &len);
                if (s) {
                    pr_err("%s: extract len=%d from _MATCH failed: %s\n",
                           __func__, ndp10x->extract_before_match,
                           syntiant_ndp_error_name(s));
                    return;
                }

                ndp10x->extracts_left = len / ndp10x->audio_frame_step;

                ndp10x_config.set =
                    SYNTIANT_NDP10X_CONFIG_SET_MATCH_PER_FRAME_ON;
                ndp10x_config.match_per_frame_on = 1;
                s = syntiant_ndp10x_config(ndp10x->ndp, &ndp10x_config);
                if (s) {
                    pr_err("%s: failed to set match-per-frame: %s\n", __func__,
                           syntiant_ndp_error_name(s));
                }
                WRITE_ONCE(ndp10x->armed, 0);
            }
            ndp10x->results++;
            result_ring =  &ndp10x->result_ring;
            result = ring_producer(result_ring);
            /* always 1 quantum free due to the design of the ring logic */
            result->summary = summary;
            getnstimeofday(&result->ts);
            result->extract_len_avail = len;
            spin_lock(&ndp10x->result_ring_lock);
            ndp10x->results_dropped += ring_add(result_ring, 1);
            spin_unlock(&ndp10x->result_ring_lock);
            wake_up_interruptible(&ndp10x->result_waitq);
        }
    }

    if (notifications & SYNTIANT_NDP_NOTIFICATION_MAILBOX_IN) {
        pr_debug("%s: mailbox_in notification \n", __func__);
        WRITE_ONCE(ndp10x->mbwait_condition, 1);
        wake_up_interruptible(&ndp10x->mbwait_waitq);
    }

    /* none of these interrupts is actionable with current firmware */
    if (notifications & SYNTIANT_NDP_NOTIFICATION_MAILBOX_OUT) {
        pr_info("%s: mailbox_out notification \n", __func__);
    }
    if (notifications & SYNTIANT_NDP_NOTIFICATION_ERROR) {
        pr_info("%s: error notification \n", __func__);
    }
    if (notifications & SYNTIANT_NDP_NOTIFICATION_DNN) {
        /* pr_info("%s:  match_dnn notification\n", __func__); */
    }

}

static int
ndp10x_can_extract(struct ndp10x_s *ndp10x)
{
    int s;
    uint32_t size;
    int frame_step = READ_ONCE(ndp10x->audio_frame_step);

    if (frame_step) {
        if (!ndp10x->extracts_left) {
            size = 0;
            s = syntiant_ndp_extract_data(ndp10x->ndp,
                                          SYNTIANT_NDP_EXTRACT_TYPE_INPUT,
                                          SYNTIANT_NDP_EXTRACT_FROM_UNREAD,
                                          NULL, &size);
            if (s) {
                pr_err("%s: extract_data failed: %s\n", __func__,
                       syntiant_ndp_error_name(s));
            } else {
                ndp10x->extracts_left = size / frame_step;
            }
        }
    } else {
        ndp10x->extracts_left = 0;
    }

    return 0 < ndp10x->extracts_left;
}

static void
ndp10x_extract_one(struct ndp10x_s *ndp10x)
{
    struct ndp_ring_s *extract_ring = &ndp10x->extract_ring;
    int frame_step = READ_ONCE(ndp10x->audio_frame_step);
    uint8_t *buf = ring_producer(extract_ring);
    int armed = READ_ONCE(ndp10x->armed);
    int extract_used;
    uint32_t size, left;
    int s;

    BUG_ON(!frame_step);
    BUG_ON(extract_ring->size % frame_step);
    BUG_ON(ring_unused(extract_ring) < frame_step);

    size = frame_step;
    s = syntiant_ndp_extract_data(ndp10x->ndp,
                                  SYNTIANT_NDP_EXTRACT_TYPE_INPUT,
                                  SYNTIANT_NDP_EXTRACT_FROM_UNREAD,
                                  armed ? NULL : buf, &size);
    if (s) {
        pr_err("%s: extract failed: %s\n", __func__,
               syntiant_ndp_error_name(s));
        return;
    }
    if (size < frame_step) {
        pr_err("%s: unexpectedly short extract size=%d, left=%d\n", __func__,
               size, ndp10x->extracts_left);
        return;
    }

    left = size - frame_step;
    ndp10x->extracts_left = left / frame_step;
    ndp10x->extracts++;
    ndp10x->extract_bytes += frame_step;
    if (ndp10x->sends_outstanding) {
        ndp10x->sends_outstanding--;
    }

    if (!armed) {
        spin_lock(&ndp10x->extract_ring_lock);
        ndp10x->extract_bytes_dropped += ring_add(extract_ring, frame_step);
        extract_used = ring_cnt(extract_ring);
        if (ndp10x->extract_waiting <= extract_used
            || extract_ring->size / 2 <= extract_used) {
            wake_up_interruptible(&ndp10x->extract_waitq);
        }
        spin_unlock(&ndp10x->extract_ring_lock);
    }
}

static int
ndp10x_can_send(struct ndp10x_s *ndp10x)
{
    struct ndp_ring_s *send_ring = &ndp10x->send_ring;
    int frame_step = READ_ONCE(ndp10x->audio_frame_step);
    int used;
    int send = 0;

    if (frame_step) {
        used = ring_cnt(send_ring);
        send = ndp10x->sends_outstanding < ndp10x->sends_max_outstanding
            && frame_step <= used;
    }

    return send;
}

static void
ndp10x_send_one(struct ndp10x_s *ndp10x)
{
    struct ndp_ring_s *send_ring = &ndp10x->send_ring;
    int frame_step = READ_ONCE(ndp10x->audio_frame_step);
    uint8_t *buf = ring_consumer(send_ring);
    int send_free;
    int s;

    BUG_ON(!frame_step);

    s = syntiant_ndp_send_data(ndp10x->ndp, buf, frame_step,
                               SYNTIANT_NDP_SEND_DATA_TYPE_STREAMING, 0);
    if (s) {
        pr_info("%s:%d error sending %d bytes of data\n",__func__,
                __LINE__, frame_step);
        return;
    }

    ndp10x->sends++;
    ndp10x->send_bytes += frame_step;
    
    pr_debug("%s: sent %d, send_waiting: %d, send_free: %d\n", __func__,
             frame_step, ndp10x->send_waiting, ring_space(send_ring));

    spin_lock(&ndp10x->send_ring_lock);
    ring_remove(send_ring, frame_step);
    send_free = ring_space(send_ring);
    ndp10x->sends_outstanding++;
    if (ndp10x->send_waiting <= send_free
        || send_ring->size / 2 <= send_free) {
        wake_up_interruptible(&ndp10x->send_waitq);
    }
    spin_unlock(&ndp10x->send_ring_lock);
}

static irqreturn_t
ndp10x_isr(int irq, void *dev_id)
{
    int continue_io = 1;
    struct ndp10x_spi_dev_s *spi_dev = dev_id;
    struct ndp10x_s *ndp10x = spi_dev->ndp10x;

#ifdef CONFIG_PM_SLEEP
    pm_wakeup_event(&spi_dev->spi->dev, 2500);
    if (READ_ONCE(ndp10x->suspended)) {
        pr_debug("%s: interrupt while suspended\n", __func__);
        return IRQ_HANDLED;
    }
#endif

    pr_debug("%s: enter\n", __func__);
    ndp10x->isrs++;

    /* BUG_ON(!ndp10x->ndp); TODO: level-triggered should fix stray interrupt? */
    if (!ndp10x->ndp) {
        pr_debug("%s: interrupt without NDP\n", __func__);
        return IRQ_HANDLED;
    }

    ndp10x_poll_one(ndp10x);
    while (continue_io) {
        continue_io = 0;
        if (ndp10x_can_extract(ndp10x)) {
            ndp10x_extract_one(ndp10x);
            ndp10x_poll_one(ndp10x);
            continue_io = 1;
        }
        if (ndp10x_can_send(ndp10x)) {
            ndp10x_send_one(ndp10x);
            ndp10x_poll_one(ndp10x);
            continue_io = 1;
        }
    }
    
    pr_debug("%s: exit\n", __func__);

    return IRQ_HANDLED;
}


#ifdef CONFIG_COMPAT
static long
ndp10x_compat_ioctl(struct file *f, unsigned int cmd, unsigned long arg)
{
    pr_debug("%s: compat ioctl cmd=%X\n", __func__, cmd);
    return ndp10x_ioctl(f, cmd, (unsigned long)compat_ptr(arg));
}
#endif

static int
ndp10x_ioctl_init(struct ndp10x_s *ndp10x)
{
    struct syntiant_ndp_integration_interfaces_s iif;
    struct syntiant_ndp_device_s *ndp = NULL;
    struct syntiant_ndp_config_s ndp_config;
    int s;

    if (ndp10x->ndp) {
        ndp10x_ndp_uninit(ndp10x);
    }
    
    iif.d = ndp10x;
    iif.malloc = ndp10x_malloc;
    iif.free = ndp10x_free;
    iif.mbwait = ndp10x_mbwait;
    iif.get_type = ndp10x_get_type;
    iif.sync = ndp10x_sync;
    iif.unsync = ndp10x_unsync;
    iif.transfer = ndp10x_transfer;

    s = syntiant_ndp_init(&ndp, &iif, SYNTIANT_NDP_INIT_MODE_RESET);
    if (s) {
        pr_alert("%s: chip %d init failed: %s\n", __func__, ndp10x->minor,
                 syntiant_ndp_error_name(s));
        return ndp10x_translate_error(s);
    }

    ndp10x->ndp = ndp;

    s = ndp10x_enable(ndp10x);
    if (s) {
        ndp10x_ndp_uninit(ndp10x);
        return s;
    }

    memset(&ndp_config, 0, sizeof(ndp_config));
    s = syntiant_ndp_get_config(ndp, &ndp_config);
    if (s) {
        ndp10x_ndp_uninit(ndp10x);
        pr_alert("%s: ndp10x_config error: %s\n", __func__,
                 syntiant_ndp_error_name(s));
        return ndp10x_translate_error(s);
    }

    pr_info("ndp10x%d: %s initialized\n", ndp10x->minor,
            ndp_config.device_type);

    return 0;
}

static int
ndp10x_ioctl_ndp10x_config(struct ndp10x_s *ndp10x,
                           struct syntiant_ndp10x_config_s *ndp10x_config)
{
    int s, spi;
    int set_input = ndp10x_config->set & SYNTIANT_NDP10X_CONFIG_SET_DNN_INPUT;
    
    if (set_input) {
        spi = (ndp10x_config->dnn_input == SYNTIANT_NDP10X_CONFIG_DNN_INPUT_SPI);
        WRITE_ONCE(ndp10x->pcm_input, spi);
        if (READ_ONCE(ndp10x->armed)) {
            ndp10x_config->set |= SYNTIANT_NDP10X_CONFIG_SET_MATCH_PER_FRAME_ON;
            ndp10x_config->match_per_frame_on = spi;
        }
        mutex_lock(&ndp10x->ndp10x_config_ioctl_mutex);
        s = ndp10x_quiesce(ndp10x);
        if (s) {
            pr_err("%s: quiesce failed: %d\n", __func__, -s);
            goto out;
        }
    }
    
    s = syntiant_ndp10x_config(ndp10x->ndp, ndp10x_config);
    if (s) {
        pr_alert("%s: ndp10x_config error: %s\n", __func__,
                 syntiant_ndp_error_name(s));
    }
    s = ndp10x_translate_error(s);

 out:
    if (set_input) {
        WRITE_ONCE(ndp10x->extracts_left, 0);
        s = ndp10x_enable(ndp10x);
        mutex_unlock(&ndp10x->ndp10x_config_ioctl_mutex);
    }
    
    return s;
}

static int
ndp10x_ioctl_ndp_config(struct ndp10x_s *ndp10x,
                        struct ndp10x_ndp_config_s *ndp_config)
{
    int s = 0;
    int s0;
    struct syntiant_ndp_config_s s_ndp_config;
    struct syntiant_ndp_config_s *c = &s_ndp_config;
    char *buf = NULL;
    unsigned int len, device_type_len;

    pr_debug("%s: devtypelen: %d, fwlen: %d, parlen: %d, lablen: %d"
             ", pkglen: %d\n",
             __func__, ndp_config->device_type_len,
             ndp_config->firmware_version_len,
             ndp_config->parameters_version_len,
             ndp_config->labels_len,
             ndp_config->pkg_version_len);

    len = ndp_config->firmware_version_len
        + ndp_config->parameters_version_len
        + ndp_config->labels_len
        + ndp_config->pkg_version_len;

    buf = ndp10x_malloc(len);
    if (!buf) {
        s = -ENOMEM;
        goto out;
    }

    c->firmware_version = buf;
    c->firmware_version_len = ndp_config->firmware_version_len;
    c->parameters_version = c->firmware_version + c->firmware_version_len;
    c->parameters_version_len = ndp_config->parameters_version_len;
    c->labels = c->parameters_version + c->parameters_version_len;
    c->labels_len = ndp_config->labels_len;
    c->pkg_version = c->labels + c->labels_len;
    c->pkg_version_len = ndp_config->pkg_version_len;

    s0 = syntiant_ndp_get_config(ndp10x->ndp, c);
    s = ndp10x_translate_error(s0);
    if (s0) {
        pr_alert("%s: unable to get configuration: %s\n", __func__,
                 syntiant_ndp_error_name(s0));
        goto out;
    }

    device_type_len = strlen(c->device_type) + 1;
    len = min(ndp_config->device_type_len, device_type_len);
    if (copy_to_user(u64_to_user_ptr(ndp_config->device_type), c->device_type, len)) {
        pr_debug("%s: device type protection error\n", __func__);
        s = -EACCES;
        goto out;
    }

    len = min(ndp_config->firmware_version_len, c->firmware_version_len);
    if (copy_to_user(u64_to_user_ptr(ndp_config->firmware_version),
                     c->firmware_version, len)) {
        pr_debug("%s: firmware version protection error\n", __func__);
        s = -EACCES;
        goto out;
    }

    len = min(ndp_config->parameters_version_len, c->parameters_version_len);
    if (copy_to_user(u64_to_user_ptr(ndp_config->parameters_version),
                     c->parameters_version, len)) {
        pr_debug("%s: parameters version protection error\n", __func__);
        s = -EACCES;
        goto out;
    }

    len = min(ndp_config->labels_len, c->labels_len);
    if (copy_to_user(u64_to_user_ptr(ndp_config->labels), c->labels, len)) {
        pr_debug("%s: labels_version protection error\n", __func__);
        s = -EACCES;
        goto out;
    }

    len = min(ndp_config->pkg_version_len, c->pkg_version_len);
    if (copy_to_user(u64_to_user_ptr(ndp_config->pkg_version), c->pkg_version, len)) {
        pr_debug("%s: pkg_version protection error\n", __func__);
        s = -EACCES;
        goto out;
    }

    ndp_config->classes = c->classes;
    ndp_config->device_type_len = device_type_len;
    ndp_config->firmware_version_len = c->firmware_version_len;
    ndp_config->parameters_version_len = c->parameters_version_len;
    ndp_config->labels_len = c->labels_len;
    ndp_config->pkg_version_len = c->pkg_version_len;

 out:
    if (buf) {
        ndp10x_free(buf);
    }
    return s;
}

static int
ndp10x_pcm_rings_init(struct ndp10x_s *ndp10x, int frame_step)
{
    int s = 0;
    int extract_ring_size, send_ring_size;

    WRITE_ONCE(ndp10x->extracts_left, 0);
    extract_ring_size =
        ndp10x->extract_buffer_size * NDP10X_BYTES_PER_MILLISECOND
        / frame_step * frame_step;
    s = ring_allocate(&ndp10x->extract_ring, extract_ring_size, 1, frame_step);
    if (s) {
        pr_alert("%s: failed to allocate extract ring\n", __func__);
        goto error;
    }

    WRITE_ONCE(ndp10x->sends_outstanding, 0);
    send_ring_size =
        ndp10x->send_buffer_size * NDP10X_BYTES_PER_MILLISECOND
        / frame_step * frame_step;
    s = ring_allocate(&ndp10x->send_ring, send_ring_size, 1, frame_step);
    if (s) {
        pr_alert("%s: failed to allocate send ring\n", __func__);
        goto error;
    }

error:
    return s;
}

static void
ndp10x_pcm_rings_reset(struct ndp10x_s *ndp10x)
{
    WRITE_ONCE(ndp10x->audio_frame_step, 0);

    synchronize_irq(ndp10x->spi_dev.spi->irq);

    spin_lock(&ndp10x->send_ring_lock);
    ring_reset(&ndp10x->send_ring);
    spin_unlock(&ndp10x->send_ring_lock);

    spin_lock(&ndp10x->extract_ring_lock);
    ring_reset(&ndp10x->extract_ring);
    spin_unlock(&ndp10x->extract_ring_lock);

    wake_up_interruptible(&ndp10x->send_waitq);

    wake_up_interruptible(&ndp10x->extract_waitq);
}

static int
ndp10x_ioctl_load(struct ndp10x_s *ndp10x, struct ndp10x_load_s *load)
{
    uint8_t *package = NULL;
    int package_len, frame_step;
    struct syntiant_ndp10x_config_s ndp10x_config;
    int s0 = SYNTIANT_NDP_ERROR_NONE;
    int s = 0;

    if (load->length == 0) {
        memset(&ndp10x_config, 0, sizeof(ndp10x_config));
        ndp10x_config.get_all = 1;
        s0 = syntiant_ndp10x_config(ndp10x->ndp, &ndp10x_config);
        if (s0) {
            pr_err("%s: failed to get ndp10x config data: %s\n", __func__,
                     syntiant_ndp_error_name(s));
            s = ndp10x_translate_error(s0);
            goto out;
        }
        if (ndp10x_config.fw_pointers_addr) {
            memset(&ndp10x_config, 0, sizeof(ndp10x_config));
            ndp10x_config.set = SYNTIANT_NDP10X_CONFIG_SET_MATCH_PER_FRAME_ON;
            ndp10x_config.match_per_frame_on = 0;
            s0 = syntiant_ndp10x_config(ndp10x->ndp, &ndp10x_config);
            if (s0) {
                pr_err("%s: failed to disable match per frame: %s\n", __func__,
                       syntiant_ndp_error_name(s));
                s = ndp10x_translate_error(s0);
            }
            ndp10x_pcm_rings_reset(ndp10x);
        }

        s0 = syntiant_ndp_load(ndp10x->ndp, NULL, 0);
        s = ndp10x_translate_error(s0);
        if (s) {
            pr_alert("%s: initialize pkg load error: %s\n",
                     __func__, syntiant_ndp_error_name(s0));
        }
        goto out;
    }


    package_len = load->length;
    package = ndp10x_malloc(package_len);
    if (!package) {
        pr_alert("%s: unable to allocate %d bytes for package\n", __func__,
                 package_len);
        s = -ENOMEM;
        goto out;
    }

    if (copy_from_user(package, u64_to_user_ptr(load->package), package_len)) {
        pr_debug("%s: load package protection error\n", __func__);
        s = -EACCES;
        goto out;
    }

    s0 = syntiant_ndp_load(ndp10x->ndp, package, package_len);
    if (s0 == SYNTIANT_NDP_ERROR_NONE) {
        memset(&ndp10x_config, 0, sizeof(ndp10x_config));
        ndp10x_config.get_all = 1;
        s0 = syntiant_ndp10x_config(ndp10x->ndp, &ndp10x_config);
        if (s0) {
            pr_err("%s: failed to get ndp10x config data: %s\n", __func__,
                     syntiant_ndp_error_name(s));
            s = ndp10x_translate_error(s0);
            goto out;
        }
        frame_step = ndp10x_config.audio_frame_step * BYTES_PER_AUDIO_SAMPLE;
        s = ndp10x_pcm_rings_init(ndp10x, frame_step);
        if (s) {
           pr_err("%s: failed to init pcm rings\n", __func__);
           goto out;
        }
        WRITE_ONCE(ndp10x->audio_frame_step, frame_step);
        if (ndp10x_config.fw_pointers_addr) {
            memset(&ndp10x_config, 0, sizeof(ndp10x_config));
            ndp10x_config.set = SYNTIANT_NDP10X_CONFIG_SET_MATCH_PER_FRAME_ON;
            ndp10x_config.match_per_frame_on = 1;
            s0 = syntiant_ndp10x_config(ndp10x->ndp, &ndp10x_config);
            if (s0) {
                pr_err("%s: failed to set match-per-frame: %s\n", __func__,
                       syntiant_ndp_error_name(s0));
            }
            s = ndp10x_translate_error(s0);
            if (s) {
                goto out;
            }
        }
    } else {
        if (s0 != SYNTIANT_NDP_ERROR_MORE) {
            pr_info("%s: load package failed: %s\n", __func__,
                    syntiant_ndp_error_name(s0));
        }
        s = ndp10x_translate_error(s0);
        goto out;
    }

 out:
    load->error_code = s0;
    if (package) {
        ndp10x_free(package);
    }
    return s;
}

static int
ndp10x_ioctl_transfer(struct ndp10x_s *ndp10x,
                      struct ndp10x_transfer_s *transfer)
{
    int s = 0;
    void *buf = NULL;
    void *outbuf = NULL;
    void *inbuf = NULL;

    if (transfer->out && transfer->in) {
        pr_info("%s: duplex transfer requested\n", __func__);
        s = -EINVAL;
        goto out;
    }

    if (!transfer->count) {
        goto out;
    }

    buf = ndp10x_malloc(transfer->count);
    if (!buf) {
        pr_alert("%s: unable to allocate %d bytes for transfer\n",
                 __func__, transfer->count);
        s = -ENOMEM;
        goto out;
    }

    if (transfer->out) {
        if (copy_from_user(buf, u64_to_user_ptr(transfer->out),
                           transfer->count)) {
            pr_debug("%s: transfer out protection error\n", __func__);
            s = -EACCES;
            goto out;
        }
        outbuf = buf;
    } else {
        inbuf = buf;
    }

    s = ndp10x_transfer(ndp10x, transfer->mcu, transfer->addr,
                        outbuf, inbuf, transfer->count);
    if (s) {
        pr_info("%s: transfer failed %s\n", __func__,
                syntiant_ndp_error_name(s));
        s = ndp10x_translate_error(s);
        goto out;
    }

    if (inbuf) {
        if (copy_to_user(u64_to_user_ptr(transfer->in), inbuf, transfer->count)) {
            pr_debug("%s: transfer in protection error\n", __func__);
            s = -EACCES;
            goto out;
        }
    }

 out:
    if (buf) {
        ndp10x_free(buf);
    }
    return s;
}

static int
ndp10x_ioctl_pcm_extract(struct ndp10x_s *ndp10x,
                         struct ndp10x_pcm_extract_s *extract)
{
    int extract_size;
    int s = 0;
    int extracted = 0;
    int remaining = 0;
    int ioctl_size = extract->buffer_length;
    int block = !extract->nonblock;
    void *buf;
    struct ndp_ring_s *extract_ring = &ndp10x->extract_ring;

    s = mutex_lock_interruptible(&ndp10x->extract_ioctl_mutex);
    if (s) {
        pr_debug("%s: mutex interrupted\n", __func__);
        goto out;
    }

    if (extract->flush) {
        spin_lock(&ndp10x->extract_ring_lock);
        ring_reset(extract_ring);
        spin_unlock(&ndp10x->extract_ring_lock);
    }

    while (extracted < ioctl_size) {
        extract_size = ioctl_size - extracted;
        WRITE_ONCE(ndp10x->extract_waiting, extract_size);
        if (block) {
            s = wait_event_interruptible
                (ndp10x->extract_waitq,
                 !READ_ONCE(ndp10x->audio_frame_step)
                 || ring_cnt(extract_ring));
            if (s == -ERESTARTSYS) {
                pr_debug("%s: extract wait interrupted\n", __func__);
                break;
            } else if (s) {
                pr_err("%s: extract wait error: %d\n", __func__, -s);
                break;
            }
        } else if (ring_empty(extract_ring)) {
            break;
        }

        if (!READ_ONCE(ndp10x->audio_frame_step)) {
            s = -EINVAL;
            break;
        }
        
        extract_size = min(extract_size, ndp10x->extract_scratch_size);
        spin_lock(&ndp10x->extract_ring_lock);
        extract_size = min(extract_size, ring_cnt_to_end(extract_ring));
        buf = ring_consumer(extract_ring);
        memcpy(ndp10x->extract_scratch, buf, extract_size);
        ring_remove(extract_ring, extract_size);
        spin_unlock(&ndp10x->extract_ring_lock);
        if (extract->buffer) {
            if (copy_to_user(u64_to_user_ptr(extract->buffer + extracted),
                             ndp10x->extract_scratch,
                             extract_size)) {
                pr_debug("%s: extract buffer access error\n", __func__);
                s = -EACCES;
                break;
            }
        }
        extracted += extract_size;
    }

    spin_lock(&ndp10x->extract_ring_lock);
    remaining = ring_cnt(extract_ring);
    spin_unlock(&ndp10x->extract_ring_lock);
    
    WRITE_ONCE(ndp10x->extract_waiting, 0);
    mutex_unlock(&ndp10x->extract_ioctl_mutex);

 out:
    extract->extracted_length = extracted;
    extract->remaining_length = remaining;
    return s;
}

static int
ndp10x_ioctl_pcm_send(struct ndp10x_s *ndp10x, struct ndp10x_pcm_send_s *send)
{
    int send_size, used;
    int sent = 0;
    int block = !send->nonblock;
    int ioctl_size = send->buffer_length;
    int s = 0;
    struct ndp_ring_s *send_ring = &ndp10x->send_ring;

    s = mutex_lock_interruptible(&ndp10x->send_ioctl_mutex);
    if (s) {
        pr_debug("%s: mutex interrupted\n", __func__);
        goto out;
    }

    WRITE_ONCE(ndp10x->send_waiting, ioctl_size);

    while (sent < ioctl_size) {
        if (block) {
            s = wait_event_interruptible
                (ndp10x->send_waitq,
                 !READ_ONCE(ndp10x->audio_frame_step) || ring_space(send_ring));
            if (s == -ERESTARTSYS) {
                pr_debug("%s: send wait interrupted\n", __func__);
                break;
            } else if (s) {
                pr_err("%s: send wait error: %d\n", __func__, -s);
                break;
            }
        } else if (READ_ONCE(ndp10x->audio_frame_step)
                   && !ring_space(send_ring)) {
            break;
        }
        
        if (!READ_ONCE(ndp10x->audio_frame_step)) {
            s = -EINVAL;
            break;
        }

        send_size = ring_space_to_end(send_ring);
        send_size = min(ioctl_size - sent, send_size);
        if (copy_from_user(ring_producer(send_ring), 
                           u64_to_user_ptr(send->buffer + sent),
                           send_size)) {
            pr_debug("%s: send buffer access error\n", __func__);
            s = -EACCES;
            break;
        }

        sent += send_size;
        WRITE_ONCE(ndp10x->send_waiting, ioctl_size - sent);

        pr_debug("%s: adding %d bytes, idle: %d\n", __func__, send_size,
                 ring_empty(send_ring));
        
        spin_lock(&ndp10x->send_ring_lock);
        used = ring_cnt(send_ring);
        ring_add(send_ring, send_size);
        spin_unlock(&ndp10x->send_ring_lock);
        if (used < ndp10x->audio_frame_step
            && ndp10x->audio_frame_step <= used + send_size ) {
            irq_wake_thread(ndp10x->spi_dev.spi->irq, &ndp10x->spi_dev);
        }
    }

    if (sent < ioctl_size) {
        WRITE_ONCE(ndp10x->send_waiting, 0);
    }
    mutex_unlock(&ndp10x->send_ioctl_mutex);

 out:
    send->sent_length = sent;
    return s;
}

static int
ndp10x_ioctl_watch(struct ndp10x_s *ndp10x, struct ndp10x_watch_s *watch)
{
    int s = 0;
    struct ndp_ring_s *result_ring = &ndp10x->result_ring;
    long timeout = watch->timeout;
    uint64_t classes = watch->classes;
    struct ndp_result_s *result;
    struct timespec ts;
    uint32_t summary;
    unsigned int info;
    int match;
    int class_index;
    int armed_mode = watch->extract_match_mode;
    int armed_watch_active = READ_ONCE(ndp10x->armed_watch_active);
    int s0;
    struct syntiant_ndp10x_config_s ndp10x_config;
    uint32_t avail;

    pr_debug("%s: armed: %d, armed_active: %d, timeout: %ld, flush: %d"
             ", classes %llu\n",
             __func__, armed_mode, armed_watch_active, timeout, watch->flush,
             classes);

    timeout *= HZ;

    watch->match = 0;
    
    if (watch->flush) {
        spin_lock(&ndp10x->result_ring_lock);
        ring_reset(result_ring);
        spin_unlock(&ndp10x->result_ring_lock);
        goto out;
    }

    if (!armed_mode && armed_watch_active) {
        s = -EBUSY;
        pr_debug("%s: already armed, returning BUSY\n", __func__);
        goto out;
    }

    if (armed_mode && !armed_watch_active) {
        pr_debug("%s: arming\n", __func__);
        
        ndp10x->extract_before_match = watch->extract_before_match *
            NDP10X_BYTES_PER_MILLISECOND;
        WRITE_ONCE(ndp10x->armed, 1);
        synchronize_irq(ndp10x->spi_dev.spi->irq);
        spin_lock(&ndp10x->extract_ring_lock);
        ring_reset(&ndp10x->extract_ring);
        spin_unlock(&ndp10x->extract_ring_lock);
        wake_up_interruptible(&ndp10x->extract_waitq);
        WRITE_ONCE(ndp10x->armed_watch_active, 1);
    }
    
    memset(&ndp10x_config, 0, sizeof(ndp10x_config));
    ndp10x_config.set = SYNTIANT_NDP10X_CONFIG_SET_MATCH_PER_FRAME_ON;
    ndp10x_config.match_per_frame_on =
        !armed_mode || READ_ONCE(ndp10x->pcm_input);
    s = syntiant_ndp10x_config(ndp10x->ndp, &ndp10x_config);
    if (s) {
        pr_err("%s: failed to set match per frame %s\n", __func__,
               syntiant_ndp_error_name(s));
    }

    for (;;) {
        if (timeout < 0) {
            s = wait_event_interruptible(ndp10x->result_waitq,
                                         !ring_empty(result_ring));
        } else if (timeout) {
            s = wait_event_interruptible_timeout
                (ndp10x->result_waitq, !ring_empty(result_ring), timeout);
            if (0 <= s) {
                timeout = s;
                s = 0;
            }
        }

        if (!timeout && ring_empty(result_ring)) {
            s = -ETIME;
            pr_debug("%s: !timeout & result ring empty - returning %d\n", 
                     __func__, s);
            break;
        }

        if (s == -ERESTARTSYS) {
            pr_debug("%s: watch wait interrupted\n", __func__);
            break;
        } else if (s == -ETIME) {
            pr_debug("%s: result wait timeout\n", __func__);
            break;
        } else if (s) {
            pr_err("%s: watch wait error: %d\n", __func__, -s);
            break;
        }

        spin_lock(&ndp10x->result_ring_lock);
        result = ring_consumer(result_ring);
        ts = result->ts;
        avail = result->extract_len_avail;
        summary = result->summary;
        ring_remove(result_ring, 1);
        spin_unlock(&ndp10x->result_ring_lock);
        match = !!(summary & NDP10X_SPI_MATCH_MATCH_MASK);
        class_index = NDP10X_SPI_MATCH_WINNER_EXTRACT(summary);
        info = summary >> NDP10X_MATCH_INFO_SHIFT;
        if ((match && (classes & (1 << class_index)))
            || ndp10x->result_per_frame) {
            break;
        }
    }

    if (!s) {
        watch->ts.tv_sec = ts.tv_sec;
        watch->ts.tv_nsec = ts.tv_nsec;
        watch->match = match;
        watch->class_index = class_index;
        watch->info = info;
        watch->extract_before_match = avail / NDP10X_BYTES_PER_MILLISECOND;
    }
    
    if (armed_mode && s != -ERESTARTSYS) {
        WRITE_ONCE(ndp10x->armed, 0);        
        WRITE_ONCE(ndp10x->armed_watch_active, 0);
        memset(&ndp10x_config, 0, sizeof(ndp10x_config));
        ndp10x_config.set = SYNTIANT_NDP10X_CONFIG_SET_MATCH_PER_FRAME_ON;
        ndp10x_config.match_per_frame_on = 1;
        s0 = syntiant_ndp10x_config(ndp10x->ndp, &ndp10x_config);
        if (s0) {
            pr_err("%s: failed to restore match per frame %s\n", __func__,
                   syntiant_ndp_error_name(s));
            if (!s) {
                s = ndp10x_translate_error(s0);
            }
        }
    }

 out:
    return s;
}

static int
ndp10x_ioctl_driver_config(struct ndp10x_s *ndp10x,
                           struct ndp10x_driver_config_s *driver_config)
{
    int s = 0;

    ndp10x->result_per_frame = driver_config->result_per_frame;

    /* TODO the rest */
    return s;
}

static int
ndp10x_ioctl_statistics(struct ndp10x_s *ndp10x,
                        struct ndp10x_statistics_s *statistics)
{
    statistics->isrs = ndp10x->isrs;
    statistics->polls = ndp10x->polls;
    statistics->frames = ndp10x->frames;
    statistics->results = ndp10x->results;
    statistics->results_dropped = ndp10x->results_dropped;
    spin_lock(&ndp10x->result_ring_lock);
    statistics->result_ring_used = ring_cnt(&ndp10x->result_ring);
    spin_unlock(&ndp10x->result_ring_lock);
    statistics->extracts = ndp10x->extracts;
    statistics->extract_bytes = ndp10x->extract_bytes;
    statistics->extract_bytes_dropped = ndp10x->extract_bytes_dropped;
    spin_lock(&ndp10x->extract_ring_lock);
    statistics->extract_ring_used = ring_cnt(&ndp10x->extract_ring);
    spin_unlock(&ndp10x->extract_ring_lock);
    statistics->sends = ndp10x->sends;
    statistics->send_bytes = ndp10x->send_bytes;
    spin_lock(&ndp10x->send_ring_lock);
    statistics->send_ring_used = ring_cnt(&ndp10x->send_ring);
    spin_unlock(&ndp10x->send_ring_lock);
    if (statistics->clear) {
        ndp10x->isrs = 0;
        ndp10x->polls = 0;
        ndp10x->frames = 0;
        ndp10x->results = 0;
        ndp10x->results_dropped = 0;
        ndp10x->extracts = 0;
        ndp10x->extract_bytes = 0;
        ndp10x->extract_bytes_dropped = 0;
        ndp10x->sends = 0;
        ndp10x->send_bytes = 0;
    }

    return 0;
}


union ioctl_arg_u {
    struct syntiant_ndp10x_config_s ndp10x_config;
    struct ndp10x_ndp_config_s ndp_config;
    struct ndp10x_load_s load;
    struct ndp10x_watch_s watch;
    struct ndp10x_transfer_s transfer;
    struct ndp10x_driver_config_s driver_config;
    struct ndp10x_pcm_extract_s pcm_extract;
    struct ndp10x_pcm_send_s pcm_send;
    struct ndp10x_statistics_s statistics;
};

static long
ndp10x_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
    int s = SYNTIANT_NDP_ERROR_NONE;
    struct ndp10x_s *ndp10x;
    int arg_size = 0;
    char *arg_name = "*unknown*";
    union ioctl_arg_u arg_u;
    union ioctl_arg_u *argp = &arg_u;

    ndp10x = file->private_data;

    switch (cmd) {
    case INIT:
        arg_name = "init";
        argp = NULL;
        break;
    case NDP10X_CONFIG:
        arg_name = "ndp10x_config";
        arg_size = sizeof(struct syntiant_ndp10x_config_s);
        break;
    case NDP_CONFIG:
        arg_name = "ndp_config";
        arg_size = sizeof(struct ndp10x_ndp_config_s);
        break;
    case LOAD:
        arg_name = "load";
        arg_size = sizeof(struct ndp10x_load_s);
        break;
    case TRANSFER:
        arg_name = "transfer";
        arg_size = sizeof(struct ndp10x_transfer_s);
        break;
    case WATCH:
        arg_name = "watch";
        arg_size = sizeof(struct ndp10x_watch_s);
        break;
    case DRIVER_CONFIG:
        arg_name = "driver_config";
        arg_size = sizeof(struct ndp10x_driver_config_s);
        break;
    case PCM_EXTRACT:
        arg_name = "pcm_extract";
        arg_size = sizeof(struct ndp10x_pcm_extract_s);
        break;
    case PCM_SEND:
        arg_name = "pcm_send";
        arg_size = sizeof(struct ndp10x_pcm_send_s);
        break;
    case STATS:
        arg_name = "statistics";
        arg_size = sizeof(struct ndp10x_statistics_s);
        break;
    }
    
    pr_debug("%s: ioctl %s, cmd=0x%x\n", __func__, arg_name, cmd);

    switch (cmd) {
    case INIT:
    case DRIVER_CONFIG:
    case STATS:
        break;
    case PCM_SEND:
    case PCM_EXTRACT:
        if (!READ_ONCE(ndp10x->audio_frame_step)) {
            return -EINVAL;
        }
        /* fall through */
    default:
        if (!ndp10x->ndp) {
            return -EINVAL;
        }
    }

    if (argp) {
        if (copy_from_user(argp, (void __user *)arg, arg_size)) {
            pr_debug("%s: %s arg protection error\n", __func__, arg_name);
            return -EACCES;
        }
    }

    switch (cmd) {
    case INIT:
        s = ndp10x_ioctl_init(ndp10x);
        break;

    case NDP10X_CONFIG:
        s = ndp10x_ioctl_ndp10x_config(ndp10x, &argp->ndp10x_config);
        break;

    case NDP_CONFIG:
        s = ndp10x_ioctl_ndp_config(ndp10x, &argp->ndp_config);
        break;

    case LOAD:
        s = ndp10x_ioctl_load(ndp10x, &argp->load);
        break;

    case TRANSFER:
        s = ndp10x_ioctl_transfer(ndp10x, &argp->transfer);
        break;

    case WATCH:
        s = ndp10x_ioctl_watch(ndp10x, &argp->watch);
        break;

    case DRIVER_CONFIG:
        s = ndp10x_ioctl_driver_config(ndp10x, &argp->driver_config);
        break;

    case PCM_EXTRACT:
        s = ndp10x_ioctl_pcm_extract(ndp10x, &argp->pcm_extract);
        break;

    case PCM_SEND:
        s = ndp10x_ioctl_pcm_send(ndp10x, &argp->pcm_send);
        break;

    case STATS:
        s = ndp10x_ioctl_statistics(ndp10x, &argp->statistics);
        break;

    default:
        s = -EINVAL;
    }

    if (!s && argp) {
        if (copy_to_user((void __user *)arg, argp, arg_size)) {
            pr_debug("%s: %s return protection error\n", __func__, arg_name);
            s = -EACCES;
        }
    }

    return s;
}

static int
ndp10x_open(struct inode *inode, struct file *file)
{
    struct ndp10x_s *ndp10x;
    int minor = MINOR(inode->i_rdev);
    if (minor >= MAX_DEV)
        return -ENODEV;

    ndp10x = &ndp10xs[minor];
    file->private_data = ndp10x;

    ndp10x->opens++;
    pr_debug("%s: opened (%d)\n", __func__, ndp10x->opens);
    return 0;
}

static int
ndp10x_release(struct inode *inode, struct file *file)
{
    struct ndp10x_s *ndp10x;

    ndp10x = file->private_data;

    ndp10x->opens--;
    pr_debug("%s: closed (%d)\n", __func__, ndp10x->opens);
    return 0;
}

static int
ndp10x_spi_setup(struct ndp10x_s *ndp10x, struct spi_device *spi)
{
    struct ndp10x_spi_dev_s *d;
    int s = 0;

    pr_debug("%s: spi setting up\n", __func__);

    d = &ndp10x->spi_dev;
    d->ndp10x = ndp10x;
    d->spi_split = SPI_SPLIT;
    if (MAX_SPEED >= 1000000) {
        d->spi_read_delay = SPI_READ_DELAY;
    } else {
        d->spi_read_delay = 0;
    }

    spi->max_speed_hz = MAX_SPEED;
    spi->chip_select = CHIP_SELECT;
    spi->bits_per_word = 8;
    spi->mode = SPI_MODE;
    s = spi_setup(spi);
    if (s) {
        pr_err("%s: failed to setup spi\n", __func__);
        return s;
    }
    d->spi = spi;
    d->spi_pcm_input_rate = spi->max_speed_hz < 1000000 ? 
        spi->max_speed_hz:1000000;
    mutex_init(&d->lock);
    spi_set_drvdata(spi, &ndp10x->spi_dev);

    return 0;
}

static void
ndp10x_config_ext_pwr(void)
{
    /* get [optional] pwr info from device tree and configure */

    uint32_t gpio;
    struct device_node * np = NULL;

    np = of_find_compatible_node(NULL, NULL, SPI_DEVICE_NAME);
    if (np == NULL) {
        pr_err("%s: error - %s node node not found\n",
                __func__, SPI_DEVICE_NAME);
        return;
    }

    gpio  = of_get_named_gpio(np, "poweron-gpio", 0);
    gpio_request(gpio,"poweron_gpio");
    gpio_set_value(gpio,0);
    msleep(2);
    gpio_set_value(gpio,1);
}
#if 0
static void
ndp10x_config_ext_clk(void)
{
    /* get [optional] clk info from device tree and configure */

    struct clk *ext_clk = NULL;
    struct clk *parent_clk = NULL;
    struct clk *output_enable = NULL;

    const char *ext_clk_name;
    const char *parent_clk_name;
    const char *output_enable_name;
    uint32_t clk_freq = 0;

    struct device_node * np = NULL;

    np = of_find_compatible_node(NULL, NULL, SPI_DEVICE_NAME);
    if (np == NULL) {
        pr_err("%s: error - %s node node not found\n",
               __func__, SPI_DEVICE_NAME);
        return;
    }

    ext_clk_name = of_get_property(np, "clk-name", NULL);
    parent_clk_name = of_get_property(np, "clk-parent", NULL);
    output_enable_name = of_get_property(np, "clk-output-enable", NULL);
    of_property_read_u32(np, "clk-freq", &clk_freq);

    pr_debug("%s: Read CLK info: %s %s %s %u\n",
             __func__,
             ext_clk_name ? ext_clk_name : "[NULL]",
             parent_clk_name ? parent_clk_name : "[NULL]",
             output_enable_name ? output_enable_name : "[NULL]",
             clk_freq);

    if (ext_clk_name) {
        ext_clk = clk_get(NULL, ext_clk_name);
        if(IS_ERR(ext_clk)) {
            pr_err("can't get %s clock [ext]\n", ext_clk_name);
        }
    }

    if (parent_clk_name) {
        parent_clk = clk_get(NULL, parent_clk_name);
        if(IS_ERR(parent_clk)) {
            pr_err("can't get %s clock [parent]\n", parent_clk_name);
        }
    }

    if (output_enable_name) {
        output_enable = clk_get(NULL, output_enable_name);
        if(IS_ERR(output_enable)) {
            pr_err("can't get %s clock [output enable]\n", output_enable_name);
        }
    }

    if (!IS_ERR(ext_clk) && !IS_ERR(parent_clk)) {
        clk_set_parent(ext_clk, parent_clk);
    }

    if (!IS_ERR(ext_clk) && clk_freq) {
        clk_set_rate(ext_clk, clk_freq);
    }

    if (!IS_ERR(ext_clk)) {
        clk_prepare_enable(ext_clk);
    }

    if (!IS_ERR(output_enable)) {
        clk_prepare_enable(output_enable);
    }
}
#endif
static void 
ndp10x_config_irq(struct spi_device *spi)
{
    struct device_node   *node = NULL;
    node = of_find_matching_node(node, spi_match);  
	if (!node) {
		pr_err("%s: there is no this node\n", __func__);
		return ;
	}

    ndp10x_irq_pin  = of_get_named_gpio(node, "ndp10x_irq_pin", 0);
    if (ndp10x_irq_pin < 0) {
        pr_err("Unable to get ndp10x_irq_pin\n");
    } else {
        pr_debug("%s: ndp10x irq pin found:%d\n", __func__, ndp10x_irq_pin);
	}
   spi->irq = gpio_to_irq(ndp10x_irq_pin);
    
}
static int
ndp10x_spi_probe(struct spi_device *spi)
{
    int s, minor,error;
    unsigned long flags = IRQF_ONESHOT | IRQF_TRIGGER_RISING;
    struct ndp10x_spi_dev_s *d;
    struct ndp10x_s *ndp10x;
	struct input_dev *input;

    pr_debug("%s: spi probing\n", __func__);
    
    minor = 0;
    ndp10x = &ndp10xs[minor];
    d = &ndp10x->spi_dev;
    s = ndp10x_spi_setup(ndp10x, spi);
    if (s) return s;

    /* enable power for the NDP10x if enabled in device tree */
    ndp10x_config_ext_pwr();

    /* configure ext clocking for device if enabled in device tree */
   // ndp10x_config_ext_clk();
    ndp10x_config_irq(spi);
#ifdef CONFIG_PM_SLEEP
    device_init_wakeup(&spi->dev, 1);
#endif

    s = request_threaded_irq(spi->irq, NULL,
                             ndp10x_isr, flags, DEVICE_NAME, d);
    if (s) {
        pr_err("%s: failed to acquire irq %d\n", __func__, spi->irq);
    }

    ndp10x->procfs_dir_ent = proc_mkdir(DEVICE_NAME, NULL);
    if (ndp10x->procfs_dir_ent == NULL) {
        s = -ENOMEM;
        goto out;
    }

    ndp10x->procfs_info_file = proc_create_data("info", 0444,
                                                ndp10x->procfs_dir_ent,
                                                &ndp10x_procfs_info_fops,
                                                ndp10x);
    if (ndp10x->procfs_info_file == NULL) {
        s = -ENOMEM;
        goto no_info;
    }

    ndp10x->procfs_mic_ctl_file = proc_create_data("mic_ctl", 0666,
                                                   ndp10x->procfs_dir_ent,
                                                   &procfs_mic_ctl_fops,
                                                   ndp10x);
    if (ndp10x->procfs_info_file == NULL) {
        s = -ENOMEM;
        goto no_mic_ctl;
    }
    
	input = devm_input_allocate_device(&spi->dev);
	if (!input) {
		pr_err("failed to allocate input device\n");
		return -ENOMEM;
    }
	
	ndp10x->input = input;

	input_set_drvdata(input, ndp10x);
	input->name = "Aov Key";
	input->dev.parent = &spi->dev;
	input->id.bustype = BUS_SPI;
	input->id.vendor = 0x0001;
    input->id.product = 0x0001;
	input->id.version = 0x0100;

    set_bit(EV_KEY, input->evbit);
    input_set_capability(input, EV_KEY, KEY_F1);
    input_set_capability(input, EV_KEY, KEY_F2);
    error = input_register_device(input);
	if (error) {
		pr_err("Unable to register input device, error: %d\n",
			error);
		return -ENOMEM;
	}

    return s;

 no_mic_ctl:
    remove_proc_entry("info", ndp10x->procfs_dir_ent);

 no_info:
    remove_proc_entry(DEVICE_NAME, NULL);

 out:
    return s;
}

static int
ndp10x_spi_init(struct ndp10x_spi_dev_s *spi_dev)
{
    int s = 0;

    s = alloc_chrdev_region(&spi_dev->spi_devt, 0, 1, SPI_DEVICE_NAME);
    if (s) {
        pr_err("Error allocating chrdev region: %d\n", s);
        goto error;
    }

    spi_dev->spi_cls = class_create(THIS_MODULE, SPI_DEVICE_NAME);
    if (IS_ERR(spi_dev->spi_cls)) {
        s = PTR_ERR(spi_dev->spi_cls);
        pr_err("Error creating ndp10x_spi_dev_s.spi_cls: %d\n", s);
        goto error;
    }

    cdev_init(&spi_dev->cdev, &ndp10x_spi_fops);
    spi_dev->cdev.owner = THIS_MODULE;
    s = cdev_add(&spi_dev->cdev, spi_dev->spi_devt, 1);
    if (s != 0) {
        pr_err("Error calling cdev_add: %d\n", s);
        spi_dev->cdev.owner = NULL;
        goto error;
    }

    spi_dev->spi_dev = device_create(spi_dev->spi_cls, NULL,
                                     spi_dev->cdev.dev, spi_dev,
                                     SPI_DEVICE_NAME);
    if (IS_ERR(spi_dev->spi_dev)) {
        s = PTR_ERR(spi_dev->spi_dev);
        pr_err("device_create failed: %d\n", s);
        goto error;
    }

    s = spi_register_driver(&ndp10x_spi_driver);
    if (s != 0) {
        pr_err("Error registering spi driver: %d\n", s);
        goto error;
    }
    spi_dev->registered = 1;

    pr_debug("%s: spi init %d\n", __func__, s);

 error:
    return s;
};

static int __init
ndp10x_init(void)
{
    struct ndp10x_s *ndp10x;
    int s;
    int minor;

    ndp10x_major = register_chrdev(0, DEVICE_NAME, &fops);
    if (ndp10x_major < 0) {
        pr_alert("%s: failed to register a major number\n", __func__);
        s = ndp10x_major;
        goto error;
    }

    pr_debug("%s: major %d\n", __func__, ndp10x_major);

    ndp10x_class = class_create(THIS_MODULE, CLASS_NAME);
    if (IS_ERR(ndp10x_class)) {
        pr_alert("%s: failed to register device class\n", __func__);
        s = PTR_ERR(ndp10x_class);
        goto error;
    }

    memset(ndp10xs, 0, sizeof(ndp10xs));

    /* TODO: multiple devices */
    minor = 0;
    ndp10x = &ndp10xs[minor];

    mutex_init(&ndp10x->ndp_mutex);
    init_waitqueue_head(&ndp10x->mbwait_waitq);

    mutex_init(&ndp10x->ndp10x_config_ioctl_mutex);
    
    mutex_init(&ndp10x->send_ioctl_mutex);
    spin_lock_init(&ndp10x->send_ring_lock);
    init_waitqueue_head(&ndp10x->send_waitq);

    mutex_init(&ndp10x->extract_ioctl_mutex);
    spin_lock_init(&ndp10x->extract_ring_lock);
    init_waitqueue_head(&ndp10x->extract_waitq);

    spin_lock_init(&ndp10x->result_ring_lock);
    init_waitqueue_head(&ndp10x->result_waitq);

    ndp10x->extract_scratch_size = 512 * 2;
    ndp10x->extract_scratch = ndp10x_malloc(ndp10x->extract_scratch_size);
    if (!ndp10x->extract_scratch) {
        s = -ENOMEM;
        goto error;
    }

    s = ring_allocate(&ndp10x->result_ring, RESULT_RING_SIZE,
                      sizeof(struct ndp_result_s), 1);
    if (s) {
        pr_alert("%s: failed to allocate result ring\n", __func__);
        goto error;
    }

    /* default configuration for driver */
    ndp10x->extract_buffer_size = 10000;
    ndp10x->send_buffer_size = 100;
    ndp10x->sends_max_outstanding = 2;

    ndp10x->minor = minor;

    ndp10x->device = device_create(ndp10x_class, NULL,
                                   MKDEV(ndp10x_major, minor),
                                   NULL, DEVICE_NAME);
    if (IS_ERR(ndp10x->device)) {
        pr_alert("%s: failed to create device\n",__func__);
        s = PTR_ERR(ndp10x->device);
        goto error;
    }

    s = ndp10x_spi_init(&ndp10x->spi_dev);
    if (s) {
        goto error;
    }

 error:
    if (s) {
        ndp10x_uninit(ndp10x);
    } else {
        pr_info("ndp10x: driver loaded, version: %s\n", SYNTIANT_NDP_ILIB_VERSION);
    }

    return s;
}

static void
ndp10x_ndp_uninit(struct ndp10x_s *ndp10x)
{
    int s;
    
    s = ndp10x_quiesce(ndp10x);
    if (s) {
        pr_err("%s: quiesce failed: %d\n", __func__, -s);
    }

    s = syntiant_ndp_uninit(ndp10x->ndp, 1, 0);
    if (s) {
        pr_alert("%s: failed to uninit device %d: %s\n", __func__,
                 ndp10x->minor, syntiant_ndp_error_name(s));
    }
    
    WRITE_ONCE(ndp10x->armed, 0);
    ndp10x->ndp = NULL;
    pr_debug("%s: NDP uninited\n", __func__);
}

static int
ndp10x_spi_remove(struct spi_device *spi)
{
    struct ndp10x_spi_dev_s *spi_dev = spi_get_drvdata(spi);

    remove_proc_entry("mic_ctl", spi_dev->ndp10x->procfs_dir_ent);
    remove_proc_entry("info", spi_dev->ndp10x->procfs_dir_ent);
    remove_proc_entry(DEVICE_NAME, NULL);

    if (spi->irq > 0) {
        free_irq(spi->irq, spi_dev);
        pr_debug("%s: spi irq %d freed\n", __func__, spi->irq);
    }
    spi_set_drvdata(spi_dev->spi, NULL);
    spi_dev->spi = NULL;

    pr_debug("%s: spi driver removed from bus\n", __func__);
    return 0;
}

static void
ndp10x_spi_exit(struct ndp10x_spi_dev_s *spi_dev)
{
    if (spi_dev->registered) {
        spi_unregister_driver(&ndp10x_spi_driver);
        spi_dev->registered = 0;
        pr_debug("%s: spi driver unregistered\n", __func__);
    }
    if (spi_dev->spi_dev && !IS_ERR(spi_dev->spi_dev)) {
        device_destroy(spi_dev->spi_cls, spi_dev->spi_devt);
        spi_dev->spi_dev = NULL;
        pr_debug("%s: spi device destroyed\n", __func__);
    }
    if (spi_dev->cdev.owner) {
        cdev_del(&spi_dev->cdev);
        spi_dev->cdev.owner = NULL;
        pr_debug("%s: spi cdev deleted\n", __func__);
    }
    if (spi_dev->spi_cls && !IS_ERR(spi_dev->spi_cls)) {
        class_destroy(spi_dev->spi_cls);
        spi_dev->spi_cls = NULL;
        pr_debug("%s: spi class destroyed\n", __func__);
    }
    if (spi_dev->spi_devt) {
        unregister_chrdev_region(spi_dev->spi_devt, 1);
        spi_dev->spi_devt = 0;
        pr_debug("%s: spi chrdev region unregistered\n", __func__);
    }
    mutex_destroy(&spi_dev->lock);
    pr_debug("%s: spi exited\n", __func__);
}

static void
ndp10x_rings_free(struct ndp10x_s *ndp10x)
{
    spin_lock(&ndp10x->extract_ring_lock);
    ring_reset(&ndp10x->extract_ring);
    spin_unlock(&ndp10x->extract_ring_lock);
    ring_free(&ndp10x->extract_ring);
    mutex_destroy(&ndp10x->extract_ioctl_mutex);

    spin_lock(&ndp10x->send_ring_lock);
    ring_reset(&ndp10x->send_ring);
    spin_unlock(&ndp10x->send_ring_lock);
    ring_free(&ndp10x->send_ring);
    mutex_destroy(&ndp10x->send_ioctl_mutex);

    spin_lock(&ndp10x->result_ring_lock);
    ring_reset(&ndp10x->result_ring);
    spin_unlock(&ndp10x->result_ring_lock);
    ring_free(&ndp10x->result_ring);

    pr_debug("%s: rings freed\n", __func__);
}

static void
ndp10x_uninit(struct ndp10x_s *ndp10x)
{
    if (ndp10x->ndp) {
        ndp10x_ndp_uninit(ndp10x);
    }

    ndp10x_spi_exit(&ndp10x->spi_dev);

    if (ndp10x->device && !IS_ERR(ndp10x->device)) {
        device_destroy(ndp10x_class, MKDEV(ndp10x_major, ndp10x->minor));
        ndp10x->device = NULL;
        pr_debug("%s: spi device destroyed\n", __func__);
    }

    ndp10x_rings_free(ndp10x);

    mutex_destroy(&ndp10x->ndp10x_config_ioctl_mutex);
    
    mutex_destroy(&ndp10x->ndp_mutex);

    if (ndp10x->extract_scratch) {
        ndp10x_free(ndp10x->extract_scratch);
        ndp10x->extract_scratch = NULL;
        pr_debug("%s: extract scratch freed\n", __func__);
    }

    if (ndp10x_class && !IS_ERR(ndp10x_class)) {
        class_unregister(ndp10x_class);
        class_destroy(ndp10x_class);
        ndp10x_class = NULL;
        pr_debug("%s: spi class unregistered and destroyed\n", __func__);
    }

    if (0 <= ndp10x_major) {
        unregister_chrdev(ndp10x_major, DEVICE_NAME);
        ndp10x_major = -1;
        pr_debug("%s: chrdev unregistered\n", __func__);
    }
}

static void __exit
ndp10x_exit(void)
{
    struct ndp10x_s *ndp10x;
    int minor;

    minor = 0;
    ndp10x = &ndp10xs[minor];

    ndp10x_uninit(ndp10x);

    pr_info("ndp10x: driver unloaded\n");
}

module_init(ndp10x_init);
module_exit(ndp10x_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Syntiant Corporation");
MODULE_DESCRIPTION("NDP10x driver");
MODULE_VERSION(SYNTIANT_NDP_ILIB_VERSION);
