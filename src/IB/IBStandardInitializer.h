// Filename: IBStandardInitializer.h
// Created on 22 Nov 2006 by Boyce Griffith
//
// Copyright (c) 2002-2010 Boyce Griffith
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

#ifndef included_IBStandardInitializer
#define included_IBStandardInitializer

/////////////////////////////// INCLUDES /////////////////////////////////////

// IBTK INCLUDES
#include <ibtk/LNodeInitStrategy.h>
#include <ibtk/LagSiloDataWriter.h>
#include <ibtk/Stashable.h>

// C++ STDLIB INCLUDES
#include <map>
#include <vector>

/////////////////////////////// CLASS DEFINITION /////////////////////////////

namespace IBAMR
{
/*!
 * \brief Class IBStandardInitializer is a concrete LNodeInitStrategy that
 * initializes the configuration of one or more Lagrangian structures from input
 * files.
 *
 * \todo Document input database entries.
 *
 * \note "C-style" indices are used for all input files.
 *
 * <HR>
 *
 * <B>Vertex file format</B>
 *
 * Vertex input files end with the extension <TT>".vertex"</TT> and have the
 * following format for two spatial dimensions:
 \verbatim
 N                   # number of vertices in the file
 x_0       y_0       # (x,y)-coordinates of vertex 0
 x_1       y_1       # (x,y)-coordinates of vertex 1
 ...
 x_{N-1}   y_{N-1}   # (x,y)-coordinates of vertex N-1
 \endverbatim
 *
 * Vertex input files end with the extension <TT>".vertex"</TT> and have the
 * following format for three spatial dimensions:
 \verbatim
 N                             # number of vertices in the file
 x_0       y_0       z_0       # (x,y,z)-coordinates of vertex 0
 x_1       y_1       z_1       # (x,y,z)-coordinates of vertex 1
 ...
 x_{N-1}   y_{N-1}   z_{N-1}   # (x,y,z)-coordinates of vertex N-1
 \endverbatim
 *
 * <HR>
 *
 * <B>Spring file format</B>
 *
 * Spring input files end with the extension <TT>".spring"</TT> and have the
 * following format:
 \verbatim
 M                                            # number of links in the file
 i_0   j_0   kappa_0   length_0   fcn_idx_0   # first vertex index, second vertex index, spring constant, rest length, spring function index
 i_1   j_1   kappa_1   length_1   fcn_idx_1
 i_2   j_2   kappa_2   length_2   fcn_idx_2
 ...
 \endverbatim
 *
 * \note There is no restriction on the number of springs that may be associated
 * with any particular node of the Lagrangian mesh.
 *
 * \note The rest length and force function indices are \em optional values.  If
 * they are not provided, by default the rest length will be set to the value \a
 * 0.0 and the force function index will be set to \a 0.  This corresponds to a
 * linear spring with zero rest length.
 *
 * \note Spring specifications are used by class LagSiloDataWriter to construct
 * unstructured mesh representations of the Lagrangian structures.
 * Consequently, even if your structure does not have any springs, it may be
 * worthwhile to generate a spring input file with all spring constants set to
 * \a 0.0.
 *
 * \note \a min(i,j) is always used as the "master" node index when constructing
 * the corresponding IBSpringForceSpec object.
 *
 * \see IBSpringForceGen
 * \see IBSpringForceSpec
 *
 * <HR>
 *
 * <B> Beam file format</B>
 *
 * Beam input files end with the extension <TT>".beam"</TT> and have the
 * following format:
 \verbatim
 M                           # number of beams in the file
 i_0   j_0   k_0   kappa_0   # first vertex index, second vertex index, third vertex index, bending rigidity
 i_1   j_1   k_1   kappa_1   # first vertex index, second vertex index, third vertex index, bending rigidity
 i_2   j_2   k_2   kappa_2   # first vertex index, second vertex index, third vertex index, bending rigidity
 ...
 \endverbatim
 *
 * \note There is no restriction on the number of beams that may be associated
 * with any particular node of the Lagrangian mesh.
 *
 * \note For each bending-resistant triple \a(i,j,k), it is necessary that
 * vertex \a j correspond to an "interior" node, i.e., a node that is not the
 * first or last node in the beam.
 *
 * \note The second vertex index is always used as the "master" node index when
 * constructing the corresponding IBBeamForceSpec object.
 *
 * \see IBBeamForceGen
 * \see IBBeamForceSpec
 *
 * <HR>
 *
 * <B> Rod file format</B>
 *
 * Rod input files end with the extension <TT>".rod"</TT> and have the following
 * format:
 \verbatim
 M                                                                                          # number of rods in the file
 i_0   j_0   ds_0   a1_0   a2_0   a3_0   b1_0   b2_0   b3_0   kappa1_0   kappa2_0   tau_0   # first  vertex index, second vertex index, material parameters
 i_1   j_1   ds_1   a1_1   a2_1   a3_1   b1_1   b2_1   b3_1   kappa1_1   kappa2_1   tau_1   # second vertex index, second vertex index, material parameters
 i_2   j_2   ds_2   a1_2   a2_2   a3_2   b1_2   b2_2   b3_2   kappa1_2   kappa2_2   tau_2   # third  vertex index, second vertex index, material parameters
 ...
 \endverbatim
 *
 * \note There is no restriction on the number of rods that may be associated
 * with any particular node of the Lagrangian mesh.
 *
 * \note The first vertex index is always used as the "master" node index when
 * constructing the corresponding IBRodForceSpec object.
 *
 * \note The parameters kappa1, kappa2, and tau (the intrinsic curvatures and
 * twist of the rod) are optional.  If not provided in the input file, they are
 * assumed to be zero.
 *
 * \see IBKirchhoffRodForceGen
 * \see IBRodForceSpec
 *
 * <HR>
 *
 * <B>Target point file format</B>
 *
 * Target point input files end with the extension <TT>".target"</TT> and have
 * the following format:
 \verbatim
 M                       # number of target points in the file
 i_0   kappa_0   eta_0   # vertex index, penalty spring constant, penalty damping coefficient
 i_1   kappa_1   eta_1
 i_2   kappa_2   eta_2
 ...
 \endverbatim
 *
 * \note Target points are anchored to their \em initial positions by linear
 * springs with the specified spring constants and with zero resting lengths.
 * Consequently, target points approximately enforce internal Dirichlet boundary
 * conditions.  The penalty parameter provides control over the energetic
 * penalty imposed when the position of the Lagrangian immersed boundary point
 * deviates from that of its specified fixed location.
 *
 * \note Damping coefficients \f$ \eta \f$ are optional and are set to \a 0.0 if
 * not supplied.  Target points are "anchored" in place using Kelvin-Voigt
 * viscoelastic elements.
 *
 * \see IBTargetPointForceGen
 * \see IBTargetPointForceSpec
 *
 * <HR>
 *
 * <B>Anchor point file format</B>
 *
 * Anchor point input files end with the extension <TT>".anchor"</TT> and have
 * the following format:
 \verbatim
 M                           # number of anchor points in the file
 i_0                         # vertex index
 i_1
 i_2
 ...
 \endverbatim
 * \note Anchor points are immersed boundary nodes which are "anchored" in
 * place.  Such points neither spread force nor interpolate velocity.
 *
 * <HR>
 *
 * <B>Mass point file format</B>
 *
 * Mass point input files end with the extension <TT>".mass"</TT> and have the
 * following format:
 \verbatim
 M                           # number of mass points in the file
 i_0   mass_0   kappa_0      # vertex index, point mass, penalty spring constant
 i_1   mass_1   kappa_1
 i_2   mass_2   kappa_2
 ...
 \endverbatim
 * \note Mass points are anchored to "ghost" massive particles by linear springs
 * with the specified spring constants and with zero resting lengths.  The
 * massive particles are "isolated" and simply move according to Newton's laws.
 * The penalty parameter provides control over the energetic penalty imposed
 * when the position of the Lagrangian immersed boundary point deviates from
 * that of its massive copy.
 *
 * <HR>
 *
 * <B>Instrumentation file format</B>
 *
 * Instrumentation input files (specifying the nodes employed to determine the
 * time-dependent positions of flow meters and pressure gauges) end with the
 * extension <TT>".inst"</TT> and have the following format:
 \verbatim
 M                                      # number of instruments in the file
 meter name 0                           # meter names
 meter name 1
 meter name 2
 N                                      # number of instrumentation points in the file
 i_0   meter_idx_0   meter_node_idx_0   # vertex index, meter index, node index within meter
 i_1   meter_idx_1   meter_node_idx_1
 i_2   meter_idx_2   meter_node_idx_2
 ...
 \endverbatim
 * \note Flow meters and pressure gauges are constructed out of "rings" of
 * immersed boundary points.  The flow is computed by computing the total
 * velocity flux through a web spanning the perimeter of the flow meter.  The
 * pressure is measured at the centroid of each flow meter.
 *
 * Note that each meter may have a different number of nodes specifying its
 * perimeter; however, the values of meter_node_idx associated with a particular
 * meter must be a continuous range of integers, starting with index 0.  E.g.,
 * the following is a valid input file:
 *
 \verbatim
 2           # number of instruments in the file
 meter 0     # meter names
 meter 1
 6           # number of instrumentation points in the file
 0   0   0   # perimeter of meter 0 consists of vertices 0, 1, and 2
 1   0   1
 2   0   2
 9   1   0   # perimeter of meter 1 consists of vertices 9, 10, and 11
 10  1   1
 11  1   2
 \endverbatim
 *
 * \see IBInstrumentPanel
 * \see IBInstrumentationSpec
 *
 * <HR>
 *
 * <B>Director file format</B>
 *
 * Orthonormal director vector input files end with the extension
 * <TT>".director"</TT> and have the following format, independent of spatial
 * dimension:
 \verbatim
 N                         # number of triads in the file
 D0_x_0   D0_y_0   D0_z_0  # coordinates of director D0 associated with vertex 0
 D1_x_0   D1_y_0   D1_z_0  # coordinates of director D1 associated with vertex 0
 D2_x_0   D2_y_0   D2_z_0  # coordinates of director D2 associated with vertex 0
 D0_x_1   D0_y_1   D0_z_1  # coordinates of director D0 associated with vertex 1
 D1_x_1   D1_y_1   D1_z_1  # coordinates of director D1 associated with vertex 1
 D2_x_1   D2_y_1   D2_z_1  # coordinates of director D2 associated with vertex 1
 D0_x_2   D0_y_2   D0_z_2  # coordinates of director D0 associated with vertex 2
 D1_x_2   D1_y_2   D1_z_2  # coordinates of director D1 associated with vertex 2
 D2_x_2   D2_y_2   D2_z_2  # coordinates of director D2 associated with vertex 2
 ...
 \endverbatim
*/
class IBStandardInitializer
    : public IBTK::LNodeInitStrategy
{
public:
    /*!
     * \brief Constructor.
     */
    IBStandardInitializer(
        const std::string& object_name,
        SAMRAI::tbox::Pointer<SAMRAI::tbox::Database> input_db);

    /*!
     * \brief Destructor.
     */
    virtual
    ~IBStandardInitializer();

    /*!
     * \brief Register a Silo data writer with the IB initializer object.
     */
    void
    registerLagSiloDataWriter(
        SAMRAI::tbox::Pointer<IBTK::LagSiloDataWriter> silo_writer);

    /*!
     * \brief Determine whether there are any Lagrangian nodes on the specified
     * patch level.
     *
     * \return A boolean value indicating whether Lagrangian data is associated
     * with the given level in the patch hierarchy.
     */
    virtual bool
    getLevelHasLagrangianData(
        const int level_number,
        const bool can_be_refined) const;

    /*!
     * \brief Determine the number of local nodes on the specified patch level.
     *
     * \return The number of local nodes on the specified level.
     */
    virtual int
    getLocalNodeCountOnPatchLevel(
        const SAMRAI::tbox::Pointer<SAMRAI::hier::PatchHierarchy<NDIM> > hierarchy,
        const int level_number,
        const double init_data_time,
        const bool can_be_refined,
        const bool initial_time);

    /*!
     * \brief Initialize the structure indexing information on the patch level.
     */
    virtual void
    initializeStructureIndexingOnPatchLevel(
        std::map<int,std::string>& strct_id_to_strct_name_map,
        std::map<int,std::pair<int,int> >& strct_id_to_lag_idx_range_map,
        const int level_number,
        const double init_data_time,
        const bool can_be_refined,
        const bool initial_time,
        IBTK::LDataManager* const lag_manager);

    /*!
     * \brief Initialize the LNodeIndex and LNodeLevel data needed to specify
     * the configuration of the curvilinear mesh on the patch level.
     *
     * \return The number of local nodes initialized on the patch level.
     */
    virtual int
    initializeDataOnPatchLevel(
        const int lag_node_index_idx,
        const int global_index_offset,
        const int local_index_offset,
        SAMRAI::tbox::Pointer<IBTK::LNodeLevelData>& X_data,
        SAMRAI::tbox::Pointer<IBTK::LNodeLevelData>& U_data,
        const SAMRAI::tbox::Pointer<SAMRAI::hier::PatchHierarchy<NDIM> > hierarchy,
        const int level_number,
        const double init_data_time,
        const bool can_be_refined,
        const bool initial_time,
        IBTK::LDataManager* const lag_manager);

    /*!
     * \brief Initialize the LNodeLevel data needed to specify the mass and
     * spring constant data required by the penalty IB method.
     *
     * \return The number of local nodes initialized on the patch level.
     */
    virtual int
    initializeMassDataOnPatchLevel(
        const int global_index_offset,
        const int local_index_offset,
        SAMRAI::tbox::Pointer<IBTK::LNodeLevelData>& M_data,
        SAMRAI::tbox::Pointer<IBTK::LNodeLevelData>& K_data,
        const SAMRAI::tbox::Pointer<SAMRAI::hier::PatchHierarchy<NDIM> > hierarchy,
        const int level_number,
        const double init_data_time,
        const bool can_be_refined,
        const bool initial_time,
        IBTK::LDataManager* const lag_manager);

    /*!
     * \brief Initialize the LNodeLevel data needed to specify director vectors
     * required by some material models.
     *
     * \return The number of local nodes initialized on the patch level.
     */
    virtual int
    initializeDirectorDataOnPatchLevel(
        const int global_index_offset,
        const int local_index_offset,
        SAMRAI::tbox::Pointer<IBTK::LNodeLevelData>& D_data,
        const SAMRAI::tbox::Pointer<SAMRAI::hier::PatchHierarchy<NDIM> > hierarchy,
        const int level_number,
        const double init_data_time,
        const bool can_be_refined,
        const bool initial_time,
        IBTK::LDataManager* const lag_manager);

    /*!
     * \brief Tag cells for initial refinement.
     *
     * When the patch hierarchy is being constructed at the initial simulation
     * time, it is necessary to instruct the gridding algorithm where to place
     * local refinement in order to accommodate portions of the curvilinear mesh
     * that will reside in any yet-to-be-constructed level(s) of the patch
     * hierarchy.
     */
    virtual void
    tagCellsForInitialRefinement(
        const SAMRAI::tbox::Pointer<SAMRAI::hier::PatchHierarchy<NDIM> > hierarchy,
        const int level_number,
        const double error_data_time,
        const int tag_index);

protected:

private:
    /*!
     * \brief Default constructor.
     *
     * \note This constructor is not implemented and should not be used.
     */
    IBStandardInitializer();

    /*!
     * \brief Copy constructor.
     *
     * \note This constructor is not implemented and should not be used.
     *
     * \param from The value to copy to this object.
     */
    IBStandardInitializer(
        const IBStandardInitializer& from);

    /*!
     * \brief Assignment operator.
     *
     * \note This constructor is not implemented and should not be used.
     *
     * \param that The value to assign to this object.
     *
     * \return A reference to this object.
     */
    IBStandardInitializer&
    operator=(
        const IBStandardInitializer& that);

    /*!
     * \brief Configure the Lagrangian Silo data writer to plot the data
     * associated with the specified level of the locally refined Cartesian
     * grid.
     */
    void
    initializeLagSiloDataWriter(
        const int level_number);

    /*!
     * \brief Read the vertex data from one or more input files.
     */
    void
    readVertexFiles();

    /*!
     * \brief Read the spring data from one or more input files.
     */
    void
    readSpringFiles();

    /*!
     * \brief Read the beam data from one or more input files.
     */
    void
    readBeamFiles();

    /*!
     * \brief Read the rod data from one or more input files.
     */
    void
    readRodFiles();

    /*!
     * \brief Read the target point data from one or more input files.
     */
    void
    readTargetPointFiles();

    /*!
     * \brief Read the anchor point data from one or more input files.
     */
    void
    readAnchorPointFiles();

    /*!
     * \brief Read the boundary mass data from one or more input files.
     */
    void
    readBoundaryMassFiles();

    /*!
     * \brief Read the director data from one or more input files.
     */
    void
    readDirectorFiles();

    /*!
     * \brief Read the instrumentation data from one or more input files.
     */
    void
    readInstrumentationFiles();

    /*!
     * \brief Determine the indices of any vertices initially located within the
     * specified patch.
     */
    void
    getPatchVertices(
        std::vector<std::pair<int,int> >& point_indices,
        const SAMRAI::tbox::Pointer<SAMRAI::hier::Patch<NDIM> > patch,
        const int level_number,
        const bool can_be_refined) const;

    /*!
     * \return The canonical Lagrangian index of the specified vertex.
     */
    int
    getCanonicalLagrangianIndex(
        const std::pair<int,int>& point_index,
        const int level_number) const;

    /*!
     * \return The initial position of the specified vertex.
     */
    std::vector<double>
    getVertexPosn(
        const std::pair<int,int>& point_index,
        const int level_number) const;

    /*!
     * \return The target point penalty force spring constant associated with a
     * particular node.
     */
    double
    getVertexTargetStiffness(
        const std::pair<int,int>& point_index,
        const int level_number) const;

    /*!
     * \return The target point penalty force damping parameter associated with
     * a particular node.
     */
    double
    getVertexTargetDamping(
        const std::pair<int,int>& point_index,
        const int level_number) const;

    /*!
     * \return Boolean indicating whether a particular node is an anchor point.
     */
    bool
    getIsAnchorPoint(
        const std::pair<int,int>& point_index,
        const int level_number) const;

    /*!
     * \return The mass associated with a particular node.
     */
    double
    getVertexMass(
        const std::pair<int,int>& point_index,
        const int level_number) const;

    /*!
     * \return The mass spring constant associated with a particular node.
     */
    double
    getVertexMassStiffness(
        const std::pair<int,int>& point_index,
        const int level_number) const;

    /*!
     * \return The directors associated with a particular node.
     */
    const std::vector<double>&
    getVertexDirectors(
        const std::pair<int,int>& point_index,
        const int level_number) const;

    /*!
     * \return The instrumentation indices associated with a particular node (or
     * std::make_pair(-1,-1) if there is no instrumentation data associated with
     * that node).
     */
    std::pair<int,int>
    getVertexInstrumentationIndices(
        const std::pair<int,int>& point_index,
        const int level_number) const;

    /*!
     * \return The specification objects associated with the specified vertex.
     */
    std::vector<SAMRAI::tbox::Pointer<IBTK::Stashable> >
    initializeSpecs(
        const std::pair<int,int>& point_index,
        const int global_index_offset,
        const int level_number) const;

    /*!
     * Read input values, indicated above, from given database.
     *
     * When assertion checking is active, the database pointer must be non-null.
     */
    void
    getFromInput(
        SAMRAI::tbox::Pointer<SAMRAI::tbox::Database> db);

    /*
     * The object name is used as a handle to databases stored in restart files
     * and for error reporting purposes.
     */
    std::string d_object_name;

    /*
     * The boolean value determines whether file read batons are employed to
     * prevent multiple MPI processes from accessing the same input files
     * simultaneously.
     */
    bool d_use_file_batons;

    /*
     * The maximum number of levels in the Cartesian grid patch hierarchy and a
     * vector of boolean values indicating whether a particular level has been
     * initialized yet.
     */
    int d_max_levels;
    std::vector<bool> d_level_is_initialized;

    /*
     * An (optional) Lagrangian Silo data writer.
     */
    SAMRAI::tbox::Pointer<IBTK::LagSiloDataWriter> d_silo_writer;

    /*
     * The base filenames of the structures are used to generate unique names
     * when registering data with the Silo data writer.
     */
    std::vector<std::vector<std::string> > d_base_filename;

    /*
     * Optional shift and scale factors.
     *
     * \note These shift and scale factors are applied to ALL structures read in
     * by this reader.
     *
     * \note The scale factor is applied both to positions and to spring rest
     * lengths.
     *
     * \note The shift factor should have the same units as the positions in the
     * input files, i.e., X_final = scale*(X_initial + shift).
     */
    double d_length_scale_factor;
    std::vector<double> d_posn_shift;

    /*
     * Vertex information.
     */
    std::vector<std::vector<int> > d_num_vertex, d_vertex_offset;
    std::vector<std::vector<std::vector<double> > > d_vertex_posn;

    /*
     * Spring information.
     */
    typedef std::pair<int,int> Edge;
    struct EdgeComp
        : public std::binary_function<Edge,Edge,bool>
    {
        inline bool
        operator()(
            const Edge& e1,
            const Edge& e2) const
            {
                return (e1.first < e2.first) || (e1.first == e2.first && e1.second < e2.second);
            }
    };
    std::vector<std::vector<bool> > d_enable_springs;
    std::vector<std::vector<std::multimap<int,Edge> > > d_spring_edge_map;
    std::vector<std::vector<std::map<Edge,double,EdgeComp> > > d_spring_stiffness, d_spring_rest_length;
    std::vector<std::vector<std::map<Edge,int,EdgeComp> > > d_spring_force_fcn_idx;

    std::vector<std::vector<bool> > d_using_uniform_spring_stiffness;
    std::vector<std::vector<double> > d_uniform_spring_stiffness;

    std::vector<std::vector<bool> > d_using_uniform_spring_rest_length;
    std::vector<std::vector<double> > d_uniform_spring_rest_length;

    std::vector<std::vector<bool> > d_using_uniform_spring_force_fcn_idx;
    std::vector<std::vector<int> > d_uniform_spring_force_fcn_idx;

    /*
     * Beam information.
     */
    typedef std::pair<int,int> Neighbors;
    std::vector<std::vector<bool> > d_enable_beams;
    std::vector<std::vector<std::multimap<int,std::pair<Neighbors,std::pair<double,std::vector<double> > > > > > d_beam_specs;

    std::vector<std::vector<bool> > d_using_uniform_beam_bend_rigidity;
    std::vector<std::vector<double> > d_uniform_beam_bend_rigidity;

    /*
     * Rod information.
     */
    std::vector<std::vector<bool> > d_enable_rods;
    std::vector<std::vector<std::multimap<int,Edge> > > d_rod_edge_map;
    std::vector<std::vector<std::multimap<int,std::pair<int,std::vector<double> > > > > d_rod_specs;

    std::vector<std::vector<bool> > d_using_uniform_rod_specs;
    std::vector<std::vector<std::vector<double> > > d_uniform_rod_specs;

    /*
     * Target point information.
     */
    std::vector<std::vector<bool> > d_enable_target_points;
    std::vector<std::vector<std::vector<double> > > d_target_stiffness, d_target_damping;

    std::vector<std::vector<bool> > d_using_uniform_target_stiffness;
    std::vector<std::vector<double> > d_uniform_target_stiffness;

    std::vector<std::vector<bool> > d_using_uniform_target_damping;
    std::vector<std::vector<double> > d_uniform_target_damping;

    /*
     * Anchor point information.
     */
    std::vector<std::vector<bool> > d_enable_anchor_points;
    std::vector<std::vector<std::vector<bool> > > d_is_anchor_point;

    /*
     * Mass information for the pIB method.
     */
    std::vector<std::vector<bool> > d_enable_bdry_mass;
    std::vector<std::vector<std::vector<double> > > d_bdry_mass, d_bdry_mass_stiffness;

    std::vector<std::vector<bool> > d_using_uniform_bdry_mass;
    std::vector<std::vector<double> > d_uniform_bdry_mass;

    std::vector<std::vector<bool> > d_using_uniform_bdry_mass_stiffness;
    std::vector<std::vector<double> > d_uniform_bdry_mass_stiffness;

    /*
     * Mass information for the pIB method.
     */
    std::vector<std::vector<std::vector<std::vector<double> > > > d_directors;

    /*
     * Instrumentation information.
     */
    std::vector<std::vector<bool> > d_enable_instrumentation;
    std::vector<std::vector<std::map<int,std::pair<int,int> > > > d_instrument_idx;

    /*
     * Data required to specify connectivity information for visualization
     * purposes.
     */
    std::vector<int> d_global_index_offset;
};
}// namespace IBAMR

/////////////////////////////// INLINE ///////////////////////////////////////

//#include "IBStandardInitializer.I"

//////////////////////////////////////////////////////////////////////////////

#endif //#ifndef included_IBStandardInitializer
