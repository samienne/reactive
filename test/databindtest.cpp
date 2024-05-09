#include <reactive/databind.h>

#include <reactive/widget/widget.h>

#include <reactive/datasourcefromcollection.h>
#include <reactive/collection.h>

#include <gtest/gtest.h>

#include <string>

using namespace reactive;

TEST(DataBind, setup)
{
    Collection<std::string> collection;

    auto s1 = dataBind<std::string>(
            dataSourceFromCollection(collection),
            [](signal::AnySignal<std::string>, size_t) -> widget::AnyWidget
            {
                return widget::makeWidget();
            });
}
