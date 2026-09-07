#pragma once

#include <stdexcept>

namespace arrange
{

class Error : public std::runtime_error
{
public:
    using std::runtime_error::runtime_error;
};

class UnsatisfiableConstraint : public Error
{
public:
    using Error::Error;
};
class DuplicateConstraint : public Error
{
public:
    using Error::Error;
};
class UnknownConstraint : public Error
{
public:
    using Error::Error;
};
class DuplicateEditVariable : public Error
{
public:
    using Error::Error;
};
class UnknownEditVariable : public Error
{
public:
    using Error::Error;
};
class RequiredFailure : public Error
{
public:
    using Error::Error;
};
class BadStrength : public Error
{
public:
    using Error::Error;
};
class BadId : public Error
{
public:
    using Error::Error;
};

}  // namespace arrange
