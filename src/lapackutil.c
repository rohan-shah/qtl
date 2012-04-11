/**********************************************************************
 *
 * lapackutil.c
 *
 * copyright (c) 2006-2012, Hao Wu and Karl Broman
 *
 * last modified Apr, 2012
 * first written Jan, 2006 
 *
 *     This program is free software; you can redistribute it and/or
 *     modify it under the terms of the GNU General Public License,
 *     version 3, as published by the Free Software Foundation.
 * 
 *     This program is distributed in the hope that it will be useful,
 *     but without any warranty; without even the implied warranty of
 *     merchantability or fitness for a particular purpose.  See the GNU
 *     General Public License, version 3, for more details.
 * 
 *     A copy of the GNU General Public License, version 3, is available
 *     at http://www.r-project.org/Licenses/GPL-3
 *
 * C functions for the R/qtl package
 *
 * These are some wrapper functions for several LAPACK routines.
 *
 * Contains: mydgelss, mydgemm, mydpotrf, mydpotrs
 *           linreg_rss, setup_linreg_rss;
 *
 **********************************************************************/

#include <math.h>
#include <stdlib.h>
#include <stdio.h>
#include <R.h>
#include <Rmath.h>
#include <R_ext/PrtUtil.h>
#include <R_ext/Applic.h>
#include <R_ext/Lapack.h>
#include "lapackutil.h"
#include "util.h"
#define TOL 1e-12


/* DGELSS function */
void mydgelss (int *n_ind, int *ncolx0, int *nphe, double *x0, double *x0_bk,
               double *pheno, double *tmppheno, double *s, double *tol, 
               int *rank, double *work, int *lwork, int *info)
{
  int i, singular=0;

  /* use dgels first */
  F77_CALL(dgels)("N", n_ind, ncolx0, nphe, x0, n_ind, tmppheno, n_ind,
		  work, lwork, info);
  
  /* if there's problem like singular, use dgelss */
  /* note that x0 will contain the result for QR decomposition. 
  If any diagonal element of R is zero, then input x0 is rank deficient */
  for(i=0; i<*ncolx0; i++)  {
    if(abs(x0[*n_ind*i+i]) < TOL) {
      singular = 1;
      break;
    }
  }

  
  if(singular) { /* switch to dgelss if input x0 is not of full rank */
    /* note that tmppheno and x0 have been destroyed already,
    we need to make another copy of them */
    /*mexPrintf("Warning - Design matrix is signular \n"); */
    /* note that at this stage both x0 and tmppheno might be destroyed,
    we need to make a copy of them */

    memcpy(x0, x0_bk, *n_ind*(*ncolx0)*sizeof(double));
    memcpy(tmppheno, pheno, *n_ind*(*nphe)*sizeof(double));
    F77_CALL(dgelss) (n_ind, ncolx0, nphe, x0, n_ind, tmppheno, n_ind, 
      s, tol, rank, work, lwork, info);
  }
}


/* DGEMM */
void mydgemm(int *nphe, int *n_ind, double *alpha, double *tmppheno,
             double *beta, double *rss_det) 
{
  F77_CALL(dgemm)("T", "N", nphe, nphe, n_ind, alpha, tmppheno, n_ind, 
             tmppheno, n_ind, beta, rss_det, nphe);
}

/* DPOTRF */
void mydpotrf(int *nphe, double *rss_det, int *info) 
{
  F77_CALL(dpotrf)("U", nphe, rss_det, nphe, info);
}

/*DPOTRS */
void mydpotrs(char *uplo, int *n, int *nrhs, double *A, 
              int *lda, double *B, int *ldb, int *info)
{
  F77_CALL(dpotrs)(uplo, n, nrhs, A, lda, B, ldb, info);
}


void setup_linreg_rss(int nrow, int ncolx, int ncoly, 
                      int *lwork, double **dwork, int **jpvt)
{
  int mn;

  *jpvt = (double *)R_alloc(ncolx, sizeof(int));

  mn = MIN(nrow, ncolx);
  
  *lwork = MAX(mn + MAX(mn, ncoly), MAX(mn + 3*ncolx + 1, 2*mn*ncoly));
  *dwork = (double *)R_alloc(*lwork, sizeof(double));
}

void linreg_rss(int nrow, int ncolx, double *x, int ncoly, double *y,
                double *rss, int lwork, double *dwork, int *jpvt,
                double *xcopy, double *ycopy, double tol)
{
  int i, j, lda, ldb, info, rank, row_index;
  char notranspose='N';

  lda=nrow;
  ldb=nrow;
  allocate_double(ncolx, &jpvt);
  
  /* fill rss with 0's */
  for(i=0; i<ncoly; i++) rss[i] = 0.0;

  auto jpvt = new int[ncolx];
  foreach(i; 0..ncolx) jpvt[i] = 0;  // keeps track of pivoted columns

  F77_CALL(dgelsy)(&nrow, &ncolx, &ncoly, x, &lda, y, &ldb, jpvt, &tol, 
                   &rank, work, &lwork, &info);

  if(rank < ncolx) { // x has < full rank
    // restore x, saving just the first rank columns after pivoting
    for(i=0; i<rank; i++)
      memcpy(x+(i*nrow), xcopy+(jpvt[i]-1)*nrow, nrow*sizeof(double));

    // restore y
    memcpy(y, ycopy, nrow*ncoly*sizeof(double));

    // now run gels (which assumes x has full rank)
    
    F77_CALL(dgels)(&notranspose, &nrow, &rank, &ncoly, x, &lda, y, &ldb, work, &lwork, &info);
  }

  // calculate RSS
  row_index = 0;
  for(i=0; i<ncoly; i++) {
    for(j=rank; j<nrow; j++)
      rss[i] += y[row_index+j]*y[row_index+j];
    row_index += nrow;
  }
}

/* end of lapackutil.c */
