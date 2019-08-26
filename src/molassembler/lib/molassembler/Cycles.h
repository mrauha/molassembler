/*! @file
 * @copyright ETH Zurich, Laboratory for Physical Chemistry, Reiher Group.
 *   See LICENSE.txt
 * @brief Class to explore cyclic structure of molecules
 *
 * Contains a wrapper class for the C-style RingDecomposerLib functions so that
 * cycle data can be used in idiomatic C++.
 */

#ifndef INCLUDE_MOLASSEMBLER_CYCLES_H
#define INCLUDE_MOLASSEMBLER_CYCLES_H

#include "RingDecomposerLib.h"
#include "boost/functional/hash.hpp"

#include "molassembler/OuterGraph.h"

#include <functional>
#include <unordered_map>

namespace Scine {

namespace molassembler {

/*!
 * @brief Wrapper class to make working with RDL in C++ more pleasant.
 *
 * Calculated data from a graph is movable, copyable and assignable in all the
 * usual ways, and is therefore suited for caching. Equality comparison of
 * this type and its nested types follows same-base, not equal-base logic of
 * comparison due to management of the C pointer types and the associated
 * allocated memory using shared_ptrs.
 */
class Cycles {
public:
  /*!
   * @brief Safe wrapper around RDL's graph and calculated data pointers
   *
   * Limited operability type to avoid any accidental moves or copies. Manages
   * memory allocation and destruction in all situations.
   */
  struct RDLDataPtrs;


  /*!
   * @brief Safe wrapper around RDL's cycle iterator and cycle pointers
   *
   * Limited operability type to avoid any accidental moves or copies. Manages
   * memory allocation and destruction in all situations. Pointer correctness
   * and iterator advancement is also provided.
   */
  struct RDLCyclePtrs;

  //! Iterator for all relevant cycles of the graph
  class AllCyclesIterator {
  public:
    // Iterator traits
    using iterator_category = std::forward_iterator_tag;
    using difference_type = std::ptrdiff_t;
    using value_type = const std::vector<BondIndex>&;
    using pointer = const std::vector<BondIndex>*;
    using reference = value_type;

    AllCyclesIterator() = delete;
    AllCyclesIterator(
      const std::shared_ptr<RDLDataPtrs>& dataPtr,
      unsigned rCycleIndex = 0
    );

    /* Rule of five members */
    AllCyclesIterator(const AllCyclesIterator& other);
    AllCyclesIterator(AllCyclesIterator&& other) noexcept;
    AllCyclesIterator& operator = (const AllCyclesIterator& other);
    AllCyclesIterator& operator = (AllCyclesIterator&& other) noexcept;
    ~AllCyclesIterator();

    AllCyclesIterator& operator ++ ();
    AllCyclesIterator operator ++ (int);
    value_type operator * () const;
    pointer operator -> () const;


    //! Must be constructed from same Cycles base and at same RC to compare equal
    bool operator == (const AllCyclesIterator& other) const;
    bool operator != (const AllCyclesIterator& other) const;

  private:
    //! Hold an owning reference to the base data to avoid dangling pointers
    std::shared_ptr<RDLDataPtrs> _rdlPtr;
    //! Manage cycle data as shared pointer to permit expected iterator functionality
    std::unique_ptr<RDLCyclePtrs> _cyclePtr;
  };

  //! Iterator for cycles of specific universal ring families
  class URFIDsCycleIterator {
  public:
    using difference_type = unsigned;
    using value_type = const std::vector<BondIndex>&;
    using pointer = const std::vector<BondIndex>*;
    using reference = value_type;
    using iterator_category = std::forward_iterator_tag;

    /* Rule of five members */
    URFIDsCycleIterator(const URFIDsCycleIterator& other);
    URFIDsCycleIterator(URFIDsCycleIterator&& other) noexcept;
    URFIDsCycleIterator& operator = (const URFIDsCycleIterator& other);
    URFIDsCycleIterator& operator = (URFIDsCycleIterator&& other) noexcept;
    ~URFIDsCycleIterator();

    URFIDsCycleIterator() = delete;

    /* Constructors */
    URFIDsCycleIterator(
      AtomIndex soughtIndex,
      const std::shared_ptr<RDLDataPtrs>& dataPtr
    );

    URFIDsCycleIterator(
      const BondIndex& soughtBond,
      const std::vector<unsigned> urfs,
      const std::shared_ptr<RDLDataPtrs>& dataPtr
    );

    URFIDsCycleIterator(
      const std::vector<BondIndex>& soughtBonds,
      const std::vector<unsigned> urfs,
      const std::shared_ptr<RDLDataPtrs>& dataPtr
    );

    URFIDsCycleIterator& operator ++ ();
    URFIDsCycleIterator operator ++ (int);
    value_type operator * () const;
    pointer operator -> () const;

    void advanceToEnd();

    bool operator == (const URFIDsCycleIterator& other) const;
    bool operator != (const URFIDsCycleIterator& other) const;

  private:
    struct URFHelper;

    std::shared_ptr<RDLDataPtrs> _rdlPtr;
    std::unique_ptr<URFHelper> _urfsPtr;
    std::unique_ptr<RDLCyclePtrs> _cyclePtr;

    void _advanceToNextPermissibleCycle();
    void _initializeCyclesFromURFID();
    void _matchCycleState(const URFIDsCycleIterator& other);
  };

//!@name Special member functions
//!@{
  /*! @brief Constructor from outer graph
   *
   * @complexity{Approximately linear in the number of bonds in cycles}
   */
  Cycles(const OuterGraph& sourceGraph, bool ignoreEtaBonds = true);
  //! @overload
  Cycles(const InnerGraph& innerGraph, bool ignoreEtaBonds = true);
//!@}

//!@name Information
//!@{
  /*! @brief Returns the number of unique ring families (URFs)
   *
   * @complexity{@math{\Theta(1)}}
   */
  unsigned numCycleFamilies() const;

  /*! @brief Returns the number of unique ring families (URFs) an index is involved in
   *
   * @complexity{@math{\Theta(U)} where @math{U} is the number of unique ring
   * families in the molecule}
   */
  unsigned numCycleFamilies(AtomIndex index) const;

  /*! @brief Returns the number of relevant cycles (RCs)
   *
   * @complexity{@math{\Theta(1)}}
   */
  unsigned numRelevantCycles() const;

  /*! @brief Returns the number of relevant cycles (RCs)
   *
   * @complexity{@math{\Theta(U)} where @math{U} is the number of unique ring
   * families in the molecule}
   */
  unsigned numRelevantCycles(AtomIndex index) const;

  //! Provide access to calculated data
  RDL_data* dataPtr() const;
//!@}

//!@name Iterators
//!@{
  AllCyclesIterator begin() const;
  AllCyclesIterator end() const;
//!@}

//!@name Ranges
//!@{
  /*! @brief Range of relevant cycles containing an atom
   *
   * @complexity{@math{O(U)} where @math{U} is the number of unique ring
   * families of the molecule}
   */
  std::pair<URFIDsCycleIterator, URFIDsCycleIterator> containing(AtomIndex atom) const;
  /*! @brief Range of relevant cycles containing a bond
   *
   * @complexity{@math{\Theta(1)}}
   */
  std::pair<URFIDsCycleIterator, URFIDsCycleIterator> containing(const BondIndex& bond) const;
  /*! @brief Range of relevant cycles containing several bonds
   *
   * @complexity{@math{\Theta(B)} where @math{B} is the number of bonds in the
   * parameters}
   */
  std::pair<URFIDsCycleIterator, URFIDsCycleIterator> containing(const std::vector<BondIndex>& bonds) const;
//!@}

//!@name Operators
//!@{
  //! Must be copy of another to compare equal. Constructed from same graph does not suffice
  bool operator == (const Cycles& other) const;
  bool operator != (const Cycles& other) const;
//!@}

private:
  std::shared_ptr<RDLDataPtrs> _rdlPtr;
  // Map form BondIndex to ordered list of its URF IDs
  std::unordered_map<BondIndex, std::vector<unsigned>, boost::hash<BondIndex>> _urfMap;
};

/*! @brief Yields the size of the smallest cycle containing an atom
 *
 * @complexity{@math{O(U + C)} where @math{U} is the number of unique ring
 * families of the molecule and @math{C} is the number of cycles containing
 * @p atom}
 *
 * @warning Do not use this a lot. Consider makeSmallestCycleMap() instead.
 */
boost::optional<unsigned> smallestCycleContaining(AtomIndex atom, const Cycles& cycles);

/*!
 * @brief Creates a mapping from atom index to the size of the smallest cycle
 *   containing that index.
 *
 * @complexity{@math{\Theta(R)} where @math{R} is the number of relevant cycles
 * in the molecule}
 *
 * @note The map does not contain entries for indices not enclosed by a cycle.
 */
std::unordered_map<AtomIndex, unsigned> makeSmallestCycleMap(const Cycles& cycleData);

/*! @brief Create cycle vertex sequence from unordered edges
 *
 * From a set of unordered graph edge descriptors, this function creates one of
 * the two possible vertex index sequences describing the cycle.
 *
 * @complexity{@math{O(E^2)} worst case}
 */
std::vector<AtomIndex> makeRingIndexSequence(
  std::vector<BondIndex> edgeDescriptors
);

/*! @brief Centralize a cycle vertex sequence at a particular vertex
 *
 * @complexity{@math{O(N)}}
 */
std::vector<AtomIndex> centralizeRingIndexSequence(
  std::vector<AtomIndex> ringIndexSequence,
  AtomIndex center
);

/*! @brief Count the number of planarity enforcing bonds
 *
 * Counts the number of planarity enforcing bonds in a set of edge descriptors.
 * Double bonds are considered planarity enforcing.
 *
 * @complexity{@math{O(N)}}
 */
unsigned countPlanarityEnforcingBonds(
  const std::vector<BondIndex>& edgeSet,
  const OuterGraph& graph
);

} // namespace molassembler

} // namespace Scine

#endif
