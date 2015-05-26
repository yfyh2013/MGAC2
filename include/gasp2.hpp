#pragma once
#include "gasp2common.hpp"
#include "gasp2param.hpp"
#include "gasp2struct.hpp"
#include "gasp2pop.hpp"
#include "gasp2qe.hpp"


using namespace std;


/*
 * GASP2 scheduling model
 *
 * 1) Workflow
 * -The server and clients are initialized.
 * -Both server and the clients read the input file.
 * -The server generates a new population from the
 * root structure definition. This becomes the rootpop.
 * -The server fitcells the rootpop, then forks off
 * structures that pass the volume constraints
 * -The forked structures are sent for eval. Size of
 * structure is taken into account by tiling on nodes.
 * -A new set of structures is generated by crossing.
 * -New pop is fitcelled, volume limited, and eval'd.
 * -Elitism filter is applied (or not, depending on mode).
 * -Repeat until limits reached.
 *
 * 2) Send/Receive Instructions
 * Instruction send/receive is handle my the main thread
 * on all clients. Every 15 seconds both clients and server
 * check for incoming messages. If there is an incoming message,
 * it is received and handled.
 *
 * Instructions are bitmasks, so multiple instructions can be sent at once.
 *
 *
 * The priority of execution is as follows:
 * a) Prepare send for pop
 * b) Send pingback with Ack/PopAvail/Busy
 * c) DoFitcell
 * d) DoCharmm
 * e) DoQE
 * f) DoCustom
 *
 * So, a bitmask of 11101010 would first fork a thread for the Pop send,
 * send 000010111 (for PopAvail, Acknowledging ping, and Busy), then
 * execute Fitcell, Charmm, QE, and then the Custom, in that order.
 *
 * All instructions are sent through a separate thread so that
 * incoming message checks are non-blocking on the main thread.
 * outgoing messages are kept in place until the receiver picks it up.
 *
 * 3) Send/Receive Structure
 *
 * Structures are sent via xml or (eventually) HDF5 files. These are
 * interpreted by the receiver. By packaging the message as a file
 * development time is saved if future features are implemented.
 *
 * The Send/Receive is carried out on separate threads on both sender
 * and receiver. The main thread waits until a mutex is flagged to indicate
 * the transfer is complete.
 *
 * 4) Fitcell/Optimization scheduling
 *
 * Fitcell is spread across as many threads as there are physical processors.
 * So, a 12 core node will have 12 threads. The threads a C11 threads,
 * but since the calculation has deterministic time (I am not aware of any
 * situation where the thread would not complete) then the threads do not
 * need to be terminated
 *
 * Optimization for QE is accomplished by receiving a machinefile from the
 * server along with a single member population, and then dispatching a thread.
 * This thread calls a special version of popen which also returns the
 * subprocess assigned to QE. Then, as QE executes, the output pipe is read
 * periodically by the thread, enabling real time data collection and eliminating
 * the need to save output files. Should the need arise that the QE process
 * needs to be killed, the PID is available for a kill by the thread. This
 * will solve the problem of QE exiting but the PID remaining active, which
 * leads to a stuckl state. This also allows for termination based on time
 * restrictions.
 *
 *
 *
 */

#define POP 0
#define HOSTS 1
#define CONTROL 2
#define OTHER 3

#define IDLE -1
#define DOWN -2


typedef struct Host {
	string hostname;
	int threads;
}Host;

typedef enum Instruction {
	None = (0u),
	Ackn = (1u<<0), //acknowledge
	Ping = (1u<<1), //message sent by server to test readiness
	Busy = (1u<<2), //sent by client when busy with work; a 0 implies Ready
	SendPop = (1u<<3), 	//server order to client to send pop (regardless of eval state)
	PopAvail = (1u<<4), //sent when a population is queued and ready for transmit
	DoFitcell = (1u<<5), //order to client to perform fitcell
	DoCharmm = (1u<<6), //reserved for future usage, not implemented
	DoQE = (1u<<7), //order to client to perform QE
	DoCustom = (1u<<8), //order to client to perform custom eval
	Shutdown = (1u<<9), //Send a shutdown signal for cleanup n stuff
	GetHost = (1u<<10),

}Instruction;

extern std::mutex eval_mut;
extern std::mutex longeval_mut;

class GASP2control {
public:
	GASP2control(int ID, string infile);
	GASP2control(time_t start, int size, string input, string restart="");
	void server_prog();
	void client_prog();

private:
	//procedural variables
	time_t starttime;
	string infile;
	string restart;
	int worldSize;
	int ID;

	string hostname;
	int nodethreads;

	GASP2param params;
	GASP2struct root; //base structure which all other structures are derived from
	GASP2pop rootpop; //masterstructure list
	vector<GASP2pop> popbuff;

	vector<Host> hostlist;

	//control instructions
	bool sendIns(Instruction i, int target);
	bool recvIns(Instruction &i, int target);

	//population send/recv
	bool sendPop(GASP2pop p, int target);
	bool recvPop(GASP2pop *p, int sender);

	//for sending MPI host info between nodes
	//can also be used for sending strings in general
	bool sendHost(string host, int procs, int target);
	bool recvHost(string &host, int &procs, int target);

	bool runEvals(Instruction i, GASP2pop p, string machinefilename);

	string makeMachinefile(vector<int> slots);

	void getHostInfo();
	void writeHost(string name, string data);

	bool writePop(GASP2pop pop, string tag, int step);

	bool parseInput(tinyxml2::XMLDocument *doc, string & errorstring);

	//string mark();

};
