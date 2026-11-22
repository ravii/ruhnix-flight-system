#include <cstdio>
#include <limits>

#include "perception/obstacle_detector.hpp"

namespace {
int failures = 0;
#define CHECK(cond)                                                                    \
    do {                                                                               \
        if (!(cond)) {                                                                 \
            std::fprintf(stderr, "%s:%d: CHECK failed: %s\n", __FILE__, __LINE__, #cond); \
            ++failures;                                                                \
        }                                                                              \
    } while (0)

using namespace uav::perception;

DepthImage make_image(float fill) {
    DepthImage img;
    img.width = 8;
    img.height = 6;
    img.depth_m.assign(img.width * img.height, fill);
    return img;
}

void test_finds_nearest_in_roi() {
    DepthImage img = make_image(10.0f);
    img.depth_m[3 * img.width + 4] = 2.5f;  // inside centered ROI
    img.depth_m[0] = 0.5f;                  // corner: outside ROI, must be ignored
    auto obs = ObstacleDetector{}.nearest(img);
    CHECK(obs.has_value());
    CHECK(obs && obs->distance_m == 2.5f && obs->x == 4 && obs->y == 3);
}

void test_ignores_invalid_returns() {
    DepthImage img = make_image(std::numeric_limits<float>::quiet_NaN());
    img.depth_m[3 * img.width + 3] = 0.0f;
    CHECK(!ObstacleDetector{}.nearest(img).has_value());
}

void test_rejects_malformed_image() {
    DepthImage img;
    img.width = 4;
    img.height = 4;
    img.depth_m.resize(3);
    CHECK(!ObstacleDetector{}.nearest(img).has_value());
}
}  // namespace

int main() {
    test_finds_nearest_in_roi();
    test_ignores_invalid_returns();
    test_rejects_malformed_image();
    if (failures) {
        std::fprintf(stderr, "%d check(s) failed\n", failures);
        return 1;
    }
    std::printf("all perception tests passed\n");
    return 0;
}
