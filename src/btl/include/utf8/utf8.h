#pragma once

#include <string>
#include <cstring>

namespace utf8
{
    class Iterator
    {
    public:
        inline Iterator(char const* ptr, char const* begin,
                char const* end) :
            ptr_(ptr),
            begin_(begin),
            end_(end)
        {
        }

        inline bool operator==(Iterator const& rhs) const
        {
            return ptr_ == rhs.ptr_;
        }

        inline bool operator!=(Iterator const& rhs) const
        {
            return ptr_ != rhs.ptr_;
        }

        inline Iterator& operator++()
        {
            if (ptr_ == end_)
                return *this;

            auto w = width();

            do
            {
                --w;
                ++ptr_;
            }
            while (ptr_ != end_ && w && (*ptr_ & 0xc0) == 0x80);

            return *this;
        }

        inline Iterator& operator--()
        {
            if (ptr_ == begin_)
                return *this;

            do
            {
                --ptr_;
            }
            while (ptr_ != begin_ && (*ptr_ & 0xc0) == 0x80);

            return *this;
        }

        inline Iterator operator++(int)
        {
            auto copy = *this;
            ++(*this);
            return copy;
        }

        inline Iterator operator--(int)
        {
            auto copy = *this;
            --(*this);
            return copy;
        }

        inline uint8_t width() const
        {
            uint8_t c = *ptr_;
            unsigned int bytes = 1;
            if ((c & 0x80) == 0)
                bytes = 1;
            else if ((c & 0xe0) == 0xc0)
                bytes = 2;
            else if ((c & 0xf0) == 0xe0)
                bytes = 3;
            else if ((c & 0xf8) == 0xf0)
                bytes = 4;
            else if ((c & 0xfc) == 0xf8)
                bytes = 5;
            else if ((c & 0xfe) == 0xfc)
                bytes = 6;

            if (ptr_ + bytes > end_)
                return (uint8_t)(end_ - ptr_);

            return (uint8_t)bytes;
        }

        inline uint32_t operator*()
        {
            uint8_t c = *ptr_;
            unsigned int w = width();

            switch (w)
            {
            case 1:
                return c;
            case 2:
               return ((uint32_t)(c & 0x1f) << 6)
                   + (uint32_t)(ptr_[1] & 0x3f);
            case 3:
               return ((uint32_t)(c & 0x0f) << 12)
                   + ((uint32_t)(ptr_[1] & 0x3f) << 6)
                   + (uint32_t)(ptr_[2] & 0x3f);
            case 4:
               return ((uint32_t)(c & 0x07) << 18)
                   + ((uint32_t)(ptr_[1] & 0x3f) << 12)
                   + ((uint32_t)(ptr_[2] & 0x3f) << 6)
                   + (uint32_t)(ptr_[3] & 0x3f);
            case 5:
               return ((uint32_t)(c & 0x03) << 24)
                   + ((uint32_t)(ptr_[1] & 0x3f) << 18)
                   + ((uint32_t)(ptr_[2] & 0x3f) << 12)
                   + ((uint32_t)(ptr_[3] & 0x3f) << 6)
                   + (uint32_t)(ptr_[4] & 0x3f);
            case 6:
               return ((uint32_t)(c & 0x01) << 30)
                   + ((uint32_t)(ptr_[1] & 0x3f) << 24)
                   + ((uint32_t)(ptr_[2] & 0x3f) << 18)
                   + ((uint32_t)(ptr_[3] & 0x3f) << 12)
                   + ((uint32_t)(ptr_[4] & 0x3f) << 6)
                   + (uint32_t)(ptr_[5] & 0x3f);
            default:
               return 0;
            }

            return 0;
        }

        char const* ptr() const
        {
            return ptr_;
        }

        size_t index() const
        {
            return ptr_ - begin_;
        }

    private:
        char const* ptr_;
        char const* begin_;
        char const* end_;
    };

    class Utf8View
    {
    public:
        inline Utf8View(char const* ptr, size_t size) :
            ptr_(ptr),
            size_(size)
        {
        }

        inline size_t size() const
        {
            return size_;
        }

        inline bool empty() const
        {
            return size_ == 0;
        }

        inline Iterator begin() const
        {
            return {ptr_, ptr_, ptr_ + size_};
        }

        inline Iterator end() const
        {
            return {ptr_ + size_, ptr_, ptr_ + size_};
        }

        inline std::string str() const
        {
            return {ptr_, size_};
        }

    private:
        /*Iterator begin_;
        Iterator end_;*/
        char const* ptr_;
        size_t size_;
    };

    inline Iterator floor(std::string const& str, size_t index)
    {
        if (index >= str.size())
            return Iterator{str.data() + str.size(), str.data(),
            str.data() + str.size()};

        while (index != 0
                && ((str[index] & 0x80) != 0)
                && ((str[index] & 0xe0) != 0xc0)
                && ((str[index] & 0xf0) != 0xe0)
                && ((str[index] & 0xf8) != 0xf0)
                && ((str[index] & 0xfc) != 0xf8)
                && ((str[index] & 0xfe) != 0xfc))
            --index;

        return Iterator{&str[index], str.data(), str.data() + str.size()};
    }

    inline Iterator ceil(std::string const& str, size_t index)
    {
        if (index > str.size())
            index = str.size();

        while (index < str.size()
                && ((str[index] & 0x80) != 0)
                && ((str[index] & 0xe0) != 0xc0)
                && ((str[index] & 0xf0) != 0xe0)
                && ((str[index] & 0xf8) != 0xf0)
                && ((str[index] & 0xfc) != 0xf8)
                && ((str[index] & 0xfe) != 0xfc))
            ++index;

        return Iterator{&str[index], str.data(), str.data() + str.size()};
    }

    inline Utf8View asUtf8(std::string const& str)
    {
        return {str.data(), str.size()};
    }

    inline Utf8View subStr(Iterator begin, Iterator end)
    {
        return {begin.ptr(), static_cast<size_t>(end.ptr() - begin.ptr())};
    }

    inline void erase(std::string& str, Iterator const& iter)
    {
        auto begin = str.begin() + iter.index();
        auto end = begin + iter.width();
        str.erase(begin, end);
    }

    inline std::pair<Utf8View, Utf8View> split(Utf8View str,
            Iterator iter)
    {
        return std::make_pair(
                subStr(str.begin(), iter),
                subStr(iter, str.end())
                );
    }

    inline std::string encode(uint32_t code)
    {
        int bytes = 0;
        if (code <= 0x7f)
            bytes = 1;
        else if (code <= 0x07ff)
            bytes = 2;
        else if (code <= 0xffff)
            bytes = 3;
        else if (code <= 0x1fffff)
            bytes = 4;
        else if (code <= 0x3ffffff)
            bytes = 5;
        else if (code <= 0x7fffffff)
            bytes = 6;
        else
            return "";

        std::string str;
        if (bytes == 1)
        {
            str = (char)(code & 0xff >> 1);
            return str;
        }

        str += (char)((code >> ((bytes-1)*6)) & 0xff >> (bytes+1))
            + (char)(0xff << (8 - bytes));
        for (int i = 1; i < bytes; ++i)
            str += (char)((code >> ((bytes-i-1)*6)) & 0x3f) + 0x80;

        return str;
    }
} // utf8

