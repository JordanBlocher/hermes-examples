#define HERMES_REPORT_ALL
#define HERMES_REPORT_FILE "application.log"
#include "definitions.h"




//  This example is part of a research project on the design of 
//  optimal meshes.
//
//  Solved PDE: - Laplace u + u = f
//
//  Domain: Square (-1, 1) x (-1, 1)
//  
//  Exact solution: u(x,y) = atan(K*x) 
//
//  BC: Neumann, given by exact solution.

// Uniform polynomial degree of all mesh elements.
int P_INIT = 2;                                   
// Number of initial uniform mesh refinements.
int UNIFORM_REF_LEVEL = 0;                        
// This is a quantitative parameter of the adapt(...) function and
// it has different meanings for various adaptive strategies.
const double THRESHOLD = 0.3;                     
// Adaptive strategy:
// STRATEGY = 0 ... refine elements until sqrt(THRESHOLD) times total
//   error is processed. If more elements have similar errors, refine
//   all to keep the mesh symmetric.
// STRATEGY = 1 ... refine all elements whose error is larger
//   than THRESHOLD times maximum element error.
// STRATEGY = 2 ... refine all elements whose error is larger
//   than THRESHOLD.
const int STRATEGY = 0;                           
// Predefined list of element refinement candidates. Possible values are
// H2D_P_ISO, H2D_P_ANISO, H2D_H_ISO, H2D_H_ANISO, H2D_HP_ISO,
// H2D_HP_ANISO_H, H2D_HP_ANISO_P, H2D_HP_ANISO.
const CandList CAND_LIST = H2D_HP_ANISO;          
// Maximum allowed level of hanging nodes:
// MESH_REGULARITY = -1 ... arbitrary level hangning nodes (default),
// MESH_REGULARITY = 1 ... at most one-level hanging nodes,
// MESH_REGULARITY = 2 ... at most two-level hanging nodes, etc.
// Note that regular meshes are not supported, this is due to
// their notoriously bad performance.
const int MESH_REGULARITY = -1;                   
// This parameter influences the selection of
// candidates in hp-adaptivity. Default value is 1.0. 
const double CONV_EXP = 0.5;                      
// Stopping criterion for adaptivity.
const double ERR_STOP = 0.01;                      
// Adaptivity process stops when the number of degrees of freedom grows
// over this limit. This is to prevent h-adaptivity to go on forever.
const int NDOF_STOP = 60000;                      
// Matrix solver: SOLVER_AMESOS, SOLVER_AZTECOO, SOLVER_MUMPS,
// SOLVER_PETSC, SOLVER_SUPERLU, SOLVER_UMFPACK.
MatrixSolverType matrix_solver_type = SOLVER_UMFPACK;  

// Problem parameters.
double K = 10.0;

// Boundary markers.
const std::string BDY_LEFT_RIGHT = "Bdy left right";
const std::string BDY_TOP_BOTTOM = "Bdy top bottom";

int main(int argc, char* argv[])
{
  // Load the mesh.
  Mesh mesh;
  MeshReaderH2D mloader;
  mloader.load("square_2_elem.mesh", &mesh);

  // Perform initial mesh refinements.
  for(int i = 0; i < UNIFORM_REF_LEVEL; i++) mesh.refine_all_elements();

  // Create an H1 space with default shapeset.
  H1Space<double> space(&mesh, P_INIT);
  int ndof = Space<double>::get_num_dofs(&space);
  info("ndof = %d", ndof);

  // Initialize the right-hand side.
  CustomRightHandSide rhs_value(K);

  // Initialize the weak formulation.
  CustomWeakForm wf(&rhs_value, BDY_LEFT_RIGHT, K);

  Solution<double> sln; 

  // NON-ADAPTIVE VERSION
  
  // Initialize the linear problem.
  DiscreteProblem<double> dp(&wf, &space);

  // Select matrix solver.
  SparseMatrix<double>* matrix = create_matrix<double>(matrix_solver_type);
  Vector<double>* rhs = create_vector<double>(matrix_solver_type);
  LinearSolver<double>* solver = create_linear_solver<double>(matrix_solver_type, matrix, rhs);

  // Assemble stiffness matrix and rhs.
  dp.assemble(matrix, rhs);
 
  // Solve the linear system of the reference problem. If successful, obtain the solutions.
  if(solver->solve()) Solution<double>::vector_to_solution(solver->get_sln_vector(), &space, &sln);
  else error ("Matrix solver failed.\n");

  // Visualize the solution.
  ScalarView view("Solution", new WinGeom(0, 0, 440, 350));
  view.show(&sln);

  // Calculate error wrt. exact solution.
  CustomExactSolution sln_exact(&mesh, K);
  double err = Global<double>::calc_abs_error(&sln, &sln_exact, HERMES_H1_NORM);
  printf("err = %g, err_squared = %g\n\n", err, err*err);
 
  // Wait for all views to be closed.
  View::wait();
  return 0;
}
