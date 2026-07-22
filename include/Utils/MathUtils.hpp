#pragma once

namespace eppyphany::Utils {
    class MathUtils {
        public:
            static double logistic(double x, double multiplier, double midpointOffset, double max = 1.0);
            static double ln(double x);
    };
}