#pragma once

#include <clocale>
#include <string>

namespace fire::test
{
/** Temporarily installs an available locale whose decimal separator is ','.
    Tests must restore LC_NUMERIC because it is process-global state. */
class ScopedCommaNumericLocale final
{
public:
    ScopedCommaNumericLocale()
    {
        if (const auto* current = std::setlocale(LC_NUMERIC, nullptr))
            previousLocale = current;
    }

    ~ScopedCommaNumericLocale()
    {
        if (! previousLocale.empty())
            std::setlocale(LC_NUMERIC, previousLocale.c_str());
    }

    bool activate() noexcept
    {
        static constexpr const char* candidates[] {
            "de_DE.UTF-8",
            "de_DE.utf8",
            "German_Germany.1252",
            "German_Germany",
            "fr_FR.UTF-8",
            "fr_FR.utf8",
            "French_France.1252"
        };

        for (const auto* candidate : candidates)
            if (std::setlocale(LC_NUMERIC, candidate) != nullptr)
                if (const auto* conventions = std::localeconv();
                    conventions != nullptr
                    && conventions->decimal_point != nullptr
                    && std::string { conventions->decimal_point } == ",")
                    return true;

        if (! previousLocale.empty())
            std::setlocale(LC_NUMERIC, previousLocale.c_str());
        return false;
    }

    ScopedCommaNumericLocale(const ScopedCommaNumericLocale&) = delete;
    ScopedCommaNumericLocale& operator=(const ScopedCommaNumericLocale&) = delete;

private:
    std::string previousLocale;
};
} // namespace fire::test
