//Dec 7, 2012--Modifications were made by U-Media Communication, inc.
/*G
 ***************************************************************************
 * Ralink Tech Inc.
 * 4F, No. 2 Technology 5th Rd.
 * Science-based Industrial Park
 * Hsin-chu, Taiwan, R.O.C.
 *
 * (c) Copyright, Ralink Technology, Inc.
 *
 *  This program is free software; you can redistribute  it and/or modify it
 *  under  the terms of  the GNU General  Public License as published by the
 *  Free Software Foundation;  either version 2 of the  License, or (at your
 *  option) any later version.
 *
 *  THIS  SOFTWARE  IS PROVIDED   ``AS  IS'' AND   ANY  EXPRESS OR IMPLIED
 *  WARRANTIES,   INCLUDING, BUT NOT  LIMITED  TO, THE IMPLIED WARRANTIES OF
 *  MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN
 *  NO  EVENT  SHALL   THE AUTHOR  BE    LIABLE FOR ANY   DIRECT, INDIRECT,
 *  INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 *  NOT LIMITED   TO, PROCUREMENT OF  SUBSTITUTE GOODS  OR SERVICES; LOSS OF
 *  USE, DATA,  OR PROFITS; OR  BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 *  ANY THEORY OF LIABILITY, WHETHER IN  CONTRACT, STRICT LIABILITY, OR TORT
 *  (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 *  THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 *  You should have received a copy of the  GNU General Public License along
 *  with this program; if not, write  to the Free Software Foundation, Inc.,
 *  675 Mass Ave, Cambridge, MA 02139, USA.
 *
 *
 ***************************************************************************
 *
 */
#include <linux/init.h>
#include <linux/version.h>
#include <linux/module.h>
#include <linux/config.h>
#include <linux/kernel.h>
#include <linux/interrupt.h>
#include <linux/fs.h>
#include <linux/sched.h>
#ifdef CONFIG_RALINK_GPIO_LED
#include <linux/timer.h>
#endif
#include <asm/uaccess.h>
#include "ralink_gpio.h"

#include <asm/rt2880/surfboardint.h>

#ifdef  CONFIG_DEVFS_FS
#include <linux/devfs_fs_kernel.h>
static  devfs_handle_t devfs_handle;
#endif

#define NAME			"ralink_gpio"
#define RALINK_GPIO_DEVNAME	"gpio"
int ralink_gpio_major = 252;
int ralink_gpio_irqnum = 0;
u32 ralink_gpio_intp = 0;
u32 ralink_gpio_edge = 0;
int gpio_dbg_level = GPIO_TRACE_NONE;
ralink_gpio_reg_info ralink_gpio_info[RALINK_GPIO_NUMBER];
extern unsigned long volatile jiffies;

#ifdef CONFIG_RALINK_GPIO_LED
#define RALINK_GPIO_LED_FREQ (HZ/10)
struct timer_list ralink_gpio_led_timer;
ralink_gpio_led_info ralink_gpio_led_data[RALINK_GPIO_NUMBER];

u32 ra_gpio_led_set = 0;
#if defined (CONFIG_RT6855A_PCIE_PORT0_ENABLE)
u32 ra_sgpo_led_set = 0;
#endif

#define DBGPRINT(level, fmt, args...)				\
do {								\
	if (level == gpio_dbg_level || gpio_dbg_level == GPIO_TRACE_ALL) {	\
		printk(fmt, ## args);				\
	}							\
} while (0)
						


struct ralink_gpio_led_status_t {
	int ticks;
	unsigned int ons;
	unsigned int offs;
	unsigned int resting;
	unsigned int times;
} ralink_gpio_led_stat[RALINK_GPIO_NUMBER];
#endif

MODULE_DESCRIPTION("Ralink SoC GPIO Driver");
MODULE_AUTHOR("Winfred Lu <winfred_lu@ralinktech.com.tw>");
MODULE_LICENSE("GPL");
ralink_gpio_reg_info info;

#if defined(CONFIG_UMEDIA_WAP783L) || defined(CONFIG_UMEDIA_WST786L) || defined(CONFIG_UMEDIA_WST376L)
static unsigned long reset_button_pressed_time = 0;
static int pressed_time_count = 0;
static int reset_going = 0;
#endif

void ralink_gpio_notify_user(int usr);

int ralink_gpio_led_set(ralink_gpio_led_info led)
{
#ifdef CONFIG_RALINK_GPIO_LED
	unsigned long tmp;
	if (0 <= led.gpio && led.gpio < RALINK_GPIO_NUMBER) {
		if (led.on > RALINK_GPIO_LED_INFINITY)
			led.on = RALINK_GPIO_LED_INFINITY;
		if (led.off > RALINK_GPIO_LED_INFINITY)
			led.off = RALINK_GPIO_LED_INFINITY;
		if (led.blinks > RALINK_GPIO_LED_INFINITY)
			led.blinks = RALINK_GPIO_LED_INFINITY;
		if (led.rests > RALINK_GPIO_LED_INFINITY)
			led.rests = RALINK_GPIO_LED_INFINITY;
		if (led.times > RALINK_GPIO_LED_INFINITY)
			led.times = RALINK_GPIO_LED_INFINITY;
		if (led.on == 0 && led.off == 0 && led.blinks == 0 &&
				led.rests == 0) {
			ralink_gpio_led_data[led.gpio].gpio = -1; //stop it
			return 0;
		}
		//register led data
		ralink_gpio_led_data[led.gpio].gpio = led.gpio;
		ralink_gpio_led_data[led.gpio].on = (led.on == 0)? 1 : led.on;
		ralink_gpio_led_data[led.gpio].off = (led.off == 0)? 1 : led.off;
		ralink_gpio_led_data[led.gpio].blinks = (led.blinks == 0)? 1 : led.blinks;
		ralink_gpio_led_data[led.gpio].rests = (led.rests == 0)? 1 : led.rests;
		ralink_gpio_led_data[led.gpio].times = (led.times == 0)? 1 : led.times;

		//clear previous led status
		ralink_gpio_led_stat[led.gpio].ticks = -1;
		ralink_gpio_led_stat[led.gpio].ons = 0;
		ralink_gpio_led_stat[led.gpio].offs = 0;
		ralink_gpio_led_stat[led.gpio].resting = 0;
		ralink_gpio_led_stat[led.gpio].times = 0;

		DBGPRINT(GPIO_TRACE_IO, "led=%d, on=%d, off=%d, blinks,=%d, reset=%d, time=%d\n", 
				ralink_gpio_led_data[led.gpio].gpio,
				ralink_gpio_led_data[led.gpio].on,
				ralink_gpio_led_data[led.gpio].off,
				ralink_gpio_led_data[led.gpio].blinks,
				ralink_gpio_led_data[led.gpio].rests,
				ralink_gpio_led_data[led.gpio].times);
#if defined (CONFIG_RT6855A_PCIE_PORT0_ENABLE)
		if (led.gpio == 32)
			return 0;
#endif
		//set gpio direction to 'out'
		if (led.gpio <= 15) {
			tmp = le32_to_cpu(*(volatile u32 *)(RALINK_REG_PIODIR));
			tmp |= RALINK_GPIO(led.gpio);
			*(volatile u32 *)(RALINK_REG_PIODIR) = tmp;
		}
		else {
			tmp = le32_to_cpu(*(volatile u32 *)(RALINK_REG_PIO3116DIR));
			tmp |= RALINK_GPIO((led.gpio-16));
			*(volatile u32 *)(RALINK_REG_PIO3116DIR) = tmp;
		}
		DBGPRINT(GPIO_TRACE_IO, "dir_%lu gpio_%d - %d %d %d %d %d\n", tmp,
				led.gpio, led.on, led.off, led.blinks,
				led.rests, led.times);
	}
	else {
		printk(KERN_ERR NAME ": gpio(%d) out of range\n", led.gpio);
		return -1;
	}
	return 0;
#else
	printk(KERN_ERR NAME ": gpio led support not built\n");
	return -1;
#endif
}
EXPORT_SYMBOL(ralink_gpio_led_set);

#if LINUX_VERSION_CODE > KERNEL_VERSION(2,6,35)
long ralink_gpio_ioctl(struct file *file, unsigned int req,
		unsigned long arg)
#else
int ralink_gpio_ioctl(struct inode *inode, struct file *file, unsigned int req,
		unsigned long arg)
#endif
{
	unsigned long tmp;
	ralink_gpio_reg_info info;
#ifdef CONFIG_RALINK_GPIO_LED
	ralink_gpio_led_info led;
#endif

	req &= RALINK_GPIO_DATA_MASK;

	switch(req) {
	case RALINK_GPIO_SET_DIR:
		*(volatile u32 *)(RALINK_REG_PIODIR) = cpu_to_le32(arg);
		break;
	case RALINK_GPIO_SET_DIR_IN:
		tmp = le32_to_cpu(*(volatile u32 *)(RALINK_REG_PIODIR));
		DBGPRINT(GPIO_TRACE_IO, "read DIR: %lu, val: %x\n", tmp, ~(0x3<<(arg*2)));
		tmp &= ~(0x3<<(arg*2));
		DBGPRINT(GPIO_TRACE_IO, "write DIR: %lu\n", tmp);
		*(volatile u32 *)(RALINK_REG_PIODIR) = cpu_to_le32(tmp);
		tmp = le32_to_cpu(*(volatile u32 *)(RALINK_REG_GPIOOE));
		DBGPRINT(GPIO_TRACE_IO, "read GPO: %lu, val: %x\n", tmp, ~(0x1<<arg));
		tmp &= ~(0x1<<arg);
		DBGPRINT(GPIO_TRACE_IO, "write GPO: %lu\n", tmp);
		*(volatile u32 *)(RALINK_REG_GPIOOE) = cpu_to_le32(tmp);
		break;
	case RALINK_GPIO_SET_DIR_OUT:
		tmp = le32_to_cpu(*(volatile u32 *)(RALINK_REG_PIODIR));
		DBGPRINT(GPIO_TRACE_IO, "read DIR: %lu, val: %x\n", tmp, 0x1<<(arg*2));
		tmp |= 0x1<<(arg*2);
		DBGPRINT(GPIO_TRACE_IO, "write DIR: %lu\n", tmp);
		*(volatile u32 *)(RALINK_REG_PIODIR) = cpu_to_le32(tmp);
		tmp = le32_to_cpu(*(volatile u32 *)(RALINK_REG_GPIOOE));
		DBGPRINT(GPIO_TRACE_IO, "read GPO: %lu, val: %x\n", tmp, 0x1<<arg);
		tmp |= 0x1<<arg;
		DBGPRINT(GPIO_TRACE_IO, "write GPO: %lu\n", tmp);
		*(volatile u32 *)(RALINK_REG_GPIOOE) = cpu_to_le32(tmp);
		break;
	case RALINK_GPIO_READ: //RALINK_GPIO_READ_INT
		tmp = le32_to_cpu(*(volatile u32 *)(RALINK_REG_PIODATA));
		put_user(tmp, (int __user *)arg);
		break;
	case RALINK_GPIO_WRITE: //RALINK_GPIO_WRITE_INT
		*(volatile u32 *)(RALINK_REG_PIODATA) = cpu_to_le32(arg);
		break;
	case RALINK_GPIO_ENABLE_INTP:
		*(volatile u32 *)(RALINK_REG_INTENA) |= cpu_to_le32(RALINK_INTCTL_PIO);
		break;
	case RALINK_GPIO_DISABLE_INTP:
		*(volatile u32 *)(RALINK_REG_INTDIS) &= ~cpu_to_le32(RALINK_INTCTL_PIO);
		break;
	case RALINK_GPIO_REG_IRQ:
		copy_from_user(&info, (ralink_gpio_reg_info *)arg, sizeof(info));
		if (0 <= info.irq && info.irq < RALINK_GPIO_NUMBER/2) {
			tmp = le32_to_cpu(*(volatile u32 *)(RALINK_REG_PIOEDGE));
			DBGPRINT(GPIO_TRACE_INT, "read PIOEDGE: %lu, val: %x, irq=: %x\n", tmp, 0x3<<(info.irq*2), info.irq);
			tmp |=  0x3<<(info.irq*2);
			DBGPRINT(GPIO_TRACE_INT, "write PIOEDGE: %lu\n", tmp);
			*(volatile u32 *)(RALINK_REG_PIOEDGE) = cpu_to_le32(tmp);
			ralink_gpio_info[info.irq].pid = info.pid;
		}
		else
			printk(KERN_ERR NAME ": irq number(%d) out of range\n",
					info.irq);
		break;
	case RALINK_GPIO3116_SET_DIR:
		*(volatile u32 *)(RALINK_REG_PIO3116DIR) = cpu_to_le32(arg);
		break;
	case RALINK_GPIO3116_SET_DIR_IN:
		tmp = le32_to_cpu(*(volatile u32 *)(RALINK_REG_PIO3116DIR));
		tmp &= ~cpu_to_le32(arg);
		*(volatile u32 *)(RALINK_REG_PIO3116DIR) = tmp;
		break;
	case RALINK_GPIO3116_SET_DIR_OUT:
		tmp = le32_to_cpu(*(volatile u32 *)(RALINK_REG_PIO3116DIR));
		tmp |= cpu_to_le32(arg);
		*(volatile u32 *)(RALINK_REG_PIO3116DIR) = tmp;
		break;

	case RALINK_GPIO_LED_SET:
#ifdef CONFIG_RALINK_GPIO_LED
		copy_from_user(&led, (ralink_gpio_led_info *)arg, sizeof(led));
		ralink_gpio_led_set(led);
#else
		printk(KERN_ERR NAME ": gpio led support not built\n");
#endif
		break;
	default:
		return -ENOIOCTLCMD;
	}
	return 0;
}

int ralink_gpio_open(struct inode *inode, struct file *file)
{
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,0)
	MOD_INC_USE_COUNT;
#else
	try_module_get(THIS_MODULE);
#endif
	return 0;
}

int ralink_gpio_release(struct inode *inode, struct file *file)
{
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,0)
	MOD_DEC_USE_COUNT;
#else
	module_put(THIS_MODULE);
#endif
	return 0;
}

struct file_operations ralink_gpio_fops =
{
	owner:		THIS_MODULE,
#if LINUX_VERSION_CODE > KERNEL_VERSION(2,6,35)
	unlocked_ioctl:	ralink_gpio_ioctl,
#else
	ioctl:		ralink_gpio_ioctl,
#endif
	open:		ralink_gpio_open,
	release:	ralink_gpio_release,
};

#ifdef CONFIG_RALINK_GPIO_LED

#if defined (CONFIG_RT6855A_PCIE_PORT0_ENABLE)
#if RALINK_GPIO_LED_LOW_ACT
#define __LED_ON(gpio)		do { \
					if(gpio<32) ra_gpio_led_set &= ~RALINK_GPIO(gpio); \
					else ra_sgpo_led_set &= ~RALINK_GPIO((gpio-32)); \
				} while (0);
#define __LED_OFF(gpio)		do { \
					if(gpio<32) ra_gpio_led_set |= RALINK_GPIO(gpio); \
					else ra_sgpo_led_set |= RALINK_GPIO((gpio-32)); \
				} while (0);
#else
#define __LED_ON(gpio)		do { \
					if(gpio<32) ra_gpio_led_set |= RALINK_GPIO(gpio); \
					else ra_sgpo_led_set |= RALINK_GPIO((gpio-32)); \
				} while (0);
#define __LED_OFF(gpio)		do { \
					if(gpio<32) ra_gpio_led_set &= ~RALINK_GPIO(gpio); \
					else ra_sgpo_led_set &= ~RALINK_GPIO((gpio-32)); \
				} while (0);
#endif
#else
#if RALINK_GPIO_LED_LOW_ACT
#define __LED_ON(gpio)      ra_gpio_led_set &= ~RALINK_GPIO(gpio);
#define __LED_OFF(gpio)     ra_gpio_led_set |= RALINK_GPIO(gpio);
#else
#define __LED_ON(gpio)      ra_gpio_led_set |= RALINK_GPIO(gpio);
#define __LED_OFF(gpio)     ra_gpio_led_set &= ~RALINK_GPIO(gpio);
#endif
#endif

static void ralink_gpio_led_do_timer(unsigned long unused)
{
	int i;
	unsigned int x;

//Cisco asked to trigger reset procedure once the reset button is pressed exceed specified time
//And blink PWR LED to indicate the reset procedure is started
#if defined(CONFIG_UMEDIA_WAP783L) || defined(CONFIG_UMEDIA_WST786L) || defined(CONFIG_UMEDIA_WST376L)
	if (reset_button_pressed_time > 0) {
		pressed_time_count++;
		if (pressed_time_count >= 50) {
			printk("Press RESET button exceed spcified time, send SIGUSR2 to user spece for trigger reset procedure\n");

			ralink_gpio_led_data[GPIO_PWR_LED].gpio = GPIO_PWR_LED;
			ralink_gpio_led_data[GPIO_PWR_LED].off = 5;
			ralink_gpio_led_data[GPIO_PWR_LED].rests = 0;
			ralink_gpio_led_data[GPIO_PWR_LED].on = 5;
			ralink_gpio_led_data[GPIO_PWR_LED].blinks = RALINK_GPIO_LED_INFINITY;
			ralink_gpio_led_data[GPIO_PWR_LED].times = 1;
			
			ralink_gpio_irqnum = GPIO_RESET_BUTTON;
#if defined (CONFIG_UMEDIA_WAP783L)
			ralink_gpio_notify_user(1);
#else
			ralink_gpio_notify_user(2);
#endif
			reset_button_pressed_time = 0;
			reset_going = 100; //delay about 10 seconds for prevent from duplicate reset command be executed
		}
	} else {
		pressed_time_count = 0;
		if(reset_going) {
			reset_going--;
		}
	}
#endif

	for (i = 0; i < RALINK_GPIO_NUMBER; i++) {
		ralink_gpio_led_stat[i].ticks++;
		if (ralink_gpio_led_data[i].gpio == -1) //-1 means unused
			continue;
		if (ralink_gpio_led_data[i].on == RALINK_GPIO_LED_INFINITY ||
				ralink_gpio_led_data[i].off == 0) { //always on
			__LED_ON(i);
			continue;
		}
		if (ralink_gpio_led_data[i].off == RALINK_GPIO_LED_INFINITY ||
				ralink_gpio_led_data[i].rests == RALINK_GPIO_LED_INFINITY ||
				ralink_gpio_led_data[i].on == 0 ||
				ralink_gpio_led_data[i].blinks == 0 ||
				ralink_gpio_led_data[i].times == 0) { //always off
			__LED_OFF(i);
			continue;
		}

		//led turn on or off
		if (ralink_gpio_led_data[i].blinks == RALINK_GPIO_LED_INFINITY ||
				ralink_gpio_led_data[i].rests == 0) { //always blinking
			x = ralink_gpio_led_stat[i].ticks % (ralink_gpio_led_data[i].on
					+ ralink_gpio_led_data[i].off);
		}
		else {
			unsigned int a, b, c, d, o, t;
			a = ralink_gpio_led_data[i].blinks / 2;
			b = ralink_gpio_led_data[i].rests / 2;
			c = ralink_gpio_led_data[i].blinks % 2;
			d = ralink_gpio_led_data[i].rests % 2;
			o = ralink_gpio_led_data[i].on + ralink_gpio_led_data[i].off;
			//t = blinking ticks
			t = a * o + ralink_gpio_led_data[i].on * c;
			//x = ticks % (blinking ticks + resting ticks)
			x = ralink_gpio_led_stat[i].ticks %
				(t + b * o + ralink_gpio_led_data[i].on * d);
			//starts from 0 at resting cycles
			if (x >= t)
				x -= t;
			x %= o;
		}
		if (x < ralink_gpio_led_data[i].on) {
			__LED_ON(i);
			if (ralink_gpio_led_stat[i].ticks && x == 0)
				ralink_gpio_led_stat[i].offs++;
			DBGPRINT(GPIO_TRACE_IO, "t%d gpio%d on,", ralink_gpio_led_stat[i].ticks, i);
		}
		else {
			__LED_OFF(i);
			if (x == ralink_gpio_led_data[i].on)
				ralink_gpio_led_stat[i].ons++;
			DBGPRINT(GPIO_TRACE_IO, "t%d gpio%d off,", ralink_gpio_led_stat[i].ticks, i);
		}

		//blinking or resting
		if (ralink_gpio_led_data[i].blinks == RALINK_GPIO_LED_INFINITY ||
				ralink_gpio_led_data[i].rests == 0) { //always blinking
			continue;
		}
		else {
			x = ralink_gpio_led_stat[i].ons + ralink_gpio_led_stat[i].offs;
			if (!ralink_gpio_led_stat[i].resting) {
				if (x == ralink_gpio_led_data[i].blinks) {
					ralink_gpio_led_stat[i].resting = 1;
					ralink_gpio_led_stat[i].ons = 0;
					ralink_gpio_led_stat[i].offs = 0;
					ralink_gpio_led_stat[i].times++;
				}
			}
			else {
				if (x == ralink_gpio_led_data[i].rests) {
					ralink_gpio_led_stat[i].resting = 0;
					ralink_gpio_led_stat[i].ons = 0;
					ralink_gpio_led_stat[i].offs = 0;
				}
			}
		}
		if (ralink_gpio_led_stat[i].resting) {
			__LED_OFF(i);
			DBGPRINT(GPIO_TRACE_IO, "resting,");
		} else {
			DBGPRINT(GPIO_TRACE_IO, "blinking,");
		}

		//number of times
		if (ralink_gpio_led_data[i].times != RALINK_GPIO_LED_INFINITY)
		{
			if (ralink_gpio_led_stat[i].times ==
					ralink_gpio_led_data[i].times) {
				__LED_OFF(i);
				ralink_gpio_led_data[i].gpio = -1; //stop
			}
			DBGPRINT(GPIO_TRACE_IO, "T%d\n", ralink_gpio_led_stat[i].times);
		} else {
			DBGPRINT(GPIO_TRACE_IO, "T@\n");
		}
	}


	//always turn the power LED on
	//__LED_ON(6);

/*
#if RALINK_GPIO_LED_LOW_ACT
	*(volatile u32 *)(RALINK_REG_PIODATA) = cpu_to_le32(ra_gpio_led_set);
#else // RALINK_GPIO_LED_LOW_ACT //
	*(volatile u32 *)(RALINK_REG_PIODATA) = cpu_to_le32(ra_gpio_led_set);
#endif // RALINK_GPIO_LED_LOW_ACT //
*/
	*(volatile u32 *)(RALINK_REG_PIODATA) = cpu_to_le32(ra_gpio_led_set);
#if defined (CONFIG_RT6855A_PCIE_PORT0_ENABLE)
	*(volatile u32 *)(RALINK_REG_SGPIOLEDDATA) = cpu_to_le32(ra_sgpo_led_set);
#endif

	DBGPRINT(GPIO_TRACE_IO, "led_set= %x\n", ra_gpio_led_set);
#if defined (CONFIG_RT6855A_PCIE_PORT0_ENABLE)
	DBGPRINT(GPIO_TRACE_IO, "spgo_led_set= %x\n", ra_sgpo_led_set);
#endif

	//ra_gpio_led_set = 0;
#if defined(CONFIG_UMEDIA_WAP783L) || defined(CONFIG_UMEDIA_WST786L) || defined(CONFIG_UMEDIA_WST376L) // Dias Kuo, For signal strength LED indicator, the POWER LED always ON
	__LED_ON(GPIO_PWR_LED);
	DBGPRINT(GPIO_TRACE_IO, "%s, %d, Always turn on the POWER LED.\n", __func__, __LINE__);
#endif

	init_timer(&ralink_gpio_led_timer);
	ralink_gpio_led_timer.expires = jiffies + RALINK_GPIO_LED_FREQ;
	add_timer(&ralink_gpio_led_timer);
}

void ralink_gpio_led_init_timer(void)
{
	int i;
	u32 val;

	for (i = 0; i < RALINK_GPIO_NUMBER; i++)
		ralink_gpio_led_data[i].gpio = -1; //-1 means unused

	// set GPIO0~GPIO5 to output mode
	val = le32_to_cpu(*(volatile u32 *)(RALINK_REG_PIODIR));
	DBGPRINT(GPIO_TRACE_IO, "read GPIO 0~15 DIR: %x\n", val);
	val |= 0x55555555;
	DBGPRINT(GPIO_TRACE_IO, "write GPIO 0~15 DIR: %x\n", val);
	*(volatile u32 *)(RALINK_REG_PIODIR) = cpu_to_le32(val);
	// set GPIO16~GPIO31 to output mode
	val = le32_to_cpu(*(volatile u32 *)(RALINK_REG_PIO3116DIR));
	DBGPRINT(GPIO_TRACE_IO, "read GPIO 16~31 DIR: %x\n", val);
	val |= 0x55555555;
	DBGPRINT(GPIO_TRACE_IO, "write GPIO 16~31 DIR: %x\n", val);
	*(volatile u32 *)(RALINK_REG_PIO3116DIR) = cpu_to_le32(val);
	// enable output control
	val = le32_to_cpu(*(volatile u32 *)(RALINK_REG_GPIOOE));
	DBGPRINT(GPIO_TRACE_IO, "read GPIO 0~31 DIR: %x\n", val);
	val |= 0xffffffff;
	DBGPRINT(GPIO_TRACE_IO, "write GPIO 0~31 DIR: %x\n", val);
	*(volatile u32 *)(RALINK_REG_GPIOOE) = cpu_to_le32(val);
#if RALINK_GPIO_LED_LOW_ACT
#if defined(CONFIG_UMEDIA_WAP783L) || defined(CONFIG_UMEDIA_WST786L) || defined(CONFIG_UMEDIA_WST376L) //high active
	ra_gpio_led_set = 0x7fffffff; // turn on  POWER LED (GPIO 31) on startup
#else
	ra_gpio_led_set = 0xffffffff;
#endif
#if defined (CONFIG_RT6855A_PCIE_PORT0_ENABLE)
	ra_sgpo_led_set = 0xffffffff;
#endif
#else // RALINK_GPIO_LED_LOW_ACT //
	ra_gpio_led_set = 0x0;
#if defined (CONFIG_RT6855A_PCIE_PORT0_ENABLE)
	ra_sgpo_led_set = 0x0;
#endif
#endif // RALINK_GPIO_LED_LOW_ACT //
	*(volatile u32 *)(RALINK_REG_PIODATA) = cpu_to_le32(ra_gpio_led_set);
#if defined (CONFIG_RT6855A_PCIE_PORT0_ENABLE)
	*(volatile u32 *)(RALINK_REG_SGPIOLEDDATA) = cpu_to_le32(ra_sgpo_led_set);
#endif

	init_timer(&ralink_gpio_led_timer);
	ralink_gpio_led_timer.function = ralink_gpio_led_do_timer;
	ralink_gpio_led_timer.expires = jiffies + RALINK_GPIO_LED_FREQ;
	add_timer(&ralink_gpio_led_timer);
}
#endif

/*
 * send a signal(SIGUSR1) to the registered user process whenever any gpio
 * interrupt comes
 * (called by interrupt handler)
 */
void ralink_gpio_notify_user(int usr)
{
	struct task_struct *p = NULL;

	if (ralink_gpio_irqnum < 0 || RALINK_GPIO_NUMBER <= ralink_gpio_irqnum) {
		printk(KERN_ERR NAME ": gpio irq number out of range\n");
		return;
	}

	//don't send any signal if pid is 0 or 1
	if ((int)ralink_gpio_info[ralink_gpio_irqnum].pid < 2)
		return;
#if LINUX_VERSION_CODE > KERNEL_VERSION(2,6,35)
	p = find_task_by_vpid(ralink_gpio_info[ralink_gpio_irqnum].pid);
#else
	p = find_task_by_pid(ralink_gpio_info[ralink_gpio_irqnum].pid);
#endif

	if (NULL == p) {
		printk(KERN_ERR NAME ": no registered process to notify\n");
		return;
	}

	if (usr == 1) {
		printk(KERN_NOTICE NAME ": GPIO%d sending a SIGUSR1 to process %d\n",
				ralink_gpio_irqnum, ralink_gpio_info[ralink_gpio_irqnum].pid);
		send_sig(SIGUSR1, p, 0);
	}
	else if (usr == 2) {
		printk(KERN_NOTICE NAME ": GPIO%d sending a SIGUSR2 to process %d\n",
				ralink_gpio_irqnum, ralink_gpio_info[ralink_gpio_irqnum].pid);
		send_sig(SIGUSR2, p, 0);
	}
}

/*
 * 1. save the PIOINT and PIOEDGE value
 * 2. clear PIOINT by writing 1
 * (called by interrupt handler)
 */
void ralink_gpio_save_clear_intp(void)
{
	ralink_gpio_intp = le32_to_cpu(*(volatile u32 *)(RALINK_REG_PIOINT));
	*(volatile u32 *)(RALINK_REG_PIOINT) = cpu_to_le32(0xffff);
	DBGPRINT(GPIO_TRACE_INT, "INTstate:%x\n", ralink_gpio_intp);
}

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,0)
void ralink_gpio_irq_handler(unsigned int irq, struct irqaction *irqaction)
#else
irqreturn_t ralink_gpio_irq_handler(int irq, void *irqaction)
#endif
{
	struct gpio_time_record {
		unsigned long timer;
		unsigned long trigger;
	};
	static struct gpio_time_record record[RALINK_GPIO_NUMBER/2];
	unsigned long now, data;
	int i;

	ralink_gpio_save_clear_intp();
	now = jiffies;
#if RALINK_GPIO_LED_LOW_ACT
	data = ~(le32_to_cpu(*(volatile u32 *)(RALINK_REG_PIODATA)));
#else
	data = le32_to_cpu(*(volatile u32 *)(RALINK_REG_PIODATA));
#endif
	for (i = 0; i < RALINK_GPIO_NUMBER/2; i++) {
		if (! (ralink_gpio_intp & (1 << i)))
			continue;
		ralink_gpio_irqnum = i;

		if (data & ralink_gpio_intp) {	// rising edge
			record[i].timer = now;
			record[i].trigger = 1;
			DBGPRINT(GPIO_TRACE_INT, "record jiffies: %lu\n", record[i].timer);

#if defined(CONFIG_UMEDIA_WAP783L) || defined(CONFIG_UMEDIA_WST786L) || defined(CONFIG_UMEDIA_WST376L)
			if(i == GPIO_RESET_BUTTON) {
				reset_button_pressed_time = now;
				printk("Reset button is pressed\n");
			}
#endif
		} else {			// falling edge
			if (record[i].trigger == 0)
				continue;
			if (time_before_eq(now, record[i].timer + 20L)) {
#if defined(CONFIG_UMEDIA_WAP783L) || defined(CONFIG_UMEDIA_WST786L) || defined(CONFIG_UMEDIA_WST376L)
				if (i == GPIO_WPS_BUTTON) {
					// 2012/10/26, Dias Kuo, According to Cisco's spec, trigger WPS by a short WPS button click
					printk(KERN_NOTICE NAME ": short (%lu), GPIO%d trigger WPS...\n", now, i);
					ralink_gpio_notify_user(2); // running WPS
				}
#else
				DBGPRINT(GPIO_TRACE_INT, "too short (%lu)\n", now);
#endif
			}	
			else if (time_before(now, record[i].timer + 200L)) {
#if defined(CONFIG_UMEDIA_WAP783L) || defined(CONFIG_UMEDIA_WST786L) || defined(CONFIG_UMEDIA_WST376L)
				if (i != GPIO_WPS_BUTTON && i != GPIO_RESET_BUTTON) {
#endif
					DBGPRINT(GPIO_TRACE_INT, "i=%d, one click (%lu)\n", i, now);
					ralink_gpio_notify_user(1);
#if defined(CONFIG_UMEDIA_WAP783L) || defined(CONFIG_UMEDIA_WST786L) || defined(CONFIG_UMEDIA_WST376L)
				} else if (i == GPIO_WPS_BUTTON) {
					ralink_gpio_notify_user(2); // running WPS
				}
#endif
			} else {
#if defined(CONFIG_UMEDIA_WAP783L) || defined(CONFIG_UMEDIA_WST786L) || defined(CONFIG_UMEDIA_WST376L)
				if (i == GPIO_WPS_BUTTON) {
					/*
					if (time_after_eq(now, record[i].timer + (HZ * 3)) && time_before(now, record[i].timer + (HZ * 6))) {	
						printk(KERN_NOTICE NAME ": GPIO%d, push WPS button between 3~6 seconds.\n", i);
						ralink_gpio_notify_user(2); // running WPS
					} else 
					*/
					if (time_after_eq(now, record[i].timer + (HZ * 6))) {
						printk(KERN_NOTICE NAME ": GPIO%d, push WPS button >= 6 seconds\n", i);
						ralink_gpio_notify_user(1); // send SIGUSR1 to user space U-MEDIA signal strength led daemon
					}
				} else if (i == GPIO_RESET_BUTTON) {
					//Ricky Cao: base on Cisco's spec, it needs to press and hold about 5 seconds to trigger reset procedure
					if (time_after_eq(now, record[i].timer + (HZ * 5)) && reset_going == 0) {
						printk(KERN_NOTICE NAME ": GPIO%d, push WPS button >= 5 seconds\n", i);
						ralink_gpio_notify_user(2);
						reset_going = 100;  //delay about 10 seconds for prevent from duplicate reset command be executed
					}
				} else {
					DBGPRINT(GPIO_TRACE_INT, "i=%d, push several seconds (%lu)\n", i, now);
					ralink_gpio_notify_user(2);
				}
#else
				DBGPRINT(GPIO_TRACE_INT, "i=%d, push several seconds (%lu)\n", i, now);
				ralink_gpio_notify_user(2);
#endif
			}
#if defined(CONFIG_UMEDIA_WAP783L) || defined(CONFIG_UMEDIA_WST786L) || defined(CONFIG_UMEDIA_WST376L)
			if(i == GPIO_RESET_BUTTON) {
				reset_button_pressed_time = 0;
				pressed_time_count = 0;
				printk("Reset button is released\n");
			}
#endif
			record[i].trigger = 0;
		}
		
		break;
	}

	return IRQ_HANDLED;
}

int __init ralink_gpio_init(void)
{
	unsigned int i;
	u32 gpiomode;
	int err;

#ifdef  CONFIG_DEVFS_FS
	if (devfs_register_chrdev(ralink_gpio_major, RALINK_GPIO_DEVNAME,
				&ralink_gpio_fops)) {
		printk(KERN_ERR NAME ": unable to register character device\n");
		return -EIO;
	}
	devfs_handle = devfs_register(NULL, RALINK_GPIO_DEVNAME,
			DEVFS_FL_DEFAULT, ralink_gpio_major, 0,
			S_IFCHR | S_IRUGO | S_IWUGO, &ralink_gpio_fops, NULL);
#else
	int r = 0;
	r = register_chrdev(ralink_gpio_major, RALINK_GPIO_DEVNAME,
			&ralink_gpio_fops);
	if (r < 0) {
		printk(KERN_ERR NAME ": unable to register character device\n");
		return r;
	}
	if (ralink_gpio_major == 0) {
		ralink_gpio_major = r;
		printk(KERN_DEBUG NAME ": got dynamic major %d\n", r);
	}
#endif

#if LINUX_VERSION_CODE > KERNEL_VERSION(2,6,35)
    	err = request_irq(SURFBOARDINT_GPIO, ralink_gpio_irq_handler, \
	    IRQF_DISABLED, NAME, NULL);
#else
	err = request_irq(SURFBOARDINT_GPIO, ralink_gpio_irq_handler, \
	    SA_INTERRUPT, NAME, NULL);
#endif
	if (err)
	    return err;

#if 0
	u32 rstctrl2;
	rstctrl2 = le32_to_cpu(*(volatile u32 *)(RALINK_REG_RSTCTRL2));
	rstctrl2 |= 1<<13;
	*(volatile u32 *)(RALINK_REG_RSTCTRL2) = cpu_to_le32(rstctrl2);
	rstctrl2 &= ~(1<<13);
	*(volatile u32 *)(RALINK_REG_RSTCTRL2) = cpu_to_le32(rstctrl2);
#endif

#if defined(CONFIG_UMEDIA_WAP783L) || defined(CONFIG_UMEDIA_WST786L) || defined(CONFIG_UMEDIA_WST376L)
	//the following actions have been done at sock_register() in order to support power led, 
	//so just return 0 for CONFIG_UMEDIA_WAP783L
	//tim.wang@u-media.com.tw, 2012-11-16
	return 0;
#endif
	//config these pins to gpio mode (share pin)
	gpiomode = le32_to_cpu(*(volatile u32 *)(RALINK_REG_GPIOMODE));
	//printk("====== original GPIO Mode: 0x%0X ======\n", gpiomode);
	gpiomode &= ~RALINK_GPIOMODE_DFT;
#ifdef CONFIG_UMEDIA_WRT892L
	//printk("====== after GPIO Mode: 0x%0X ======\n", gpiomode);
	// Please ref P.62 of RT6856 Data sheet v0.1_2011-11-02.pdf
	gpiomode &= ~(1<<1); // GPIO 2 to GPIO
	gpiomode &= ~(1<<4);	// GPIO 30 to GPIO
	//printk("====== after2 GPIO Mode: 0x%0X ======\n", gpiomode);
#endif
#if defined(CONFIG_UMEDIA_WAP783L) || defined(CONFIG_UMEDIA_WST786L) || defined(CONFIG_UMEDIA_WST376L)
	printk("Initiate GPIO mode for WAP-783L\n");
	gpiomode &= ~(RALINK_GPIOMODE_SPICS1 | RALINK_GPIOMODE_PCM | RALINK_GPIOMODE_SPICS5);
#endif
	*(volatile u32 *)(RALINK_REG_GPIOMODE) = cpu_to_le32(gpiomode);

	//enable gpio interrupt
	*(volatile u32 *)(RALINK_REG_INTENA) |= cpu_to_le32(RALINK_INTCTL_PIO);
	for (i = 0; i < RALINK_GPIO_NUMBER/2; i++) {
		ralink_gpio_info[i].irq = i;
		ralink_gpio_info[i].pid = 0;
	}
	// *(volatile u32 *)(RALINK_REG_PIOEDGE) = cpu_to_le32(0xffffffff);	// set GPIO0~GPIO15 to interrupt edge

#ifdef CONFIG_RALINK_GPIO_LED
	ralink_gpio_led_init_timer();
#endif
	printk("Ralink gpio driver initialized\n");

	u32 tmp = 0;
#ifdef CONFIG_UMEDIA_WRT892L
	// GPIO 0~15, ref. P.122 of RT6856 Data sheet v0.1_2011-11-02.pdf
	tmp = le32_to_cpu(*(volatile u32 *)(RALINK_REG_PIODIR));
	//printk("read RALINK_REG_PIO=0x%x, DIRGPIO 0~15 DIR: %x\n", RALINK_REG_PIODIR, tmp);
	tmp &= ~(1 << 4); // set GPIO2 to input (Load Default Button)
	tmp &= ~(1 << 12); // set GPIO6 to input (WPS Button)
	//printk("write GPIO 0~15 DIR: %x\n", tmp);
	*(volatile u32 *)(RALINK_REG_PIODIR) = cpu_to_le32(tmp);

	// GPIO 16~31, ref. P.126 of RT6856 Data sheet v0.1_2011-11-02.pdf
	tmp = le32_to_cpu(*(volatile u32 *)(RALINK_REG_PIO3116DIR));
	//printk("read RALINK_REG_PIO=0x%x, DIRGPIO 16~31 DIR: %x\n", RALINK_REG_PIO3116DIR, tmp);
	tmp |= (1 << 20); // set GPIO 26 to output (WPS Orange LED)
	tmp |= (1 << 22); // set GPIO 27 to output (USB LED)
	tmp |= (1 << 28); // set GPIO 30 to output (WPS Green LED)
	//printk("write GPIO 16~31 DIR: %x\n", tmp);
	*(volatile u32 *)(RALINK_REG_PIO3116DIR) = cpu_to_le32(tmp);
#endif
	
#if defined(CONFIG_UMEDIA_WAP783L) || defined(CONFIG_UMEDIA_WST786L) || defined(CONFIG_UMEDIA_WST376L)
	printk("Initiate GPIO for WAP-783L\n");
	
	unsigned long val=0;

	// set GPIO2 and GPIO6 to input mode
	val = le32_to_cpu(*(volatile u32 *)(RALINK_REG_PIODIR));
	printk("read GPIO 0~15 DIR: %x\n", val);
	val &= ~0x1010;
	printk("write GPIO 0~15 DIR: %x\n", val);
	*(volatile u32 *)(RALINK_REG_PIODIR) = val;
	
	// set GPIO21 and GPIO31 to output mode
	val = le32_to_cpu(*(volatile u32 *)(RALINK_REG_PIO3116DIR));
	printk("read GPIO 16~31 DIR: %x\n", val);
	val |= 0x40000400;
	printk("write GPIO 16~31 DIR: %x\n", val);
	*(volatile u32 *)(RALINK_REG_PIO3116DIR) = val;

	// 2012/10/3, Dias Kuo, According to Cisco's spec, blink power LED on booting every 1 HZ
	// Example: 1 HZ (blink 1 times per 1 sec), 2 HZ (blink 2 times per 1 sec)
	// TODO: We also need to blink power LED before GPIO initiation (uboot, uncompressing kernel, ...)
	ralink_gpio_led_info led;

	led.gpio = GPIO_PWR_LED;
	led.on = 5; 		// 500ms
	led.off = 5; 		// 500ms
	led.blinks = 40;	// 20 seconds
	led.rests = 0;
	led.times = 1;
	
	ralink_gpio_led_set(led);
#endif
	
	return 0;
}

#if defined(CONFIG_UMEDIA_WAP783L) || defined(CONFIG_UMEDIA_WST786L) || defined(CONFIG_UMEDIA_WST376L)
void blink_pwr_led(void)
{
	u32 gpiomode;
	unsigned long val=0;

#if !defined(CONFIG_UMEDIA_WST376L) //CONFIG_UMEDIA_WAP783L || CONFIG_UMEDIA_WST786L
	printk("Initiate power LED for WAP-783L\n");
	//config these pins to gpio mode (share pin)
	gpiomode = le32_to_cpu(*(volatile u32 *)(RALINK_REG_GPIOMODE));
	//printk("====== original GPIO Mode: 0x%0X ======\n", gpiomode);
	gpiomode &= ~RALINK_GPIOMODE_DFT;
	printk("Initiate GPIO mode for WAP-783L\n");
	gpiomode &= ~(RALINK_GPIOMODE_SPICS1 | RALINK_GPIOMODE_PCM | RALINK_GPIOMODE_SPICS5);
	*(volatile u32 *)(RALINK_REG_GPIOMODE) = cpu_to_le32(gpiomode);

	// set GPIO2 and GPIO6 to input mode
	val = le32_to_cpu(*(volatile u32 *)(RALINK_REG_PIODIR));
	printk("read GPIO 0~15 DIR: %x\n", val);
	val &= ~0x1010;
	printk("write GPIO 0~15 DIR: %x\n", val);
	*(volatile u32 *)(RALINK_REG_PIODIR) = val;
	
	// set GPIO21 and GPIO31 to output mode
	val = le32_to_cpu(*(volatile u32 *)(RALINK_REG_PIO3116DIR));
	printk("read GPIO 16~31 DIR: %x\n", val);
	val |= 0x40000400;
	printk("write GPIO 16~31 DIR: %x\n", val);
	*(volatile u32 *)(RALINK_REG_PIO3116DIR) = val;
#else //CONFIG_UMEDIA_WST376L
	// set GPIO0 and GPIO1 to input mode
	val = le32_to_cpu(*(volatile u32 *)(RALINK_REG_PIODIR));
	printk("read GPIO 0~15 DIR: %x\n", val);
	val &= ~0x3;
	printk("write GPIO 0~15 DIR: %x\n", val);
	*(volatile u32 *)(RALINK_REG_PIODIR) = val;
	
	// set GPIO6 to output mode
	val = le32_to_cpu(*(volatile u32 *)(RALINK_REG_PIO3116DIR));
	printk("read GPIO 16~31 DIR: %x\n", val);
	val |= 0x40;
	printk("write GPIO 16~31 DIR: %x\n", val);
	*(volatile u32 *)(RALINK_REG_PIO3116DIR) = val;
#endif

	// 2012/10/3, Dias Kuo, According to Cisco's spec, blink power LED on booting every 1 HZ
	// Example: 1 HZ (blink 1 times per 1 sec), 2 HZ (blink 2 times per 1 sec)
	// TODO: We also need to blink power LED before GPIO initiation (uboot, uncompressing kernel, ...)
	ralink_gpio_led_info led;

	led.gpio = GPIO_PWR_LED;
	led.on = 5; 		// 500ms
	led.off = 5; 		// 500ms
	led.blinks = 80;	//40 seconds
	led.rests = 0;
	led.times = 1;
	ralink_gpio_led_set(led);
	//adjust the start ticks to let the POWER LED won't light too long at first blinking of linux phase
	//no better choice to use this way due to not sure why as tested the mdelay did not really delay anything here 
	//mdelay(100)
	// tim.wang@u-media.com.tw, 2013-1-11
	ralink_gpio_led_stat[GPIO_PWR_LED].ticks = 4;
}
#endif

void __exit ralink_gpio_exit(void)
{
#ifdef  CONFIG_DEVFS_FS
	devfs_unregister_chrdev(ralink_gpio_major, RALINK_GPIO_DEVNAME);
	devfs_unregister(devfs_handle);
#else
	unregister_chrdev(ralink_gpio_major, RALINK_GPIO_DEVNAME);
#endif

	//config these pins to normal mode
	*(volatile u32 *)(RALINK_REG_GPIOMODE) &= ~RALINK_GPIOMODE_DFT;
	//disable gpio interrupt
	*(volatile u32 *)(RALINK_REG_INTDIS) &= ~cpu_to_le32(RALINK_INTCTL_PIO);
#ifdef CONFIG_RALINK_GPIO_LED
	del_timer(&ralink_gpio_led_timer);
#endif
	printk("Ralink gpio driver exited\n");
}

module_init(ralink_gpio_init);
module_exit(ralink_gpio_exit);

