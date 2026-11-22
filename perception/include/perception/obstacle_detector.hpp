#pragma once

#include <cstddef>
#include <optional>
#include <vector>

namespace uav::perception
{

// Row-major depth image in meters. Non-finite or <= 0 values mean "no return".
struct DepthImage
{
    std::size_t width = 0;
    std::size_t height = 0;
    std::vector<float> depth_m;

    float at(std::size_t x, std::size_t y) const { return depth_m[y * width + x]; }
};

struct Obstacle
{
    float distance_m;
    std::size_t x;
    std::size_t y;
};

// Finds the closest valid depth return inside a centered region of interest.
// Baseline detector; to be replaced by a proper point-cloud / learned pipeline.
class ObstacleDetector
{
public:
    struct Config
    {
        float roi_fraction = 0.5f;  // ROI size relative to the image, per axis
        float min_range_m = 0.2f;
        float max_range_m = 20.0f;
    };

    ObstacleDetector() : ObstacleDetector(Config{}) {}
    explicit ObstacleDetector(const Config& config) : config_(config) {}

    std::optional<Obstacle> nearest(const DepthImage& image) const;

private:
    Config config_;
};

}  // namespace uav::perception
