#define HERMES_REPORT_INFO
#include "definitions.h"

using namespace Hermes;
using namespace Hermes::Hermes2D;

// This example shows the use of subdomains.

const bool STOKES = false;

#define PRESSURE_IN_L2                      

// Initial polynomial degree for velocity components.
const int P_INIT_VEL = 2;                         

// Initial polynomial degree for pressure.
// Note: P_INIT_VEL should always be greater than
// P_INIT_PRESSURE because of the inf-sup condition.
const int P_INIT_PRESSURE = 1;

// Reynolds number.
const double RE = 200.0;
const double TEMP_INIT = 20.0;

// Inlet velocity (reached after STARTUP_TIME).
const double VEL_INLET = 1.0;

// Domain length.
const double L = 10;

// During this time, inlet velocity increases gradually
// from 0 to VEL_INLET, then it stays constant.
const double STARTUP_TIME = 1.0;

const double TAU = 0.1;                           // Time step.
const double T_FINAL = 30000.0;                   // Time interval length.
const double NEWTON_TOL = 1e-4;                   // Stopping criterion for the Newton's method.
const int NEWTON_MAX_ITER = 10;                   // Maximum allowed number of Newton iterations.
const double H = 6;                               // Domain height (necessary to define the parabolic
                                                  // velocity profile at inlet).

// Heat source inside of the inner circle.
const double HEAT_SOURCE = 1e8;                   // Not realistic, but we want this thing to heat up quickly.

// Specific heat outside of the inner circle.
const double SPECIFIC_HEAT_OUTER = 711;           // Water.

// Specific heat inside of the inner circle.
const double SPECIFIC_HEAT_INNER = 4190;          // Graphite.

// Density outside of the inner circle.
const double RHO_OUTER = 1000;                    // Water.

// Density in the inner circle.
const double RHO_INNER = 1800;                    // Graphite.

// Thermal diffusivity ouside of the inner circle.
const double THERMAL_DIFFUSIVITY_OUTER = 1.4E-7;  // Water.

// Thermal diffusivity in the inner circle.
const double THERMAL_DIFFUSIVITY_INNER = 1.22E-3; // Graphite.

// Viscosity of the media outside of the inner circle.
const double VISCOSITY_OUTER = 8.9e-4;            // Water.

// Uniform polynomial degree of mesh elements.
const int P_INIT = 2;

// Number of initial mesh refinements.
const int INIT_REF_NUM = 3;        // Uniform.
const int INIT_REF_NUM_HOLE = 4;   // Towards the hole.

// Possibilities: SOLVER_AMESOS, SOLVER_AZTECOO, SOLVER_MUMPS,
// SOLVER_PETSC, SOLVER_SUPERLU, SOLVER_UMFPACK.
Hermes::MatrixSolverType matrix_solver_type = Hermes::SOLVER_UMFPACK;  

// Boundary markers.
const std::string BDY_INLET = "Inlet";
const std::string BDY_OUTER = "Outer";
const std::string BDY_INNER = "Inner";

int main(int argc, char* argv[])
{
  // Load the mesh.
  Mesh mesh_whole_domain, mesh_without_hole;
  Hermes::vector<Mesh*> meshes (&mesh_whole_domain, &mesh_without_hole);
  MeshReaderH2DXML mloader;
  mloader.load("subdomains.xml", meshes);

  // Perform initial mesh refinements (optional).
  for(int i = 0; i < INIT_REF_NUM; i++)
    for(unsigned int meshes_i = 0; meshes_i < meshes.size(); meshes_i++)
      meshes[meshes_i]->refine_all_elements();

  // Perform refinement towards the hole.
  for(unsigned int meshes_i = 0; meshes_i < meshes.size(); meshes_i++)
    meshes[meshes_i]->refine_towards_boundary("Inner", INIT_REF_NUM_HOLE);

  // Initialize boundary conditions.
  // Flow.
  EssentialBCNonConst bc_inlet_vel_x(BDY_INLET, VEL_INLET, H, STARTUP_TIME);
  Hermes::Hermes2D::DefaultEssentialBCConst<double> bc_other_vel_x(Hermes::vector<std::string>(BDY_OUTER, BDY_INNER), 0.0);
  Hermes::Hermes2D::EssentialBCs<double> bcs_vel_x(Hermes::vector<EssentialBoundaryCondition<double> *>(&bc_inlet_vel_x, &bc_other_vel_x));
  Hermes::Hermes2D::DefaultEssentialBCConst<double> bc_vel_y(Hermes::vector<std::string>(BDY_INLET, BDY_OUTER, BDY_INNER), 0.0);
  Hermes::Hermes2D::EssentialBCs<double> bcs_vel_y(&bc_vel_y);
  Hermes::Hermes2D::EssentialBCs<double> bcs_pressure;

  // Temperature.
  Hermes::Hermes2D::DefaultEssentialBCConst<double> bc_temperature(Hermes::vector<std::string>(BDY_INLET, BDY_OUTER), 20.0);
  EssentialBCs<double> bcs_temperature(&bc_temperature);

  // Spaces for velocity components and pressure.
  H1Space<double> xvel_space(&mesh_without_hole, &bcs_vel_x, P_INIT_VEL);
  H1Space<double> yvel_space(&mesh_without_hole, &bcs_vel_y, P_INIT_VEL);
#ifdef PRESSURE_IN_L2
  L2Space<double> p_space(&mesh_without_hole, P_INIT_PRESSURE);
#else
  H1Space<double> p_space(&mesh_without_hole, &bcs_pressure, P_INIT_PRESSURE);
#endif
  // Space for temperature.
  H1Space<double> temperature_space(&mesh_whole_domain, &bcs_temperature, P_INIT);

  // Calculate and report the number of degrees of freedom.
  int ndof = Space<double>::get_num_dofs(Hermes::vector<Space<double> *>(&xvel_space, &yvel_space, &p_space, &temperature_space));
  info("ndof = %d.", ndof);

  // Define projection norms.
  ProjNormType vel_proj_norm = HERMES_H1_NORM;
#ifdef PRESSURE_IN_L2
  ProjNormType p_proj_norm = HERMES_L2_NORM;
#else
  ProjNormType p_proj_norm = HERMES_H1_NORM;
#endif
  ProjNormType temperature_proj_norm = HERMES_H1_NORM;

  // Solutions for the Newton's iteration and time stepping.
  info("Setting initial conditions.");
  ZeroInitialCondition xvel_prev_time(&mesh_without_hole), yvel_prev_time(&mesh_without_hole), p_prev_time(&mesh_without_hole);
  ConstInitialCondition  temperature_prev_time(&mesh_whole_domain, TEMP_INIT); 

  // Initialize weak formulation.
  CustomWeakFormHeatAndFlow wf(STOKES, RE, TAU, &xvel_prev_time, &yvel_prev_time, &temperature_prev_time, 
    HEAT_SOURCE, SPECIFIC_HEAT_OUTER, SPECIFIC_HEAT_INNER, RHO_OUTER, RHO_INNER, THERMAL_DIFFUSIVITY_OUTER, THERMAL_DIFFUSIVITY_INNER, RE * VISCOSITY_OUTER / (RHO_OUTER * L));
  
  // Initialize the FE problem.
  DiscreteProblem<double> dp(&wf, Hermes::vector<Space<double> *>(&xvel_space, &yvel_space, &p_space, &temperature_space));

  // Initialize the Newton solver.
  Hermes::Hermes2D::NewtonSolver<double> newton(&dp, matrix_solver_type);

  // Initialize views.
  Views::VectorView vview("velocity [m/s]", new Views::WinGeom(0, 0, 500, 300));
  Views::ScalarView pview("pressure [Pa]", new Views::WinGeom(0, 310, 500, 300));
  Views::ScalarView tempview("temperature [C]", new Views::WinGeom(510, 0, 500, 300));
  vview.set_min_max_range(0, 1.6);
  vview.fix_scale_width(80);
  //pview.set_min_max_range(-0.9, 1.0);
  pview.fix_scale_width(80);
  pview.show_mesh(true);

  // Project the initial condition on the FE space to obtain initial
  // coefficient vector for the Newton's method.
  double* coeff_vec = new double[ndof];
  info("Projecting initial condition to obtain initial vector for the Newton's method.");
  OGProjection<double>::project_global(Hermes::vector<Space<double> *>(&xvel_space, &yvel_space, &p_space, &temperature_space), 
    Hermes::vector<MeshFunction<double> *>(&xvel_prev_time, &yvel_prev_time, &p_prev_time, &temperature_prev_time), 
    coeff_vec, matrix_solver_type, 
    Hermes::vector<ProjNormType>(vel_proj_norm, vel_proj_norm, p_proj_norm, temperature_proj_norm));

  // Time-stepping loop:
  char title[100];
  int num_time_steps = T_FINAL / TAU;
  double current_time = 0.0;
  for (int ts = 1; ts <= num_time_steps; ts++)
  {
    current_time += TAU;
    info("---- Time step %d, time = %g:", ts, current_time);

    // Update time-dependent essential BCs.
    if (current_time <= STARTUP_TIME) 
    {
      info("Updating time-dependent essential BC.");
      Space<double>::update_essential_bc_values(Hermes::vector<Space<double> *>(&xvel_space, &yvel_space, &p_space, &temperature_space), current_time);
    }

    // Perform Newton's iteration.
    info("Solving nonlinear problem:");
    bool verbose = true;
    // Perform Newton's iteration and translate the resulting coefficient vector into previous time level solutions.
    newton.set_verbose_output(verbose);
    if (!newton.solve(coeff_vec, NEWTON_TOL, NEWTON_MAX_ITER)) 
      error("Newton's iteration failed.");
    else
    {
      Hermes::vector<Solution<double> *> tmp(&xvel_prev_time, &yvel_prev_time, &p_prev_time, &temperature_prev_time);
      Hermes::Hermes2D::Solution<double>::vector_to_solutions(newton.get_sln_vector(), Hermes::vector<Space<double> *>(&xvel_space, &yvel_space, &p_space, &temperature_space), tmp);
    }
    
    // Show the solution at the end of time step.
    sprintf(title, "Velocity [m/s], time %g s", current_time);
    vview.set_title(title);
    vview.show(&xvel_prev_time, &yvel_prev_time, Views::HERMES_EPS_LOW);
    sprintf(title, "Pressure [Pa], time %g s", current_time);
    pview.set_title(title);
    pview.show(&p_prev_time);
    sprintf(title, "Temperature [C], time %g s", current_time);
    tempview.set_title(title);
    tempview.show(&temperature_prev_time);
  }

  delete [] coeff_vec;

  // Wait for all views to be closed.
  Views::View::wait();
  return 0;
}
