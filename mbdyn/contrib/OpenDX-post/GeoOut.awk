# Prints the labels
function print_labels() {
	printf("object \"TriadLabels\" class array type int rank 1 shape 1 items %d data follows\n", NumNodes);
	for (i = 1; i <= NumNodes; i++) {
		printf("\t%d\n", Nodes[i]);
	}
}
# Prints the objects at one step
function print_step() {
	t = Start+Step*Incr;
	printf("attribute \"dep\" string \"positions\"\n");
	printf("object \"TriadPositions%f\" class array type float rank 1 shape 3 items %d data follows\n", t, NumNodes);
	for (i = 1; i <= NumNodes; i++) {
		printf("\t%e %e %e\n", 
			Positions[i,1], Positions[i,2], Positions[i,3]);
	}
	printf("object \"TriadCosines%f\" class array type float rank 1 shape 9 items %d data follows\n", t, NumNodes);
	for (i = 1; i <= NumNodes; i++) {
		printf("\t%e %e %e %e %e %e %e %e %e\n", 
			Cosines[i,1], Cosines[i,2], Cosines[i,3], 
			Cosines[i,4], Cosines[i,5], Cosines[i,6], 
			Cosines[i,7], Cosines[i,8], Cosines[i,9]);
	}
	printf("attribute \"dep\" string \"positions\"\n");
	printf("object \"OrientedTriads%f\" class field\n", t);
	printf("component \"positions\" \"TriadPositions%f\"\n", t);
	printf("component \"cosines\" \"TriadCosines%f\"\n", t);
	printf("component \"labels\" \"TriadLabels\"\n");
	printf("object \"MbdynSym%f\" class group\n", t);
	printf("member \"OrientedTriads\" value \"OrientedTriads%f\"\n", t);
}
# Prints the members of the group at the end
function print_members() {
	printf("object \"series\" class series\n");
	for (i = 0; i <= Step; i++) {
		t = Start+i*Incr;
		printf("member %d position %f value \"MbdynSym%f\"\n", i, t, t);
	}
	printf("end\n");
}
# Initialize vars
BEGIN {
	FirstLabel = -1;
	FirstStep = 1;
	Incr = .5e-3;
	Start = 0.0;
	Step = 0;
	deg2rad = 0.017453293;
	rad2deg = 57.29578;
	AngleScale = deg2rad;
}
# Generic rule --- add here any specific check to filter out undesired labels
$1 > 10 {
	if ($1 == FirstLabel) {
		Node = 0;
		if (FirstStep) {
			FirstStep = 0;
			print_labels();
		}
		print_step();
		Step++;
	}

	if (FirstLabel == -1) {
		FirstLabel = $1;
	}
	
	Node++;
	if (FirstStep) {
		NumNodes = Node;
		Nodes[Node] = $1;
	}

	Positions[Node,1] = $2;
	Positions[Node,2] = $3;
	Positions[Node,3] = $4;

	dCosAlpha = cos($5*AngleScale);
	dSinAlpha = sin($5*AngleScale);
	dCosBeta = cos($6*AngleScale);
	dSinBeta = sin($6*AngleScale);
	dCosGamma = cos($7*AngleScale);
	dSinGamma = sin($7*AngleScale);
	Cosines[Node,1] = dCosBeta*dCosGamma;
	Cosines[Node,2] = dCosAlpha*dSinGamma+dSinAlpha*dSinBeta*dCosGamma;
	Cosines[Node,3] = dSinAlpha*dSinGamma-dCosAlpha*dSinBeta*dCosGamma;
	Cosines[Node,4] = -dCosBeta*dSinGamma;
	Cosines[Node,5] = dCosAlpha*dCosGamma-dSinAlpha*dSinBeta*dSinGamma;
	Cosines[Node,6] = dSinAlpha*dCosGamma+dCosAlpha*dSinBeta*dSinGamma;
	Cosines[Node,7] = dSinBeta;
	Cosines[Node,8] = -dSinAlpha*dCosBeta;
	Cosines[Node,9] = dCosAlpha*dCosBeta;
}
# Print last step and members
END {
	print_step();
	print_members();
}

