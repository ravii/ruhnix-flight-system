#include "esc/current_controller.hpp"

#include <algorithm>

namespace esc
{

AlphaBeta CurrentController::update(const Input& in, float dt_s)
{
    measured_ = park(clarke(in.phase_current_a), in.theta_elec);

    const float lim = config_.voltage_limit;
    auto pi = [&](float ref, float meas, float& integ) {
        const float err = ref - meas;
        integ = std::clamp(integ + config_.ki * err * dt_s, -lim, lim);
        return std::clamp(config_.kp * err + integ, -lim, lim);
    };

    const Dq v{pi(in.current_ref_a.d, measured_.d, integrator_.d),
        pi(in.current_ref_a.q, measured_.q, integrator_.q)};
    return inv_park(v, in.theta_elec);
}

}  // namespace esc
