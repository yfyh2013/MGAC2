//
//Copyright (c) 2017 Albert M. Lund (a.m.lund@utah.edu)
//
//This source code is subject to the license found at https://github.com/MGAC-group/MGAC2
//

#include "gasp2struct.hpp"

using namespace std;

//no longer used
const vector<Vec3> axissettings = {
		Vec3(1.0,2.0,0.0),
		Vec3(1.0,0.0,2.0),
		Vec3(2.0,1.0,0.0),
		Vec3(2.0,0.0,1.0),
		Vec3(0.0,1.0,2.0),
		Vec3(0.0,2.0,1.0),
};

vector<string> GASP2struct::names;

//constructor, duh
GASP2struct::GASP2struct() {
	molecules.clear();
	isFitcell = false;
	didOpt = false;
	complete = false;
	finalstate = OKStruct;
	crylabel = newName("CRY");
	time = 0;
	steps = 0;

	energy = 0.0;
	force = 0.0;
	pressure = 0.0;
	contacts = 0;
	pseudoenergy = 0.0;
	interdist = 0.0;
	intradist = 0.0;
	contactdist=0.2;
	maxvol = 0.0;
	minvol = 0.0;

	ID.generate();
	parentA.clear();
	parentB.clear();

	cluster=-1;
	clustergroup=-1;

	generation=0;


}

//gets the volume, checks to make sure it isn't NaN
double GASP2struct::getVolume() {
	double vol = cellVol(unit);
	//strongly penalize structures that spit
	//out a nan for the volume
	//results from bad things (tm), like bad cell values
	//that make impossible unit cells
	if(std::isnan(vol))
		vol = -1.0;
	return vol;
}


//returns a fractional volume score for sorting
double GASP2struct::getVolScore() {
	double expectvol = 0.0;
	double vol = getVolume();

	//propogate the NaN
	if(vol < 0.0)
		return -1.0;

	//must work independent of fitcell
	for(int i = 0; i < molecules.size(); i++) {
		expectvol += molecules[i].expectvol;
	}
	if(!isFitcell) {
		Spgroup spg = spacegroups[unit.spacegroup];
		int nops = spg.R.size();
		expectvol *= (double) nops;
	}
	return std::abs(vol-expectvol)/expectvol;

}


//determines if the volume is within the volume limits
bool GASP2struct::minmaxVol() {
	double expectvol = 0.0;
	double vol = getVolume();
	//must work independent of fitcell
	//cout << "vol: " << vol << " ";
	for(int i = 0; i < molecules.size(); i++) {
		expectvol += molecules[i].expectvol;
	}
	if(!isFitcell) {
		Spgroup spg = spacegroups[unit.spacegroup];
		int nops = spg.R.size();
		expectvol *= (double) nops;
	}

	//cout << "expectvol: " << expectvol << " ";
	//cout << "a: " << (((vol/expectvol)-1.0) < (minvol/100.0));
	//cout << " b: " << (((vol/expectvol)-1.0) > (maxvol/100.0)) << endl;
	//cout << "cell:" <<unit.a<<" "<<unit.b<<" "<<unit.c<<endl;

	if( ((vol/expectvol)-1.0) < (minvol/100.0) ||
		((vol/expectvol)-1.0) > (maxvol/100.0) )
			return false;
	return true;
}

//secondary check to make sure a structure is within the max volume
bool GASP2struct::checkMaxVol() {
	double expectvol = 0.0;
	double vol = getVolume();
	//must work independent of fitcell
	//cout << "vol: " << vol << " ";
	for(int i = 0; i < molecules.size(); i++) {
		expectvol += molecules[i].expectvol;
	}
	if(!isFitcell) {
		Spgroup spg = spacegroups[unit.spacegroup];
		int nops = spg.R.size();
		expectvol *= (double) nops;
	}

	if( ((vol/expectvol)-1.0) > (maxvol/100.0) )
			return true;
	return false;

}



bool GASP2struct::enforceCrystalType() {
	//find the crystal type from
	Spgroup spg = spacegroups[unit.spacegroup];

	//enforce angles and lengths
	if (spg.L == Lattice::Cubic) {
		//a = b = c; alpha = beta = gamma = 90;
		unit.a = unit.b = unit.c = 1.0;
		unit.alpha = unit.beta = unit.gamma = rad(90.0);

	} else if (spg.L == Lattice::Tetragonal) {
		//a = b; alpha = beta = gamma = 90;
		unit.a = unit.b = unit.ratA;
		unit.c = unit.ratC;
		unit.alpha = unit.beta = unit.gamma = rad(90.0);


	} else if (spg.L == Lattice::Orthorhombic) {
		//alpha = beta = gamma = 90;
		unit.a = unit.ratA;
		unit.b = unit.ratB;
		unit.c = unit.ratC;
		unit.alpha = unit.beta = unit.gamma = rad(90.0);


	} else if (spg.L == Lattice::Hexagonal) {
		//a = b; alpha = beta = 90; gamma = 120
		unit.a = unit.b = unit.ratA;
		unit.c = unit.ratC;
		unit.alpha = unit.beta = rad(90.0);
		unit.gamma = rad(120.0);

	} else if (spg.L == Lattice::Rhombohedral) {
		//a = b = c; alpha = beta = gamma;
		unit.a = unit.b = unit.c = 1.0;
		unit.alpha = unit.beta = unit.gamma = unit.rhomC;

	} else if (spg.L == Lattice::Monoclinic) {
		//alpha = gamma = 90;
		unit.a = unit.ratA;
		unit.b = unit.ratB;
		unit.c = unit.ratC;
		unit.beta = unit.monoB;
		unit.alpha = unit.gamma = rad(90.0);


	} else if (spg.L == Lattice::Triclinic) {
		//unconstrained
		unit.a = unit.ratA;
		unit.b = unit.ratB;
		unit.c = unit.ratC;
		unit.alpha = unit.triA;
		unit.beta = unit.triB;
		unit.gamma = unit.triC;
	}

	//test for bad sets
	if( ( unit.alpha + unit.beta + unit.gamma) >= 2*PI || ( unit.alpha + unit.beta + unit.gamma) <= 0)
		return false;
	if( (-unit.alpha + unit.beta + unit.gamma) >= 2*PI || (-unit.alpha + unit.beta + unit.gamma) <= 0)
		return false;
	if( ( unit.alpha - unit.beta + unit.gamma) >= 2*PI || ( unit.alpha - unit.beta + unit.gamma) <= 0)
		return false;
	if( ( unit.alpha + unit.beta - unit.gamma) >= 2*PI || ( unit.alpha + unit.beta - unit.gamma) <= 0)
		return false;

	return true;
}


//centers the molecule coordinates about the centroid
void GASP2struct::centerMol(GASP2molecule & mol) {
	Vec3 center = getMolCentroid(mol);
	double length;
	mol.extent = 0.0;
	for(int i = 0; i < mol.atoms.size(); i++) {
		mol.atoms[i].pos -= center;
		length = len(mol.atoms[i].pos);
		if(length > mol.extent)
			mol.extent = length;
	}
}

//center the molecule coordinates about the centroid, then translates to a new
//position
void GASP2struct::centerMol(GASP2molecule & mol, Vec3 pos) {
	Vec3 newpos =  pos - getMolCentroid(mol);
	for(int i = 0; i < mol.atoms.size(); i++) {
		mol.atoms[i].pos += newpos;
	}
}

//applies dihedral rotations
bool GASP2struct::applyDihedrals(GASP2molecule &mol) {
	Mat3 rot;
	Vec3 a,b,c,d, axis;
	vector<Index> moved, fixed;
	double init, diff, sign, dist;

	//for each molecule
	for(int i = 0; i < mol.dihedrals.size(); i++) {
		a = mol.atoms[mol.dihedrals[i].a].pos;
		b = mol.atoms[mol.dihedrals[i].b].pos;
		c = mol.atoms[mol.dihedrals[i].c].pos;
		d = mol.atoms[mol.dihedrals[i].d].pos;

		//find the change needed to correct
		init = dihedral(a,b,c,d);
//		diff = init - mol.dihedrals[i].ang;
		diff = init - mol.dihedrals[i].ang;

		//short circuit if the chage is small
		if(std::abs(diff) < 1.0)
			continue;

		//determine which half of the atoms is being modified
		//so that the sign of the rotation about the dihedral
		//axis is correct (and results are consistent with mercury)
		sign = 1.0;
		for(int j = 0; j < mol.dihedrals[i].update.size(); j++)
			if(mol.dihedrals[i].update[j] == mol.dihedrals[i].a)
				sign = -1.0;

		//generate the axis
		axis = norm(c-b);
		//generate the matrix
		rot = Rot3(axis, diff*sign);

		//cout << "rot: " << rot << endl;

		//perform the rotation
		for(int j = 0; j < mol.dihedrals[i].update.size(); j++) {
			mol.atoms[mol.dihedrals[i].update[j]].pos =
					rot*(mol.atoms[mol.dihedrals[i].update[j]].pos - b) + b;
		}

//		a = mol.atoms[mol.dihedrals[i].a].pos;
//		b = mol.atoms[mol.dihedrals[i].b].pos;
//		c = mol.atoms[mol.dihedrals[i].c].pos;
//		d = mol.atoms[mol.dihedrals[i].d].pos;
//		double final = dihedral(a,b,c,d);
//		cout << "Diff: " << deg(std::abs(diff)*sign) << " sign:" << sign << " setting: " << deg(mol.dihedrals[i].ang) << " init: " << deg(init) << " final: " << deg(final) << endl;


		//build the comparison lists
		moved.clear(); fixed.clear();
		for(int j = 0; j < mol.atoms.size(); j++) {
			//never include b or c in the lists, otherwise the test will fail
			if(j == mol.dihedrals[i].b || j == mol.dihedrals[i].c)
				continue;
			else {
				bool flag = false;
				for(int k = 0; k < mol.dihedrals[i].update.size(); k++) {
					if(mol.dihedrals[i].update[k] == j) {
						moved.push_back(j);
						flag = true;
						break;
					}
				}
				if(flag == false)
					fixed.push_back(j);
			}
		}


		//check for constraint violation
		for(int j = 0; j < moved.size(); j++) {
			for(int k = 0; k < fixed.size(); k++) {
				dist = len(mol.atoms[moved[j]].pos - mol.atoms[fixed[k]].pos);
				if(dist < (intradist + rcov(mol.atoms[moved[j]].type) +
						rcov(mol.atoms[fixed[k]].type) ) ) {
					//cout << "bad dih\n";
					return false;
				}
			}
		}

	}//for each molecule


	return true;
}


//applies molecules rotations
void GASP2struct::applyRot(GASP2molecule &mol) {

	centerMol(mol);
	Mat3 current = stabilize(getPlaneRot(mol));

	//zero the rotation;
	for(int i = 0; i < mol.atoms.size(); i++)
		mol.atoms[i].pos = inv(current)*mol.atoms[i].pos;

	//apply new rotation
	for(int i = 0; i < mol.atoms.size(); i++)
		mol.atoms[i].pos = mol.rot*mol.atoms[i].pos;

}


//applies symmetry operations
void GASP2struct::symmetrize(GASP2molecule &mol) {
	if(mol.symm == 0)
		return;

	Mat3 toFrac = cartToFrac(unit);
	Mat3 toCart = fracToCart(unit);;
	Vec3 temp;

	//rotate the molecule but ignore translation
	for(int i = 0; i < mol.atoms.size(); i++) {
		temp = toFrac*mol.atoms[i].pos;
		temp = mol.symmR * temp;
		mol.atoms[i].pos = toCart*temp;
	}
	//set the centroids
	mol.pos = modVec3(mol.symmR*mol.pos + mol.symmT);

	//recenter for sanity
	centerMol(mol);
}

//generates a supercell with arbitrary shell levels
void GASP2struct::makeSuperCell(vector<GASP2molecule> &supercell, int shells) {
	int nsize = supercell.size();
	GASP2molecule temp;
	for(int i = 0; i < shells; i++) {
		for(int j = 0; j < shells; j++) {
			for(int k = 0; k < shells; k++) {
				if(i == 0 && j == 0 && k ==0)
					continue;
				for(int n = 0; n < nsize; n++) {
					temp = supercell[n];
					temp.pos += Vec3( (double) i, (double) j, (double) k);
					supercell.push_back(temp);
				}
			}
		}
	}
}

//resets the cell axes and positions of molecules in a supercell
void GASP2struct::resetMols(double d, vector<GASP2molecule> &supercell, Vec3 ratios) {
	//cout << mark() << "resetmol: " <<ID.toStr()<< endl;
	//reset the cell params in unit
	unit.a = d * ratios[0];
	unit.b = d * ratios[1];
	unit.c = d * ratios[2];

	Mat3 toCart = fracToCart(unit);
	//Vec3 center;

	for(int i = 0; i < supercell.size(); i++) {
		supercell[i].center = toCart*supercell[i].pos;
		centerMol(supercell[i],supercell[i].center);
	}
	//cout << mark() << "resetmolout: " <<ID.toStr()<< endl;
}

//calculates a pseudoenergy using the exp-6 potential
double GASP2struct::calcPseudoE(vector<GASP2molecule> supercell, int shells, int molcount) {
	double e = 0.0;




	//find the center starting index
	int middle = shells/2;
	int center = molcount * middle * (1+ shells+shells*shells);
	//cout << "center: " << center << endl;

	double dist, theta, r, rmin, rd, coul,te;
	//cout << "start" << endl;
	//calculate the distances
	for(int i = 0; i < supercell.size(); i++) {
		for(int j = center; j < (center+molcount); j++) {
			//cout << "i,j: " << i << "," << j << endl;
			if(i == j) continue;
			//check to see if there is interaction
			dist = len(supercell[i].center-supercell[j].center);
			//cutoff of 10 angstroms
			if(dist > (100.0 + supercell[i].extent + supercell[j].extent) ) {
				//cout << "cont" << endl;
				continue;
			}

			for(int m = 0; m < supercell[i].atoms.size(); m++) {
				for(int n = 0; n < supercell[j].atoms.size(); n++) {

					//this is the exp-6 potential with couloumb pseudocomponent
					//two terms; the exponential term (repulsion),
					//and the attractive vdw term (r^6)
					//this form uses an alpha steepness of 1.0

					//distance rij between atoms
					r = len(supercell[i].atoms[m].pos - supercell[j].atoms[n].pos);
					rmin = (vdw(supercell[i].atoms[m].type) + vdw(supercell[j].atoms[n].type));
					if(r < (rmin*0.7 )) {
						//continue;
						return 0.0;
					}
//					if(r < 10.0) {
						//energy well minimum

					//energy well depth
					theta = ewell(supercell[i].atoms[m].type, supercell[j].atoms[n].type) / -5.0;
					// rij ^ 6 term
					rd = rmin/r;
					rd = rd * rd * rd;
					rd = rd * rd;

					//coulumb term
					//coul = ( 4.0 * supercell[i].atoms[m].charge * supercell[j].atoms[n].charge ) / r;

					//energy addition
					//te = (theta * ( 6.0*std::exp(1.0-(r/rmin)) - rd ) + coul);
					te = (theta * ( 6.0*std::exp(1.0-(r/rmin)) - rd ));

//					if(te < 0)
						//if( (r > rmin) && (te < -0.001) )
					e += te;

						//cout << r << "," << te << endl;

//					}

				}
			}
		}
	}

	//cout << "count: " << count << endl;
	e = e / static_cast<double>(spacegroups[unit.spacegroup].R.size());
	return e;
}

//determines the number of nearby contacts
void GASP2struct::calcContacts(vector<GASP2molecule> supercell, int shells, int molcount) {
	int count = 0;
	double dist;

	//find the center starting index
	int middle = shells/2;
	int center = molcount * middle * (1+ shells+shells*shells);
	//cout << "center: " << center << endl;



	//calculate the distances
	for(int i = 0; i < supercell.size(); i++) {
		for(int j = center; j < (center+molcount); j++) {
			//cout << "i,j: " << i << "," << j << endl;
			if(i == j) continue;
			//check to see if there is interaction
			dist = len(supercell[i].center-supercell[j].center);
			if(dist > (VDWLimit + contactdist + supercell[i].extent + supercell[j].extent) ) {
				//cout << "cont" << endl;
				continue;
			}

			for(int m = 0; m < supercell[i].atoms.size(); m++) {
				for(int n = 0; n < supercell[j].atoms.size(); n++) {
					dist = len(supercell[i].atoms[m].pos - supercell[j].atoms[n].pos);

					if(dist < (contactdist + vdw(supercell[i].atoms[m].type) + vdw(supercell[j].atoms[n].type)) ) {
					//if(dist < (contactdist + vdw2(supercell[i].atoms[m].type, supercell[j].atoms[n].type)) ) {
						//cout << "dist: " << dist << endl;
						//cout << mark() << "checkconout: " <<ID.toStr()<< endl;q
						count+=1;
					}
				}
			}
		}
	}

	//cout << "count: " << count << endl;

	contacts = count / spacegroups[unit.spacegroup].R.size();

}


//check connectivity of molecules to each other to make sure molecules are not touching
bool GASP2struct::checkConnect(vector<GASP2molecule> supercell) {

	double dist;
	//cout << mark() << "checkcon: " <<ID.toStr()<< endl;
	for(int i = 0; i < supercell.size(); i++) {
		for(int j = i; j < supercell.size(); j++) {
			if(i == j) continue;
			//check to see if there is interaction
			dist = len(supercell[i].center-supercell[j].center);
			if(dist > (VDWLimit + interdist + supercell[i].extent + supercell[j].extent) )
				continue;

			for(int m = 0; m < supercell[i].atoms.size(); m++) {
				for(int n = 0; n < supercell[j].atoms.size(); n++) {
					dist = len(supercell[i].atoms[m].pos - supercell[j].atoms[n].pos);

					if(dist < (interdist + vdw(supercell[i].atoms[m].type) + vdw(supercell[j].atoms[n].type)) ) {
					//if(dist < (interdist + vdw2(supercell[i].atoms[m].type, supercell[j].atoms[n].type)) ) {
						//cout << "dist: " << dist << endl;
						//cout << mark() << "checkconout: " <<ID.toStr()<< endl;
						return true;
					}
				}
			}
		}
	}
	//cout << mark() << "checkconout: " <<ID.toStr()<< endl;
	return false;
}

//adjusts cell axes to minimize volume while preventing contacts
double GASP2struct::collapseCell(vector<GASP2molecule> supercell, Vec3 ratios) {
	//cout << mark() << "collapse: " <<ID.toStr()<< endl;
	int steps;
	bool touches;
	bool lastVolOK=false, VolOK=false;
	double diff_d, last_d;
	//phase 1: double d until connects are okay or volume exceeded
	double d = 1.0;
	resetMols(d, supercell, ratios);
	touches = checkConnect(supercell);
	while (touches) {
		last_d = d;
		d *= 2.0;
		resetMols(d, supercell, ratios);
//		if(checkMaxVol()) {
//			return d;
//		}
		touches = checkConnect(supercell);
	}

	//optimization: check the volume now to see if it
	//passes volume constraints. if not, we reject it
//	if(!lastVolOK)
//		return -1.0;

	double maxd = d;
	//find the longest axis
	double maxaxis = 0;
	if(unit.a > maxaxis) maxaxis = unit.a;
	if(unit.b > maxaxis) maxaxis = unit.b;
	if(unit.c > maxaxis) maxaxis = unit.c;

	steps = maxaxis/0.1;
	diff_d = maxd/steps;
	d = 1.0;


	//find "connective minimum"
	while(steps > 0 ) {
		resetMols(d,supercell,ratios);
		if(checkConnect(supercell))
			d+= diff_d;
		else
			break;

		steps--;
	}

	pseudoenergy = calcPseudoE(supercell, 3, molecules.size());

//	double max_d;
//	max_d = d;
//	//d = 1.0;
//	//max_d = maxd;
//	d = max_d-(40.0*diff_d);
//
//
//	double pseudo;
//	double minpseudo = 9999.0;
//	double trueD = max_d;
//
////	ofstream tempfile("energy",ofstream::app);
////	ofstream ciffile("traj.cif",ofstream::app);
////	string temp;
//	steps = 0;
//	int cc;
//	while(d < max_d) {
//		resetMols(d,supercell,ratios);
//		//if(checkConnect(supercell))
//		pseudo = calcPseudoE(supercell, 3, molecules.size());
//		//calcContacts(supercell, 3, molecules.size());
//		if(pseudo < minpseudo) {
//			minpseudo = pseudo;
//			trueD = d;
//		}
////		tempfile << steps << "," << d*unit.ratA << "," << pseudo << endl;
////		cifString(temp,steps);
////		ciffile << temp << endl;
//		d += diff_d;
//		steps++;
//	}
//   pseudoenergy=minpseudo;
//	d = trueD;
//	resetMols(d,supercell,ratios);

//	tempfile.close();
//	ciffile.close();

//	if(steps == 0) {
//		cout << mark() << "Something bad happened with a collapse cell" << endl;
//	}

	//TODO: Increase linearly to determine true distance

	//phase 2: binary search decreasing d
//	steps = 0;
//	while (steps < 1000) {
//		diff_d = std::abs(d - last_d) / 2.0;
//		last_d = d;
//		if (diff_d < 0.0625 && touches==false)
//			break; //this is 0.125 (not 0.25) because diff / 2
//		resetMols(d, supercell, ratios);
//		touches = checkConnect(supercell);
//		if (touches)
//			d += diff_d;
//		else
//			d -= diff_d;
//		steps++;
//	}
//	if(steps >= 1000) {
//		cout << mark() << "Something bad happened with a collapse cell" << endl;
//	}
	//cout << mark() << "collapseout: " <<ID.toStr()<< endl;

	calcContacts(supercell, 3, molecules.size());

	return d;

}


//determine the axis order (sort of defunct)
Vec3 GASP2struct::getOrder(Vec3 rat) {
	Vec3 result;

	int large = 0;
	int small = 0;
	for (int i = 1; i < 3; i++) {
		if (rat[i] > rat[large])
			large = i;
		if (rat[i] < rat[small])
			small = i;
	}
	int mid;
	for (int i = 0; i < 3; i++)
		if (i != large && i != small)
			mid = i;

	result[0] = (double)large;
	result[1] = (double)mid;
	result[2] = (double)small;

	return result;
}

//adjust cell ratios (defunct function)
void GASP2struct::modCellRatio(Vec3 &ratios, Vec3 order, int n, double delta) {
	int axisswitch;
	Spgroup spg = spacegroups[unit.spacegroup];
	axisswitch = (int)order[n];
	ratios[axisswitch] += delta;

	//catch any bad ratios
	for (int i = 0; i < 3; i++)
		if (ratios[i] < 0.1)
			ratios[i] = 0.1;

	//catch for unit cell ratios where two or more axes
	//are constrained to each other by crystal type
	if (spg.L == Lattice::Tetragonal || spg.L == Lattice::Hexagonal) {
		//a = b
		if (axisswitch == 0) //a
			ratios[1] = ratios[0];
		else if (axisswitch == 1) //b
			ratios[0] = ratios[1];
	} else if (spg.L == Lattice::Cubic || spg.L == Lattice::Rhombohedral) {
		//a = b = c
		if (axisswitch == 0) //a
			ratios[2] = ratios[1] = ratios[0];
		else if (axisswitch == 1) //b
			ratios[2] = ratios[0] = ratios[1];
		else //c
			ratios[0] = ratios[1] = ratios[2];

	}

}

//this assumes that QE has done an optimization previously and
//that fitcell does not need to be performed
//instead, this fills in the symmetry for the system correctly
//so that structures can be restarted
bool GASP2struct::simplesymm() {
	//if(energy < 0.0) {
		unfitcell();

		//apply dihedrals and rotations to molecules
		for(int i = 0; i < molecules.size(); i++)
			applyRot(molecules[i]);

		//cout << mark() << "enter spacegroup" << endl;
		Spgroup spg = spacegroups[unit.spacegroup];
		//cout << mark() << "leave spacegroup" << endl;
		int nops = spg.R.size();

		//cout << mark() << "nops " << nops << endl;

		//symmetrize the molecules
		vector<GASP2molecule> symmcell;
		GASP2molecule temp;
		for(Index j = 0; j < nops; j++) {
			for(int i = 0; i < molecules.size(); i++) {
				temp = molecules[i];
				temp.symmR = spg.R[j];
				temp.symmT = spg.T[j];
				temp.symm = j;
				symmcell.push_back(temp);
			}
		}
		//cout << mark() << "t2" << endl;
		//symmetrize the molecules
		for(int i = 0; i < symmcell.size(); i++)
			symmetrize(symmcell[i]);
		molecules = symmcell;

		isFitcell = true;
	//}
	//else
		//isFitcell = false;
}

//fitcell applies all rotation and translation operations
//and then minimizes the volume of the unit cell
bool GASP2struct::fitcell(double tlimit) {
	//std::thread::id itit = std::this_thread::get_id();
	//cout << "pid: " << itit << endl;
	auto t = std::chrono::high_resolution_clock::now();

	unfitcell();

	if(enforceCrystalType() == false) {
		//cout << "BadCell" << endl;
		finalstate = FitcellBadCell;
		return false;
	}

	//apply dihedrals and rotations to molecules
	for(int i = 0; i < molecules.size(); i++) {
		if(applyDihedrals(molecules[i]) == false) {
			//cout << "BadDih" << endl;
			finalstate = FitcellBadDih;
			return false;
		}
		applyRot(molecules[i]);
	}

	//parse the relevent symmetry operations
	//cout << "Spacegroup: " << spacegroupNames[unit.spacegroup] << endl;

	//cout << mark() << "enter spacegroup" << endl;
	Spgroup spg = spacegroups[unit.spacegroup];
	//cout << mark() << "leave spacegroup" << endl;
	int nops = spg.R.size();

	//cout << mark() << "t1" << endl;

	//symmetrize the molecules
	vector<GASP2molecule> symmcell;
	GASP2molecule temp;
	for(Index j = 0; j < nops; j++) {
		for(int i = 0; i < molecules.size(); i++) {
			temp = molecules[i];
			temp.symmR = spg.R[j];
			temp.symmT = spg.T[j];
			temp.symm = j;
			symmcell.push_back(temp);
		}
	}
	//cout << mark() << "t2" << endl;
	//symmetrize the molecules
	for(int i = 0; i < symmcell.size(); i++)
		symmetrize(symmcell[i]);
	molecules = symmcell;

	Vec3 ratios, order;
	double vol, last_vol;

	//make the super cell and the optimize the cell lengths
	vector<GASP2molecule> supercell;
	supercell = symmcell;
	makeSuperCell(supercell, 3);

	//cout << "Supercell size: " << supercell.size()  << endl;

	//cout << mark() << "t3" <<" " <<ID.toStr()<< endl;

	ratios = Vec3(unit.ratA, unit.ratB, unit.ratC);
	//order = getOrder(axissettings[unit.axisorder]);
	//order = axissettings[unit.axisorder];

	collapseCell(supercell, ratios);

	//this actually belongs in collapseCell
	//calcContacts(supercell, 3, molecules.size());


	//AML NOTES:
	/*
	 * There is basically a problem with bias by introducing ratio collapsing.
	 * The reason is that, although it permits other shapes of unit cell,
	 * which is less biased than the originaly fitcell, it still favors
	 * unit cells which have a small initial dimension, meaning that the
	 * axis that is initially collapsed will almost always be the smallest
	 * dimension. This essentially means that every structure that is generated
	 * initially will suffer from this, and that unit cells that have more or
	 * less even ratios will only very rarely appear in the population.
	 *
	 * So, instead, what needs to happen is that the unit cell is collapsed
	 * ONCE. and whatever the unit cell shape is, is what fitcell produces.
	 * This guarantees that there are no biases resulting from adjusting unit
	 * cells. The pitfall is that much more time needs to be spent to generate
	 * unit cells that are valid for an initial population.
	 *
	 * As a sidenote, the advantage is that fitcell will take much less time to
	 * complete in this context. Axisorder can also be technically ditched in the
	 * schema.
	 *
	 *
	 */

//	last_vol = getVolume();
//	vol = last_vol;
//
//	//need to parameterize this later
//	auto start = std::chrono::steady_clock::now();
//
//	std:chrono::duration<double> diff;
//
//	for(int n = 0; n < 3; n++) {
//		while (true) {
//			//cout << mark() << "tn1:"<< n <<" " <<ID.toStr()<< endl;
//			modCellRatio(ratios, order, n, -0.1f);
//			//cout << mark() << "tn2:"<< n <<" " <<ID.toStr()<< endl;
//			collapseCell(supercell, ratios);
//			//cout << mark() << "tn3:"<< n <<" " <<ID.toStr()<< endl;
//			vol = getVolume();
//			if(vol >= last_vol) {
//				modCellRatio(ratios, order, n, 0.1f);
//				collapseCell(supercell, ratios);
//				break;
//			}
//			else
//				last_vol = vol;
//			//protection against long running
//			auto end = std::chrono::steady_clock::now();
//			diff = end-start;
//			if(diff.count() > tlimit) {
//				cout << mark() << "Fitcell took too long on structure " << ID.toStr();
//				cout << ", structure may be valid but was not evaluated" << endl;
//				finalstate = NoFitcell;
//				return false;
//			}
//		}
//		//this->cifOut("fitcelldebug.cif");
//	}
//	//cout << mark() << "t4" <<" " <<ID.toStr()<< endl;
//	//final collapse, and explicit data clear
//	collapseCell(supercell, ratios);
	//molecules = supercell;
	//supercell.clear();
	//symmcell.clear();

	isFitcell = true;

	//cout << "finished fitcell!" << endl;
//	auto t2 = std::chrono::high_resolution_clock::now();
	//cout << "zzz, " << unit.spacegroup << ", " << nops << ", " << chrono::duration_cast<chrono::milliseconds>(t2-t).count() << endl;
	//cout << "unit.a: " << unit.a << endl;
	return true;
}


//undo the fitcell by removing symmetrically equivalent molecules
bool GASP2struct::unfitcell() {

	vector<GASP2molecule> tempmol;
	for(int i = 0; i < molecules.size(); i++) {
		if(molecules[i].symm == 0)
			tempmol.push_back(molecules[i]);
	}
	if(tempmol.size() == 0) {
		cout << "Something went wrong during the unfitcell; no identity-symm molecules were found...\n";
		return false;
	}
	molecules.clear();
	molecules = tempmol;
	isFitcell = false;


	return true;
}

//input helper to read a CIF format structure
bool GASP2struct::readCifMol(string name, string outname, string plane) {

	ifstream input;
	input.open(name);


	GASP2molecule tempmol;
	GASP2atom tempatom;

	int pos;
	string line;
	vector<string> fields;

	while(!input.eof()) {
		getline(input, line);

		pos = line.find("_cell_length_a");
		if(pos!=string::npos) {
			fields = split(line,' ');
			unit.a = std::stod(fields[1]);
		}

		pos = line.find("_cell_length_b");
		if(pos!=string::npos) {
			fields = split(line,' ');
			unit.b = std::stod(fields[1]);
		}

		pos = line.find("_cell_length_c");
		if(pos!=string::npos) {
			fields = split(line,' ');
			unit.c = std::stod(fields[1]);
		}

		pos = line.find("_cell_angle_alpha");
		if(pos!=string::npos) {
			fields = split(line,' ');
			unit.alpha = rad(std::stod(fields[1]));
		}

		pos = line.find("_cell_angle_beta");
		if(pos!=string::npos) {
			fields = split(line,' ');
			unit.beta = rad(std::stod(fields[1]));
		}

		pos = line.find("_cell_angle_gamma");
		if(pos!=string::npos) {
			fields = split(line,' ');
			unit.gamma = rad(std::stod(fields[1]));
		}

		pos = line.find("_symmetry_Int_Tables_number");
		if(pos!=string::npos) {
			fields = split(line,' ');
			unit.spacegroup = std::stoi(fields[1]);
		}

		//atom loop
		pos = line.find("_atom_site_fract_z");
		if(pos!=string::npos) {

			Mat3 toCart = fracToCart(unit);

			getline(input,line);
			fields = split(line,' ');
			while(!input.eof() && fields.size() == 5) {

				tempatom.label = newName(fields[0]);
				tempatom.type = getElemType(fields[1]);
				tempatom.pos[0] = std::stod(fields[2]);
				tempatom.pos[1] = std::stod(fields[3]);
				tempatom.pos[2] = std::stod(fields[4]);
				tempatom.pos = toCart*tempatom.pos;
				tempmol.atoms.push_back(tempatom);
				//cout << names[tempatom.label] << "," << getElemName(tempatom.type) << "," << tempatom.pos << endl;

				getline(input,line);
				fields = split(line,' ');
			}
		}

		//only read the first .cif
		pos = line.find("#END");
		if(pos!=string::npos) {
			break;
		}


	}

	input.close();
	//cout << "close done" << endl;

	tempmol.label = newName("MOL");

	if(plane.length() > 0) {
		vector<string> vstemp = split(plane,',');
		int size = vstemp.size();
		if(size != 3) {
			cout << "The plane specification for "+names[tempmol.label]+" does not have 3 elements!\n";
			return false;
		}
		tempmol.p1 = atomLookup(newName(vstemp[0]), tempmol);
		//cout << "atom" << endl;
		tempmol.p2 = atomLookup(newName(vstemp[1]), tempmol);
		tempmol.p3 = atomLookup(newName(vstemp[2]), tempmol);
		if(tempmol.p1 < 0 || tempmol.p2 < 0 || tempmol.p3 < 0) {
			cout << "An atom designation in plane for "+names[tempmol.label]+" did not match any atom in the molecule!\n";
			return false;
		}
	}

	//cout << tempmol.p1 << "," << tempmol.p2 << "," << tempmol.p3 << "," << endl;

	molecules.push_back(tempmol);
	//cout << "push" << endl;

	if(plane.length() > 0) check();

	//cout << "check" << endl;

	ofstream outf;
	outf.open(outname.c_str(), ofstream::out);
	if(outf.fail()) {
		cout << "opening the output file failed!" << endl;
		return false;
	}

	outf << serializeXML();

	//cout << "serialize" << endl;

	outf.close();

}

//performs structure checks to make sure everything is still connected properly
bool GASP2struct::check() {
	Index a,b,c,d;
	double val;

	Mat3 toFrac = cartToFrac(unit);

	for(int i = 0; i < molecules.size(); i++) {
		//bonds
		for(int j = 0; j < molecules[i].bonds.size(); j++) {
			a = molecules[i].bonds[j].a;
			b = molecules[i].bonds[j].b;
			val = len(molecules[i].atoms[a].pos - molecules[i].atoms[b].pos);
			if(val > molecules[i].bonds[j].maxLen || val < molecules[i].bonds[j].minLen)
				finalstate = OptBadBond;
			molecules[i].bonds[j].len = val;
		}
		//angles
		for(int j = 0; j < molecules[i].angles.size(); j++) {
			a = molecules[i].angles[j].a;
			b = molecules[i].angles[j].b;
			c = molecules[i].angles[j].c;
			val = angle(molecules[i].atoms[a].pos,molecules[i].atoms[b].pos,molecules[i].atoms[c].pos);
			if(val > molecules[i].angles[j].maxAng || val < molecules[i].angles[j].minAng)
				finalstate = OptBadAng;
			molecules[i].angles[j].ang = val;
		}
		//dihedrals
		for(int j = 0; j < molecules[i].dihedrals.size(); j++) {
			a = molecules[i].dihedrals[j].a;
			b = molecules[i].dihedrals[j].b;
			c = molecules[i].dihedrals[j].c;
			d = molecules[i].dihedrals[j].d;
			val = dihedral(molecules[i].atoms[a].pos,molecules[i].atoms[b].pos,
					molecules[i].atoms[c].pos,molecules[i].atoms[d].pos);
			if(val > molecules[i].dihedrals[j].maxAng || val < molecules[i].dihedrals[j].minAng)
				finalstate = OptBadDih;
			molecules[i].dihedrals[j].ang = val;
		}


		//plane rotations
		molecules[i].rot = getPlaneRot(molecules[i]);
		//cout << "rot" << endl;

		//position and recenter
		molecules[i].pos = toFrac*getMolCentroid(molecules[i]);
		centerMol(molecules[i]);
		//cout << "center" << endl;
	}

	//recalc pseudoenergy
	vector<GASP2molecule> supercell;
	supercell = molecules;
	makeSuperCell(supercell, 3);
	pseudoenergy = calcPseudoE(supercell, 3, molecules.size());


	return true;
}

//initializes a structure genome
bool GASP2struct::init(Spacemode mode, Index spcg) {
	//set UUIDs appropriately
	ID.generate();
	parentA.clear();
	parentB.clear();

	unfitcell();
	isFitcell = false;
	complete = false;
	finalstate = OKStruct;
	time = 0.0;
	energy = 0.0;
	pressure = 0.0;
	force = 0.0;
	time = 0;
	steps = 0;
	unit.spacegroup = 1;

	//randomize spacegroup
	//one, two x2, three, four, six,
	discrete_distribution<> daxis({1,2,1,1,1});

	uniform_real_distribution<double> dcent(0.0,1.0);
	uniform_real_distribution<double> dsub(0.0,1.0);

	switch(daxis(rgen)) {
	case 0:	unit.axn = Axisnum::One; break;
	case 1:	unit.axn = Axisnum::Two; break;
	case 2:	unit.axn = Axisnum::Three; break;
	case 3:	unit.axn = Axisnum::Four; break;
	case 4:	unit.axn = Axisnum::Six; break;
	default:
		cout << "An error was encountered while setting the axis number!\n";
		return false;
	}

	if(mode == Limited) {
		//Cn, Cnv, Cnh x2, Sn x2, Dn, Dnd, Dnh
		discrete_distribution<> dschoen({1,1,2,2,1,1,1});
		switch(dschoen(rgen)) {
		case 0: unit.typ = Schoenflies::Cn; break;
		case 1: unit.typ = Schoenflies::Cnv; break;
		case 2: unit.typ = Schoenflies::Cnh; break;
		case 3: unit.typ = Schoenflies::Sn; break;
		case 4: unit.typ = Schoenflies::Dn; break;
		case 5: unit.typ = Schoenflies::Dnd; break;
		case 6: unit.typ = Schoenflies::Dnh; break;
		default:
			cout << "An error was encountered while setting the Schoenflies type!\n";
			return false;
		}
	} else if (mode == Full) {
		//Cn, Cnv, Cnh x2, Sn x2, Dn, Dnd, Dnh + T, Th, O, Td, Oh
		discrete_distribution<> dschoenextend({2,2,4,4,2,2,2,1,1,1,1,1});
		switch(dschoenextend(rgen)) {
		case 0: unit.typ = Schoenflies::Cn; break;
		case 1: unit.typ = Schoenflies::Cnv; break;
		case 2: unit.typ = Schoenflies::Cnh; break;
		case 3: unit.typ = Schoenflies::Sn; break;
		case 4: unit.typ = Schoenflies::Dn; break;
		case 5: unit.typ = Schoenflies::Dnd; break;
		case 6: unit.typ = Schoenflies::Dnh; break;
		case 7: unit.typ = Schoenflies::T; break;
		case 8: unit.typ = Schoenflies::Th; break;
		case 9: unit.typ = Schoenflies::O; break;
		case 10: unit.typ = Schoenflies::Td; break;
		case 11: unit.typ = Schoenflies::Oh; break;
		default:
			cout << "An error was encountered while setting the Schoenflies type!\n";
			return false;
		}
	}
	unit.cen = dcent(rgen);
	unit.sub = dsub(rgen);

	//cout << "spacegroup: " << getSchoenflies(unit.typ)<<" "<<getAxis(unit.axn)<<" "<<unit.cen<<" "<<unit.sub<<endl;

	if(mode == Single)
		unit.spacegroup = spcg;
	else {
		if(!setSpacegroup()) //D4d or D6d was generated
			return false;
	}


	//randomize ratios & special cell angles
	uniform_real_distribution<double> drat(0.5,4.0);
	unit.ratA = drat(rgen);
	unit.ratB = drat(rgen);
	unit.ratC = drat(rgen);
	uniform_int_distribution<> axisset(0,5);
	unit.axisorder=axisset(rgen);
// the original ratios were kind of poorly picked
//we realyl want to restrict these values to avoid long,
//thin unit cells

//	uniform_real_distribution<double> dtrimono(0.01,180.0);
//	uniform_real_distribution<double> drhom(0.01,120.0);
	uniform_real_distribution<double> dtrimono(60.0,120.0);
	uniform_real_distribution<double> drhom(60.0,89.0);
	unit.monoB = rad(dtrimono(rgen));
	unit.triA = rad(dtrimono(rgen));
	unit.triB = rad(dtrimono(rgen));
	unit.triC = rad(dtrimono(rgen));
	unit.rhomC = rad(drhom(rgen));


	//cout << "ratios: " <<unit.ratA<<" "<<unit.ratB<<" "<<unit.ratC<<endl;
	//set stoichiometry
	for(int i = 0; i < unit.stoich.size(); i++) {
		if(unit.stoich[i].min < unit.stoich[i].max) {
			uniform_int_distribution<> dstoich(unit.stoich[i].min, unit.stoich[i].max);
			unit.stoich[i].count = dstoich(rgen);
		}
		else
			unit.stoich[i].count = unit.stoich[i].min;

		//cout << "stoich["<<i<<"]: "<<unit.stoich[i].count<<endl;
	}
	vector<GASP2molecule> tempmols;
	tempmols.clear();
	for(int i = 0; i < unit.stoich.size(); i++) {
		GASP2molecule mol = molecules[molLookup(unit.stoich[i].mol)];
		for(int n = 0; n < unit.stoich[i].count; n++) {
			tempmols.push_back(mol);
		}
	}

	molecules.clear();
	molecules = tempmols;


	//for each molecule
	uniform_real_distribution<double> dpos(0.0,1.0);
	uniform_real_distribution<double> drot(-1.0,1.0);
	uniform_real_distribution<double> dtheta(0.0, 2*PI);
	for(int i = 0; i < molecules.size(); i++) {
		Vec3 tempvec; double temprot;
		//randomize rotation matrix
		tempvec = norm(Vec3(drot(rgen),drot(rgen),drot(rgen)));
		temprot = dtheta(rgen);
		//cout << "Rot["<<i<<"]: "<<tempvec<<" "<<temprot<<endl;
		molecules[i].rot = Rot3(tempvec, temprot);
		//randomize position
		tempvec = Vec3(dpos(rgen),dpos(rgen),dpos(rgen));
		//cout << "Pos["<<i<<"]: "<<tempvec<<endl;
		molecules[i].pos = tempvec;
		//randomize dihedrals
		for(int j = 0; j < molecules[i].dihedrals.size(); j++) {
			uniform_real_distribution<double> ddihed(
					molecules[i].dihedrals[j].minAng,
					molecules[i].dihedrals[j].maxAng);
			molecules[i].dihedrals[j].ang = ddihed(rgen);
			//cout <<"dihedral["<<i<<","<<j<<"]:"<<molecules[i].dihedrals[j].ang<<endl;
		}
	}


	return true;
}


//mutates a structure genome
bool GASP2struct::mutateStruct(double rate, Spacemode mode) {

	Index spcg = unit.spacegroup;

	//set UUIDs appropriately
	parentA = ID;
	ID.generate();
	parentB.clear();
	energy = 0.0;

	unfitcell();
	complete = false;
	finalstate = OKStruct;

	//rate of mutation
	bernoulli_distribution mut(rate);

	//randomize spacegroup
	//one, two x4, three, four, six,
	discrete_distribution<> daxis({1,4,1,1,1});
	uniform_real_distribution<double> dcent(0.0,1.0);
	uniform_real_distribution<double> dsub(0.0,1.0);

	if(mut(rgen)) {
		switch(daxis(rgen)) {
		case 0:	unit.axn = Axisnum::One; break;
		case 1:	unit.axn = Axisnum::Two; break;
		case 2:	unit.axn = Axisnum::Three; break;
		case 3:	unit.axn = Axisnum::Four; break;
		case 4:	unit.axn = Axisnum::Six; break;
		default:
			cout << "An error was encountered while setting the axis number!\n";
			return false;
		}
	}

	if(mut(rgen)) {
		if(mode == Limited) {
			//Cn, Cnv, Cnh x2, Sn x2, Dn, Dnd, Dnh
			discrete_distribution<> dschoen({1,1,2,2,1,1,1});
			switch(dschoen(rgen)) {
			case 0: unit.typ = Schoenflies::Cn; break;
			case 1: unit.typ = Schoenflies::Cnv; break;
			case 2: unit.typ = Schoenflies::Cnh; break;
			case 3: unit.typ = Schoenflies::Sn; break;
			case 4: unit.typ = Schoenflies::Dn; break;
			case 5: unit.typ = Schoenflies::Dnd; break;
			case 6: unit.typ = Schoenflies::Dnh; break;
			default:
				cout << "An error was encountered while setting the Schoenflies type!\n";
				return false;
			}
		} else if (mode == Full) {
			//Cn, Cnv, Cnh x2, Sn x2, Dn, Dnd, Dnh + T, Th, O, Td, Oh
			discrete_distribution<> dschoenextend({2,2,4,4,2,2,2,1,1,1,1,1});
			switch(dschoenextend(rgen)) {
			case 0: unit.typ = Schoenflies::Cn; break;
			case 1: unit.typ = Schoenflies::Cnv; break;
			case 2: unit.typ = Schoenflies::Cnh; break;
			case 3: unit.typ = Schoenflies::Sn; break;
			case 4: unit.typ = Schoenflies::Dn; break;
			case 5: unit.typ = Schoenflies::Dnd; break;
			case 6: unit.typ = Schoenflies::Dnh; break;
			case 7: unit.typ = Schoenflies::T; break;
			case 8: unit.typ = Schoenflies::Th; break;
			case 9: unit.typ = Schoenflies::O; break;
			case 10: unit.typ = Schoenflies::Td; break;
			case 11: unit.typ = Schoenflies::Oh; break;
			default:
				cout << "An error was encountered while setting the Schoenflies type!\n";
				return false;
			}
		}
	}

	if(mut(rgen))
		unit.cen = dcent(rgen);
	if(mut(rgen))
		unit.sub = dsub(rgen);


	if(mode == Single)
		unit.spacegroup = spcg;
	else {
		if(!setSpacegroup()) //D4d or D6d was generated
			return false;
	}


	//randomize ratios & special cell angles
	uniform_real_distribution<double> drat(0.5,4.0);
	uniform_int_distribution<> axisset(0,5);
	if(mut(rgen))
		unit.ratA = drat(rgen);
	if(mut(rgen))
		unit.ratB = drat(rgen);
	if(mut(rgen))
		unit.ratC = drat(rgen);
	if(mut(rgen))
		unit.axisorder=axisset(rgen);
	uniform_real_distribution<double> dtrimono(60.0,120.0);
	uniform_real_distribution<double> drhom(60.0,89.0);
	if(mut(rgen))
		unit.monoB = rad(dtrimono(rgen));
	if(mut(rgen))
		unit.triA = rad(dtrimono(rgen));
	if(mut(rgen))
		unit.triB = rad(dtrimono(rgen));
	if(mut(rgen))
		unit.triC = rad(dtrimono(rgen));
	if(mut(rgen))
		unit.rhomC = rad(drhom(rgen));


	//set stoichiometry
	bool stoichchange = false;
	for(int i = 0; i < unit.stoich.size(); i++) {
		if(unit.stoich[i].min < unit.stoich[i].max) {
			if(mut(rgen)) {
				uniform_int_distribution<> dstoich(unit.stoich[i].min, unit.stoich[i].max);
				unit.stoich[i].count = dstoich(rgen);
				stoichchange = true;
			}
		}
		else
			unit.stoich[i].count = unit.stoich[i].min;

	}

	//for each molecule
	uniform_real_distribution<double> dpos(0.0,1.0);
	uniform_real_distribution<double> drot(-1.0,1.0);
	uniform_real_distribution<double> dtheta(0.0, 2.0*PI);


	//if the stoichiometry changed, we completely
	//reinitialize the set of molecules
	if(stoichchange) {
		vector<GASP2molecule> tempmols;
		tempmols.clear();
		for(int i = 0; i < unit.stoich.size(); i++) {
			GASP2molecule mol = molecules[molLookup(unit.stoich[i].mol)];
			for(int n = 0; n < unit.stoich[i].count; n++) {
				tempmols.push_back(mol);
			}
		}

		molecules.clear();
		molecules = tempmols;
		for(int i = 0; i < molecules.size(); i++) {
			Vec3 tempvec; double temprot;
			//randomize rotation matrix
			tempvec = norm(Vec3(drot(rgen),drot(rgen),drot(rgen)));
			temprot = dtheta(rgen);
			molecules[i].rot = Rot3(tempvec, temprot);
			//randomize position
			tempvec = Vec3(dpos(rgen),dpos(rgen),dpos(rgen));
			molecules[i].pos = tempvec;
			//randomize dihedrals
			for(int j = 0; j < molecules[i].dihedrals.size(); j++) {
				uniform_real_distribution<double> ddihed(
						molecules[i].dihedrals[j].minAng,
						molecules[i].dihedrals[j].maxAng);
				molecules[i].dihedrals[j].ang = ddihed(rgen);
			}
		}

	}//if(stoichchange)
	//if the stoichiometry stayed the same, we individually tweak the molecules
	else {
		for(int i = 0; i < molecules.size(); i++) {
			Vec3 tempvec; double temprot;
			//randomize rotation matrix
			if(mut(rgen)) {
				tempvec = norm(Vec3(drot(rgen),drot(rgen),drot(rgen)));
				temprot = dtheta(rgen);
				molecules[i].rot = Rot3(tempvec, temprot);
			}
			//randomize position
			if(mut(rgen)) {
				tempvec = Vec3(dpos(rgen),dpos(rgen),dpos(rgen));
				molecules[i].pos = tempvec;
			}
			//randomize dihedrals
			for(int j = 0; j < molecules[i].dihedrals.size(); j++) {
				if(mut(rgen)) {
					uniform_real_distribution<double> ddihed(
							molecules[i].dihedrals[j].minAng,
							molecules[i].dihedrals[j].maxAng);
					molecules[i].dihedrals[j].ang = ddihed(rgen);
				}
			}
		}
	} //else(stoichchange)

	return true;
}

//cross two structures
void GASP2struct::crossStruct(GASP2struct partner, GASP2struct &childA, GASP2struct &childB, double rate,  Spacemode mode) {
	this->unfitcell();
	partner.unfitcell();

	childA = *this;
	childB = partner;
	//set IDs
	childA.parentA = this->ID;
	childA.parentB = partner.ID;
	childB.parentA = this->ID;
	childB.parentB = partner.ID;
	childA.ID.generate();
	childB.ID.generate();
	//set some important values and unfitcell
	childA.unfitcell();
	childB.unfitcell();
	childA.complete = childB.complete = false;
	childA.didOpt = childB.didOpt = false;
	childA.finalstate = childB.finalstate = OKStruct;
	childA.time = childB.time = 0;
	childA.steps = childB.steps = 0;
	childA.energy = 0.0;
	childB.energy = 0.0;
	childA.pseudoenergy = 0.0;
	childB.pseudoenergy = 0.0;

	//perform the crossing
	//it is implied by De Jong et al. that
	//the uniform crossover provides good sampling
	//coverage, and that the balance between
	//exploration of the search space can be reconciled
	//with exploitation of subspaces by adjusting the
	//probabilty that two bits are crossed.
	//since the search space is not necessarily binary
	//and has many paramters this seems like a good approach.
	//a starting value of 0.5 crossover rate seems ideal.
	//tuning so that the value decreases may be worthwhile.

	//the rationale for picking 0.5 is that the GA coupled with
	//elitism will lead to a converged population quickly. so,
	//within the hypersurface represented by the best N structures
	//there is no reason to not to perform a maximal search.
	//in theory this plays nice with the spacegroup
	//encoding because convergence on a few spacegroups
	//should occur quickly

	bernoulli_distribution crx(rate);

	//unit cell stuff
	if(crx(rgen))
		swap(childA.unit.typ, childB.unit.typ);
	if(crx(rgen))
		swap(childA.unit.axn, childB.unit.axn);
	if(crx(rgen))
		swap(childA.unit.cen, childB.unit.cen);
	if(crx(rgen))
		swap(childA.unit.sub, childB.unit.sub);
	if(crx(rgen))
		swap(childA.unit.triA, childB.unit.triA);
	if(crx(rgen))
		swap(childA.unit.triB, childB.unit.triB);
	if(crx(rgen))
		swap(childA.unit.triC, childB.unit.triC);
	if(crx(rgen))
		swap(childA.unit.monoB, childB.unit.monoB);
	if(crx(rgen))
		swap(childA.unit.rhomC, childB.unit.rhomC);
	if(crx(rgen))
		swap(childA.unit.ratA, childB.unit.ratA);
	if(crx(rgen))
		swap(childA.unit.ratB, childB.unit.ratB);
	if(crx(rgen))
		swap(childA.unit.ratC, childB.unit.ratC);
	if(crx(rgen))
		swap(childA.unit.axisorder, childB.unit.axisorder);


	vector<GASP2molecule> temp, a, b;
	NIndex m;
	//this crossing algorithm can lead to self-crossing
	//when different number of molecules are presented
	//this isn't a problem, just something to be aware of
	for(int i = 0; i < childA.unit.stoich.size(); i++) {
		temp.clear();
		m = childA.unit.stoich[i].mol;
		//collect all the molecules of this type
		for(int j = 0; j < childA.molecules.size(); j++)
			if(childA.molecules[j].label == m)
				temp.push_back(childA.molecules[j]);
		for(int j = 0; j < childB.molecules.size(); j++)
			if(childB.molecules[j].label == m)
				temp.push_back(childB.molecules[j]);

		//perform the crossover
		int end = temp.size() - 1;
		for(int j = 0; j < temp.size() / 2; j++) {
			if(crx(rgen))
				swap(temp[j].pos[0],temp[end-j].pos[0]);
			if(crx(rgen))
				swap(temp[j].pos[1],temp[end-j].pos[1]);
			if(crx(rgen))
				swap(temp[j].pos[2],temp[end-j].pos[2]);
			//the rotation matrix is not simply
			//swapped; to properly search the space
			//multiplication is required
			//since the parents are theoretically
			//preserved, there is no loss of good genes
			//if there is a candidate for changing
			//the rgen value, this is probably it
			if(crx(rgen)) {
			//an oversight: part of the time, instead of multiplying, we should just swap
				Mat3 t = temp[end-j].rot;
				temp[end-j].rot = temp[j].rot;
				temp[j].rot = t;
			}
			//otherwise, we should consider the multiplication
			//this favors swapping of good matrices, but doesn't end the world
			//if we multiply matrices, which allows for searching the rotation space
			else {
				if(crx(rgen)) {
					Mat3 t = temp[j].rot*temp[end-j].rot;
					Mat3 n = temp[end-j].rot*temp[j].rot;
					temp[j].rot = t;
					temp[end-j].rot = n;
				}
			}

			for(int k = 0; k < temp[0].dihedrals.size(); k++) {
				if(crx(rgen))
					swap(temp[j].dihedrals[k].ang, temp[end-j].dihedrals[k].ang);
			}
		}

		//assign molecules to each stucture

		//1) decide if the molecule counts will be swapped
		if(crx(rgen))
			swap(childA.unit.stoich[i].count, childB.unit.stoich[i].count);

		//decide if the molecules will be swapped
		//if they are swapped, we reverse the order of molecules
		//guaranteeing that when assigned they are placed correctly
		if(crx(rgen))
			std::reverse(temp.begin(),temp.end());

		//decide if the molecules will be mixed up
		if(crx(rgen))
			std::shuffle(temp.begin(), temp.end(), rgen);


		//assign molecules
		for(int j = 0; j < childA.unit.stoich[i].count; j++)
			a.push_back(temp[j]);
		for(int j = childA.unit.stoich[i].count; j < childB.unit.stoich[i].count+childA.unit.stoich[i].count; j++)
			b.push_back(temp[j]);

	}

	//dump the molecules into the new structures
	childA.molecules = a;
	childB.molecules = b;

	//set up the spacegroup stuff
	if(mode != Single) {
		if(!childA.setSpacegroup()) {
			childA.finalstate = FitcellBadCell;
			childA.complete = true;
		}
		if(!childB.setSpacegroup()) {
			childB.finalstate = FitcellBadCell;
			childB.complete = true;
		}
	}

}


//evaluate the structure by calling the externally assigned functor
bool GASP2struct::evaluate(string hostfile, GASP2param params) {

	if(!isFitcell) {
		cout << "was not fitcelled..." << endl;
		finalstate = NoFitcell;
		return false;
	}

	//never evaluate something that has already been evaluated
	if(!complete) {
		complete = eval(molecules, unit, energy, force, pressure, time, hostfile, params);
		steps++;
		cout << mark() << "evaluate energy: " << fixed << setw(6) <<  energy << endl;
		check();
		didOpt = true;
	}

	return complete;
};

//set the spacegroup from the spacegroup gene
bool GASP2struct::setSpacegroup(bool frExclude) {
	Index index;
	vector<cryGroup> temp;

	//check for invalid schoenflies
	if( (unit.typ == Schoenflies::Dnd) && (unit.axn == Axisnum::Four || unit.axn == Axisnum::Six) ) {
		//cout << "D4d or D6d was generated; structure " << ID.toStr() << " rejected.\n";
		return false;
	}

	//check for extended groups
	Axisnum a;
	switch(unit.typ) {
	case Schoenflies::T:
	case Schoenflies::Th:
	case Schoenflies::O:
	case Schoenflies::Td:
	case Schoenflies::Oh:
		a = Axisnum::UNK;
	default:
		a = unit.axn;
	}

	//get a sublist of groups
	for(int i = 0; i < groupgenes.size(); i++) {
		if( (a == groupgenes[i].a) && (unit.typ == groupgenes[i].s) )
			if(frExclude) {
				if(groupgenes[i].c != Centering::F && groupgenes[i].c != Centering::R)
					temp.push_back(groupgenes[i]);
			}
			else
				temp.push_back(groupgenes[i]);
	}



	//select centering group
	int c = indexSelect(unit.cen, temp.size());
	//set spacegroup from list
	unit.spacegroup = temp[c].indices[indexSelect(unit.sub, temp[c].indices.size())];
	//cout << "Spacegroup: " << unit.spacegroup + 1 << endl;
	return true;
}

//this functions randomizes structures


//searches for a name in the namelist
//if the name is not found, the name is added.
NIndex GASP2struct::newName(string name) {
	NIndex n = names.size();
	for(int i = 0; i < n; i++) {
		if(name == names[i])
			return i;
	}
	names.push_back(name);
	return n;
}

NIndex GASP2struct::newName(const char * name) {
	string temp = name;
	return newName(temp);
}

//searches for a name index that matches the given index
//in the atoms of a molecule. returns -1 if not found.
Index GASP2struct::atomLookup(NIndex nameInd, GASP2molecule mol) {
	for(int i = 0; i < mol.atoms.size(); i++) {
		if(nameInd == mol.atoms[i].label)
			return i;
	}
	return -1;
}

//returns the index of the first molecule matching the name
Index GASP2struct::molLookup(NIndex nameInd) {
	for(int i = 0; i < molecules.size(); i++) {
		if(nameInd == molecules[i].label)
			return i;
	}
	return -1;
}

//output a serializedXML string for the structure
string GASP2struct::serializeXML() {
	tinyxml2::XMLPrinter pr(NULL);

	pr.OpenElement("crystal");
	pr.PushAttribute("interdist",interdist);
	pr.PushAttribute("intradist",intradist);
	pr.PushAttribute("contactdist",contactdist);
	pr.PushAttribute("maxvol",maxvol);
	pr.PushAttribute("minvol",minvol);
	pr.PushAttribute("name",names[crylabel].c_str());
	pr.PushAttribute("cluster",cluster);
	pr.PushAttribute("clustergroup",clustergroup);
	pr.OpenElement("info");
		pr.PushAttribute("id", ID.toStr().c_str());
		pr.PushAttribute("parentA",parentA.toStr().c_str());
		pr.PushAttribute("parentB",parentB.toStr().c_str());
		pr.PushAttribute("opt", tfconv(didOpt).c_str());
		pr.PushAttribute("fitcell",tfconv(isFitcell).c_str());
		pr.PushAttribute("complete",tfconv(complete).c_str());
		pr.PushAttribute("energy", energy);

		string strtemp = getStructError(finalstate);

		pr.PushAttribute("error", strtemp.c_str());
		pr.PushAttribute("time",(int) time);
		pr.PushAttribute("steps",steps);
		pr.PushAttribute("force",force);
		pr.PushAttribute("pressure",pressure);
		pr.PushAttribute("contacts",contacts);
		pr.PushAttribute("pseudoenergy",pseudoenergy);
	pr.CloseElement(); //info
	//cout << "info close" << endl;
	pr.OpenElement("cell");
		pr.PushAttribute("a",unit.a);
		pr.PushAttribute("b",unit.b);
		pr.PushAttribute("c",unit.c);
		pr.PushAttribute("al",deg(unit.alpha));
		pr.PushAttribute("bt",deg(unit.beta));
		pr.PushAttribute("gm",deg(unit.gamma));
		pr.PushAttribute("tA",deg(unit.triA));
		pr.PushAttribute("tB",deg(unit.triB));
		pr.PushAttribute("tC",deg(unit.triC));
		pr.PushAttribute("mB",deg(unit.monoB));
		pr.PushAttribute("rhmC",deg(unit.rhomC));
		pr.PushAttribute("rA",unit.ratA);
		pr.PushAttribute("rB",unit.ratB);
		pr.PushAttribute("rC",unit.ratC);
		pr.PushAttribute("axisorder",unit.axisorder);
		pr.PushAttribute("spacegroup",spacegroupNames[unit.spacegroup].c_str());
		pr.PushAttribute("cen",unit.cen);
		pr.PushAttribute("sub",unit.sub);
		pr.PushAttribute("typ", getSchoenflies(unit.typ).c_str());
		pr.PushAttribute("axn", getAxis(unit.axn).c_str());
		for(int i = 0; i < unit.stoich.size(); i++) {
			pr.OpenElement("stoichiometry");
				pr.PushAttribute("mol",names[unit.stoich[i].mol].c_str());
				pr.PushAttribute("min",unit.stoich[i].min);
				pr.PushAttribute("max",unit.stoich[i].max);
				pr.PushAttribute("count",unit.stoich[i].count);
			pr.CloseElement();//stoich
		}
	pr.CloseElement(); //cell

	//cout << "cellclose" << endl;

	for(int i = 0; i < molecules.size(); i++) {
		GASP2molecule mol = molecules[i];
		pr.OpenElement("molecule");
			string pln = "";

			pr.PushAttribute("name",names[mol.label].c_str());
			pln += (names[mol.atoms[mol.p1].label] +" ");
			pln += (names[mol.atoms[mol.p2].label] +" ");
			pln += (names[mol.atoms[mol.p3].label] +" ");
			pr.PushAttribute("plane",pln.c_str());
			pr.PushAttribute("plind",(int)mol.plindex);
			pr.PushAttribute("symm",(int)mol.symm);
			pr.PushAttribute("expectvol",mol.expectvol);
			//cout << "first" << endl;
			pln.clear();
			pr.OpenElement("rot");
			for(int j = 0; j < 9; j++)
				pln += (to_string(mol.rot[j/3][j%3]) + " ");
			pr.PushText(pln.c_str());
			pr.CloseElement(); //rot


			pln.clear();
			pr.OpenElement("pos");
			for(int j = 0; j < 3; j++)
				pln += (to_string(mol.pos[j]) + " ");
			pr.PushText(pln.c_str());
			pr.CloseElement(); //pos

			for(int j = 0; j < mol.atoms.size(); j++) {
			pr.OpenElement("atom");
				pr.PushAttribute("elem",getElemName(mol.atoms[j].type).c_str());
				pr.PushAttribute("title",names[mol.atoms[j].label].c_str());
				pr.PushAttribute("x",mol.atoms[j].pos[0]);
				pr.PushAttribute("y",mol.atoms[j].pos[1]);
				pr.PushAttribute("z",mol.atoms[j].pos[2]);
				pr.PushAttribute("c",mol.atoms[j].charge);
			pr.CloseElement(); //atom
			}

			for(int j = 0; j < mol.dihedrals.size(); j++) {
			pr.OpenElement("dihedral");
				pr.PushAttribute("title",names[mol.dihedrals[j].label].c_str());
				pln.clear();
				pln += (names[mol.atoms[mol.dihedrals[j].a].label] +" ");
				pln += (names[mol.atoms[mol.dihedrals[j].b].label] +" ");
				pln += (names[mol.atoms[mol.dihedrals[j].c].label] +" ");
				pln += (names[mol.atoms[mol.dihedrals[j].d].label] +" ");

				pr.PushAttribute("angle",pln.c_str());

				pln.clear();
				for(int k = 0; k < mol.dihedrals[j].update.size(); k++)
					pln += (names[mol.atoms[mol.dihedrals[j].update[k]].label] +" ");

				pr.PushAttribute("update",pln.c_str());
				pr.PushAttribute("min",deg(mol.dihedrals[j].minAng));
				pr.PushAttribute("max",deg(mol.dihedrals[j].maxAng));
				pr.PushAttribute("val",deg(mol.dihedrals[j].ang));

			pr.CloseElement(); //dih
			}
			//cout << "dih close" << endl;
			for(int j = 0; j < mol.bonds.size(); j++) {
			pr.OpenElement("bond");
				pr.PushAttribute("title",names[mol.bonds[j].label].c_str());
				pln.clear();
				pln += (names[mol.atoms[mol.bonds[j].a].label] +" ");
				pln += (names[mol.atoms[mol.bonds[j].b].label] +" ");
				pr.PushAttribute("atoms",pln.c_str());
				pr.PushAttribute("min",mol.bonds[j].minLen);
				pr.PushAttribute("max",mol.bonds[j].maxLen);
				pr.PushAttribute("val",mol.bonds[j].len);
			pr.CloseElement(); //bonds
			}

			for(int j = 0; j < mol.angles.size(); j++) {
			pr.OpenElement("angle");
				pr.PushAttribute("title",names[mol.angles[j].label].c_str());
				pln.clear();
				pln += (names[mol.atoms[mol.angles[j].a].label] +" ");
				pln += (names[mol.atoms[mol.angles[j].b].label] +" ");
				pln += (names[mol.atoms[mol.angles[j].c].label] +" ");
				pr.PushAttribute("angle",pln.c_str());
				pr.PushAttribute("min",deg(mol.angles[j].minAng));
				pr.PushAttribute("max",deg(mol.angles[j].maxAng));
				pr.PushAttribute("val",deg(mol.angles[j].ang));
			pr.CloseElement(); //angles
			}
		pr.CloseElement(); //molecule
	}
	pr.CloseElement(); //crystal

	//cout << "cryclose" << endl;
	//pr.PushAttribute("",);

	string out;
	out = pr.CStr();
	return out;
}


bool GASP2struct::parseXMLDoc(tinyxml2::XMLDocument *doc, string& errorstring ) {

	tinyxml2::XMLElement *crystal = doc->FirstChildElement("mgac")->FirstChildElement("crystal");
	return parseXMLStruct(crystal, errorstring);

}

bool GASP2struct::parseXMLStruct(tinyxml2::XMLElement * crystal, string & errorstring) {

	double dtemp;
	const char * stemp;
	string strtemp;
	int itemp;

	//parse the crystal
	//at this point we assume only one crystal in the specification
	//multiple spacegroups will likely be handled by a delimited list
	//shoudl that mode of operation ever be implemented

	if(!crystal) {
		errorstring = "No crystal structure specified!\n";
		return false;
	}
	else {
		//label
		if(! crystal->QueryIntAttribute("name",&itemp) ) {
			if(itemp > 0)
				crylabel = itemp;
		} else {
			stemp = crystal->Attribute("name");
			if(stemp)
				crylabel =  newName(stemp);
			else {
				errorstring = "A crystal name was not specified!\n";
				return false;
			}
		}


		//interdist
		if(!crystal->QueryDoubleAttribute("interdist", &dtemp)) {
			interdist = dtemp;
		}
		else {
			errorstring = "interdist is required but not specified!.\n";
			return false;
		}
//		if(interdist <= 0.0) {
//			errorstring = "interdist is out of bounds (interdist > 0.0).\n";
//			return false;
//		}

		//intradist
		if(!crystal->QueryDoubleAttribute("intradist", &dtemp)) {
			intradist = dtemp;
		}
		else {
			errorstring = "intradist is required but not specified!.\n";
			return false;
		}
		if(intradist <= 0.0) {
			errorstring = "intradist is out of bounds (intradist > 0.0).\n";
			return false;
		}

		//contactdist
		if(!crystal->QueryDoubleAttribute("contactdist", &dtemp)) {
			contactdist = dtemp;
		}
		else {
			errorstring = "contactdist is required but not specified!.\n";
			return false;
		}
		if(contactdist <= interdist) {
			errorstring = "contactdist must be greater than interdist!.\n";
			return false;
		}

		//maxvol
		if(!crystal->QueryDoubleAttribute("maxvol", &dtemp)) {
			maxvol = dtemp;
		}
		else {
			errorstring = "maxvol is required but not specified!.\n";
			return false;
		}

		//minvol
		if(!crystal->QueryDoubleAttribute("minvol", &dtemp)) {
			minvol = dtemp;
		}
		else {
			errorstring = "minvol is required but not specified!.\n";
			return false;
		}
		if(minvol > maxvol) {
			errorstring = "minvol is out of bounds (minvol > maxvol).\n";
			return false;
		}

		cluster=-1;
		if(! crystal->QueryIntAttribute("cluster",&itemp) ) {
			cluster = itemp;
		}

		clustergroup=-1;
		if(! crystal->QueryIntAttribute("clustergroup",&itemp) ) {
			clustergroup = itemp;
		}

		//parse the molecules
		GASP2molecule tempmol;
		tinyxml2::XMLElement *molecule, *item;
		molecule = crystal->FirstChildElement("molecule");
		if(!molecule) {
			errorstring = "No molecules specified in the structure!\n";
			return false;
		}
		while(molecule) {
			if(!readMol(molecule, errorstring, tempmol))
				return false;
			//push the molecule(s) onto the list
			this->molecules.push_back(tempmol);
			tempmol.clear();
			molecule = molecule->NextSiblingElement("molecule");
		}

		//parse the unitcell info (if present)
		//MUST HAPPEN AFTER MOLECULES ARE READ.
		GASP2cell tempcell;

		item = crystal->FirstChildElement("cell");
		if(item) {
			if(!readCell(item, errorstring, tempcell))
				return false;
			else
				unit = tempcell;
		}

		//parse the structure info (if present)

		item = crystal->FirstChildElement("info");
		if(item) {
			if(!readInfo(item, errorstring))
				return false;
		}
	}

	return true;
}

bool GASP2struct::readAtom(tinyxml2::XMLElement *elem, string& errorstring, GASP2atom &at) {

	const char * stemp;
	int itemp;
	string strtemp;
	double dtemp;


	if(elem) {
		//atom elem
		stemp = elem->Attribute("elem");
		if(stemp) {
			strtemp = stemp;
			at.type = getElemType(strtemp);
		}
		else {
			errorstring = "An atom element was not specified!\n";
			return false;
		}
		if(at.type == Elem::UNK) {
			errorstring = "An unknown or unsupported element name was used!\n";
			return false;
		}

		//atom label
		//first search for an index, then parse as string
		if(! elem->QueryIntAttribute("title",&itemp) ) {
			if(itemp > 0)
				at.label = itemp;
		} else {
			stemp = elem->Attribute("title");
			if(stemp)
				at.label =  newName(stemp);
			else {
				errorstring = "An atom title name was not specified!\n";
				return false;
			}
		}

		//x coord
		if(!elem->QueryDoubleAttribute("x", &dtemp)) {
			at.pos[0] = dtemp;
		}
		else {
			errorstring = "No value was given for an X coordinate!\n";
			return false;
		}
		//y coord
		if(!elem->QueryDoubleAttribute("y", &dtemp)) {
			at.pos[1] = dtemp;
		}
		else {
			errorstring = "No value was given for a Y coordinate!\n";
			return false;
		}
		//z coord
		if(!elem->QueryDoubleAttribute("z", &dtemp)) {
			at.pos[2] = dtemp;
		}
		else {
			errorstring = "No value was given for a Z coordinate!\n";
			return false;
		}

		at.charge = 0.0;
		if(!elem->QueryDoubleAttribute("c", &dtemp)) {
			at.charge = dtemp;
		}

	return true;
	}
	else {
		errorstring = "Bad atom! Something is wrong...";

		return false;
	}

}

bool GASP2struct::readDih(tinyxml2::XMLElement *elem, string& errorstring, GASP2dihedral &dih, GASP2molecule mol) {
	int itemp;
	const char * stemp;
	string strtemp;
	vector<string> vstemp;
	double dtemp;


	//title
	//first search for an index, then parse as string
	if(! elem->QueryIntAttribute("title",&itemp) ) {
		if(itemp > 0)
			dih.label = itemp;
	} else {
		stemp = elem->Attribute("title");
		if(stemp)
			dih.label =  newName(stemp);
		else {
			errorstring = "A dihedral title name was not specified!\n";
			return false;
		}
	}

	//angle
	stemp = elem->Attribute("angle");
	if(stemp) {
		strtemp = stemp;
		vstemp = split(strtemp);
		int size = vstemp.size();
		if(size != 4) {
			errorstring = "The dihedral angle specification for "+names[dih.label]+" does not have 4 elements!\n";
			return false;
		}
		dih.a = atomLookup(newName(vstemp[0]), mol);
		dih.b = atomLookup(newName(vstemp[1]), mol);
		dih.c = atomLookup(newName(vstemp[2]), mol);
		dih.d = atomLookup(newName(vstemp[3]), mol);
		if(dih.a < 0 || dih.b < 0 || dih.c < 0 || dih.d < 0) {
			errorstring = "An atom designation in dihedral "+names[dih.label]+" did not match any atom in the molecule!\n";
			return false;
		}
	}

	//update
	dih.update.clear();
	stemp = elem->Attribute("update");
	if(stemp) {
		strtemp = stemp;
		vstemp = split(strtemp);
		int size = vstemp.size();
		if(size < 1) {
			errorstring = "The dihedral angle update specification for "+names[dih.label]+" does not have any elements!\n";
			return false;
		}
		for(int i = 0; i < size; i++) {
			Index t = atomLookup(newName(vstemp[i]), mol);
			if(t < 0) {
				errorstring = "An atom designation in dihedral update "+names[dih.label]+" did not match any atom in the molecule!\n";
				return false;
			}
			dih.update.push_back(t);
		}
	}

	//min
	dih.minAng = rad(-180.0);
	if(!elem->QueryDoubleAttribute("min", &dtemp)) {
		dih.minAng = rad(dtemp);
	}
	if(dih.minAng < rad(-180.0) || dih.minAng > rad(180.0)) {
		errorstring = "The minimum angle in dihedral "+names[dih.label]+" is out of bounds (-180.0 < ang < 180.0).\n";
		return false;
	}

	//max
	dih.maxAng = rad(180.0);
	if(!elem->QueryDoubleAttribute("max", &dtemp)) {
		dih.maxAng = rad(dtemp);
	}
	if(dih.maxAng < rad(-180.0) || dih.maxAng > rad(180.0)) {
		errorstring = "The maximum angle in dihedral "+names[dih.label]+" is out of bounds (-180.0 < ang < 180.0).\n";
		return false;
	}


	//val
	dih.ang = rad(0.0);
	if(!elem->QueryDoubleAttribute("val", &dtemp)) {
		dih.ang = rad(dtemp);
	}
	if(dih.ang < rad(-180.0) || dih.ang > rad(180.0)) {
		errorstring = "The angle value in dihedral "+names[dih.label]+" is out of bounds (-180.0 < ang < 180.0).\n";
		return false;
	}

	return true;

}

bool GASP2struct::readBond(tinyxml2::XMLElement *elem, string& errorstring, GASP2bond &bond, GASP2molecule mol) {
	int itemp;
	const char * stemp;
	string strtemp;
	vector<string> vstemp;
	double dtemp;


	//title
	//first search for an index, then parse as string
	if(! elem->QueryIntAttribute("title",&itemp) ) {
		if(itemp > 0)
			bond.label = itemp;
	} else {
		stemp = elem->Attribute("title");
		if(stemp)
			bond.label =  newName(stemp);
		else {
			errorstring = "A bond title name was not specified!\n";
			return false;
		}
	}

	//atoms
	stemp = elem->Attribute("atoms");
	if(stemp) {
		strtemp = stemp;
		vstemp = split(strtemp);
		int size = vstemp.size();
		if(size != 2) {
			errorstring = "The bond specification for "+names[bond.label]+" does not have 2 elements!\n";
			return false;
		}
		bond.a = atomLookup(newName(vstemp[0]), mol);
		bond.b = atomLookup(newName(vstemp[1]), mol);
		if(bond.a < 0 || bond.b < 0) {
			errorstring = "An atom designation in bond "+names[bond.label]+" did not match any atom in the molecule!\n";
			return false;
		}
	}

	//min
	bond.minLen = -180.0;
	if(!elem->QueryDoubleAttribute("min", &dtemp)) {
		bond.minLen = dtemp;
	}
	if(bond.minLen <= 0.0) {
		errorstring = "The minimum length in bond "+names[bond.label]+" is out of bounds (len > 0.0).\n";
		return false;
	}

	//max
	bond.maxLen = 180.0;
	if(!elem->QueryDoubleAttribute("max", &dtemp)) {
		bond.maxLen = dtemp;
	}
	if(bond.maxLen <= 0.0 || bond.maxLen < bond.minLen) {
		errorstring = "The maximum length in bond "+names[bond.label]+" is out of bounds (len > 0.0, max > min).\n";
		return false;
	}

	//val
	bond.len = 0.0;
	if(!elem->QueryDoubleAttribute("val", &dtemp)) {
		bond.len = dtemp;
	}

	return true;
}

bool GASP2struct::readAngle(tinyxml2::XMLElement *elem, string& errorstring, GASP2angle &ang, GASP2molecule mol) {
	int itemp;
	const char * stemp;
	string strtemp;
	vector<string> vstemp;
	double dtemp;


	//title
	//first search for an index, then parse as string
	if(! elem->QueryIntAttribute("title",&itemp) ) {
		if(itemp > 0)
			ang.label = itemp;
	} else {
		stemp = elem->Attribute("title");
		if(stemp)
			ang.label =  newName(stemp);
		else {
			errorstring = "An angle title name was not specified!\n";
			return false;
		}
	}

	//angle
	stemp = elem->Attribute("angle");
	if(stemp) {
		strtemp = stemp;
		vstemp = split(strtemp);
		int size = vstemp.size();
		if(size != 3) {
			errorstring = "The angle specification for "+names[ang.label]+" does not have 3 elements!\n";
			return false;
		}
		ang.a = atomLookup(newName(vstemp[0]), mol);
		ang.b = atomLookup(newName(vstemp[1]), mol);
		ang.c = atomLookup(newName(vstemp[2]), mol);
		if(ang.a < 0 || ang.b < 0 || ang.c < 0) {
			errorstring = "An atom designation in angle "+names[ang.label]+" did not match any atom in the molecule!\n";
			return false;
		}
	}

	//min
	ang.minAng = rad(0.0);
	if(!elem->QueryDoubleAttribute("min", &dtemp)) {
		ang.minAng = rad(dtemp);
	}
	if(ang.minAng < rad(0.0) || ang.minAng > rad(180.0)) {
		errorstring = "The minimum angle in angle "+names[ang.label]+" is out of bounds (0.0 < ang < 180.0).\n";
		return false;
	}

	//max
	ang.maxAng = rad(180.0);
	if(!elem->QueryDoubleAttribute("max", &dtemp)) {
		ang.maxAng = rad(dtemp);
	}
	if(ang.maxAng < rad(0.0) || ang.maxAng > rad(180.0)) {
		errorstring = "The maximum angle in angle "+names[ang.label]+" is out of bounds (0.0 < ang < 180.0).\n";
		return false;
	}


	//val
	ang.ang = 0.0;
	if(!elem->QueryDoubleAttribute("val", &dtemp)) {
		ang.ang = rad(dtemp);
	}

	return true;
}

bool GASP2struct::readMol(tinyxml2::XMLElement *elem, string& errorstring, GASP2molecule &mol) {

	double dtemp;
	const char * stemp;
	string strtemp;
	vector<string> vstemp;
	int itemp;

	GASP2atom tempatom;
	GASP2bond tempbond;
	GASP2angle tempangle;
	GASP2dihedral tempdih;
	tinyxml2::XMLElement *item;

	//name
	if(! elem->QueryIntAttribute("name",&itemp) ) {
		if(itemp > 0)
			mol.label = itemp;
	} else {
		stemp = elem->Attribute("name");
		if(stemp)
			mol.label =  newName(stemp);
		else {
			errorstring = "A molecule name was not specified!\n";
			return false;
		}
	}

	//plindex
	mol.plindex = 0;
	if(! elem->QueryIntAttribute("plind",&itemp) ) {
		if(itemp > 0)
			mol.plindex = itemp;
	}

	//symm
	mol.symm = 0;
	if(! elem->QueryIntAttribute("symm",&itemp) ) {
		if(itemp > 0)
			mol.symm = itemp;
	}

	//expectvol
	if(!elem->QueryDoubleAttribute("expectvol", &dtemp)) {
		mol.expectvol = dtemp;
	}
	else {
		errorstring = "No expected volume was specified for "+names[mol.label]+"!\n";
		return false;
	}
	if(mol.expectvol <= 0.0) {
		errorstring = "The expected volume for "+names[mol.label]+" is out of bounds (vol > 0.0).\n";
		return false;
	}

	//parse the atoms
	//MUST COME BEFORE DIHEDRALS, etc
	item = elem->FirstChildElement("atom");
	if(!item) {
		errorstring = "No atoms specified in molecule!\n";
		return false;
	}
	while(item) {
		if(! readAtom(item, errorstring, tempatom) )
			return false;
		//commit the atom to the list
		mol.atoms.push_back(tempatom);
		item = item->NextSiblingElement("atom");
	}

	//plane
	stemp = elem->Attribute("plane");
	if(stemp) {
		strtemp = stemp;
		vstemp = split(strtemp);
		int size = vstemp.size();
		if(size != 3) {
			errorstring = "The plane specification for "+names[mol.label]+" does not have 3 elements!\n";
			return false;
		}
		mol.p1 = atomLookup(newName(vstemp[0]), mol);
		mol.p2 = atomLookup(newName(vstemp[1]), mol);
		mol.p3 = atomLookup(newName(vstemp[2]), mol);
		if(mol.p1 < 0 || mol.p2 < 0 || mol.p3 < 0) {
			errorstring = "An atom designation in plane for "+names[mol.label]+" did not match any atom in the molecule!\n";
			return false;
		}
	}

	//parse the dihedrals
	item = elem->FirstChildElement("dihedral");
	while(item) {
		if(! readDih(item, errorstring, tempdih, mol) )
			return false;
		mol.dihedrals.push_back(tempdih);
		item = item->NextSiblingElement("dihedral");
		tempdih.clear();
	}

	//parse the bonds
	item = elem->FirstChildElement("bond");
	while(item) {
		if(! readBond(item, errorstring, tempbond, mol) )
			return false;
		mol.bonds.push_back(tempbond);
		item = item->NextSiblingElement("bond");
	}

	//parse the angles
	item = elem->FirstChildElement("angle");
	while(item) {
		if(! readAngle(item, errorstring, tempangle, mol) )
			return false;
		mol.angles.push_back(tempangle);
		item = item->NextSiblingElement("angle");
	}

	//rotation matrix
	mol.rot = vl_1;
	item = elem->FirstChildElement("rot");
	if(item) {
		stemp = item->GetText();
		if(stemp) {
			strtemp = stemp;
			vstemp = split(strtemp);
			int size = vstemp.size();
			if(size != 9) {
				errorstring = "The rotation matrix for "+names[mol.label]+" does not have 9 elements!\n";
				return false;
			}
			for(int i = 0; i < size; i++) {
				istringstream ( vstemp[i] ) >> dtemp;
				mol.rot[i/3][i%3] = dtemp;
			}
		}
	}

	//position vector
	mol.pos = vl_0;
	item = elem->FirstChildElement("pos");
	if(item) {
		stemp = item->GetText();
		if(stemp) {
			strtemp = stemp;
			vstemp = split(strtemp);
			int size = vstemp.size();
			if(size != 3) {
				errorstring = "The position vector for "+names[mol.label]+" does not have 3 elements!\n";
				return false;
			}
			for(int i = 0; i < size; i++) {
				istringstream ( vstemp[i] ) >> dtemp;
				mol.pos[i] = dtemp;
			}
		}
	}
	return true;
}

bool GASP2struct::readCell(tinyxml2::XMLElement *elem, string& errorstring, GASP2cell &cell) {

	double dtemp;
	int itemp;
	const char * stemp;
	string strtemp;
	tinyxml2::XMLElement * item;

	cell.clear();

	//a
	cell.a = 0.01;
	if(!elem->QueryDoubleAttribute("a", &dtemp)) {
		cell.a = dtemp;
	}
	if(cell.a <= 0.0) {
		errorstring = "Cell length A must be greater than 0!\n";
		return false;
	}

	//b
	cell.b = 0.01;
	if(!elem->QueryDoubleAttribute("b", &dtemp)) {
		cell.b = dtemp;
	}
	if(cell.b <= 0.0) {
		errorstring = "Cell length B must be greater than 0!\n";
		return false;
	}

	//c
	cell.c = 0.01;
	if(!elem->QueryDoubleAttribute("c", &dtemp)) {
		cell.c = dtemp;
	}
	if(cell.c <= 0.0) {
		errorstring = "Cell length C must be greater than 0!\n";
		return false;
	}

	//alpha
	cell.alpha = rad(90.0);
	if(!elem->QueryDoubleAttribute("al", &dtemp)) {
		cell.alpha = rad(dtemp);
	}
	if(cell.alpha <= rad(0.0) || cell.alpha >= rad(180.0)) {
		errorstring = "Cell alpha is out of bound (0.0 < x < 180.0)!\n";
		return false;
	}

	//beta
	cell.beta = rad(90.0);
	if(!elem->QueryDoubleAttribute("bt", &dtemp)) {
		cell.beta = rad(dtemp);
	}
	if(cell.beta <= rad(0.0) || cell.beta >= rad(180.0)) {
		errorstring = "Cell beta is out of bound (0.0 < x < 180.0)!\n";
		return false;
	}

	//gamma
	cell.gamma = rad(90.0);
	if(!elem->QueryDoubleAttribute("gm", &dtemp)) {
		cell.gamma = rad(dtemp);
	}
	if(cell.gamma <= rad(0.0) || cell.gamma >= rad(180.0)) {
		errorstring = "Cell gamma is out of bound (0.0 < x < 180.0)!\n";
		return false;
	}

	//triclinic A
	cell.triA = rad(90.0);
	if(!elem->QueryDoubleAttribute("tA", &dtemp)) {
		cell.triA = rad(dtemp);
	}
	if(cell.triA <= rad(0.0) || cell.triA >= rad(180.0)) {
		errorstring = "Cell triA is out of bound (0.0 < x < 180.0)!\n";
		return false;
	}

	//tricilinic B
	cell.triB = rad(90.0);
	if(!elem->QueryDoubleAttribute("tB", &dtemp)) {
		cell.triB = rad(dtemp);
	}
	if(cell.triB <= rad(0.0) || cell.triB >= rad(180.0)) {
		errorstring = "Cell triB is out of bound (0.0 < x < 180.0)!\n";
		return false;
	}

	//triclinic C
	cell.triC = rad(90.0);
	if(!elem->QueryDoubleAttribute("tC", &dtemp)) {
		cell.triC = rad(dtemp);
	}
	if(cell.triC <= rad(0.0) || cell.triC >= rad(180.0)) {
		errorstring = "Cell triC is out of bound (0.0 < x < 180.0)!\n";
		return false;
	}

	//monoclinic B
	cell.monoB = rad(90.0);
	if(!elem->QueryDoubleAttribute("mB", &dtemp)) {
		cell.monoB = rad(dtemp);
	}
	if(cell.monoB <= rad(0.0) || cell.monoB >= rad(180.0)) {
		errorstring = "Cell monoB is out of bound (0.0 < x < 180.0)!\n";
		return false;
	}

	//rhombehedral C
	cell.rhomC = rad(60.0);
	if(!elem->QueryDoubleAttribute("rhmC", &dtemp)) {
		cell.rhomC = rad(dtemp);
	}
	if(cell.rhomC <= rad(0.0) || cell.rhomC >= rad(120.0)) {
		errorstring = "Cell rhomC is out of bound (0.0 < x < 120.0)!\n";
		return false;
	}

	//rA
	cell.ratA = 0.01;
	if(!elem->QueryDoubleAttribute("rA", &dtemp)) {
		cell.ratA = dtemp;
	}
	if(cell.ratA <= 0.0) {
		errorstring = "Cell ratio A must be greater than 0!\n";
		return false;
	}

	//rB
	cell.ratB = 0.01;
	if(!elem->QueryDoubleAttribute("rB", &dtemp)) {
		cell.ratB = dtemp;
	}
	if(cell.ratB <= 0.0) {
		errorstring = "Cell ratio B must be greater than 0!\n";
		return false;
	}

	//rC
	cell.ratC = 0.01;
	if(!elem->QueryDoubleAttribute("rC", &dtemp)) {
		cell.ratC = dtemp;
	}
	if(cell.ratC <= 0.0) {
		errorstring = "Cell ratio C must be greater than 0!\n";
		return false;
	}

	//axisorder
	cell.axisorder = 0;
	if(!elem->QueryIntAttribute("axisorder", &itemp)) {
		cell.axisorder=itemp;
	}
	if(cell.axisorder < 0 || cell.axisorder > 5) {
		errorstring = "Axis order out of bounds (0 <= x <= 5)!\n";
		return false;
	}

	//spacegroup
	cell.spacegroup = 1;
	stemp = elem->Attribute("spacegroup");
	if(!elem->QueryIntAttribute("spacegroup", &itemp)) {
		if(itemp > 0 && itemp <= spacegroupNames.size())
			cell.spacegroup = itemp;
		else {
			errorstring = "A non-valid spacegroup identifier was given (out of bounds).\n";
			return false;
		}
	}
	else if (stemp){
		//stemp = elem->Attribute("spacegroup");
		strtemp = stemp;
		cell.spacegroup = 0;
		for(int i = 0; i < spacegroupNames.size(); i++) {
			if(strtemp.compare(spacegroupNames[i]) == 0) {
				cell.spacegroup = i;
				break;
			}
		}
		if(cell.spacegroup == 0) {
			errorstring = "A non-valid string identifier was given for a spacegroup value.\n";
			return false;
		}
	}

	//schoenflies typ
	cell.typ = Schoenflies::Cn;
	stemp = elem->Attribute("typ");
	if(stemp) {
		strtemp = stemp;
		cell.typ = getSchoenflies(strtemp);
		if(cell.typ == Schoenflies::UNK) {
			errorstring = "A non-valid string identifier was given for the Schoenflies type.\n";
			return false;
		}
	}

	//Axisnum axn
	cell.axn = Axisnum::One;
	stemp = elem->Attribute("axn");
	if(stemp) {
		strtemp = stemp;
		cell.axn = getAxis(strtemp);
		if(cell.axn == Axisnum::UNK) {
			errorstring = "A non-valid string identifier was given for the axis type.\n";
			return false;
		}
	}

	//Centering cen
	cell.cen = 0.0;
	if(!elem->QueryDoubleAttribute("cen", &dtemp)) {
		cell.cen = dtemp;
	}
	if(cell.cen < 0.0 || cell.cen > 1.0) {
		errorstring = "Centering must be on interval [0.0,1.0]!\n";
		return false;
	}
	//Subtype sub
	cell.sub = 0.0;
	if(!elem->QueryDoubleAttribute("sub", &dtemp)) {
		cell.sub = dtemp;
	}
	if(cell.sub < 0.0 || cell.sub > 1.0) {
		errorstring = "Subtype must be on interval [0.0,1.0]!\n";
		return false;
	}

	//stoichiometry
	//parse the angles
	GASP2stoich tempstoich;

	item = elem->FirstChildElement("stoichiometry");
	while(item) {
		if(! readStoich(item, errorstring, tempstoich) )
			return false;
		cell.stoich.push_back(tempstoich);
		item = item->NextSiblingElement("stoichiometry");
	}

	return true;
}

bool GASP2struct::readInfo(tinyxml2::XMLElement *elem, string& errorstring) {


	const char * stemp;
	string strtemp;
	UUID uuidtemp;
	double dtemp;
	int itemp;


	//ID
	stemp = elem->Attribute("id");
	if(stemp) {
		strtemp = stemp;
		if(strtemp.size() != 36) {
			errorstring = "A given UUID does not have the right length!\n";
			return false;
		}
		uuidtemp.frStr(strtemp);
		ID = uuidtemp;
	}

	//ParentA
	stemp = elem->Attribute("parentA");
	if(stemp) {
		strtemp = stemp;
		if(strtemp.size() != 36) {
			errorstring = "A given UUID does not have the right length!\n";
			return false;
		}
		uuidtemp.frStr(strtemp);
		parentA = uuidtemp;
	}

	//ParentB
	stemp = elem->Attribute("parentB");
	if(stemp) {
		strtemp = stemp;
		if(strtemp.size() != 36) {
			errorstring = "A given UUID does not have the right length!\n";
			return false;
		}
		uuidtemp.frStr(strtemp);
		parentB = uuidtemp;
	}

	//opt flag
	stemp = elem->Attribute("opt");
	if(stemp) {
		strtemp = stemp;
		if(strtemp != "true" && strtemp != "false" && strtemp != "t" && strtemp != "f")	{
			errorstring = "Opt flag is not true or false!\n";
			return false;
		}
		didOpt = false;
		if(strtemp == "true")
			didOpt = true;
	}

	//complete flag
	stemp = elem->Attribute("complete");
	if(stemp) {
		strtemp = stemp;
		if(strtemp != "true" && strtemp != "false" && strtemp != "t" && strtemp != "f")	{
			errorstring = "Complete flag is not true or false!\n";
			return false;
		}
		complete = false;
		if(strtemp == "true")
			complete = true;
	}

	//fitcell flag
	stemp = elem->Attribute("fitcell");
	if(stemp) {
		strtemp = stemp;
		if(strtemp != "true" && strtemp != "false" && strtemp != "t" && strtemp != "f")	{
			errorstring = "Fitcell flag is not true or false!\n";
			return false;
		}
		isFitcell = false;
		if(strtemp == "true")
			isFitcell = true;
	}

	//energy
	energy = 0.0;
	if(!elem->QueryDoubleAttribute("energy", &dtemp)) {
		energy = dtemp;
	}

	pseudoenergy = 0.0;
	if(!elem->QueryDoubleAttribute("pseudoenergy", &dtemp)) {
		pseudoenergy = dtemp;
	}

	//force
	force = 0.0;
	if(!elem->QueryDoubleAttribute("force", &dtemp)) {
		force = dtemp;
	}

	//pressure
	pressure = 0.0;
	if(!elem->QueryDoubleAttribute("pressure", &dtemp)) {
		pressure = dtemp;
	}

	//error
	stemp = elem->Attribute("error");
	if(stemp) {
		strtemp = stemp;
		if(strtemp == "OKStruct")
			finalstate = OKStruct;
		else if(strtemp == "OptBadBond")
			finalstate = OptBadBond;
		else if(strtemp == "OptBadAng")
			finalstate = OptBadAng;
		else if(strtemp == "OptBadDih")
			finalstate = OptBadDih;
		else if(strtemp == "FitcellBadDih")
			finalstate = FitcellBadDih;
		else if(strtemp == "FitcellBadCell")
			finalstate = FitcellBadCell;
		else if(strtemp == "NoFitcell")
			finalstate = NoFitcell;
		else {
			errorstring = "An structure error was given, but it does not match any known error types!\n";
			return false;
		}
	}

	//time
	time = 0;
	if(!elem->QueryIntAttribute("time", &itemp)) {
		time = itemp;
	}

	//steps
	steps = 0;
	if(!elem->QueryIntAttribute("steps", &itemp)) {
		steps = itemp;
	}

	//contacts
	contacts = 0;
	if(!elem->QueryIntAttribute("contacts", &itemp)) {
		contacts = itemp;
	}


	return true;

}

bool GASP2struct::readStoich(tinyxml2::XMLElement *elem, string& errorstring, GASP2stoich &stoich) {

	int itemp;
	const char * stemp;
	string strtemp;

	//title
	//first search for an index, then parse as string
	stemp = elem->Attribute("mol");
	if(stemp) {
		strtemp = stemp;
		bool flag = false;
		for(int i = 0; i < molecules.size(); i++) {
			if(names[molecules[i].label] == strtemp) {
				stoich.mol = molecules[i].label;
				flag = true;
				break;
			}
		}
		if(flag == false) {
			errorstring = "The molecules for one of the stoichiometries does not match the name of an existing molecule!\n";
			return false;
		}
	}
	else {
		errorstring = "A stoichiometry molecule name was not found!\n";
		return false;
	}

	//count
	stoich.count = 1;
	if(!elem->QueryIntAttribute("count", &itemp)) {
		stoich.count = itemp;
	}

	stoich.min = 1;
	if(!elem->QueryIntAttribute("min", &itemp)) {
		stoich.min = itemp;
	}

	stoich.max = 1;
	if(!elem->QueryIntAttribute("max", &itemp)) {
		stoich.max = itemp;
		if(stoich.max < stoich.min) {
			errorstring = "The max count for stoichiometry " + strtemp + " is less than the min count!\n";
			return false;
		}
	}

	if(stoich.max < stoich.count)
		stoich.max = stoich.count;
	if(stoich.min > stoich.count)
		stoich.min = stoich.count;

	//cout << "stoich stats: " << stoich.max << " " << stoich.min << endl;

	return true;

}


void GASP2struct::logStruct() {
	cout << endl << endl;
	cout << setprecision(8) << fixed;
	cout << "Structure name: " << names[crylabel] << endl;
	cout << "Root structure id: " << ID.toStr() << endl;
	cout << "Spacegroup: " << spacegroupNames[unit.spacegroup] << "  (ID:" << unit.spacegroup << ")" << endl;
//	cout << "Parent A id : " << parentA.toStr() << endl;
//	cout << "Parent B id : " << parentB.toStr() << endl << endl;

//	cout << "Name index:" << endl;
//	for(int i = 0; i< names.size(); i++) {
//		cout << "  " << i << ": " << names[i] << endl;
//	}
//	cout << endl;
	cout << "Stoichiometry:\n";
	for(int i = 0; i< unit.stoich.size(); i++) {
		cout << "  " << names[unit.stoich[i].mol] << ": " << unit.stoich[i].count << endl;

	}


	cout << "Number of molecules: " << molecules.size() << endl << endl;;

	for(int i = 0; i < molecules.size(); i++) {
		cout << "  Molecule:" << names[molecules[i].label] << " volume: " << molecules[i].expectvol <<endl;
		for(int j = 0; j < molecules[i].atoms.size(); j++) {
			GASP2atom at = molecules[i].atoms[j];
			cout << "    Atm "<< names[at.label]<<" "<<getElemName(at.type)<<" "<<at.pos<<endl;
		}
		for(int j = 0; j < molecules[i].dihedrals.size(); j++) {
			GASP2dihedral dh = molecules[i].dihedrals[j];
			cout << "    Dih " << names[dh.label]<<" ("<<deg(dh.minAng)<<","<<deg(dh.maxAng)<<")"<<endl;
			cout << "        Update: ";
			for(int k = 0; k < dh.update.size(); k++)
				cout << names[molecules[i].atoms[ dh.update[k] ].label] <<" ";
			cout << endl;
		}
		for(int j = 0; j < molecules[i].angles.size(); j++) {
			GASP2angle an = molecules[i].angles[j];
			cout << "    Ang " << names[an.label]<<" ("<<deg(an.minAng)<<","<<deg(an.maxAng)<<")"<<endl;
		}
		for(int j = 0; j < molecules[i].bonds.size(); j++) {
			GASP2bond bn = molecules[i].bonds[j];
			cout << "    Bon " << names[bn.label]<<" ("<<bn.minLen<<","<<bn.maxLen<<")"<<endl;
		}
		cout << endl;
	}
	cout << endl;

}

//get the planerotation of the molecule
Mat3 GASP2struct::getPlaneRot(GASP2molecule mol) {
	Mat3 out;
	Vec3 a = mol.atoms[mol.p2].pos - mol.atoms[mol.p1].pos;
	Vec3 b = mol.atoms[mol.p3].pos - mol.atoms[mol.p1].pos;
	Vec3 N = cross(a,b);
	Vec3 c = cross(N,a);

	out[0] = norm(N);
	out[1] = norm(a);
	out[2] = norm(c);

	out = trans(out);

	return out;
}

//get the centroid of the molecule
Vec3 GASP2struct::getMolCentroid(GASP2molecule mol) {
	Vec3 out = vl_0;
	for(int i = 0; i < mol.atoms.size(); i++)
		out += mol.atoms[i].pos;
	return out/static_cast<double>(mol.atoms.size());
}

//output a cif file (NOT USED ANYMORE, USE cifString)
bool GASP2struct::cifOut(string name) {

	ofstream outf;
	outf.open(name.c_str(), ofstream::out | ofstream::app);
	if(outf.fail())
		return false;

	if(finalstate != OKStruct)
		return false;

	GASP2cell outcell;
//	outcell.a = 10.0;
//	outcell.b = 10.0;
//	outcell.c = 10.0;
//	outcell.alpha = rad(90.0);
//	outcell.beta = rad(90.0);
//	outcell.gamma = rad(90.0);
	outcell = unit;

	Mat3 toFrac = cartToFrac(outcell);

	Vec3 temp;
	//limiter
	//int size = molecules[0].atoms.size();




	outf << "data_" << names[crylabel]+"_"+ID.toStr() << endl;
	outf << "#meta e="<<energy<<",f="<<force<<",p="<<pressure<<",v="<<getVolume()<<",vs="<<getVolScore()<<endl;
	outf << "#meta t="<<time<<",s="<<steps<<",st="<<getStructError(finalstate)<<",c="<<tfconv(complete)<<",fc="<<tfconv(isFitcell)<< endl;
	outf << "loop_" << endl;
	outf << "_symmetry_equiv_pos_as_xyz" << endl;
	outf << "x,y,z" << endl;
	outf << "_cell_length_a " << outcell.a << endl;
	outf << "_cell_length_b " << outcell.b << endl;
	outf << "_cell_length_c " << outcell.c << endl;
	outf << "_cell_angle_alpha " << deg(outcell.alpha) << endl;
	outf << "_cell_angle_beta " << deg(outcell.beta) << endl;
	outf << "_cell_angle_gamma " << deg(outcell.gamma) << endl;

	outf << "loop_\n_atom_site_label\n_atom_site_type_symbol\n";
	outf << "_atom_site_fract_x\n_atom_site_fract_y\n_atom_site_fract_z"
			<< endl;
	for (int i = 0; i < molecules.size(); i++) {
		for (int j = 0; j < molecules[i].atoms.size(); j++) {
			outf << names[molecules[i].atoms[j].label] << " ";
			outf << getElemName(molecules[i].atoms[j].type) << " ";
			temp = toFrac*molecules[i].atoms[j].pos + molecules[i].pos;
			outf << temp[0] << " ";
			outf << temp[1] << " ";
			outf << temp[2] << endl;
		}
	}
	outf << "#END" << endl << endl;

	outf.close();
	return true;

}

//generates a CIF from the structure info
bool GASP2struct::cifString(string &out, int rank) {

	out = "";

	stringstream outf;
	outf.clear();

	if(finalstate != OKStruct)
		return false;

	GASP2cell outcell;
//	outcell.a = 10.0;
//	outcell.b = 10.0;
//	outcell.c = 10.0;
//	outcell.alpha = rad(90.0);
//	outcell.beta = rad(90.0);
//	outcell.gamma = rad(90.0);
	outcell = unit;

	Mat3 toFrac = cartToFrac(outcell);

	Vec3 temp;
	//limiter
	//int size = molecules[0].atoms.size();

	outf << setprecision(5) << fixed;
	outf << "data_" << setfill('0') << setw(3) << rank << "_" << names[crylabel]+"_"+ID.toStr() << endl;
	outf << "_symmetry_space_group_name_H-M  '" << spacegroupNames[unit.spacegroup] << "'\n";
	outf << "#meta e="<<energy<<",f="<<force<<",p="<<pressure<<",v="<<getVolume()<<",vs="<<getVolScore()<<",ct="<<contacts<<",pse="<<pseudoenergy<<endl;
	outf << "#meta t="<<time<<",s="<<steps<<",st="<<getStructError(finalstate)<<",c="<<tfconv(complete)<<",fc="<<tfconv(isFitcell)<<",cl="<<cluster<< endl;
//	outf << "loop_" << endl;
//	outf << "_symmetry_equiv_pos_site_id" << endl;
//	outf << "_symmetry_equiv_pos_as_xyz" << endl;
//	for(int i = 0; i <
//	outf << "x,y,z" << endl;
	outf << "_cell_length_a " << outcell.a << endl;
	outf << "_cell_length_b " << outcell.b << endl;
	outf << "_cell_length_c " << outcell.c << endl;
	outf << "_cell_angle_alpha " << deg(outcell.alpha) << endl;
	outf << "_cell_angle_beta " << deg(outcell.beta) << endl;
	outf << "_cell_angle_gamma " << deg(outcell.gamma) << endl;

	outf << "loop_\n_atom_site_label\n_atom_site_type_symbol\n";
	outf << "_atom_site_fract_x\n_atom_site_fract_y\n_atom_site_fract_z"
			<< endl;
	for (int i = 0; i < molecules.size(); i++) {
		centerMol(molecules[i]);
		for (int j = 0; j < molecules[i].atoms.size(); j++) {
			outf << names[molecules[i].atoms[j].label] << " ";
			outf << getElemName(molecules[i].atoms[j].type) << " ";
			temp = toFrac*molecules[i].atoms[j].pos + molecules[i].pos;
			outf << temp[0] << " ";
			outf << temp[1] << " ";
			outf << temp[2] << endl;
		}
	}
	outf << "#END" << endl << endl;

	out = outf.str();
	return true;

}

//convert fractional cooridnates to cartesian
Mat3 fracToCart(GASP2cell cl) {
	double phi = cellPhi(cl);
	double kappa = (cl.c*(cos(cl.alpha)-cos(cl.beta)*cos(cl.gamma))) / sin(cl.gamma);

	return Mat3	(cl.a,	cl.b*cos(cl.gamma), cl.c*cos(cl.beta),
				 0,		cl.b*sin(cl.gamma),	kappa,
				 0,		0,					cl.c*phi/sin(cl.gamma)
				);
}
//vice versa
Mat3 cartToFrac(GASP2cell cl) {
	return inv(fracToCart(cl));

}

//helpder function for frac/cart conversion
double cellPhi(GASP2cell cl) {

	double cosA = cos(cl.alpha);
	double cosB = cos(cl.beta);
	double cosC = cos(cl.gamma);
	double phi = 2.0 * cosA * cosB * cosC;

    return   sqrt(1.0 - cosA*cosA
            		- cosB*cosB
            		- cosC*cosC
            		+ phi) ;
}

//get the cell volume
double cellVol(GASP2cell cl) {
	return cl.a * cl.b * cl.c * cellPhi(cl);
}



//the simple comparison reports the Chebyshev distance between
//two different structures. the distance is calculated by normalizing
//each of the INDEPENDENT paramters of the GA schema, such that a value
//of 1.0 is equivalent to the effective step size of the scheme parameter
//(e.g., given a step size of 20 degrees, 30 degrees gives a value of 1.5)
//FIXME:THIS CODE ASSUMES THAT EQUIVALENT STOICHIMETRIES ARE GIVEN AS INPUT
//crystals that do not have the same stoichimetry will have PROBLEMS

//IF TRUE, STRUCTURES ARE "THE SAME"
bool GASP2struct::simpleCompare(GASP2struct alt, GASP2param p, double & average, double & chebyshev, double & euclid, double & num) {
	vector<double> scores;
	scores.reserve(100);

	chebyshev = -1.0;
	average = -1.0;
	euclid = -1.0;
	num = -1.0;
	//a negative distance is impossible, so these cannot be the same :P
	if(unit.spacegroup != alt.unit.spacegroup)
		return false;



	//needed to revise this because different lattices
	//have different fixed parameters
	//if we add scores for certain genes, there will be
	//artifically high similarity
	Spgroup spg = spacegroups[unit.spacegroup];





	//enforce angles and lengths
	if (spg.L == Lattice::Cubic ) {
		//a = b = c; alpha = beta = gamma = 90;
	} else if (spg.L == Lattice::Tetragonal) {
		//a = b; alpha = beta = gamma = 90;
		scores.push_back(std::abs(unit.ratA - alt.unit.ratA) / p.ratiostep);
		scores.push_back(std::abs(unit.ratC - alt.unit.ratC) / p.ratiostep);
	} else if (spg.L == Lattice::Orthorhombic) {
		//alpha = beta = gamma = 90;
		scores.push_back(std::abs(unit.ratA - alt.unit.ratA) / p.ratiostep);
		scores.push_back(std::abs(unit.ratB - alt.unit.ratB) / p.ratiostep);
		scores.push_back(std::abs(unit.ratC - alt.unit.ratC) / p.ratiostep);
	} else if (spg.L == Lattice::Hexagonal) {
		//a = b; alpha = beta = 90; gamma = 120
		scores.push_back(std::abs(unit.ratA - alt.unit.ratA) / p.ratiostep);
		scores.push_back(std::abs(unit.ratC - alt.unit.ratC) / p.ratiostep);
	} else if (spg.L == Lattice::Rhombohedral) {
		//a = b = c; alpha = beta = gamma;
		scores.push_back(std::abs(unit.rhomC - alt.unit.rhomC) / p.angstep);
	} else if (spg.L == Lattice::Monoclinic) {
		//alpha = gamma = 90;
		scores.push_back(std::abs(unit.monoB - alt.unit.monoB) / p.angstep);

		scores.push_back(std::abs(unit.ratA - alt.unit.ratA) / p.ratiostep);
		scores.push_back(std::abs(unit.ratB - alt.unit.ratB) / p.ratiostep);
		scores.push_back(std::abs(unit.ratC - alt.unit.ratC) / p.ratiostep);
	} else if (spg.L == Lattice::Triclinic) {
		//unconstrained
		scores.push_back(std::abs(unit.triA - alt.unit.triA) / p.angstep);
		scores.push_back(std::abs(unit.triB - alt.unit.triB) / p.angstep);
		scores.push_back(std::abs(unit.triC - alt.unit.triC) / p.angstep);

		scores.push_back(std::abs(unit.ratA - alt.unit.ratA) / p.ratiostep);
		scores.push_back(std::abs(unit.ratB - alt.unit.ratB) / p.ratiostep);
		scores.push_back(std::abs(unit.ratC - alt.unit.ratC) / p.ratiostep);
	}

	//THIS CODE ASSUMES POSITIONAL EQUIVALENCE BETWEEN PAIRS OF MOLECULES
	//probably should not be used for more than z'=1 until a clean solution
	//can be implemented
	for(int i = 0; i < molecules.size(); i++) {
		if(molecules[i].symm == 0 && alt.molecules[i].symm == 0) {
			//double tempscore = 0.0;
			scores.push_back(std::abs(molecules[i].pos[0] - alt.molecules[i].pos[0]) / p.posstep);
			scores.push_back(std::abs(molecules[i].pos[1] - alt.molecules[i].pos[1]) / p.posstep);
			scores.push_back(std::abs(molecules[i].pos[2] - alt.molecules[i].pos[2]) / p.posstep);

			//rotation matrix: because the output of the angle algorithm is unreliable,
			//we generate two angle/axis pairs which are equivalent but inverse of each other
			//then the pair with the smallest angle between the axis is taken as the distance
			Vec3 axis[3]; double ang[3];
			getAngleAxis(molecules[i].rot, axis[0], ang[0]);
			getAngleAxis(alt.molecules[i].rot, axis[1],ang[1]);
			axis[2] = -1.0 * axis[1]; ang[2] = -1.0 * ang[1];

			for(int n=0; n<3;n++)
				if(ang[n] < 0.0) ang[n] += (2.0*PI);


			double diff = angle(axis[0], Vec3(0.0,0.0,0.0), axis[1]);
			double diffB = angle(axis[0], Vec3(0.0,0.0,0.0), axis[3]);
			double angdiff = std::abs(ang[0] - ang[1]);
			double angdiffB = std::abs(ang[0] - ang[3]);

			if(diffB < diff) {
				diff = diffB;
				angdiff = angdiffB;
			}

			scores.push_back(diff / p.rotstep / 2.0); //adjusted to always be twice as many as rotstep
			if(angdiff > PI) angdiff -= PI;
			scores.push_back(angdiff / p.rotstep);
			//end rotation matrix calcs

			for(int k = 0; k < molecules[i].dihedrals.size(); k++) {
				angdiff = std::abs(molecules[i].dihedrals[k].ang - alt.molecules[i].dihedrals[k].ang);
				if(angdiff > PI) angdiff = PI - (angdiff - PI);
				scores.push_back(angdiff / p.dihstep);
			}
		}
	}
	chebyshev = 0.0;
	average = 0.0;
	euclid = 0.0;
	num = 0.0;
	for(int i = 0; i < scores.size(); i++) {
		if (scores[i] > chebyshev) chebyshev = scores[i];
		if (scores[i] <= p.clustersize) num += 1.0;
		euclid += scores[i] * scores[i];
		average += scores[i];
	}
	euclid = sqrt(euclid);
	//THIS is important!
	//the maximum is removed from the average;
	//so, the score is the average minus the highest score. we ignore the highest value because
	//if n-1/n paramters are within the average threshold, the likelihood of those being the
	//same structure is very high, even though some paramters may be disparate.
	//this will prevent oversampling of decent sized ranges; all that remains is tuning
	average -= chebyshev;
	average /= static_cast<double>(scores.size()-1);

	double percentage =  ( num / static_cast<double>(scores.size()) );
	//cout << percentage << " ";
	//check the cell lengths
	//if all three are within 1.0 angstroms of each other
	//and the genetic similarity is > 50%
	//then they are more or less the same
	//why 1.0 ang? because highly similar unit cells will optimize to each other
	//essentially, a wide net reduces the computational load
	if(percentage >= 0.5) {
		if( (std::abs(unit.a - alt.unit.a) < 1.0) &&
			(std::abs(unit.b - alt.unit.b) < 1.0) &&
			(std::abs(unit.c - alt.unit.c) < 1.0) ) {
			//cout << "cellsame" << endl;
			return true;
		}

	}


	//if(average <= p.clustersize && chebyshev <= p.chebyshevlimit)
	if( percentage > p.clusterdiff ) {
		//cout << "genesame" << endl;
		return true;
	}
	//cout << "notsame" << endl;
	return false;


}

//FIXME:THESE FUNCTIONS DO NOT HANDLE VECTORS FOR DIFFERENT MOLECULE TYPES
//THIS FUNCTION HAS NEVER WORKED CORRECTLY
void GASP2struct::setVector(vector<double> in, int mol, int dih) {


	Vec3 axis; double ang;

	unit.alpha = in[0];
	unit.beta = in[1];
	unit.gamma = in[2];
	unit.ratA = in[3];
	unit.ratB = in[4];
	unit.ratC = in[5];

	int molstep = 7 + dih;

	for(int i = 0; i < mol; i++) {
		//pos
		molecules[i].pos[0] = in[i*molstep + 6 + 0];
		molecules[i].pos[1] = in[i*molstep + 6 + 1];
		molecules[i].pos[2] = in[i*molstep + 6 + 2];
		//rot & ang
		axis[0] = in[i*molstep + 6 + 3];
		axis[1] = in[i*molstep + 6 + 4];
		axis[2] = in[i*molstep + 6 + 5];
		ang = std::fmod(in[i*molstep + 6 + 6], 2.0*PI);
		molecules[i].rot = Rot3(norm(axis), ang);

		//dih
		for(int j = 0; j < dih; j++) {
			molecules[i].dihedrals[j].ang = std::fmod(in[i*molstep + 6 + 7 + j], 2.0*PI);
			if(molecules[i].dihedrals[j].ang > PI)
				molecules[i].dihedrals[j].ang -= PI;
		}
	}

	//backset the angle genes
	unit.rhomC = unit.gamma;
	unit.monoB = unit.beta;
	unit.triA = unit.alpha;
	unit.triB = unit.beta;
	unit.triC = unit.gamma;

}

//FIXME:THESE FUNCTIONS DO NOT HANDLE VECTORS FOR DIFFERENT MOLECULE TYPES
//THIS FUNCTION HAS NEVER WORKED CORRECTLY
vector<double> GASP2struct::getVector(int &mol, int &dih) {
	vector<double> out;
	Vec3 axis, pos; double ang;
	//al,bt,gm,rA,rB,rC are always first
	out.push_back(unit.alpha);
	out.push_back(unit.beta);
	out.push_back(unit.gamma);
	out.push_back(unit.ratA);
	out.push_back(unit.ratB);
	out.push_back(unit.ratC);

	mol = molecules.size();
	dih = molecules[0].dihedrals.size();

	for(int i = 0; i < mol; i++) {
		//pos
		out.push_back(molecules[i].pos[0]);
		out.push_back(molecules[i].pos[1]);
		out.push_back(molecules[i].pos[2]);
		//rot & ang
		getAngleAxis(molecules[i].rot, axis, ang);
		out.push_back(axis[0]);
		out.push_back(axis[1]);
		out.push_back(axis[2]);
		out.push_back(ang);
		//dih
		for(int j = 0; j < dih; j++) {
			out.push_back(molecules[i].dihedrals[j].ang);
		}
	}

	return out;
}

//THIS FUNCTION IS BROKEN
bool vectoradd(vector<double> &sum, vector<double> add, int mol, int dih) {

	double diff, revdiff;
	//this sums a structure vector in such a way that
	//modulus parameters are averaged appropriately

	//alpha, beta, gamma, and rA,rB,rC are non-modulus

	//axis, ang, and dihedrals are modulus and need to be treated thusly

	if(sum.size() == add.size()) {
		//a,b,c,ra,rb,rc
		for(int i = 0; i < 6; i++)
			sum[i] += add[i];

		int molstep = dih + 7;

		for(int i = 0; i < mol; i++) {
			//pos
			sum[i*molstep + 6 + 0] += add[i*molstep + 6 + 0];
			sum[i*molstep + 6 + 1] += add[i*molstep + 6 + 1];
			sum[i*molstep + 6 + 2] += add[i*molstep + 6 + 2];

			//axis
			Vec3 ref(sum[i*molstep + 6 + 3], sum[i*molstep + 6 + 4], sum[i*molstep + 6 + 5]);
			Vec3 axis(add[i*molstep + 6 + 3], add[i*molstep + 6 + 4], add[i*molstep + 6 + 5]);
			axis = norm(axis);
			if(len(ref) == 0.0)
				ref = axis;
			else
				ref = norm(ref);

			Vec3 revaxis = -1.0 * axis;

			//find the smallest difference axis with the current axis
			//pick that axis and go with it, but make sure to add to it
			diff = angle(axis, Vec3(0.0,0.0,0.0), ref);
			revdiff = angle(revaxis, Vec3(0.0,0.0,0.0), ref);
			if(revdiff < diff) {
				axis = revaxis; add[i*molstep + 6 + 6] *= -1.0;
			}
			sum[i*molstep + 6 + 3] += axis[0];
			sum[i*molstep + 6 + 4] += axis[1];
			sum[i*molstep + 6 + 5] += axis[2];

			//ang
			if(add[i*molstep + 6 + 6] < 0.0)
				add[i*molstep + 6 + 6] += 2.0*PI;
			sum[i*molstep + 6 + 6] += add[i*molstep + 6 + 6] + 2.0*PI;

			//dih
			for(int j = 0; j < dih; j++) {
				if(add[i*molstep + 6 + 7 + j] < 0.0)
					add[i*molstep + 6 + 7 + j] += 2.0*PI;
				sum[i*molstep + 6 + 7 + j] += add[i*molstep + 6 + 7 + j] + 2.0*PI;
			}
		}

	}
	else
		return false;
	return true;
}
//ALSO A BROKEN FUNCTION
void vectordiv(vector<double> &sum, double val) {
	for(int i = 0; i < sum.size(); i++)
		sum[i] /= val;
}


//helper function for SQL commits
void GASP2struct::sqlbindCreate(sqlite3_stmt * stmt) {


	//bind the data
	string stemp = ID.toStr();
	sqlite3_bind_text(stmt,1,stemp.c_str(),stemp.size(),SQLITE_TRANSIENT);
	stemp = parentA.toStr();
	sqlite3_bind_text(stmt,2,stemp.c_str(),stemp.size(),SQLITE_TRANSIENT);
	stemp = parentB.toStr();
	sqlite3_bind_text(stmt,3,stemp.c_str(),stemp.size(),SQLITE_TRANSIENT);
	sqlite3_bind_int(stmt,4,generation);
	sqlite3_bind_int(stmt,5,version);
	sqlite3_bind_double(stmt, 6, energy);
	sqlite3_bind_double(stmt, 7, force);
	sqlite3_bind_double(stmt, 8, pressure);
	sqlite3_bind_int(stmt, 9, unit.spacegroup);
	sqlite3_bind_int(stmt, 10, cluster);
	sqlite3_bind_int(stmt, 11, contacts);
	sqlite3_bind_double(stmt, 12, pseudoenergy);
	sqlite3_bind_int(stmt, 13, getAxisInt(unit.axn));
	sqlite3_bind_int(stmt, 14, getSchoenfliesInt(unit.typ)); //this might be a bad idea, consider text
	sqlite3_bind_double(stmt, 15, unit.cen);
	sqlite3_bind_double(stmt, 16, unit.sub);

	int state=0;
	if(complete) state=1;
	sqlite3_bind_int(stmt, 17, state);
	sqlite3_bind_int(stmt, 18, structErrToInt(finalstate));
	sqlite3_bind_int(stmt, 19, time);
	sqlite3_bind_int(stmt, 20, steps);
	sqlite3_bind_double(stmt, 21, unit.a);
	sqlite3_bind_double(stmt, 22, unit.b);
	sqlite3_bind_double(stmt, 23, unit.c);
	sqlite3_bind_double(stmt, 24, unit.alpha);
	sqlite3_bind_double(stmt, 25, unit.beta);
	sqlite3_bind_double(stmt, 26, unit.gamma);
	sqlite3_bind_double(stmt, 27, unit.ratA);
	sqlite3_bind_double(stmt, 28, unit.ratB);
	sqlite3_bind_double(stmt, 29, unit.ratC);
	sqlite3_bind_double(stmt, 30, unit.triA);
	sqlite3_bind_double(stmt, 31, unit.triB);
	sqlite3_bind_double(stmt, 32, unit.triC);
	sqlite3_bind_double(stmt, 33, unit.monoB);
	sqlite3_bind_double(stmt, 34, unit.rhomC);


	stemp = this->serializeXML();
	sqlite3_bind_text(stmt,35,stemp.c_str(),stemp.size(),SQLITE_TRANSIENT);

	//step in time, step in time
	sqlite3_step(stmt);

	//reset the bindings
	sqlite3_reset(stmt);

}

//helper for SQL commits
void GASP2struct::sqlbindUpdate(sqlite3_stmt * stmt) {

	string stemp;
	int ierr;
	//bind the data
	sqlite3_bind_double(stmt, 1, energy);
	sqlite3_bind_double(stmt, 2, force);
	sqlite3_bind_double(stmt, 3, pressure);

	int state=0;
	if(complete) state=1;
	sqlite3_bind_int(stmt, 4, state);
	sqlite3_bind_int(stmt, 5, structErrToInt(finalstate));
	sqlite3_bind_int(stmt, 6, time);
	sqlite3_bind_int(stmt, 7, steps);
	sqlite3_bind_double(stmt, 8, unit.a);
	sqlite3_bind_double(stmt, 9, unit.b);
	sqlite3_bind_double(stmt, 10, unit.c);
	sqlite3_bind_double(stmt, 11, unit.alpha);
	sqlite3_bind_double(stmt, 12, unit.beta);
	sqlite3_bind_double(stmt, 13, unit.gamma);
	sqlite3_bind_double(stmt, 14, unit.ratA);
	sqlite3_bind_double(stmt, 15, unit.ratB);
	sqlite3_bind_double(stmt, 16, unit.ratC);
	sqlite3_bind_double(stmt, 17, unit.triA);
	sqlite3_bind_double(stmt, 18, unit.triB);
	sqlite3_bind_double(stmt, 19, unit.triC);
	sqlite3_bind_double(stmt, 20, unit.monoB);
	sqlite3_bind_double(stmt, 21, unit.rhomC);

	stemp = this->serializeXML();
	sqlite3_bind_text(stmt,22,stemp.c_str(),stemp.size(),SQLITE_TRANSIENT);

	stemp = ID.toStr();
	sqlite3_bind_text(stmt,23,stemp.c_str(),stemp.size(),SQLITE_TRANSIENT);

	//step in time, step in time
	ierr = sqlite3_step(stmt);
	//cout << "update step err: " << ierr << endl;

	//reset the bindings
	sqlite3_reset(stmt);

}


string getStructError(StructError finalstate) {
		string strtemp;
		if(finalstate == OKStruct)
			strtemp = "OKStruct";
		else if(finalstate == OptBadBond)
			strtemp = "OptBadBond";
		else if(finalstate == OptBadAng)
			strtemp = "OptBadAng";
		else if(finalstate == OptBadDih)
			strtemp = "OptBadDih";
		else if(finalstate == FitcellBadDih)
			strtemp = "FitcellBadDih";
		else if(finalstate == FitcellBadCell)
			strtemp = "FitcellBadCell";
		else if(finalstate == NoFitcell)
			strtemp = "NoFitcell";
		return strtemp;
}


int structErrToInt(StructError err) {
	switch(err) {
	case OKStruct:
		return 0;
	case OptBadBond:
		return 1;
	case OptBadAng:
		return 2;
	case OptBadDih:
		return 3;
	case FitcellBadDih:
		return 4;
	case FitcellBadCell:
		return 5;
	case NoFitcell:
		return 6;
	}
	return 0;
}


//AML: This whole binary format things is WAY too complicated.
//if I ever decide to send proper binary format data via MPI
//then maybe it will be relevent

//this should only be used on a ROOT structure
//otherwise weird things might happen
//void GASP2struct::makexmlformats() {
//	GASP2format form;
//
//
//
//	for(int i = 0; i < molecules.size(); i++) {
//		form.name=names[molecules[i].label];
//
//		tinyxml2::XMLPrinter pr(NULL);
//		pr.OpenElement("mgacformat");
//		pr.PushAttribute("name", form.name.c_str());
//		string pln = "";
//		pln += (names[molecules[i].atoms[molecules[i].p1].label] +" ");
//		pln += (names[molecules[i].atoms[molecules[i].p2].label] +" ");
//		pln += (names[molecules[i].atoms[molecules[i].p3].label] +" ");
//		pr.PushAttribute("plane",pln.c_str());
//		pr.PushAttribute("expectvol", molecules[i].expectvol);
//			for(int j = 0; j < molecules[i].atoms.size(); j++) {
//			pr.OpenElement("record");
//
//				pr.PushAttribute("i", j);
//
//				pr.PushAttribute("type", "atom");
//				pr.PushAttribute("title", names[molecules[i].atoms[j].label].c_str() );
//				pr.PushAttribute("elem",getElemName(molecules[i].atoms[j].type).c_str());
//			pr.CloseElement();
//			}
//			for(int j = 0; j < molecules[i].dihedrals.size(); j++) {
//			pr.OpenElement("record");
//
//				pr.PushAttribute("i", j);
//
//				pr.PushAttribute("title",names[molecules[i].dihedrals[j].label].c_str());
//				pln.clear();
//				pln += (names[molecules[i].atoms[molecules[i].dihedrals[j].a].label] +" ");
//				pln += (names[molecules[i].atoms[molecules[i].dihedrals[j].b].label] +" ");
//				pln += (names[molecules[i].atoms[molecules[i].dihedrals[j].c].label] +" ");
//				pln += (names[molecules[i].atoms[molecules[i].dihedrals[j].d].label] +" ");
//
//				pr.PushAttribute("angle",pln.c_str());
//
//				pln.clear();
//				for(int k = 0; k < molecules[i].dihedrals[j].update.size(); k++)
//					pln += (names[molecules[i].atoms[molecules[i].dihedrals[j].update[k]].label] +" ");
//
//				pr.PushAttribute("update",pln.c_str());
//				pr.PushAttribute("min",deg(molecules[i].dihedrals[j].minAng));
//				pr.PushAttribute("max",deg(molecules[i].dihedrals[j].maxAng));
//
//
//			pr.CloseElement();
//			}
//			for(int j = 0; j < molecules[i].bonds.size(); j++) {
//			pr.OpenElement("record");
//
//			pr.CloseElement();
//			}
//			for(int j = 0; j < molecules[i].angles.size(); j++) {
//			pr.OpenElement("record");
//
//			pr.CloseElement();
//			}
//
//		pr.CloseElement();
//
//
//	}
//
//
//}


