#pragma once
#include <cmath>
#include <initializer_list>
namespace bev {
// Acceptance policy only; estimator, holdout metrics and persistence are pending.
struct CalibrationEvidence {
    bool observable = false, independent_validation = false;
    double reprojection_p95_px = 0, seam_p95_m = 0, abs_time_offset_ms = 0;
    unsigned stable_windows = 0;
};
struct CalibrationLimits {
    double reprojection_p95_px = 0, seam_p95_m = 0, abs_time_offset_ms = 0;
    unsigned stable_windows = 0;
};
inline bool accept_calibration(const CalibrationEvidence &e, const CalibrationLimits &l) {
    for (double v : {e.reprojection_p95_px, e.seam_p95_m, e.abs_time_offset_ms,
                     l.reprojection_p95_px, l.seam_p95_m, l.abs_time_offset_ms})
        if (!std::isfinite(v) || v < 0)
            return false;
    return e.observable && e.independent_validation && l.stable_windows > 0 &&
           e.stable_windows >= l.stable_windows && e.reprojection_p95_px <= l.reprojection_p95_px &&
           e.seam_p95_m <= l.seam_p95_m && e.abs_time_offset_ms <= l.abs_time_offset_ms;
}
} // namespace bev
