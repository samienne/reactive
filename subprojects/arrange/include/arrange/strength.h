#pragma once

namespace arrange
{

class Strength
{
public:
    static Strength required();
    static Strength strong(double weight = 1.0);
    static Strength medium(double weight = 1.0);
    static Strength weak(double weight = 1.0);
    static Strength create(double strong, double medium, double weak, double weight = 1.0);

    double value() const noexcept;
    bool isRequired() const noexcept;

    bool operator==(const Strength& other) const noexcept;
    bool operator!=(const Strength& other) const noexcept;
    bool operator<(const Strength& other) const noexcept;
    bool operator<=(const Strength& other) const noexcept;
    bool operator>(const Strength& other) const noexcept;
    bool operator>=(const Strength& other) const noexcept;

private:
    explicit Strength(double value) noexcept;

    double value_{};
};

}  // namespace arrange
