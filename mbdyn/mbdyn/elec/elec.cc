/* 
 * MBDyn (C) is a multibody analysis code. 
 * http://www.mbdyn.org
 *
 * Copyright (C) 1996-2000
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

/* Elementi elettrici */

#ifdef HAVE_CONFIG_H
#include <mbconfig.h>           /* This goes first in every *.c,*.cc file */
#endif /* HAVE_CONFIG_H */

#ifdef USE_ELECTRIC_NODES

#include <elec.h>
#include <dataman.h>
#include <discctrl.h>

/* Electric - begin */

Electric::Electric(unsigned int uL, Electric::Type T, 
		   const DofOwner* pDO, flag fOut)
: Elem(uL, Elem::ELECTRIC, fOut), 
ElemWithDofs(uL, Elem::ELECTRIC, pDO, fOut), 
ElecT(T) 
{
   NO_OP; 
}


Electric::~Electric(void) 
{
   NO_OP;
}

   
/* Contributo al file di restart 
 * (Nota: e' incompleta, deve essere chiamata dalla funzione corrispndente
 * relativa alla classe derivata */
std::ostream& Electric::Restart(std::ostream& out) const {
   return out << "  electric: " << GetLabel();
}


/* Tipo dell'elemento (usato solo per debug ecc.) */
Elem::Type Electric::GetElemType(void) const 
{
   return Elem::ELECTRIC; 
}


void Electric::Output(OutputHandler& OH) const
{
   if (fToBeOutput()) {
#ifdef DEBUG	
      OH.Output() << "Electric Element " << uLabel 
	<< ": sorry, not implemented yet" << std::endl;
#endif	
   }
}   

/* Electric - end */


#if defined(USE_STRUCT_NODES)

/* Accelerometer - begin */

/* Costruttore */
Accelerometer::Accelerometer(unsigned int uL, const DofOwner* pDO,
			     const StructNode* pS, const AbstractNode* pA,
			     const Vec3& TmpDir,
			     doublereal dO, doublereal dT, 
			     doublereal dC, doublereal dK,
			     flag fOut)
: Elem(uL, Elem::ELECTRIC, fOut), 
Electric(uL, Electric::ACCELEROMETER, pDO, fOut),
pStrNode(pS), pAbsNode(pA),
Dir(TmpDir), dOmega(dO), dTau(dT), dCsi(dC), dKappa(dK)
{
   NO_OP;
}


/* Distruttore banale */   
Accelerometer::~Accelerometer(void)
{
   NO_OP;
}


/* Contributo al file di restart */
std::ostream& Accelerometer::Restart(std::ostream& out) const
{
   Electric::Restart(out) << ", accelerometer, " 
     << pStrNode->GetLabel() << ", "
     << pAbsNode->GetLabel() << ", reference, node, ";
   return Dir.Write(out, ", ") << ", node, "
     << dOmega << ", "
     << dTau << ", "
     << dCsi << ", "
     << dKappa << ';' << std::endl;
}


/* Costruisce il contributo allo jacobiano */
VariableSubMatrixHandler& 
Accelerometer::AssJac(VariableSubMatrixHandler& WorkMat,
		      doublereal dCoef,
		      const VectorHandler& /* XCurr */ ,
		      const VectorHandler& /* XPrimeCurr */ )
{
   DEBUGCOUT("Entering Accelerometer::AssJac()" << std::endl);

   /* Casting di WorkMat */
   SparseSubMatrixHandler& WM = WorkMat.SetSparse();
   WM.ResizeInit(15, 0, 0.);
   
   /* Indici delle equazioni */
   integer iFirstPositionIndex = pStrNode->iGetFirstPositionIndex();
   integer iAbstractIndex = pAbsNode->iGetFirstIndex();
   integer iFirstIndex = iGetFirstIndex();
   
   /*
    * | c      0 0                0 -a^T  c*xP^T*a/\ || Delta_vP  |
    * | 0      1 c*O^2/T          0  0    0          || Delta_y1P |
    * | 0      0 1+c*(2*C*O+1/T) -c  0    0          || Delta_y2P |
    * |-K*O^2 -c c*O*(1+2*C/T)    1  0    0          || Delta_zP  | = res
    *                                                 | Delta_xP  |
    *                                                 | Delta_gP  |
    * 
    * con: c  = dCoef
    *      a  = RNode*Dir
    *      xp = Velocita' del nodo
    *      O  = dOmega
    *      T  = dTau
    *      C  = dCsi
    *      K  = dKappa
    * 
    * v e' la misura della veocita' del punto,
    * y1 e y2 sono stati dell'acceleromtro; v, y1, y2 appartengono al DofOwner
    * dell'elemento;
    * z e' la variabile del nodo astratto;
    * x e g sono posizione e parametri di rotazione del nodo strutturale
    * 
    * 
    * Funzione di trasferimento dell'accelerometro nel dominio di Laplace:
    * 
    *  e0                        T * s
    * ----(s) = K * ---------------------------------
    * acc.          (1 + T*s)*(1 + 2*C/O*s + s^2/O^2)
    * 
    */
   
   /* Dinamica dell'accelerometro */
   WM.fPutItem(1, iFirstIndex+1, iFirstIndex+1, dCoef);
   WM.fPutItem(2, iAbstractIndex+1, iFirstIndex+1, -dKappa*dOmega*dOmega);
   WM.fPutItem(3, iFirstIndex+2, iFirstIndex+2, 1.);
   WM.fPutItem(4, iAbstractIndex+1, iFirstIndex+2, -dCoef);
   WM.fPutItem(5, iFirstIndex+2, iFirstIndex+3, dCoef*dOmega*dOmega/dTau);
   WM.fPutItem(6, iFirstIndex+3, iFirstIndex+3, 
	       1.+dCoef*(2.*dCsi*dOmega+1./dTau));
   WM.fPutItem(7, iAbstractIndex+1, iFirstIndex+3, 
	       dCoef*dOmega*(dOmega+2.*dCsi/dTau));
   WM.fPutItem(8, iFirstIndex+3, iAbstractIndex+1, -dCoef);
   WM.fPutItem(9, iAbstractIndex+1, iAbstractIndex+1, 1.);
      
   /* Misura dell'accelerazione */
   Vec3 TmpDir((pStrNode->GetRRef())*Dir);
   for(int iCnt = 1; iCnt <= 3; iCnt++) {
      WM.fPutItem(9+iCnt, iFirstIndex+1, iFirstPositionIndex+iCnt, 
		  -TmpDir.dGet(iCnt));
   }
   
   Vec3 XP(pStrNode->GetVCurr());
   TmpDir = -TmpDir.Cross(XP);
   for(int iCnt = 1; iCnt <= 3; iCnt++) {
      WM.fPutItem(12+iCnt, iFirstIndex+1, iFirstPositionIndex+3+iCnt, 
		  dCoef*TmpDir.dGet(iCnt));
   }
   
   return WorkMat;
}


/* Costruisce il contributo al residuo */
SubVectorHandler& 
Accelerometer::AssRes(SubVectorHandler& WorkVec,
		      doublereal /* dCoef */ ,
		      const VectorHandler& XCurr, 
		      const VectorHandler& XPrimeCurr)
{   
   DEBUGCOUT("Entering Accelerometer::AssRes()" << std::endl);
      
   /* Dimensiona e resetta la matrice di lavoro */
   integer iNumRows = 0;
   integer iNumCols = 0;
   this->WorkSpaceDim(&iNumRows, &iNumCols);
   WorkVec.Resize(iNumRows);
   WorkVec.Reset(0.);
      
   integer iAbstractIndex = pAbsNode->iGetFirstIndex();
   integer iFirstIndex = iGetFirstIndex();

   /*
    *        | Delta_vP  |   | -v+a^T*xP                      |
    *        | Delta_y1P |   | -y1P-O^2/T*y2                  |
    *  jac * | Delta_y2P | = | -y2P+z-(2*C*O+1/T)*y2          |
    *        | Delta_zP  |   | -zP+y1-O*(O+2*C/T)*y2+K*O^2*vP |
    *        | Delta_xP  |
    *        | Delta_gP  |
    * 
    * per il significato dei termini vedi AssJac */
    
   WorkVec.fPutRowIndex(1, iFirstIndex+1);
   WorkVec.fPutRowIndex(2, iFirstIndex+2);
   WorkVec.fPutRowIndex(3, iFirstIndex+3);
   WorkVec.fPutRowIndex(4, iAbstractIndex+1);
   
   Vec3 XP(pStrNode->GetVCurr());
   Mat3x3 R(pStrNode->GetRCurr());
   doublereal v = XCurr.dGetCoef(iFirstIndex+1);
   doublereal vp = XPrimeCurr.dGetCoef(iFirstIndex+1);
   doublereal y1 = XCurr.dGetCoef(iFirstIndex+2);
   doublereal y1p = XPrimeCurr.dGetCoef(iFirstIndex+2);
   doublereal y2 = XCurr.dGetCoef(iFirstIndex+3);
   doublereal y2p = XPrimeCurr.dGetCoef(iFirstIndex+3);
   doublereal z = XCurr.dGetCoef(iAbstractIndex+1);
   doublereal zp = XPrimeCurr.dGetCoef(iAbstractIndex+1);

   WorkVec.fPutCoef(1, (R*Dir).Dot(XP)-v);
   WorkVec.fPutCoef(2, -y1p-dOmega*dOmega/dTau*y2);
   WorkVec.fPutCoef(3, -y2p+z-(2.*dCsi*dOmega+1./dTau)*y2);
   WorkVec.fPutCoef(4, -zp+y1-dOmega*(dOmega+2.*dCsi/dTau)*y2+
		    dKappa*dOmega*dOmega*vp);
         
   return WorkVec;
}


/* Output (provvisorio) */
void Accelerometer::Output(OutputHandler& OH) const
{
   if(fToBeOutput()) {      
#ifdef DEBUG   
      OH.Output() << "Accelerometer " << uLabel << ':' << std::endl
	<< "linked to structural node " << pStrNode->GetLabel() 
	<< " and to abstract node " << pAbsNode->GetLabel() << std::endl
	<< "Omega: " << dOmega
	<< ", Tau: " << dTau
	<< ", Csi: " << dCsi
	<< ", Kappa: " << dKappa << std::endl;
#endif   
   }   
}


unsigned int Accelerometer::iGetNumDof(void) const
{
   return 3;
}


DofOrder::Order Accelerometer::SetDof(unsigned int i) const
{
   ASSERT(i >= 0 && i < 3);
   return DofOrder::DIFFERENTIAL; 
}


void Accelerometer::WorkSpaceDim(integer* piNumRows, integer* piNumCols) const
{
   *piNumRows = 4; 
   *piNumCols = 10; 
}

   
void Accelerometer::SetInitialValue(VectorHandler& /* X */ ) const
{
   NO_OP;
}


void Accelerometer::SetValue(VectorHandler& X, VectorHandler& /* XP */ ) const 
{
   doublereal v = (pStrNode->GetRCurr()*Dir).Dot(pStrNode->GetVCurr());
   X.fPutCoef(iGetFirstIndex()+1, v);
}

/* Accelerometer - end */


/* TraslAccel - begin */

/* Costruttore */
TraslAccel::TraslAccel(unsigned int uL, const DofOwner* pDO,
		       const StructNode* pS, const AbstractNode* pA,
		       const Vec3& TmpDir, const Vec3& Tmpf,
		       flag fOut)
: Elem(uL, Elem::ELECTRIC, fOut), 
Electric(uL, Electric::ACCELEROMETER, pDO, fOut),
pStrNode(pS), pAbsNode(pA),
Dir(TmpDir), f(Tmpf)
{
   ASSERT(pStrNode != NULL);
   ASSERT(pStrNode->GetNodeType() == Node::STRUCTURAL);
   ASSERT(pAbsNode != NULL);
   ASSERT(pAbsNode->GetNodeType() == Node::ABSTRACT);
}


/* Distruttore banale */   
TraslAccel::~TraslAccel(void)
{
   NO_OP;
}


/* Contributo al file di restart */
std::ostream& TraslAccel::Restart(std::ostream& out) const
{
   Electric::Restart(out) << ", accelerometer, translational, " 
     << pStrNode->GetLabel() << ", "
     << pAbsNode->GetLabel() << ", reference, node, ",
     Dir.Write(out, ", ") << ", reference, node, ",
     f.Write(out, ", ") << ';' << std::endl;
   return out;
}


/* Costruisce il contributo allo jacobiano */
VariableSubMatrixHandler& 
TraslAccel::AssJac(VariableSubMatrixHandler& WorkMat,
		   doublereal dCoef,
		   const VectorHandler& /* XCurr */ ,
		   const VectorHandler& /* XPrimeCurr */ )
{
   DEBUGCOUT("Entering TraslAccel::AssJac()" << std::endl);

   /* Casting di WorkMat */
   SparseSubMatrixHandler& WM = WorkMat.SetSparse();
   WM.ResizeInit(9, 0, 0.);
   
   /* Indici delle equazioni */
   integer iFirstColIndex = pStrNode->iGetFirstColIndex();
   integer iAbstractIndex = pAbsNode->iGetFirstIndex()+1;
   integer iFirstIndex = iGetFirstIndex()+1;

   WM.fPutItem(1, iAbstractIndex, iAbstractIndex, dCoef);
   WM.fPutItem(2, iAbstractIndex, iFirstIndex, -1.);
   WM.fPutItem(3, iFirstIndex, iFirstIndex, dCoef);
   
   Vec3 tmpf = pStrNode->GetRCurr()*f;
   Vec3 tmpd = pStrNode->GetRCurr()*Dir;
   Vec3 tmp = tmpf.Cross(tmpd);
   WM.fPutItem(4, iFirstIndex, iFirstColIndex+1, -tmpd.dGet(1));
   WM.fPutItem(5, iFirstIndex, iFirstColIndex+2, -tmpd.dGet(2));
   WM.fPutItem(6, iFirstIndex, iFirstColIndex+3, -tmpd.dGet(3));
   
   tmp = tmpd.Cross((pStrNode->GetVCurr()*(-dCoef)+tmpf));
   WM.fPutItem(7, iFirstIndex, iFirstColIndex+4, tmp.dGet(1));
   WM.fPutItem(8, iFirstIndex, iFirstColIndex+5, tmp.dGet(2));
   WM.fPutItem(9, iFirstIndex, iFirstColIndex+6, tmp.dGet(3));

   return WorkMat;
}


/* Costruisce il contributo al residuo */
SubVectorHandler& 
TraslAccel::AssRes(SubVectorHandler& WorkVec,
		   doublereal /* dCoef */ ,
		   const VectorHandler& XCurr, 
		   const VectorHandler& XPrimeCurr)
{   
   DEBUGCOUT("Entering TraslAccel::AssRes()" << std::endl);
      
   /* Dimensiona e resetta la matrice di lavoro */
   WorkVec.Resize(2);
      
   integer iAbstractIndex = pAbsNode->iGetFirstIndex()+1;
   integer iFirstIndex = iGetFirstIndex()+1;

   WorkVec.fPutRowIndex(1, iAbstractIndex);
   WorkVec.fPutRowIndex(2, iFirstIndex);
   
   Vec3 tmpf = pStrNode->GetRCurr()*f;
   Vec3 tmpd = pStrNode->GetRCurr()*Dir;
   
   doublereal v = XCurr.dGetCoef(iFirstIndex);
   doublereal vp = XPrimeCurr.dGetCoef(iFirstIndex);
   doublereal a = pAbsNode->dGetX();
   
   WorkVec.fPutCoef(1, vp-a);
   WorkVec.fPutCoef(2, tmpd.Dot((pStrNode->GetVCurr()+pStrNode->GetWCurr().Cross(tmpf)))-v);
   
   return WorkVec;
}


/* Output (provvisorio) */
void TraslAccel::Output(OutputHandler& OH) const
{
   if(fToBeOutput()) {      
#ifdef DEBUG   
      OH.Output() << "TraslAccel " << uLabel << ':' << std::endl
	<< "linked to structural node " << pStrNode->GetLabel() 
	<< " and to abstract node " << pAbsNode->GetLabel() << std::endl;
#endif   
   }   
}

unsigned int TraslAccel::iGetNumDof(void) const
{
   return 1;
}
 

DofOrder::Order TraslAccel::SetDof(unsigned int i) const 
{
   ASSERT(i == 0);
   return DofOrder::DIFFERENTIAL; 
}


void TraslAccel::WorkSpaceDim(integer* piNumRows, integer* piNumCols) const
{
   *piNumRows = 2; 
   *piNumCols = 8; 
}
      
   
void TraslAccel::SetInitialValue(VectorHandler& /* X */ ) const
{
   NO_OP;
}


void TraslAccel::SetValue(VectorHandler& X, VectorHandler& XP) const
{
   doublereal v = 
     (pStrNode->GetRCurr()*Dir).Dot(pStrNode->GetVCurr()
				    +pStrNode->GetWCurr().Cross(pStrNode->GetRCurr()*f));
   X.fPutCoef(iGetFirstIndex()+1, v);
   XP.fPutCoef(iGetFirstIndex()+1, 0.);
}

/* TraslAccel - end */


/* RotAccel - begin */

/* Costruttore */
RotAccel::RotAccel(unsigned int uL, const DofOwner* pDO,
		   const StructNode* pS, const AbstractNode* pA,
		   const Vec3& TmpDir, 
		   flag fOut)
: Elem(uL, Elem::ELECTRIC, fOut), 
Electric(uL, Electric::ACCELEROMETER, pDO, fOut),
pStrNode(pS), pAbsNode(pA),
Dir(TmpDir)
{
   ASSERT(pStrNode != NULL);
   ASSERT(pStrNode->GetNodeType() == Node::STRUCTURAL);
   ASSERT(pAbsNode != NULL);
   ASSERT(pAbsNode->GetNodeType() == Node::ABSTRACT);
}


/* Distruttore banale */   
RotAccel::~RotAccel(void)
{
   NO_OP;
}


/* Contributo al file di restart */
std::ostream& RotAccel::Restart(std::ostream& out) const
{
   Electric::Restart(out) << ", accelerometer, rotational, "
     << pStrNode->GetLabel() << ", "
     << pAbsNode->GetLabel() << ", reference, node, ",
     Dir.Write(out, ", ") << ';' << std::endl;
   return out;
}


/* Costruisce il contributo allo jacobiano */
VariableSubMatrixHandler& 
RotAccel::AssJac(VariableSubMatrixHandler& WorkMat,
		   doublereal dCoef,
		   const VectorHandler& /* XCurr */ ,
		   const VectorHandler& /* XPrimeCurr */ )
{
   DEBUGCOUT("Entering RotAccel::AssJac()" << std::endl);

   /* Casting di WorkMat */
   SparseSubMatrixHandler& WM = WorkMat.SetSparse();
   WM.ResizeInit(6, 0, 0.);
   
   /* Indici delle equazioni */
   integer iFirstColIndex = pStrNode->iGetFirstColIndex();
   integer iAbstractIndex = pAbsNode->iGetFirstIndex()+1;
   integer iFirstIndex = iGetFirstIndex()+1;

   WM.fPutItem(1, iAbstractIndex, iAbstractIndex, dCoef);
   WM.fPutItem(2, iAbstractIndex, iFirstIndex, -1.);
   WM.fPutItem(3, iFirstIndex, iFirstIndex, dCoef);
   
   Vec3 tmp = pStrNode->GetRCurr()*Dir;
   WM.fPutItem(4, iFirstIndex, iFirstColIndex+4, tmp.dGet(1));
   WM.fPutItem(5, iFirstIndex, iFirstColIndex+5, tmp.dGet(2));
   WM.fPutItem(6, iFirstIndex, iFirstColIndex+6, tmp.dGet(3));

   return WorkMat;
}


/* Costruisce il contributo al residuo */
SubVectorHandler& 
RotAccel::AssRes(SubVectorHandler& WorkVec,
		   doublereal /* dCoef */ ,
		   const VectorHandler& XCurr, 
		   const VectorHandler& XPrimeCurr)
{   
   DEBUGCOUT("Entering RotAccel::AssRes()" << std::endl);
      
   /* Dimensiona e resetta la matrice di lavoro */
   WorkVec.Resize(2);
      
   integer iAbstractIndex = pAbsNode->iGetFirstIndex()+1;
   integer iFirstIndex = iGetFirstIndex()+1;

   WorkVec.fPutRowIndex(1, iAbstractIndex);
   WorkVec.fPutRowIndex(2, iFirstIndex);
   
   Vec3 tmp = pStrNode->GetRCurr()*Dir;
   
   doublereal v = XCurr.dGetCoef(iFirstIndex);
   doublereal vp = XPrimeCurr.dGetCoef(iFirstIndex);
   doublereal a = pAbsNode->dGetX();
   
   WorkVec.fPutCoef(1, vp-a);
   WorkVec.fPutCoef(2, tmp.Dot(pStrNode->GetWCurr())-v);
   
   return WorkVec;
}


/* Output (provvisorio) */
void RotAccel::Output(OutputHandler& OH) const
{
   if(fToBeOutput()) {      
#ifdef DEBUG   
      OH.Output() << "RotAccel " << uLabel << ':' << std::endl
	<< "linked to structural node " << pStrNode->GetLabel() 
	<< " and to abstract node " << pAbsNode->GetLabel() << std::endl;
#endif   
   }   
}

unsigned int RotAccel::iGetNumDof(void) const
{
   return 1;
}
 

DofOrder::Order RotAccel::SetDof(unsigned int i) const 
{
   ASSERT(i == 0);
   return DofOrder::DIFFERENTIAL; 
}


void RotAccel::WorkSpaceDim(integer* piNumRows, integer* piNumCols) const
{
   *piNumRows = 2; 
   *piNumCols = 5;
}
      
   
void RotAccel::SetInitialValue(VectorHandler& /* X */ ) const
{
   NO_OP;
}


void RotAccel::SetValue(VectorHandler& X, VectorHandler& /* XP */ ) const
{
   doublereal v = (pStrNode->GetRCurr()*Dir).Dot(pStrNode->GetWCurr());
   X.fPutCoef(iGetFirstIndex()+1, v);
}

/* RotAccel - end */


/* DispMeasure - begin */

/* Costruttore */
DispMeasure::DispMeasure(unsigned int uL, const DofOwner* pDO,
			 const StructNode* pS1, const StructNode* pS2,
			 const AbstractNode* pA,
			 const Vec3& Tmpf1, const Vec3& Tmpf2,
			 flag fOut)
: Elem(uL, Elem::ELECTRIC, fOut), 
Electric(uL, Electric::DISPLACEMENT, pDO, fOut),
pStrNode1(pS1), pStrNode2(pS2), pAbsNode(pA),
f1(Tmpf1), f2(Tmpf2)
{
   ASSERT(pStrNode1 != NULL);
   ASSERT(pStrNode1->GetNodeType() == Node::STRUCTURAL);
   ASSERT(pStrNode2 != NULL);
   ASSERT(pStrNode2->GetNodeType() == Node::STRUCTURAL);
   ASSERT(pAbsNode != NULL);
   ASSERT(pAbsNode->GetNodeType() == Node::ABSTRACT);
}


/* Distruttore banale */   
DispMeasure::~DispMeasure(void)
{
   NO_OP;
}


/* Contributo al file di restart */
std::ostream& DispMeasure::Restart(std::ostream& out) const
{
   Electric::Restart(out) << ", displacement, "
     << pStrNode1->GetLabel() << ", reference, node, ",
   f1.Write(out, ", ") << pStrNode2->GetLabel() << ", reference, node, ",
   f2.Write(out, ", ") << pAbsNode->GetLabel() << ';' << std::endl;
   return out;
}


/* Costruisce il contributo allo jacobiano */
VariableSubMatrixHandler& 
DispMeasure::AssJac(VariableSubMatrixHandler& WorkMat,
		    doublereal dCoef,
		    const VectorHandler& /* XCurr */ ,
		    const VectorHandler& /* XPrimeCurr */ )
{
   DEBUGCOUT("Entering DispMeasure::AssJac()" << std::endl);
   
   /* Casting di WorkMat */
   SparseSubMatrixHandler& WM = WorkMat.SetSparse();
   WM.ResizeInit(1, 0, 0.);
   
   /* Indici delle equazioni */
   integer iAbstractIndex = pAbsNode->iGetFirstIndex()+1;

   WM.fPutItem(1, iAbstractIndex, iAbstractIndex, dCoef);

   return WorkMat;
}


/* Costruisce il contributo al residuo */
SubVectorHandler& 
DispMeasure::AssRes(SubVectorHandler& WorkVec,
		   doublereal /* dCoef */ ,
		   const VectorHandler& /* XCurr */ ,
		   const VectorHandler& /* XPrimeCurr */ )
{   
   DEBUGCOUT("Entering DispMeasure::AssRes()" << std::endl);
      
   /* Dimensiona e resetta la matrice di lavoro */
   WorkVec.Resize(1);
      
   integer iAbstractIndex = pAbsNode->iGetFirstIndex()+1;

   WorkVec.fPutRowIndex(1, iAbstractIndex);
   
   Vec3 x1 = pStrNode1->GetXCurr()+pStrNode1->GetRCurr()*f1;
   Vec3 x2 = pStrNode2->GetXCurr()+pStrNode2->GetRCurr()*f2;
   
   doublereal a = pAbsNode->dGetX();
   
   doublereal d = (x2-x1).Norm();
   
   WorkVec.fPutCoef(1, d-a);
   
   return WorkVec;
}


/* Output (provvisorio) */
void DispMeasure::Output(OutputHandler& /* OH */ ) const
{
   /*
   if (fToBeOutput()) {    
   }
    */
}


void 
DispMeasure::WorkSpaceDim(integer* piNumRows, integer* piNumCols) const
{
   *piNumRows = 1;
   *piNumCols = 1; 
}
 
/* Setta i valori iniziali delle variabili (e fa altre cose)
 * prima di iniziare l'integrazione */
void 
DispMeasure::SetValue(VectorHandler& X, VectorHandler& XP) const
{
   integer iIndex = pAbsNode->iGetFirstIndex()+1;
   
   /* distanza */
   Vec3 ff1 = pStrNode1->GetRCurr()*f1;
   Vec3 ff2 = pStrNode2->GetRCurr()*f2;
   
   Vec3 x1 = pStrNode1->GetXCurr()+ff1;
   Vec3 x2 = pStrNode2->GetXCurr()+ff2;
      
   doublereal d = (x2-x1).Norm();

   ((AbstractNode*&)pAbsNode)->SetX(d);
   X.fPutCoef(iIndex, d);
   
   /* velocita' */   
   Vec3 v1 = pStrNode1->GetVCurr()+(pStrNode1->GetWCurr()).Cross(ff1);
   Vec3 v2 = pStrNode2->GetVCurr()+(pStrNode2->GetWCurr()).Cross(ff2);
      
   doublereal v = (v2-v1).Norm();

   ((AbstractNode*&)pAbsNode)->SetXPrime(v);
   
   XP.fPutCoef(iIndex, v);
}
   


/* DispMeasure - end */

#endif // USE_STRUCT_NODES


/* Legge un forgetting factor */

ForgettingFactor* ReadFF(MBDynParser& HP, integer iNumOutputs)
{
   ForgettingFactor* pFF = NULL;
   
   if (HP.IsKeyWord("forgettingfactor")) {
      if (HP.IsKeyWord("const")) {
	 doublereal d = HP.GetReal();
	 
	 SAFENEWWITHCONSTRUCTOR(pFF,
				ConstForgettingFactor,
				ConstForgettingFactor(d));
	 
      } else if (HP.IsKeyWord("dynamic")) {
	 /* uso la 2^a versione */
	 integer n1 = HP.GetInt();
	 integer n2 = HP.GetInt();
	 doublereal dRho = HP.GetReal();
	 doublereal dFact = HP.GetReal();
	 doublereal dKRef = HP.GetReal();
	 doublereal dKLim = HP.GetReal();
	 
	 SAFENEWWITHCONSTRUCTOR(pFF,
				DynamicForgettingFactor2,
				DynamicForgettingFactor2(n1, n2, 
							 iNumOutputs,
							 dRho, dFact,
							 dKRef, dKLim));
	 
      } else {	      
	 std::cerr << "line " << HP.GetLineData() 
	   << ": unknown forgetting factor" << std::endl;
	 THROW(ErrGeneric());
      }	      
   } else {
      /* default */
      SAFENEWWITHCONSTRUCTOR(pFF,
			     ConstForgettingFactor,
			     ConstForgettingFactor(1.));
   }
   
   ASSERT(pFF != NULL);
   return pFF;
}


/* Legge un eccitatore persistente */

PersistentExcitation* ReadPX(DataManager* pDM, MBDynParser& HP, integer iNumInputs)
{
   PersistentExcitation* pPX = NULL;
   
   if (HP.IsKeyWord("excitation")) {
      if (iNumInputs == 1) {
	 DriveCaller* pDC = ReadDriveData(pDM, HP, pDM->pGetDrvHdl());
	 SAFENEWWITHCONSTRUCTOR(pPX, ScalarPX, ScalarPX(pDC));
      } else {
	 DriveCaller** ppDC = NULL;
	 SAFENEWARR(ppDC, DriveCaller*, iNumInputs);
	 
	 for (integer i = iNumInputs; i-- > 0; ) {
	    ppDC[i] = ReadDriveData(pDM, HP, pDM->pGetDrvHdl());	    
	 }
	 SAFENEWWITHCONSTRUCTOR(pPX, VectorPX, VectorPX(iNumInputs, ppDC));
      }
   } else {
      /* Null excitation */
      SAFENEW(pPX, NullPX);
   }
   
   ASSERT(pPX != NULL);
   return pPX;
}

                   


/* Legge un elemento elettrico */
   
Elem* ReadElectric(DataManager* pDM,
		   MBDynParser& HP, 
		   const DofOwner* pDO, 
		   unsigned int uLabel)
{
   DEBUGCOUTFNAME("ReadElectric()");
   
   const char* sKeyWords[] = {
      "accelerometer",
      "displacement",
      "discretecontrol",	
      "identification",
          "const",
          "dynamic",
      "control",
      "adaptivecontrol"	                    
   };
   
   /* enum delle parole chiave */
   enum KeyWords {
      UNKNOWN = -1,
	
      ACCELEROMETER = 0,
      DISPLACEMENT,
      DISCRETECONTROL,	
      IDENTIFICATION,
          CONST,
          DYNAMIC,
      CONTROL,
      ADAPTIVECONTROL,
      
      LASTKEYWORD
   };
   
   /* tabella delle parole chiave */
   KeyTable K((int)LASTKEYWORD, sKeyWords);
   
   /* parser del blocco di controllo */
   HP.PutKeyTable(K);
   
   /* lettura del tipo di elemento elettrico */   
   KeyWords CurrKeyWord = KeyWords(HP.GetWord());
   
#ifdef DEBUG   
   if (CurrKeyWord >= 0) {      
      std::cout << "electric element type: " 
	<< sKeyWords[CurrKeyWord] << std::endl;
   }   
#endif   

   Elem* pEl = NULL;
   
   switch (CurrKeyWord) {
      /*  */
      
    case ACCELEROMETER: {
#if defined(USE_STRUCT_NODES)

       int f = 0;
       if (HP.IsKeyWord("translational")) {
	  f = 1;
       } else if(HP.IsKeyWord("rotational")) {
	  f = 2;
       }
       
       if (f) {
	  
	  /* nodo strutturale collegato */
	  unsigned int uNode = (unsigned int)HP.GetInt();	     
	  DEBUGCOUT("Linked to Structural Node " << uNode << std::endl);
	  
	  /* verifica di esistenza del nodo strutturale */
	  StructNode* pStrNode;
	  if ((pStrNode = pDM->pFindStructNode(uNode)) == NULL) {
	     std::cerr << "line " << HP.GetLineData() 
	       << ": structural node " << uNode
	       << " not defined" << std::endl;	  
	     THROW(DataManager::ErrGeneric());
	  }		  
	  
	  /* nodo astratto collegato */
	  uNode = (unsigned int)HP.GetInt();	     
	  DEBUGCOUT("Linked to Abstract Node " << uNode << std::endl);
	  
	  /* verifica di esistenza del nodo astratto */
	  AbstractNode* pAbsNode;
	  if ((pAbsNode = (AbstractNode*)(pDM->pFindNode(Node::ABSTRACT, uNode))) == NULL) {
	     std::cerr << "line " << HP.GetLineData() 
	       << ": abstract node " << uNode
	       << " not defined" << std::endl;	  
	     THROW(DataManager::ErrGeneric());
	  }		  
	  
	  /* Direzione */
	  Vec3 Dir(HP.GetVecRel(ReferenceFrame(pStrNode)));
	  doublereal d = Dir.Dot();
	  if (d > 0.) {
	     Dir /= d;
	     DEBUGCOUT("Direction: " << std::endl << Dir << std::endl);
	  } else {
	     std::cerr << "Warning, null direction in accelerometer "
	       << uLabel << std::endl;
	  }
	  
	  switch (f) {
	   case 1: {
	      /* offset */
	      Vec3 Tmpf(HP.GetPosRel(ReferenceFrame(pStrNode)));
	      flag fOut = pDM->fReadOutput(HP, Elem::ELECTRIC);
	      
	      SAFENEWWITHCONSTRUCTOR(pEl, 
				     TraslAccel,
				     TraslAccel(uLabel, pDO, 
						pStrNode, pAbsNode,
						Dir, Tmpf, fOut));
	      break;
	   } 
	   case 2: {
	      flag fOut = pDM->fReadOutput(HP, Elem::ELECTRIC);
	      
	      SAFENEWWITHCONSTRUCTOR(pEl, 
				     RotAccel,
				     RotAccel(uLabel, pDO, 
					      pStrNode, pAbsNode,
					      Dir, fOut));
	      break;
	   }
	   default: {
	      std::cerr << "you shouldn't be here!" << std::endl;
	      THROW(ErrGeneric());
	   }
	  }	  
	  
       } else {
	  
	  /* nodo strutturale collegato */
	  unsigned int uNode = (unsigned int)HP.GetInt();	     
	  DEBUGCOUT("Linked to Structural Node " << uNode << std::endl);
	  
	  /* verifica di esistenza del nodo strutturale */
	  StructNode* pStrNode;
	  if ((pStrNode = pDM->pFindStructNode(uNode)) == NULL) {
	     std::cerr << "line " << HP.GetLineData() 
	       << ": structural node " << uNode
	       << " not defined" << std::endl;	  
	     THROW(DataManager::ErrGeneric());
	  }		  
	  
	  /* nodo astratto collegato */
	  uNode = (unsigned int)HP.GetInt();	     
	  DEBUGCOUT("Linked to Abstract Node " << uNode << std::endl);
	  
	  /* verifica di esistenza del nodo astratto */
	  AbstractNode* pAbsNode;
	  if ((pAbsNode = (AbstractNode*)(pDM->pFindNode(Node::ABSTRACT, uNode))) == NULL) {
	     std::cerr << "line " << HP.GetLineData() 
	       << ": abstract node " << uNode
	       << " not defined" << std::endl;	  
	     THROW(DataManager::ErrGeneric());
	  }		  
	  
	  /* Direzione */
	  Vec3 Dir(HP.GetVecRel(ReferenceFrame(pStrNode)));
	  doublereal d = Dir.Dot();
	  if (d > 0.) {
	     Dir /= d;
	     DEBUGCOUT("Direction: " << std::endl << Dir << std::endl);
	  } else {
	     std::cerr << "Warning, null direction in accelerometer "
	       << uLabel << std::endl;
	  }
	  
	  /* Parametri */
	  doublereal dOmega = HP.GetReal();
	  if (dOmega <= 0.) {		  
	     std::cerr << "Warning, illegal Omega in accelerometer " 
	       << uLabel << std::endl;
	     THROW(DataManager::ErrGeneric());
	  }
	  
	  doublereal dTau = HP.GetReal();
	  if (dTau <= 0.) {		  
	     std::cerr << "Warning, illegal Tau in accelerometer " 
	       << uLabel << "; aborting ..." << std::endl;	  
	     THROW(DataManager::ErrGeneric());
	  }
	  
	  doublereal dCsi = HP.GetReal();
	  if (dCsi <= 0. || dCsi > 1.) {		  
	     std::cerr << "Warning, illegal Csi in accelerometer " 
	       << uLabel << std::endl;
	     THROW(DataManager::ErrGeneric());
	  }
	  
	  doublereal dKappa = HP.GetReal();
	  if (dKappa == 0.) {		  
	     std::cerr << "Warning, null Kappa in accelerometer " 
	       << uLabel << std::endl;
	     THROW(DataManager::ErrGeneric());
	  }	     
	  
	  DEBUGCOUT("Omega: " << dOmega 
		    << ", Tau: " << dTau
		    << ", Csi: " << dCsi
		    << ", Kappa: " << dKappa << std::endl);
	  
	  flag fOut = pDM->fReadOutput(HP, Elem::ELECTRIC);
	  
	  SAFENEWWITHCONSTRUCTOR(pEl, 
				 Accelerometer,
				 Accelerometer(uLabel, pDO, pStrNode, pAbsNode,
					       Dir, dOmega, dTau, dCsi, dKappa, 
					       fOut));
       }
       
       break;
       
#else // USE_STRUCT_NODES
       std::cerr << "you're not allowed to use accelerometer elements" << std::endl;
       THROW(ErrGeneric());
#endif // USE_STRUCT_NODES
    }
      
    case DISPLACEMENT: {
#if defined(USE_STRUCT_NODES)
	  
       /* nodo strutturale collegato 1 */
       unsigned int uNode = (unsigned int)HP.GetInt();	     
       DEBUGCOUT("Linked to Structural Node " << uNode << std::endl);
       
       /* verifica di esistenza del nodo strutturale */
       StructNode* pStrNode1;
       if ((pStrNode1 = pDM->pFindStructNode(uNode)) == NULL) {
	  std::cerr << "line " << HP.GetLineData() 
	      << ": structural node " << uNode
	    << " not defined" << std::endl;	  
	  THROW(DataManager::ErrGeneric());
       }
       
       /* offset 1 */
       Vec3 Tmpf1(HP.GetPosRel(ReferenceFrame(pStrNode1)));
	  
       /* nodo strutturale collegato 2 */
       uNode = (unsigned int)HP.GetInt();	     
       DEBUGCOUT("Linked to Structural Node " << uNode << std::endl);
       
       /* verifica di esistenza del nodo strutturale */
       StructNode* pStrNode2;
       if ((pStrNode2 = pDM->pFindStructNode(uNode)) == NULL) {
	  std::cerr << "line " << HP.GetLineData() 
	      << ": structural node " << uNode
	    << " not defined" << std::endl;	  
	  THROW(DataManager::ErrGeneric());
       }		  
	  
       /* offset 2 */
       Vec3 Tmpf2(HP.GetPosRel(ReferenceFrame(pStrNode2)));
       
       /* nodo astratto collegato */
       uNode = (unsigned int)HP.GetInt();	     
       DEBUGCOUT("Linked to Abstract Node " << uNode << std::endl);
       
       /* verifica di esistenza del nodo astratto */
       AbstractNode* pAbsNode;
       if ((pAbsNode = (AbstractNode*)(pDM->pFindNode(Node::ABSTRACT, uNode))) == NULL) {
	  std::cerr << "line " << HP.GetLineData() 
	      << ": abstract node " << uNode
	    << " not defined" << std::endl;	  
	  THROW(DataManager::ErrGeneric());
       }		  
       
       flag fOut = pDM->fReadOutput(HP, Elem::ELECTRIC);
       
       SAFENEWWITHCONSTRUCTOR(pEl, 
			      DispMeasure,
			      DispMeasure(uLabel, pDO, 
					  pStrNode1, pStrNode2, pAbsNode,
					  Tmpf1, Tmpf2, fOut));
       break;
       
#else // USE_STRUCT_NODES
       std::cerr << "you're not allowed to use displacement measure elements" << std::endl;
       THROW(ErrGeneric());
#endif // USE_STRUCT_NODES
    }
      
      /*  */
    case DISCRETECONTROL: {
       /* lettura dei dati specifici */
       
       /* Dati generali del controllore */
       integer iNumOutputs = HP.GetInt();
       integer iNumInputs = HP.GetInt();
       integer iOrderA = HP.GetInt();
       integer iOrderB = iOrderA;
       if (HP.IsKeyWord("fir")) {	 
	  iOrderB = HP.GetInt();
       }
              
       integer iNumIter = HP.GetInt();

       DEBUGCOUT("Discrete controller of order " << iOrderA);
       if (iOrderB != iOrderA) {
	  DEBUGCOUT(" (fir order " << iOrderB << ')' << std::endl);
       }
       DEBUGCOUT(": " << iNumOutputs << " output(s) and " 
		 << iNumInputs << " input(s)" << std::endl
		 << "Update every " << iNumIter << " iterations" << std::endl);
       
       /* Tipo di controllo */
       DiscreteControlProcess* pDCP = NULL;
       switch (HP.GetWord()) {
	case CONTROL: {
	   /* Add the file with control data */
	   const char* sControlFile(HP.GetFileName());
	   
	   DEBUGCOUT("Getting control matrices from file <"
		     << sControlFile << '>' << std::endl);
	   
	   std::ifstream iFIn(sControlFile);
	   if (!iFIn) {
	      std::cerr << "Error in opening control file <" 
		<< sControlFile << '>' << std::endl;	      
	      THROW(DataManager::ErrGeneric());
	   }
	   
	   /* Construction of controller */
	   SAFENEWWITHCONSTRUCTOR(pDCP, 
				  DiscreteControlARXProcess_Debug,
				  DiscreteControlARXProcess_Debug(iNumOutputs, 
								  iNumInputs,
								  iOrderA,
								  iOrderB,
								  iFIn));
	   
	   iFIn.close();	   
	   break;
	}
	  
	case IDENTIFICATION: {
	   flag f_ma = 0;
	   if (HP.IsKeyWord("arx")) {
	      f_ma = 0;
	   } else if (HP.IsKeyWord("armax")) {
	      f_ma = 1;
	   }

	   /* Forgetting factor */
	   ForgettingFactor* pFF = ReadFF(HP, iNumOutputs);
	   
	   /* Persistent excitation */
	   PersistentExcitation* pPX = ReadPX(pDM, HP, iNumInputs);
	   HP.PutKeyTable(K);
	   
	   char* s = NULL;
	   if (HP.IsKeyWord("file")) {
	      s = (char*)HP.GetFileName();
	   }
	   	   
	   /* Construction of controller */
	   SAFENEWWITHCONSTRUCTOR(pDCP,
				  DiscreteIdentProcess_Debug,
				  DiscreteIdentProcess_Debug(iNumOutputs,
							     iNumInputs,
							     iOrderA,
							     iOrderB,
							     pFF, pPX,
							     f_ma, s));
	      
	   break;
	}	   
	   
	case ADAPTIVECONTROL:  {
#ifdef USE_DBC
	   flag f_ma = 0;
	   doublereal dPeriodicFactor(0.);
	   
	   if (HP.IsKeyWord("arx")) {
	      DEBUGCOUT("ARX adaptive control" << std::endl);
	      f_ma = 0;
	   } else if (HP.IsKeyWord("armax")) {
	      DEBUGCOUT("ARMAX adaptive control" << std::endl);
	      f_ma = 1;
	   }

	   if (HP.IsKeyWord("periodic")) {
	      dPeriodicFactor = HP.GetReal();
           }
	   
	   GPCDesigner* pCD = NULL;
	   if (HP.IsKeyWord("gpc")) {
	      DEBUGCOUT("GPC adaptive control" << std::endl);

	      integer iPredS = HP.GetInt();
	      integer iContrS = HP.GetInt();	 
	      integer iPredH = HP.GetInt();
	      integer iContrH = 0;
	      
	      DEBUGCOUT("prediction advancing horizon: " << iPredS << std::endl
			<< "prediction receding horizon: " << iPredH << std::endl
			<< "control advancing horizon: " << iContrS << std::endl
			<< "control receding horizon: " << iContrH << std::endl);
	      
	      if (iPredS < 0) {
		 std::cerr << "Prediction advancing horizon (" << iPredS 
		   << ") must be positive" << std::endl;
		 THROW(ErrGeneric());
	      }
	      if (iPredH < 0) {
		 std::cerr << "Prediction receding horizon (" << iPredH
		   << ") must be positive" << std::endl;
		 THROW(ErrGeneric());
	      }
	      if (iPredH >= iPredS) {
		 std::cerr << "Prediction receding horizon (" << iPredH 
		   << ") must be smaller than prediction advancing horizon ("
		   << iPredS << ")" << std::endl;
		 THROW(ErrGeneric());
	      }
	      if (iContrS < 0) {
		 std::cerr << "Control advancing horizon (" << iContrS
		   << ") must be positive" << std::endl;
		 THROW(ErrGeneric());
	      }
	      
	      doublereal* pW = NULL;
	      doublereal* pR = NULL;
	      SAFENEWARR(pW, doublereal, iPredS-iPredH);
	      SAFENEWARR(pR, doublereal, iContrS-iContrH);
	      
	      if (HP.IsKeyWord("predictionweights")) {
		 DEBUGCOUT("prediction weights:" << std::endl);
		 for (integer i = iPredS-iPredH; i-- > 0; ) {
		    pW[i] = HP.GetReal();
		    DEBUGCOUT("W[" << i+1 << "] = " << pW[i] << std::endl);
		 }
	      } else {
		 for (integer i = 0; i < iPredS-iPredH; i++) {
		    pW[i] = 1.;
		 }
	      }
	      
	      if (HP.IsKeyWord("controlweights")) {
		 DEBUGCOUT("control weights:" << std::endl);
		 for (integer i = iContrS-iContrH; i-- > 0; ) {
		    pR[i] = HP.GetReal();
		    DEBUGCOUT("R[" << i+1 << "] = " << pR[i] << std::endl);
		 }
	      } else {
		 for (integer i = 0; i < iContrS-iContrH; i++) {
		    pR[i] = 1.;
		 }
	      }
	      
	      DEBUGCOUT("Weight Drive:" << std::endl);
	      DriveCaller* pLambda = ReadDriveData(pDM, HP, pDM->pGetDrvHdl());
	      
	      SAFENEWWITHCONSTRUCTOR(pCD,
				     GPC,
				     GPC(iNumOutputs, iNumInputs,
					 iOrderA, iOrderB, 
					 iPredS, iContrS, 
					 iPredH, iContrH,
					 pW, pR, pLambda,
					 dPeriodicFactor, f_ma));
	      
	   } else if (HP.IsKeyWord("deadbeat")) {
	      DEBUGCOUT("DeadBeat adaptive control" << std::endl);
	      
	      int iPredS = HP.GetInt();
	      int iContrS = HP.GetInt();
	      SAFENEWWITHCONSTRUCTOR(pCD,
				     DeadBeat,
				     DeadBeat(iNumOutputs, iNumInputs,
					      iOrderA, iOrderB, 
					      iPredS, iContrS,
					      dPeriodicFactor, f_ma));
	   }
	   
	   /* Forgetting factor */
	   DEBUGCOUT("Forgetting Factor:" << std::endl);
	   ForgettingFactor* pFF = ReadFF(HP, iNumOutputs);
	   
	   /* Persistent excitation */
	   DEBUGCOUT("Persistent Excitation:" << std::endl);
	   PersistentExcitation* pPX = ReadPX(pDM, HP, iNumInputs);
	   HP.PutKeyTable(K);
	   
	   DriveCaller* pTrig = NULL;
	   if (HP.IsKeyWord("trigger")) {	      
	      DEBUGCOUT("Trigger:" << std::endl);
	      pTrig = ReadDriveData(pDM, HP, pDM->pGetDrvHdl());
	   } else {
	      SAFENEWWITHCONSTRUCTOR(pTrig, 
				     OneDriveCaller,
				     OneDriveCaller(pDM->pGetDrvHdl()));
	   }
	   
	   /* desired output */
	   DriveCaller** pvDesiredOut = NULL;
	   if (HP.IsKeyWord("desiredoutput")) {
	      DEBUGCOUT("Desired output:" << std::endl);
	      SAFENEWARR(pvDesiredOut, DriveCaller*, iNumOutputs);
	      
	      for (integer i = 0; i < iNumOutputs; i++) {
		 DEBUGCOUT("output[" << i+1 << "]:" << std::endl);
		 pvDesiredOut[i] = ReadDriveData(pDM, HP, pDM->pGetDrvHdl());
	      }
	   }
	   	   
	   char* s = NULL;
	   if (HP.IsKeyWord("file")) {
	      s = (char*)HP.GetFileName();
	      DEBUGCOUT("Identified matrices will be output in file <" << s << '>' << std::endl);
	   }
	   	   
	   /* Construction of controller */
	   ASSERT(f_ma == 0 || f_ma == 1);
	   SAFENEWWITHCONSTRUCTOR(pDCP,
				  DAC_Process_Debug,
				  DAC_Process_Debug(iNumOutputs,
						    iNumInputs,
						    iOrderA,
						    iOrderB,
						    pFF,
						    pCD,
						    pPX,
						    pTrig,
						    pvDesiredOut,
						    s,
						    f_ma));
	   
	   break;
#else /* !USE_DBC */
	      std::cerr << "GPC/deadbeat control is not available" << std::endl;
	      THROW(ErrGeneric());
#endif /* !USE_DBC */
	}	   

	  
	  
	default: {
	   std::cerr << "Sorry, not implemented yed" << std::endl;	   
	   THROW(ErrNotImplementedYet());
	}
       }
       
       
       
       if (!HP.IsKeyWord("outputs")) {
	  std::cerr << "Error, outputs expected at line " 
	    << HP.GetLineData() << std::endl;
	  
	  THROW(DataManager::ErrGeneric());
       }
       
       ScalarDof* pOutputs = NULL;
       SAFENEWARR(pOutputs, ScalarDof, iNumOutputs);
       DriveCaller** ppOutScaleFact = NULL;
       SAFENEWARR(ppOutScaleFact, DriveCaller*, iNumOutputs);
       
       /* Allocazione nodi e connessioni */
       for (int i = 0; i < iNumOutputs; i++) {
	  pOutputs[i] = ReadScalarDof(pDM, HP, 1);
	  if (HP.IsKeyWord("scale")) {
	     ppOutScaleFact[i] = ReadDriveData(pDM, HP, pDM->pGetDrvHdl());
	  } else {
	     ppOutScaleFact[i] = NULL;
	     SAFENEWWITHCONSTRUCTOR(ppOutScaleFact[i], 
				    OneDriveCaller, 
				    OneDriveCaller(pDM->pGetDrvHdl()));
	  }
       }
                            
       if (!HP.IsKeyWord("inputs")) {
	  std::cerr << "Error, inputs expected at line "
	    << HP.GetLineData() << std::endl;
	  THROW(DataManager::ErrGeneric());
       }	   
       
       /* Same thing for input nodes */
       ScalarDof* pInputs = NULL;
       SAFENEWARR(pInputs, ScalarDof, iNumInputs);
       
       /* Allocazione nodi e connessioni */
       for (int i = 0; i < iNumInputs; i++) {
	  pInputs[i] = ReadScalarDof(pDM, HP, 1);
	  if (pInputs[i].pNode->GetNodeType() ==  Node::PARAMETER) {
	     std::cerr << "Sorry, parameters are not allowed as input nodes" 
	       << std::endl;	     
	     THROW(DataManager::ErrGeneric());	      
	  }	      
       }
       
       HP.PutKeyTable(K);

       flag fOut = pDM->fReadOutput(HP, Elem::ELECTRIC);
       
       SAFENEWWITHCONSTRUCTOR(pEl,
			      DiscreteControlElem,
			      DiscreteControlElem(uLabel, pDO,
						  iNumOutputs,
						  pOutputs,
						  ppOutScaleFact,
						  iNumInputs,
						  pInputs,
						  pDCP,
						  iNumIter,
						  fOut));
       
       break;
    }
                        
      /* Aggiungere altri elementi elettrici */
      
    default: {
       std::cerr << "unknown electric element type in electric element " << uLabel 
	 << " at line " << HP.GetLineData() << std::endl;       
       THROW(DataManager::ErrGeneric());
    }	
   }
   
   /* Se non c'e' il punto e virgola finale */
   if (HP.fIsArg()) {
      std::cerr << "semicolon expected at line " << HP.GetLineData() << std::endl;     
      THROW(DataManager::ErrGeneric());
   }   
   
   return pEl;
} /* ReadElectric() */

#endif /* USE_ELECTRIC_NODES */

