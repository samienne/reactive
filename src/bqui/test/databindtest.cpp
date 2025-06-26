#include <bqui/databind.h>

#include <bqui/widget/widget.h>

#include <bqui/datasourcefromcollection.h>
#include <bqui/collection.h>

#include <gtest/gtest.h>

#include <string>

using namespace bqui;

TEST(DataBind, setup)
{
    Collection<std::string> collection;

    auto s1 = dataBind<std::string>(
            dataSourceFromCollection(collection),
            [](bq::signal::AnySignal<std::string>, size_t) -> widget::AnyWidget
            {
                return widget::makeWidget();
            });
}
