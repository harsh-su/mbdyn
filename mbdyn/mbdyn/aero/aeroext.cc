/* 
 * MBDyn (C) is a multibody analysis code. 
 * http://www.mbdyn.org
 *
 * Copyright (C) 1996-2003
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

#ifdef USE_EXTERNAL 
#include "aeroext.h"
#include <myassert.h>
#include <except.h>
#include <dataman.h>
#include <external.h>
#include <drive_.h>

/* le label delle comunicazioni vengono costruite in questo modo:
 *
 *	blockID*10   +  1   label e numero nodi aggiuntivi 
 *			2   posizioni
 *			3   velocita'
 *			4   forze
 *			5   momenti
 *
 */

/* Aerodynamic External - begin */
AerodynamicExternal::AerodynamicExternal(unsigned int uLabel,
					const DriveCaller* pDC,
					int NN,
			    		const StructNode** ppN,
			    		const doublereal* pRefL,
			    		MPI::Intercomm* pIC,
					flag fOut,
					bool VF,
					bool MF)
:Elem(uLabel, Elem::AERODYNAMIC, fOut), 
AerodynamicElem(uLabel, AerodynamicElem::AERODYNAMICEXTERNAL, fOut),
DriveOwner(pDC),
pdBuffer(NULL),
pdBufferVel(NULL),
NodeN(NN),
ppNode(ppN),
pRefLength(pRefL),
OffN(3),
pOffsetVectors(NULL),
pInterfComm(pIC),
pSenReq(NULL),
pRecReq(NULL),
VelFlag(VF),
MomFlag(MF),
SentFlag(false)
{
	
	/* costruisce la mat 3xN degli offset */
	SAFENEWWITHCONSTRUCTOR(pOffsetVectors, Mat3xN, Mat3xN(OffN, 0.));
	for (int i=1; i<=3; i++) pOffsetVectors->Put(i,i,1.);
	ConstructAndInitialize();
	
		
}

AerodynamicExternal::AerodynamicExternal(unsigned int uLabel,
					const DriveCaller* pDC,
					int NN,
			    		const StructNode** ppN,
			    		const doublereal* pRefL,
			    		MPI::Intercomm* pIC,
					int	ON,
			    		Mat3xN* OV,
			    		flag fOut,
			    		bool VF,
					bool MF)
:Elem(uLabel, Elem::AERODYNAMIC, fOut), 
AerodynamicElem(uLabel, AerodynamicElem::AERODYNAMICEXTERNAL, fOut),
DriveOwner(pDC),
pdBuffer(NULL),
pdBufferVel(NULL),
NodeN(NN),
ppNode(ppN),
pRefLength(pRefL),
OffN(ON),
pOffsetVectors(OV),
pInterfComm(pIC),
pSenReq(NULL),
pRecReq(NULL),
VelFlag(VF),
MomFlag(MF),
SentFlag(false)
{	
	ConstructAndInitialize();
}	

AerodynamicExternal::~AerodynamicExternal(void)
{
	if (pdBuffer != NULL) SAFEDELETE(pdBuffer);
	if (pdBufferVel != NULL) SAFEDELETE(pdBufferVel);
	if (ppNode != NULL) SAFEDELETEARR(ppNode);
	if (pRefLength != NULL) SAFEDELETEARR(pRefLength);
	if (pSenReq != NULL) SAFEDELETEARR(pSenReq);
	if (pRecReq != NULL) SAFEDELETEARR(pRecReq);
}
		
void AerodynamicExternal::ConstructAndInitialize(void) 
{
	/* dimensiona l'array per il buffer */
	DEBUGCOUTFNAME("AerodynamicExternal::ConstructAndInitialize");
	SAFENEWWITHCONSTRUCTOR(pdBuffer, MyVectorHandler,
			MyVectorHandler(8+3*NodeN*(OffN+1)));
	
	if (VelFlag || MomFlag) SAFENEWWITHCONSTRUCTOR(pdBufferVel, MyVectorHandler,
			MyVectorHandler(3*NodeN*(OffN+1)));
	
	/* invio delle posizioni iniziali dei nodi per il calcolo della matrice di interpolazione */
	unsigned int*  pLabList = NULL; 
	SAFENEWARR(pLabList, unsigned int, 2*NodeN);
	Mat3xN MatTmp(OffN,0.);
	Vec3 X;
	for (int i=0; i < NodeN; i++) {
		pLabList[i] = ppNode[i]->GetLabel();
		pLabList[i+NodeN] = 1+OffN;
		X = ppNode[i]->GetXCurr();
		pdBuffer->Put(i*(OffN+1)*3+1, X);
		for (int j=1; j <= OffN; j++) {
			MatTmp.LeftMult(ppNode[i]->GetRCurr(), *pOffsetVectors);
			MatTmp *= pRefLength[i];
			pdBuffer->Put(i*(OffN+1)*3+j*3+1, X + MatTmp.GetVec(j));
		}
	}
	/* il rank del processo con cui si comunica e' zero perche' il comuncatire del 
	codice di interfaccia e' sempre fatto da una macchina sola */
	pInterfComm->Send(pLabList, 2*NodeN, MPI::UNSIGNED, 0,(this->GetLabel())*10+1); 
	pInterfComm->Send(pdBuffer->pdGetVec(), 3*NodeN*(OffN+1), MPI::DOUBLE, 0,(this->GetLabel())*10+2);
	
	/* creo le request per la trasmissione e ricezione dei dati */
	 
	if (VelFlag) {
		SAFENEWARR(pSenReq, MPI::Prequest, 2);
		pSenReq[0] = pInterfComm->Send_init(pdBuffer->pdGetVec(), 8+3*NodeN*(OffN+1), MPI::DOUBLE, 0,(this->GetLabel())*10+2);
		pSenReq[1] = pInterfComm->Send_init(pdBufferVel->pdGetVec(), 3*NodeN*(OffN+1), MPI::DOUBLE, 0,(this->GetLabel())*10+3);
	} else { 
		SAFENEWARR(pSenReq, MPI::Prequest, 1);
		pSenReq[0] = pInterfComm->Send_init(pdBuffer->pdGetVec(), 8+3*NodeN*(OffN+1), MPI::DOUBLE, 0,(this->GetLabel())*10+2);
	}			
	if (MomFlag) {
		SAFENEWARR(pRecReq, MPI::Prequest, 2);
		pRecReq[0] = pInterfComm->Recv_init(pdBuffer->pdGetVec(), 3*NodeN*(OffN+1), MPI::DOUBLE, 0,(this->GetLabel())*10+4);
		pRecReq[1] = pInterfComm->Recv_init(pdBufferVel->pdGetVec(), 3*NodeN*(OffN+1), MPI::DOUBLE, 0,(this->GetLabel())*10+5);
	}else {
		SAFENEWARR(pRecReq, MPI::Prequest, 1);
		pRecReq[0] = pInterfComm->Recv_init(pdBuffer->pdGetVec(), 3*NodeN*(OffN+1), MPI::DOUBLE, 0,(this->GetLabel())*10+4);
	}
	SAFEDELETEARR(pLabList);	
}


/* invia posizione e velocita' predetti */ 
void AerodynamicExternal::AfterPredict(VectorHandler& X  , 
					VectorHandler&  XP  )
{
	DEBUGCOUTFNAME("AerodynamicExternal::AfterPredict");
	std::cout << "New Step "  << std::endl; 
	Send(X, XP);
}

/* invia posizione e velocita' predetti */ 
void AerodynamicExternal::Update(const VectorHandler& X  , 
					const VectorHandler&  XP  )
{
	DEBUGCOUTFNAME("AerodynamicExternal::Update");
	Send(X, XP);
}

void AerodynamicExternal::Send(const VectorHandler& X  , 
					const VectorHandler&  XP  )
{
	Vec3 Vinf(0.);
	Vec3 Xc(0.), V;
	Mat3x3 R;
	doublereal rho, c, p, T;
	GetAirProps(Xc, rho, c, p, T); 
	fGetAirVelocity(Vinf,Xc); 
	/* current time */
	pdBuffer->fPutCoef(1, DriveOwner::dGet());
	pdBuffer->Put(2, Vinf);
	pdBuffer->fPutCoef(5, rho);
	pdBuffer->fPutCoef(6,c);
	pdBuffer->fPutCoef(7,p);
	pdBuffer->fPutCoef(8,T);
 		
	Mat3xN MatTmp(OffN,0.);
	for (int i=0; i < NodeN; i++) {
		Xc = ppNode[i]->GetXCurr();
		pdBuffer->Put(8+i*(OffN+1)*3+1, Xc);
		if (VelFlag) {
			V = ppNode[i]->GetVCurr();
			pdBufferVel->Put(i*(OffN+1)*3+1, V);
		}
		for (int j=1; j <= OffN; j++) {
			R = ppNode[i]->GetRCurr();
			MatTmp.LeftMult(R, *pOffsetVectors);
			MatTmp *= pRefLength[i];
			pdBuffer->Put(8+i*(OffN+1)*3+j*3+1, Xc + MatTmp.GetVec(j));
			if (VelFlag) pdBufferVel->Put(i*(OffN+1)*3+j*3+1, V + ( Mat3x3(ppNode[i]->GetWCurr()) * R)* MatTmp.GetVec(j));
		}
	}
	MPI::Status ss;
	pSenReq->Wait(ss);
	int pippo = ss.Get_error();
	std::cout << "pippo 1 " << pippo << std::endl; 
	if (VelFlag) {
		(pSenReq[1]).Wait(ss);
		int pippo = ss.Get_error();
		std::cout << "pippo 2 " << pippo << std::endl; 
	}
	MPI::Prequest::Startall(1 + VelFlag, pSenReq);
	std::cout <<"nnnnnnnnnnnnnnnnnnnnnnnnnnnnnnnnnn "  << std::endl; 
	/* attiva la ricezione delle forze */
	MPI::Prequest::Startall(1 , pRecReq);
	SentFlag = true; 
}

SubVectorHandler&
AerodynamicExternal::AssRes(SubVectorHandler& WorkVec,
	       		 	doublereal dCoef,
	       			const VectorHandler& XCurr,
	       			const VectorHandler& XPrimeCurr)
{
	DEBUGCOUTFNAME("AerodynamicExternal::AssRes");
	/* verifica il completamento dei send */
	if(!SentFlag) Send(XCurr, XPrimeCurr);
	
	MPI::Status ss;
	pSenReq->Wait(ss);
	int pippo = ss.Get_error();
	std::cout << "pippo 1 " << pippo << std::endl; 
	if (VelFlag) {
		(pSenReq[1]).Wait(ss);
		int pippo = ss.Get_error();
		std::cout << "pippo 2 " << pippo << std::endl; 
	}
	WorkVec.Resize(6*NodeN);
   	WorkVec.Reset(0.);

	
	for (int i=0; i < NodeN; i++) {
		integer iFirstIndex = ppNode[i]->iGetFirstMomentumIndex();
   		for (int iCnt = 1; iCnt <= 6; iCnt++) {
      			WorkVec.fPutRowIndex(i*6+iCnt, iFirstIndex+iCnt);
   		}
	}
		
	/* attende il completamento dei receive */
	MPI::Prequest::Waitall(1 + MomFlag, pRecReq);
	
	/* calcola le forze su ciascun nodo */
	/* le forze e i momenti sono gia' espressi nel sistema di riferimento global */
	Vec3 FT(0.);
	Vec3 MT(0.);
	for (int i=0; i < NodeN; i++) {
		Vec3 F(0.);
		Vec3 Fo(0.);
		Vec3 M(0.);
		F[0] +=  pdBuffer->dGetCoef(i*(OffN+1)*3+1);
		F[1] +=  pdBuffer->dGetCoef(i*(OffN+1)*3+2);
		F[2] +=  pdBuffer->dGetCoef(i*(OffN+1)*3+3);
		if (MomFlag) {
			M[0] +=  pdBufferVel->dGetCoef(i*(OffN+1)*3+1);
			M[1] +=  pdBufferVel->dGetCoef(i*(OffN+1)*3+2);
			M[2] +=  pdBufferVel->dGetCoef(i*(OffN+1)*3+3);
		}
		for (int j=1; j <= OffN; j++) {
			Fo[0] =  pdBuffer->dGetCoef(i*(OffN+1)*3+j*3+1);
			Fo[1] =  pdBuffer->dGetCoef(i*(OffN+1)*3+j*3+2);
			Fo[2] =  pdBuffer->dGetCoef(i*(OffN+1)*3+j*3+3);			
			F += Fo;
			if (MomFlag) {
				M[0] +=  pdBufferVel->dGetCoef(i*(OffN+1)*3+j*3+1);
				M[1] +=  pdBufferVel->dGetCoef(i*(OffN+1)*3+j*3+2);
				M[2] +=  pdBufferVel->dGetCoef(i*(OffN+1)*3+j*3+3);
			}
			//Vec3 TmpF(*pdBuffer, i*(OffN+1)*3+(j+1)*3+1);
			
			M -= Fo.Cross( ppNode[i]->GetRCurr() * pOffsetVectors->GetVec(j) * pRefLength[i]);
		}
		WorkVec.Add(i*6+1, F);
   		WorkVec.Add(i*6+4, M);
		FT +=F;
		MT +=M;
	}
	std::cerr << "FT " << FT << std::endl;
	std::cerr << "MT " << MT << std::endl;
	SentFlag = false; 
	return WorkVec;			
}

Elem *
ReadAerodynamicExternal(DataManager* pDM, MBDynParser& HP, unsigned int uLabel)
{
   	/* legge i dati d'ingresso e li passa al costruttore dell'elemento */
   	AerodynamicExternal* pEl = NULL;
   
   	unsigned int NodeN = HP.GetInt(); 
   
   	DEBUGCOUT("Aerodynamic External Block made of : " << NodeN << " Nodes." << std::endl);    
   	std::cout << "Aerodynamic External Block made of : " << NodeN << " Nodes." << std::endl;    
   	
	const StructNode** ppNodeList = NULL;
	SAFENEWARR(ppNodeList, const StructNode*, NodeN);
	
	StructNode* pTmpNode; 
   	for (unsigned int iN=0; iN < NodeN; iN++) {
		unsigned int NodeL = HP.GetInt(); 
   		if ((pTmpNode = pDM->pFindStructNode(NodeL)) == NULL) {
      			std::cerr << "Aerodynamic External(" << uLabel 
				<< "): structural node " << NodeL
				<< " at line " << HP.GetLineData()
				<< " not defined" << std::endl;
      			THROW(DataManager::ErrGeneric());
		} else {
		 	ppNodeList[iN] = pTmpNode;
		}
	}
	doublereal* pRefLenght = NULL;
	SAFENEWARR(pRefLenght, doublereal, NodeN);
	if (HP.IsKeyWord("list")) {
   		for (unsigned int iN=0; iN < NodeN; iN++) {
			pRefLenght[iN] = HP.GetReal();
		}
	} else {
		pRefLenght[0] = HP.GetReal();
   		for (unsigned int iN=1; iN < NodeN; iN++) {
			pRefLenght[iN] = pRefLenght[iN-1];
		}
	}
	
	int OffN = 0;
	Mat3xN* pOffVec = NULL;
	if (HP.IsKeyWord("offset")) {
		OffN = HP.GetInt();
		SAFENEWWITHCONSTRUCTOR(pOffVec, Mat3xN, Mat3xN(OffN));
		for (int iOf = 1; iOf <= OffN; iOf++) {
			for (int iCnt = 1; iCnt <= 3; iCnt++) {
				pOffVec->Put(iCnt, iOf, HP.GetReal());
			}
		}
	}

	
	int comm = 0;
	if (HP.IsKeyWord("comm")) {
		comm = HP.GetInt();
	}
	std::list<MPI::Intercomm>::iterator iComm =  InterfaceComms.begin();
	std::list<MPI::Intercomm>::iterator iComm_end =  InterfaceComms.end();
	if (iComm == iComm_end) {
                   std::cerr << "Aerodynamic External(" << uLabel
                                << "): there are no external communicators " 
                                << "Aborting." << std::endl;
                   THROW(DataManager::ErrGeneric());
	}

	for (int i = 0; i++ < comm; iComm++) {
		if (iComm == iComm_end) {
                   std::cerr << "Aerodynamic External(" << uLabel
                                << "): external communicator " << comm 
				<< "does not exist. Last communicator is " << i-1     
                                << ". Aborting." << std::endl;
                   THROW(DataManager::ErrGeneric());
		}
	}
	MPI::Intercomm* pInterC = &(*(iComm));

	bool VelFlag = false;
	bool MomFlag = false;
	
	if (HP.IsKeyWord("velocity")) VelFlag = true;
	if (HP.IsKeyWord("moment")) MomFlag = true;
		  
   	/* per ora l'elemento non genera output */
	flag fOut = 0;
	DriveCaller* pDC = NULL;
	SAFENEWWITHCONSTRUCTOR(pDC, TimeDriveCaller, TimeDriveCaller(pDM->pGetDrvHdl()));
	
   	if (OffN) {
		SAFENEWWITHCONSTRUCTOR(pEl, 
				AerodynamicExternal,
				AerodynamicExternal(uLabel,
						pDC,
						NodeN,
						ppNodeList,
						pRefLenght,
						pInterC,
						OffN,
						pOffVec,
						fOut,
						VelFlag,
						MomFlag));
	} else {
		SAFENEWWITHCONSTRUCTOR(pEl, 
				AerodynamicExternal,
				AerodynamicExternal(uLabel,
						pDC,
						NodeN,
						ppNodeList,
						pRefLenght,
						pInterC,
						fOut,
						VelFlag,
						MomFlag));
	}
						
 	return pEl;   	
	
}

   
/* Aerodynamic External - end */


/* Aerodynamic External Modal - begin */

AerodynamicExternalModal::AerodynamicExternalModal(unsigned int uLabel,
							const DriveCaller* pDC,
			    				Modal* pM,
			    				MPI::Intercomm* IC,
			    				flag fOut,
			    				bool VF,
							bool MF)
:Elem(uLabel, Elem::AERODYNAMIC, fOut), 
AerodynamicElem(uLabel, AerodynamicElem::AERODYNAMICEXTERNALMODAL, fOut),
DriveOwner(pDC),
pdBuffer(NULL),
pdBufferVel(NULL),
pModal(pM),
ModalNodes(pM->uGetNFemNodes()),
pInterfComm(IC),
pSenReq(NULL),
pRecReq(NULL),
VelFlag(VF),
MomFlag(MF),
SentFlag(false)
{
	SAFENEWWITHCONSTRUCTOR(pdBuffer, MyVectorHandler,
			MyVectorHandler(8+3*ModalNodes));
	
	if (VelFlag || MomFlag) SAFENEWWITHCONSTRUCTOR(pdBufferVel, MyVectorHandler,
			MyVectorHandler(3*ModalNodes));
	
	unsigned int*  pLabList = NULL; 
	SAFENEWARR(pLabList, unsigned int, 2);
	pLabList[0] = pModal->GetLabel();
	pLabList[1] = ModalNodes;
	/* il rank del processo con cui si comunica e' zero perche' il comuncatore del 
	codice di interfaccia e' sempre fatto da una macchina sola */
	pInterfComm->Send(pLabList, 2, MPI::UNSIGNED, 0,(this->GetLabel())*10+1); 
	
	Mat3xN* pNodePos = pModal->GetCurrFemNodesPosition();
	for (int i=0; i < ModalNodes; i++) {
		pdBuffer->Put(i*3, pNodePos->GetVec(i+1));
	}
	pInterfComm->Send(pdBuffer->pdGetVec(), 3*ModalNodes, MPI::DOUBLE, 0,(this->GetLabel())*10+2);
	/* creo le request per la trasmissione e ricezione dei dati */
	 
	if (VelFlag) {
		SAFENEWARR(pSenReq, MPI::Prequest, 2);
		pSenReq[0] = pInterfComm->Send_init(pdBuffer->pdGetVec(), 8+3*ModalNodes, MPI::DOUBLE, 0,(this->GetLabel())*10+2);
		pSenReq[1] = pInterfComm->Send_init(pdBufferVel->pdGetVec(), 3*ModalNodes, MPI::DOUBLE, 0,(this->GetLabel())*10+3);
	} else { 
		SAFENEWARR(pSenReq, MPI::Prequest, 1);
		pSenReq[0] = pInterfComm->Send_init(pdBuffer->pdGetVec(), 8+3*ModalNodes, MPI::DOUBLE, 0,(this->GetLabel())*10+2);
	}			
	if (MomFlag) {
		SAFENEWARR(pRecReq, MPI::Prequest, 2);
		pRecReq[0] = pInterfComm->Recv_init(pdBuffer->pdGetVec(), 3*ModalNodes, MPI::DOUBLE, 0,(this->GetLabel())*10+4);
		pRecReq[1] = pInterfComm->Recv_init(pdBufferVel->pdGetVec(), 3*ModalNodes, MPI::DOUBLE, 0,(this->GetLabel())*10+5);
	}else {
		SAFENEWARR(pRecReq, MPI::Prequest, 1);
		pRecReq[0] = pInterfComm->Recv_init(pdBuffer->pdGetVec(), 3*ModalNodes, MPI::DOUBLE, 0,(this->GetLabel())*10+4);
	}

}

AerodynamicExternalModal::~AerodynamicExternalModal(void)
{
	if (pdBuffer != NULL) SAFEDELETE(pdBuffer);
	if (pdBufferVel != NULL) SAFEDELETE(pdBufferVel);
	if (pSenReq != NULL) SAFEDELETEARR(pSenReq);
	if (pRecReq != NULL) SAFEDELETEARR(pRecReq);
}

/* invia posizione e velocita' predetti */ 
void AerodynamicExternalModal::AfterPredict(VectorHandler& X  , 
					VectorHandler&  XP  )
{
	DEBUGCOUTFNAME("AerodynamicExternalModal::AfterPredict");
	Send(X, XP);
}

/* invia posizione e velocita' predetti */ 
void AerodynamicExternalModal::Update(const VectorHandler& X  , 
					const VectorHandler&  XP  )
{
	DEBUGCOUTFNAME("AerodynamicExternalModal::Update");
	Send(X, XP);
}

void AerodynamicExternalModal::Send(const VectorHandler& X  , 
					const VectorHandler&  XP  )
{	

	Vec3 Vinf(0.);
	doublereal rho, c, p, T;
	GetAirProps(Vinf, rho, c, p, T); 
	/* current time */
	pdBuffer->fPutCoef(1, DriveOwner::dGet());
	pdBuffer->Put(2, Vinf);
	pdBuffer->fPutCoef(5, rho);
	pdBuffer->fPutCoef(6,c);
	pdBuffer->fPutCoef(7,p);
	pdBuffer->fPutCoef(8,T);

	Mat3xN* pNodePos = pModal->GetCurrFemNodesPosition();
	Mat3xN* pNodeVel = NULL;
	if (VelFlag) pNodeVel = pModal->GetCurrFemNodesVelocity();
	for (int i=0; i < ModalNodes; i++) {
		pdBuffer->Put(8+i*3, pNodePos->GetVec(i+1));
		if (VelFlag) pdBufferVel->Put(i*3, pNodeVel->GetVec(i+1));
	}
		
	MPI::Prequest::Startall(1 + VelFlag, pSenReq);
	/* attiva la ricezione delle forze */
	MPI::Prequest::Startall(1 + MomFlag, pRecReq);
	SentFlag = true; 
 
}


SubVectorHandler&
AerodynamicExternalModal::AssRes(SubVectorHandler& WorkVec,
	       			doublereal dCoef,
	       			const VectorHandler& XCurr,
	       			const VectorHandler& XPrimeCurr)
{
	DEBUGCOUTFNAME("AerodynamicExternalModal::AssRes");
	/* verifica il completamento dei send */
	if(!SentFlag) Send(XCurr, XPrimeCurr);
	MPI::Prequest::Waitall(1 + VelFlag, pSenReq);
   	integer NModes = pModal->uGetNModes();
	WorkVec.Resize(NModes);
   	WorkVec.Reset(0.);

	
	integer iFirstIndex = pModal->iGetFirstIndex();
	for (int iCnt=0; iCnt < NModes; iCnt++) {
      		WorkVec.fPutRowIndex(iCnt, iFirstIndex+NModes+iCnt);
	}
		
	/* attende il completamento dei receive */
	MPI::Prequest::Waitall(1 + MomFlag, pRecReq);
	
	/* calcola le forze su ciascun nodo 
	 * le forze e i momenti sono espressi nel sistema di riferimento global 
	 * vanno quindi riportati nel sistema di riferimento del nodo modal e
	 * poi proiettati sulla base modale				
	 */
		
	Vec3 F(0.);
	Vec3 M(0.);
	Mat3x3 R((pModal->pGetModalNode())->GetRCurr());
	Mat3x3 RT = R.Transpose();

	for (int iNode=0; iNode < ModalNodes; iNode++) {
		F[0] +=  pdBuffer->dGetCoef(iNode*(ModalNodes)*3+1);
		F[1] +=  pdBuffer->dGetCoef(iNode*(ModalNodes)*3+2);
		F[2] +=  pdBuffer->dGetCoef(iNode*(ModalNodes)*3+3);
		if (MomFlag) {
			M[0] +=  pdBufferVel->dGetCoef(iNode*(ModalNodes)*3+1);
			M[1] +=  pdBufferVel->dGetCoef(iNode*(ModalNodes)*3+2);
			M[2] +=  pdBufferVel->dGetCoef(iNode*(ModalNodes)*3+3);
		}
		for (int iMode=0; iMode < NModes; iMode++) {
			WorkVec.Add(iMode+1, RT*F*((pModal->pGetPHIt())->GetVec(iMode*ModalNodes+iNode+1)));
			if (MomFlag) WorkVec.Add(iMode+1, RT*M*((pModal->pGetPHIr())->GetVec(iMode*ModalNodes+iNode+1)));
		}	
	}
	SentFlag = false; 
	return WorkVec;			
}
	

Elem *
ReadAerodynamicExternalModal(DataManager* pDM, MBDynParser& HP, unsigned int uLabel)
{
   	/* legge i dati d'ingresso e li passa al costruttore dell'elemento */
   	AerodynamicExternalModal* pEl = NULL;
	
      	/* giunto modale collegato */		     
   	Elem* pM = pDM->ReadElem(HP, Elem::JOINT);
   	Modal* pModalJoint = (Modal*)pM->pGet();
   	if (pModalJoint->GetJointType() != Joint::MODAL) {
      		std::cerr << "Joint " << pModalJoint->GetLabel()
	      		<< " is required to be a modal joint" << std::endl;
      		THROW(DataManager::ErrGeneric());
   	}
	int comm = 0;
	if (HP.IsKeyWord("comm")) {
		comm = HP.GetInt();
	}
	
	std::list<MPI::Intercomm>::iterator iComm =  InterfaceComms.begin();
	for (int i = 0; i++ < comm; iComm++) {}
	MPI::Intercomm* pInterC = &(*(iComm));

	bool VelFlag = false;
	bool MomFlag = false;
	
	if (HP.IsKeyWord("velocity")) VelFlag = true;
	if (HP.IsKeyWord("moment")) MomFlag = true;
		  
   	/* per ora l'elemento non genera output */
	flag fOut = 0;
	DriveCaller* pDC = NULL;
	SAFENEWWITHCONSTRUCTOR(pDC, TimeDriveCaller, TimeDriveCaller(pDM->pGetDrvHdl()));

	SAFENEWWITHCONSTRUCTOR(pEl, 
			AerodynamicExternalModal,
			AerodynamicExternalModal(uLabel,
						pDC,
						pModalJoint,
						pInterC,
						fOut,
						VelFlag,
						MomFlag));
						
 	return pEl;   		
}

/* Aerodynamic External Modal - end */

#endif /* USE_EXTERNAL */

