/** @file
    @brief Produces a distortion mesh and partial display description
           from a table of display locations to angle inputs.

    @date 2015

    @author
    Russ Taylor working through ReliaSolve.com for Sensics, Inc.
    <http://sensics.com/osvr>
*/

// Copyright 2015 Sensics, Inc.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#pragma once

// Internal Includes

// Library includes
#include <Eigen/Core>
#include <Eigen/Geometry>

// Standard includes
#include <array>
#include <cmath>
#include <iomanip>
#include <sstream>
#include <string>
#include <vector>

// Global constants and variables
#define MY_PI (4.0 * std::atan(1.0))

template <typename T> struct RectBounds {
    using value_type = T;
    value_type left;
    value_type right;
    value_type top;
    value_type bottom;
    RectBounds<T> reflectedHorizontally() const { return RectBounds<T>{-right, -left, top, bottom}; }
};
using RectBoundsd = RectBounds<double>;

using Point2d = std::array<double, 2>;
using Point3d = std::array<double, 3>;

/// Gentle wrapper around Point2d assigning longitude and latitude meaning (respectively) to the elements.
struct LongLat {
    Point2d longLat;
    /// angle in x
    double& longitude() { return longLat[0]; }
    /// angle in x (read-only)
    double longitude() const { return longLat[0]; }

    /// angle in y
    double& latitude() { return longLat[1]; }
    /// angle in y (read-only)
    double latitude() const { return longLat[1]; }
};

struct Config {
    bool computeScreenBounds = true;
    RectBoundsd suppliedScreenBounds;
    bool useFieldAngles = true;
    double toMeters = 1.0;
    double depth = 2.0;
    bool verifyAngles = false;
    /// parameters to verify_angles
    double xx, xy, yx, yy, maxAngleDiffDegrees;

    bool verbose = false;
};

// Screen-space to/from angle-space map entry
class XYLatLong {
  public:
    double x = 0;
    double y = 0;
    double latitude = 0;
    double longitude = 0;

    XYLatLong(double px, double py, double plat, double plong) {
        x = px;
        y = py;
        latitude = plat;
        longitude = plong;
    }
    XYLatLong() { x = y = latitude = longitude = 0; }
};

/// Convenient storage for the input source (typically file name) and line number associated with some measurement or
/// (more typically) a derived quantity.
struct DataOrigin {
    std::string inputSource;
    std::size_t lineNumber;

    /// Is the origin of this data known? (or default-constructed aka unknown?)
    bool known() const { return !inputSource.empty(); }
};

/// Stream insertion operator for DataOrigin
inline std::ostream& operator<<(std::ostream& os, DataOrigin const& orig) {
    if (orig.known()) {
        std::ostringstream oss;
        oss << orig.inputSource << ":" << orig.lineNumber;
        os << oss.str();
    } else {
        os << "(unknown)";
    }
    return os;
}

struct InputMeasurements;
struct InputMeasurement {
    /// In arbitrary units
    Point2d screen;

    /// in degrees (either field angles or longitude/latitude, depending on config option)
    LongLat viewAnglesDegrees;

    /// Line number in loaded file
    std::size_t lineNumber = 0;

    /// Provide the parent container, get a single object usable to refer to the source of this measurement.
    DataOrigin getOrigin(InputMeasurements const& parent) const;
};

struct InputMeasurements {
    /// Filename that these measurements were loaded from.
    std::string inputSource;
    /// Collection of measurements as they were loaded.
    std::vector<InputMeasurement> measurements;

    /// Is the measurement collection empty?
    bool empty() const { return measurements.empty(); }
    /// How many measurements do we have?
    std::size_t size() const { return measurements.size(); }
};

inline DataOrigin InputMeasurement::getOrigin(InputMeasurements const& parent) const {
    // Out of line to allow declaration of InputMeasurements.
    return DataOrigin{parent.inputSource, lineNumber};
}

struct NormalizedMeasurements;
struct NormalizedMeasurement {
    /// Normalized screen units, in [0, 1] in each dimension.
    Point2d screen;

    /// In arbitrary units in 3D space (in eye space: eye at 0, 0, 0, looking along -z), based on the view angles from
    /// the corresponding input measurement.
    Point3d pointFromView;

    /// Line number in loaded file
    std::size_t lineNumber;

    /// Provide the parent container, get a single object usable to refer to the source of this measurement.
    DataOrigin getOrigin(NormalizedMeasurements const& parent) const;
};

struct NormalizedMeasurements {
    /// Filename that these measurements were loaded from.
    std::string inputSource;
    /// Collection of measurements that have been normalized and transformed.
    std::vector<NormalizedMeasurement> measurements;

    /// Is the measurement collection empty?
    bool empty() const { return measurements.empty(); }
    /// How many measurements do we have?
    std::size_t size() const { return measurements.size(); }
};

inline DataOrigin NormalizedMeasurement::getOrigin(NormalizedMeasurements const& parent) const {
    // Out of line to allow declaration of NormalizedMeasurements.
    return DataOrigin{parent.inputSource, lineNumber};
}

// 3D coordinate
class XYZ {
  public:
    double x;
    double y;
    double z;

    XYZ(double px, double py, double pz) {
        x = px;
        y = py;
        z = pz;
    }
    XYZ() { x = y = z = 0; }

    /// Return the rotation about the Y axis, where 0 rotation points along
    /// the -Z axis and positive rotation heads towards the -X axis.
    /// The X axis in atan space corresponds to the -z axis in head space,
    /// and the Y axis in atan space corresponds to the -x axis in head space.
    double rotationAboutY() const { return std::atan2(-x, -z); }

    /// Project from the origin through our point onto a plane whose
    /// equation is specified.
    XYZ projectOntoPlane(double A, double B, double C, double D) const {
        XYZ ret;

        // Solve for the value of S that satisfies:
        //    Asx + Bsy + Csz + D = 0,
        //    s = -D / (Ax + By + Cz)
        // Then for the location sx, sy, sz.

        double s = -D / (A * x + B * y + C * z);
        ret.x = s * x;
        ret.y = s * y;
        ret.z = s * z;

        return ret;
    }

    /// Return the rotation distance from another point.
    double distanceFrom(const XYZ& p) const {
        return std::sqrt((x - p.x) * (x - p.x) + (y - p.y) * (y - p.y) + (z - p.z) * (z - p.z));
    }

    void debugPrint(std::ostream& os) const {
        static const auto PRECISION = 4;
        static const auto WIDTH = PRECISION + 3;
        std::ostringstream ss;
        ss << std::setprecision(PRECISION);
        ss << "(" << std::setw(WIDTH) << x;
        ss << ", " << std::setw(WIDTH) << y;
        ss << ", " << std::setw(WIDTH) << z << ")";
        os << ss.str();
    }
};

using XYZList = std::vector<XYZ>;

/// Mapping entry, along with its associated 3D coordinate
class Mapping {
  public:
    /// eye/camera space
    XYLatLong xyLatLong;
    /// Screen/world space
    XYZ xyz;

    Mapping(XYLatLong const& ll, XYZ const& x) : xyLatLong(ll), xyz(x) {}
    Mapping() = default;
};

// Description of a screen
struct ScreenDescription {
    double hFOVDegrees;
    double vFOVDegrees;
    double overlapPercent;
    double xCOP;
    double yCOP;

    // These are quantities computed along the way to getting the
    // screen that are needed by the mesh calculations, so they
    // are stored in the screen to pass from the findScreen to
    // the findMesh functions.
    double A, B, C, D;           //!< Ax + By + Cz + D = 0 screen plane
    XYZ screenLeft, screenRight; //!< Left-most and right-most points on screen
    double maxY;                 //!< Maximum absolute value of Y for points on screen
};

using Plane = Eigen::Hyperplane<double, 3>;

// Invoke like PlaneA::get(myPlane) to access the A component of your plane.
struct PlaneA;
struct PlaneB;
struct PlaneC;
struct PlaneD;
namespace detail {
template <typename T> struct PlaneCoefficientIndexTrait;
template <> struct PlaneCoefficientIndexTrait<PlaneA> : std::integral_constant<std::size_t, 0> {};

template <> struct PlaneCoefficientIndexTrait<PlaneB> : std::integral_constant<std::size_t, 1> {};

template <> struct PlaneCoefficientIndexTrait<PlaneC> : std::integral_constant<std::size_t, 2> {};

template <> struct PlaneCoefficientIndexTrait<PlaneD> : std::integral_constant<std::size_t, 3> {};

template <typename Derived> struct PlaneAccessProxy {
  public:
    /// Const accessor for this coefficient of the plane equation.
    static double get(Plane const& p) { return p.coeffs()[PlaneCoefficientIndexTrait<Derived>()]; }
};
} // namespace detail

struct PlaneA : detail::PlaneAccessProxy<PlaneA> {};
struct PlaneB : detail::PlaneAccessProxy<PlaneB> {};
struct PlaneC : detail::PlaneAccessProxy<PlaneC> {};
struct PlaneD : detail::PlaneAccessProxy<PlaneD> {};

inline Eigen::Vector3d toEigen(XYZ const& p) { return Eigen::Vector3d(p.x, p.y, p.z); }
inline XYZ toXYZ(Eigen::Vector3d const& p) { return XYZ{p.x(), p.y(), p.z()}; }

/// Output from find_screen that is used to generate the configuration
struct ProjectionDescription {
    double hFOVDegrees;
    double vFOVDegrees;
    double overlapPercent = 100.;
    /// Center of Projection
    Point2d cop = {0.5, 0.5};
};
/// Output from find_screen that is only needed by the mesh computation.
struct ScreenDetails {
    Plane screenPlane;               //!< Ax + By + Cz + D = 0 screen plane
    Point3d screenLeft, screenRight; //!< Left-most and right-most points on screen
    double maxY;                     //!< Maximum absolute value of Y for points on screen
};

using MeshDescriptionRow = std::array< //!< 2-vector of from, to coordinates
    std::array<double, 2>,             //!< 2-vector of unit coordinates (x,y)
    2>;
/// Holds a list of mappings from physical-display normalized
/// coordinates to canonical-display normalized coordinates.
typedef std::vector< //!< Vector of mappings
    MeshDescriptionRow>
    MeshDescription;

template <typename T> class InclusiveBounds {
  public:
    using value_type = T;
    InclusiveBounds() = default;
    InclusiveBounds(value_type minVal, value_type maxVal) : valid_(true), minVal_(minVal), maxVal_(maxVal) {
        if (maxVal_ < minVal_) {
            using std::swap;
            swap(minVal_, maxVal_);
        }
    }
    explicit operator bool() const { return valid_; }
    bool contains(value_type val) const { return (!valid_) || (val >= minVal_ && val <= maxVal_); }
    bool outside(value_type val) const { return valid_ && (val < minVal_ || val > maxVal_); }

    value_type getMin() const { return minVal_; }
    value_type getMax() const { return maxVal_; }

  private:
    bool valid_ = false;
    value_type minVal_;
    value_type maxVal_;
};
template <typename T> inline std::ostream& operator<<(std::ostream& os, InclusiveBounds<T> const& bounds) {
    std::ostringstream oss;
    oss << "[";
    if (bounds) {
        const auto oldFlags = oss.flags();
        const auto streamFlags = os.flags();
        oss.flags(streamFlags);
        oss.precision(os.precision());
        oss << bounds.getMin();
        oss.flags(oldFlags);
        oss << ", ";
        oss.precision(os.precision());
        oss.flags(streamFlags);
        oss << bounds.getMax();
        oss.flags(oldFlags);
    } else {
        oss << "unbounded";
    }
    oss << "]";
    os << oss.str();
    return os;
}

using InclusiveBoundsd = InclusiveBounds<double>;
using InclusiveBoundsf = InclusiveBounds<float>;

template <typename T> struct XYInclusiveBounds {
    // Do we have any bounds?
    explicit operator bool() const { return static_cast<bool>(x) || static_cast<bool>(y); }
    InclusiveBounds<T> x;
    InclusiveBounds<T> y;
};

template <typename T> inline std::ostream& operator<<(std::ostream& os, XYInclusiveBounds<T> const& xyBounds) {
    if (!xyBounds) {
        os << "unbounded";
        return os;
    }
    if (xyBounds.x) {
        os << "x: " << xyBounds.x;
    }
    if (xyBounds.x && xyBounds.y) {
        os << ", ";
    }
    if (xyBounds.y) {
        os << "y: " << xyBounds.y;
    }
    return os;
}

using XYInclusiveBoundsd = XYInclusiveBounds<double>;
using XYInclusiveBoundsf = XYInclusiveBounds<float>;
