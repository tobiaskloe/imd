
/******************************************************************************
*
* IMD -- The ITAP Molecular Dynamics Program
*
* Copyright 1996-2001 Institute for Theoretical and Applied Physics,
* University of Stuttgart, D-70550 Stuttgart
*
******************************************************************************/

/*****************************************************************************
*
* imd_forces -- force loop
*
******************************************************************************/

/******************************************************************************
* $Revision$
* $Date$
******************************************************************************/

#include "imd.h"
#include "potaccess.h"

/******************************************************************************
*
*  do_forces, version for scalar processors
*
*  computes the forces between atoms in two given cells
*
******************************************************************************/

void do_forces(cell *p, cell *q, vektor pbc, real *Epot, real *Virial, 
               real *Vir_xx, real *Vir_yy, real *Vir_zz,
               real *Vir_yz, real *Vir_zx, real *Vir_xy)
{
  int i,j,k;
  vektor d;
  vektor tmp_d;
  vektor force;
  real r2, rho_h;
  real tmp_virial;
#ifdef P_AXIAL
  vektor tmp_vir_vect;
#endif
  real pot_zwi, pot_grad;
  int col, col2, is_short=0, inc = ntypes * ntypes;
  int jstart, q_typ, p_typ;
  real *qptr, *pfptr, *qfptr, *qpdptr, *ppdptr, *qpoptr, *ppoptr;
  
  tmp_virial     = 0.0;
#ifdef P_AXIAL
  tmp_vir_vect.x = 0.0;
  tmp_vir_vect.y = 0.0;
#ifndef TWOD
  tmp_vir_vect.z = 0.0;
#endif
#endif
    
  /* for each atom in first cell */
  for (i=0; i<p->n; ++i) {

    tmp_d.x = ORT(p,i,X) - pbc.x;
    tmp_d.y = ORT(p,i,Y) - pbc.y;
#ifndef TWOD
    tmp_d.z = ORT(p,i,Z) - pbc.z;
#endif

    p_typ  = SORTE(p,i);
#ifdef TWOD
    jstart = (((p==q) && (pbc.x==0) && (pbc.y==0))               ? i+1 : 0);
#else
    jstart = (((p==q) && (pbc.x==0) && (pbc.y==0) && (pbc.z==0)) ? i+1 : 0);
#endif
    qptr   = &ORT(q,jstart,X);

    /* for each atom in neighbouring cell */
    for (j = jstart; j < q->n; ++j) {

      /* calculate distance */
      d.x = *qptr - tmp_d.x; ++qptr;
      d.y = *qptr - tmp_d.y; ++qptr;
#ifndef TWOD
      d.z = *qptr - tmp_d.z; ++qptr;
#endif

      q_typ = SORTE(q,j);
      col   = p_typ * ntypes + q_typ;
      r2    = SPROD(d,d);

#ifdef DEBUG
      if (0==r2) { char msgbuf[256];
        sprintf(msgbuf, "Distance is zero between particles %d and %d!\n",
                NUMMER(p,i), NUMMER(q,j));
        error(msgbuf);
      }
#endif

      /* compute pair interactions */
#if defined(PAIR) || defined(PAIR_PRE) || defined(KEATING) || defined(STIWEB)
#ifdef PAIR_PRE
      pot_zwi  = 0.0;
      pot_grad = 0.0;
      if (r2 < r2_cut[p_typ][q_typ]) {
	  if ( !use_pot_table ) {
#ifdef LJ
	      PAIR_INT_LJ(pot_zwi, pot_grad, p_typ, q_typ, r2)
#endif

#ifdef MORSE
	      PAIR_INT_MORSE(pot_zwi, pot_grad, p_typ, q_typ, r2)
#endif

#ifdef BUCK
	      PAIR_INT_BUCK(pot_zwi, pot_grad, p_typ, q_typ, r2) 
#endif
	  }
	  else 
              PAIR_INT(pot_zwi, pot_grad, pair_pot, col, inc, r2, is_short)   
#elif MONOLJ
      if (r2 <= monolj_r2_cut) {
        PAIR_INT_MONOLJ(pot_zwi, pot_grad, r2)
#elif KEATING
      if (r2 < keat_r2_cut[p_typ][q_typ]) {
	PAIR_INT_KEATING(pot_zwi, pot_grad, p_typ, q_typ, r2)
#elif STIWEB  
      if (r2 < sw_2_a1[p_typ][q_typ]) {
	PAIR_INT_STIWEB(pot_zwi, pot_grad, p_typ, q_typ, r2)
#else
      if (r2 <= pair_pot.end[col]) {
        PAIR_INT(pot_zwi, pot_grad, pair_pot, col, inc, r2, is_short)
#endif
#ifdef SPRING
      if ( spring_cst[p_typ][q_typ] != 0.0 )
 	  if ( r2 < spring_r2_cut[p_typ][q_typ] ) {
	      pot_zwi  = 0.5 * spring_cst[p_typ][q_typ] * r2;
	      pot_grad = spring_cst[p_typ][q_typ];
	  }
#endif

        /* store force in temporary variable */
        force.x = d.x * pot_grad;
        force.y = d.y * pot_grad;
#ifndef TWOD
        force.z = d.z * pot_grad;
#endif

        /* accumulate forces */
        pfptr = &KRAFT(p,i,X);
        qfptr = &KRAFT(q,j,X);
        *pfptr     += force.x; 
        *qfptr     -= force.x; 
        *(++pfptr) += force.y; 
        *(++qfptr) -= force.y; 
#ifndef TWOD
        *(++pfptr) += force.z; 
        *(++qfptr) -= force.z; 
#endif
        *Epot      += pot_zwi;

#ifndef MONOLJ
#ifdef EAM2
        pot_zwi *= 0.5;   /* avoid double counting */
#endif
#ifdef ORDPAR
        if (r2 < op_r2_cut[p_typ][q_typ]) {
	  POTENG(p,i) += op_weight[p_typ][q_typ] * pot_zwi;
	  POTENG(q,j) += op_weight[q_typ][p_typ] * pot_zwi;
	  NBANZ(p,i)++;
	  NBANZ(q,j)++;
        }
#else
        POTENG(p,i) += pot_zwi;
        POTENG(q,j) += pot_zwi;
#endif
#endif

#ifdef P_AXIAL
        tmp_vir_vect.x -= d.x * force.x;
        tmp_vir_vect.y -= d.y * force.y;
#ifndef TWOD
        tmp_vir_vect.z -= d.z * force.z;
#endif
#else
        tmp_virial     -= r2 * pot_grad;
#endif

#ifdef STRESS_TENS
        if (do_press_calc) {
          /* avoid double counting of the virial */
          force.x *= 0.5;
          force.y *= 0.5;
#ifndef TWOD
          force.z *= 0.5;
#endif
          PRESSTENS(p,i,xx) -= d.x * force.x;
          PRESSTENS(q,j,xx) -= d.x * force.x;
          PRESSTENS(p,i,yy) -= d.y * force.y;
          PRESSTENS(q,j,yy) -= d.y * force.y;
          PRESSTENS(p,i,xy) -= d.x * force.y;
          PRESSTENS(q,j,xy) -= d.x * force.y;
#ifndef TWOD
          PRESSTENS(p,i,zz) -= d.z * force.z;
          PRESSTENS(q,j,zz) -= d.z * force.z;
          PRESSTENS(p,i,yz) -= d.y * force.z;
          PRESSTENS(q,j,yz) -= d.y * force.z;
          PRESSTENS(p,i,zx) -= d.z * force.x;
          PRESSTENS(q,j,zx) -= d.z * force.x;
#endif
	}
#endif

#ifdef NVX
        HEATCOND(p,i) += pot_zwi - r2 * pot_grad;
        HEATCOND(q,j) += pot_zwi - r2 * pot_grad;
#endif
      }
#endif /* PAIR || PAIR_PRE */

#ifdef EAM2
      /* compute host electron density */
      if (r2 < rho_h_tab.end[col])  {
        VAL_FUNC(rho_h, rho_h_tab, col,  inc, r2, is_short);
        EAM_RHO(p,i) += rho_h; 
      }
      if (p_typ==q_typ) {
        if (r2 < rho_h_tab.end[col]) EAM_RHO(q,j) += rho_h; 
      } else {
        col2 = q_typ * ntypes + p_typ;
        if (r2 < rho_h_tab.end[col2]) {
          VAL_FUNC(rho_h, rho_h_tab, col2, inc, r2, is_short);
          EAM_RHO(q,j) += rho_h; 
        }
      }
#endif

#ifdef COVALENT
      /* make neighbor tables for covalent systems */
#ifdef KEATING
      if (r2 < keat_r2_cut[p_typ][q_typ]) {
#endif
#ifdef TTBP
      if (r2 <= smooth_pot.end[col]) {
#endif
#ifdef STIWEB
      if (r2 < sw_2_a1[p_typ][q_typ]) {
#endif
#ifdef TERSOFF
      if (r2 <= ter_r2_cut[p_typ][q_typ]) {
#endif 
        neightab *neigh;
        real  *tmp_ptr;

        /* update neighbor table of particle i */
        neigh = NEIGH(p,i);
        if (neigh->n_max <= neigh->n) {
          error("neighbor table too small, increase neigh_len");
        }
        neigh->typ[neigh->n] = q_typ;
        neigh->cl [neigh->n] = q;
        neigh->num[neigh->n] = j;
        tmp_ptr  = &neigh->dist[3*neigh->n];
        *tmp_ptr = d.x; ++tmp_ptr; 
        *tmp_ptr = d.y; ++tmp_ptr; 
        *tmp_ptr = d.z;
        neigh->n++;

        /* update neighbor table of particle j */
        neigh = NEIGH(q,j);
        if (neigh->n_max <= neigh->n) {
          error("neighbor table too small, increase neigh_len");
        }
        neigh->typ[neigh->n] = p_typ;
        neigh->cl [neigh->n] = p;
        neigh->num[neigh->n] = i;
        tmp_ptr  = &neigh->dist[3*neigh->n];
        *tmp_ptr = -d.x; ++tmp_ptr; 
        *tmp_ptr = -d.y; ++tmp_ptr; 
        *tmp_ptr = -d.z;
        neigh->n++;
      }
#endif  /* COVALENT */

    } /* for j */
  } /* for i */

#ifndef TERSOFF
#ifndef MONOLJ
  if (is_short==1) printf("\n Short distance!\n");
#endif
#ifdef P_AXIAL
  *Vir_xx += tmp_vir_vect.x;
  *Vir_yy += tmp_vir_vect.y;
  *Virial += tmp_vir_vect.x;
  *Virial += tmp_vir_vect.y;
#ifndef TWOD
  *Vir_zz += tmp_vir_vect.z;
  *Virial += tmp_vir_vect.z;
#endif
#else
  *Virial += tmp_virial;
#endif 
#endif /* TERSOFF */ 

}






