/* 
 * MBDyn (C) is a multibody analysis code. 
 * http://www.mbdyn.org
 *
 * Copyright (C) 1996-2004
 *
 * Pierangelo Masarati	<masarati@aero.polimi.it>
 * Paolo Mantegazza	<mantegazza@aero.polimi.it>
 *
 * Dipartimento di Ingegneria Aerospaziale - Politecnico di Milano
 * via La Masa, 34 - 20156 Milano, Italy
 * http://www.aero.polimi.it
 *
 * Changing this copyright notice is forbidden.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation (version 2 of the License).
 * 
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

/*
 * The Naive Solver is copyright (C) 2004 by
 * Paolo Mantegazza <mantegazza@aero.polimi.it>
 */

#ifdef HAVE_CONFIG_H
#include <mbconfig.h>           /* This goes first in every *.c,*.cc file */
#endif /* HAVE_CONFIG_H */

#include <sys/types.h>
#include <unistd.h>
#include <signal.h>

#include "spmh.h"
#include "spmapmh.h"
#include "naivewrap.h"
#include "mthrdslv.h"

/* NaiveSolver - begin */
	
NaiveSolver::NaiveSolver(const integer &size, const doublereal& dMP,
		NaiveMatrixHandler *const a)
: LinearSolver(0),
iSize(size),
dMinPiv(dMP),
A(a),
piv(size)
{
	NO_OP;
}

NaiveSolver::~NaiveSolver(void)
{
	NO_OP;
}

void
NaiveSolver::Init(void)
{
	bHasBeenReset = true;
}

void
NaiveSolver::Solve(void) const
{
	if (bHasBeenReset) {
      		((NaiveSolver *)this)->Factor();
      		bHasBeenReset = false;
	}

	naivslv(A->ppdRows, iSize, A->piNzc, A->ppiCols,
			LinearSolver::pdRhs, &piv[0]);
}

void
NaiveSolver::Factor(void)
{
	int		rc;

#if 0
	integer i = 0;
	for (integer j = 0; j < iSize; j++) {
		i += A->piNzr[j];
	}
	std::cerr << "prima: " << i << std::endl;
#endif
	
	rc = naivfct(A->ppdRows, iSize,
			A->piNzr, A->ppiRows, 
			A->piNzc, A->ppiCols,
			A->ppnonzero, 
			&piv[0], dMinPiv);

#if 0
	i = 0;
	for (integer j = 0; j < iSize; j++) {
		i += A->piNzr[j];
	}
	std::cerr << "dopo: " << i << std::endl;
#endif
	
	if (rc) {
		if (rc & ENULCOL) {
			silent_cerr("NaiveSolver: ENULCOL("
					<< (rc & ~ENULCOL) << ")" << std::endl);
			THROW(ErrGeneric());
		}

		if (rc & ENOPIV) {
			silent_cerr("NaiveSolver: ENOPIV("
					<< (rc & ~ENOPIV) << ")" << std::endl);
			THROW(ErrGeneric());
		}

		/* default */
		THROW(ErrGeneric());
	}
}

/* NaiveSolver - end */

/* NaiveSparseSolutionManager - begin */

NaiveSparseSolutionManager::NaiveSparseSolutionManager(integer Dim,
		const doublereal dMP)
: A(Dim),
VH(Dim)
{
	SAFENEWWITHCONSTRUCTOR(pLS, NaiveSolver, NaiveSolver(Dim, dMP, &A));

	(void)pLS->ChangeResPoint(VH.pdGetVec());
	(void)pLS->ChangeSolPoint(VH.pdGetVec());

	pLS->SetSolutionManager(this);
}

NaiveSparseSolutionManager::~NaiveSparseSolutionManager(void) 
{
	NO_OP;
}

void
NaiveSparseSolutionManager::MatrReset(const doublereal d)
{
	A.Reset(d);
}

void
NaiveSparseSolutionManager::MatrInit(const doublereal d)
{
	MatrReset(d);
	pLS->Init();
}

/* Risolve il sistema  Fattorizzazione + Bacward Substitution*/
void
NaiveSparseSolutionManager::Solve(void)
{
	pLS->Solve();
}

/* Rende disponibile l'handler per la matrice */
MatrixHandler*
NaiveSparseSolutionManager::pMatHdl(void) const
{
	return &A;
}

/* Rende disponibile l'handler per il termine noto */
MyVectorHandler*
NaiveSparseSolutionManager::pResHdl(void) const
{
	return &VH;
}

/* Rende disponibile l'handler per la soluzione */
MyVectorHandler*
NaiveSparseSolutionManager::pSolHdl(void) const
{
	return &VH;
}

/* NaiveSparseSolutionManager - end */

