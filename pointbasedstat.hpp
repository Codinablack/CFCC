// MIT License

// Author : Codinablack@github.com

// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:

// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.

// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

#include <concepts>
#include <vector>
#include <memory>
#include <algorithm>
#include <numeric>
#include <stdexcept>
#include <cmath>

template<class T>
concept PositiveNumber =
    std::is_integral_v<T> &&
    std::is_unsigned_v<T> &&
    // Ensures at least 16-bit width
    // prevents usage of bool and char
    sizeof(T) >= 2;

// Forward declaration
template<PositiveNumber NumberType>
class PointStat;

// Base modifier class
// We constrain everything to positive numbers 
// to build in safety rather than throw exceptions
template<PositiveNumber NumberType>
class Modifier {
public:
    enum class Type {
        Multiply,
        Divide,
        Add,
        Subtract
    };

    // Lets build in some 0 value checks into the class,
    // to save from doing this work when applying the modifier.
    Modifier(   Type type, 
                NumberType value, 
                bool p_scale = true)
                : type_(type)
                , value_(value)
                , proportional_scaling_(p_scale) 
    {
        if (value == 0) 
        {
            // log::invalid_argument("0 being passed to Modifier constructor");
            switch(type_) 
            {

                case Type::Multiply: 
                {
                    // We can't allow 0 as a multiplier
                    value_ = 1;
                    break;
                }

                case Type::Divide:
                {
                    // We can't allow 0 as a divider
                    value_ = 1;
                    break;
                }

                case Type::Add:
                {
                    // zero is actually ok here, and we don't want to ruin a person's
                    // design on some sort of system by changing it so we will allow
                    // I would recommend logging here as well.
                    break;
                }

                case Type::Subtract:
                {
                    // another place you probably want to log. 
                    break;
                }

                default:
                {
                    // log::error("Type safety broken for Modifier");
                    break;
                }
            }
        }
    }

    ~Modifier() = default;

    Type getType() const { return type_; }
    NumberType getValue() const { return value_; }
    bool getProportionalScaling() const { return proportional_scaling_; }

private:
    Type type_;
    NumberType value_;
    bool proportional_scaling_;
};

template<PositiveNumber NumberType>
class PointStat {
public:

    PointStat(NumberType initial, NumberType max)
        : current_(initial)
        , max_(max)
        , base_max_(max)
    {
        // Again we are choosing to build in type safety to avoid paying costs
        // on checking if our values are safe to use everytime we want to use them.
        if (max <= 0) 
        {
            throw std::invalid_argument("PointStat max must be positive");
        }
        // Here you might rather just throw or set the max to initial
        // I can probably add "policies" for such things.. later..
        if (initial > max) {
           // log::invalid_argument("PointStat initial value set higher than max value during construction");
            current_ = max;
        }
    }

    // Add a modifier
    void addModifier(std::unique_ptr<Modifier<NumberType>> modifier) 
    {
        auto mod_type = modifier->getType();
        auto start_value = max_;
        if (NumberType result = applyModifier(*modifier); result > 0) 
        {
            max_ = result;
            
            if (modifier->getProportionalScaling())
            {
                // Scale current value proportionally
                double ratio = static_cast<double>(current_) / start_value;
                current_ = static_cast<NumberType>(ratio * max_);
                
                // Ensure current doesn't become zero due to rounding
                if (current_ == 0 and ratio > 0) {
                    current_ = 1;
                }
            }
            
            modifiers_.push_back(std::move(modifier));
        }
    }

    // Remove a modifier
    bool removeModifier(const std::unique_ptr<Modifier<NumberType>>& modifier) 
    {
        auto it = std::find_if(modifiers_.begin(), modifiers_.end(), 
                              [&modifier](const auto& elem) { return elem.get() == modifier.get(); });
        
        if (it != modifiers_.end()) {
            // Store original values
            NumberType old_max = max_;
            
            // Remove the modifier
            modifiers_.erase(it);
            
            // Recalculate max from base
            recalculateMax();
            
            // Apply proportional scaling if needed
            if (modifier->getProportionalScaling() && old_max > 0) {
                // Scale current value proportionally
                double ratio = static_cast<double>(current_) / old_max;
                current_ = static_cast<NumberType>(ratio * max_);
                
                // Ensure current doesn't become zero due to rounding
                if (current_ == 0 and ratio > 0) {
                    current_ = 1;
                }
            }
            
            return true;
        }
        return false;
    }

    NumberType current() 
    {
        return current_;
    }

    NumberType value() 
    {
        return current_;
    }

    NumberType max() 
    {
        return max_;
    }

    NumberType baseMax() 
    {
        return base_max_;
    }

    bool clearModifiers() 
    {
        if (modifiers_.empty()) {
            return false;
        }
        
        // Store current ratio for potential proportional scaling
        double ratio = static_cast<double>(current_) / max_;
        
        // Clear all modifiers
        modifiers_.clear();
        
        // Reset max to base max
        max_ = base_max_;
        
        // Scale current value proportionally
        current_ = static_cast<NumberType>(ratio * max_);
        
        // Ensure current doesn't exceed max
        if (current_ > max_) {
            current_ = max_;
        }
        
        return true;
    }

    // Add points with bounds checking
    bool add(NumberType points) 
    {
        // Check for potential overflow
        if (points > std::numeric_limits<NumberType>::max() - current_) {
            current_ = max_;
            return false;
        }
        
        // Add points but cap at max
        NumberType new_value = current_ + points;
        if (new_value > max_) {
            current_ = max_;
            return false;  // Couldn't add all points
        } else {
            current_ = new_value;
            return true;   // Successfully added all points
        }
    }

    // Remove points with bounds checking
    bool remove(NumberType points) 
    {
        if (points > current_) {
            current_ = 0;
            return false;  // Couldn't remove all points
        } else {
            current_ -= points;
            return true;   // Successfully removed all points
        }
    }

    // Convenience method for chaining modifiers
    PointStat& modify(std::unique_ptr<Modifier<NumberType>> modifier) {
        addModifier(std::move(modifier));
        return *this;
    }

private:

    struct _apply_results 
    {
        _apply_results(NumberType val, bool success) : _value(val), _success(success) {}
        NumberType _value;
        bool _success;
    };

    // Recalculate max value from base_max_ and all modifiers
    void recalculateMax() {
        max_ = base_max_;
        for (const auto& mod : modifiers_) {
            NumberType result = applyModifier(*mod);
            if (result > 0) {
                max_ = result;
            }
        }
    }

    // returns the amount applied if any, so if it fails, it returns 0
    NumberType applyModifier(const Modifier<NumberType>& modifier) 
    {
        switch(modifier.getType())
        {
            case Modifier<NumberType>::Type::Multiply:
            {
                if (const auto results = canApplyMultiplier(modifier); results._success) 
                {
                    return results._value;
                }
                return 0;
            }

            case Modifier<NumberType>::Type::Divide:
            {
                if (const auto results = canApplyDivider(modifier); results._success)
                {
                    return results._value;
                }
                return 0;
            }

            case Modifier<NumberType>::Type::Add:
            {
                if (const auto results = canApplyAdditive(modifier); results._success) 
                {
                    return results._value;
                }
                return 0;
            }

            case Modifier<NumberType>::Type::Subtract:
            {
                if (const auto results = canApplySubtractive(modifier); results._success) 
                {
                    return results._value;
                }
                return 0;
            }

            [[unlikely]] default:
            {
                return 0;
            }
        }
        // potential log location as this should be unreachable
        return 0;
    }

    _apply_results canApplyMultiplier(const Modifier<NumberType>& modifier)
    {
        const NumberType temp = modifier.getValue() * max_;
        if (temp < max_ && modifier.getValue() > 1) {
            // Overflow detected
            return _apply_results{0, false};
        }
        return _apply_results{temp, true};
    }

    _apply_results canApplyDivider(const Modifier<NumberType>& modifier)
    {
        if (modifier.getValue() == 0) {
            return _apply_results{0, false};
        }
        
        const NumberType temp = max_ / modifier.getValue();
        if (temp == 0) {
            // we disallow equaling zero because that would
            // bypass our built in type safety
            return _apply_results{0, false};
        }
        return _apply_results{temp, true};
    }

    _apply_results canApplyAdditive(const Modifier<NumberType>& modifier)
    {
        if (max_ > std::numeric_limits<NumberType>::max() - modifier.getValue()) {
            // Overflow detected
            return _apply_results{0, false};
        }
        const NumberType temp = max_ + modifier.getValue();
        return _apply_results{temp, true};
    }

    _apply_results canApplySubtractive(const Modifier<NumberType>& modifier)
    {
        if (modifier.getValue() >= max_) {
            // Would result in zero or underflow
            return _apply_results{0, false};
        } 
        
        const NumberType temp = max_ - modifier.getValue();
        // Because this function is internally used to determine the max value
        // the final value is not permitted to be 0 as that violates our safety.
        if (temp == 0) {
            return _apply_results{0, false};
        } 
        return _apply_results{temp, true};
    }

    std::vector<std::unique_ptr<Modifier<NumberType>>> modifiers_;
    NumberType current_;
    NumberType base_max_;
    NumberType max_;
};
