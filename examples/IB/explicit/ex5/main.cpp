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

// Headers for application-specific algorithm/data structure objects
#include <ibamr/IBExplicitHierarchyIntegrator.h>
#include <ibamr/IBMethod.h>
#include <ibamr/IBStandardForceGen.h>
#include <ibamr/IBStandardInitializer.h>
#include <ibamr/INSStaggeredHierarchyIntegrator.h>
#include <ibamr/INSStaggeredStochasticForcing.h>
#include <ibamr/RNG.h>
#include <ibamr/app_namespaces.h>
#include <ibtk/AppInitializer.h>
#include <ibtk/LData.h>
#include <ibtk/LDataManager.h>
#include <ibtk/muParserCartGridFunction.h>
#include <ibtk/muParserRobinBcCoefs.h>

// Function prototypes
void output_data(boost::shared_ptr<PatchHierarchy > patch_hierarchy,
                 boost::shared_ptr<INSHierarchyIntegrator> navier_stokes_integrator,
                 LDataManager* l_data_manager,
                 const int iteration_num,
                 const double loop_time,
                 const int output_level,
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
    // Initialize PETSc, MPI, and SAMRAI.
    PetscInitialize(&argc, &argv, NULL, NULL);
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

        const bool is_from_restart = app_initializer->isFromRestart();
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

        // Create major algorithm and data objects that comprise the
        // application.  These objects are configured from the input database
        // and, if this is a restarted run, from the restart database.
        boost::shared_ptr<INSStaggeredHierarchyIntegrator> navier_stokes_integrator = new INSStaggeredHierarchyIntegrator(
            "INSStaggeredHierarchyIntegrator",
            app_initializer->getComponentDatabase("INSStaggeredHierarchyIntegrator"));
        boost::shared_ptr<IBMethod> ib_method_ops = new IBMethod("IBMethod", app_initializer->getComponentDatabase("IBMethod"));
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

        // Configure the IB solver.
        boost::shared_ptr<IBStandardInitializer> ib_initializer = new IBStandardInitializer(
            "IBStandardInitializer", app_initializer->getComponentDatabase("IBStandardInitializer"));
        ib_method_ops->registerLInitStrategy(ib_initializer);
        boost::shared_ptr<IBStandardForceGen> ib_force_fcn = new IBStandardForceGen();
        ib_method_ops->registerIBLagrangianForceFunction(ib_force_fcn);

        // Create Eulerian initial condition specification objects.
        if (input_db->keyExists("VelocityInitialConditions"))
        {
            boost::shared_ptr<CartGridFunction> u_init = new muParserCartGridFunction(
                "u_init", app_initializer->getComponentDatabase("VelocityInitialConditions"), grid_geometry);
            navier_stokes_integrator->registerVelocityInitialConditions(u_init);
        }

        if (input_db->keyExists("PressureInitialConditions"))
        {
            boost::shared_ptr<CartGridFunction> p_init = new muParserCartGridFunction(
                "p_init", app_initializer->getComponentDatabase("PressureInitialConditions"), grid_geometry);
            navier_stokes_integrator->registerPressureInitialConditions(p_init);
        }

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

        // Create stochastic forcing function specification object.
        boost::shared_ptr<INSStaggeredStochasticForcing> f_fcn =
            new INSStaggeredStochasticForcing("INSStaggeredStochasticForcing",
                                              app_initializer->getComponentDatabase("INSStaggeredStochasticForcing"),
                                              navier_stokes_integrator);
        time_integrator->registerBodyForceFunction(f_fcn);

        // Seed the random number generator.
        int seed = 0;
        if (input_db->keyExists("SEED"))
        {
            seed = input_db->getInteger("SEED");
        }
        else
        {
            TBOX_ERROR("Key data `seed' not found in input.");
        }
        RNG::parallel_seed(seed);

        // We are primarily interested in the Lagrangian data so we can skip writing Eulerian data:
        int output_level = 1; // -1=none, 0=txt, 1=silo+txt, 2=visit+silo+txt, 3=visit+SAMRAI+silo+txt, >3=verbose
        if (input_db->keyExists("OUTPUT_LEVEL"))
        {
            output_level = input_db->getInteger("OUTPUT_LEVEL");
        }

        // Set up visualization plot file writers.
        boost::shared_ptr<VisItDataWriter > visit_data_writer = app_initializer->getVisItDataWriter();
        boost::shared_ptr<LSiloDataWriter> silo_data_writer = app_initializer->getLSiloDataWriter();
        if (uses_visit)
        {
            ib_initializer->registerLSiloDataWriter(silo_data_writer);
            time_integrator->registerVisItDataWriter(visit_data_writer);
            ib_method_ops->registerLSiloDataWriter(silo_data_writer);
        }

        // Initialize hierarchy configuration and data on all patches.
        time_integrator->initializePatchHierarchy(patch_hierarchy, gridding_algorithm);

        // Deallocate initialization objects.
        ib_method_ops->freeLInitStrategy();
        ib_initializer.setNull();
        app_initializer.setNull();

        // Print the input database contents to the log file.
        plog << "Input database:\n";
        input_db->printClassData(plog);

        // Write restart data before starting main time integration loop.
        if (dump_restart_data && !is_from_restart)
        {
            pout << "\nWriting restart files...\n\n";
            RestartManager::getManager()->writeRestartFile(restart_dump_dirname, 0);
        }

        // Write out initial visualization data.
        int iteration_num = time_integrator->getIntegratorStep();
        double loop_time = time_integrator->getIntegratorTime();
        if (dump_viz_data && uses_visit)
        {
            if (output_level >= 0)
                pout << "\nWriting visualization files at timestep # " << iteration_num << " t=" << loop_time << "\n";
            time_integrator->setupPlotData();
            if (output_level >= 2) visit_data_writer->writePlotData(patch_hierarchy, iteration_num, loop_time);
            if (output_level >= 1) silo_data_writer->writePlotData(iteration_num, loop_time);
        }
        if (dump_postproc_data)
        {
            output_data(patch_hierarchy,
                        navier_stokes_integrator,
                        ib_method_ops->getLDataManager(),
                        iteration_num,
                        loop_time,
                        output_level,
                        postproc_data_dump_dirname);
        }

        // Main time step loop.
        double loop_time_end = time_integrator->getEndTime();
        while (!MathUtilities<double>::equalEps(loop_time, loop_time_end) && time_integrator->stepsRemaining())
        {
            iteration_num = time_integrator->getIntegratorStep();
            loop_time = time_integrator->getIntegratorTime();

            if (output_level > 3)
            {
                pout << "\n";
                pout << "+++++++++++++++++++++++++++++++++++++++++++++++++++\n";
                pout << "At beginning of timestep # " << iteration_num << "\n";
                pout << "Simulation time is " << loop_time << "\n";
            }

            const double dt = time_integrator->getMaximumTimeStepSize();
            time_integrator->advanceHierarchy(dt);
            loop_time += dt;

            if (output_level > 3)
            {
                pout << "\n";
                pout << "At end       of timestep # " << iteration_num << "\n";
                pout << "Simulation time is " << loop_time << "\n";
                pout << "+++++++++++++++++++++++++++++++++++++++++++++++++++\n";
                pout << "\n";
            }

            // At specified intervals, write visualization and restart files,
            // print out timer data, and store hierarchy data for post
            // processing.
            iteration_num += 1;
            const bool last_step = !time_integrator->stepsRemaining();
            if (dump_viz_data && uses_visit && (iteration_num % viz_dump_interval == 0 || last_step))
            {
                if (output_level >= 0)
                    pout << "\nWriting visualization files at timestep # " << iteration_num << " t=" << loop_time
                         << "\n";
                time_integrator->setupPlotData();
                if (output_level >= 2) visit_data_writer->writePlotData(patch_hierarchy, iteration_num, loop_time);
                if (output_level >= 1) silo_data_writer->writePlotData(iteration_num, loop_time);
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
                output_data(patch_hierarchy,
                            navier_stokes_integrator,
                            ib_method_ops->getLDataManager(),
                            iteration_num,
                            loop_time,
                            output_level,
                            postproc_data_dump_dirname);
            }
        }

        // Cleanup Eulerian boundary condition specification objects (when
        // necessary).
        for (unsigned int d = 0; d < NDIM; ++d) delete u_bc_coefs[d];

    } // cleanup dynamically allocated objects prior to shutdown

    SAMRAIManager::shutdown();
    PetscFinalize();
    return 0;
} // main

void output_data(boost::shared_ptr<PatchHierarchy > patch_hierarchy,
                 boost::shared_ptr<INSHierarchyIntegrator> navier_stokes_integrator,
                 LDataManager* l_data_manager,
                 const int iteration_num,
                 const double loop_time,
                 const int output_level,
                 const string& data_dump_dirname)
{
    if (output_level >= 0)
    {
        pout << "\nWriting output files at timestep # " << iteration_num << " t=" << loop_time << "\n";
    }
    string file_name = data_dump_dirname + "/" + "hier_data.";
    char temp_buf[128];
    sprintf(temp_buf, "%05d.samrai.%05d", iteration_num, SAMRAI_MPI::getRank());
    file_name += temp_buf;

    // Write Cartesian data.
    if (output_level >= 3)
    {
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
    }

    // Write Lagrangian data.
    if (output_level >= 0)
    {
        const int finest_hier_level = patch_hierarchy->getFinestLevelNumber();
        boost::shared_ptr<LData> X_data = l_data_manager->getLData("X", finest_hier_level);
        Vec X_petsc_vec = X_data->getVec();
        Vec X_lag_vec;
        VecDuplicate(X_petsc_vec, &X_lag_vec);
        l_data_manager->scatterPETScToLagrangian(X_petsc_vec, X_lag_vec, finest_hier_level);
        file_name = data_dump_dirname + "/" + "X.";
        sprintf(temp_buf, "%05d", iteration_num);
        file_name += temp_buf;
        PetscViewer viewer;
        PetscViewerASCIIOpen(PETSC_COMM_WORLD, file_name.c_str(), &viewer);
        VecView(X_lag_vec, viewer);
        PetscViewerDestroy(&viewer);
        VecDestroy(&X_lag_vec);
    }
    return;
} // output_data
