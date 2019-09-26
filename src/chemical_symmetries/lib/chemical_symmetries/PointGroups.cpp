/*!@file
 * @copyright ETH Zurich, Laboratory for Physical Chemistry, Reiher Group.
 *   See LICENSE.txt
 */

#include "chemical_symmetries/PointGroups.h"

#include "temple/Functional.h"
#include "boost/optional.hpp"
#include <Eigen/Geometry>

namespace Scine {
namespace Symmetry {
namespace elements {

template<typename EnumType>
constexpr auto underlying(const EnumType e) {
  return static_cast<std::underlying_type_t<EnumType>>(e);
}

inline bool collinear(const Eigen::Vector3d& a, const Eigen::Vector3d& b) {
  return std::fabs(std::fabs(a.dot(b) / (a.norm() * b.norm())) - 1) <= 1e-8;
}

inline bool orthogonal(const Eigen::Vector3d& a, const Eigen::Vector3d& b) {
  return std::fabs(a.dot(b) / (a.norm() * b.norm())) <= 1e-8;
}

SymmetryElement::Matrix Identity::matrix() const {
  return Matrix::Identity();
}

boost::optional<SymmetryElement::Vector> Identity::vector() const {
  return boost::none;
}

std::string Identity::name() const {
  return "E";
}

SymmetryElement::Matrix Inversion::matrix() const {
  return -Matrix::Identity();
}

boost::optional<SymmetryElement::Vector> Inversion::vector() const {
  return boost::none;
}

std::string Inversion::name() const {
  return "i";
}

Rotation::Rotation(
  const Eigen::Vector3d& passAxis,
  const unsigned passN,
  const unsigned passPower,
  const bool passReflect
) : axis(passAxis.normalized()),
    n(passN),
    power(passPower),
    reflect(passReflect)
{}

Rotation Rotation::Cn(const Eigen::Vector3d& axis, const unsigned n, const unsigned power) {
  return Rotation(axis, n, power, false);
}

Rotation Rotation::Sn(const Eigen::Vector3d& axis, const unsigned n, const unsigned power) {
  return Rotation(axis, n, power, true);
}

Rotation Rotation::operator * (const Rotation& rhs) const {
  if(collinear(axis, rhs.axis)) {
    if(n == rhs.n) {
      return Rotation(axis, n, power + rhs.power, reflect xor rhs.reflect);
    } else {
      throw std::logic_error("Rotation data model cannot handle collinear multiplication of axes of different order n");
    }
  } else if(orthogonal(axis, rhs.axis)) {
    // Rotate rhs' axis by *this, but keep everything else
    return Rotation(matrix() * rhs.axis, rhs.n, rhs.power, rhs.reflect);
  } else {
    throw std::logic_error("Rotation data model cannot handle non-orthogonal multiplication of rotations");
  }
}

SymmetryElement::Matrix Rotation::matrix() const {
  if(!reflect) {
    return Eigen::AngleAxisd(2 * M_PI * power / n, axis).toRotationMatrix();
  }

  const double angle = 2 * M_PI * power / n;
  const double sine = std::sin(angle);
  const double cosine = std::cos(angle);
  const double onePlusCosine = 1 + cosine;

  const double xx = cosine - axis(0) * axis(0) * onePlusCosine;
  const double yy = cosine - axis(1) * axis(1) * onePlusCosine;
  const double zz = cosine - axis(2) * axis(2) * onePlusCosine;

  const double xy = - axis(0) * axis(1) * onePlusCosine;
  const double xz = - axis(0) * axis(2) * onePlusCosine;
  const double yz = - axis(1) * axis(2) * onePlusCosine;

  const double x = axis(0) * sine;
  const double y = axis(1) * sine;
  const double z = axis(2) * sine;

  Eigen::Matrix3d rotationMatrix;

  rotationMatrix <<
        xx, xy - z, xz + y,
    xy + z,     yy, yz - x,
    xz - y, yz + x,     zz;

  return rotationMatrix;
}

boost::optional<SymmetryElement::Vector> Rotation::vector() const {
  return axis;
}

std::string Rotation::name() const {
  std::string composite = (reflect ? "S" : "C");
  composite += std::to_string(n);
  if(power > 1) {
    composite += "^" + std::to_string(power);
  }
  if(std::fabs(axis.z()) < 1e-8) {
    composite += "'";
  } else if(std::fabs(axis.x()) + std::fabs(axis.y()) > 1e-8) {
    composite += (
      " along {"
      + std::to_string(axis.x()) + ", "
      + std::to_string(axis.y()) + ", "
      + std::to_string(axis.z()) + "}"
    );
  }
  return composite;
}

Reflection::Reflection(const Eigen::Vector3d& passNormal) : normal(passNormal.normalized()) {}

SymmetryElement::Matrix Reflection::matrix() const {
  Eigen::Matrix3d reflection;

  const double normalSquareNorm = normal.squaredNorm();

  for(unsigned i = 0; i < 3; ++i) {
    for(unsigned j = 0; j < 3; ++j) {
      reflection(i, j) = (i == j ? 1 : 0) - 2 * normal(i) * normal(j) / normalSquareNorm;
    }
  }

  return reflection;
}

boost::optional<SymmetryElement::Vector> Reflection::vector() const {
  if(orthogonal(normal, Eigen::Vector3d::UnitZ())) {
    return normal.cross(Eigen::Vector3d::UnitZ());
  }

  if(orthogonal(normal, Eigen::Vector3d::UnitX())) {
    return normal.cross(Eigen::Vector3d::UnitX());
  }

  if(orthogonal(normal, Eigen::Vector3d::UnitY())) {
    return normal.cross(Eigen::Vector3d::UnitY());
  }

  return boost::none;
}

std::string Reflection::name() const {
  std::string composite = "sigma";

  if(normal.cwiseAbs().isApprox(Eigen::Vector3d::UnitZ(), 1e-8)) {
    composite += "_h";
  } else if(orthogonal(normal, Eigen::Vector3d::UnitZ())) {
    composite += "_v";
  } else {
    composite += (
      " w/ normal {"
      + std::to_string(normal.x()) + ", "
      + std::to_string(normal.y()) + ", "
      + std::to_string(normal.z()) + "}"
    );
  }

  if(normal.cwiseAbs().isApprox(Eigen::Vector3d::UnitX(), 1e-8)) {
    composite += " (yz)";
  } else if(normal.cwiseAbs().isApprox(Eigen::Vector3d::UnitY(), 1e-8)) {
    composite += " (xz)";
  }

  return composite;
}

Rotation operator * (const Rotation& rot, const Reflection& reflection) {
  if(!collinear(rot.axis, reflection.normal)) {
    throw std::logic_error("Cannot handle off-axis Rotation / Reflection combination");
  }

  return Rotation(rot.axis, rot.n, rot.power, !rot.reflect);
}

Rotation operator * (const Reflection& reflection, const Rotation& rot) {
  return rot * reflection;
}

Eigen::Matrix3d improperRotationMatrix(
  const Eigen::Vector3d& axis,
  const double angle
) {
  const double sine = std::sin(angle);
  const double cosine = std::cos(angle);
  const double onePlusCosine = 1 + cosine;

  const double xx = cosine - axis(0) * axis(0) * onePlusCosine;
  const double yy = cosine - axis(1) * axis(1) * onePlusCosine;
  const double zz = cosine - axis(2) * axis(2) * onePlusCosine;

  const double xy = - axis(0) * axis(1) * onePlusCosine;
  const double xz = - axis(0) * axis(2) * onePlusCosine;
  const double yz = - axis(1) * axis(2) * onePlusCosine;

  const double x = axis(0) * sine;
  const double y = axis(1) * sine;
  const double z = axis(2) * sine;

  Eigen::Matrix3d rotationMatrix;

  rotationMatrix <<
        xx, xy - z, xz + y,
    xy + z,     yy, yz - x,
    xz - y, yz + x,     zz;

  return rotationMatrix;
}

Eigen::Matrix3d properRotationMatrix(
  const Eigen::Vector3d& axis,
  const double angle
) {
  return Eigen::AngleAxisd(angle, axis).toRotationMatrix();
}

Eigen::Matrix3d reflectionMatrix(const Eigen::Vector3d& planeNormal) {
  Eigen::Matrix3d reflection;

  const double normalSquareNorm = planeNormal.squaredNorm();

  for(unsigned i = 0; i < 3; ++i) {
    for(unsigned j = 0; j < 3; ++j) {
      reflection(i, j) = (i == j ? 1 : 0) - 2 * planeNormal(i) * planeNormal(j) / normalSquareNorm;
    }
  }

  return reflection;
}

//! Returns all symmetry elements of a point group
std::vector<std::unique_ptr<SymmetryElement>> symmetryElements(PointGroup group) noexcept {
  if(group == PointGroup::Cinfv) {
    group = PointGroup::C8v;
  }

  if(group == PointGroup::Dinfh) {
    group = PointGroup::D8h;
  }

  auto make = [](auto element) {
    using Type = decltype(element);
    return std::make_unique<Type>(element);
  };

  Identity E {};
  Inversion inversion {};

  const auto e_x = Eigen::Vector3d::UnitX();
  const auto e_y = Eigen::Vector3d::UnitY();
  const auto e_z = Eigen::Vector3d::UnitZ();

  Reflection sigma_xy {e_z};
  Reflection sigma_xz {e_y};
  Reflection sigma_yz {e_x};

  const double tetrahedronAngle = 2 * std::atan(std::sqrt(2));

  auto addProperAxisElements = [&](ElementsList& list, const Eigen::Vector3d& axis, const unsigned n) {
    // C2 gives only a C2, but C3 should also give a C3², etc.
    const Rotation element = Rotation::Cn(axis, n);
    Rotation composite = element;
    for(unsigned i = n; i > 1; --i) {
      list.push_back(make(composite));
      composite = element * composite;
    }
  };

  auto addImproperAxisElements = [&](ElementsList& list, const Eigen::Vector3d& axis, const unsigned n) {
    const Rotation element = Rotation::Sn(axis, n);
    Rotation composite = element;
    for(unsigned i = n; i > 1; --i) {
      list.push_back(make(composite));
      composite = element * composite;
    }
  };

  ElementsList elements;
  elements.push_back(make(E));

  switch(group) {
    case(PointGroup::C1):
      return elements;

    case(PointGroup::Ci):
      {
        elements.push_back(make(inversion));
        return elements;
      }

    case(PointGroup::Cs):
      {
        elements.push_back(make(sigma_xy));
        return elements;
      }

    case(PointGroup::C2):
    case(PointGroup::C3):
    case(PointGroup::C4):
    case(PointGroup::C5):
    case(PointGroup::C6):
    case(PointGroup::C7):
    case(PointGroup::C8):
      {
        const unsigned n = 2 + underlying(group) - underlying(PointGroup::C2);
        addProperAxisElements(elements, e_z, n);
        assert(elements.size() == n);
        return elements;
      }

    case(PointGroup::C2h):
    case(PointGroup::C3h):
    case(PointGroup::C4h):
    case(PointGroup::C5h):
    case(PointGroup::C6h):
    case(PointGroup::C7h):
    case(PointGroup::C8h):
      {
        elements.push_back(make(sigma_xy));
        const unsigned n = 2 + underlying(group) - underlying(PointGroup::C2h);
        std::vector<Rotation> rotations;
        const Rotation element = Rotation::Cn(e_z, n);
        Rotation composite = element;
        for(unsigned i = n; i > 1; --i) {
          rotations.push_back(composite);
          composite = element * composite;
        }
        const unsigned S = rotations.size();
        // Add sigma_xy modified Cn axes
        for(unsigned i = 0; i < S; ++i) {
          rotations.push_back(sigma_xy * rotations.at(i));
        }
        for(auto& rotation : rotations) {
          elements.emplace_back(
            make(std::move(rotation))
          );
        }
        assert(elements.size() == 2 * n);
        return elements;
      }

    case(PointGroup::C2v):
    case(PointGroup::C3v):
    case(PointGroup::C4v):
    case(PointGroup::C5v):
    case(PointGroup::C6v):
    case(PointGroup::C7v):
    case(PointGroup::C8v):
      {
        const unsigned n = 2 + underlying(group) - underlying(PointGroup::C2v);
        addProperAxisElements(elements, e_z, n);
        // Reflection planes include z and increment by pi/n along z
        const auto rotation = Rotation::Cn(e_z, 2 * n);
        Eigen::Vector3d planeNormal = e_y;
        for(unsigned i = 0; i < n; ++i) {
          elements.push_back(
            make(Reflection(planeNormal))
          );
          planeNormal = rotation.matrix() * planeNormal;
        }
        assert(elements.size() == 2 * n);
        return elements;
      }

    case(PointGroup::S4):
    case(PointGroup::S6):
    case(PointGroup::S8):
      {
        const unsigned n = 4 + 2 * (underlying(group) - underlying(PointGroup::S4));
        addImproperAxisElements(elements, e_z, n);
        assert(elements.size() == n);
        return elements;
      }

    case(PointGroup::D2):
    case(PointGroup::D3):
    case(PointGroup::D4):
    case(PointGroup::D5):
    case(PointGroup::D6):
    case(PointGroup::D7):
    case(PointGroup::D8):
      {
        const unsigned n = 2 + underlying(group) - underlying(PointGroup::D2);
        addProperAxisElements(elements, e_z, n);
        // Dn groups have C2 axes along pi/n increments in the xy plane
        const auto rotation = Rotation::Cn(e_z, 2 * n);
        Eigen::Vector3d c2axis = e_x;
        for(unsigned i = 0; i < n; ++i) {
          elements.push_back(make(Rotation::Cn(c2axis, 2)));
          c2axis = rotation.matrix() * c2axis;
        }
        assert(elements.size() == 2 * n);
        return elements;
      }

    case(PointGroup::D2h):
    case(PointGroup::D3h):
    case(PointGroup::D4h):
    case(PointGroup::D5h):
    case(PointGroup::D6h):
    case(PointGroup::D7h):
    case(PointGroup::D8h):
      {
        elements.push_back(make(sigma_xy));
        const unsigned n = 2 + underlying(group) - underlying(PointGroup::D2h);
        elements.reserve(4 * n);
        std::vector<Rotation> rotations;
        const Rotation element = Rotation::Cn(e_z, n);
        Rotation composite = element;
        for(unsigned i = n; i > 1; --i) {
          rotations.push_back(composite);
          composite = element * composite;
        }
        const unsigned S = rotations.size();
        // Generate the S_n axes from the sigma_xy * C_n
        for(unsigned i = 0; i < S; ++i) {
          rotations.push_back(sigma_xy * rotations.at(i));
        }
        for(auto& rotation : rotations) {
          elements.emplace_back(
            make(std::move(rotation))
          );
        }

        /* Dnh groups have C2 axes along pi/n increments in the xy plane
         * and sigma_v planes perpendicular to those C2 axes
         */
        const auto rotation = Rotation::Cn(e_z, 2 * n);
        Eigen::Vector3d c2axis = e_x;
        for(unsigned i = 0; i < n; ++i) {
          elements.push_back(make(Rotation::Cn(c2axis, 2)));
          elements.push_back(
            make(Reflection(
              e_z.cross(c2axis)
            ))
          );
          c2axis = rotation.matrix() * c2axis;
        }

        assert(elements.size() == 4 * n);
        return elements;
      }

    case(PointGroup::D2d):
    case(PointGroup::D3d):
    case(PointGroup::D4d):
    case(PointGroup::D5d):
    case(PointGroup::D6d):
    case(PointGroup::D7d):
    case(PointGroup::D8d):
      {
        const unsigned n = 2 + underlying(group) - underlying(PointGroup::D2d);
        addImproperAxisElements(elements, e_z, 2 * n);
        /* C2 axes */
        const auto rotationMatrix = Rotation::Cn(e_z, 2 * n).matrix();
        Eigen::Vector3d c2axis = e_x;
        for(unsigned i = 0; i < n; ++i) {
          elements.push_back(make(Rotation::Cn(c2axis, 2)));
          c2axis = rotationMatrix * c2axis;
        }
        /* sigma_ds */
        Eigen::Vector3d planeNormal = (e_x + rotationMatrix * e_x).normalized().cross(e_z);
        for(unsigned i = 0; i < n; ++i) {
          elements.push_back(make(Reflection(planeNormal)));
          planeNormal = rotationMatrix * planeNormal;
        }
        assert(elements.size() == 4 * n);
        return elements;
      }

    /* Cubic groups */
    case(PointGroup::T):
      {
        elements.reserve(12);
        const Eigen::Vector3d axis_2 = properRotationMatrix(e_y, tetrahedronAngle) * e_z;
        const Eigen::Vector3d axis_3 = Rotation::Cn(e_z, 3).matrix() * axis_2;
        const Eigen::Vector3d axis_4 = Rotation::Cn(e_z, 3).matrix() * axis_3;
        addProperAxisElements(elements, e_z, 3);
        addProperAxisElements(elements, axis_2, 3);
        addProperAxisElements(elements, axis_3, 3);
        addProperAxisElements(elements, axis_4, 3);
        elements.push_back(make(Rotation::Cn((e_z + axis_2).normalized(), 2)));
        elements.push_back(make(Rotation::Cn((e_z + axis_3).normalized(), 2)));
        elements.push_back(make(Rotation::Cn((e_z + axis_4).normalized(), 2)));
        assert(elements.size() == 12);
        return elements;
      }

    case(PointGroup::Td):
      {
        elements.reserve(24);
        Eigen::Matrix<double, 3, Eigen::Dynamic> positions(3, 4);
        positions.col(0) = e_z;
        positions.col(1) = properRotationMatrix(e_y, tetrahedronAngle) * e_z;
        positions.col(2) = Rotation::Cn(e_z, 3).matrix() * positions.col(1);
        positions.col(3) = Rotation::Cn(e_z, 3).matrix() * positions.col(2);
        // C3 axes
        addProperAxisElements(elements, e_z, 3);
        addProperAxisElements(elements, positions.col(1), 3);
        addProperAxisElements(elements, positions.col(2), 3);
        addProperAxisElements(elements, positions.col(3), 3);
        const Eigen::Vector3d axis_12 = (e_z + positions.col(1)).normalized();
        const Eigen::Vector3d axis_13 = (e_z + positions.col(2)).normalized();
        const Eigen::Vector3d axis_14 = (e_z + positions.col(3)).normalized();
        // S4, C2, S4^3
        addImproperAxisElements(elements, axis_12, 4);
        addImproperAxisElements(elements, axis_13, 4);
        addImproperAxisElements(elements, axis_14, 4);
        // Sigma d
        for(unsigned i = 0; i < 3; ++i) {
          for(unsigned j = i + 1; j < 4; ++j) {
            elements.push_back(
              make(Reflection(
                positions.col(i).cross(positions.col(j))
              ))
            );
          }
        }
        assert(elements.size() == 24);
        return elements;
      }
    case(PointGroup::Th):
      {
        // TODO
        assert(elements.size() == 24);
        return elements;
      }

    case(PointGroup::O):
      {
        // TODO
        assert(elements.size() == 24);
        return elements;
      }
    case(PointGroup::Oh):
      {
        elements.push_back(make(inversion));
        elements.reserve(48);
        /* 8 C3 and 8 S6 share the linear combinations of three axes */
        { // +++ <-> ---
          const Eigen::Vector3d axis_ppp = (  e_x + e_y + e_z).normalized();
          addProperAxisElements(elements, axis_ppp, 3);
          elements.push_back(make(Rotation::Sn(axis_ppp, 6)));
          elements.push_back(make(Rotation::Sn(-axis_ppp, 6)));
        }
        { // ++- <-> --+
          const Eigen::Vector3d axis_ppm = (  e_x + e_y - e_z).normalized();
          addProperAxisElements(elements, axis_ppm, 3);
          elements.push_back(make(Rotation::Sn(axis_ppm, 6)));
          elements.push_back(make(Rotation::Sn(-axis_ppm, 6)));
        }
        { // +-+ <-> -+-
          const Eigen::Vector3d axis_pmp = (  e_x - e_y + e_z).normalized();
          addProperAxisElements(elements, axis_pmp, 3);
          elements.push_back(make(Rotation::Sn(axis_pmp, 6)));
          elements.push_back(make(Rotation::Sn(-axis_pmp, 6)));
        }
        { // -++ <-> +--
          const Eigen::Vector3d axis_mpp = (- e_x + e_y + e_z).normalized();
          addProperAxisElements(elements, axis_mpp, 3);
          elements.push_back(make(Rotation::Sn(axis_mpp, 6)));
          elements.push_back(make(Rotation::Sn(-axis_mpp, 6)));
        }
        /* 6 C2 along linear combinations of two axes */
        elements.push_back(make(Rotation::Cn((e_x + e_y).normalized(), 2)));
        elements.push_back(make(Rotation::Cn((e_x - e_y).normalized(), 2)));
        elements.push_back(make(Rotation::Cn((e_x + e_z).normalized(), 2)));
        elements.push_back(make(Rotation::Cn((e_x - e_z).normalized(), 2)));
        elements.push_back(make(Rotation::Cn((e_y + e_z).normalized(), 2)));
        elements.push_back(make(Rotation::Cn((e_y - e_z).normalized(), 2)));
        /* 6 C4 and 3 C2 (C4^2) along axes */
        addProperAxisElements(elements, e_x, 4);
        addProperAxisElements(elements, e_y, 4);
        addProperAxisElements(elements, e_z, 4);
        /* 6 S4 along axes */
        elements.push_back(make(Rotation::Sn(  e_x, 4)));
        elements.push_back(make(Rotation::Sn(- e_x, 4)));
        elements.push_back(make(Rotation::Sn(  e_y, 4)));
        elements.push_back(make(Rotation::Sn(- e_y, 4)));
        elements.push_back(make(Rotation::Sn(  e_z, 4)));
        elements.push_back(make(Rotation::Sn(- e_z, 4)));
        /* 3 sigma h along combinations of two axes */
        elements.push_back(make(sigma_xy));
        elements.push_back(make(sigma_xz));
        elements.push_back(make(sigma_yz));
        /* 6 sigma d along linear combinations of three axes */
        elements.push_back(make(Reflection((e_x + e_y).cross(e_z))));
        elements.push_back(make(Reflection((e_x - e_y).cross(e_z))));
        elements.push_back(make(Reflection((e_x + e_z).cross(e_y))));
        elements.push_back(make(Reflection((e_x - e_z).cross(e_y))));
        elements.push_back(make(Reflection((e_y + e_z).cross(e_x))));
        elements.push_back(make(Reflection((e_y - e_z).cross(e_x))));
        assert(elements.size() == 48);
        return elements;
      }

    /* Icosahedral groups */
    case(PointGroup::I):
      {
        // TODO
        assert(elements.size() == 60);
        return elements;
      }
    case(PointGroup::Ih):
      {
        // TODO
        assert(elements.size() == 120);
        return elements;
      }

    default:
      return {};
  }
}

NPGroupingsMapType npGroupings(
  const std::vector<std::unique_ptr<SymmetryElement>>& elements
) {
  assert(elements.front()->matrix() == elements::Identity().matrix());
  const unsigned E = elements.size();

  /* There can be multiple groupings of symmetry elements of equal l for
   * different points in space. For now, we are ASSUMING that a particular
   * grouping of symmetry elements leads the folded point to lie along the
   * axis defined by the probe point used here to determine the grouping.
   *
   * So we store the point suggested by the element along with its grouping
   * so we can test this theory (in ElementGrouping).
   *
   * TODO mark this resolved after it's been confirmed
   */

  NPGroupingsMapType npGroupings;

  auto testVector = [&](const Eigen::Vector3d& v) {
    // Check if there is already a grouping for this vector
    for(const auto& iterPair : npGroupings) {
      if(temple::any_of(
        iterPair.second,
        [&v](const ElementGrouping& grouping) -> bool {
          return grouping.probePoint.isApprox(v, 1e-8);
        }
      )) {
        return;
      }
    }

    Eigen::Matrix<double, 3, Eigen::Dynamic> mappedPoints(3, E);
    mappedPoints.col(0) = v;
    unsigned np = 1;
    std::vector<
      std::vector<unsigned>
    > groups {
      {0}
    };

    for(unsigned i = 1; i < E; ++i) {
      Eigen::Vector3d mapped = elements.at(i)->matrix() * mappedPoints.col(0);
      bool found = false;
      for(unsigned j = 0; j < np; ++j) {
        if(mappedPoints.col(j).isApprox(mapped, 1e-8)) {
          found = true;
          groups.at(j).push_back(i);
          break;
        }
      }

      if(!found) {
        mappedPoints.col(np) = mapped;
        ++np;
        groups.push_back(std::vector<unsigned> {i});
      }
    }

    assert(std::is_sorted(std::begin(groups), std::end(groups)));

    ElementGrouping grouping;
    grouping.probePoint = mappedPoints.col(0);
    grouping.groups = std::move(groups);
    auto findIter = npGroupings.find(np);
    if(findIter == std::end(npGroupings)) {
      npGroupings.emplace(np, std::vector<ElementGrouping> {std::move(grouping)});
    } else {
      auto& groupingsList = findIter->second;
      if(
        !temple::any_of(
          groupingsList,
          [&grouping](const ElementGrouping& group) -> bool {
            return grouping.groups == group.groups;
          }
        )
      ) {
        groupingsList.push_back(std::move(grouping));
      }
    }
  };

  testVector(Eigen::Vector3d::UnitZ());
  testVector(Eigen::Vector3d::UnitZ() + 0.1 * Eigen::Vector3d::UnitX());
  testVector(Eigen::Vector3d::UnitX());
  testVector(Eigen::Vector3d::UnitY());

  for(const auto& elementPtr : elements) {
    if(auto axisOption = elementPtr->vector()) {
      testVector(*axisOption);
    }
  }

  return npGroupings;
}

} // namespace elements
} // namespace Symmetry
} // namespace Scine