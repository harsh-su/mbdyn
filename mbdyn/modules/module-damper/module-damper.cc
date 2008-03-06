/* 
 * MBDyn (C) is a multibody analysis code. 
 * http://www.mbdyn.org
 *
 * Copyright (C) 2007
 *
 * Marco Morandini	<morandini@aero.polimi.it>
 * Pierangelo Masarati	<masarati@aero.polimi.it>
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
 * This module was sponsored by AgustaWestland.
 */

#ifdef HAVE_CONFIG_H
#include <mbconfig.h>           /* This goes first in every *.c,*.cc file */
#endif /* HAVE_CONFIG_H */


#include "dataman.h"
#include "constltp.h"
#include "drive_.h"

#include "nlrheo_damper.h"

class DamperConstitutiveLaw
: public ConstitutiveLaw<doublereal, doublereal> {
private:
	sym_params& pa;
	DriveCaller *pTime;

public:
	DamperConstitutiveLaw(sym_params & pap, DriveCaller *pT)
	: pa(pap), pTime(pT)
	{
		ConstitutiveLaw<doublereal, doublereal>::FDE =  0.; //FIXME dStiffness;

		if (nlrheo_init(&pa)) {
			silent_cerr("DamperConstitutiveLaw: init failed" << std::endl);
			throw ErrGeneric();
		}
	};

	virtual ~DamperConstitutiveLaw(void) {
		SAFEDELETE(pTime);
		(void)(nlrheo_destroy(&pa));
	};

	virtual ConstitutiveLaw<doublereal, doublereal>* pCopy(void) const {
		silent_cerr("DamperConstitutiveLaw1D::pCopy "
			"not implemented.\n" 
			"Please build explicitly "
			"different instances of the constitutive law" << std::endl);
		throw ErrGeneric();
		return 0;
	};

	ConstLawType::Type GetConstLawType(void) const {
		return ConstLawType::VISCOELASTIC;
	};

	virtual void Update(const doublereal& Eps, const doublereal& EpsPrime = 0.) {
		if (nlrheo_update(&pa, pTime->dGet(), Eps, EpsPrime, 1)) {
			silent_cerr("DamperConstitutiveLaw: unable to update" << std::endl);
			throw ErrGeneric();
		}
	};

	virtual void AfterConvergence(const doublereal& Eps, const doublereal& EpsPrime = 0.) {
		if (nlrheo_update(&pa, pTime->dGet(), Eps, EpsPrime, 0)) {
			silent_cerr("DamperConstitutiveLaw: unable to update after convergence" << std::endl);
			throw ErrGeneric();
		}
	};
   
	virtual const doublereal& GetF(void) const {
		return pa.F;
	};

	virtual const doublereal& GetFDE(void) const {
		return pa.FDE;
	};

	virtual const doublereal& GetFDEPrime(void) const {
		return pa.FDEPrime;
	};
};

static MBDynParser *pHP;

extern "C" int
nlrheo_get_int(int *i)
{
	ASSERT(pHP != 0);

	*i = pHP->GetInt();

	return 0;
}

extern "C" int
nlrheo_get_real(double *d)
{
	ASSERT(pHP != 0);

	*d = pHP->GetReal();

	return 0;
}

/* specific functional object(s) */
struct DamperCLR : public ConstitutiveLawRead<doublereal, doublereal> {
	virtual ConstitutiveLaw<doublereal, doublereal> *
	Read(const DataManager* pDM, MBDynParser& HP, ConstLawType::Type& CLType) {
		ConstitutiveLaw<doublereal, doublereal>* pCL = 0;
		
		
		CLType = ConstLawType::VISCOELASTIC;
		
		double scale_eps = 1;
		if (HP.IsKeyWord("scale_eps")) {
			scale_eps = HP.GetReal();
			// check?
		}

		double scale_f = 1;
		if (HP.IsKeyWord("scale_f")) {
			scale_f = HP.GetReal();
			// check?
		}

		// By default, cut at 
		double a = 1./(2.*M_PI*160.);
		if (HP.IsKeyWord("filter")) {
			doublereal dOmega = HP.GetReal();
			if (dOmega <= 0.) {
				silent_cerr("DamperConstitutiveLaw: "
					"invalid angular frequency "
					"at line " << HP.GetLineData()
					<< std::endl);
				throw ErrGeneric();
			}
			a = 1./dOmega;
		}

		sym_params* pap = 0;

		pHP = &HP;
		if (nlrheo_parse(&pap, scale_eps, scale_f, a)) {
			silent_cerr("DamperConstitutiveLaw: "
				"parse error at line " << HP.GetLineData()
				<< std::endl);
			throw ErrGeneric();
		}

		DriveCaller *pT = 0;
		SAFENEWWITHCONSTRUCTOR(pT,
			TimeDriveCaller,
			TimeDriveCaller(pDM->pGetDrvHdl()));
		SAFENEWWITHCONSTRUCTOR(pCL, 
			DamperConstitutiveLaw, 
			DamperConstitutiveLaw(*pap, pT));

		return pCL;
	};
};

extern "C" int
module_init(const char *module_name, void *pdm, void *php)
{
#if 0
	DataManager	*pDM = (DataManager *)pdm;
	MBDynParser	*pHP = (MBDynParser *)php;
#endif

	ConstitutiveLawRead<doublereal, doublereal> *rf1D
		= new DamperCLR;
	if (!SetCL1D("nonlinear" "rheologic" "damper", rf1D)) {
		delete rf1D;

		silent_cerr("DamperConstitutiveLaw: "
			"module_init(" << module_name << ") "
			"failed" << std::endl);

		return -1;
	}

	return 0;
}


