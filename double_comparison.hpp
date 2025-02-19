#ifndef DOUBLE_COMPARISON_MACROS
#define DOUBLE_COMPARISON_MACROS

#include <cmath>
#ifndef DEFAULT_DOUBLE_COMPARISON_BASE
# define DEFAULT_DOUBLE_COMPARISON_BASE 1e-8
#endif
#ifndef DOT_DOUBLE_COMPARISON_BASE
# define DOT_DOUBLE_COMPARISON_BASE_ACCURACY 15
# define DOT_DOUBLE_COMPARISON_BASE 1e-15
#endif

#ifndef DOUBLE_EQUAL
# define DOUBLE_EQUAL_INNER(a, b, BASE, ...) ((a) - (b) < (BASE) && (b) - (a) < (BASE))
# define DOUBLE_EQUAL(a, b, ...) DOUBLE_EQUAL_INNER(a, b, ##__VA_ARGS__, DEFAULT_DOUBLE_COMPARISON_BASE)
#endif

#ifndef DOUBLE_NOT_EQUAL
# define DOUBLE_NOT_EQUAL_INNER(a, b, BASE, ...) ((a) - (b) >= BASE || (b) - (a) >= BASE)
# define DOUBLE_NOT_EQUAL(a, b, ...) DOUBLE_NOT_EQUAL_INNER(a, b, ##__VA_ARGS__, DEFAULT_DOUBLE_COMPARISON_BASE)
#endif

#ifndef DOUBLE_GREATER
# define DOUBLE_GREATER_INNER(a, b, BASE, ...) ((a) - (b) >= BASE)
# define DOUBLE_GREATER(a, b, ...) DOUBLE_GREATER_INNER(a, b, ##__VA_ARGS__, DEFAULT_DOUBLE_COMPARISON_BASE)
#endif

#ifndef DOUBLE_LESS
# define DOUBLE_LESS_INNER(a, b, BASE, ...) ((a) - (b) <= -BASE)
# define DOUBLE_LESS(a, b, ...) DOUBLE_LESS_INNER(a, b, ##__VA_ARGS__, DEFAULT_DOUBLE_COMPARISON_BASE)
#endif

#ifndef DOUBLE_GREATER_OR_EQUAL
# define DOUBLE_GREATER_OR_EQUAL_INNER(a, b, BASE, ...) ((a) - (b) > -1 * (BASE))
# define DOUBLE_GREATER_OR_EQUAL(a, b, ...) DOUBLE_GREATER_OR_EQUAL_INNER(a, b, ##__VA_ARGS__, DEFAULT_DOUBLE_COMPARISON_BASE)
#endif

#ifndef DOUBLE_LESS_OR_EQUAL
# define DOUBLE_LESS_OR_EQUAL_INNER(a, b, BASE, ...) ((a) - (b) < (BASE))
# define DOUBLE_LESS_OR_EQUAL(a, b, ...) DOUBLE_LESS_OR_EQUAL_INNER(a, b, ##__VA_ARGS__, DEFAULT_DOUBLE_COMPARISON_BASE)
#endif

#ifndef DOUBLE_GREATER_OR_EQUAL_ZERO
# define DOUBLE_GREATER_OR_EQUAL_ZERO_INNER(a, BASE, ...) ((a) > -1 * (BASE))
# define DOUBLE_GREATER_OR_EQUAL_ZERO(a, ...) DOUBLE_GREATER_OR_EQUAL_ZERO_INNER(a, ##__VA_ARGS__, DEFAULT_DOUBLE_COMPARISON_BASE)
#endif

#ifndef DOUBLE_LESS_OR_EQUAL_ZERO
# define DOUBLE_LESS_OR_EQUAL_ZERO_INNER(a, BASE, ...) ((a) < (BASE))
# define DOUBLE_LESS_OR_EQUAL_ZERO(a, ...) DOUBLE_LESS_OR_EQUAL_ZERO_INNER(a, ##__VA_ARGS__, DEFAULT_DOUBLE_COMPARISON_BASE)
#endif

#ifndef DOUBLE_GREATER_ZERO
# define DOUBLE_GREATER_ZERO_INNER(a, BASE, ...) ((a) > (BASE))
# define DOUBLE_GREATER_ZERO(a, ...) DOUBLE_GREATER_ZERO_INNER(a, ##__VA_ARGS__, DEFAULT_DOUBLE_COMPARISON_BASE)
#endif

#ifndef DOUBLE_LESS_ZERO
# define DOUBLE_LESS_ZERO_INNER(a, BASE, ...) ((a) < -1 * (BASE))
# define DOUBLE_LESS_ZERO(a, ...) DOUBLE_LESS_ZERO_INNER(a, ##__VA_ARGS__, DEFAULT_DOUBLE_COMPARISON_BASE)
#endif

#ifndef DOUBLE_EQUAL_ZERO
# define DOUBLE_EQUAL_ZERO_INNER(a, BASE, ...) ((a) < (BASE) && (a) > -1 * (BASE))
# define DOUBLE_EQUAL_ZERO(a, ...) DOUBLE_EQUAL_ZERO_INNER(a, ##__VA_ARGS__, DEFAULT_DOUBLE_COMPARISON_BASE)
#endif

#ifndef DOUBLE_NOT_EQUAL_ZERO
# define DOUBLE_NOT_EQUAL_ZERO_INNER(a, BASE, ...) ((a) > (BASE) || (a) < -1 * (BASE))
# define DOUBLE_NOT_EQUAL_ZERO(a, ...) DOUBLE_NOT_EQUAL_ZERO_INNER(a, ##__VA_ARGS__, DEFAULT_DOUBLE_COMPARISON_BASE)
#endif

namespace BTool {
    // 获取小数点点位, 数据范围:[1e-14 ~ 1e14]
    // value: 0, 返回0
    // value: 0.099999999999999, 返回-1
    // value: 0.09999999999999, 返回-14
    // value: 0.100000000000001, 返回-1
    // value: 0.10000000000001, 返回-14
    // value: 1e-15, 返回-1
    // value: 1e-14, 返回-14
    // value: 0.1, 返回-1
    // value: 1e-15及更小, 返回0
    // value: 1.1, 返回-1
    // value: 1.12, 返回-2
    // value: 1.2345678901234567e9, 返回-6
    // value: 10, 返回1
    // value: 100, 返回2
    // value: 1.23456789012345e14, 返回0
    // value: 1.23456789012345e19, 返回5
    inline int GetDoubleAccuracy(const double& value)
    {
        double dot_part = std::fabs(value);
        if (dot_part < DOT_DOUBLE_COMPARISON_BASE) {
            return 0;
        }

        int index = 0;
        double base{DOT_DOUBLE_COMPARISON_BASE};
        while (DOUBLE_GREATER_OR_EQUAL(dot_part, 10.0, std::min(base, 1.0))) {
            ++index;
            dot_part /= 10.0;
            base *= 10.0;
        }
        int int_index = index;
        index = 0;
        base = {DOT_DOUBLE_COMPARISON_BASE};
        if (int_index >= DOT_DOUBLE_COMPARISON_BASE_ACCURACY - 1)
            base *= 10.0;
        while (DOUBLE_GREATER_ZERO(std::fabs(dot_part - std::round(dot_part)), base)) {
            ++index;
            dot_part *= 10.0;
            base *= 10.0;
        }
        return int_index - index;
    }
}

#endif // DOUBLE_COMPARISON_MACROS