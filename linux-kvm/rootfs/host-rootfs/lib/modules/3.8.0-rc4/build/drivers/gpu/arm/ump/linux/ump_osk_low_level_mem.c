/*
 * Copyright (C) 2010-2012 ARM Limited. All rights reserved.
 * 
 * This program is free software and is provided to you under the terms of the GNU General Public License version 2
 * as published by the Free Software Foundation, and any use by you of this program is subject to the terms of such GNU licence.
 * 
 * A copy of the licence is included with the program, and can also be obtained from Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

/**
 * @file ump_osk_memory.c
 * Implementation of the OS abstraction layer for the kernel device driver
 */

/* needed to detect kernel version specific code */
#include <linux/version.h>

#include "ump_osk.h"
#include "ump_uk_types.h"
#include "ump_ukk.h"
#include "ump_kernel_common.h"
#include <linux/module.h>            /* kernel module definitions */
#include <linux/kernel.h>
#include <linux/mm.h>
#include <linux/slab.h>

#include <asm/memory.h>
#include <asm/uaccess.h>			/* to verify pointers from user space */
#include <asm/cacheflush.h>
#include <linux/dma-mapping.h>

typedef struct ump_vma_usage_tracker
{
	atomic_t references;
	ump_memory_allocation *descriptor;
} ump_vma_usage_tracker;

static void ump_vma_open(struct vm_area_struct * vma);
static void ump_vma_close(struct vm_area_struct * vma);
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,26)
static int ump_cpu_page_fault_handler(struct vm_area_struct *vma, struct vm_fault *vmf);
#else
static unsigned long ump_cpu_page_fault_handler(struct vm_area_struct * vma, unsigned long address);
#endif

static struct vm_operations_struct ump_vm_ops =
{
	.open = ump_vma_open,
	.close = ump_vma_close,
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,26)
	.fault = ump_cpu_page_fault_handler
#else
	.nopfn = ump_cpu_page_fault_handler
#endif
};

/*
 * Page fault for VMA region
 * This should never happen since we always map in the entire virtual memory range.
 */
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,26)
static int ump_cpu_page_fault_handler(struct vm_area_struct *vma, struct vm_fault *vmf)
#else
static unsigned long ump_cpu_page_fault_handler(struct vm_area_struct * vma, unsigned long address)
#endif
{
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,26)
	void __user * address;
	address = vmf->virtual_address;
#endif
	MSG_ERR(("Page-fault in UMP memory region caused by the CPU\n"));
	MSG_ERR(("VMA: 0x%08lx, virtual address: 0x%08lx\n", (unsigned long)vma, address));

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,26)
	return VM_FAULT_SIGBUS;
#else
	return NOPFN_SIGBUS;
#endif
}

static void ump_vma_open(struct vm_area_struct * vma)
{
	ump_vma_usage_tracker * vma_usage_tracker;
	int new_val;

	vma_usage_tracker = (ump_vma_usage_tracker*)vma->vm_private_data;
	BUG_ON(NULL == vma_usage_tracker);

	new_val = atomic_inc_return(&vma_usage_tracker->references);

	DBG_MSG(4, ("VMA open, VMA reference count incremented. VMA: 0x%08lx, reference count: %d\n", (unsigned long)vma, new_val));
}

static void ump_vma_close(struct vm_area_struct * vma)
{
	ump_vma_usage_tracker * vma_usage_tracker;
	_ump_uk_unmap_mem_s args;
	int new_val;

	vma_usage_tracker = (ump_vma_usage_tracker*)vma->vm_private_data;
	BUG_ON(NULL == vma_usage_tracker);

	new_val = atomic_dec_return(&vma_usage_tracker->references);

	DBG_MSG(4, ("VMA close, VMA reference count decremented. VMA: 0x%08lx, reference count: %d\n", (unsigned long)vma, new_val));

	if (0 == new_val)
	{
		ump_memory_allocation * descriptor;

		descriptor = vma_usage_tracker->descriptor;

		args.ctx = descriptor->ump_session;
		args.cookie = descriptor->cookie;
		args.mapping = descriptor->mapping;
		args.size = descriptor->size;

		args._ukk_private = NULL; /** @note unused */

		DBG_MSG(4, ("No more VMA references left, releasing UMP memory\n"));
		_ump_ukk_unmap_mem( & args );

		/* vma_usage_tracker is free()d by _ump_osk_mem_mapregion_term() */
	}
}

_mali_osk_errcode_t _ump_osk_mem_mapregion_init( ump_memory_allocation * descriptor )
{
	ump_vma_usage_tracker * vma_usage_tracker;
	struct vm_area_struct *vma;

	if (NULL == descriptor) return _MALI_OSK_ERR_FAULT;

	vma_usage_tracker = kmalloc(sizeof(ump_vma_usage_tracker), GFP_KERNEL);
	if (NULL == vma_usage_tracker)
	{
		DBG_MSG(1, ("Failed to allocate memory for ump_vma_usage_tracker in _mali_osk_mem_mapregion_init\n"));
		return -_MALI_OSK_ERR_FAULT;
	}

	vma = (struct vm_area_struct*)descriptor->process_mapping_info;
	if (NULL == vma )
	{
		kfree(vma_usage_tracker);
		return _MALI_OSK_ERR_FAULT;
	}

	vma->vm_private_data = vma_usage_tracker;
	vma->vm_flags |= VM_IO;
	vma->vm_flags |= VM_DONTEXPAND | VM_DONTDUMP;

	if (0==descriptor->is_cached)
	{
		vma->vm_page_prot = pgprot_writecombine(vma->vm_page_prot);
	}
	DBG_MSG(3, ("Mapping with page_prot: 0x%x\n", vma->vm_page_prot ));

	/* Setup the functions which handle further VMA handling */
	vma->vm_ops = &ump_vm_ops;

	/* Do the va range allocation - in this case, it was done earlier, so we copy in that information */
	descriptor->mapping = (void __user*)vma->vm_start;

	atomic_set(&vma_usage_tracker->references, 1); /*this can later be increased if process is forked, see ump_vma_open() */
	vma_usage_tracker->descriptor = descriptor;

	return _MALI_OSK_ERR_OK;
}

void _ump_osk_mem_mapregion_term( ump_memory_allocation * descriptor )
{
	struct vm_area_struct* vma;
	ump_vma_usage_tracker * vma_usage_tracker;

	if (NULL == descriptor) return;

	/* Linux does the right thing as part of munmap to remove the mapping
	 * All that remains is that we remove the vma_usage_tracker setup in init() */
	vma = (struct vm_area_struct*)descriptor->process_mapping_info;

	vma_usage_tracker = vma->vm_private_data;

	/* We only get called if mem_mapregion_init succeeded */
	kfree(vma_usage_tracker);
	return;
}

_mali_osk_errcode_t _ump_osk_mem_mapregion_map( ump_memory_allocation * descriptor, u32 offset, u32 * phys_addr, unsigned long size )
{
	struct vm_area_struct *vma;
	_mali_osk_errcode_t retval;

	if (NULL == descriptor) return _MALI_OSK_ERR_FAULT;

	vma = (struct vm_area_struct*)descriptor->process_mapping_info;

	if (NULL == vma ) return _MALI_OSK_ERR_FAULT;

	retval = remap_pfn_range( vma, ((u32)descriptor->mapping) + offset, (*phys_addr) >> PAGE_SHIFT, size, vma->vm_page_prot) ? _MALI_OSK_ERR_FAULT : _MALI_OSK_ERR_OK;;

		DBG_MSG(4, ("Mapping virtual to physical memory. ID: %u, vma: 0x%08lx, virtual addr:0x%08lx, physical addr: 0x%08lx, size:%lu, prot:0x%x, vm_flags:0x%x RETVAL: 0x%x\n",
		        ump_dd_secure_id_get(descriptor->handle),
		        (unsigned long)vma,
		        (unsigned long)(vma->vm_start + offset),
		        (unsigned long)*phys_addr,
		        size,
		        (unsigned int)vma->vm_page_prot, vma->vm_flags, retval));

	return retval;
}


void _ump_osk_msync( ump_dd_mem * mem, void * virt, u32 offset, u32 size, ump_uk_msync_op op )
{
	int i;
	const void *start_v, *end_v;

	DBG_MSG(3, ("Flushing nr of blocks: %u, size: %u. First: paddr: 0x%08x vaddr: 0x%08x offset: 0x%08x size:%uB\n",
	            mem->nr_blocks, mem->size_bytes, mem->block_array[0].addr, virt, offset, size));

	/* Flush L1 using virtual address, the entire range in one go.
	 * Only flush if user space process has a valid write mapping on given address. */
	if(access_ok(VERIFY_WRITE, virt, size))
	{
		start_v = (void *)virt;
		end_v   = (void *)(start_v + size - 1);
		/*  There is no dmac_clean_range, so the L1 is always flushed,
		 *  also for UMP_MSYNC_CLEAN. */
		dmac_flush_range(start_v, end_v);
	}
	else
	{
		DBG_MSG(1, ("Attempt to flush %d@%x as part of ID %u rejected: there is no valid mapping.\n",
		            size, virt, mem->secure_id));
		return;
	}

	/* Flush L2 using physical addresses, block for block. */
	for (i=0 ; i < mem->nr_blocks; i++)
	{
		u32 start_p, end_p;
		ump_dd_physical_block *block;
		block = &mem->block_array[i];

		if(offset >= block->size)
		{
			offset -= block->size;
			continue;
		}

		if(offset)
		{
			start_p = (u32)block->addr + offset;
			/* We'll zero the offset later, after using it to calculate end_p. */
		}
		else
		{
			start_p = (u32)block->addr;
		}

		if(size < block->size - offset)
		{
			end_p = start_p + size - 1;
			size = 0;
		}
		else
		{
			if(offset)
			{
				end_p = start_p + (block->size - offset - 1);
				size -= block->size - offset;
				offset = 0;
			}
			else
			{
				end_p = start_p + block->size - 1;
				size -= block->size;
			}
		}

		switch(op)
		{
				case _UMP_UK_MSYNC_CLEAN:
						outer_clean_range(start_p, end_p);
						break;
				case _UMP_UK_MSYNC_CLEAN_AND_INVALIDATE:
						outer_flush_range(start_p, end_p);
						break;
				default:
						break;
		}

		if(0 == size)
		{
			/* Nothing left to flush. */
			break;
		}
	}

	return;
}
