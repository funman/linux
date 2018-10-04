//Jun 12, 2012--Modifications were made by U-Media Communication, inc.




/*
 *  Interrupt service routines for Trendchip board
 */
#include <linux/init.h>
#include <linux/irq.h>
#include <linux/interrupt.h>
#include <asm/io.h>
#include <asm/irq.h>
#include <asm/mipsregs.h>
#include <asm/tc3162/tc3162.h>
#include <linux/sched.h>

#ifdef CONFIG_MIPS_TC3262
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/kernel_stat.h>
#include <linux/kernel.h>
#include <linux/random.h>
#include <asm/mipsmtregs.h>
#else
#include <linux/io.h>
#include <asm/irq_cpu.h>
#endif

#define ALLINTS (IE_SW0 | IE_SW1 | IE_IRQ0 | IE_IRQ1 | IE_IRQ2 | IE_IRQ3 | IE_IRQ4 | IE_IRQ5)

#define CR_INTC_BASE    0xBFB40000                
			// --- Interrupt Type Register ---		  
#define CR_INTC_ITR     (CR_INTC_BASE+0x0000)
			// --- Interrupt Mask Register ---		  
#define CR_INTC_IMR     (CR_INTC_BASE+0x0004)
			// --- Interrupt Pending Register ---	  
#define CR_INTC_IPR     (CR_INTC_BASE+0x0008)
			// --- Interrupt Set Register ---		  
#define CR_INTC_ISR    	(CR_INTC_BASE+0x000c)     
			// --- Interrupt Priority Register 0 ---  
#define CR_INTC_IPR0    (CR_INTC_BASE+0x0010)     
			// --- Interrupt Priority Register 1 ---  
#define CR_INTC_IPR1    (CR_INTC_BASE+0x0014)     
			// --- Interrupt Priority Register 2 ---  
#define CR_INTC_IPR2    (CR_INTC_BASE+0x0018)     
			// --- Interrupt Priority Register 3 ---  
#define CR_INTC_IPR3    (CR_INTC_BASE+0x001c)     
			// --- Interrupt Priority Register 4 ---  
#define CR_INTC_IPR4    (CR_INTC_BASE+0x0020)     
			// --- Interrupt Priority Register 5 ---  
#define CR_INTC_IPR5    (CR_INTC_BASE+0x0024)    
			// --- Interrupt Priority Register 6 ---  
#define CR_INTC_IPR6    (CR_INTC_BASE+0x0028)     
			// --- Interrupt Priority Register 7 ---  
#define CR_INTC_IPR7    (CR_INTC_BASE+0x002c)     
			// --- Interrupt Vector egister --- 	  
#define CR_INTC_IVR     (CR_INTC_BASE+0x0030)     

#define tc_outl(offset,val)    	(*(volatile unsigned long *)(offset) = val)
#define tc_inl(offset) 			(*(volatile unsigned long *)(offset))

#ifdef CONFIG_MIPS_TC3262

static DEFINE_SPINLOCK(tc3162_irq_lock);

static inline void unmask_mips_mt_irq(unsigned int irq)
{
	unsigned int vpflags = dvpe();
	int cpu_irq = 0;

	if ((irq == SI_SWINT1_INT1) || (irq == SI_SWINT_INT1))  
		cpu_irq = 1;

	set_c0_status(0x100 << cpu_irq);
	irq_enable_hazard();
	evpe(vpflags);
}

static inline void mask_mips_mt_irq(unsigned int irq)
{
	unsigned int vpflags = dvpe();
	int cpu_irq = 0;

	if ((irq == SI_SWINT1_INT1) || (irq == SI_SWINT_INT1))  
		cpu_irq = 1;

	clear_c0_status(0x100 << cpu_irq);
	irq_disable_hazard();
	evpe(vpflags);
}

static unsigned int mips_mt_cpu_irq_startup(unsigned int irq)
{
	unsigned int vpflags = dvpe();
	int cpu_irq = 0;

	if ((irq == SI_SWINT1_INT1) || (irq == SI_SWINT_INT1))  
		cpu_irq = 1;

	VPint(CR_INTC_IMR) |=  (1 << (irq-1));
	if (irq == SI_SWINT_INT0)
		VPint(CR_INTC_IMR) |=  (1 << (SI_SWINT1_INT0-1));
	else if (irq == SI_SWINT_INT1)
		VPint(CR_INTC_IMR) |=  (1 << (SI_SWINT1_INT1-1));

	clear_c0_cause(0x100 << cpu_irq);
	evpe(vpflags);
	unmask_mips_mt_irq(irq);

	return 0;
}

/*
 * While we ack the interrupt interrupts are disabled and thus we don't need
 * to deal with concurrency issues.  Same for mips_cpu_irq_end.
 */
static void mips_mt_cpu_irq_ack(unsigned int irq)
{
	unsigned int vpflags = dvpe();
	int cpu_irq = 0;

	if ((irq == SI_SWINT1_INT1) || (irq == SI_SWINT_INT1))  
		cpu_irq = 1;

	clear_c0_cause(0x100 << cpu_irq);
	evpe(vpflags);
	mask_mips_mt_irq(irq);
}

static struct irq_chip mips_mt_cpu_irq_controller = {
	.name		= "MIPS",
	.startup	= mips_mt_cpu_irq_startup,
	.ack		= mips_mt_cpu_irq_ack,
	.mask		= mask_mips_mt_irq,
	.mask_ack	= mips_mt_cpu_irq_ack,
	.unmask		= unmask_mips_mt_irq,
	.eoi		= unmask_mips_mt_irq,
};

#define __BUILD_IRQ_DISPATCH(irq_n) \
static void __tc3262_irq_dispatch##irq_n(void) \
{								\
	do_IRQ(irq_n);				\
}	

#define __BUILD_IRQ_DISPATCH_FUNC(irq_n)  __tc3262_irq_dispatch##irq_n 

/* pre-built 64 irq dispatch function */
__BUILD_IRQ_DISPATCH(0)
__BUILD_IRQ_DISPATCH(1)
__BUILD_IRQ_DISPATCH(2)
__BUILD_IRQ_DISPATCH(3)
__BUILD_IRQ_DISPATCH(4)
__BUILD_IRQ_DISPATCH(5)
__BUILD_IRQ_DISPATCH(6)
__BUILD_IRQ_DISPATCH(7)
__BUILD_IRQ_DISPATCH(8)
__BUILD_IRQ_DISPATCH(9)
__BUILD_IRQ_DISPATCH(10)
__BUILD_IRQ_DISPATCH(11)
__BUILD_IRQ_DISPATCH(12)
__BUILD_IRQ_DISPATCH(13)
__BUILD_IRQ_DISPATCH(14)
__BUILD_IRQ_DISPATCH(15)
__BUILD_IRQ_DISPATCH(16)
__BUILD_IRQ_DISPATCH(17)
__BUILD_IRQ_DISPATCH(18)
__BUILD_IRQ_DISPATCH(19)
__BUILD_IRQ_DISPATCH(20)
__BUILD_IRQ_DISPATCH(21)
__BUILD_IRQ_DISPATCH(22)
__BUILD_IRQ_DISPATCH(23)
__BUILD_IRQ_DISPATCH(24)
__BUILD_IRQ_DISPATCH(25)
__BUILD_IRQ_DISPATCH(26)
__BUILD_IRQ_DISPATCH(27)
__BUILD_IRQ_DISPATCH(28)
__BUILD_IRQ_DISPATCH(29)
__BUILD_IRQ_DISPATCH(30)
__BUILD_IRQ_DISPATCH(31)
__BUILD_IRQ_DISPATCH(32)
__BUILD_IRQ_DISPATCH(33)
__BUILD_IRQ_DISPATCH(34)
__BUILD_IRQ_DISPATCH(35)
__BUILD_IRQ_DISPATCH(36)
__BUILD_IRQ_DISPATCH(37)
__BUILD_IRQ_DISPATCH(38)
__BUILD_IRQ_DISPATCH(39)
__BUILD_IRQ_DISPATCH(40)
__BUILD_IRQ_DISPATCH(41)
__BUILD_IRQ_DISPATCH(42)
__BUILD_IRQ_DISPATCH(43)
__BUILD_IRQ_DISPATCH(44)
__BUILD_IRQ_DISPATCH(45)
__BUILD_IRQ_DISPATCH(46)
__BUILD_IRQ_DISPATCH(47)
__BUILD_IRQ_DISPATCH(48)
__BUILD_IRQ_DISPATCH(49)
__BUILD_IRQ_DISPATCH(50)
__BUILD_IRQ_DISPATCH(51)
__BUILD_IRQ_DISPATCH(52)
__BUILD_IRQ_DISPATCH(53)
__BUILD_IRQ_DISPATCH(54)
__BUILD_IRQ_DISPATCH(55)
__BUILD_IRQ_DISPATCH(56)
__BUILD_IRQ_DISPATCH(57)
__BUILD_IRQ_DISPATCH(58)
__BUILD_IRQ_DISPATCH(59)
__BUILD_IRQ_DISPATCH(60)
__BUILD_IRQ_DISPATCH(61)
__BUILD_IRQ_DISPATCH(62)
__BUILD_IRQ_DISPATCH(63)

/* register pre-built 64 irq dispatch function */
static void (*irq_dispatch_tab[])(void) =
{
__BUILD_IRQ_DISPATCH_FUNC(0),
__BUILD_IRQ_DISPATCH_FUNC(1),
__BUILD_IRQ_DISPATCH_FUNC(2),
__BUILD_IRQ_DISPATCH_FUNC(3),
__BUILD_IRQ_DISPATCH_FUNC(4),
__BUILD_IRQ_DISPATCH_FUNC(5),
__BUILD_IRQ_DISPATCH_FUNC(6),
__BUILD_IRQ_DISPATCH_FUNC(7),
__BUILD_IRQ_DISPATCH_FUNC(8),
__BUILD_IRQ_DISPATCH_FUNC(9),
__BUILD_IRQ_DISPATCH_FUNC(10),
__BUILD_IRQ_DISPATCH_FUNC(11),
__BUILD_IRQ_DISPATCH_FUNC(12),
__BUILD_IRQ_DISPATCH_FUNC(13),
__BUILD_IRQ_DISPATCH_FUNC(14),
__BUILD_IRQ_DISPATCH_FUNC(15),
__BUILD_IRQ_DISPATCH_FUNC(16),
__BUILD_IRQ_DISPATCH_FUNC(17),
__BUILD_IRQ_DISPATCH_FUNC(18),
__BUILD_IRQ_DISPATCH_FUNC(19),
__BUILD_IRQ_DISPATCH_FUNC(20),
__BUILD_IRQ_DISPATCH_FUNC(21),
__BUILD_IRQ_DISPATCH_FUNC(22),
__BUILD_IRQ_DISPATCH_FUNC(23),
__BUILD_IRQ_DISPATCH_FUNC(24),
__BUILD_IRQ_DISPATCH_FUNC(25),
__BUILD_IRQ_DISPATCH_FUNC(26),
__BUILD_IRQ_DISPATCH_FUNC(27),
__BUILD_IRQ_DISPATCH_FUNC(28),
__BUILD_IRQ_DISPATCH_FUNC(29),
__BUILD_IRQ_DISPATCH_FUNC(30),
__BUILD_IRQ_DISPATCH_FUNC(31),
__BUILD_IRQ_DISPATCH_FUNC(32),
__BUILD_IRQ_DISPATCH_FUNC(33),
__BUILD_IRQ_DISPATCH_FUNC(34),
__BUILD_IRQ_DISPATCH_FUNC(35),
__BUILD_IRQ_DISPATCH_FUNC(36),
__BUILD_IRQ_DISPATCH_FUNC(37),
__BUILD_IRQ_DISPATCH_FUNC(38),
__BUILD_IRQ_DISPATCH_FUNC(39),
__BUILD_IRQ_DISPATCH_FUNC(40),
__BUILD_IRQ_DISPATCH_FUNC(41),
__BUILD_IRQ_DISPATCH_FUNC(42),
__BUILD_IRQ_DISPATCH_FUNC(43),
__BUILD_IRQ_DISPATCH_FUNC(44),
__BUILD_IRQ_DISPATCH_FUNC(45),
__BUILD_IRQ_DISPATCH_FUNC(46),
__BUILD_IRQ_DISPATCH_FUNC(47),
__BUILD_IRQ_DISPATCH_FUNC(48),
__BUILD_IRQ_DISPATCH_FUNC(49),
__BUILD_IRQ_DISPATCH_FUNC(50),
__BUILD_IRQ_DISPATCH_FUNC(51),
__BUILD_IRQ_DISPATCH_FUNC(52),
__BUILD_IRQ_DISPATCH_FUNC(53),
__BUILD_IRQ_DISPATCH_FUNC(54),
__BUILD_IRQ_DISPATCH_FUNC(55),
__BUILD_IRQ_DISPATCH_FUNC(56),
__BUILD_IRQ_DISPATCH_FUNC(57),
__BUILD_IRQ_DISPATCH_FUNC(58),
__BUILD_IRQ_DISPATCH_FUNC(59),
__BUILD_IRQ_DISPATCH_FUNC(60),
__BUILD_IRQ_DISPATCH_FUNC(61),
__BUILD_IRQ_DISPATCH_FUNC(62),
__BUILD_IRQ_DISPATCH_FUNC(63)
};

#endif

__IMEM static inline void unmask_mips_irq(unsigned int irq)
{
#ifdef CONFIG_MIPS_TC3262
	unsigned long flags;
	int cpu = smp_processor_id();

	spin_lock_irqsave(&tc3162_irq_lock, flags);
#ifdef CONFIG_MIPS_MT_SMTC
	if (cpu_data[cpu].vpe_id != 0) {
#else
	if (cpu != 0) {
#endif
		if (irq == SI_TIMER_INT)
			irq = SI_TIMER1_INT;
	}

	if (irq <= 32)
		VPint(CR_INTC_IMR) |=  (1 << (irq-1));
	else
		VPint(CR_INTC_IMR_1) |=  (1 << (irq-33));
	spin_unlock_irqrestore(&tc3162_irq_lock, flags);
#else
	VPint(CR_INTC_IMR) |=  (1 << irq);
#endif
}

__IMEM static inline void mask_mips_irq(unsigned int irq)
{
#ifdef CONFIG_MIPS_TC3262
	unsigned long flags;
	int cpu = smp_processor_id();

	spin_lock_irqsave(&tc3162_irq_lock, flags);
#ifdef CONFIG_MIPS_MT_SMTC
	if (cpu_data[cpu].vpe_id != 0) {
#else
	if (cpu != 0) {
#endif
		if (irq == SI_TIMER_INT)
			irq = SI_TIMER1_INT;
	}

	if (irq <= 32)
		VPint(CR_INTC_IMR) &= ~(1 << (irq-1));
	else
		VPint(CR_INTC_IMR_1) &= ~(1 << (irq-33));
	spin_unlock_irqrestore(&tc3162_irq_lock, flags);
#else
	VPint(CR_INTC_IMR) &= ~(1 << irq);
#endif
//2012-06-12, David Lin, [Merge from linux-2.6.21 of SDK3.6.0.0]
#ifdef CONFIG_IR_CONTROL
	//umedia-Jacky.Yang-7-Jun-2010, add Timer0 Interrupt and drop CONFIG_RALINK_TIMER. Timer1 can't work properly.
	if (irq == 1) {
		irq = SURFBOARDINT_TIMER0;
	}
#endif
}

void tc3162_enable_irq(unsigned int irq)
{
#ifdef CONFIG_MIPS_TC3262
	unsigned long flags;

	spin_lock_irqsave(&tc3162_irq_lock, flags);
	if (irq <= 32)
		VPint(CR_INTC_IMR) |=  (1 << (irq-1));
	else
		VPint(CR_INTC_IMR_1) |=  (1 << (irq-33));
	spin_unlock_irqrestore(&tc3162_irq_lock, flags);
#else
	VPint(CR_INTC_IMR) |=  (1 << irq);
#endif
}
EXPORT_SYMBOL(tc3162_enable_irq);

void tc3162_disable_irq(unsigned int irq)
{
#ifdef CONFIG_MIPS_TC3262
	unsigned long flags;

	spin_lock_irqsave(&tc3162_irq_lock, flags);
	if (irq <= 32)
		VPint(CR_INTC_IMR) &= ~(1 << (irq-1));
	else
		VPint(CR_INTC_IMR_1) &= ~(1 << (irq-33));
	spin_unlock_irqrestore(&tc3162_irq_lock, flags);
#else
	VPint(CR_INTC_IMR) &= ~(1 << (irq-1));
#endif
}
EXPORT_SYMBOL(tc3162_disable_irq);

static struct irq_chip tc3162_irq_chip = {
	.name		= "MIPS",
	.ack		= mask_mips_irq,
	.mask		= mask_mips_irq,
	.mask_ack	= mask_mips_irq,
	.unmask		= unmask_mips_irq,
	.eoi		= unmask_mips_irq,
#ifdef CONFIG_MIPS_MT_SMTC_IRQAFF
	.set_affinity	= plat_set_irq_affinity,
#endif /* CONFIG_MIPS_MT_SMTC_IRQAFF */
};

extern void vsmp_int_init(void);

void intPrioritySet(uint8 source, uint8 level)
{
    uint32 IPRn,IPLn;
    uint32 word;
    
    IPRn=level/4;
    IPLn=level%4;
    word = tc_inl(CR_INTC_IPR0+IPRn*4);
    word |= source << ((3-IPLn)*8);
    tc_outl(CR_INTC_IPR0+IPRn*4, word);
}
void set_irq_priority(void)
{
	unsigned int IntPriority[32]={
		// UART_INT, RTC_ALARM_INT, RTC_TICK_INT, RESERVED0, TIMER0_INT
		IPL8,	IPL6,	IPL22,	IPL21,	IPL30,		
		// TIMER1_INT, TIMER2_INT, TIMER3_INT, TIMER4_INT, TIMER5_INT
		IPL29,	IPL17,	IPL19,	IPL9,	IPL10,
		// GPIO_INT, RESERVED1, RESERVED2, RESERVED3, APB_DMA0_INT
		IPL11,	IPL12,	IPL13,	IPL14,	IPL18,
		// APB_DMA1_INT, RESERVED4, RESERVED5, DYINGGASP_INT, DMT_INT
		//IPL15,	IPL16,	IPL17,	IPL0,	IPL3,
		IPL15,	IPL16,	IPL7,	IPL20,	IPL3,
		// ARBITER_ERR_INT, MAC_INT, SAR_INT, USB_INT, RESERVED8
		IPL31,	IPL5,	IPL4,	IPL23,	IPL24, 
		// RESERVED9, XSLV0_INT, XSLV1_INT, XSLV2_INT, XAPB0_INT
		IPL25,	IPL26,	IPL27,	IPL28,	IPL2,
		// XAPB1_INT, SWR_INT
		//IPL1,	IPL20	
		IPL1,	IPL0	
	};

	int i;	

	tc_outl(CR_INTC_IPR0, 0);
	tc_outl(CR_INTC_IPR1, 0);
	tc_outl(CR_INTC_IPR2, 0);
	tc_outl(CR_INTC_IPR3, 0);
	tc_outl(CR_INTC_IPR4, 0);
	tc_outl(CR_INTC_IPR5, 0);
	tc_outl(CR_INTC_IPR6, 0);
	tc_outl(CR_INTC_IPR7, 0);

	for (i = 0; i < 32; i++)
		intPrioritySet(i, IntPriority[i]);
}

void __init arch_init_irq(void)
{
	unsigned int i;

	set_irq_priority();

	/* clear interrupt counter for VPE0 and VPE1 */
	if (isRT6855A)
		tc_outl(CR_INTC_ITR, (1 << 18) | (1 << 10));

	/* Disable all hardware interrupts */
	clear_c0_status(ST0_IM);
	clear_c0_cause(CAUSEF_IP);

	/* Initialize IRQ action handlers */
	for (i = 0; i < NR_IRQS; i++) {
#ifdef CONFIG_MIPS_TC3262
		/*
	 	 * Only MT is using the software interrupts currently, so we just
	 	 * leave them uninitialized for other processors.
	 	 */
		if (cpu_has_mipsmt) {
			if ((i == SI_SWINT1_INT0) || (i == SI_SWINT1_INT1) ||
				(i == SI_SWINT_INT0) || (i == SI_SWINT_INT1)) { 
				set_irq_chip(i, &mips_mt_cpu_irq_controller);
				continue;
			}
		}

		if ((i == SI_TIMER_INT) || (i == SI_TIMER1_INT))
			set_irq_chip_and_handler(i, &tc3162_irq_chip,
					 handle_percpu_irq);
		else
			set_irq_chip_and_handler(i, &tc3162_irq_chip,
					 handle_level_irq);
#else
		set_irq_chip_and_handler(i, &tc3162_irq_chip,
					 handle_level_irq);
#endif
	}

#ifdef CONFIG_MIPS_TC3262
	if (cpu_has_veic || cpu_has_vint) {
		write_c0_status((read_c0_status() & ~ST0_IM ) |
			                (STATUSF_IP0 | STATUSF_IP1)); 

		/* register irq dispatch functions */
		for (i = 0; i < NR_IRQS; i++)
			set_vi_handler(i, irq_dispatch_tab[i]);
	} else {
		change_c0_status(ST0_IM, ALLINTS);
	}
#else
	/* Enable all interrupts */
	change_c0_status(ST0_IM, ALLINTS);
#endif
#ifdef CONFIG_MIPS_MT_SMP
	vsmp_int_init();
#endif
}

__IMEM asmlinkage void plat_irq_dispatch(void)
{
#ifdef CONFIG_MIPS_TC3262
	int irq = ((read_c0_cause() & ST0_IM) >> 10);

	do_IRQ(irq);
#else
	do_IRQ(VPint(CR_INTC_IVR));
#endif
}

