// Filename: BoussinesqForcing.h
// Created on 24 Aug 2012 by Boyce Griffith
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

#ifndef included_BoussinesqForcing
#define included_BoussinesqForcing

/////////////////////////////// INCLUDES /////////////////////////////////////

// IBAMR INCLUDES
#include <ibamr/AdvDiffHierarchyIntegrator.h>
#include <ibamr/app_namespaces.h>

/////////////////////////////// CLASS DEFINITION /////////////////////////////

/*!
 * \brief Class BoussinesqForcing provides forcing for the momentum equations
 * based on the Boussinesq approximation to the variable-density incompressible
 * Navier-Stokes equations.
 */
class BoussinesqForcing : public CartGridFunction
{
public:
    /*!
     * \brief Class constructor.
     */
    BoussinesqForcing(boost::shared_ptr<Variable > T_var,
                      boost::shared_ptr<AdvDiffHierarchyIntegrator> adv_diff_hier_integrator,
                      int gamma);

    /*!
     * \brief Empty destructor.
     */
    ~BoussinesqForcing();

    /*!
     * \name Methods to set patch data.
     */
    //\{

    /*!
     * \brief Indicates whether the concrete BoussinesqForcing object is
     * time-dependent.
     */
    bool isTimeDependent() const;

    /*!
     * \brief Evaluate the function on the patch interiors on the specified
     * levels of the patch hierarchy.
     */
    void setDataOnPatchHierarchy(const int data_idx,
                                 boost::shared_ptr<Variable > var,
                                 boost::shared_ptr<PatchHierarchy > hierarchy,
                                 const double data_time,
                                 const bool initial_time = false,
                                 const int coarsest_ln = -1,
                                 const int finest_ln = -1);

    /*!
     * \brief Evaluate the function on the patch interior.
     */
    void setDataOnPatch(const int data_idx,
                        boost::shared_ptr<Variable > var,
                        boost::shared_ptr<Patch > patch,
                        const double data_time,
                        const bool initial_time = false,
                        boost::shared_ptr<PatchLevel > patch_level = boost::shared_ptr<PatchLevel >(NULL));

    //\}

private:
    BoussinesqForcing();

    BoussinesqForcing(const BoussinesqForcing& from);

    BoussinesqForcing& operator=(const BoussinesqForcing& that);

    boost::shared_ptr<Variable > d_T_var;
    boost::shared_ptr<AdvDiffHierarchyIntegrator> d_adv_diff_hier_integrator;
    double d_gamma;
};

//////////////////////////////////////////////////////////////////////////////

#endif //#ifndef included_BoussinesqForcing
