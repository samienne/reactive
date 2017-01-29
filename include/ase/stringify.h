#pragma once

#include <string>
#include <sstream>
#include <vector>

namespace ase
{
    inline void vectorize(std::vector<std::string>& /*vec*/)
    {
    }

    template<typename Arg1, typename... Args>
    inline void vectorize(std::vector<std::string>& vec, Arg1&& arg1,
            Args&&... args)
    {
        std::ostringstream ss;
        ss << arg1;
        vec.push_back(ss.str());
        vectorize(vec, args...);
    }

    template<typename... Args>
    inline std::string stringify(std::string const& s, Args&&... args)
    {
        std::vector<std::string> strings;
        vectorize(strings, args...);
        std::string res;

        for (auto i = s.begin(); i != s.end(); ++i)
        {
            if (*i != '%')
            {
                res += *i;
                continue;
            }

            ++i;
            if (i == s.end())
                break;

            if (*i == '%')
            {
                res += '%';
                continue;
            }
            else if (*i > '0' && *i <= '9') // Don't accept 0
            {
                unsigned int index = *i - '0' - 1;
                if (index >= strings.size())
                {
                    res += '%';
                    res += *i;
                    continue;
                }

                res += strings[index];
            }
        }

        return res;
    }
}

