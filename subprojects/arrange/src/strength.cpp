#include <arrange/errors.h>
#include <arrange/strength.h>

#include <algorithm>

namespace arrange
{

namespace
{

constexpr double kRequiredValue = 1'001'001'000.0;
constexpr double kStrongValue = 1'000'000.0;
constexpr double kMediumValue = 1'000.0;
constexpr double kWeakValue = 1.0;

double clampWeight(double weight)
{
    return std::max(0.0, weight);
}

}  // namespace

Strength::Strength(double value) noexcept
    : value_(value)
{
}

Strength Strength::required()
{
    return Strength{kRequiredValue};
}

Strength Strength::strong(double weight)
{
    return Strength{kStrongValue * clampWeight(weight)};
}

Strength Strength::medium(double weight)
{
    return Strength{kMediumValue * clampWeight(weight)};
}

Strength Strength::weak(double weight)
{
    return Strength{kWeakValue * clampWeight(weight)};
}

Strength Strength::create(double strong, double medium, double weak, double weight)
{
    const double w = clampWeight(weight);
    const double s = std::max(0.0, std::min(1000.0, strong));
    const double m = std::max(0.0, std::min(1000.0, medium));
    const double k = std::max(0.0, std::min(1000.0, weak));
    return Strength{w * (s * 1'000'000.0 + m * 1'000.0 + k)};
}

double Strength::value() const noexcept
{
    return value_;
}

bool Strength::isRequired() const noexcept
{
    return value_ >= kRequiredValue;
}

bool Strength::operator==(const Strength& other) const noexcept
{
    return value_ == other.value_;
}

bool Strength::operator!=(const Strength& other) const noexcept
{
    return value_ != other.value_;
}

bool Strength::operator<(const Strength& other) const noexcept
{
    return value_ < other.value_;
}

bool Strength::operator<=(const Strength& other) const noexcept
{
    return value_ <= other.value_;
}

bool Strength::operator>(const Strength& other) const noexcept
{
    return value_ > other.value_;
}

bool Strength::operator>=(const Strength& other) const noexcept
{
    return value_ >= other.value_;
}

}  // namespace arrange
