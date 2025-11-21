/*!
 * @file kmalloc.cpp
 * GOAL Kernel memory allocator.
 * Simple two-sided bump allocator.
 * DONE
 */

#include "vm_malloc.hpp"

#include <cstring>
#include <cstdio>
#include "printer.hpp"
#include "ptr.hpp"

namespace vm
{
	// The memory block
	u8* g_ee_main_mem;

	// global and debug heaps
	Ptr<FHeapInfo> g_global_heap;
	Ptr<FHeapInfo> g_debug_heap;

	void vm_malloc_initialize()
	{
		free(g_ee_main_mem);
		g_ee_main_mem = (u8*)malloc(TOTAL_HEAP_SIZE);
		g_global_heap.offset = GLOBAL_HEAP_INFO_ADDR;
		g_debug_heap.offset = DEBUG_HEAP_INFO_ADDR;
		const Ptr<u8> glob_mem(GLOBAL_HEAP_ADDR);
		vm_init_heap(g_global_heap, glob_mem, GLOBAL_HEAP_SIZE);
		const Ptr<u8> debug_mem(DEBUG_HEAP_ADDR);
		vm_init_heap(g_global_heap, debug_mem, DEBUG_HEAP_SIZE);
	}

	void vm_malloc_deinitialize()
	{
		free(g_ee_main_mem);
		g_global_heap.offset = 0;
		g_debug_heap.offset = 0;
	}

	Ptr<FHeapInfo> vm_init_heap(Ptr<FHeapInfo> heap, Ptr<u8> mem, s32 size)
	{
		heap->bot_base = mem;
		heap->bot_free = mem;
		heap->top_free = mem + size;
		heap->top_base = heap->top_free;
		std::memset(mem.c(), 0, size);
		return heap;
	}

	void vm_print_heap_status(Ptr<FHeapInfo> heap)
	{
		Msg(6,
		    "[%8x] vm-heap\n"
		    "\tbase: #x%x\n"
		    "\ttop-bot_base: #x%x\n"
		    "\tcur: #x%x\n"
		    "\ttop: #x%x\n",
		    heap.offset, heap->bot_base.offset, heap->top_base.offset, heap->bot_free.offset,
		    heap->top_free.offset);
		Msg(6,
		    "\t used bot: %d of %d bytes\n"
		    "\t used top_free: %d of %d bytes\n",
		    heap->bot_free - heap->bot_base, heap->top_base - heap->bot_base, heap->top_base - heap->top_free,
		    heap->top_base - heap->bot_base);

		if (heap == g_global_heap)
		{
			Msg(6, "\t %d bytes before stack\n", GLOBAL_HEAP_END - heap->bot_free.offset);
		}
	}

	u32 vm_heap_used(Ptr<FHeapInfo> heap)
	{
		return heap->bot_free - heap->bot_base;
	}

	Ptr<u8> vm_malloc(Ptr<FHeapInfo> heap, u32 size, EMallocFlags flags, char const* name)
	{
		int32_t alignment_flag = flags & EMF_AlignMask;

		// if we got a null heap, put it on the global heap, but warn about it
		if (!heap.offset)
		{
			Msg(6, "-----------> vm_malloc: alloc %s,  mem %s #x%x (a:%d  %d-bytes)\n", "DEBUG", name, -1,
			    alignment_flag, size);
			heap = g_global_heap;
		}

		uint32_t mem_start;

		if (!(flags & EMF_Top))
		{
			// allocate from bottom
			if (alignment_flag == EMF_Align64)
				mem_start = (0xffffffc0 & (heap->bot_free.offset + 0x40 - 1));
			else if (alignment_flag == EMF_Align256)
				mem_start = (0xffffff00 & (heap->bot_free.offset + 0x100 - 1));
			else // includes 0x10!
				mem_start = (0xfffffff0 & (heap->bot_free.offset + 0x10 - 1));

			if (size == 0)
			{
				Msg(6, "[WARNING] vm_malloc : size 0 allocation from bottom.\n");
				return Ptr<u8>(mem_start);
			}

			const uint32_t mem_end = mem_start + size;

			if (heap->top_free.offset < mem_end)
			{
				vm_print_heap_status(heap);
				Msg(6, "vm_malloc: !alloc mem %s (%d bytes) heap %x\n", name, size, heap.offset);
				return Ptr<u8>(0);
			}

			heap->bot_free.offset = mem_end;
			if (flags & EMF_MemSet)
				std::memset(Ptr<u8>(mem_start).c(), 0, (size_t)size);
			return Ptr<u8>(mem_start);
		}
		else
		{
			// allocate from top_free
			if (alignment_flag == 0)
			{
				alignment_flag = EMF_Align16;
			}

			mem_start = (heap->top_free.offset - size) & (-alignment_flag);

			if (size == 0)
			{
				Msg(6, "[WARNING] vm_malloc : size 0 allocation from top_free\n");
				return Ptr<u8>(mem_start);
			}

			if (heap->bot_free.offset >= mem_start)
			{
				Msg(6, "vm_malloc: !alloc mem from top_free %s (%d bytes) heap %x\n", name, size, heap.offset);
				vm_print_heap_status(heap);
				return Ptr<u8>(0);
			}

			heap->top_free.offset = mem_start;

			if (flags & EMF_MemSet)
				std::memset(Ptr<u8>(mem_start).c(), 0, (size_t)size);
			return Ptr<u8>(mem_start);
		}
	}

	void vm_free(Ptr<u8> a)
	{
		(void)a;
		Msg(6, "[ERROR] vm_malloc: vm_free called\n");
	}
}
