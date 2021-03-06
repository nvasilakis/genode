/*
 * \brief  Generic allocator interface
 * \author Norman Feske
 * \date   2006-04-16
 */

/*
 * Copyright (C) 2006-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _INCLUDE__BASE__ALLOCATOR_H_
#define _INCLUDE__BASE__ALLOCATOR_H_

#include <base/stdint.h>
#include <base/exception.h>

namespace Genode {

	class Allocator
	{
		public:

			/*********************
			 ** Exception types **
			 *********************/

			class Out_of_memory : public Exception { };

			/**
			 * Destructor
			 */
			virtual ~Allocator() { }

			/**
			 * Allocate block
			 *
			 * \param size      block size to allocate
			 * \param out_addr  resulting pointer to the new block,
			 *                  undefined in the error case
			 * \return          true on success
			 */
			virtual bool alloc(size_t size, void **out_addr) = 0;

			/**
			 * Allocate typed block
			 *
			 * This template allocates a typed block returned as a pointer to
			 * a non-void type. By providing this function, we prevent the
			 * compiler from warning us about "dereferencing type-punned
			 * pointer will break strict-aliasing rules".
			 */
			template <typename T> bool alloc(size_t size, T **out_addr)
			{
				void *addr = 0;
				bool ret = alloc(size, &addr);
				*out_addr = (T *)addr;
				return ret;
			}

			/**
			 * Free block a previously allocated block
			 */
			virtual void free(void *addr, size_t size) = 0;

			/**
			 * Return total amount of backing store consumed by the allocator
			 */
			virtual size_t consumed() { return 0; }

			/**
			 * Return meta-data overhead per block
			 */
			virtual size_t overhead(size_t size) = 0;

			/**
			 * Return true if the size argument of 'free' is required
			 *
			 * The generic 'Allocator' interface requires the caller of 'free'
			 * to supply a valid size argument but not all implementations make
			 * use of this argument. If this function returns false, it is safe
			 * to call 'free' with an invalid size.
			 *
			 * Allocators that rely on the size argument must not be used for
			 * constructing objects whose constructors may throw exceptions.
			 * See the documentation of 'operator delete(void *, Allocator *)'
			 * below for more details.
			 */
			virtual bool need_size_for_free() const { return true; }


			/***************************
			 ** Convenience functions **
			 ***************************/

			/**
			 * Allocate block and signal error as an exception
			 *
			 * \param   size block size to allocate
			 * \return  pointer to the new block
			 * \throw   Out_of_memory
			 */
			void *alloc(size_t size)
			{
				void *result = 0;
				if (!alloc(size, &result))
					throw Out_of_memory();

				return result;
			}
	};


	class Range_allocator : public Allocator
	{
		public:

			/**
			 * Destructor
			 */
			virtual ~Range_allocator() { }

			/**
			 * Add free address range to allocator
			 */
			virtual int add_range(addr_t base, size_t size) = 0;

			/**
			 * Remove address range from allocator
			 */
			virtual int remove_range(addr_t base, size_t size) = 0;

			/**
			 * Return value of allocation functons
			 *
			 * 'OK'              on success, or
			 * 'OUT_OF_METADATA' if meta-data allocation failed, or
			 * 'RANGE_CONFLICT'  if no fitting address range is found
			 */
			struct Alloc_return
			{
				enum Value { OK = 0, OUT_OF_METADATA = -1, RANGE_CONFLICT = -2 };
				Value const value;
				Alloc_return(Value value) : value(value) { }

				bool is_ok()    const { return value == OK; }
				bool is_error() const { return !is_ok(); }
			};

			/**
			 * Allocate block
			 *
			 * \param size      size of new block
			 * \param out_addr  start address of new block,
			 *                  undefined in the error case
			 * \param align     alignment of new block specified
			 *                  as the power of two
			 */
			virtual Alloc_return alloc_aligned(size_t size, void **out_addr, int align = 0) = 0;

			/**
			 * Allocate block at address
			 *
			 * \param size   size of new block
			 * \param addr   desired address of block
			 *
			 * \return  'ALLOC_OK' on success, or
			 *          'OUT_OF_METADATA' if meta-data allocation failed, or
			 *          'RANGE_CONFLICT' if specified range is occupied
			 */
			virtual Alloc_return alloc_addr(size_t size, addr_t addr) = 0;

			/**
			 * Free a previously allocated block
			 *
			 * NOTE: We have to declare the 'Allocator::free(void *)' function
			 * here as well to make the compiler happy. Otherwise the C++
			 * overload resolution would not find 'Allocator::free(void *)'.
			 */
			virtual void free(void *addr) = 0;
			virtual void free(void *addr, size_t size) = 0;

			/**
			 * Return the sum of available memory
			 *
			 * Note that the returned value is not neccessarily allocatable
			 * because the memory may be fragmented.
			 */
			virtual size_t avail() = 0;

			/**
			 * Check if address is inside an allocated block
			 *
			 * \param addr  address to check
			 *
			 * \return      true if address is inside an allocated block, false
			 *              otherwise
			 */
			virtual bool valid_addr(addr_t addr) = 0;
	};


	/**
	 * Destroy object
	 *
	 * For destroying an object, we need to specify the allocator
	 * that was used by the object. Because we cannot pass the
	 * allocator directly to the delete operator, we mimic the
	 * delete operator using this template function.
	 *
	 * \param T      implicit object type
	 *
	 * \param alloc  allocator from which the object was allocated
	 * \param obj    object to destroy
	 */
	template <typename T>
	void destroy(Allocator *alloc, T *obj)
	{
		if (!obj)
			return;

		/* call destructors */
		obj->~T();

		/* free memory at the allocator */
		alloc->free(obj, sizeof(T));
	}
}

void *operator new    (Genode::size_t size, Genode::Allocator *allocator);
void *operator new [] (Genode::size_t size, Genode::Allocator *allocator);


/**
 * Delete operator invoked when an exception occurs during the construction of
 * a dynamically allocated object
 *
 * When an exception occurs during the construction of a dynamically allocated
 * object, the C++ standard devises the automatic invocation of the global
 * operator delete. When passing an allocator as argument to the new operator
 * (the typical case for Genode), the compiler magically calls the operator
 * delete taking the allocator type as second argument. This is how we end up
 * here.
 *
 * There is one problem though: We get the pointer of the to-be-deleted object
 * but not its size. But Genode's 'Allocator' interface requires the object
 * size to be passed as argument to 'Allocator::free()'.
 *
 * Even though in the general case, we cannot assume all 'Allocator'
 * implementations to remember the size of each allocated object, the commonly
 * used 'Heap', 'Sliced_heap', 'Allocator_avl', and 'Slab' do so and ignore the
 * size argument. When using either of those allocators, we are fine. Otherwise
 * we print a warning and pass the zero size argument anyway.
 *
 * :Warning: Never use an allocator that depends on the size argument of the
 *   'free()' function for the allocation of objects that may throw exceptions
 *   at their construction time!
 */
void operator delete (void *, Genode::Allocator *);

#endif /* _INCLUDE__BASE__ALLOCATOR_H_ */
