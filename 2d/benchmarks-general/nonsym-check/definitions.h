#include "hermes2d.h"

using namespace Hermes::Hermes2D;
using namespace WeakFormsH1;
using Hermes::Ord;

/* Exact solution */

class CustomExactSolution : public ExactSolutionScalar<double>
{
public:
  CustomExactSolution(Mesh* mesh) : ExactSolutionScalar(mesh) 
  {
  }

  virtual void derivatives(double x, double y, double& dx, double& dy) const;
  
  virtual double value(double x, double y) const;

  virtual Ord ord(Ord x, Ord y) const; 
};

/* Weak forms */

class CustomJacobian : public MatrixFormVol<double>
{
public:
  CustomJacobian(int i, int j) : MatrixFormVol(i, j) 
  {
  }

  virtual double value(int n, double *wt, Func<double> *u_ext[], Func<double> *u,
                       Func<double> *v, Geom<double> *e, ExtData<double> *ext) const; 

  virtual Ord ord(int n, double *wt, Func<Ord> *u_ext[], Func<Ord> *u, Func<Ord> *v,
                  Geom<Ord> *e, ExtData<Ord> *ext) const; 
};

class CustomResidual : public VectorFormVol<double>
{
public:
  CustomResidual(int i) : VectorFormVol(i) 
  {
  }

  virtual double value(int n, double *wt, Func<double> *u_ext[],
                       Func<double> *v, Geom<double> *e, ExtData<double> *ext) const;

  virtual Ord ord(int n, double *wt, Func<Ord> *u_ext[], Func<Ord> *v,
                  Geom<Ord> *e, ExtData<Ord> *ext) const; 
};

class CustomWeakForm : public WeakForm<double>
{
public:
  CustomWeakForm(std::string marker_bdy_right);
};