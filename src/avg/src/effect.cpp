#include "effect.h"

#include "effectfactory.h"
#include "effectshader.h"

#include <atomic>
#include <stdexcept>
#include <string>
#include <utility>

namespace avg
{

int toInt(Divisor divisor)
{
    return static_cast<int>(divisor);
}

bool isValid(Divisor divisor)
{
    switch (divisor)
    {
        case Divisor::One:
        case Divisor::Two:
        case Divisor::Four:
        case Divisor::Eight:
            return true;
    }

    return false;
}

std::ostream& operator<<(std::ostream& stream, Divisor divisor)
{
    return stream << "1/" << toInt(divisor);
}

PassInput PassInput::backdrop()
{
    return PassInput();
}

PassInput PassInput::pass(size_t index)
{
    PassInput input;
    input.pass_ = index;
    return input;
}

bool PassInput::isBackdrop() const
{
    return !pass_.has_value();
}

size_t PassInput::getPass() const
{
    return *pass_;
}

bool PassInput::operator==(PassInput const& rhs) const
{
    return pass_ == rhs.pass_;
}

bool PassInput::operator!=(PassInput const& rhs) const
{
    return !(*this == rhs);
}

std::ostream& operator<<(std::ostream& stream, PassInput const& input)
{
    if (input.isBackdrop())
        return stream << "backdrop";

    return stream << "pass(" << input.getPass() << ")";
}

ShaderId ShaderId::create()
{
    static std::atomic<uint64_t> next(1);

    ShaderId id;
    id.value_ = next.fetch_add(1, std::memory_order_relaxed);
    return id;
}

bool ShaderId::operator==(ShaderId const& rhs) const
{
    return value_ == rhs.value_;
}

bool ShaderId::operator!=(ShaderId const& rhs) const
{
    return !(*this == rhs);
}

bool PassOutput::operator==(PassOutput const& rhs) const
{
    return divisor == rhs.divisor && format == rhs.format;
}

bool PassOutput::operator!=(PassOutput const& rhs) const
{
    return !(*this == rhs);
}

bool Pass::operator==(Pass const& rhs) const
{
    return shader.id == rhs.shader.id
        && inputs == rhs.inputs
        && output == rhs.output
        && geometry == rhs.geometry
        && padding == rhs.padding
        && parameters == rhs.parameters
        ;
}

bool Pass::operator!=(Pass const& rhs) const
{
    return !(*this == rhs);
}

std::ostream& operator<<(std::ostream& stream, Geometry geometry)
{
    switch (geometry)
    {
        case Geometry::Region:
            return stream << "region";
        case Geometry::Shape:
            return stream << "shape";
    }

    return stream << "geometry(" << static_cast<int>(geometry) << ")";
}

namespace
{
    bool isTargetFormat(ase::Format format)
    {
        switch (format)
        {
            case ase::FORMAT_SRGBA:
            case ase::FORMAT_R8:
            case ase::FORMAT_RGBA16F:
                return true;
            default:
                return false;
        }
    }

    std::string passName(size_t index)
    {
        return "pass " + std::to_string(index);
    }
} // anonymous namespace

std::vector<std::string> validate(std::vector<Pass> const& passes)
{
    std::vector<std::string> errors;

    if (passes.empty())
    {
        errors.push_back("an effect needs at least one pass");
        return errors;
    }

    std::vector<bool> isRead(passes.size(), false);

    for (size_t i = 0; i < passes.size(); ++i)
    {
        Pass const& pass = passes[i];
        bool const isLast = i + 1 == passes.size();

        if (isLast && pass.geometry != Geometry::Shape)
        {
            errors.push_back("the final " + passName(i)
                    + " must draw the shape");
        }

        if (!isLast && pass.geometry == Geometry::Shape)
        {
            errors.push_back(passName(i)
                    + " draws the shape but is not the final pass");
        }

        if (pass.geometry == Geometry::Shape)
        {
            if (pass.output)
            {
                errors.push_back(passName(i)
                        + " draws the shape into the main target and cannot"
                        " have an output");
            }
        }
        else if (!pass.output)
        {
            errors.push_back(passName(i) + " has no output");
        }
        else
        {
            if (!isValid(pass.output->divisor))
            {
                errors.push_back(passName(i) + " has divisor "
                        + std::to_string(toInt(pass.output->divisor))
                        + "; it must be 1, 2, 4 or 8");
            }

            if (!isTargetFormat(pass.output->format))
            {
                errors.push_back(passName(i) + " has format "
                        + std::to_string(static_cast<int>(pass.output->format))
                        + "; it must be FORMAT_SRGBA, FORMAT_R8 or"
                        " FORMAT_RGBA16F");
            }
        }

        if (pass.padding < 0)
        {
            errors.push_back(passName(i) + " has negative padding "
                    + std::to_string(pass.padding));
        }

        if (pass.parameters)
        {
            ase::ShaderInterface::Block const& block =
                pass.parameters->getBlock();
            ase::ShaderInterface::Block const* declared =
                pass.shader.interface.findBlock(block.name);

            if (!declared)
            {
                errors.push_back(passName(i) + " has parameters for block '"
                        + block.name + "', which its shader does not"
                        " declare");
            }
            else if (*declared != block)
            {
                errors.push_back(passName(i) + " has parameters whose"
                        " layout differs from its shader's block '"
                        + block.name + "'");
            }
        }

        if (pass.inputs.size() > maxPassInputs)
        {
            errors.push_back(passName(i) + " has "
                    + std::to_string(pass.inputs.size())
                    + " inputs; the most is "
                    + std::to_string(maxPassInputs));
        }

        size_t const samplerCount = pass.shader.interface.getSamplers().size();
        if (samplerCount != pass.inputs.size())
        {
            errors.push_back(passName(i) + " has "
                    + std::to_string(pass.inputs.size())
                    + " inputs but its shader declares "
                    + std::to_string(samplerCount) + " samplers");
        }

        for (PassInput const& input : pass.inputs)
        {
            if (input.isBackdrop())
                continue;

            if (input.getPass() == i)
                errors.push_back(passName(i) + " reads its own output");
            else if (input.getPass() > i)
            {
                errors.push_back(passName(i) + " reads "
                        + passName(input.getPass())
                        + ", which runs after it");
            }
            else
                isRead[input.getPass()] = true;
        }
    }

    for (size_t i = 0; i + 1 < passes.size(); ++i)
    {
        if (!isRead[i])
            errors.push_back(passName(i) + "'s output is never read");
    }

    return errors;
}

struct Effect::Content
{
    std::vector<Pass> passes;
    std::shared_ptr<EffectRecipe const> recipe;
};

class EffectAccess
{
public:
    static Effect make(std::vector<Pass> passes,
            std::shared_ptr<EffectRecipe const> recipe)
    {
        return Effect(std::make_shared<Effect::Content const>(
                    Effect::Content{ std::move(passes), std::move(recipe) }));
    }

    static bool isSameContent(Effect const& a, Effect const& b)
    {
        return a.content_ == b.content_;
    }

    static EffectRecipe const* getRecipe(Effect const& effect)
    {
        return effect.content_->recipe.get();
    }
};

EffectRecipe::~EffectRecipe()
{
}

Effect makeEffect(std::vector<Pass> passes,
        std::shared_ptr<EffectRecipe const> recipe)
{
    std::vector<std::string> errors = validate(passes);
    if (errors.empty())
        return EffectAccess::make(std::move(passes), std::move(recipe));

    std::string message = "invalid effect:";
    for (std::string const& error : errors)
        message += "\n  " + error;

    throw std::invalid_argument(message);
}

Effect::Effect(std::shared_ptr<Content const> content) :
    content_(std::move(content))
{
}

Effect::~Effect()
{
}

std::vector<Pass> const& Effect::getPasses() const
{
    return content_->passes;
}

Pass const& Effect::getFinalPass() const
{
    return content_->passes.back();
}

bool Effect::operator==(Effect const& rhs) const
{
    return content_ == rhs.content_ || content_->passes == rhs.content_->passes;
}

bool Effect::operator!=(Effect const& rhs) const
{
    return !(*this == rhs);
}

bool isSameKind(Effect const& a, Effect const& b)
{
    EffectRecipe const* recipe = EffectAccess::getRecipe(a);
    EffectRecipe const* other = EffectAccess::getRecipe(b);

    return recipe && other && recipe->isSameKind(*other);
}

Effect lerp(Effect const& a, Effect const& b, float t)
{
    if (EffectAccess::isSameContent(a, b) || !isSameKind(a, b))
        return b;

    return EffectAccess::getRecipe(a)->interpolate(
            *EffectAccess::getRecipe(b), t);
}

} // namespace avg
