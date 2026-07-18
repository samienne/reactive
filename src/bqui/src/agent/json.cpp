#include "bqui/agent/json.h"

#include <cstdlib>

namespace bqui::agent
{

JsonValue::JsonValue(Variant value) : value_(std::move(value)) {}

bool JsonValue::isObject() const
{
    return std::holds_alternative<JsonObject>(value_);
}

bool JsonValue::isArray() const
{
    return std::holds_alternative<JsonArray>(value_);
}

bool JsonValue::asBool(bool fallback) const
{
    if (auto p = std::get_if<bool>(&value_))
        return *p;
    return fallback;
}

double JsonValue::asNumber(double fallback) const
{
    if (auto p = std::get_if<double>(&value_))
        return *p;
    return fallback;
}

std::string const& JsonValue::asString() const
{
    static std::string const empty;
    if (auto p = std::get_if<std::string>(&value_))
        return *p;
    return empty;
}

JsonArray const& JsonValue::asArray() const
{
    static JsonArray const empty;
    if (auto p = std::get_if<JsonArray>(&value_))
        return *p;
    return empty;
}

std::optional<JsonValue> JsonValue::find(std::string const& key) const
{
    if (auto p = std::get_if<JsonObject>(&value_))
    {
        auto it = p->find(key);
        if (it != p->end())
            return it->second;
    }

    return std::nullopt;
}

namespace
{

/**
 * @brief A recursive-descent JSON parser that fails rather than throws.
 *
 * `parse*` methods set `ok_` to false on any malformed input and return a
 * default value, so a caller only has to check success once at the end.
 */
class Parser
{
public:
    explicit Parser(std::string const& text) : text_(text) {}

    std::optional<JsonValue> parse()
    {
        skipWhitespace();
        JsonValue value = parseValue();
        skipWhitespace();

        if (!ok_ || pos_ != text_.size())
            return std::nullopt;

        return value;
    }

private:
    void skipWhitespace()
    {
        while (pos_ < text_.size())
        {
            char c = text_[pos_];
            if (c == ' ' || c == '\t' || c == '\n' || c == '\r')
                ++pos_;
            else
                break;
        }
    }

    char peek() const
    {
        return pos_ < text_.size() ? text_[pos_] : '\0';
    }

    bool consume(char c)
    {
        if (peek() == c)
        {
            ++pos_;
            return true;
        }

        ok_ = false;
        return false;
    }

    bool consumeLiteral(char const* literal)
    {
        for (char const* p = literal; *p; ++p)
        {
            if (peek() != *p)
            {
                ok_ = false;
                return false;
            }
            ++pos_;
        }

        return true;
    }

    JsonValue parseValue()
    {
        if (!ok_)
            return {};

        // Cap nesting so adversarial deeply-nested input cannot overflow the
        // stack; the agent's commands are shallow.
        if (depth_ >= kMaxDepth)
        {
            ok_ = false;
            return {};
        }

        switch (peek())
        {
            case '{': ++depth_; { auto v = parseObject(); --depth_; return v; }
            case '[': ++depth_; { auto v = parseArray(); --depth_; return v; }
            case '"': return JsonValue(parseString());
            case 't':
                consumeLiteral("true");
                return JsonValue(true);
            case 'f':
                consumeLiteral("false");
                return JsonValue(false);
            case 'n':
                consumeLiteral("null");
                return JsonValue(nullptr);
            default:
                return parseNumber();
        }
    }

    JsonValue parseObject()
    {
        JsonObject object;
        consume('{');
        skipWhitespace();

        if (peek() == '}')
        {
            ++pos_;
            return JsonValue(std::move(object));
        }

        for (;;)
        {
            skipWhitespace();
            if (peek() != '"')
            {
                ok_ = false;
                return {};
            }

            std::string key = parseString();
            skipWhitespace();
            if (!consume(':'))
                return {};

            skipWhitespace();
            JsonValue value = parseValue();
            if (!ok_)
                return {};

            object.emplace(std::move(key), std::move(value));

            skipWhitespace();
            if (peek() == ',')
            {
                ++pos_;
                continue;
            }

            if (consume('}'))
                return JsonValue(std::move(object));

            return {};
        }
    }

    JsonValue parseArray()
    {
        JsonArray array;
        consume('[');
        skipWhitespace();

        if (peek() == ']')
        {
            ++pos_;
            return JsonValue(std::move(array));
        }

        for (;;)
        {
            skipWhitespace();
            JsonValue value = parseValue();
            if (!ok_)
                return {};

            array.push_back(std::move(value));

            skipWhitespace();
            if (peek() == ',')
            {
                ++pos_;
                continue;
            }

            if (consume(']'))
                return JsonValue(std::move(array));

            return {};
        }
    }

    std::string parseString()
    {
        std::string result;
        if (!consume('"'))
            return result;

        while (pos_ < text_.size())
        {
            char c = text_[pos_++];
            if (c == '"')
                return result;

            if (c == '\\')
            {
                if (pos_ >= text_.size())
                    break;

                char escape = text_[pos_++];
                switch (escape)
                {
                    case '"': result += '"'; break;
                    case '\\': result += '\\'; break;
                    case '/': result += '/'; break;
                    case 'b': result += '\b'; break;
                    case 'f': result += '\f'; break;
                    case 'n': result += '\n'; break;
                    case 'r': result += '\r'; break;
                    case 't': result += '\t'; break;
                    case 'u': result += parseUnicodeEscape(); break;
                    default: ok_ = false; return result;
                }
            }
            else
            {
                result += c;
            }
        }

        ok_ = false;
        return result;
    }

    std::string parseUnicodeEscape()
    {
        if (pos_ + 4 > text_.size())
        {
            ok_ = false;
            return {};
        }

        unsigned int code = 0;
        for (int i = 0; i < 4; ++i)
        {
            char c = text_[pos_++];
            code <<= 4;
            if (c >= '0' && c <= '9')
                code |= static_cast<unsigned int>(c - '0');
            else if (c >= 'a' && c <= 'f')
                code |= static_cast<unsigned int>(c - 'a' + 10);
            else if (c >= 'A' && c <= 'F')
                code |= static_cast<unsigned int>(c - 'A' + 10);
            else
            {
                ok_ = false;
                return {};
            }
        }

        return encodeUtf8(code);
    }

    static std::string encodeUtf8(unsigned int code)
    {
        std::string out;
        if (code < 0x80)
        {
            out += static_cast<char>(code);
        }
        else if (code < 0x800)
        {
            out += static_cast<char>(0xc0 | (code >> 6));
            out += static_cast<char>(0x80 | (code & 0x3f));
        }
        else
        {
            out += static_cast<char>(0xe0 | (code >> 12));
            out += static_cast<char>(0x80 | ((code >> 6) & 0x3f));
            out += static_cast<char>(0x80 | (code & 0x3f));
        }

        return out;
    }

    JsonValue parseNumber()
    {
        size_t start = pos_;
        while (pos_ < text_.size())
        {
            char c = text_[pos_];
            bool numeric = (c >= '0' && c <= '9') || c == '-' || c == '+'
                || c == '.' || c == 'e' || c == 'E';
            if (numeric)
                ++pos_;
            else
                break;
        }

        if (pos_ == start)
        {
            ok_ = false;
            return {};
        }

        std::string token = text_.substr(start, pos_ - start);
        char* end = nullptr;
        double value = std::strtod(token.c_str(), &end);
        if (end != token.c_str() + token.size())
        {
            ok_ = false;
            return {};
        }

        return JsonValue(value);
    }

    static constexpr int kMaxDepth = 64;

    std::string const& text_;
    size_t pos_ = 0;
    int depth_ = 0;
    bool ok_ = true;
};

} // namespace

std::optional<JsonValue> parseJson(std::string const& text)
{
    return Parser(text).parse();
}

} // namespace bqui::agent
