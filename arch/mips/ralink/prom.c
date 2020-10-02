//Nov 26, 2012--Modifications were made by U-Media Communication, inc.
#include <linux/init.h>
#include <linux/mm.h>
#include <linux/sched.h>
#include <linux/bootmem.h>
#include <linux/blkdev.h>

#include <asm/addrspace.h>
#include <asm/bootinfo.h>
#include <asm/cpu.h>
#include <asm/time.h>
#include <asm/tc3162/tc3162.h>

extern int __imem, __dmem;


#ifdef CONFIG_MIPS_TC3262
unsigned char io_swap_noneed=0;
EXPORT_SYMBOL(io_swap_noneed);
#endif

static void tc3162_component_setup(void)
{
	unsigned int controlReg;
	unsigned long flags;

	/* setup bus timeout value */
	VPint(CR_AHB_AACS) = 0xffff;

	/* reset hwnat */
	if (isRT65168) {
		/* table reset */
		VPint(0xbfbe0024) = 0x0;
		VPint(0xbfbe0024) = 0xffff;
		
		/* hwnat swreset */
		VPint(0xbfbe0000) = (1<<1);
	}

#ifdef CONFIG_CPU_TC3162
#ifdef CONFIG_TC3162_IMEM
	/* setup imem start address */
	VPint(CR_IMEM) = CPHYSADDR(&__imem);

	/* clear internal imem */
	local_irq_save(flags);
	controlReg = read_c0_cctl();
	write_c0_cctl(controlReg & ~CCTL_IMEMOFF);
	write_c0_cctl(controlReg | CCTL_IMEMOFF);
	write_c0_cctl(controlReg);
	local_irq_restore(flags);

	/* refill internal imem */
	local_irq_save(flags);
	controlReg = read_c0_cctl();
	write_c0_cctl(controlReg & ~CCTL_IMEMFILL4);
	write_c0_cctl(controlReg | CCTL_IMEMFILL4);
	write_c0_cctl(controlReg);
	local_irq_restore(flags);
	
	printk("Enable IMEM addr=%x\n", CPHYSADDR(&__imem));
#endif

#ifdef CONFIG_TC3162_DMEM
	/* setup dmem start address */
	VPint(CR_DMEM) = CPHYSADDR(&__dmem);

	memcpy((void *) 0xa0001000, (void *) KSEG1ADDR(&__dmem), 0x800);

	/* clear internal dmem */
	local_irq_save(flags);
	controlReg = read_c0_cctl();
	write_c0_cctl(controlReg & ~CCTL_DMEMOFF);
	write_c0_cctl(controlReg | CCTL_DMEMOFF);
	write_c0_cctl(controlReg);
	local_irq_restore(flags);

	/* internal dmem on */
	local_irq_save(flags);
	controlReg = read_c0_cctl();
	write_c0_cctl(controlReg & ~CCTL_DMEMON);
	write_c0_cctl(controlReg | CCTL_DMEMON);
	write_c0_cctl(controlReg);
	local_irq_restore(flags);

	printk("Enable DMEM addr=%x\n", CPHYSADDR(&__dmem));

	memcpy((void *) KSEG1ADDR(&__dmem), (void *) 0xa0001000, 0x800);
#endif
#endif
}

const char *get_system_type(void)
{
#ifdef CONFIG_MIPS_TC3262
	if (isTC3182)
		return "TrendChip TC3182 SOC";
	else if (isRT65168)
		return "Ralink RT65168 SOC";
	else if (isRT63165 || isRT6855A){
		io_swap_noneed = 1;
		return "Ralink RT63165 SOC";
	}else
		return "TrendChip TC3169 SOC";
#else
	if (isTC3162U)
		return "TrendChip TC3162U SOC";
	else if (isTC3162L5P5)
		return "TrendChip TC3162L5/P5 SOC";
	else if (isTC3162L4P4)
		return "TrendChip TC3162L4/P4 SOC";	
	else if (isTC3162L3P3)
		return "TrendChip TC3162L2F/P2F";
	else if (isTC3162L2P2)
		return "TrendChip TC3162L2/P2";
	else 
		return "TrendChip TC3162";
#endif
}

extern struct plat_smp_ops msmtc_smp_ops;

#define ra_inb(offset)          (*(volatile unsigned char *)(offset))
#define ra_inw(offset)          (*(volatile unsigned short *)(offset))
#define ra_inl(offset)          (*(volatile unsigned long *)(offset))

#define ra_outb(offset,val)     (*(volatile unsigned char *)(offset) = val)
#define ra_outw(offset,val)     (*(volatile unsigned short *)(offset) = val)
#define ra_outl(offset,val)     (*(volatile unsigned long *)(offset) = val)

#define LCR_DLAB        (1 << 7)        /* Divisor Latch Access Bit */
#define LCR_SB          (1 << 6)        /* Set Break */
#define LCR_STKYP       (1 << 5)        /* Sticky Parity */
#define LCR_EPS         (1 << 4)        /* Even Parity Select */
#define LCR_PEN         (1 << 3)        /* Parity Enable */
#define LCR_STB         (1 << 2)        /* Stop Bit */
#define LCR_WLS1        (1 << 1)        /* Word Length Select */
#define LCR_WLS0        (1 << 0)        /* Word Length Select */


static unsigned long uclk_20M[13]={ // 65000*(b*16*3)/1000000
        59904,          // Baud rate 115200
        29952,          // Baud rate 57600
        19968,          // Baud rate 38400
        14976,          // Baud rate 28800
        9984,           // Baud rate 19200
        7488,           // Baud rate 14400
        4992,           // Baud rate 9600
        2496,           // Baud rate 4800
        1248,           // Baud rate 2400
        624,            // Baud rate 1200
        312,            // Baud rate 600
        156,            // Baud rate 300
        57              // Baud rate 110
};


void bbu_uart_init(void)
{
        int i;
        unsigned long div_x, div_y;
        unsigned long word;

        // Set FIFO controo enable, reset RFIFO, TFIFO, 16550 mode, watermark=0x00 (1 byte)
        ra_outb(CR_UART_FCR, (0x0f|(0x0<<6)));

        // Set modem control to 0
        ra_outb(CR_UART_MCR, 0x0);

        // Disable IRDA, Disable Power Saving Mode, RTS , CTS flow control
        ra_outb(CR_UART_MISCC, 0x0);

        // Set interrupt Enable to, enable Tx, Rx and Line status
        /*ra_outb(CR_UART_IER, 0x01);*/

        /* access the bardrate divider */

        ra_outb(CR_UART_LCR, LCR_DLAB);

        div_y = 65000;
        div_x = uclk_20M[1];
        word = (div_x<<16)|div_y;
        ra_outl(CR_UART_XYD, word);

        /* Set Baud Rate Divisor to 1*16 */
        ra_outb(CR_UART_BRDL, 0x01);
        ra_outb(CR_UART_BRDH, 0x00);

        /* Set DLAB = 0, clength = 8, stop =1, no parity check  */
        ra_outb(CR_UART_LCR, LCR_WLS0 | LCR_WLS1);
}

void __init prom_init(void)
{
	unsigned long memsize;
	unsigned char samt;
	unsigned long col;
	unsigned long row;

	bbu_uart_init();
#ifdef CONFIG_MIPS_TC3262
	if (isRT63165 || isRT6855A) {
		/* enable external sync */
#if defined (CONFIG_RT2880_ROOTFS_IN_FLASH)
#ifdef CONFIG_SYSFS
		strcat(arcs_cmdline, " es=1 root=/dev/mtdblock5 ");
#else
		strcat(arcs_cmdline, " es=1 root=1f05 ");
#endif
#else
		strcat(arcs_cmdline, " es=1 root=/dev/ram0 ");
#endif
	}
//#ifndef CONFIG_SMP
#if 0
	{
		/* when kernel is UP, set ES=1. Otherwise, set in mips_mt_set_cpuoptions */
		unsigned int oconfig7 = read_c0_config7();
		unsigned int nconfig7 = oconfig7;

		nconfig7 |= (1 << 8);

		__asm__ __volatile("sync");
		write_c0_config7(nconfig7);
		ehb();
		printk("Config7: 0x%08x\n", read_c0_config7());
	}
#endif
	if (isRT63165 || isRT6855A) {
		/* DDR */
		if (VPint(CR_AHB_HWCONF) & (1<<25)) {
			memsize = 0x800000 * (1 << (((VPint(CR_DMC_DDR_CFG1) >> 18) & 0x7) - 1));
		/* SDRAM */
		} else {
			unsigned long sdram_cfg1;
			
			/* calculate SDRAM size */
			sdram_cfg1 = VPint(0xbfb20004);
			row = 11 + ((sdram_cfg1>>16) & 0x3);
			col = 8 + ((sdram_cfg1>>20) & 0x3);
			/* 4 bands and 16 bit width */
			memsize = (1 << row) * (1 << col) * 4 * 2;
		}
	} else {
		memsize = 0x800000 * (1 << (((VPint(CR_DMC_CTL1) >> 18) & 0x7) - 1));
	}
#else
	/* calculate SDRAM size */
	samt = VPchar(CR_DMC_SAMT);
	row = 8 + (samt & 0x3);
	col = 11 + ((samt>>2) & 0x3);
	/* 4 bands and 16 bit width */
	memsize = (1 << row) * (1 << col) * 4 * 2;
#endif

	printk("%s prom init\n", get_system_type());

	tc3162_component_setup();

	add_memory_region(0 + 0x20000, memsize - 0x20000, BOOT_MEM_RAM);

	//mips_machgroup = MACH_GROUP_TRENDCHIP;
	//mips_machtype = MACH_TRENDCHIP_TC3162;

#ifdef CONFIG_MIPS_MT_SMP
	register_smp_ops(&vsmp_smp_ops);
#endif
#ifdef CONFIG_MIPS_MT_SMTC
	register_smp_ops(&msmtc_smp_ops);
#endif
}

void __init prom_free_prom_memory(void)
{
	/* We do not have any memory to free */
}

int prom_putchar(char data)
{
	while (!(LSR_INDICATOR & LSR_THRE))
		;
	//disable console message, tim.wang@u-media.com.tw, 2012-11-23
#ifndef CONFIG_UMEDIA_DISABLE_CONSOLE
	VPchar(CR_UART_THR) = data; 
#endif
	return 1;
}
EXPORT_SYMBOL(prom_putchar);

char prom_getchar(void)
{
	while (!(LSR_INDICATOR & LSR_RECEIVED_DATA_READY))
		;
	return VPchar(CR_UART_RBR);
}

static char ppbuf[1024];

void
prom_write(const char *buf, unsigned int n)
{
	char ch;

	while (n != 0) {
		--n;
		if ((ch = *buf++) == '\n')
			prom_putchar('\r');
		prom_putchar(ch);
	}
}
EXPORT_SYMBOL(prom_write);

void
prom_printf(const char *fmt, ...)
{
	va_list args;
	int i;

	va_start(args, fmt);
	i = vscnprintf(ppbuf, sizeof(ppbuf), fmt, args);
	va_end(args);

	prom_write(ppbuf, i);
}
EXPORT_SYMBOL(prom_printf);

#ifdef CONFIG_KGDB
static unsigned long  uclk_65000[13]={
	357500, 	// uclk 5.5     Baud Rate 115200
	175500, 	// uclk 2.7     Baud Rate 57600
	119808, 	// uclk 1.8432  Baud Rate 38400
	89856,	// uclk	1.3824	Baud Rate 28800
	59904,	// uclk 0.9216	Baud Rate 19200
	44928,	// uclk 0.6912	Baud Rate 14400
	29952,	// uclk 0.4608	Baud Rate 9600
	14976,	// uclk 0.2304	Baud Rate 4800
	7488,	// uclk 0.1152	Baud Rate 2400 
	3744,	// uclk 0.0576	Baud Rate 1200
	1872,	// uclk 0.0288	Baud Rate 600
	936,		// uclk 0.0144	Baud Rate 300
	343		// uclk 0.00528	Baud Rate 110
};

static void hsuartInit(void)
{
	unsigned long	div_x,div_y;
	unsigned long	word;
	unsigned long   tmp;

	tmp = VPint(CR_GPIO_CTRL);
	tmp &= ~0x0fa30000;
	tmp |= 0x0fa30000;
	VPint(CR_GPIO_CTRL) = tmp; // set GPIO pin 13 & pin 12 are alternative outputs, GPIO pin 11 & pin 10 are alternative inputs
	tmp = VPint(CR_GPIO_ODRAIN);
	tmp &= ~0x00003000;
	tmp |= 0x00003000;
	VPint (CR_GPIO_ODRAIN) = tmp; // set GPIO output enable

// Set FIFO controo enable, reset RFIFO, TFIFO, 16550 mode, watermark=0x00 (1 byte)
	VPchar(CR_HSUART_FCR) = UART_FCR|UART_WATERMARK;

// Set modem control to 0
	VPchar(CR_HSUART_MCR) = UART_MCR;

// Disable IRDA, Disable Power Saving Mode, RTS , CTS flow control
	VPchar(CR_HSUART_MISCC) = UART_MISCC;

	/* access the bardrate divider */
	VPchar(CR_HSUART_LCR) = UART_BRD_ACCESS;

	div_y = UART_XYD_Y;
	div_x = (unsigned int)(uclk_65000[0]/SYS_HCLK)*2;
	word = (div_x<<16)|div_y;
	VPint(CR_HSUART_XYD) = word;

/* Set Baud Rate Divisor to 3*16		*/
	VPchar(CR_HSUART_BRDL) = UART_BRDL;
	VPchar(CR_HSUART_BRDH) = UART_BRDH;

/* Set DLAB = 0, clength = 8, stop =1, no parity check 	*/
	VPchar(CR_HSUART_LCR) = UART_LCR;

// Set interrupt Enable to, enable Tx, Rx and Line status
	VPchar(CR_HSUART_IER) = UART_IER;	
}

static int hsuartInitialized = 0;

int putDebugChar(char c)
{
	if (!hsuartInitialized) {
		hsuartInit();
		hsuartInitialized = 1;
	}

	while (!(VPchar(CR_HSUART_LSR) & LSR_THRE))
		;
	VPchar(CR_HSUART_THR) = c; 

	return 1;
}

char getDebugChar(void)
{
	if (!hsuartInitialized) {
		hsuartInit();
		hsuartInitialized = 1;
	}

	while (!(VPchar(CR_HSUART_LSR) & LSR_RECEIVED_DATA_READY))
		;
	return VPchar(CR_HSUART_RBR);
}
#endif
