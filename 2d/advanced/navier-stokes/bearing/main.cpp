#define HERMES_REPORT_ALL
#define HERMES_REPORT_FILE "application.log"
#include "definitions.h"

// Flow in between two circles, inner circle is rotating with surface 
// velocity VEL. The time-dependent laminar incompressible Navier-Stokes equations
// are discretized in time via the implicit Euler method. The Newton's method is 
// used to solve the nonlinear problem at each time step. We also show how
// to use discontinuous ($L^2$) elements for pressure and thus make the
// velocity discretely divergence-free. Comparison to approximating the
// pressure with the standard (continuous) Taylor-Hood elements is enabled.
// 
// PDE: incompressible Navier-Stokes equations in the form
//     \partial v / \partial t - \Delta v / Re + (v \cdot \nabla) v + \nabla p = 0,
//     div v = 0.
//
// BC: tangential velocity V on Gamma_1 (inner circle),
//     zero velocity on Gamma_2 (outer circle).
//
// Geometry: Area in between two concentric circles with radiuses r1 and r2,
//           r1 < r2. These radiuses can be changed in the file "domain.mesh".
//
// The following parameters can be changed:

const int INIT_REF_NUM = 2;                       // Number of initial uniform mesh refinements. 
const int INIT_BDY_REF_NUM_INNER = 2;             // Number of initial mesh refinements towards boundary. 
const int INIT_BDY_REF_NUM_OUTER = 2;             // Number of initial mesh refinements towards boundary. 

const bool STOKES = false;                        // For application of Stokes flow (creeping flow).
#define PRESSURE_IN_L2                            // If this is defined, the pressure is approximated using
                                                  // discontinuous L2 elements (making the velocity discreetely
                                                  // divergence-free, more accurate than using a continuous
                                                  // pressure approximation). Otherwise the standard continuous
                                                  // elements are used. The results are striking - check the
                                                  // tutorial for comparisons.
const int P_INIT_VEL = 2;                         // Initial polynomial degree for velocity components.
const int P_INIT_PRESSURE = 1;                    // Initial polynomial degree for pressure.
                                                  // Note: P_INIT_VEL should always be greater than
                                                  // P_INIT_PRESSURE because of the inf-sup condition.
const double RE = 5000.0;                         // Reynolds number.
const double VEL = 0.1;                           // Surface velocity of inner circle.
const double STARTUP_TIME = 1.0;                  // During this time, surface velocity of the inner circle increases 
                                                  // gradually from 0 to VEL, then it stays constant.
const double TAU = 10.0;                          // Time step.
const double T_FINAL = 3600.0;                    // Time interval length.
const double NEWTON_TOL = 1e-5;                   // Stopping criterion for the Newton's method.
const int NEWTON_MAX_ITER = 10;                   // Maximum allowed number of Newton iterations.
MatrixSolverType matrix_solver_type = SOLVER_UMFPACK;  // Possibilities: SOLVER_AMESOS, SOLVER_AZTECOO, SOLVER_MUMPS,
                                                  // SOLVER_PETSC, SOLVER_SUPERLU, SOLVER_UMFPACK.

// Current time (used in weak forms).
double current_time = 0;

// Custom function to calculate drag coefficient.
double integrate_over_wall(MeshFunction<double>* meshfn, int marker)
{
  Quad2D* quad = &g_quad_2d_std;
  meshfn->set_quad_2d(quad);

  double integral = 0.0;
  Element* e;
  Mesh* mesh = meshfn->get_mesh();

  for_all_active_elements(e, mesh)
  {
    for(int edge = 0; edge < e->get_num_surf(); edge++)
    {
      if ((e->en[edge]->bnd) && (e->en[edge]->marker == marker))
      {
        update_limit_table(e->get_mode());
        RefMap* ru = meshfn->get_refmap();

        meshfn->set_active_element(e);
        int eo = quad->get_edge_points(edge);
        meshfn->set_quad_order(eo, H2D_FN_VAL);
        double *uval = meshfn->get_fn_values();
        double3* pt = quad->get_points(eo);
        double3* tan = ru->get_tangent(edge);
        for (int i = 0; i < quad->get_num_points(eo); i++)
          integral += pt[i][2] * uval[i] * tan[i][2];
      }
    }
  }
  return integral * 0.5;
}

int main(int argc, char* argv[])
{
  

  // Load the mesh.
  Mesh mesh;
  MeshReaderH2D mloader;
  mloader.load("domain-excentric.mesh", &mesh);
  //mloader.load("domain-concentric.mesh", &mesh);

  // Initial mesh refinements.
  for (int i=0; i < INIT_REF_NUM; i++) mesh.refine_all_elements();
  mesh.refine_towards_boundary("Inner", INIT_BDY_REF_NUM_INNER, false);  // true for anisotropic refinements
  mesh.refine_towards_boundary("Outer", INIT_BDY_REF_NUM_OUTER, false);  // false for isotropic refinements

  // Initialize boundary conditions.
  EssentialBCNonConstX bc_inner_vel_x(std::string("Inner"), VEL, STARTUP_TIME);
  EssentialBCNonConstY bc_inner_vel_y(std::string("Inner"), VEL, STARTUP_TIME);
  DefaultEssentialBCConst<double> bc_outer_vel(std::string("Outer"), 0.0);
  EssentialBCs<double> bcs_vel_x(Hermes::vector<EssentialBoundaryCondition<double> *>(&bc_inner_vel_x, &bc_outer_vel));
  EssentialBCs<double> bcs_vel_y(Hermes::vector<EssentialBoundaryCondition<double> *>(&bc_inner_vel_y, &bc_outer_vel));

  // Spaces for velocity components and pressure.
  H1Space<double> xvel_space(&mesh, &bcs_vel_x, P_INIT_VEL);
  H1Space<double> yvel_space(&mesh, &bcs_vel_y, P_INIT_VEL);
#ifdef PRESSURE_IN_L2
  L2Space<double> p_space(&mesh, P_INIT_PRESSURE);
#else
  H1Space<double> p_space(&mesh, P_INIT_PRESSURE);
#endif
  Hermes::vector<Space<double>*> spaces = Hermes::vector<Space<double>*>(&xvel_space, &yvel_space, &p_space);

  // Calculate and report the number of degrees of freedom.
  int ndof = Space<double>::get_num_dofs(spaces);
  info("ndof = %d.", ndof);

  // Define projection norms.
  ProjNormType vel_proj_norm = HERMES_H1_NORM;
#ifdef PRESSURE_IN_L2
  ProjNormType p_proj_norm = HERMES_L2_NORM;
#else
  ProjNormType p_proj_norm = HERMES_H1_NORM;
#endif

  // Solutions for the Newton's iteration and time stepping.
  info("Setting initial conditions.");
  ZeroSolution xvel_prev_time(&mesh);
  ZeroSolution yvel_prev_time(&mesh);
  ZeroSolution p_prev_time(&mesh);
 
  Hermes::vector<Solution<double>*> slns = Hermes::vector<Solution<double>*>(&xvel_prev_time, &yvel_prev_time, 
                                                             &p_prev_time);

  // Initialize weak formulation.
  WeakForm<double>* wf = new WeakFormNSNewton(STOKES, RE, TAU, &xvel_prev_time, &yvel_prev_time);

  // Initialize the FE problem.
  DiscreteProblem<double> dp(wf, spaces);

  // Set up the solver, matrix, and rhs according to the solver selection.
  SparseMatrix<double>* matrix = create_matrix<double>(matrix_solver_type);
  Vector<double>* rhs = create_vector<double>(matrix_solver_type);
  LinearSolver<double>* solver = create_linear_solver<double>(matrix_solver_type, matrix, rhs);

  // Initialize views.
  VectorView vview("velocity [m/s]", new WinGeom(0, 0, 600, 500));
  ScalarView pview("pressure [Pa]", new WinGeom(610, 0, 600, 500));
  //vview.set_min_max_range(0, 1.6);
  vview.fix_scale_width(80);
  //pview.set_min_max_range(-0.9, 1.0);
  pview.fix_scale_width(80);
  pview.show_mesh(true);

  // Project the initial condition on the FE space to obtain initial
  // coefficient vector for the Newton's method.
  double* coeff_vec = new double[Space<double>::get_num_dofs(spaces)];
  // Newton's vector is set to zero (no OG projection needed).
  memset(coeff_vec, 0, ndof * sizeof(double));
  /*
  // This can be used for more complicated initial conditions.
    info("Projecting initial condition to obtain initial vector for the Newton's method.");
    OGProjection<double>::project_global(spaces, slns, coeff_vec, matrix_solver_type, 
                                 Hermes::vector<ProjNormType>(vel_proj_norm, vel_proj_norm, p_proj_norm));
  */

  // Time-stepping loop:
  char title[100];
  int num_time_steps = T_FINAL / TAU;
  for (int ts = 1; ts <= num_time_steps; ts++)
  {
    current_time += TAU;
    info("---- Time step %d, time = %g:", ts, current_time);

    // Update time-dependent essential BCs.
    info("Updating time-dependent essential BC.");
    Space<double>::update_essential_bc_values(Hermes::vector<Space<double>*>(&xvel_space, &yvel_space, &p_space), current_time);

    // Perform Newton's iteration.
    info("Solving nonlinear problem:");
    Hermes::Hermes2D::NewtonSolver<double> newton(&dp, matrix_solver_type);
    try
    {
      newton.solve(coeff_vec, NEWTON_TOL, NEWTON_MAX_ITER);
    }
    catch(Hermes::Exceptions::Exception e)
    {
      e.printMsg();
      error("Newton's iteration failed.");
    };

    // Update previous time level solutions.
    Solution<double>::vector_to_solutions(coeff_vec, spaces, slns);

    // Show the solution at the end of time step.
    sprintf(title, "Velocity, time %g", current_time);
    vview.set_title(title);
    vview.show(&xvel_prev_time, &yvel_prev_time);
    sprintf(title, "Pressure, time %g", current_time);
    pview.set_title(title);
    pview.show(&p_prev_time);
 }

  // Clean up.
  delete [] coeff_vec;
  delete matrix;
  delete rhs;
  delete solver;

  // Wait for all views to be closed.
  View::wait();
  return 0;
}
