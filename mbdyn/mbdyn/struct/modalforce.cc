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

#ifdef HAVE_CONFIG_H
#include <mbconfig.h>           /* This goes first in every *.c,*.cc file */
#endif /* HAVE_CONFIG_H */

#include <dataman.h>
#include "modalforce.h"

#include <fstream>

/* ModalForce - begin */

/* Costruttore */
ModalForce::ModalForce(unsigned int uL,
	Modal *pmodal,
	std::vector<int>& modeList,
	std::vector<DriveCaller *>& f,
	flag fOut)
: Elem(uL, fOut), 
Force(uL, fOut), 
pModal(pmodal),
modeList(modeList),
f(f)
{
	ASSERT(pModal != 0);
}

ModalForce::~ModalForce(void)
{
	if (!f.empty()) {
		for (unsigned i = 0; i < f.size(); i++) {
			delete f[i];
		}
	}
}

SubVectorHandler&
ModalForce::AssRes(SubVectorHandler& WorkVec,
	doublereal dCoef,
	const VectorHandler& XCurr, 
	const VectorHandler& XPrimeCurr)
{
	integer iR, iC;
	WorkSpaceDim(&iR, &iC);
	WorkVec.ResizeReset(iR);

	integer iModalIndex = pModal->iGetModalIndex() + pModal->uGetNModes();
	for (unsigned iMode = 0; iMode < modeList.size(); iMode++) {
		WorkVec.PutItem(1 + iMode, iModalIndex + modeList[iMode], f[iMode]->dGet());
	}

	return WorkVec;
}

void
ModalForce::Output(OutputHandler& OH) const
{
	if (fToBeOutput()) {
		std::ostream& out = OH.Forces();

		out << GetLabel();
		for (std::vector<DriveCaller *>::const_iterator i = f.begin(); i != f.end(); i++) {
			out << " " << (*i)->dGet();
		}
		out << std::endl;
	}
}
   
Elem*
ReadModalForce(DataManager* pDM, 
	MBDynParser& HP, 
	unsigned int uLabel)
{
	Modal *pModal = dynamic_cast<Modal *>(pDM->ReadElem(HP, Elem::JOINT));
	if (pModal == 0) {
		silent_cerr("ModalForce(" << uLabel << "): illegal Modal joint "
			" at line " << HP.GetLineData() << std::endl);
		throw ErrGeneric();
	}

	std::vector<int> modeList;
	if (HP.IsKeyWord("list")) {
		std::vector<bool> gotIt(pModal->uGetNModes());
		for (integer i = 0; i < pModal->uGetNModes(); i++) {
			gotIt[i] = false;
		}

		int iNumModes = HP.GetInt();
		if (iNumModes <= 0 || iNumModes > pModal->uGetNModes()) {
			silent_cerr("ModalForce(" << uLabel << "): "
				"illegal mode number " << iNumModes
				<< " at line " << HP.GetLineData() << std::endl);
			throw ErrGeneric();
		}
		modeList.resize(iNumModes);

		for (int i = 0; i < iNumModes; i++) {
			int iModeIdx = HP.GetInt();
			if (iModeIdx <= 0 || iModeIdx > pModal->uGetNModes()) {
				silent_cerr("ModalForce(" << uLabel << "): "
					"illegal mode index " << iModeIdx
					<< " at line " << HP.GetLineData() << std::endl);
				throw ErrGeneric();
			}

			if (gotIt[iModeIdx]) {
				silent_cerr("ModalForce(" << uLabel << "): "
					"mode " << iModeIdx << " already set "
					"at line " << HP.GetLineData() << std::endl);
				throw ErrGeneric();
			}

			modeList[i] = iModeIdx;
			gotIt[iModeIdx] = true;
		}

	} else {
		modeList.resize(pModal->uGetNModes());
		for (integer i = 0; i < pModal->uGetNModes(); i++) {
			modeList[i] = i+1;
		}
	}

	std::vector<DriveCaller *> f(modeList.size());
	for (unsigned i = 0; i < f.size(); i++) {
		f[i] = ReadDriveData(pDM, HP, false);
		if (f[i] == 0) {
			silent_cerr("ModalForce(" << uLabel << "): "
				"unable to read DriveCaller for mode #" << i + 1
				<< " at line " << HP.GetLineData() << std::endl);
			throw ErrGeneric();
		}
	}

	flag fOut = pDM->fReadOutput(HP, Elem::FORCE);
	Elem *pEl = 0;
	SAFENEWWITHCONSTRUCTOR(pEl, ModalForce,
		ModalForce(uLabel, pModal, modeList, f, fOut));

	return pEl;
}
