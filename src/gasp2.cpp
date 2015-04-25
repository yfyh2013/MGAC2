#include "gasp2.hpp"

using namespace std;

//this constructor sets up the client
//side control scheme, and the placeholders
//for threads, hostfiles, and copy structures.

GASP2control::GASP2control(int ID, string infile) {

	this->ID = ID;

	//parse the infile
	tinyxml2::XMLDocument doc;
	string errorstring;
	if(!doc.LoadFile(infile.c_str()))
		parseInput(&doc, errorstring);
	else
		exit(1);

	//rgen is in gasp2common
	//kinda wonky, but is okay
	rgen.seed(params.seed+ID);

}

//this constructor is invoked for a server
//controller ONLY. the server acts as both client
//and server by dispatching pthreads in addition
//to managing the other clients
GASP2control::GASP2control(time_t start, int size, string input, string restart) {
	starttime = start;
	worldSize = size;
	this->restart = restart;
	infile = input;
	ID = 0;

	//parse the infile with output handling!
	tinyxml2::XMLDocument doc;
	string errorstring;
	doc.LoadFile(infile.c_str());
	if(doc.ErrorID() == 0) {
		if(parseInput(&doc, errorstring)==false) {
			cout << "There was an error in the input file: " << errorstring << endl;
			exit(1);
		}

	}
	else {
		cout << "!!! There was a problem with opening the input file!" << endl;
		cout << "Check to see if the file exists or if the XML file" << endl;
		cout << "is properly formed, with tags formatted correctly." << endl;
		cout << "Aborting... " << endl;
		exit(1);
	}


	//parse the restart if it exists

	rgen.seed(params.seed);

}

void GASP2control::getHostInfo() {

	FILE * p =  popen("cat /proc/cpuinfo | grep \"physical id\" | sort | uniq | wc -l","r");
	FILE * c =  popen("cat /proc/cpuinfo | grep \"core id\" | sort | uniq | wc -l","r");
	char p_str[10], c_str[10];

	fgets(p_str, 10, p);
	fgets(c_str, 10, c);

	stringstream ss;
	int phys, core;

	ss << p_str; ss >> phys; ss.clear();
	ss << c_str; ss >> core; ss.clear();

	nodethreads = phys*core;
	char _host[1024];
	gethostname(_host,1024);
	hostname = _host;

}

void GASP2control::server_prog() {

	getHostInfo();

	//collect the info from all hosts
	Host h; h.hostname = hostname;
	h.threads = nodethreads;
	hostlist.push_back(h);
	for(int i = 1; i < worldSize; i++) {
		recvHost(h.hostname, h.threads, i);
		hostlist.push_back(h);
	}

	//generate the local machinefile info
	UUID u; u.generate();
	string localmachinefile = "/tmp/machinefile-";
	localmachinefile.append(u.toStr());


	Instruction recv, send;
	//this vector indicates the owner of all nodes
	//so, if ownerlist[i] = 0, node i is owned by
	//the server thread
	//0-worldsize means the node is running
	//IDLE (-1) means the node is idle
	//DOWN (-2) means the node is in a bad state
	//and is not responding to pings
	vector<int> ownerlist(worldSize);
	//this vector contains a ping test for each node

	vector<bool> pinged(worldSize);
	//the polling time is 200 ms
	chrono::milliseconds t(200);
	chrono::milliseconds timeout(0);

	//initialize ownerlist
	ownerlist[0] = IDLE;

	//establish who is paying attention initially
	for(int i = 1; i < worldSize; i++) {
		//try to get a signal for ~5 seconds
		sendIns(Ping, i);
		cout << mark() << "Sending initial ping to client " << i << endl;
	}
	this_thread::sleep_for(chrono::seconds(1));
	for(int i = 1; i < worldSize; i++) {
		recvIns(recv, i);
		if(recv & Ackn) {
			cout << mark() << "Client " << i << " ack'd" << endl;
			ownerlist[i] = IDLE;
		}
		else
			ownerlist[i] = DOWN;

	}


	//do the initialization
	rootpop.init(root, params.popsize);
	int replace = static_cast<int>(static_cast<double>(params.popsize)*params.replacement);

	//calc the total number of processing elements
	int totalPE = 0;
	for(int i = 0; i < worldSize; i++) {
		totalPE += hostlist[i].threads;

	}


	//pick the calculation mode
	Instruction evalmode = None;
	if(params.calcmethod == "qe")
		evalmode = (Instruction) (evalmode | DoQE);
//	else if(params.calcmethod == "fitcell")
//		evalmode = (Instruction) (evalmode | DoFitcell);

	GASP2pop evalpop, lastpop;
	lastpop = rootpop;
	vector<GASP2pop> split;

	vector<future<bool>> popsend(worldSize);
	future<bool> eval;
	future<bool> writer;

	MPI_Status m;

//	cout << mark() << "fitcell starting" << endl;
//	evalpop = lastpop;
//	evalpop.addIndv(replace);
//	evalpop.runFitcell(nodethreads);
//	cout << mark() << "fitcell ending" << endl;
//
//
//	remove(localmachinefile.c_str());
//	for(int i = 1; i < worldSize; i++)
//		sendIns(Shutdown, i);
//
//	return;


	if(params.mode == "stepwise") {
		for(int step = 0; step < params.generations; step++) {
			//cross
			if(params.type == "elitism") {
				evalpop = lastpop;
				evalpop.addIndv(replace);
			}
			else if (params.type == "classic")
				evalpop = lastpop.fullCross();

			cout << mark() << "Crossover size: " << evalpop.size() << endl;
			cout << mark() << "Got to init!" << endl;
			//fitcell
			int count = evalpop.size() / totalPE;
			int remainder = evalpop.size() % totalPE;
			int remleft = remainder;
			int index = 0;
			split.clear();
			for(int i = 0; i < worldSize; i++) {
				int subsize = hostlist[i].threads * count;
				if(remleft < hostlist[i].threads) {
					subsize += remleft;
					remleft = 0;
				}
				else {
					subsize += hostlist[i].threads;
					remleft -= hostlist[i].threads;
				}
				//cout << "subsize: " << subsize << endl;
				split.push_back(evalpop.subpop(index, subsize));
				index += subsize;
			}
			cout << mark() << "Pop sizes: ";
			for(int i = 0; i < worldSize; i++)
				cout << split[i].size() << " ";
			cout << endl;
			cout << mark() << "Sending pops!" << endl;
			//send populations for fitcell


			//because of how MPI handles order of messages
			//blocking sends from the master thread must be issued
			//in the thread and not-asynchronously. multiple sends
			//issued in multiple threads creates potential for a
			//race condition, because the messages dispatched from
			//the master thread are out of order
			for(int i = 1; i < worldSize; i++) {
				sendIns(PopAvail, i);
				sendPop(split[i], i);
			}

			cout << mark() << "Performing fitcell!" << endl;
			//do the Fitcell;
			for(int i = 1; i < worldSize; i++) {
				sendIns(DoFitcell, i);
				ownerlist[i] = i;
			}
			split[0].runFitcell(hostlist[0].threads);

			cout << mark() << "Fitcell complete!" << endl;

			bool complete = false;
			for(int i = 1; i < worldSize; i++)
				pinged[i] = false;

//			while(!complete) {
//				//ping
//				int recvd;
//				//cout << mark() << "Trying a ping!" << endl;
//				for(int i = 1; i < worldSize; i++) {
//					MPI_Iprobe(0, CONTROL, MPI_COMM_WORLD, &recvd, &m);
//					if(recvd){
//						recvIns(recv, i);
//						cout << mark() << "Received message " << recv << " from " << i << endl;
//						if(recv & PopAvail) {
//							if(!popsend[i].valid())
//								popsend[i] = async(launch::async, &GASP2control::recvPop, this, &split[i], i);
//							if(recv & Busy)
//								ownerlist[i] = i;
//						}
//						MPI_Iprobe(0, CONTROL, MPI_COMM_WORLD, &recvd, &m);
//					}
//				}
//				for(int i = 1; i < worldSize; i++) {
//					if(popsend[i].valid() && popsend[i].wait_for(timeout)==future_status::ready) {
//						popsend[i].get();
//						ownerlist[i] = IDLE;
//					}
//					if(pinged[i] == false)
//						sendIns(Ping, i);
//					if(ownerlist[i] != IDLE)
//						complete = complete & true;
//					else
//						complete = false;
//				}
//				this_thread::sleep_for(t);
//			}

				//collect
			this_thread::sleep_for(chrono::seconds(10000));

//				busy = false;
//				for(int i = 1; i < worldSize;i++)
//					sendIns(Ping, i);
//
//				for(int i = 1; i < worldSize;i++) {
//					MPI_Iprobe(0, CONTROL, MPI_COMM_WORLD, recvd, &m);
//						recvIns(recv, i);
//						if(recv & Busy)
//							busy = true;
//				}
//
//				this_thread::sleep_for(t);
//			}

			cout << "Done!" << endl;


			//evaluate




			//sort, scale, and reduce
			if(params.type == "elitism") {

			}
			else if (params.type == "classic") {

			}


			//save pops

		}

	}
	else if(params.mode == "steadystate") {
		cout << "Steadystate not yet implemented" << endl;
	}


//	ofstream outf;
//	outf.open(localmachinefile.c_str(), ofstream::out);
//	if(outf.fail()) {
//		cout << "Could not write machinefile!\n";
//		exit(1);
//	}
//	string name = makeMachinefile({0,1});
//	outf << name <<endl;
//	outf.close();
//	rootpop.init(root, params.popsize);
//	GASP2pop temp = rootpop.newPop(1);
//
//	temp.runFitcell(8);
//	temp.volumesort();
//
//	string hosts("blahblahblahblah");
//	temp.remIndv(temp.size() - 1);
//	temp.writeCIF("pre.cif");
//	temp.runEval(localmachinefile, params, QE::runQE);
//	temp.indv(0)->forceOK();
//	temp.writeCIF("post.cif");

	//clean up files
	remove(localmachinefile.c_str());
	for(int i = 1; i < worldSize; i++)
		sendIns(Shutdown, i);

}


bool GASP2control::runEvals(Instruction i, GASP2pop p, string machinefilename) {

	//get the hostfile stuff
//	string machinefile; int junk;
//	recvHost(machinefile, junk, 0);
//	ofstream outf;
//	outf.open(machinefilename.c_str(), ofstream::out);
//	if(outf.fail()) {
//		cout << "Could not write machinefile!\n";
//		exit(1);
//	}
//	outf << machinefile << endl;
//	outf.close();

	//only one of these will execute
	if(i & DoFitcell) {
		//cout << "runnign fitcell, ID " << ID << endl;
		p.runFitcell(nodethreads);
	}
	if(i & DoCharmm)
		cout << "Charmm not implemented!" << endl;
	if(i & DoQE)
		p.runEval(machinefilename, params, QE::runQE);
	if(i & DoCustom)
		cout << "Custom eval not implemented!" << endl;

	cout << "eval finished on client " << ID << endl;
	return true;

}

void GASP2control::client_prog() {

	getHostInfo();
	//cout << "client prog: " << ID << endl;
	sendHost(hostname, nodethreads, 0);

	UUID u; u.generate();
	string localmachinefile = "/tmp/machinefile-";
	localmachinefile.append(u.toStr());


	MPI_Status m;
	bool busy = false;
	int recvd;
	GASP2pop evalpop;
	Instruction i, ack;
	future<bool> popsend, eval;
	chrono::milliseconds timeout(0);
	chrono::milliseconds t(200);


	int target = 0;

	while(true) {
		i = None; ack = None;

		MPI_Iprobe(0, CONTROL, MPI_COMM_WORLD, &recvd, &m);
		while(recvd) {
			recvIns(i, 0);

			//cout << "client received: " << i << endl;
			if(i & Shutdown) {
				remove(localmachinefile.c_str());
				break;
			}
			if(i & SendPop) {
				if(!popsend.valid()) {
					popsend = async(launch::async, &GASP2control::sendPop, this, evalpop, 0);
					ack = (Instruction) (ack | PopAvail);
				}
			}
			if(i & Ping){
				ack = (Instruction) (ack | Ackn);
				if(busy)
					ack = (Instruction) (ack | Busy);
			}
			if((i & PopAvail) && (busy==false) ) {
				//if(!popsend.valid()) {
				//	popsend = async(launch::async, &GASP2control::recvPop, this, &evalpop, 0);
				//}
				recvPop(&evalpop, 0);
			}
			if( (i & (DoFitcell | DoCharmm | DoQE | DoCustom)) && (busy == false)){
//				if(!eval.valid()) {
//					eval = async(launch::async, &GASP2control::runEvals, this, i, evalpop, localmachinefile);
//					busy = true;
//				}
//				else {
//					cout << mark() << "ERROR ON CLIENT " << ID << ": COULD NOT LAUNCH EVAL THREAD!" << endl;
//					MPI_Abort(MPI_COMM_WORLD,1);
//				}
				//evalpop.runFitcell(nodethreads);
				GASP2pop temptemp = evalpop;
				temptemp.runFitcell(nodethreads);
				cout << mark() << "client " << ID << " finished fitcell" << endl;

			}
			//cout << "client sending: " << ack << endl;
			//only send a message if there's actually a message to send
			if(ack != None)
				sendIns(ack,0);

			MPI_Iprobe(0, CONTROL, MPI_COMM_WORLD, &recvd, &m);
		}

		//cleanup threads
		if(popsend.valid() && popsend.wait_for(timeout)==future_status::ready) {
			popsend.get();
		}

		if(eval.valid() && eval.wait_for(timeout)==future_status::ready) {
			cout << "client test" << endl;
			eval.get();
			busy = false;
			//if the popsend thread is already working, wait until it's done
			if(popsend.valid())
				popsend.wait();
			//send the pop when clear
			if(!popsend.valid()) {
				popsend = async(launch::async, &GASP2control::sendPop, this, evalpop, 0);
			}
			//notify the main host
			sendIns(PopAvail, 0);
			//wait until the pop is received
			if(popsend.valid())
				popsend.wait();

		}


        this_thread::sleep_for(t);
	};

}



bool GASP2control::parseInput(tinyxml2::XMLDocument *doc, string& errors) {

	//pass the doc to gasp2param
	if(params.parseXML(doc, errors)==false)
		return false;
	if(root.parseXMLDoc(doc, errors)==false)
		return false;

	if(ID == 0) {
		params.logParams();
		root.logStruct();
	}
	return true;

}

bool GASP2control::sendHost(string host, int procs, int target) {
	int t = host.length();
	//send size info
	MPI_Send(&t, 1, MPI_INT, target,HOSTS,MPI_COMM_WORLD);

	//send info
	MPI_Send(host.c_str(),t,MPI_CHAR,target,HOSTS,MPI_COMM_WORLD);
	//send proc info
	MPI_Send(&procs, 1, MPI_INT, target, HOSTS, MPI_COMM_WORLD);
	return true;
}

bool GASP2control::recvHost(string &host, int &procs, int target) {
	int ierr;
	int v = 0;
	//recv size info
	MPI_Recv(&v, 1, MPI_INT, target,HOSTS,MPI_COMM_WORLD, MPI_STATUS_IGNORE);
	char * buff = new char[v];
	//recv hostname
	MPI_Recv((void *) buff, v, MPI_CHAR, target,HOSTS,MPI_COMM_WORLD,MPI_STATUS_IGNORE);
	host = buff;
	//recv proc count

	MPI_Recv(&procs, 1, MPI_INT, target, HOSTS, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
	//cout << "procs:" << procs << endl;
	delete [] buff;

	return true;
}


bool GASP2control::sendPop(GASP2pop p, int target) {
	string pop = p.saveXML();
	//cout << mark() << " Sendpop " << target << " launched, ID " << ID << endl;

	int t = pop.length();
	//send size info
	MPI_Send(&t, 1, MPI_INT, target,POP,MPI_COMM_WORLD);

	//send info
	MPI_Send(pop.c_str(),t,MPI_CHAR,target,POP,MPI_COMM_WORLD);

	return true;
}

bool GASP2control::recvPop(GASP2pop *p, int target) {
	string pop;
	int v = 0;
	//cout << mark() << " Recvpop " << target << " launched, ID " << ID << endl;
	//recv size info
	MPI_Recv(&v, 1, MPI_INT, target,POP,MPI_COMM_WORLD, MPI_STATUS_IGNORE);
	char * buff = new char[v];
	//recv hostname
	MPI_Recv((void *) buff, v, MPI_CHAR, target,POP,MPI_COMM_WORLD,MPI_STATUS_IGNORE);
	pop = buff;

	delete [] buff;

	string error;
	p->parseXML(pop,error);

	return true;
}


bool GASP2control::sendIns(Instruction i, int target) {
	//cout << "sending ins to target " << target << endl;

	MPI_Request r;
	MPI_Isend(&i, 1, MPI_INT, target, CONTROL, MPI_COMM_WORLD, &r);
	return true;
}

bool GASP2control::recvIns(Instruction &i, int target) {
	//cout << "receiving ins from target " << target << endl;
	MPI_Request r;
	MPI_Irecv(&i, 1, MPI_INT, target, CONTROL, MPI_COMM_WORLD, &r);
	//cout << "procs:" << procs << endl;

	return true;
}


string GASP2control::makeMachinefile(vector<int> slots) {
	stringstream out;
	out.str("");

	for(int i = 0; i < slots.size(); i++) {
		if(slots[i] < hostlist.size()) {
			for(int j = 0; j < hostlist[slots[i]].threads; j++)
				out << hostlist[slots[i]].hostname << endl;
		}
	}
	return out.str();
}
