#include "perception/obstacle_detector.hpp"

#include <cmath>

namespace uav::perception {

std::optional<Obstacle> ObstacleDetector::nearest(const DepthImage& image) const {
    if (image.width == 0 || image.height == 0 ||
        image.depth_m.size() != image.width * image.height) {
        return std::nullopt;
    }

    const auto roi_w = static_cast<std::size_t>(static_cast<float>(image.width) * config_.roi_fraction);
    const auto roi_h = static_cast<std::size_t>(static_cast<float>(image.height) * config_.roi_fraction);
    const std::size_t x0 = (image.width - roi_w) / 2;
    const std::size_t y0 = (image.height - roi_h) / 2;

    std::optional<Obstacle> best;
    for (std::size_t y = y0; y < y0 + roi_h; ++y) {
        for (std::size_t x = x0; x < x0 + roi_w; ++x) {
            const float d = image.at(x, y);
            if (!std::isfinite(d) || d < config_.min_range_m || d > config_.max_range_m) {
                continue;
            }
            if (!best || d < best->distance_m) {
                best = Obstacle{d, x, y};
            }
        }
    }
    return best;
}

}  // namespace uav::perception
