
/******************************************************************************
*
* IMD -- The ITAP Molecular Dynamics Program
*
* Copyright 1996-2001 Institute for Theoretical and Applied Physics,
* University of Stuttgart, D-70550 Stuttgart
*
******************************************************************************/

/******************************************************************************
*
* imd_main_mpi_3d.c -- main loop, mpi specific part, three dimensions
*
******************************************************************************/

/******************************************************************************
* $Revision$
* $Date$
******************************************************************************/

#include "imd.h"

/******************************************************************************
*
* calc_forces 
*
* The forces of the atoms are calulated here. To achive this, atoms on
* the surface of a cpu are exchanged with the neigbours.
*
* If AR is defined, we use actio=reactio even across CPUs, otherwise we don't
*
* The force calculation is split into those steps:
*
* i)   send atoms positions of cells on surface neighbours, 
*      receive atom positions from neigbours
* ii)  zero forces on all cells (local and buffer)
* iii) calculate forces in local cells, use lower half of neigbours 
*      for each cell and use actio==reactio
* iv)  calculate forces also for upper half of neighbours for all cells
*      that are on the upper surface
* iv)  or send forces back and add them
*
******************************************************************************/

void calc_forces(int steps)
{
  int n, k;
  real tmpvec1[8], tmpvec2[8] = {0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0};

  /* fill the buffer cells */
  if ((steps == steps_min) || (0 == steps % BUFSTEP)) setup_buffers();
  send_cells(copy_cell,pack_cell,unpack_cell);

  /* clear global accumulation variables */
  tot_pot_energy = 0.0;
  virial = 0.0;
  vir_xx = 0.0;
  vir_yy = 0.0;
  vir_zz = 0.0;
  vir_yz = 0.0;
  vir_zx = 0.0;
  vir_xy = 0.0;

  /* clear per atom accumulation variables */
#ifdef _OPENMP
#pragma omp parallel for
#endif
  for (k=0; k<nallcells; ++k) {
    int  i;
    cell *p;
    p = cell_array + k;
    for (i=0; i<p->n; ++i) {
      KRAFT(p,i,X) = 0.0;
      KRAFT(p,i,Y) = 0.0;
      KRAFT(p,i,Z) = 0.0;
#ifdef UNIAX
      DREH_MOMENT(p,i,X) = 0.0;
      DREH_MOMENT(p,i,Y) = 0.0;
      DREH_MOMENT(p,i,Z) = 0.0;
#endif
#ifdef NVX
      HEATCOND(p,i) = 0.0;
#endif      
#ifdef STRESS_TENS
      PRESSTENS(p,i,xx) = 0.0;
      PRESSTENS(p,i,yy) = 0.0;
      PRESSTENS(p,i,zz) = 0.0;
      PRESSTENS(p,i,yz) = 0.0;
      PRESSTENS(p,i,zx) = 0.0;
      PRESSTENS(p,i,xy) = 0.0;
#endif      
#ifndef MONOLJ
      POTENG(p,i) = 0.0;
#endif
#ifdef ORDPAR
      NBANZ(p,i) = 0;
#endif
#ifdef COVALENT
      NEIGH(p,i)->n = 0;
#endif
#ifdef EAM2
      EAM_RHO(p,i) = 0.0; /* zero host electron density at atom site */
#endif
    }
  }

  /* What follows is the standard one-cpu force 
     loop acting on our local data cells */

  /* compute forces for all pairs of cells */
  for (n=0; n<nlists; ++n) {
#ifdef _OPENMP
#pragma omp parallel for schedule(runtime) reduction(+:tot_pot_energy,virial,vir_xx,vir_yy,vir_zz,vir_yz,vir_zx,vir_xy)
#endif
    for (k=0; k<npairs[n]; ++k) {
      vektor pbc;
      pair *P;
      P = pairs[n] + k;
      pbc.x = P->ipbc[0]*box_x.x + P->ipbc[1]*box_y.x + P->ipbc[2]*box_z.x;
      pbc.y = P->ipbc[0]*box_x.y + P->ipbc[1]*box_y.y + P->ipbc[2]*box_z.y;
      pbc.z = P->ipbc[0]*box_x.z + P->ipbc[1]*box_y.z + P->ipbc[2]*box_z.z;
      do_forces(cell_array + P->np, cell_array + P->nq, pbc,
                &tot_pot_energy, &virial, &vir_xx, &vir_yy, &vir_zz,
                                          &vir_yz, &vir_zx, &vir_xy);
    }
  }

#ifdef COVALENT
  /* complete neighbor tables for remaining pairs of cells */
  for (n=0; n<nlists; ++n) {
#ifdef _OPENMP
#pragma omp parallel for schedule(runtime)
#endif
    for (k=npairs[n]; k<npairs2[n]; ++k) {
      vektor pbc;
      pair *P;
      P = pairs[n] + k;
      pbc.x = P->ipbc[0]*box_x.x + P->ipbc[1]*box_y.x + P->ipbc[2]*box_z.x;
      pbc.y = P->ipbc[0]*box_x.y + P->ipbc[1]*box_y.y + P->ipbc[2]*box_z.y;
      pbc.z = P->ipbc[0]*box_x.z + P->ipbc[1]*box_y.z + P->ipbc[2]*box_z.z;
      do_neightab(cell_array + P->np, cell_array + P->nq, pbc);
    }
  }

  /* second force loop for covalent systems */
/* does not work correctly - different threads may write to same variables 
#ifdef _OPENMP
#pragma omp parallel for schedule(runtime) reduction(+:tot_pot_energy,virial,vir_xx,vir_yy,vir_zz,vir_yz,vir_zx,vir_xy)
#endif
*/
  for (k=0; k<ncells; ++k) {
    do_forces2(cell_array + CELLS(k),
               &tot_pot_energy, &virial, &vir_xx, &vir_yy, &vir_zz,
                                         &vir_yz, &vir_zx, &vir_xy);
  }
#endif /* COVALENT */

#ifndef AR
  /* If we don't use actio=reactio accross the cpus, we have do do
     the force loop also on the other half of the neighbours for the 
     cells on the surface of the CPU */

  /* compute forces for remaining pairs of cells */
  for (n=0; n<nlists; ++n) {
/* does not work correctly - different threads may write to same variables 
#ifdef _OPENMP
#pragma omp parallel for schedule(runtime)
#endif
*/
    for (k=npairs[n]; k<npairs2[n]; ++k) {
      vektor pbc;
      pair *P;
      P = pairs[n] + k;
      pbc.x = P->ipbc[0]*box_x.x + P->ipbc[1]*box_y.x + P->ipbc[2]*box_z.x;
      pbc.y = P->ipbc[0]*box_x.y + P->ipbc[1]*box_y.y + P->ipbc[2]*box_z.y;
      pbc.z = P->ipbc[0]*box_x.z + P->ipbc[1]*box_y.z + P->ipbc[2]*box_z.z;
      /* potential energy and virial are already complete;          */
      /* to avoid double counting, we update only the dummy tmpvec2 */
      do_forces(cell_array + P->np, cell_array + P->nq, pbc,
                tmpvec2, tmpvec2+1, tmpvec2+2, tmpvec2+3, tmpvec2+4,
                                    tmpvec2+5, tmpvec2+6, tmpvec2+7);
    }
  }
#endif  /* not AR */

#ifdef EAM2

#ifdef AR
  send_forces(add_rho_h,pack_rho_h,unpack_add_rho_h);
#endif
  send_cells(copy_rho_h,pack_rho_h_v,unpack_rho_h);

  /* second EAM2 loop over all cells pairs */
  for (n=0; n<nlists; ++n) {
#ifdef _OPENMP
#pragma omp parallel for schedule(runtime) reduction(+:tot_pot_energy,virial,vir_xx,vir_yy,vir_zz,vir_yz,vir_zx,vir_xy)
#endif
    for (k=0; k<npairs[n]; ++k) {
      vektor pbc;
      pair *P;
      P = pairs[n]+k;
      pbc.x = P->ipbc[0]*box_x.x + P->ipbc[1]*box_y.x + P->ipbc[2]*box_z.x;
      pbc.y = P->ipbc[0]*box_x.y + P->ipbc[1]*box_y.y + P->ipbc[2]*box_z.y;
      pbc.z = P->ipbc[0]*box_x.z + P->ipbc[1]*box_y.z + P->ipbc[2]*box_z.z;
      do_forces_eam2(cell_array + P->np, cell_array + P->nq, pbc,
                     &tot_pot_energy, &virial, &vir_xx, &vir_yy, &vir_zz,
                                               &vir_yz, &vir_zx, &vir_xy);
    }
  }

#ifndef AR
  /* If we don't use actio=reactio accross the cpus, we have do do
     the force loop also on the other half of the neighbours for the 
     cells on the surface of the CPU */

  /* compute forces for remaining pairs of cells */
  for (n=0; n<nlists; ++n) {
#ifdef _OPENMP
#pragma omp parallel for schedule(runtime)
#endif
    for (k=npairs[n]; k<npairs2[n]; ++k) {
      vektor pbc;
      pair *P;
      P = pairs[n]+k;
      pbc.x = P->ipbc[0]*box_x.x + P->ipbc[1]*box_y.x + P->ipbc[2]*box_z.x;
      pbc.y = P->ipbc[0]*box_x.y + P->ipbc[1]*box_y.y + P->ipbc[2]*box_z.y;
      pbc.z = P->ipbc[0]*box_x.z + P->ipbc[1]*box_y.z + P->ipbc[2]*box_z.z;
      /* potential energy and virial are already complete;          */
      /* to avoid double counting, we update only the dummy tmpvec2 */
      do_forces_eam2(cell_array + P->np, cell_array + P->nq, pbc,
                     tmpvec2, tmpvec2+1, tmpvec2+2, tmpvec2+3, tmpvec2+4,
                                         tmpvec2+5, tmpvec2+6, tmpvec2+7);
    }
  }
#endif /* not AR */

#endif /* EAM2 */

  /* sum up results of different CPUs */
  tmpvec1[0] = tot_pot_energy;
  tmpvec1[1] = virial;
  tmpvec1[2] = vir_xx;
  tmpvec1[3] = vir_yy;
  tmpvec1[4] = vir_zz;
  tmpvec1[5] = vir_yz;
  tmpvec1[6] = vir_zx;
  tmpvec1[7] = vir_xy;

  MPI_Allreduce( tmpvec1, tmpvec2, 8, REAL, MPI_SUM, cpugrid); 

  tot_pot_energy = tmpvec2[0];
  virial         = tmpvec2[1];
  vir_xx         = tmpvec2[2];
  vir_yy         = tmpvec2[3];
  vir_zz         = tmpvec2[4];
  vir_yz         = tmpvec2[5];
  vir_zx         = tmpvec2[6];
  vir_xy         = tmpvec2[7];

#ifdef AR
  send_forces(add_forces,pack_forces,unpack_forces);
#endif

}

/******************************************************************************
*
*  fix_cells
*
*  check if each atom is in the correct cell and on the correct CPU;
*  move atoms that have left their cell or CPU
*
*  this also uses Plimpton's comm scheme
*
******************************************************************************/

void fix_cells(void)
{
  int i,j,k,l,clone;
  cell *p, *q;
  ivektor coord, lcoord;
  int to_cpu;
  msgbuf *buf;

  empty_mpi_buffers();

  /* for each cell in bulk */
  for (i=cellmin.x; i < cellmax.x; ++i)
    for (j=cellmin.y; j < cellmax.y; ++j)
      for (k=cellmin.z; k < cellmax.z; ++k) {

	p = PTR_3D_V(cell_array, i, j, k, cell_dim);

	/* loop over atoms in cell */
	l=0;
	while( l<p->n ) {

          coord  = cell_coord( ORT(p,l,X), ORT(p,l,Y), ORT(p,l,Z) );
	  lcoord = local_cell_coord( coord );

 	  /* see if atom is in wrong cell */
	  if ((lcoord.x == i) && (lcoord.y == j) && (lcoord.z == k)) {
            l++;
          } 
          else {

            to_cpu = cpu_coord(coord);
            buf    = NULL;

            /* atom is on my cpu */

	    if (to_cpu==myid) {
               q = PTR_VV(cell_array,lcoord,cell_dim);
               MOVE_ATOM(q, p, l);
#ifdef CLONE
               if (l < p->n-nclones)
                 for (clone=1; clone<nclones; clone++)
                   MOVE_ATOM(q, p, l+clone);
               else /* we are dealing with the last in the stack */
                 for (clone=1; clone<nclones; clone++)
                   MOVE_ATOM(q, p, l); 
#endif
            }

            /* west */
            else if ((cpu_dim.x>1) && 
              ((to_cpu==nbwest) || (to_cpu==nbnw)  || (to_cpu==nbws) ||
               (to_cpu==nbuw  ) || (to_cpu==nbunw) || (to_cpu==nbuws)||
               (to_cpu==nbdw  ) || (to_cpu==nbdwn) || (to_cpu==nbdsw))) {
              buf = &send_buf_west;
            }
            
            /* east */
            else if ((cpu_dim.x>1) &&
              ((to_cpu==nbeast) || (to_cpu==nbse)  || (to_cpu==nben) ||
               (to_cpu==nbue  ) || (to_cpu==nbuse) || (to_cpu==nbuen)||
               (to_cpu==nbde  ) || (to_cpu==nbdes) || (to_cpu==nbdne))) {
              buf = &send_buf_east;
            }
                   
            /* south  */
            else if ((cpu_dim.y>1) &&
              ((to_cpu==nbsouth) || (to_cpu==nbus)  || (to_cpu==nbds))) {
              buf = &send_buf_south;
            }
                   
            /* north  */
            else if ((cpu_dim.y>1) &&
              ((to_cpu==nbnorth) || (to_cpu==nbun)  || (to_cpu==nbdn))) {
              buf = &send_buf_north;
            }
            
            /* down  */
            else if ((cpu_dim.z>1) && (to_cpu==nbdown)) {
              buf = &send_buf_down;
            }
            
            /* up  */
            else if ((cpu_dim.z>1) && (to_cpu==nbup)) {
              buf = &send_buf_up;
            }
            else error("Atom jumped multiple CPUs");

            if (buf != NULL) {
              copy_one_atom( buf, p, l, 1);
#ifdef CLONE
              if (l < p->n-nclones)
                for (clone=1; clone<nclones; clone++)
                  copy_one_atom( buf, p, l+clone, 1);
              else /* we are dealing with the last in the stack */
                for (clone=1; clone<nclones; clone++)
                  copy_one_atom( buf, p, l, 1);
#endif
	    }
	  }
	}
      }

  /* send atoms to neighbbour CPUs */
  send_atoms();

}

#ifdef SR

/******************************************************************************
*
* send_atoms  -  only used for fix_cells
*
******************************************************************************/

void send_atoms()
{
  MPI_Status  stat;

  if (cpu_dim.x > 1) {
    /* send east, receive west, move atoms from west to cells */
    sendrecv_buf( &send_buf_east, nbeast, &recv_buf_west, nbwest, &stat);
    MPI_Get_count( &stat, REAL, &recv_buf_west.n );
    process_buffer( &recv_buf_west, (cell *) NULL );

    /* send west, receive east, move atoms from east to cells */
    sendrecv_buf( &send_buf_west, nbwest, &recv_buf_east, nbeast, &stat);
    MPI_Get_count( &stat, REAL, &recv_buf_east.n );
    process_buffer( &recv_buf_east, (cell *) NULL );

    /* append atoms from west and east into north send buffer */
    copy_atoms_buf( &send_buf_north, &recv_buf_west );
    copy_atoms_buf( &send_buf_north, &recv_buf_east );
    /* check special case cpu_dim.y==2 */ 
    if (nbsouth!=nbnorth) {
      /* append atoms from west and east to south send buffer */
      copy_atoms_buf( &send_buf_south, &recv_buf_west );
      copy_atoms_buf( &send_buf_south, &recv_buf_east );
    }
  }

  if (cpu_dim.y > 1) {
    /* send north, receive south, move atoms from south to cells */
    sendrecv_buf(  &send_buf_north, nbnorth, &recv_buf_south, nbsouth, &stat);
    MPI_Get_count( &stat, REAL, &recv_buf_south.n );
    process_buffer( &recv_buf_south, (cell *) NULL );

    /* send south, receive north, move atoms from north to cells */
    sendrecv_buf( &send_buf_south, nbsouth, &recv_buf_north, nbnorth, &stat);
    MPI_Get_count( &stat, REAL, &recv_buf_north.n );
    process_buffer( &recv_buf_north, (cell *) NULL );

    /* append atoms from north, south, east, west to up send buffer */
    copy_atoms_buf( &send_buf_up, &recv_buf_north );
    copy_atoms_buf( &send_buf_up, &recv_buf_south );
    copy_atoms_buf( &send_buf_up, &recv_buf_east  );
    copy_atoms_buf( &send_buf_up, &recv_buf_west  );
    /* check special case cpu_dim.z==2 */ 
    if (nbdown!=nbup) {
      /* append atoms from north, south, east, west to down send buffer */
      copy_atoms_buf( &send_buf_down, &recv_buf_north );
      copy_atoms_buf( &send_buf_down, &recv_buf_south );
      copy_atoms_buf( &send_buf_down, &recv_buf_east  );
      copy_atoms_buf( &send_buf_down, &recv_buf_west  );
    }
  }

  if (cpu_dim.z > 1) {
    /* send up, receive down, move atoms from down to cells */
    sendrecv_buf( &send_buf_up, nbup, &recv_buf_down, nbdown, &stat);
    MPI_Get_count( &stat, REAL, &recv_buf_down.n );
    process_buffer( &recv_buf_down, (cell *) NULL );
  
    /* send down, receive up, move atoms from up to cells */
    sendrecv_buf( &send_buf_down, nbdown, &recv_buf_up, nbup, &stat );
    MPI_Get_count( &stat, REAL, &recv_buf_up.n );
    process_buffer( &recv_buf_up, (cell *) NULL );
  }

}

#else /* not SR */

/******************************************************************************
*
* send_atoms  -  only used for fix_cells
*
******************************************************************************/

void send_atoms()
{
  MPI_Status  stateast[2],  statwest[2];
  MPI_Status statnorth[2], statsouth[2];
  MPI_Status    statup[2],  statdown[2];

  MPI_Request  reqeast[2],   reqwest[2];
  MPI_Request reqnorth[2],  reqsouth[2];
  MPI_Request    requp[2],   reqdown[2];

  if (cpu_dim.x > 1) {
    /* send east */
    irecv_buf( &recv_buf_west, nbwest, &reqwest[1] );
    isend_buf( &send_buf_east, nbeast, &reqwest[0] );

    /* send west */
    irecv_buf( &recv_buf_east, nbeast, &reqeast[1] );
    isend_buf( &send_buf_west, nbwest, &reqeast[0] );

    /* Wait for atoms from west, move them to cells */
    MPI_Waitall(2, reqwest, statwest);
    MPI_Get_count( &statwest[1], REAL, &recv_buf_west.n );
    process_buffer( &recv_buf_west, (cell *) NULL );

    /* Wait for atoms from east, move them to cells */
    MPI_Waitall(2, reqeast, stateast);
    MPI_Get_count( &stateast[1], REAL, &recv_buf_east.n );
    process_buffer( &recv_buf_east, (cell *) NULL );

    /* append atoms from west and east into north send buffer */
    copy_atoms_buf( &send_buf_north, &recv_buf_west );
    copy_atoms_buf( &send_buf_north, &recv_buf_east );
    /* check special case cpu_dim.y==2 */ 
    if (nbsouth!=nbnorth) {
      /* append atoms from west and east to south send buffer */
      copy_atoms_buf( &send_buf_south, &recv_buf_east );
      copy_atoms_buf( &send_buf_south, &recv_buf_west );
    }
  }

  if (cpu_dim.y > 1) {
    /* Send atoms north */
    irecv_buf( &recv_buf_south, nbsouth, &reqsouth[1] );
    isend_buf( &send_buf_north, nbnorth, &reqsouth[0] );
  
    /* Send atoms south */
    irecv_buf( &recv_buf_north, nbnorth, &reqnorth[1] );
    isend_buf( &send_buf_south, nbsouth, &reqnorth[0] );

    /* Wait for atoms from south, move them to cells */
    MPI_Waitall(2, reqsouth, statsouth);
    MPI_Get_count( &statsouth[1], REAL, &recv_buf_south.n );
    process_buffer( &recv_buf_south, (cell *) NULL );

    /* Wait for atoms from north, move them to cells */
    MPI_Waitall(2, reqnorth, statnorth);
    MPI_Get_count( &statnorth[1], REAL, &recv_buf_north.n );
    process_buffer( &recv_buf_north, (cell *) NULL );

    /* append atoms from north, south, east, west to up send buffer */
    copy_atoms_buf( &send_buf_up, &recv_buf_north );
    copy_atoms_buf( &send_buf_up, &recv_buf_south );
    copy_atoms_buf( &send_buf_up, &recv_buf_east  );
    copy_atoms_buf( &send_buf_up, &recv_buf_west  );
    /* check special case cpu_dim.z==2 */ 
    if (nbdown!=nbup) {
      /* append atoms from north, south, east, west to down send buffer */
      copy_atoms_buf( &send_buf_down, &recv_buf_north );
      copy_atoms_buf( &send_buf_down, &recv_buf_south );
      copy_atoms_buf( &send_buf_down, &recv_buf_east  );
      copy_atoms_buf( &send_buf_down, &recv_buf_west  );
    }
  }

  if (cpu_dim.z > 1) {
    /* Send atoms up */
    irecv_buf( &recv_buf_down , nbdown, &reqdown[1]);
    isend_buf( &send_buf_up   , nbup  , &reqdown[0]);
  
    /* Send atoms down */
    irecv_buf( &recv_buf_up  , nbup  , &requp[1] );
    isend_buf( &send_buf_down, nbdown, &requp[0] );

    /* Wait for atoms from down, move them to cells */
    MPI_Waitall(2, reqdown, statdown);
    MPI_Get_count( &statdown[1], REAL, &recv_buf_down.n );
    process_buffer( &recv_buf_down, (cell *) NULL );

    /* Wait for atoms from up, move them to cells */
    MPI_Waitall(2, requp, statup);
    MPI_Get_count( &statup[1], REAL, &recv_buf_up.n );
    process_buffer( &recv_buf_up, (cell *) NULL );
  }

}

#endif /* not SR */
