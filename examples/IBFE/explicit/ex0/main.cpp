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

// Config files
#include <IBAMR_config.h>
#include <IBTK_config.h>
#include <SAMRAI_config.h>

// Headers for basic PETSc functions
#include <petscsys.h>

// Headers for basic SAMRAI objects
#include <BergerRigoutsos.h>
#include <CartesianGridGeometry.h>
#include <ChopAndPackLoadBalancer.h>
#include <StandardTagAndInitialize.h>

// Headers for basic libMesh objects
#include <libmesh/equation_systems.h>
#include <libmesh/exodusII_io.h>
#include <libmesh/mesh.h>
#include <libmesh/mesh_generation.h>
#include <libmesh/periodic_boundary.h>

// Headers for application-specific algorithm/data structure objects
#include <boost/multi_array.hpp>
#include <ibamr/IBExplicitHierarchyIntegrator.h>
#include <ibamr/IBFEMethod.h>
#include <ibamr/INSCollocatedHierarchyIntegrator.h>
#include <ibamr/INSStaggeredHierarchyIntegrator.h>
#include <ibamr/app_namespaces.h>
#include <ibtk/AppInitializer.h>
#include <ibtk/libmesh_utilities.h>
#include <ibtk/muParserCartGridFunction.h>
#include <ibtk/muParserRobinBcCoefs.h>

// Elasticity model data.
namespace ModelData
{
// Problem parameters.
static const double R = 0.25;
static const double w = 0.0625;
static const double gamma = 0.0;
static const double mu = 1.0;

// Coordinate mapping function.
void coordinate_mapping_function(libMesh::Point& X, const libMesh::Point& s, void* /*ctx*/)
{
    X(0) = (R + s(1)) * cos(s(0) / R) + 0.5;
    X(1) = (R + gamma + s(1)) * sin(s(0) / R) + 0.5;
    return;
} // coordinate_mapping_function

// Stress tensor function.
bool smooth_case = false;
void PK1_stress_function(TensorValue<double>& PP,
                         const TensorValue<double>& FF,
                         const libMesh::Point& /*X*/,
                         const libMesh::Point& /*s*/,
                         Elem* const /*elem*/,
                         const std::vector<NumericVector<double>*>& /*system_data*/,
                         double /*time*/,
                         void* /*ctx*/)
{
    PP = (mu / w) * FF;
    if (smooth_case)
    {
        PP(0, 1) = 0.0;
        PP(1, 1) = 0.0;
    }
    return;
} // PK1_stress_function
}
using namespace ModelData;

// Function prototypes
void output_data(boost::shared_ptr<PatchHierarchy > patch_hierarchy,
                 boost::shared_ptr<INSHierarchyIntegrator> navier_stokes_integrator,
                 Mesh& mesh,
                 EquationSystems* equation_systems,
                 const int iteration_num,
                 const double loop_time,
                 const string& data_dump_dirname);

/*******************************************************************************
 * For each run, the input filename and restart information (if needed) must   *
 * be given on the command line.  For non-restarted case, command line is:     *
 *                                                                             *
 *    executable <input file name>                                             *
 *                                                                             *
 * For restarted run, command line is:                                         *
 *                                                                             *
 *    executable <input file name> <restart directory> <restart number>        *
 *                                                                             *
 *******************************************************************************/
int main(int argc, char* argv[])
{
    // Initialize libMesh, PETSc, MPI, and SAMRAI.
    LibMeshInit init(argc, argv);
    SAMRAI_MPI::setCommunicator(PETSC_COMM_WORLD);
    SAMRAI_MPI::setCallAbortInSerialInsteadOfExit();
    SAMRAIManager::startup();

    { // cleanup dynamically allocated objects prior to shutdown

        // Parse command line options, set some standard options from the input
        // file, initialize the restart database (if this is a restarted run),
        // and enable file logging.
        boost::shared_ptr<AppInitializer> app_initializer = new AppInitializer(argc, argv, "IB.log");
        boost::shared_ptr<Database> input_db = app_initializer->getInputDatabase();

        // Get various standard options set in the input file.
        const bool dump_viz_data = app_initializer->dumpVizData();
        const int viz_dump_interval = app_initializer->getVizDumpInterval();
        const bool uses_visit = dump_viz_data && app_initializer->getVisItDataWriter();
        const bool uses_exodus = dump_viz_data && !app_initializer->getExodusIIFilename().empty();
        const string exodus_filename = app_initializer->getExodusIIFilename();

        const bool dump_restart_data = app_initializer->dumpRestartData();
        const int restart_dump_interval = app_initializer->getRestartDumpInterval();
        const string restart_dump_dirname = app_initializer->getRestartDumpDirectory();

        const bool dump_postproc_data = app_initializer->dumpPostProcessingData();
        const int postproc_data_dump_interval = app_initializer->getPostProcessingDataDumpInterval();
        const string postproc_data_dump_dirname = app_initializer->getPostProcessingDataDumpDirectory();
        if (dump_postproc_data && (postproc_data_dump_interval > 0) && !postproc_data_dump_dirname.empty())
        {
            Utilities::recursiveMkdir(postproc_data_dump_dirname);
        }

        const bool dump_timer_data = app_initializer->dumpTimerData();
        const int timer_dump_interval = app_initializer->getTimerDumpInterval();

        // Create a simple FE mesh with periodic boundary conditions in the "x"
        // direction.
        //
        // Note that boundary condition data must be registered with each FE
        // system before calling IBFEMethod::initializeFEData().
        Mesh mesh(NDIM);
        const double R = 0.25;
        const double w = 0.0625;
        const double dx0 = 1.0 / 64.0;
        const double dx = input_db->getDouble("DX");
        const double MFAC = input_db->getDouble("MFAC");
        const double ds = MFAC * dx;
        string elem_type = input_db->getString("ELEM_TYPE");
        bool nested_meshes = input_db->getBoolWithDefault("CONVERGENCE_STUDY", false);
        const int n_x = nested_meshes ? round(16.0 * round(2.0 * M_PI * R / dx0 / 16.0) / MFAC) * round(dx0 / dx) :
                                        ceil(2.0 * M_PI * R / ds);
        const int n_y = nested_meshes ? round(4.0 * round(w / dx0 / 4.0) / MFAC) * round(dx0 / dx) : ceil(w / ds);
        MeshTools::Generation::build_square(
            mesh, n_x, n_y, 0.0, 2.0 * M_PI * R, 0.0, w, Utility::string_to_enum<ElemType>(elem_type));
        VectorValue<double> boundary_translation(2.0 * M_PI * R, 0.0, 0.0);
        PeriodicBoundary pbc(boundary_translation);
        pbc.myboundary = 3;
        pbc.pairedboundary = 1;

        // Configure stress tensor options.
        smooth_case = input_db->getBool("SMOOTH_CASE");

        // Create major algorithm and data objects that comprise the
        // application.  These objects are configured from the input database
        // and, if this is a restarted run, from the restart database.
        boost::shared_ptr<INSHierarchyIntegrator> navier_stokes_integrator;
        const string solver_type = app_initializer->getComponentDatabase("Main")->getString("solver_type");
        if (solver_type == "STAGGERED")
        {
            navier_stokes_integrator = new INSStaggeredHierarchyIntegrator(
                "INSStaggeredHierarchyIntegrator",
                app_initializer->getComponentDatabase("INSStaggeredHierarchyIntegrator"));
        }
        else if (solver_type == "COLLOCATED")
        {
            navier_stokes_integrator = new INSCollocatedHierarchyIntegrator(
                "INSCollocatedHierarchyIntegrator",
                app_initializer->getComponentDatabase("INSCollocatedHierarchyIntegrator"));
        }
        else
        {
            TBOX_ERROR("Unsupported solver type: " << solver_type << "\n"
                                                   << "Valid options are: COLLOCATED, STAGGERED");
        }
        boost::shared_ptr<IBFEMethod> ib_method_ops =
            new IBFEMethod("IBFEMethod",
                           app_initializer->getComponentDatabase("IBFEMethod"),
                           &mesh,
                           app_initializer->getComponentDatabase("GriddingAlgorithm")->getInteger("max_levels"));
        boost::shared_ptr<IBHierarchyIntegrator> time_integrator =
            new IBExplicitHierarchyIntegrator("IBHierarchyIntegrator",
                                              app_initializer->getComponentDatabase("IBHierarchyIntegrator"),
                                              ib_method_ops,
                                              navier_stokes_integrator);
        boost::shared_ptr<CartesianGridGeometry > grid_geometry = new CartesianGridGeometry(
            "CartesianGeometry", app_initializer->getComponentDatabase("CartesianGeometry"));
        boost::shared_ptr<PatchHierarchy > patch_hierarchy = new PatchHierarchy("PatchHierarchy", grid_geometry);
        boost::shared_ptr<StandardTagAndInitialize > error_detector =
            new StandardTagAndInitialize("StandardTagAndInitialize",
                                               time_integrator,
                                               app_initializer->getComponentDatabase("StandardTagAndInitialize"));
        boost::shared_ptr<BergerRigoutsos > box_generator = new BergerRigoutsos();
        boost::shared_ptr<ChopAndPackLoadBalancer > load_balancer =
            new ChopAndPackLoadBalancer("ChopAndPackLoadBalancer", app_initializer->getComponentDatabase("ChopAndPackLoadBalancer"));
        boost::shared_ptr<GriddingAlgorithm > gridding_algorithm =
            new GriddingAlgorithm("GriddingAlgorithm",
                                        app_initializer->getComponentDatabase("GriddingAlgorithm"),
                                        error_detector,
                                        box_generator,
                                        load_balancer);

        // Configure the IBFE solver.
        FEDataManager* fe_data_manager = ib_method_ops->getFEDataManager();
        ib_method_ops->registerInitialCoordinateMappingFunction(coordinate_mapping_function);
        ib_method_ops->registerPK1StressFunction(PK1_stress_function);

        // Create Eulerian initial condition specification objects.  These
        // objects also are used to specify exact solution values for error
        // analysis.
        boost::shared_ptr<CartGridFunction> u_init = new muParserCartGridFunction(
            "u_init", app_initializer->getComponentDatabase("VelocityInitialConditions"), grid_geometry);
        navier_stokes_integrator->registerVelocityInitialConditions(u_init);

        boost::shared_ptr<CartGridFunction> p_init = new muParserCartGridFunction(
            "p_init", app_initializer->getComponentDatabase("PressureInitialConditions"), grid_geometry);
        navier_stokes_integrator->registerPressureInitialConditions(p_init);

        // Create Eulerian boundary condition specification objects (when necessary).
        const IntVector& periodic_shift = grid_geometry->getPeriodicShift();
        vector<RobinBcCoefStrategy*> u_bc_coefs(NDIM);
        if (periodic_shift.min() > 0)
        {
            for (unsigned int d = 0; d < NDIM; ++d)
            {
                u_bc_coefs[d] = NULL;
            }
        }
        else
        {
            for (unsigned int d = 0; d < NDIM; ++d)
            {
                ostringstream bc_coefs_name_stream;
                bc_coefs_name_stream << "u_bc_coefs_" << d;
                const string bc_coefs_name = bc_coefs_name_stream.str();

                ostringstream bc_coefs_db_name_stream;
                bc_coefs_db_name_stream << "VelocityBcCoefs_" << d;
                const string bc_coefs_db_name = bc_coefs_db_name_stream.str();

                u_bc_coefs[d] = new muParserRobinBcCoefs(
                    bc_coefs_name, app_initializer->getComponentDatabase(bc_coefs_db_name), grid_geometry);
            }
            navier_stokes_integrator->registerPhysicalBoundaryConditions(u_bc_coefs);
        }

        // Create Eulerian body force function specification objects.
        if (input_db->keyExists("ForcingFunction"))
        {
            boost::shared_ptr<CartGridFunction> f_fcn = new muParserCartGridFunction(
                "f_fcn", app_initializer->getComponentDatabase("ForcingFunction"), grid_geometry);
            time_integrator->registerBodyForceFunction(f_fcn);
        }

        // Set up visualization plot file writers.
        boost::shared_ptr<VisItDataWriter > visit_data_writer = app_initializer->getVisItDataWriter();
        if (uses_visit)
        {
            time_integrator->registerVisItDataWriter(visit_data_writer);
        }
        AutoPtr<ExodusII_IO> exodus_io(uses_exodus ? new ExodusII_IO(mesh) : NULL);

        // Initialize hierarchy configuration and data on all patches.
        EquationSystems* equation_systems = fe_data_manager->getEquationSystems();
        for (unsigned int k = 0; k < equation_systems->n_systems(); ++k)
        {
            System& system = equation_systems->get_system(k);
            system.get_dof_map().add_periodic_boundary(pbc);
        }
        ib_method_ops->initializeFEData();
        time_integrator->initializePatchHierarchy(patch_hierarchy, gridding_algorithm);

        // Deallocate initialization objects.
        app_initializer.setNull();

        // Print the input database contents to the log file.
        plog << "Input database:\n";
        input_db->printClassData(plog);

        // Setup data used to determine the accuracy of the computed solution.
        VariableDatabase* var_db = VariableDatabase::getDatabase();

        const boost::shared_ptr<hier::Variable > u_var = navier_stokes_integrator->getVelocityVariable();
        const boost::shared_ptr<VariableContext> u_ctx = navier_stokes_integrator->getCurrentContext();

        const int u_idx = var_db->mapVariableAndContextToIndex(u_var, u_ctx);
        const int u_cloned_idx = var_db->registerClonedPatchDataIndex(u_var, u_idx);

        const boost::shared_ptr<hier::Variable > p_var = navier_stokes_integrator->getPressureVariable();
        const boost::shared_ptr<VariableContext> p_ctx = navier_stokes_integrator->getCurrentContext();

        const int p_idx = var_db->mapVariableAndContextToIndex(p_var, p_ctx);
        const int p_cloned_idx = var_db->registerClonedPatchDataIndex(p_var, p_idx);
        visit_data_writer->registerPlotQuantity("P error", "SCALAR", p_cloned_idx);

        const int coarsest_ln = 0;
        const int finest_ln = patch_hierarchy->getFinestLevelNumber();
        for (int ln = coarsest_ln; ln <= finest_ln; ++ln)
        {
            patch_hierarchy->getPatchLevel(ln)->allocatePatchData(u_cloned_idx);
            patch_hierarchy->getPatchLevel(ln)->allocatePatchData(p_cloned_idx);
        }

        // Write out initial visualization data.
        int iteration_num = time_integrator->getIntegratorStep();
        double loop_time = time_integrator->getIntegratorTime();
        if (dump_viz_data)
        {
            pout << "\n\nWriting visualization files...\n\n";
            if (uses_visit)
            {
                time_integrator->setupPlotData();
                visit_data_writer->writePlotData(patch_hierarchy, iteration_num, loop_time);
            }
            if (uses_exodus)
            {
                exodus_io->write_timestep(
                    exodus_filename, *equation_systems, iteration_num / viz_dump_interval + 1, loop_time);
            }
        }

        // Open streams to save volume of structure.
        ofstream volume_stream;
        if (SAMRAI_MPI::getRank() == 0)
        {
            volume_stream.open("volume.curve", ios_base::out | ios_base::trunc);
        }

        // Main time step loop.
        double loop_time_end = time_integrator->getEndTime();
        double dt = 0.0;
        while (!MathUtilities<double>::equalEps(loop_time, loop_time_end) && time_integrator->stepsRemaining())
        {
            iteration_num = time_integrator->getIntegratorStep();
            loop_time = time_integrator->getIntegratorTime();

            pout << "\n";
            pout << "+++++++++++++++++++++++++++++++++++++++++++++++++++\n";
            pout << "At beginning of timestep # " << iteration_num << "\n";
            pout << "Simulation time is " << loop_time << "\n";

            dt = time_integrator->getMaximumTimeStepSize();
            time_integrator->advanceHierarchy(dt);
            loop_time += dt;

            pout << "\n";
            pout << "At end       of timestep # " << iteration_num << "\n";
            pout << "Simulation time is " << loop_time << "\n";
            pout << "+++++++++++++++++++++++++++++++++++++++++++++++++++\n";
            pout << "\n";

            // At specified intervals, write visualization and restart files,
            // print out timer data, and store hierarchy data for post
            // processing.
            iteration_num += 1;
            const bool last_step = !time_integrator->stepsRemaining();
            if (dump_viz_data && (iteration_num % viz_dump_interval == 0 || last_step))
            {
                pout << "\nWriting visualization files...\n\n";
                if (uses_visit)
                {
                    time_integrator->setupPlotData();
                    visit_data_writer->writePlotData(patch_hierarchy, iteration_num, loop_time);
                }
                if (uses_exodus)
                {
                    exodus_io->write_timestep(
                        exodus_filename, *equation_systems, iteration_num / viz_dump_interval + 1, loop_time);
                }
            }
            if (dump_restart_data && (iteration_num % restart_dump_interval == 0 || last_step))
            {
                pout << "\nWriting restart files...\n\n";
                RestartManager::getManager()->writeRestartFile(restart_dump_dirname, iteration_num);
            }
            if (dump_timer_data && (iteration_num % timer_dump_interval == 0 || last_step))
            {
                pout << "\nWriting timer data...\n\n";
                TimerManager::getManager()->print(plog);
            }
            if (dump_postproc_data && (iteration_num % postproc_data_dump_interval == 0 || last_step))
            {
                pout << "\nWriting state data...\n\n";
                output_data(patch_hierarchy,
                            navier_stokes_integrator,
                            mesh,
                            equation_systems,
                            iteration_num,
                            loop_time,
                            postproc_data_dump_dirname);
            }

            // Compute velocity and pressure error norms.
            const int coarsest_ln = 0;
            const int finest_ln = patch_hierarchy->getFinestLevelNumber();
            for (int ln = coarsest_ln; ln <= finest_ln; ++ln)
            {
                boost::shared_ptr<PatchLevel > level = patch_hierarchy->getPatchLevel(ln);
                if (!level->checkAllocated(u_cloned_idx)) level->allocatePatchData(u_cloned_idx);
                if (!level->checkAllocated(p_cloned_idx)) level->allocatePatchData(p_cloned_idx);
            }

            pout << "\n"
                 << "+++++++++++++++++++++++++++++++++++++++++++++++++++\n"
                 << "Computing error norms.\n\n";

            u_init->setDataOnPatchHierarchy(u_cloned_idx, u_var, patch_hierarchy, loop_time);
            p_init->setDataOnPatchHierarchy(p_cloned_idx, p_var, patch_hierarchy, loop_time - 0.5 * dt);

            HierarchyMathOps hier_math_ops("HierarchyMathOps", patch_hierarchy);
            hier_math_ops.setPatchHierarchy(patch_hierarchy);
            hier_math_ops.resetLevels(coarsest_ln, finest_ln);
            const double volume = hier_math_ops.getVolumeOfPhysicalDomain();
            const int wgt_cc_idx = hier_math_ops.getCellWeightPatchDescriptorIndex();
            const int wgt_sc_idx = hier_math_ops.getSideWeightPatchDescriptorIndex();

            boost::shared_ptr<CellVariable<double> > u_cc_var = u_var;
            if (u_cc_var)
            {
                HierarchyCellDataOpsReal<double> hier_cc_data_ops(patch_hierarchy, coarsest_ln, finest_ln);
                hier_cc_data_ops.subtract(u_cloned_idx, u_idx, u_cloned_idx);
                pout << "Error in u at time " << loop_time << ":\n"
                     << "  L1-norm:  " << hier_cc_data_ops.L1Norm(u_cloned_idx, wgt_cc_idx) << "\n"
                     << "  L2-norm:  " << hier_cc_data_ops.L2Norm(u_cloned_idx, wgt_cc_idx) << "\n"
                     << "  max-norm: " << hier_cc_data_ops.maxNorm(u_cloned_idx, wgt_cc_idx) << "\n";
            }

            boost::shared_ptr<SideVariable<double> > u_sc_var = u_var;
            if (u_sc_var)
            {
                HierarchySideDataOpsReal<double> hier_sc_data_ops(patch_hierarchy, coarsest_ln, finest_ln);
                hier_sc_data_ops.subtract(u_cloned_idx, u_idx, u_cloned_idx);
                pout << "Error in u at time " << loop_time << ":\n"
                     << "  L1-norm:  " << hier_sc_data_ops.L1Norm(u_cloned_idx, wgt_sc_idx) << "\n"
                     << "  L2-norm:  " << hier_sc_data_ops.L2Norm(u_cloned_idx, wgt_sc_idx) << "\n"
                     << "  max-norm: " << hier_sc_data_ops.maxNorm(u_cloned_idx, wgt_sc_idx) << "\n";
            }

            HierarchyCellDataOpsReal<double> hier_cc_data_ops(patch_hierarchy, coarsest_ln, finest_ln);
            const double p_mean = (1.0 / volume) * hier_cc_data_ops.integral(p_idx, wgt_cc_idx);
            hier_cc_data_ops.addScalar(p_idx, p_idx, -p_mean);
            const double p_cloned_mean = (1.0 / volume) * hier_cc_data_ops.integral(p_cloned_idx, wgt_cc_idx);
            hier_cc_data_ops.addScalar(p_cloned_idx, p_cloned_idx, -p_cloned_mean);
            hier_cc_data_ops.subtract(p_cloned_idx, p_idx, p_cloned_idx);
            pout << "Error in p at time " << loop_time - 0.5 * dt << ":\n"
                 << "  L1-norm:  " << hier_cc_data_ops.L1Norm(p_cloned_idx, wgt_cc_idx) << "\n"
                 << "  L2-norm:  " << hier_cc_data_ops.L2Norm(p_cloned_idx, wgt_cc_idx) << "\n"
                 << "  max-norm: " << hier_cc_data_ops.maxNorm(p_cloned_idx, wgt_cc_idx) << "\n"
                 << "+++++++++++++++++++++++++++++++++++++++++++++++++++\n";

            // Compute the volume of the structure.
            double J_integral = 0.0;
            System& X_system = equation_systems->get_system<System>(IBFEMethod::COORDS_SYSTEM_NAME);
            NumericVector<double>* X_vec = X_system.solution.get();
            NumericVector<double>* X_ghost_vec = X_system.current_local_solution.get();
            X_vec->localize(*X_ghost_vec);
            DofMap& X_dof_map = X_system.get_dof_map();
            std::vector<std::vector<unsigned int> > X_dof_indices(NDIM);
            AutoPtr<FEBase> fe(FEBase::build(NDIM, X_dof_map.variable_type(0)));
            AutoPtr<QBase> qrule = QBase::build(QGAUSS, NDIM, FIFTH);
            fe->attach_quadrature_rule(qrule.get());
            const std::vector<double>& JxW = fe->get_JxW();
            const std::vector<std::vector<VectorValue<double> > >& dphi = fe->get_dphi();
            TensorValue<double> FF;
            boost::multi_array<double, 2> X_node;
            const MeshBase::const_element_iterator el_begin = mesh.active_local_elements_begin();
            const MeshBase::const_element_iterator el_end = mesh.active_local_elements_end();
            for (MeshBase::const_element_iterator el_it = el_begin; el_it != el_end; ++el_it)
            {
                Elem* const elem = *el_it;
                fe->reinit(elem);
                for (unsigned int d = 0; d < NDIM; ++d)
                {
                    X_dof_map.dof_indices(elem, X_dof_indices[d], d);
                }
                const int n_qp = qrule->n_points();
                get_values_for_interpolation(X_node, *X_ghost_vec, X_dof_indices);
                for (int qp = 0; qp < n_qp; ++qp)
                {
                    jacobian(FF, qp, X_node, dphi);
                    J_integral += abs(FF.det()) * JxW[qp];
                }
            }
            J_integral = SAMRAI_MPI::sumReduction(J_integral);
            if (SAMRAI_MPI::getRank() == 0)
            {
                volume_stream.precision(12);
                volume_stream.setf(ios::fixed, ios::floatfield);
                volume_stream << loop_time << " " << J_integral << endl;
            }
        }

        // Close the logging streams.
        if (SAMRAI_MPI::getRank() == 0)
        {
            volume_stream.close();
        }

        // Cleanup Eulerian boundary condition specification objects (when
        // necessary).
        for (unsigned int d = 0; d < NDIM; ++d) delete u_bc_coefs[d];

    } // cleanup dynamically allocated objects prior to shutdown

    SAMRAIManager::shutdown();
    return 0;
} // main

void output_data(boost::shared_ptr<PatchHierarchy > patch_hierarchy,
                 boost::shared_ptr<INSHierarchyIntegrator> navier_stokes_integrator,
                 Mesh& mesh,
                 EquationSystems* equation_systems,
                 const int iteration_num,
                 const double loop_time,
                 const string& data_dump_dirname)
{
    plog << "writing hierarchy data at iteration " << iteration_num << " to disk" << endl;
    plog << "simulation time is " << loop_time << endl;

    // Write Cartesian data.
    string file_name = data_dump_dirname + "/" + "hier_data.";
    char temp_buf[128];
    sprintf(temp_buf, "%05d.samrai.%05d", iteration_num, SAMRAI_MPI::getRank());
    file_name += temp_buf;
    boost::shared_ptr<HDFDatabase> hier_db = new HDFDatabase("hier_db");
    hier_db->create(file_name);
    VariableDatabase* var_db = VariableDatabase::getDatabase();
    ComponentSelector hier_data;
    hier_data.setFlag(var_db->mapVariableAndContextToIndex(navier_stokes_integrator->getVelocityVariable(),
                                                           navier_stokes_integrator->getCurrentContext()));
    hier_data.setFlag(var_db->mapVariableAndContextToIndex(navier_stokes_integrator->getPressureVariable(),
                                                           navier_stokes_integrator->getCurrentContext()));
    patch_hierarchy->putToDatabase(hier_db->putDatabase("PatchHierarchy"), hier_data);
    hier_db->putDouble("loop_time", loop_time);
    hier_db->putInteger("iteration_num", iteration_num);
    hier_db->close();

    // Write Lagrangian data.
    file_name = data_dump_dirname + "/" + "fe_mesh.";
    sprintf(temp_buf, "%05d", iteration_num);
    file_name += temp_buf;
    file_name += ".xda";
    mesh.write(file_name);
    file_name = data_dump_dirname + "/" + "fe_equation_systems.";
    sprintf(temp_buf, "%05d", iteration_num);
    file_name += temp_buf;
    equation_systems->write(file_name, (EquationSystems::WRITE_DATA | EquationSystems::WRITE_ADDITIONAL_DATA));
    return;
} // output_data
