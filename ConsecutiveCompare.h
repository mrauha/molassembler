#ifndef INCLUDE_TEMPLATE_MAGIC_CONSECUTIVE_COMPARE_H
#define INCLUDE_TEMPLATE_MAGIC_CONSECUTIVE_COMPARE_H

namespace constable {

template<class CompareFunctor, typename T>
constexpr bool consecutiveCompare(
  CompareFunctor&& comparator,
  const T& a,
  const T& b
) {
  return comparator(a, b);
  /* If comparator(a, b) returns false, then the alternatives all return false
   * anyway. There's no need to evaluate comparator(b, a).
   */
}

template<class CompareFunctor, typename T, class... CompareTriples>
constexpr bool consecutiveCompare(
  CompareFunctor&& comparator,
  const T& a,
  const T& b,
  const CompareTriples& ... compareTriples
) {
  if(comparator(a, b)) {
    return true;
  } else if(comparator(b, a)) {
    return false;
  } else {
    return consecutiveCompare(compareTriples...);
  }
}

template<typename T>
constexpr bool consecutiveCompareSmaller(
  const T& a,
  const T& b
) {
  return a < b;
}

template<typename T, class ... ComparisonPairs>
constexpr bool consecutiveCompareSmaller(
  const T& a,
  const T& b,
  const ComparisonPairs& ... comparisonPairs
) {
  if(a < b) {
    return true;
  } else if(b < a) {
    return false;
  } else {
    return consecutiveCompareSmaller(comparisonPairs...);
  }
}

} // namespace constable

#endif
