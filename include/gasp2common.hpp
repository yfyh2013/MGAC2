#pragma once
#include <iostream>
#include <iomanip>
#include <vector>
#include <fstream>
#include <sstream>
#include <string>
#include <unistd.h>
#include <ctime>
#include <chrono>
#include <stdio.h>
#include <stdlib.h>
#include "tinyxml2.h"
#include "svl/SVL.h"
#include "mpi.h"
#include <uuid/uuid.h>
#include <random>

extern "C" {
	extern char _binary_spacegroups_xml_start;
	extern char _binary_spacegroups_xml_end;
}




#define PI 3.1415926535897932384626433

using namespace std;

extern tinyxml2::XMLDocument spacegroups;
extern vector<string> spacegroupNames;
extern mt19937_64 rgen;

typedef unsigned int Index;
typedef unsigned int NIndex;


typedef enum struct Elem {
	UNK=0,C,H,N,O,P,Cl,F,S,Br,
}Elem;

typedef enum struct Schoenflies {
	Cn, Cnv, Cnh, Sn, Dn, Dnd, Dnh,
	T, Th, O, Td, Oh, UNK=0
}Schoenflies;

typedef enum struct Axisnum {
	One, Two, Three, Four, Six, UNK=0
}Axisnum;

typedef enum Spacemode {
	Limited=0, //Excludes Tetrahedral/Octahedral schoenflies and F centerings
	Single, //Single spacegroup only
	Full, //All spacegroups (including T,O and F)
}Spacemode;

typedef double Centering; //encodes P C/A/B/N/D/E I, F, R, H depending on schoenflies/axis
typedef double Subtype; //encodes glide and screw axes based on group number





double angle ( const Vec3 & A, const Vec3 & B, const Vec3 & C );

double dihedral ( const Vec3 & A, const Vec3 & B, const Vec3 & C, const Vec3 & D );

double rcov( const Elem type );

double vdw( Elem type );

Elem getElemType(string in);

string getElemName(Elem in);

Axisnum getAxis(string in);
string getAxis(Axisnum in);

Schoenflies getSchoenflies(string in);
string getSchoenflies(Schoenflies in);

vector<string> split(string in, char delim=' ');

bool loadSpaceGroups();

string tfconv(bool var);

//wrapper for libuuid
class UUID {
private:
        uuid_t uuid;
public:
        UUID(void);
        UUID(const UUID &u);
        void generate();
        void clear();
        string toStr();
        void frStr(string in);
        UUID& operator=(UUID u);
        friend bool operator==(const UUID &u, const UUID &v);
};


