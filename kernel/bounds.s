	.file	1 "bounds.c"
	.section .mdebug.abi32
	.previous

 # -G value = 0, Arch = mips32r2, ISA = 33
 # GNU C version 3.4.2 (mipsel-linux-uclibc)
 #	compiled by GNU C version 4.1.2 20061115 (prerelease) (Debian 4.1.1-21).
 # GGC heuristics: --param ggc-min-expand=98 --param ggc-min-heapsize=128354
 # options passed:  -nostdinc
 # -I/media/HDD01/ralink/GPL/WAP300N/test1/WAP300N_GPL/RT288x_SDK/source/linux-2.6.36MT.x/arch/mips/include
 # -Iinclude
 # -I/media/HDD01/ralink/GPL/WAP300N/test1/WAP300N_GPL/RT288x_SDK/source/linux-2.6.36MT.x/arch/mips/include/asm/tc3162/
 # -I/media/HDD01/ralink/GPL/WAP300N/test1/WAP300N_GPL/RT288x_SDK/source/linux-2.6.36MT.x/arch/mips/include/asm/mach-generic
 # -iprefix -U__PIC__ -U__pic__ -D__KERNEL__
 # -DVMLINUX_LOAD_ADDRESS=0xffffffff80020000 -DDATAOFFSET=0
 # -DKBUILD_STR(s)=#s -DKBUILD_BASENAME=KBUILD_STR(bounds)
 # -DKBUILD_MODNAME=KBUILD_STR(bounds) -isystem -include -MD
 # -mno-check-zero-division -mabi=32 -mno-abicalls -msoft-float
 # -march=mips32r2 -auxbase-strip -O2 -Wall -Wundef -Wstrict-prototypes
 # -Wno-trigraphs -Werror-implicit-function-declaration
 # -Wno-format-security -Wdeclaration-after-statement -fno-strict-aliasing
 # -fno-common -fno-delete-null-pointer-checks -ffunction-sections -fno-pic
 # -ffreestanding -fomit-frame-pointer -fverbose-asm
 # options enabled:  -feliminate-unused-debug-types -fdefer-pop
 # -fomit-frame-pointer -foptimize-sibling-calls -funit-at-a-time
 # -fcse-follow-jumps -fcse-skip-blocks -fexpensive-optimizations
 # -fthread-jumps -fstrength-reduce -fpeephole -fforce-mem -ffunction-cse
 # -fkeep-static-consts -fcaller-saves -fpcc-struct-return -fgcse -fgcse-lm
 # -fgcse-sm -fgcse-las -floop-optimize -fcrossjumping -fif-conversion
 # -fif-conversion2 -frerun-cse-after-loop -frerun-loop-opt
 # -fschedule-insns -fschedule-insns2 -fsched-interblock -fsched-spec
 # -fsched-stalled-insns -fsched-stalled-insns-dep -fbranch-count-reg
 # -freorder-blocks -freorder-functions -fcprop-registers
 # -ffunction-sections -fverbose-asm -fregmove -foptimize-register-move
 # -fargument-alias -fmerge-constants -fzero-initialized-in-bss -fident
 # -fpeephole2 -fguess-branch-probability -fmath-errno -ftrapping-math
 # -mgas -msoft-float -mno-check-zero-division -mexplicit-relocs
 # -march=mips32r2 -mabi=32 -mno-flush-func_flush_cache
 # -mflush-func=_flush_cache

	.section	.text.foo,"ax",@progbits
	.align	2
	.globl	foo
	.ent	foo
	.type	foo, @function
foo:
	.frame	$sp,0,$31		# vars= 0, regs= 0/0, args= 0, gp= 0
	.mask	0x00000000,0
	.fmask	0x00000000,0
#APP
	
->NR_PAGEFLAGS 23 __NR_PAGEFLAGS	 #
	
->MAX_NR_ZONES 2 __MAX_NR_ZONES	 #
#NO_APP
	j	$31
	.end	foo
	.ident	"GCC: (GNU) 3.4.2"
