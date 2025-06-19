#pragma once

#include "element.h"
#include "streambase.h"
#include "reactive/streamtraits.h"

#include <memory>

namespace bq
{
    namespace stream
    {

    template <typename T>
    class InputRange
    {
    public:
        using Element = stream::Element<T>;

        class Iterator
        {
        public:
            using Element = stream::Element<T>;

            Iterator(std::shared_ptr<Element> const& element) :
                element_(element)
            {
            }

            Iterator operator++()
            {
                if (!element_)
                    return *this;

                element_ = element_->getNext();
                return *this;
            }

            Iterator operator++(int)
            {
                if (!element_)
                    return *this;

                Iterator old = *this;
                element_ = element_->getNext();

                return old;
            }

            auto operator*() const
                -> decltype(std::declval<Element>().getValue())
            {
                return element_->getValue();
            }

            bool operator==(Iterator const& rhs) const
            {
                return element_ == rhs.element_;
            }

            bool operator!=(Iterator const& rhs) const
            {
                return element_ != rhs.element_;
            }

        private:
            std::shared_ptr<Element> element_;
        };

        using const_iterator = Iterator;

        InputRange(std::shared_ptr<Element> const& begin,
                std::shared_ptr<Element> const& end) :
            begin_(begin),
            end_(end)
        {
        }

        Iterator begin()
        {
            return Iterator(begin_);
        }

        Iterator end()
        {
            return Iterator(end_);
        }

    private:
        std::shared_ptr<Element> begin_;
        std::shared_ptr<Element> end_;
    };

    template <typename TStream, typename = typename
        std::enable_if
        <
            IsStream<TStream>::value
        >::type>
    StreamRange<TStream> from(TStream const& stream)
    {
        return stream.getRange();
    }

    }
}

