#pragma once

#include <functional>
#include <algorithm>
#include <atomic>


namespace VM {

	/**
     * \brief An implementation of a thread safe shared pointer
     *
     * #include "shared_ptr.h"
	 * int main() {
     * util::SharedPtr<int> sp(new int(10));
     *
     * ...
     *
     * return 0;
     *
     */
    namespace Impl {

        class DefaultDeleter {
        public:
            template <typename T>
            void operator()(T* ptr) {
                delete ptr;
            }
        };

        class DestructorDeleter {
        public:
            template <typename T>
            void operator()(T* ptr) {
                ptr->~T();
            }
        };

        template <typename T>
        class SharedPtrImpl {
        public:
            template <typename D = DefaultDeleter>
            SharedPtrImpl(T* ptr = nullptr, D deleter = D()) : m_ptr(ptr), m_deleter(deleter), m_counter(1) { }

            template <typename D = DestructorDeleter, typename... Args>
            SharedPtrImpl(T* ptr, D deleter, Args&&... args) : m_ptr(ptr), m_deleter(deleter), m_counter(1) {
                new (m_ptr) T(std::forward<Args>(args)...);
            }

            ~SharedPtrImpl() {
                m_deleter(m_ptr);
            }

            T* operator->() const {
                return m_ptr;
            }

            T& operator*() const {
                return *m_ptr;
            }

            T* get() const {
                return m_ptr;
            }

            void hold() {
                ++m_counter;
            }

            bool release() {
                --m_counter;
                return m_counter == 0;
            }

        private:
            T* m_ptr = nullptr;
            std::function<void(T*)> m_deleter;
            std::atomic<unsigned int>  m_counter;
        };

    } // namespace impl

    template <typename T>
    class SharedPtr {
    public:
        explicit SharedPtr(Impl::SharedPtrImpl<T>* impl) : m_impl(impl) { }

        template <typename D = Impl::DefaultDeleter>
        explicit SharedPtr(T* ptr = nullptr, D deleter = D()) : m_impl(new Impl::SharedPtrImpl<T>(ptr, deleter)) { }

        template <typename U, typename D = Impl::DefaultDeleter>
        explicit SharedPtr(U* ptr = nullptr, D deleter = D()) : m_impl(new Impl::SharedPtrImpl<T>(ptr, deleter)) { }

        ~SharedPtr() {
            if (m_impl->release()) {
                delete m_impl;
            }
        }

        SharedPtr(const SharedPtr<T>& other) {
            m_impl = other.m_impl;
            m_impl->hold();
        }

        template <typename U>
        SharedPtr(const SharedPtr<U>& other) {
            m_impl = other.m_impl;
            m_impl->hold();
        }

        SharedPtr<T>& operator=(const SharedPtr<T>& other) {
            SharedPtr<T> tmp(other);
            swap(tmp);
            return *this;
        }

        template <typename U>
        SharedPtr<T>& operator=(const SharedPtr<U>& other) {
            SharedPtr<T> tmp(other);
            swap(tmp);
            return *this;
        }

        T* operator->() const {
            return m_impl->operator->();
        }

        T& operator*() const {
            return **m_impl;
        }

        template <typename U>
        bool operator<(const SharedPtr<U>& other) {
            return m_impl->get() < other.get();
        }

        T* get() const {
            return m_impl->get();
        }

        template <typename D = Impl::DefaultDeleter>
        void reset(T* ptr = nullptr, D deleter = D()) {
            if (m_impl->release()) {
                delete m_impl;
            }

            m_impl = new Impl::SharedPtrImpl<T>(ptr, deleter);
        }

        void swap(SharedPtr<T>& other) {
            std::swap(m_impl, other.m_impl);
        }

    private:
        Impl::SharedPtrImpl<T>* m_impl;
    };

    template <typename T>
    void swap(SharedPtr<T>& left, SharedPtr<T>& right) {
        left.swap(right);
    }

    /**
     * \brief Construct shared object 
     * \tparam T - Object's type
     * \tparam Args - Constructor arguments
     * \param args - Constructor arguments
     * \return The shared pointer
     * \usage make_shared<int>(1)
     */
    template <typename T, typename... Args>
    SharedPtr<T> make_shared(Args&&... args) {
        char* ptr = static_cast<char*>(operator new(sizeof(T) * sizeof(Impl::SharedPtrImpl<T>)));
        if (ptr == nullptr) {
            throw std::bad_alloc();
        }

        auto t_ptr = reinterpret_cast<T*>(ptr + sizeof(Impl::SharedPtrImpl<T>));

        try {
            auto impl = new(ptr) Impl::SharedPtrImpl<T>(t_ptr, Impl::DestructorDeleter(), std::forward<T>(args)...);
            return SharedPtr<T>(impl);
        }
        catch (...) {
            operator delete(ptr);
            throw;
        }
    }

} // namespace util
