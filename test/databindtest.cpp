#include <reactive/signal/databind.h>

#include <reactive/datasourcefromcollection.h>
#include <reactive/collection.h>

#include <gtest/gtest.h>

#include <iostream>
#include <string>

using namespace reactive;

TEST(DataBind, setup)
{
    Collection<std::string> collection;

    auto s1 = signal::dataBind<std::string>(
            dataSourceFromCollection(collection),
            [](Signal<std::string>, size_t) -> WidgetFactory
            {
                return makeWidgetFactory();
            });
}