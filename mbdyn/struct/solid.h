/*
 * MBDyn (C) is a multibody analysis code.
 * http://www.mbdyn.org
 *
 * Copyright (C) 1996-2022
 *
 * Pierangelo Masarati  <masarati@aero.polimi.it>
 * Paolo Mantegazza     <mantegazza@aero.polimi.it>
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
 AUTHOR: Reinhard Resch <mbdyn-user@a1.net>
        Copyright (C) 2022(-2022) all rights reserved.

        The copyright of this code is transferred
        to Pierangelo Masarati and Paolo Mantegazza
        for use in the software MBDyn as described
        in the GNU Public License version 2.1
*/

#ifndef ___SOLID_H__INCLUDED___
#define ___SOLID_H__INCLUDED___

#include <array>
#include <memory>

#include "strnodead.h"
#include "sp_matvecass.h"
#include "elem.h"
#include "constltp.h"
#include "gravity.h"

class Hexahedron8;
class Hexahedron20;
class Pentahedron15;
class Tetrahedron10h;
class Gauss2;
class Gauss3;
class GaussP15;
class GaussT10h;

class SolidElem: virtual public Elem, public ElemGravityOwner, public InitialAssemblyElem {
public:
     SolidElem(unsigned uLabel,
               flag fOut);
     virtual ~SolidElem();

     virtual Elem::Type GetElemType() const override;

     virtual void
     SetValue(DataManager *pDM,
              VectorHandler& X, VectorHandler& XP,
              SimulationEntity::Hints *ph) override;

     virtual std::ostream& Restart(std::ostream& out) const override;

     virtual unsigned int iGetInitialNumDof() const override;

     virtual bool bIsDeformable() const override;
};

template <typename ElementType, typename CollocationType>
SolidElem*
ReadSolid(DataManager* pDM, MBDynParser& HP, unsigned int uLabel);

#endif
