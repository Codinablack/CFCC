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

#pragma once
#include <concepts>
#include <limits>
#include <cmath>
#include <functional>
#include <string>
#include <vector>

namespace Components {
    namespace Skills {
        // Defines the type of formula for the points required growth curve
        enum FormulaType : uint8_t {
            LINEAR,
            LOGARITHMIC,
            EXPONENTIAL,
            QUADRATIC,
            CUBIC,
            STEP,
            ROOT,
            INVERSE
        };

        // Maximum possible points, used for overflow prevention
        static constexpr uint64_t PointMax = UINT64_MAX;
        static constexpr uint16_t LevelMax = UINT16_MAX;

        class CustomSkill {
        public:

            CustomSkill(FormulaType form = FormulaType::EXPONENTIAL, uint16_t max = 0, uint16_t x = 1, uint16_t y = 1, uint16_t z = 1) : 
            factor_x(x),
            factor_y(y),
            factor_z(z),
            max_level(max),
            formula(form) 
            {
                //
            }

            [[nodiscard]]
            constexpr uint64_t points() noexcept 
            {
                [[unlikely]]
                if (max_level > 0 and current_level >= max_level) 
                {
                    return pointsRequired(max_level);
                }
                [[likely]]
                return current_points;
            }

            // It's worth noting that even tho we have a bonus level
            // along with a current level, both of which are uint16_t,
            // we are still returning a max of a uint16_t, so maxing both out
            // will not result in an even higher level, it's still hard capped
            [[nodiscard]]
            constexpr uint16_t level(bool count_bonus = true) const noexcept 
            {
                if (not count_bonus or bonus_level == 0) 
                {
                    return current_level;
                }
                uint32_t total = static_cast<uint32_t>(current_level) + bonus_level;
                return total < LevelMax ? static_cast<uint16_t>(total) : LevelMax;
            }

            bool addPoints(uint32_t points) noexcept
            {
                [[unlikely]]
                if (not points) 
                {
                    return false;
                }

                auto temp_level = static_cast<uint64_t>(current_level);
                auto temp_current_points = current_points;

                while (true) 
                {
                    if (max_level and temp_level >= max_level) 
                    {
                        temp_current_points = 0;
                        break;
                    }

                    uint64_t points_required = pointsRequired(temp_level + 1);
                    if (points_required == std::numeric_limits<uint64_t>::max() or points_required == 0)
                    {
                        temp_current_points += points;
                        break;
                    }

                    uint64_t excess_points = points_required - temp_current_points;
                    if (points >= excess_points)
                    {
                        points -= excess_points;
                        temp_level++;
                        temp_current_points = 0;
                    }
                    else 
                    {
                        temp_current_points += points;
                        points = 0;
                        break;
                    }
                }

                current_points = temp_current_points;
                current_level = static_cast<uint16_t>(temp_level);
                return true;
            }

            bool removePoints(uint32_t points_to_remove) noexcept
            {
                [[unlikely]]
                if (not points_to_remove) 
                {
                    return false;
                }

                auto temp_level = static_cast<uint64_t>(current_level);
                auto temp_points = current_points;

                // We remove first to avoid the loop if we can.
                if (points_to_remove >= temp_points)
                {
                    points_to_remove -= temp_points;
                    temp_points = 0;
                }
                else 
                {
                    temp_points -= points_to_remove;
                    points_to_remove = 0;
                }

                while (points_to_remove > 0 and temp_level > 0) 
                {
                    auto required_points = pointsRequired(temp_level);
                    if (points_to_remove >= required_points)
                    {
                        points_to_remove -= required_points;
                        temp_level--; 
                        temp_points = 0;
                    }
                    else 
                    {
                        temp_points = required_points - points_to_remove;
                        points_to_remove = 0;
                        break;
                    }
                }

                current_level = temp_level > 0 ? static_cast<uint16_t>(temp_level) : 1;
                current_points = temp_level > 0 ? temp_points : 0;
                return true;
            }

            bool addLevels(uint16_t levels, bool save_progress = false) noexcept
            {
                [[unlikely]] 
                if (not levels) 
                {
                    return false;
                }

                uint64_t required = pointsRequired(current_level);
                double percent = save_progress and required > 0 ? static_cast<double>(current_points) / required : 0.0;
                current_level = max_level and (levels + current_level) >= max_level ? max_level : levels + current_level;
                current_points = save_progress ? static_cast<uint64_t>(pointsRequired(current_level) * percent) : 0;
                return true;
            }


            bool removeLevels(uint16_t levels, bool save_progress = false) noexcept 
            {
                [[unlikely]]
                if (not levels) 
                {
                    return false;
                }
                uint64_t required = pointsRequired(current_level);
                double percent = save_progress and required > 0 ? static_cast<double>(current_points) / required : 0.0;
                current_level = std::max<uint16_t>(1, current_level - levels);
                current_points = save_progress ? static_cast<uint64_t>(pointsRequired(current_level) * percent) : 0;
                return true;
            }

            template<typename Number = uint8_t>
            Number percent() const noexcept 
            {
                static_assert(std::is_arithmetic_v<Number>, "percent() requires an arithmetic return type");
                auto required = pointsRequired(level);

                [[likely]] 
                if (current_points and required)
                {
                    auto level = (current_level + 1);
                    auto raw_percent = (current_points * 100ULL) / required;
                    return static_cast<Number>(raw_percent);
                }
                [[unlikely]]
                return static_cast<Number>(0);
            }

            void setBonus(int16_t level) noexcept
            {
                bonus_level = level;
            }

        private:

            uint64_t current_points = 0;
            uint16_t factor_x = 1;
            uint16_t factor_y = 1;
            uint16_t factor_z = 1;
            uint16_t current_level = 1;
            int16_t bonus_level = 0;
            uint16_t max_level = 0;  // Maximum allowed level, if 0, limit is numerical limit;
            FormulaType formula = FormulaType::EXPONENTIAL;

            [[nodiscard]] 
            uint64_t pointsRequired(uint64_t target_level)
            {
                switch (formula) 
                {
                    case FormulaType::LINEAR: return linearGrowth(target_level);
                    case FormulaType::LOGARITHMIC: return logarithmicGrowth(target_level);
                    case FormulaType::EXPONENTIAL: return exponentialGrowth(target_level);
                    case FormulaType::QUADRATIC: return quadraticGrowth(target_level);
                    case FormulaType::CUBIC: return cubicGrowth(target_level);
                    case FormulaType::STEP: return stepGrowth(target_level);
                    case FormulaType::ROOT: return rootGrowth(target_level);
                    case FormulaType::INVERSE: return inverseGrowth(target_level);
                }
                return 0;
            }

            static constexpr uint64_t integerSqrt(uint64_t n)
            {
                uint64_t left = 0, right = n, ans = 0;
                while (left <= right) 
                {
                    uint64_t mid = left + (right - left) / 2;
                    if (mid <= n / mid) 
                    {
                        ans = mid;
                        left = mid + 1;
                    }
                    else 
                    {
                        right = mid - 1;
                    }
                }
                return ans;
            }

            static constexpr uint64_t integerPow(uint64_t base, uint64_t exp)
            {
                uint64_t result = 1;
                while (exp) 
                {
                    if (exp & 1) 
                    {
                        [[unlikely]]
                        if (result > PointMax / base)
                        {
                            return PointMax;
                        }     
                        result *= base;
                    }
                    exp >>= 1;

                    [[unlikely]]
                    if (exp and base > PointMax / base)
                    {
                        return PointMax;
                    }
                    base *= base;
                }
                return result;
            }

            [[nodiscard]]
            constexpr uint64_t linearGrowth(uint64_t target_level) const 
            {
                auto x = static_cast<uint64_t>(factor_x);
                auto y = static_cast<uint64_t>(factor_y);
                auto z = static_cast<uint64_t>(factor_z);

                [[unlikely]]
                if (x > 0 and y > PointMax / x) return PointMax;
                uint64_t xy = x * y;

                [[unlikely]]
                if (z > 0 and target_level > PointMax / z) return PointMax;
                uint64_t zt = z * target_level;

                [[unlikely]]
                if (xy > PointMax - zt) return PointMax;
                return xy + zt;
            }

            [[nodiscard]] 
            constexpr uint64_t logarithmicGrowth(uint64_t target_level) const
            {
                auto x = static_cast<uint64_t>(factor_x);
                auto y = static_cast<uint64_t>(factor_y);
                auto z = static_cast<uint64_t>(factor_z);

                [[unlikely]]
                if (y > PointMax / target_level) return PointMax;
                uint64_t product = y * target_level;

                [[unlikely]]
                if (z > PointMax - product) return PointMax;
                uint64_t sum = product + z;

                [[unlikely]]
                if (sum == 0) return 0;
                uint64_t log2_val = 0;
                while (sum >>= 1) ++log2_val;

                [[unlikely]]
                if (x > PointMax / log2_val) return PointMax;
                return x * log2_val;
            }

            [[nodiscard]] 
            constexpr uint64_t exponentialGrowth(uint64_t target_level)
            {
                auto x = static_cast<uint64_t>(factor_x);
                auto y = static_cast<uint64_t>(factor_y);
                auto z = static_cast<uint64_t>(factor_z);

                [[unlikely]]
                if (target_level <= z + 1) return x;
                uint64_t exponent = target_level - (z + 1);
                uint64_t power = integerPow(y, exponent);

                [[unlikely]]
                if (x > 0 and power > PointMax / x) return PointMax;
                return x * power;
            }

            [[nodiscard]] 
            constexpr uint64_t quadraticGrowth(uint64_t level) const
            {
                auto x = static_cast<uint64_t>(factor_x);
                auto y = static_cast<uint64_t>(factor_y);
                auto z = static_cast<uint64_t>(factor_z);

                [[unlikely]]
                if (level > PointMax / level) return PointMax;
                uint64_t level2 = level * level;

                [[unlikely]]
                if (x > 0 and level2 > PointMax / x) return PointMax;
                uint64_t x_part = x * level2;

                [[unlikely]]
                if (y > 0 and level > PointMax / y) return PointMax;
                uint64_t y_part = y * level;

                [[unlikely]]
                if (x_part > PointMax - y_part) return PointMax;
                uint64_t sum = x_part + y_part;

                [[unlikely]]
                if (sum > PointMax - z) return PointMax;
                return sum + z;
            }

            [[nodiscard]] 
            constexpr uint64_t cubicGrowth(uint64_t level) const
            {
                auto x = static_cast<uint64_t>(factor_x);

                [[unlikely]]
                if (level > PointMax / level) return PointMax;
                uint64_t level2 = level * level;

                [[unlikely]]
                if (level2 > PointMax / level) return PointMax;
                uint64_t level3 = level2 * level;

                [[unlikely]]
                if (x > 0 and level3 > PointMax / x) return PointMax;
                return x * level3;
            }

            [[nodiscard]] 
            constexpr uint64_t stepGrowth(uint64_t level) const
            {
                auto x = static_cast<uint64_t>(factor_x);
                auto y = static_cast<uint64_t>(factor_y);
                auto z = static_cast<uint64_t>(factor_z);

                [[unlikely]]
                if (z > PointMax - level) return PointMax;
                uint64_t stepped = level + z;

                [[unlikely]]
                if (y == 0) return PointMax;
                uint64_t quotient = stepped / y;

                [[unlikely]]
                if (x > 0 and quotient > PointMax / x) return PointMax;
                return x * quotient;
            }

            [[nodiscard]] 
            constexpr uint64_t rootGrowth(uint64_t level) const
            {
                auto x = static_cast<uint64_t>(factor_x);
                auto y = static_cast<uint64_t>(factor_y);
                auto z = static_cast<uint64_t>(factor_z);

                [[unlikely]]
                if (y > PointMax - level) return PointMax;
                uint64_t input = level + y;

                uint64_t root = integerSqrt(input);

                [[unlikely]]
                if (x > 0 and root > PointMax / x) return PointMax;
                uint64_t product = x * root;

                [[unlikely]]
                if (product > PointMax - z) return PointMax;
                return product + z;
            }

            [[nodiscard]] 
            constexpr uint64_t inverseGrowth(uint64_t level) const
            {
                auto x = static_cast<uint64_t>(factor_x);
                auto y = static_cast<uint64_t>(factor_y);
                auto z = static_cast<uint64_t>(factor_z);

                [[unlikely]]
                if (y > PointMax - level) return PointMax;
                uint64_t denom = y + level;

                [[unlikely]]
                if (denom == 0) return PointMax;
                uint64_t div = x / denom;

                [[unlikely]]
                if (div > PointMax - z) return PointMax;
                return div + z;
            }
        };
    }
}
