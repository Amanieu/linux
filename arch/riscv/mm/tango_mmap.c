#include <linux/mm.h>
#include <linux/mman.h>
#include <linux/random.h>
#include <linux/personality.h>
#include <linux/security.h>
#include <linux/hugetlb.h>
#include <linux/compat.h>

/* Definitions for Tango's guest mmap area */
#define TANGO_TASK_SIZE                 0x100000000ul
#define TANGO_MIN_GAP			(SZ_128M)
#define TANGO_STACK_TOP			0x80000000
#define TANGO_MAX_GAP			(TANGO_STACK_TOP/6*5)
#define TANGO_TASK_UNMAPPED_BASE	PAGE_ALIGN(TANGO_TASK_SIZE / 4)
#define TANGO_STACK_RND_MASK		(0x7ff >> (PAGE_SHIFT - 12))

#ifndef arch_get_mmap_end
#define arch_get_mmap_end(addr)	(TASK_SIZE)
#endif

#ifndef arch_get_mmap_base
#define arch_get_mmap_base(addr, base) (base)
#endif

static int mmap_is_legacy(unsigned long rlim_stack)
{
	if (current->personality & ADDR_COMPAT_LAYOUT)
		return 1;

	if (rlim_stack == RLIM_INFINITY)
		return 1;

	return sysctl_legacy_va_layout;
}

static unsigned long tango_mmap_base(unsigned long rnd, unsigned long gap)
{
	unsigned long pad = stack_guard_gap;

	/* Account for stack randomization if necessary */
	if (current->flags & PF_RANDOMIZE)
		pad += (TANGO_STACK_RND_MASK << PAGE_SHIFT);

	/* Values close to RLIM_INFINITY can overflow. */
	if (gap + pad > gap)
		gap += pad;

	if (gap < TANGO_MIN_GAP)
		gap = TANGO_MIN_GAP;
	else if (gap > TANGO_MAX_GAP)
		gap = TANGO_MAX_GAP;

	return PAGE_ALIGN(TANGO_STACK_TOP - gap - rnd);
}

void arch_setup_new_exec(void)
{
	unsigned long random_factor = 0UL;
	unsigned long rlim_stack = rlimit(RLIMIT_STACK);

	current_thread_info()->tango_syscall = 0;

	if (current->flags & PF_RANDOMIZE) {
		random_factor = (get_random_long() &
			((1UL << mmap_rnd_compat_bits) - 1)) << PAGE_SHIFT;
	}

	if (mmap_is_legacy(rlim_stack)) {
		current->mm->context.tango_mmap_base =
			TANGO_TASK_UNMAPPED_BASE + random_factor;
	} else {
		current->mm->context.tango_mmap_base =
			tango_mmap_base(random_factor, rlim_stack);
	}
}

/* Get an address range which is currently unmapped.
 * For shmat() with addr=0.
 *
 * Ugly calling convention alert:
 * Return value with the low bits set means error value,
 * ie
 *	if (ret & ~PAGE_MASK)
 *		error = ret;
 *
 * This function "knows" that -ENOMEM has the bits set.
 */
unsigned long
arch_get_unmapped_area(struct file *filp, unsigned long addr,
		unsigned long len, unsigned long pgoff, unsigned long flags)
{
	struct mm_struct *mm = current->mm;
	struct vm_area_struct *vma, *prev;
	struct vm_unmapped_area_info info;
	const unsigned long mmap_end = arch_get_mmap_end(addr);

	if (len > mmap_end - mmap_min_addr)
		return -ENOMEM;

	if (is_tango_compat_task() && addr + len > TANGO_TASK_SIZE) {
		if (flags & MAP_FIXED)
			return -ENOMEM;
		else
			addr = 0;
	}

	if (flags & MAP_FIXED)
		return addr;

	if (addr) {
		addr = PAGE_ALIGN(addr);
		vma = find_vma_prev(mm, addr, &prev);
		if (mmap_end - len >= addr && addr >= mmap_min_addr &&
		    (!vma || addr + len <= vm_start_gap(vma)) &&
		    (!prev || addr >= vm_end_gap(prev)))
			return addr;
	}

	info.flags = 0;
	info.length = len;
	if (is_tango_compat_task()) {
		info.low_limit = mm->context.tango_mmap_base;
		info.high_limit = TANGO_TASK_SIZE;
	} else {
		info.low_limit = mm->mmap_base;
		info.high_limit = mmap_end;
	}
	info.align_mask = 0;
	return vm_unmapped_area(&info);
}

/*
 * This mmap-allocator allocates new areas top-down from below the
 * stack's low limit (the base):
 */
unsigned long
arch_get_unmapped_area_topdown(struct file *filp, unsigned long addr,
			  unsigned long len, unsigned long pgoff,
			  unsigned long flags)
{
	struct vm_area_struct *vma, *prev;
	struct mm_struct *mm = current->mm;
	struct vm_unmapped_area_info info;
	const unsigned long mmap_end = arch_get_mmap_end(addr);

	/* requested length too big for entire address space */
	if (len > mmap_end - mmap_min_addr)
		return -ENOMEM;

	if (is_tango_compat_task() && addr + len > TANGO_TASK_SIZE) {
		if (flags & MAP_FIXED)
			return -ENOMEM;
		else
			addr = 0;
	}

	if (flags & MAP_FIXED)
		return addr;

	/* requesting a specific address */
	if (addr) {
		addr = PAGE_ALIGN(addr);
		vma = find_vma_prev(mm, addr, &prev);
		if (mmap_end - len >= addr && addr >= mmap_min_addr &&
				(!vma || addr + len <= vm_start_gap(vma)) &&
				(!prev || addr >= vm_end_gap(prev)))
			return addr;
	}

	info.flags = VM_UNMAPPED_AREA_TOPDOWN;
	info.length = len;
	info.low_limit = max(PAGE_SIZE, mmap_min_addr);
	if (is_tango_compat_task())
		info.high_limit = mm->context.tango_mmap_base;
	else
		info.high_limit = arch_get_mmap_base(addr, mm->mmap_base);
	info.align_mask = 0;
	addr = vm_unmapped_area(&info);

	/*
	 * A failed mmap() very likely causes application failure,
	 * so fall back to the bottom-up function here. This scenario
	 * can happen with large stack limits and large mmap()
	 * allocations.
	 */
	if (offset_in_page(addr)) {
		VM_BUG_ON(addr != -ENOMEM);
		info.flags = 0;
		if (is_tango_compat_task()) {
			info.low_limit = TANGO_TASK_UNMAPPED_BASE;
			info.high_limit = TANGO_TASK_SIZE;
		} else {
			info.low_limit = TASK_UNMAPPED_BASE;
			info.high_limit = mmap_end;
		}
		addr = vm_unmapped_area(&info);
	}

	return addr;
}

unsigned long
hugetlb_get_unmapped_area(struct file *file, unsigned long addr,
		unsigned long len, unsigned long pgoff, unsigned long flags)
{
	struct mm_struct *mm = current->mm;
	struct vm_area_struct *vma;
	struct hstate *h = hstate_file(file);
	struct vm_unmapped_area_info info;

	if (len & ~huge_page_mask(h))
		return -EINVAL;
	if (len > TASK_SIZE)
		return -ENOMEM;

	if (is_tango_compat_task() && addr + len > TANGO_TASK_SIZE) {
		if (flags & MAP_FIXED)
			return -ENOMEM;
		else
			addr = 0;
	}

	if (flags & MAP_FIXED) {
		if (prepare_hugepage_range(file, addr, len))
			return -EINVAL;
		return addr;
	}

	if (addr) {
		addr = ALIGN(addr, huge_page_size(h));
		vma = find_vma(mm, addr);
		if (TASK_SIZE - len >= addr &&
		    (!vma || addr + len <= vm_start_gap(vma)))
			return addr;
	}

	info.flags = 0;
	info.length = len;
	if (is_tango_compat_task()) {
		info.low_limit = TANGO_TASK_UNMAPPED_BASE;
		info.high_limit = TANGO_TASK_SIZE;
	} else {
		info.low_limit = TASK_UNMAPPED_BASE;
		info.high_limit = TASK_SIZE;
	}
	info.align_mask = PAGE_MASK & ~huge_page_mask(h);
	info.align_offset = 0;
	return vm_unmapped_area(&info);
}
