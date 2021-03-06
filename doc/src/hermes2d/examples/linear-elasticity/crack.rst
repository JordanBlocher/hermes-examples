Crack
-----

This example employs adaptive multimesh hp-FEM to solve
equations of linear elasticity that we already saw in the tutorial example
P01-linear/08-system.

Problem description
~~~~~~~~~~~~~~~~~~~

The computational domain is a $1.5 \times 0.3$ m rectangle containing two horizontal 
cracks, as shown in the following figure:

.. figure:: example-crack/domain.png
   :align: center
   :scale: 60% 
   :figclass: align-center
   :alt: Domain.

The cracks have a flat diamond-like shape and their width along with some other parameters 
can be changed in the mesh file::

    a = 0.25   # horizontal size of an eleemnt
    b = 0.1    # vertical size of an element
    w = 0.001  # width of the cracks

Solved are equations of linear elasticity with the following boundary conditions: 
$u_1 = u_2 = 0$ on the left edge, zero external force on the rest of the boundary.
The elastic body is loaded with its own weight.

Sample results
~~~~~~~~~~~~~~

Detail of singularity in Von Mises stress at the left end of the left crack:

.. figure:: example-crack/sol.png
   :align: center
   :scale: 50% 
   :figclass: align-center
   :alt: Solution.

Convergence comparisons
~~~~~~~~~~~~~~~~~~~~~~~

Final meshes for $u_1$ and $u_2$ (h-FEM with linear elements):

.. figure:: example-crack/mesh-x-h1.png
   :align: center
   :scale: 70% 
   :figclass: align-center
   :alt: Solution.

.. figure:: example-crack/mesh-y-h1.png
   :align: center
   :scale: 70% 
   :figclass: align-center
   :alt: Solution.

Final meshes for $u_1$ and $u_2$ (h-FEM with quadratic elements):

.. figure:: example-crack/mesh-x-h2.png
   :align: center
   :scale: 70% 
   :figclass: align-center
   :alt: Solution.

.. figure:: example-crack/mesh-x-h2.png
   :align: center
   :scale: 70% 
   :figclass: align-center
   :alt: Solution.

Final meshes for $u_1$ and $u_2$ (hp-FEM):

.. figure:: example-crack/mesh-x-hp.png
   :align: center
   :scale: 70% 
   :figclass: align-center
   :alt: Solution.

.. figure:: example-crack/mesh-y-hp.png
   :align: center
   :scale: 70% 
   :figclass: align-center
   :alt: Solution.

DOF convergence graphs:

.. figure:: example-crack/conv_dof.png
   :align: center
   :scale: 50% 
   :figclass: align-center
   :alt: DOF convergence graph.

CPU time convergence graphs:

.. figure:: example-crack/conv_cpu.png
   :align: center
   :scale: 50% 
   :figclass: align-center
   :alt: CPU convergence graph.

Next let us compare the multimesh hp-FEM with the standard (single-mesh) hp-FEM:

.. figure:: example-crack/conv_dof_compar.png
   :align: center
   :scale: 50% 
   :figclass: align-center
   :alt: DOF convergence graph.

The same comparison in terms of CPU time:

.. figure:: example-crack/conv_cpu_compar.png
   :align: center
   :scale: 50% 
   :figclass: align-center
   :alt: CPU convergence graph.


