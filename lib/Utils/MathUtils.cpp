#include "Utils/MathUtils.hpp"
#include <cmath>

namespace eppyphany::Utils {
    double MathUtils::logistic(double x, double multiplier, double midpointOffset, double max) {
        return max / (1 + std::exp(multiplier * (midpointOffset - x)));
    }

    double MathUtils::ln(double x) {
        return std::log(x) / std::log(M_E);
    }
}