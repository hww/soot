#pragma once

/*!
 * @file kmalloc.h
 * Memory allocator.
 * Simple two-sided bump allocator.
 * DONE
 */

#include "platform.hpp"
#include "ptr.hpp"

/** The heaps size */
#define HEAPS_QUANTITY 2 /* Global and debug */
#define GLOBAL_HEAP_SIZE ((size_t)(4*1024*1024))
#define DEBUG_HEAP_SIZE ((size_t)(1*1024*1024))
#define TOTAL_HEAP_SIZE (GLOBAL_HEAP_SIZE+DEBUG_HEAP_SIZE+HEAPS_QUANTITY*sizeof(FHeapInfo))
/** The heap location */
#define GLOBAL_HEAP_INFO_ADDR (0)
#define GLOBAL_HEAP_ADDR (sizeof(FHeapInfo))
#define GLOBAL_HEAP_END (GLOBAL_HEAP_ADDR+GLOBAL_HEAP_SIZE)
/** The heap location */
#define DEBUG_HEAP_INFO_ADDR (GLOBAL_HEAP_END)
#define DEBUG_HEAP_ADDR (DEBUG_HEAP_INFO_ADDR+sizeof(FHeapInfo))


namespace vm
{
	/**
	 * \brief The block of memory managed by VM
	 */
	extern u8* g_ee_main_mem;

	/*!
	 * A kheap has a top_free/bottom linear allocator
	 */
	struct FHeapInfo
	{
		/** beginning of heap */
		Ptr<u8> bot_base;
		/** bot_free - the top position of bottom allocations */
		Ptr<u8> bot_free;
		/** bot_free - the bottom position of allocations */
		Ptr<u8> top_free;
		/** end of heap */
		Ptr<u8> top_base; 
	};

	// Kernel heaps
	extern Ptr<FHeapInfo> g_global_heap;
	extern Ptr<FHeapInfo> g_debug_heap;

	// flags for vm_malloc/vm_malloc
	enum EMallocFlags : u32
	{
		EMF_Top = 0x2000,
		//! Flag to allocate temporary memory from heap top_free
		EMF_MemSet = 0x1000,
		//! Flag to clear memory
		EMF_AlignMask = 0xfff,
		EMF_Align256 = 0x100,
		EMF_Align64 = 0x40,
		EMF_Align16 = 0x10
	};

	// malloc functions

	/**
	 * \brief Allocate a memory block
	 * \param heap - the heap 
	 * \param size - size of memory to allocate
	 * \param flags - additional flags for alignment, top/bottom allocation, set to zero
	 * \param name - the name of the block
	 * \return the pointer to the block
	 */
	Ptr<u8> vm_malloc(Ptr<FHeapInfo> heap, u32 size, EMallocFlags flags, char const* name);
	/**
	 * \brief Print the status of a heap.  This prints to stdout on the runtime,
	 *        which will not be sent to the Listener.
	 * \param heap - the pointer to heap
	 */
	void vm_print_heap_status(Ptr<FHeapInfo> heap);
	/**
	 * \brief Calculate the heap used space
	 * \param heap - the pointer to heap
	 * \return size of used space
	 */
	u32 vm_heap_used(Ptr<FHeapInfo> heap);

	/**
	 * \brief Release memory
	 * \param a the pointer to a block of memory
	 */
	void vm_free(Ptr<u8> a);
	/**
	 * \brief Initialize the heap
	 *        Initialize a FHeapInfo structure, and clear the heap's memory to 0
	 * \param heap - pointer to the heap structure
	 * \param mem - pointer to the memory block
	 * \param size - the memory block size
	 * \return - get the heap info pointer
	 */
	Ptr<FHeapInfo> vm_init_heap(Ptr<FHeapInfo> heap, Ptr<u8> mem, s32 size);
	/**
	 * \brief Initialize globals
	 */
	void vm_malloc_initialize();
	/**
	 * \brief Deinitialize globals
	 */
	void vm_malloc_deinitialize();
}
