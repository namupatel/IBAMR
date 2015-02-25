// Filename: IBStandardForceGen.h
// Created on 03 May 2005 by Boyce Griffith
//
// Copyright (c) 2002-2014, Boyce Griffith
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//
//    * Redistributions of source code must retain the above copyright notice,
//      this list of conditions and the following disclaimer.
//
//    * Redistributions in binary form must reproduce the above copyright
//      notice, this list of conditions and the following disclaimer in the
//      documentation and/or other materials provided with the distribution.
//
//    * Neither the name of The University of North Carolina nor the names of
//      its contributors may be used to endorse or promote products derived from
//      this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
// ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
// LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
// CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
// SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
// INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
// CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
// ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
// POSSIBILITY OF SUCH DAMAGE.

#ifndef included_IBStandardForceGen
#define included_IBStandardForceGen

/////////////////////////////// INCLUDES /////////////////////////////////////

#include <stddef.h>
#include <map>
#include <set>
#include <vector>

#include "ibamr/IBLagrangianForceStrategy.h"
#include "ibamr/IBSpringForceFunctions.h"
#include "ibtk/ibtk_utilities.h"
#include "petscmat.h"


namespace IBTK
{
class LData;
class LDataManager;
} // namespace IBTK
namespace SAMRAI
{
namespace hier
{

class PatchHierarchy;
} // namespace hier
} // namespace SAMRAI

/////////////////////////////// CLASS DEFINITION /////////////////////////////

namespace IBAMR
{
/*!
 * \brief Class IBStandardForceGen is a concrete IBLagrangianForceStrategy that
 * is intended to be used in conjunction with curvilinear mesh data generated by
 * class IBStandardInitializer.
 *
 * Class IBStandardForceGen provides support for linear and nonlinear spring
 * forces, linear beam forces, and target point penalty forces.
 *
 * \note By default, function default_linear_spring_force() is associated with
 * spring \a force_fcn_idx 0.  This is the default spring force function index
 * used by class IBStandardInitializer in cases in which a force function index
 * is not specified in a spring input file.  Users may override this default
 * force function with any function that implements the interface required by
 * registerSpringForceFunction().  Users may also specify additional force
 * functions that may be associated with arbitrary integer indices.
 */
class IBStandardForceGen : public IBLagrangianForceStrategy
{
public:
    /*!
     * \brief Default constructor.
     */
    IBStandardForceGen();

    /*!
     * \brief Destructor.
     */
    ~IBStandardForceGen();

    /*!
     * \brief Register a spring force specification function with the force
     * generator.
     *
     * These functions are employed to compute the force generated by a
     * particular spring for the specified displacement, spring constant, rest
     * length, and Lagrangian index.
     *
     * \note By default, function default_linear_spring_force() is associated
     * with \a force_fcn_idx 0.
     */
    void registerSpringForceFunction(int force_fcn_index,
                                     const SpringForceFcnPtr spring_force_fcn_ptr,
                                     const SpringForceDerivFcnPtr spring_force_deriv_fcn_ptr = NULL);

    /*!
     * \brief Setup the data needed to compute the forces on the specified level
     * of the patch hierarchy.
     */
    void initializeLevelData(boost::shared_ptr<SAMRAI::hier::PatchHierarchy > hierarchy,
                             int level_number,
                             double init_data_time,
                             bool initial_time,
                             IBTK::LDataManager* l_data_manager);

    /*!
     * \brief Compute the force generated by the Lagrangian structure on the
     * specified level of the patch hierarchy.
     *
     * \note Nodal forces computed by this method are \em added to the force
     * vector.
     */
    void computeLagrangianForce(boost::shared_ptr<IBTK::LData> F_data,
                                boost::shared_ptr<IBTK::LData> X_data,
                                boost::shared_ptr<IBTK::LData> U_data,
                                boost::shared_ptr<SAMRAI::hier::PatchHierarchy > hierarchy,
                                int level_number,
                                double data_time,
                                IBTK::LDataManager* l_data_manager);

    /*!
     * \brief Compute the non-zero structure of the force Jacobian matrix.
     *
     * \note Elements indices must be global PETSc indices.
     */
    void
    computeLagrangianForceJacobianNonzeroStructure(std::vector<int>& d_nnz,
                                                   std::vector<int>& o_nnz,
                                                   boost::shared_ptr<SAMRAI::hier::PatchHierarchy > hierarchy,
                                                   int level_number,
                                                   IBTK::LDataManager* l_data_manager);

    /*!
     * \brief Compute the Jacobian of the force with respect to the present
     * structure configuration.
     *
     * \note The elements of the Jacobian should be "accumulated" in the
     * provided matrix J.
     */
    void computeLagrangianForceJacobian(Mat& J_mat,
                                        MatAssemblyType assembly_type,
                                        double X_coef,
                                        boost::shared_ptr<IBTK::LData> X_data,
                                        double U_coef,
                                        boost::shared_ptr<IBTK::LData> U_data,
                                        boost::shared_ptr<SAMRAI::hier::PatchHierarchy > hierarchy,
                                        int level_number,
                                        double data_time,
                                        IBTK::LDataManager* l_data_manager);

    /*!
     * \brief Compute the potential energy with respect to the present structure
     * configuration and velocity.
     */
    double computeLagrangianEnergy(boost::shared_ptr<IBTK::LData> X_data,
                                   boost::shared_ptr<IBTK::LData> U_data,
                                   boost::shared_ptr<SAMRAI::hier::PatchHierarchy > hierarchy,
                                   int level_number,
                                   double data_time,
                                   IBTK::LDataManager* l_data_manager);

private:
    /*!
     * \brief Copy constructor.
     *
     * \note This constructor is not implemented and should not be used.
     *
     * \param from The value to copy to this object.
     */
    IBStandardForceGen(const IBStandardForceGen& from);

    /*!
     * \brief Assignment operator.
     *
     * \note This operator is not implemented and should not be used.
     *
     * \param that The value to assign to this object.
     *
     * \return A reference to this object.
     */
    IBStandardForceGen& operator=(const IBStandardForceGen& that);

    /*!
     * \name Data maintained separately for each level of the patch hierarchy.
     */
    //\{
    struct SpringData
    {
        std::vector<int> lag_mastr_node_idxs, lag_slave_node_idxs;
        std::vector<int> petsc_mastr_node_idxs, petsc_slave_node_idxs;
        std::vector<SpringForceFcnPtr> force_fcns;
        std::vector<SpringForceDerivFcnPtr> force_deriv_fcns;
        std::vector<const double*> parameters;
    };
    std::vector<SpringData> d_spring_data;

    struct BeamData
    {
        std::vector<int> petsc_mastr_node_idxs, petsc_next_node_idxs, petsc_prev_node_idxs;
        std::vector<const double*> rigidities;
        std::vector<const IBTK::Vector*> curvatures;
    };
    std::vector<BeamData> d_beam_data;

    struct TargetPointData
    {
        std::vector<int> petsc_node_idxs;
        std::vector<const double*> kappa, eta;
        std::vector<const IBTK::Point*> X0;
    };
    std::vector<TargetPointData> d_target_point_data;

    std::vector<boost::shared_ptr<IBTK::LData> > d_X_ghost_data, d_F_ghost_data, d_dX_data;
    std::vector<bool> d_is_initialized;
    //\}

    /*!
     * Spring force routines.
     */
    void initializeSpringLevelData(std::set<int>& nonlocal_petsc_idx_set,
                                   boost::shared_ptr<SAMRAI::hier::PatchHierarchy > hierarchy,
                                   int level_number,
                                   double init_data_time,
                                   bool initial_time,
                                   IBTK::LDataManager* l_data_manager);
    void computeLagrangianSpringForce(boost::shared_ptr<IBTK::LData> F_data,
                                      boost::shared_ptr<IBTK::LData> X_data,
                                      boost::shared_ptr<SAMRAI::hier::PatchHierarchy > hierarchy,
                                      int level_number,
                                      double data_time,
                                      IBTK::LDataManager* l_data_manager);

    /*!
     * Beam force routines.
     */
    void initializeBeamLevelData(std::set<int>& nonlocal_petsc_idx_set,
                                 boost::shared_ptr<SAMRAI::hier::PatchHierarchy > hierarchy,
                                 int level_number,
                                 double init_data_time,
                                 bool initial_time,
                                 IBTK::LDataManager* l_data_manager);
    void computeLagrangianBeamForce(boost::shared_ptr<IBTK::LData> F_data,
                                    boost::shared_ptr<IBTK::LData> X_data,
                                    boost::shared_ptr<SAMRAI::hier::PatchHierarchy > hierarchy,
                                    int level_number,
                                    double data_time,
                                    IBTK::LDataManager* l_data_manager);

    /*!
     * TargetPoint force routines.
     */
    void initializeTargetPointLevelData(std::set<int>& nonlocal_petsc_idx_set,
                                        boost::shared_ptr<SAMRAI::hier::PatchHierarchy > hierarchy,
                                        int level_number,
                                        double init_data_time,
                                        bool initial_time,
                                        IBTK::LDataManager* l_data_manager);
    void computeLagrangianTargetPointForce(boost::shared_ptr<IBTK::LData> F_data,
                                           boost::shared_ptr<IBTK::LData> X_data,
                                           boost::shared_ptr<IBTK::LData> U_data,
                                           boost::shared_ptr<SAMRAI::hier::PatchHierarchy > hierarchy,
                                           int level_number,
                                           double data_time,
                                           IBTK::LDataManager* l_data_manager);

    /*!
     * \brief Spring force functions.
     */
    std::map<int, SpringForceFcnPtr> d_spring_force_fcn_map;
    std::map<int, SpringForceDerivFcnPtr> d_spring_force_deriv_fcn_map;
};
} // namespace IBAMR

//////////////////////////////////////////////////////////////////////////////

#endif //#ifndef included_IBStandardForceGen
