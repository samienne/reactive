#pragma once

#include "bqui/bquivisibility.h"

#include <map>
#include <memory>
#include <optional>
#include <string>
#include <variant>
#include <vector>

namespace bqui::agent
{
    class JsonValue;

    using JsonObject = std::map<std::string, JsonValue>;
    using JsonArray = std::vector<JsonValue>;

    /**
     * @brief A parsed JSON value: null, bool, number, string, array, or object.
     *
     * A minimal read side for the small inbound agent commands; the accessors
     * return nullopt / a default rather than throwing, so malformed or
     * unexpected input is handled without crashing.
     */
    class BQUI_EXPORT JsonValue
    {
    public:
        using Variant = std::variant<
            std::nullptr_t,
            bool,
            double,
            std::string,
            JsonArray,
            JsonObject
            >;

        JsonValue() = default;
        JsonValue(Variant value);

        bool isObject() const;
        bool isArray() const;

        /** @brief The bool, or `fallback` if this is not a bool. */
        bool asBool(bool fallback = false) const;

        /** @brief The number, or `fallback` if this is not a number. */
        double asNumber(double fallback = 0.0) const;

        /** @brief The string, or "" if this is not a string. */
        std::string const& asString() const;

        /** @brief The array's elements, or an empty list if not an array. */
        JsonArray const& asArray() const;

        /** @brief The named member, or nullopt if absent or this is no object. */
        std::optional<JsonValue> find(std::string const& key) const;

    private:
        Variant value_ = nullptr;
    };

    /**
     * @brief Parse a JSON document.
     *
     * @return The parsed value, or nullopt if the text is not valid JSON.
     */
    BQUI_EXPORT std::optional<JsonValue> parseJson(std::string const& text);
} // namespace bqui::agent
