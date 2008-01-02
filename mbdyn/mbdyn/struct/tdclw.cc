/* $Header$ */
/*
 * MBDyn (C) is a multibody analysis code.
 * http://www.mbdyn.org
 *
 * Copyright (C) 1996-2007
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

/* Cerniera deformabile */

#ifdef HAVE_CONFIG_H
#include "mbconfig.h"           /* This goes first in every *.c,*.cc file */
#endif /* HAVE_CONFIG_H */

#include "dataman.h"
#include "tdclw.h"

/* TDConstitutiveLawWrapper - begin */

template <class T, class Tder>
class TDConstitutiveLawWrapper
: public ConstitutiveLaw<T, Tder> {
private:
	doublereal dF, dL, dLCurr;
	T FPrev;
	ConstitutiveLaw<T, Tder> *pCL;

public:
	TDConstitutiveLawWrapper(const doublereal& df,
		const doublereal& dl,
		ConstitutiveLaw<T, Tder> *pcl);
	virtual ~TDConstitutiveLawWrapper(void);

	ConstLawType::Type GetConstLawType(void) const;

	virtual ConstitutiveLaw<T, Tder>* pCopy(void) const;
	virtual std::ostream& Restart(std::ostream& out) const;

	virtual void Update(const T& Eps, const T& EpsPrime = 0.);
	virtual void IncrementalUpdate(const T& DeltaEps, const T& EpsPrime = 0.);

	virtual void AfterConvergence(const T& Eps, const T& EpsPrime = 0.);
};

template <class T, class Tder>
TDConstitutiveLawWrapper<T, Tder>::TDConstitutiveLawWrapper(const doublereal& df,
	const doublereal& dl, ConstitutiveLaw<T, Tder> *pcl)
: dF(df), dL(dl), dLCurr(0.), FPrev(0.), pCL(pcl)
{
	NO_OP;
}

template <class T, class Tder>
TDConstitutiveLawWrapper<T, Tder>::~TDConstitutiveLawWrapper(void)
{
	if (pCL) {
		delete pCL;
	}
}

template <class T, class Tder>
ConstLawType::Type
TDConstitutiveLawWrapper<T, Tder>::GetConstLawType(void) const
{
	return pCL->GetConstLawType();
}

template <class T, class Tder>
ConstitutiveLaw<T, Tder>*
TDConstitutiveLawWrapper<T, Tder>::pCopy(void) const
{
	ConstitutiveLaw<T, Tder>* pcl = NULL;

	typedef TDConstitutiveLawWrapper cl;
	SAFENEWWITHCONSTRUCTOR(pcl, cl, cl(dF, dL, pCL->pCopy()));
	return pcl;
}

template <class T, class Tder>
std::ostream&
TDConstitutiveLawWrapper<T, Tder>::Restart(std::ostream& out) const
{
	out << "tdclw, " << dF << ", " << dL << ", ";
	return pCL->Restart(out);
}

template <class T, class Tder>
void
TDConstitutiveLawWrapper<T, Tder>::Update(const T& Eps, const T& EpsPrime)
{
	pCL->Update(Eps, EpsPrime);

	doublereal d = 1. + dF*exp(-dLCurr/dL);

	ConstitutiveLaw<T, Tder>::F = pCL->GetF()*d;
	if (GetConstLawType() & ConstLawType::ELASTIC) {
		ConstitutiveLaw<T, Tder>::FDE = pCL->GetFDE()*d;
	}
	if (GetConstLawType() & ConstLawType::VISCOUS) {
		ConstitutiveLaw<T, Tder>::FDEPrime = pCL->GetFDEPrime()*d;
	}
}

template <class T, class Tder>
void
TDConstitutiveLawWrapper<T, Tder>::IncrementalUpdate(const T& DeltaEps, const T& EpsPrime)
{
	Update(ConstitutiveLaw<T, Tder>::Epsilon + DeltaEps, EpsPrime);
}

template <class T, class Tder>
void
TDConstitutiveLawWrapper<T, Tder>::AfterConvergence(const T& Eps, const T& EpsPrime)
{
	// average force * (old - new epsilon, to avoid unary - operator
	dLCurr += ((pCL->GetF() + FPrev)*(ConstitutiveLaw<T, Tder>::Epsilon - Eps))/2.;

	ConstitutiveLaw<T, Tder>::Epsilon = Eps;
	FPrev = pCL->GetF();
}

/* TDConstitutiveLawWrapper - end */


/* TDCLWR - begin */

template <class T, class Tder>
struct TDCLWR : public ConstitutiveLawRead<T, Tder> {
	virtual ConstitutiveLaw<T, Tder> *
	Read(const DataManager* pDM, MBDynParser& HP, ConstLawType::Type& CLType);
};

template <class T, class Tder>
ConstitutiveLaw<T, Tder> *
TDCLWR<T, Tder>::Read(const DataManager* pDM, MBDynParser& HP, ConstLawType::Type& CLType)
{
	ConstitutiveLaw<T, Tder>* pCL = 0;

	doublereal dF = HP.GetReal();
	if (dF <= 0.) {
		silent_cerr("Invalid negative or null delta force in TDCLW "
			"at line " << HP.GetLineData() << std::endl);
		throw ErrGeneric();
	}

	doublereal dL = HP.GetReal();
	if (dL >= 0.) {
		silent_cerr("Invalid positive or null reference work in TDCLW "
			"at line " << HP.GetLineData() << std::endl);
		throw ErrGeneric();
	}

	ConstitutiveLaw<T, Tder>* pCL2;
	if (typeid(T) == typeid(doublereal)) {
		pCL2 = dynamic_cast<ConstitutiveLaw<T, Tder> *>(pDM->ReadConstLaw1D(HP, CLType));
	} else if (typeid(T) == typeid(Vec3)) {
		pCL2 = dynamic_cast<ConstitutiveLaw<T, Tder> *>(pDM->ReadConstLaw3D(HP, CLType));
	} else if (typeid(T) == typeid(Vec6)) {
		pCL2 = dynamic_cast<ConstitutiveLaw<T, Tder> *>(pDM->ReadConstLaw6D(HP, CLType));
	} else {
		throw ErrGeneric();
	}

	typedef TDConstitutiveLawWrapper<T, Tder> L;
	SAFENEWWITHCONSTRUCTOR(pCL, L, L(dF, dL, pCL2));

	return pCL;
}

/* TDCLWR - end */

void
TDCLW_init(void)
{
	SetCL1D("tdclw", new TDCLWR<doublereal, doublereal>);
	SetCL3D("tdclw", new TDCLWR<Vec3, Mat3x3>);
	SetCL6D("tdclw", new TDCLWR<Vec6, Mat6x6>);
}