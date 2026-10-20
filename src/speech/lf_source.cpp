#include "speech/lf_source.hpp"
#include <cmath>
#include <numbers>
#include <stdexcept>
namespace ortho {
LfPulse::LfPulse(const LfParameters& parameters) {
    const double tp = parameters.tp, te = parameters.te, ta = parameters.ta;
    if (!std::isfinite(tp) || !std::isfinite(te) || !std::isfinite(ta) || tp < 0.1 || tp > 0.7 ||
        te < tp + 0.01 || te > 0.9 || te > 2 * tp - 0.01 || ta < 0.001 || ta > 0.2 || ta >= 1 - te)
        throw std::invalid_argument("Invalid LF source shape.");
    // Standard LF equations, with T0=1 and Ee=1. See Gobl (2021), equations
    // 4-6: https://www.isca-archive.org/interspeech_2021/gobl21_interspeech.pdf
    // The finite return reaches zero at T0. Solve its nonzero epsilon root.
    const double tail = 1 - te;
    double low = 1e-9, high = 1 / ta;
    for (int i = 0; i < 80; ++i) {
        const double middle = (low + high) / 2;
        if (-std::expm1(-middle * tail) - middle * ta > 0)
            low = middle;
        else
            high = middle;
    }
    const double epsilon = (low + high) / 2;
    const double shift = std::exp(-epsilon * tail);
    const double return_gain = 1 / (epsilon * ta);
    const double return_area = -return_gain * ((1 - shift) / epsilon - tail * shift);
    const double omega = std::numbers::pi / tp;
    const double sine = std::sin(omega * te), cosine = std::cos(omega * te);
    // E0 enforces continuity at Te. Its substitution into the open-phase
    // integral permits a well-conditioned zero-area solve for alpha.
    const auto net_area = [&](double alpha) {
        const double open_area = -(alpha * sine - omega * cosine + omega * std::exp(-alpha * te)) /
                                 (sine * (alpha * alpha + omega * omega));
        return open_area + return_area;
    };
    low = -50;
    high = 50;
    if (!(net_area(low) > 0 && net_area(high) < 0))
        throw std::invalid_argument("Cannot balance LF source shape.");
    for (int i = 0; i < 80; ++i) {
        const double middle = (low + high) / 2;
        if (net_area(middle) > 0)
            low = middle;
        else
            high = middle;
    }
    const double alpha = (low + high) / 2;
    for (int i = 1; i < table_size; ++i) {
        const double t = double(i) / table_size;
        const double value = t < te ? -std::exp(alpha * (t - te)) * std::sin(omega * t) / sine
                                    : -return_gain * (std::exp(-epsilon * (t - te)) - shift);
        if (!std::isfinite(value) || std::abs(value) > 10)
            throw std::invalid_argument("Unstable LF source shape.");
        cycle_[i] = static_cast<float>(value);
    }
}
float LfPulse::sample(float phase) const {
    if (!(phase > 0 && phase < 1))
        return 0;
    const float at = phase * table_size;
    const int index = static_cast<int>(at);
    const float fraction = at - index;
    return cycle_[index] + (cycle_[index + 1] - cycle_[index]) * fraction;
}
} // namespace ortho
