#pragma once

#include <btl/tsan.h>

#include <atomic>
#include <thread>
#include <mutex>

namespace btl
{
    namespace detail
    {
        class spin_lock_bool
        {
        public:
            spin_lock_bool() :
                lock_(false)
            {
                TSAN_ANNOTATE_RWLOCK_CREATE(&lock_);
            }

            spin_lock_bool(spin_lock_bool const&) = delete;
            spin_lock_bool(spin_lock_bool&&) = delete;

            ~spin_lock_bool()
            {
                TSAN_ANNOTATE_RWLOCK_DESTROY(&lock_);
            }

            spin_lock_bool& operator=(spin_lock_bool const&) = delete;
            spin_lock_bool& operator=(spin_lock_bool&&) = delete;

            void lock()
            {
                bool expect = false;
                while (!lock_.compare_exchange_weak(expect, true))
                {
                    expect = false;
                    std::this_thread::yield();
                }

                TSAN_ANNOTATE_RWLOCK_ACQUIRED(&lock_, true);
            }

            bool try_lock()
            {
                bool expect = false;
                bool succeeded = lock_.compare_exchange_weak(expect, true);
                if (succeeded)
                {
                    TSAN_ANNOTATE_RWLOCK_ACQUIRED(&lock_, true);
                }

                return succeeded;
            }

            void unlock()
            {
                TSAN_ANNOTATE_RWLOCK_RELEASED(&lock_, true);
                lock_ = false;
            }

        private:
            std::atomic<bool> lock_;
        };

        class spin_lock_flagged
        {
        public:
            spin_lock_flagged()
            {
                TSAN_ANNOTATE_RWLOCK_CREATE(&lock_);
            }

            spin_lock_flagged(spin_lock_flagged const&) = delete;
            spin_lock_flagged(spin_lock_flagged&&) = delete;

            ~spin_lock_flagged()
            {
                TSAN_ANNOTATE_RWLOCK_DESTROY(&lock_);
            }

            spin_lock_flagged& operator=(spin_lock_flagged const&) = delete;
            spin_lock_flagged& operator=(spin_lock_flagged&&) = delete;

            void lock()
            {
                while (lock_.test_and_set(std::memory_order_acquire))
                    ;

                TSAN_ANNOTATE_RWLOCK_ACQUIRED(&lock_, true);
            }

            bool try_lock()
            {
                bool succeeded = !lock_.test_and_set(std::memory_order_acquire);
                if (succeeded)
                {
                    TSAN_ANNOTATE_RWLOCK_ACQUIRED(&lock_, true);
                }

                return succeeded;
            }

            void unlock()
            {
                TSAN_ANNOTATE_RWLOCK_RELEASED(&lock_, true);
                lock_.clear(std::memory_order_release);
            }

        private:
            std::atomic_flag lock_ = ATOMIC_FLAG_INIT;
        };
    }

    using SpinLock = detail::spin_lock_flagged;

    class RecursiveSpinLock
    {
    public:
        RecursiveSpinLock() :
            lock_(std::thread::id())
        {
            TSAN_ANNOTATE_RWLOCK_CREATE(&lock_);
        }

        RecursiveSpinLock(RecursiveSpinLock const&) = delete;
        RecursiveSpinLock(RecursiveSpinLock&&) = delete;

        ~RecursiveSpinLock()
        {
            TSAN_ANNOTATE_RWLOCK_DESTROY(&lock_);
        }

        RecursiveSpinLock& operator=(RecursiveSpinLock const&) = delete;
        RecursiveSpinLock& operator=(RecursiveSpinLock&&) = delete;

        void lock()
        {
            auto thisThread = std::this_thread::get_id();
            auto noId = std::thread::id();

            if (thisThread != noId ||
                    !lock_.compare_exchange_weak(noId, thisThread))
            {
                if (std::thread::id() != noId)
                    noId = std::thread::id();
                std::this_thread::yield();
            }

            TSAN_ANNOTATE_RWLOCK_ACQUIRED(&lock_, true);
        }

        bool try_lock()
        {
            auto thisThread = std::this_thread::get_id();
            auto noId = std::thread::id();

            if(!lock_.compare_exchange_weak(noId, thisThread))
            {
                if (std::thread::id() != noId)
                    return false;
                else if (!lock_.compare_exchange_weak(noId, thisThread))
                    return false;
            }

            TSAN_ANNOTATE_RWLOCK_ACQUIRED(&lock_, true);
            return true;
        }

        void unlock()
        {
            TSAN_ANNOTATE_RWLOCK_RELEASED(&lock_, true);
            lock_ = std::thread::id();
        }

    private:
        std::atomic<std::thread::id> lock_;
    };
}

