#ifndef INCLUDE_MOLECULE_MANIP_MOLECULE_H
#define INCLUDE_MOLECULE_MANIP_MOLECULE_H

#include "StereocenterList.h"
#include "CycleData.h"
#include "LocalGeometryModel.h"

#include "temple/constexpr/Bitmask.h"

#include "Delib/AtomCollection.h"
#include "Delib/BondOrderCollection.h"

/*! @file
 *
 * Contains the Molecule class declaration, which is the central class of the
 * library.
 */

namespace molassembler {

//!  Central class of the library, modeling a molecular graph with all state.
class Molecule {
public:
/* "Global" options */
  //! Sets the temperature regime to be used for all Molecules
  static TemperatureRegime temperatureRegime;
  //! Sets the manner in which chiral state is preserved for all Molecules
  static ChiralStatePreservation chiralStatePreservation;

/* Static functions */
  //! Calculates a bond order collection via UFF-like bond distance modelling
  static Delib::BondOrderCollection uffBondOrders(
    const Delib::AtomCollection& atomCollection
  );

  using PseudoHashType = unsigned long long;

  /*! For modifying equality comparison strictness in member modularCompare
   *
   * Differing strictnesses of comparisons may be desirable for various
   * purposes, hence a modular comparison function is provided.
   */
  enum class ComparisonComponents : unsigned {
    ElementTypes,
    BondOrders,
    Symmetries,
    Stereopermutations // Symmetries must be set in conjunction with this
  };

  //! Convolutes the atom's element type and bonds into a characteristic number
  static PseudoHashType hashAtomEnvironment(
    const temple::Bitmask<ComparisonComponents>& bitmask,
    const Delib::ElementType& elementType,
    const std::vector<BondType>& sortedBonds,
    boost::optional<Symmetry::Name> symmetryNameOptional,
    boost::optional<unsigned> assignedOptional
  );

  //! Specify the algorithm used to discretize floating-point bond orders into bond types
  enum class BondDiscretizationOption : unsigned {
    //! All bond orders >= 0.5 are considered single bonds
    Binary,
    //! Bond orders are rounded to the nearest integer
    UFF
  };

  //! Result type of an interpret call.
  struct InterpretResult {
    //! The list of individual molecules found in the 3D information
    std::vector<Molecule> molecules;
    //! A map from an index within the AtomCollection to its index in molecules member
    std::vector<unsigned> componentMap;
  };

  /*! Interpret molecules in 3D information and a bond order collection.
   *
   * The graph is inferred via bond discretization from the bond order
   * collection.
   *
   * @note Assumes that the provided atom collection's positions are in
   * Angstrom units.
   */
  static InterpretResult interpret(
    const Delib::AtomCollection& atomCollection,
    const Delib::BondOrderCollection& bondOrders,
    const BondDiscretizationOption& discretization = BondDiscretizationOption::Binary
  );

  /*! Interpret molecules in 3D information.
   *
   * The graph is inferred via bond discretization from pairwise atom distances.
   *
   * @note Assumes that the provided atom collection's positions are in
   * Angstrom units.
   */
  static InterpretResult interpret(
    const Delib::AtomCollection& atomCollection,
    const BondDiscretizationOption& discretization = BondDiscretizationOption::Binary
  );

private:
/* State */
  GraphType _adjacencies;
  StereocenterList _stereocenters;

/* Members */
/* Private members */
  /*!
   * Adds an vertex to the graph and sets it's element type property, returning
   * the new index.
   */
  AtomIndexType _addAtom(const Delib::ElementType& elementType);

  //! Generates a list of stereocenters based on graph properties alone
  StereocenterList _detectStereocenters() const;

  //! Ensures basic expectations about what constitutes a Molecule are met
  void _ensureModelInvariants() const;

  /*! Returns if an atom could be a CNStereocenter with multiple assignments
   *
   * Criteria applied are:
   * - Minimum of three adjacent indices
   * - If the high-temperature approximation is invoked, trivalent nitrogen
   *   inverts too rapidly to carry stereoinformation (unless part of a cycle
   *   of size 4 or smaller, where strain hinders inversion)
   */
  bool _isCNStereocenterCandidate(const AtomIndexType& atomIndex) const;

  /*! Returns if an edge could be an EZStereocenter with multiple assignments
   *
   * Criteria applied are:
   * - Bond type must be double
   * - 2-3 non-eta bonds for each edge vertex
   */
  bool _isEZStereocenterCandidate(const GraphType::edge_descriptor& edgeIndex) const;

  //! Returns whether the specified index is valid or not
  bool _isValidIndex(const AtomIndexType& index) const;

  /*!
   * Returns a list of edge indices where each endpoint has 1 or two additional
   * substituent besides the edge neighbor
   */
  std::vector<EdgeIndexType> _getEZStereocenterCandidates() const;

  /*!
   * Fits a stereocenter to a position collection, excluding the seesaw symmetry
   * if a four-coordinate carbon atom is to be fitted to a position collection
   */
  void _pickyFitStereocenter(
    Stereocenters::CNStereocenter& stereocenter,
    const Symmetry::Name& expectedSymmetry,
    const Delib::PositionCollection& positions
  ) const;

  //!  Reduces an atom's neighbors to ligand types
  std::vector<LocalGeometry::BindingSiteInformation> _reduceToSiteInformation(
    const AtomIndexType& index
  ) const;

  //!  Updates the molecule's StereocenterList after a graph modification
  void _propagateGraphChange();

public:
//!@name Constructors
//!@{
  //! Default-constructor creates a hydrogen molecule.
  Molecule() noexcept;

  //! Construct a minimal molecule from two element types and a shared bond type
  Molecule(
    const Delib::ElementType& a,
    const Delib::ElementType& b,
    const BondType& bondType
  ) noexcept;

  //! Constructs a molecule from connectivity alone, inferring the stereocenters
  explicit Molecule(GraphType graph);

  /*! Construct a molecule from connectivity and 3D information.
   *
   * @note Assumes that the provided position collection is in Angstrom units.
   */
  Molecule(
    GraphType graph,
    const Delib::PositionCollection& positions
  );

  //! Construct a molecule from the underlying data fragments
  Molecule(
    GraphType graph,
    StereocenterList stereocenters
  );
//!@}

//!@name Modifiers
//!@{
  //! Adds an atom by attaching it to an existing atom.
  AtomIndexType addAtom(
    const Delib::ElementType& elementType,
    const AtomIndexType& adjacentTo,
    const BondType& bondType
  );

  //! Adds a bond between existing atoms.
  void addBond(
    const AtomIndexType& a,
    const AtomIndexType& b,
    const BondType& bondType
  );

  /*! Sets the stereocenter assignment at a particular atom
   *
   * This sets the stereocenter assignment at a specific atom index. For this,
   * a stereocenter must be instantiated and contained in the StereocenterList
   * returned by getStereocenterList(). The supplied assignment must be either
   * boost::none or smaller than stereocenterPtr->numStereopermutations().
   *
   * @note Although molecules in which this occurs are infrequent, consider the
   * StereocenterList you have accessed prior to calling this function and
   * particularly any iterators thereto invalidated. This is because an
   * assignment change can trigger a ranking change, which can in turn lead
   * to the introduction of new stereocenters or the removal of old ones.
   */
  void assignStereocenter(
    const AtomIndexType& a,
    const boost::optional<unsigned>& assignment
  );

  /*! Assigns a stereocenter stereopermutation at random
   *
   * This sets the stereocetner assignment at a specific index, taking relative
   * statistical occurence weights of each stereopermutation into account. For
   * this, a stereocenter must be instantiated and contained in the
   * StereocenterList returned by getStereocenterList().
   *
   * @note Although molecules in which this occurs are infrequent, consider the
   * StereocenterList you have accessed prior to calling this function and
   * particularly any iterators thereto invalidated. This is because an
   * assignment change can trigger a ranking change, which can in turn lead
   * to the introduction of new stereocenters or the removal of old ones.
   */
  void assignStereocenterRandomly(const AtomIndexType& a);

  /*! Removes an atom from the graph, including bonds to it.
   *
   * Removes an atom from the molecular graph, including bonds to the atom,
   * after checking that removing it is safe, i.e. the removal does not
   * disconnect the graph.
   *
   * @throws if the supplied index is invalid or isSafeToRemoveAtom returns false.
   */
  void removeAtom(const AtomIndexType& a);

  /*!
   * Removes an atom after checking if removing that bond is safe, i.e. does not
   * disconnect the graph. An example of bonds that can always be removed are
   * ring-closing bonds, since they never disconnect the molecular graph.
   *
   * @throws if isSafeToRemoveBond returns false.
   *
   * @note It is not safe to remove a bond just because one of the involved
   * atoms is terminal, since that atom would then be disconnected from the
   * rest of the molecule. This function merely removes a bond from the graph.
   * It is, however, considered safe to remove the terminal vertex, which
   * involves removing the bond to it.
   */
  void removeBond(const AtomIndexType& a, const AtomIndexType& b);

  //! Changes an existing bond's type
  bool setBondType(
    const AtomIndexType& a,
    const AtomIndexType& b,
    const BondType& bondType
  );

  //! Changes an existing atom's element type
  void setElementType(
    const AtomIndexType& a,
    const Delib::ElementType& elementType
  );

  /*! Sets the local geometry at an atom index
   *
   * This sets the local geometry at a specific atom index. There are a number
   * of cases that this function treats differently, besides faulty arguments:
   * If there is already a CNStereocenter instantiated at this atom index, its
   * underlying symmetry is altered. If there is no CNStereocenter at
   * this index, one is instantiated. In all cases, new or modified
   * stereocenters are default-assigned if there is only one possible
   * assignment.
   * @throws if
   *   - the supplied atomic index is invalid
   *   - there is an EZStereocenter at that index
   *   - or the provided symmetry is a different size than that of an existing
   *     CNStereocenter or the expected symmetry
   */
  void setGeometryAtAtom(
    const AtomIndexType& a,
    const Symmetry::Name& symmetryName
  );
//!@}

//!@name Information
//!@{
  /*! Determines what the local geometry at a non-terminal atom ought to be
   *
   * Returns the expected symmetry name at a non-terminal atom.
   * @throws if the supplied atomic index is invalid
   */
  Symmetry::Name determineLocalGeometry(
    const AtomIndexType& index
  ) const;

  //! Returns a graphivz string representation of the molecule
  std::string dumpGraphviz() const;

  /*! Fetches the atomic indices of vertices adjacent to a particular index
   *
   * Fetches the atomic indices of vertices adjacent to a particular index.
   * @throws if the supplied atomic index is invalid
   */
  std::vector<AtomIndexType> getAdjacencies(const AtomIndexType& a) const;

  //! Fetches the optional bond type between two atom indices
  boost::optional<BondType> getBondType(
    const AtomIndexType& a,
    const AtomIndexType& b
  ) const;

  BondType getBondType(const GraphType::edge_descriptor& edge) const;

  CycleData getCycleData() const;

  /*! Returns the element type of an atomic index
   *
   * Returns the element type of an atomic index
   * @throws if the atomic index is invalid
   */
  Delib::ElementType getElementType(const AtomIndexType& index) const;

  //! Returns a collection detailing all element types
  Delib::ElementTypeCollection getElementCollection() const;

  //! Provides read-only access to the graph member
  const GraphType& getGraph() const;

  //! Provides read-only access to the list of stereocenters
  const StereocenterList& getStereocenterList() const;

  //! Returns the number of adjacencies of an atomic position
  unsigned getNumAdjacencies(const AtomIndexType& a) const;

  StereocenterList inferStereocentersFromPositions(
    const Delib::PositionCollection& positions
  ) const;

  //! Checks if two atomic indices are connected by a bond
  bool isAdjacent(
    const AtomIndexType& a,
    const AtomIndexType& b
  ) const;

  //! An atom is considered removable if it isn't an articulation vertex
  bool isSafeToRemoveAtom(const AtomIndexType& a) const;

  //! A bond is considered removable if it isn't a bridge edge
  bool isSafeToRemoveBond(const AtomIndexType& a, const AtomIndexType& b) const;

  /*! Modular comparison of this Molecule with another.
   *
   * This permits detailed specification of which elements of the molecular
   * information you want to use in the comparison.
   *
   * Equality comparison is performed in several stages: First, at each atom
   * position, a hash is computed that encompasses all local information that
   * is specified to be used in the comparisonBitmask. This hash is then used
   * during graph isomorphism calculation to avoid finding an isomorphism that
   * does not consider the specified factors.
   *
   * If an isomorphism is found, it is then validated. Bond orders and
   * stereocenters across both molecules are compared using the found
   * isomorphism as an index map.
   *
   * @note The number of stereopermutations that a stereocenter has is
   * considered part of the Symmetry ComparisonOptions.
   *
   * @note If you choose to discard bond order checking, this merely
   * deactivates bond order hashing and a post-isomorphism-search bond order
   * re-check. Bond order information - if present in the molecule prior to
   * calling this function - is also present in stereocenter ranking information
   * and hence can influence the number of stereopermutations and the currently
   * set stereopermutation index. This can lead to unexpected but logically
   * consistent comparison behavior.
   */
  bool modularCompare(
    const Molecule& other,
    const temple::Bitmask<ComparisonComponents>& comparisonBitmask
  ) const;

  //! Fetch the number of atoms
  unsigned numAtoms() const;

  //! Fetch the number of bonds
  unsigned numBonds() const;

  RankingInformation rankPriority(
    const AtomIndexType& a,
    const std::set<AtomIndexType>& excludeAdjacent = {},
    const boost::optional<Delib::PositionCollection>& positionsOption = boost::none
  ) const;

  //! Get the vertex indices on both ends of a graph edge
  std::array<AtomIndexType, 2> vertices(const GraphType::edge_descriptor& edge) const;
//!@}

//!@name Iterators
//!@{
  /*! Returns a range-for temporary object allowing c++11 style for loop
   * iteration through an atom's adjacencies
   */
  RangeForTemporary<GraphType::adjacency_iterator> iterateAdjacencies(
    const AtomIndexType& a
  ) const;

  /*! Returns a range-for temporary object allowing c++11-style for loop
   * iteration through edges
   */
  RangeForTemporary<GraphType::edge_iterator> iterateEdges() const;

  /*! Returns a range-for temporary object allowing c++11-style for loop
   * iteration through edges around a specific atom
   */
  RangeForTemporary<GraphType::out_edge_iterator> iterateEdges(const AtomIndexType& a) const;
//!@}


//!@name Operators
//!@{
  //! Returns the adjacencies of the specified atom index
  RangeForTemporary<GraphType::adjacency_iterator> operator [] (
    const AtomIndexType& a
  ) const;

  //! Equality operator, performs most strict equality comparison
  bool operator == (const Molecule& other) const;
  bool operator != (const Molecule& other) const;
//!@}

/* Friends */
  friend struct MoleculeValidator;
  friend class RankingTree;
};

} // namespace molassembler

std::ostream& operator << (
  std::ostream& os,
  const molassembler::Molecule& molecule
);

#endif
