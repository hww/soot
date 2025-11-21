#pragma once
#include <cassert>

#include "platform.hpp"

namespace vm
{
	/**
	 * @brief Base class for all classes that support reference counting
	 * class MyClass : public RCBase
	 * {
	 *   public:
	 *      ~MyClass() {cout << this << " is no longer needed" << endl;}
	 *      	void print() {cout << "Hello" << endl;}
	 * };
	 * int main(void)
	 * {
	 *		// 1: Demonstrate RCBase class
	 *		// Module 1 creates an object
	 *		MyClass *a = new MyClass();
	 *		a->grab();		// RC=1
	 *
	 *		// Module 2 grabs object
	 *		MyClass* ptr = a;
	 *		ptr->grab();    // RC=2
	 *
	 *		// Module 1 no longer needs object
	 *		a->release();      		//RC=1
	 *		a = nullptr;
	 *
	 *		// Module 2 no longer needs object
	 *		ptr->release();    		//object will be destroyed here
	 *		ptr = nullptr;
	 * }
	 *
	 */
	class RCBase
	{
	public:
		RCBase() : mRefCount(0) {}
		virtual ~RCBase() = default;

		void grab() const { ++mRefCount; }

		void release() const
		{
			assert(mRefCount > 0);
			--mRefCount;

			if (mRefCount == 0) { delete const_cast<RCBase*>(this); }
		}

	private:
		mutable int mRefCount;
	};

	/**
     * @brief A reference counting-managed pointer for classes derived
     * from RCBase which can be used as C pointer
     *
     * @usage
     *
     * // Module 1 creates an object
	 * RCPtr< MyClass > a2 = new MyClass();	// RC=1
	 *	
	 * // Module 2 grabs object
	 * RCPtr< MyClass > ptr2 = a2;		// RC=2
	 *    
	 * // Module 2 invokes a method
	 * ptr2->print();
	 * (*ptr2).print();
	 *
	 * // Module 1 no longer needs object
	 * a2 = NULL;      			// RC=1
	 * 
	 * // Module 2 no longer needs object
	 * ptr2 = NULL;    			// object will be destroyed here
     *
     *
     */
	template < class T >
	class RCPtr
	{
	public:
		/** Construct using a c pointer e.g.RCPtr< T > x = new T(); */
		RCPtr(T* ptr = nullptr) : mPtr(ptr)
		{
			if (ptr != nullptr) { ptr->grab(); }
		}

		/** Copy constructor */
		RCPtr(const RCPtr& ptr)
			: mPtr(ptr.mPtr)
		{
			if (mPtr != nullptr) { mPtr->grab(); }
		}

		~RCPtr()
		{
			if (mPtr != nullptr) { mPtr->release(); }
		}

		/** Assign a pointer e.g.x = new T(); */
		RCPtr& operator=(T* ptr)
		{
			// The following grab and release operations have to be performed
			// in that order to handle the case where ptr == mPtr
			if (ptr != nullptr) { ptr->grab(); }
			if (mPtr != nullptr) { mPtr->release(); }
			mPtr = ptr;
			return (*this);
		}

		/** Assign another RCPtr */
		RCPtr& operator=(const RCPtr& ptr)
		{
			return (*this) = ptr.mPtr;
		}

		/** Retrieve actual pointer */
		T* get() const
		{
			return mPtr;
		}
		/**
		 * Some overloaded operators to facilitate dealing with an RCPtr
		 * as a conventional c pointer.
		 * Without these operators, one can still use the less transparent
		 * get() method to access the pointer.
		 */

		 /** x->member */
		T* operator->() const { return mPtr; }
		/** *x, (*x).member */
		T& operator*() const { return *mPtr; }
		/** T* y = x; */
		operator T* () const { return mPtr; }
		/** if(x) { .... x is not nullptr .... */
		operator bool() const { return mPtr != nullptr; }

		bool operator==(const RCPtr& ptr) { return mPtr == ptr.mPtr; }
		bool operator==(const T* ptr) { return mPtr == ptr; }

	private:
		T* mPtr;	//Actual pointer
	};


	/**
     * @brief A reference counting-managed pointer for classes derived
     * from RCBase which can be used as C pointer
     *
     * @usage
     *
     * // Module 1 creates an object
     * RCPtr< MyClass > a2 = new MyClass();	// RC=1
     *
     * // Module 2 grabs object
     * RCPtr< MyClass > ptr2 = a2;		// RC=2
     *
     * // Module 2 invokes a method
     * ptr2->print();
     * (*ptr2).print();
     *
     * // Module 1 no longer needs object
     * a2 = NULL;      			// RC=1
     *
     * // Module 2 no longer needs object
     * ptr2 = NULL;    			// object will be destroyed here
     *
     *
     */
	class RCRef
	{
		const PTRINT MASK = 0x00FFFFFFFFFFFFFF;
	public:
		/** Construct using a c pointer e.g.RCPtr< T > x = new T(); */
		RCRef(RCBase* ptr = nullptr) 
		{
			set(ptr);
			if (ptr != nullptr) { ptr->grab(); }
		}

		/** Copy constructor */
		RCRef(const RCRef& ptr)
		{
			mPtr = set(ptr.mPtr);
			if (ptr.mPtr != nullptr) { ptr.mPtr->grab(); }
		}

		~RCRef()
		{
			if (mPtr != nullptr) { get()->release(); }
		}



		/** Assign a pointer e.g.x = new T(); */
		RCRef& operator=(RCBase* ptr)
		{
			// The following grab and release operations have to be performed
			// in that order to handle the case where ptr == mPtr
			if (ptr != nullptr) { ptr->grab(); }
			if (get() != nullptr) { get()->release(); }
			mPtr = ptr;
			return (*this);
		}

		/** Assign another RCPtr */
		RCRef& operator=(const RCRef& ptr)
		{
			return (*this) = ptr.mPtr;
		}

		/** Retrieve actual pointer */
		RCBase* get() const
		{
			return reinterpret_cast<RCBase*>(reinterpret_cast<PTRINT>(mPtr) & MASK);
		}

		RCBase* set(RCBase* ptr)
		{
			mPtr = reinterpret_cast<RCBase*>((reinterpret_cast<PTRINT>(mPtr) & ~MASK) | (reinterpret_cast<PTRINT>(ptr) & MASK));
		}
		/**
		 * Some overloaded operators to facilitate dealing with an RCPtr
		 * as a conventional c pointer.
		 * Without these operators, one can still use the less transparent
		 * get() method to access the pointer.
		 */

		 /** x->member */
		RCBase* operator->() const { return get(); }
		/** *x, (*x).member */
		RCBase& operator*() const { return *get(); }
		/** T* y = x; */
		operator RCBase* () const { return get(); }
		/** if(x) { .... x is not nullptr .... */
		operator bool() const { return get() != nullptr; }


		bool operator==(const RCRef& other) const { return get() == other.get(); }
		bool operator==(const RCBase* other) const { return get() == other; }

	private:
		RCBase* mPtr;	// Actual pointer
	};

}
