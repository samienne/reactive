#ifndef BG_ATOMIC_H
#define BG_ATOMIC_H

namespace Bg
{
    template <class T>
    class Atomic
    {
    public:
        Atomic(T value = 0) :
            value_(value)
        {
        }

        Atomic(Atomic<T> const& other)
        {
            __sync_synchronize();
            T v1 = other.value_;
            __sync_synchronize();
            value_ = v1;
        }

        ~Atomic()
        {
        }

        T operator+=(T value)
        {
            return __sync_add_and_fetch(&value_, value);
        }

        T operator-=(T value)
        {
            return __sync_sub_and_fetch(&value_, value);
        }

        T operator+(T value) const
        {
            __sync_synchronize();
            T v1 = value.value_;
            T v2 = value_;
            return v1 + v2;
        }

        T operator-(T value) const
        {
            __sync_synchronize();
            T v1 = value.value_;
            T v2 = value_;
            return v2 - v1;
        }

        T operator=(T value)
        {
            value_ = value;
            __sync_synchronize();
            return value;
        }

        bool operator==(T value)
        {
            __sync_synchronize();
            T v1 = value;
            return value == v1;
        }

        bool operator!=(T value)
        {
            __sync_synchronize();
            T v1 = value;
            return value != v1;
        }

        operator T() const
        {
            __sync_synchronize();
            T v1 = value_;
            __sync_synchronize();

            return v1;
        }

        T operator->() const
        {
            return value_;
        }

    private:
        volatile T value_;
    };
}

#endif // BG_ATOMIC_H

