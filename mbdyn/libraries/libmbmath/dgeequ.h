/* $Header$ */
/* 
 * MBDyn (C) is a multibody analysis code. 
 * http://www.mbdyn.org
 *
 * Copyright (C) 1996-2008
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

#ifndef DGEEQU_H
#define DGEEQU_H

#include <vector>

#include "mh.h"

#ifdef USE_LAPACK
#include "fullmh.h"
#endif // USE_LAPACK

//#include "spmapmh.h"
//#include "ccmh.h"
//#include "dirccmh.h"
//#include "naivemh.h"

void
dgeequ_prepare(const MatrixHandler& mh,
	std::vector<doublereal>& r, std::vector<doublereal>& c,
	integer& nrows, integer& ncols);

template <class T, class Titer>
void
dgeequ(const T& mh, std::vector<doublereal>& r, std::vector<doublereal>& c,
	doublereal& rowcnd, doublereal& colcnd, doublereal& amax)
{
	integer nrows, ncols;
	dgeequ_prepare(mh, r, c, nrows, ncols);

	const doublereal SMLNUM = 1.e-15;
	const doublereal BIGNUM = 1/SMLNUM;

	doublereal rcmin;
	doublereal rcmax;

	for (Titer i = mh.begin(); i != mh.end(); ++i) {
		doublereal d = std::abs(i->dCoef);
		if (d > r[i->iRow]) {
			r[i->iRow] = d;
		}
	}

	rcmin = BIGNUM;
	rcmax = 0.;
	for (std::vector<doublereal>::iterator i = r.begin(); i != r.end(); ++i) {
		if (*i > rcmax) {
			rcmax = *i;
		}
		if (*i < rcmin) {
			rcmin = *i;
		}
	}

	amax = rcmax;

	if (rcmin == 0.) {
		// error
	}

	for (std::vector<doublereal>::iterator i = r.begin(); i != r.end(); ++i) {
		*i = 1./(std::min(std::max(*i, SMLNUM), BIGNUM));
	}

	rowcnd = std::max(rcmin, SMLNUM)/std::min(rcmax, BIGNUM);

	for (Titer i = mh.begin(); i != mh.end(); ++i) {
		doublereal d = std::abs(i->dCoef)*r[i->iRow];
		if (d > c[i->iCol]) {
			c[i->iCol] = d;
		}
	}

	rcmin = BIGNUM;
	rcmax = 0.;
	for (std::vector<doublereal>::iterator i = c.begin(); i != c.end(); ++i) {
		if (*i > rcmax) {
			rcmax = *i;
		}
		if (*i < rcmin) {
			rcmin = *i;
		}
	}

	if (rcmin == 0.) {
		// error
	}

	for (std::vector<doublereal>::iterator i = c.begin(); i != c.end(); ++i) {
		*i = 1./(std::min(std::max(*i, SMLNUM), BIGNUM));
	}

	colcnd = std::max(rcmin, SMLNUM)/std::min(rcmax, BIGNUM);
}

#ifdef USE_LAPACK
void
dgeequ(const FullMatrixHandler& mh,
	std::vector<doublereal>& r, std::vector<doublereal>& c,
	doublereal& rowcnd, doublereal& colcnd, doublereal& amax);
#endif // USE_LAPACK

#endif // DGEEQU_H
