/* ---------------------------------------------------------------
Práctica 1.
Código fuente: manfut.c
Grau Informàtica
49259953W i Sergi Puigpinós Palau.
47694432E i Jordi Lazo Florensa.
--------------------------------------------------------------- */
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <math.h>
#include <limits.h>
#include <pthread.h>
#include <semaphore.h>
#include "manfut.h"

#define GetPorter(j) (Jugadors[j])
#define GetDefensor(j) (Jugadors[NPorters+j])
#define GetMitg(j) (Jugadors[NPorters+NDefensors+j])
#define GetDelanter(j) (Jugadors[NPorters+NDefensors+NMitjos+j])

char *color_red = "\033[01;31m";
char *color_green = "\033[01;32m";
char *color_blue = "\033[01;34m";
char *end_color = "\033[00m";

// Definition functions prototype
void LlegirFitxerJugadors(char *pathJugadors);
void CalcularEquipOptim(long int PresupostFitxatges, PtrJugadorsEquip MillorEquip, int numOfThreads);
TJugadorsEquip* evaluateThreadFunc(void* arguments);
TBoolean ObtenirJugadorsEquip (TEquip equip, PtrJugadorsEquip jugadors);
TEquip GetEquipInicial();
TBoolean JugadorsRepetits(TJugadorsEquip jugadors);
int CostEquip(TJugadorsEquip equip);
int PuntuacioEquip(TJugadorsEquip equip);
void error(char *str);
unsigned int Log2(unsigned long long int n);
void PrintJugadors();
void PrintEquipJugadors(TJugadorsEquip equip);

// Global variables definition
TJugador Jugadors[DMaxJugadors];
int NJugadors, NPorters, NDefensors, NMitjos, NDelanters;
char cad[256];

//Shared variables
TJugadorsEquip MillorEquip;
int MaxPuntuacio=-1;

struct threadsArg{
	TEquip first;
	TEquip end;
	long int PresupostFitxatges;
};

struct Tstatistics{
    int numComb = 0;
    int numInvComb = 0;
    int numValidComb = 0;
    int avgCostValidComb = 0;
    int avgScoreValidComb = 0;
    TJugadorsEquip bestCombination;
    int bestScore = 0;
    TJugadorsEquip worseCombination;
    int worseScore = 0;
};

int main(int argc, char *argv[])
{
	TJugadorsEquip MillorEquip, AuxEquip;
	long int PresupostFitxatges;
	int numOfThreads;
	float IntervalBegin=-1, IntervalEnd=-1;
	
	if (argc<3) 
		error("Error in arguments: ManFut <presupost> <fitxer_jugadors> <num_threads>");

	if (argc>1)
		PresupostFitxatges = atoi(argv[1]);
	
	if (argc>2)
		LlegirFitxerJugadors(argv[2]);

	numOfThreads = atoi(argv[3]);
	if (numOfThreads <= 0)
		error("Invalid number of Threads");

	//TODO define M

	// Calculate the best team.
	CalcularEquipOptim(PresupostFitxatges, &MillorEquip, numOfThreads);
	write(1,color_blue,strlen(color_blue));
	write(1,"-- Best Team -------------------------------------------------------------------------------------\n",strlen("-- Best Team -------------------------------------------------------------------------------------\n"));
	PrintEquipJugadors(MillorEquip);
	sprintf(cad,"   Cost %d, Points: %d.\n", CostEquip(MillorEquip), PuntuacioEquip(MillorEquip));
	write(1,cad,strlen(cad));
	write(1,"-----------------------------------------------------------------------------------------------------\n",strlen("-----------------------------------------------------------------------------------------------------\n"));
	write(1,end_color,strlen(end_color));

	exit(0);
}


// Read file with the market players list (each line containts a plater: "Id.;Name;Position;Cost;Team;Points")
void LlegirFitxerJugadors(char *pathJugadors)
{
	char buffer[256], tipus[10];
	int fdin;
	int nofi;
	
	if ((fdin=open(pathJugadors, O_RDONLY)) < 0) 
		error("Error opening input file.");
	
	// Read players.
	NJugadors=NPorters=NDefensors=NMitjos=NDelanters=0;
	do
	{
		int x=0,i,f;

		while((nofi=read(fdin,&buffer[x],1))!=0 && buffer[x++]!='\n');
		buffer[x]='\0';
		
		if (buffer[0]=='#') continue;
		
		// Player's identificator
		i=0;
		for (f=0;buffer[f]!=';';f++);
		buffer[f]=0;
		Jugadors[NJugadors].id = atoi(&(buffer[i]));

		// Player's name
		i=++f;
		for (;buffer[f]!=';';f++);
		buffer[f]=0;
		strcpy(Jugadors[NJugadors].nom,&(buffer[i]));

		// Player's position
		i=++f;
		for (;buffer[f]!=';';f++);
		buffer[f]=0;
		if (strcmp(&(buffer[i]),"Portero")==0)
		{
			NPorters++;
			Jugadors[NJugadors].tipus=JPorter;
		}
		else if (strcmp(&(buffer[i]),"Defensa")==0)
		{
			NDefensors++;
			Jugadors[NJugadors].tipus=JDefensor;
		}
		else if (strcmp(&(buffer[i]),"Medio")==0)
		{
			NMitjos++;
			Jugadors[NJugadors].tipus=JMitg;
		}
		else if (strcmp(&(buffer[i]),"Delantero")==0)
		{
			NDelanters++;
			Jugadors[NJugadors].tipus=JDelanter;
		}
		else error("Error player type.");
		

		// Player's cost
		i=++f;
		for (f=0;buffer[f]!=';';f++);
		buffer[f]=0;
		Jugadors[NJugadors].cost = atoi(&(buffer[i]));
		
		// Player's team
		i=++f;
		for (f=0;buffer[f]!=';';f++);
		buffer[f]=0;
		strcpy(Jugadors[NJugadors].equip,&(buffer[i]));

		// Player's points
		i=++f;
		for (f=0;buffer[f]!='\n';f++);
		buffer[f]=0;
		Jugadors[NJugadors].punts = atoi(&(buffer[i]));
		
		NJugadors++;
	}
	while(nofi);
	
	sprintf(cad,"Number of players: %d, Port:%d, Def:%d, Med:%d, Del:%d.\n",NJugadors, NPorters, NDefensors, NMitjos, NDelanters);
	write(1,cad,strlen(cad));
	
	close(fdin);
}	




//TODO ******************************************************************************************

//TODO includes

//Messenger shared variables
int LIST_SIZE = 100;
char *messageArray[LIST_SIZE];
bool bForcePrint = false;
int messageArrayIndx = 0;

//Mutex & locks
pthread_mutex_t checkTeamLock = PTHREAD_MUTEX_INITIALIZER; 
pthread_mutex_t messengerArrayLock = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t itemAdded = PTHREAD_COND_INITIALIZER;
pthread_mutex_t messengerSemMutex = PTHREAD_MUTEX_INITIALIZER;



void CalcularEquipOptim(long int PresupostFitxatges, PtrJugadorsEquip MillorEquip, int numOfThreads)
{
	unsigned int maxbits;
	TEquip equip, primerEquip, ultimEquip, first, end;
	struct threadsArg *args[numOfThreads];	
	pthread_t TArray[numOfThreads];
	
	// Calculated number of bits required for all teams codification.
	maxbits=Log2(NPorters)*DPosPorters+Log2(NDefensors)*DPosDefensors+Log2(NMitjos)*DPosMitjos+Log2(NDelanters)*DPosDelanters;
	if (maxbits>Log2(ULLONG_MAX))
		error("The number of player overflow the maximum width supported.");

	// Calculate first and end team that have to be evaluated.
	first=primerEquip=GetEquipInicial();
	end=ultimEquip=pow(2,maxbits);

	sprintf(cad, "MAIN; First: %lld ; End: %lld \n", first, end);
	write(1,cad,strlen(cad));

	// Evaluating different teams/combinations.
	sprintf (cad,"Evaluating form %llXH to %llXH (Maxbits: %d). Evaluating %lld teams...\n",first,end, maxbits,end-first);
	write(1,cad,strlen(cad));

	int remaining = ultimEquip-primerEquip;

	//Initialize mutex
	//TODO if returns -1
	pthread_mutex_init(&checkTeamLock, NULL); 
	pthread_mutex_init(&messengerArrayLock, NULL); 
	sem_init(&messengerSemMutex, 0, LIST_SIZE); 

	//Start threads
	for (int i=0; i<numOfThreads; ++i){	
		args[i] = malloc(sizeof(struct threadsArg));
		int pivot = remaining / (numOfThreads-i);
		int prevThreadEnd = i==0 ? first : args[i-1]->end+1;
		int threadEnd = i==numOfThreads-1 ? end : prevThreadEnd+pivot;	
		args[i]->first = prevThreadEnd;	
		args[i]->end = threadEnd;		
		args[i]->PresupostFitxatges = PresupostFitxatges;		
		if (pthread_create(&TArray[i], NULL, (void *)evaluateThreadFunc, (void *)args[i]) != 0)
			error("Error on creating pthread");
		remaining -= pivot;		
	}

	//Join threads and get the data from each one
	TJugadorsEquip *retV;
	int max = 0;
	for (int i=0; i<numOfThreads; ++i){		
		if (pthread_join(TArray[i], (void *) &retV) != 0){
			//Cancel threads
			for (int i = 0; i < numOfThreads; i++)
			{
				if (TArray[i]!=0)
					pthread_cancel(TArray[i]);
			}	
		}
		if (i==0 || PuntuacioEquip(*retV) > max){
			memcpy(MillorEquip,retV,sizeof(TJugadorsEquip));	
			max = PuntuacioEquip(*MillorEquip);
		}
	}		
	free(retV);
}



TJugadorsEquip* evaluateThreadFunc(void* arguments)
{
	struct threadsArg *args = arguments;
	struct Tstatistics statistics;
	TEquip equip;

	//sprintf(cad, "Thread: %d ; First: %lld ; End: %lld \n", getpid(), args->first, args->end);	
	//write(1,cad,strlen(cad));

	for (equip=args->first;equip<=args->end;equip++)
	{
		TJugadorsEquip jugadors;
		
		// Get playes from team number. Returns false if the team is not valid.
		if (!ObtenirJugadorsEquip(equip, &jugadors))
			continue;
		
		statistics.numComb++;
		
		// Reject teams with repeated players.
		if (JugadorsRepetits(jugadors))
		{
			statistics.numInvComb++;
			//sprintf(cad,"%s Invalid.\r%s", color_red, end_color);
			//write(1,cad,strlen(cad));
			continue;	// Equip no valid.
		}
		
		int costEquip = CostEquip(jugadors);
		int puntuacioEquip = PuntuacioEquip(jugadors);

		
		pthread_mutex_lock(checkTeamLock);
		checkTeam(equip, jugadors, args->PresupostFitxatges, costEquip, puntuacioEquip);
		pthread_mutex_unlock(checkTeamLock);

		//TODO fill parameters
		calculateStatistics();

		if (statistics.numComb%M == 0)
            printStatistics();

	}

	//TODO implement end / use barrier? 


	free(arguments);
}

void checkTeam(TEquip equip, TJugadorsEquip jugadors, int PresupostFitxatges, int costEquip, int puntuacioEquip){
	// Chech if the team points is bigger than current optimal team, then evaluate if the cost is lower than the available budget
	if (puntuacioEquip>MaxPuntuacio && costEquip<PresupostFitxatges)
	{
		// We have a new partial optimal team.
		MaxPuntuacio=puntuacioEquip;
		//memcpy(ThreadMillorEquip,&jugadors,sizeof(TJugadorsEquip));
		MillorEquip=jugadors;
		sprintf(cad,"%s Team %lld -> Cost: %d  Points: %d. %s\n", color_green, equip, costEquip, puntuacioEquip, end_color);
		write(1,cad,strlen(cad));
	}
	else
	{
		sprintf(cad,"Team %lld -> Cost: %d  Points: %d. \r", equip, costEquip, puntuacioEquip);
		write(1,cad,strlen(cad));
	}
}

void calculateStatistics(TJugadorsEquip jugadors, int costEquip, int puntuacioEquip, struct Tstatistics statistics) {
	statistics.avgCostValidComb = ((statistics.avgCostValidComb * statistics.numValidComb) + costEquip) / (statistics.numValidComb+1);
	statistics.avgScoreValidComb = ((statistics.avgScoreValidComb * statistics.numValidComb) + puntuacioEquip) / (statistics.numValidComb+1);
	statistics.numValidComb++;
	if (puntuacioEquip > statistics.bestScore){    //Best combination
		statistics.bestScore = puntuacioEquip;
		statistics.bestCombination = jugadors;
	}else if (statistics.worseScore == 0 || puntuacioEquip < statistics.worseScore) {   //Worse combination
		statistics.worseScore = puntuacioEquip;
		statistics.worseCombination = jugadors;
	}
}



//Messenger thread







void printMessages(){
	for (String s : messageList) {
		System.out.println(s);
	}
	messageList.clear();
}

void addMessageToQueue(char* message) {
	sem_wait(&messengerSemMutex)
	pthread_mutex_lock(&messengerArrayLock); 
	//TODO possible crash
	messageArray[messageArrayIndx] = message;
	messageArrayIndx++;
	pthread_cond_signal(&itemAdded);
	pthread_mutex_unlock(&messengerArrayLock);
	
}

public static void forcePrint(){
	//Lock to avoid changing the boolean while the messenger thread is resetting its value
	try {
		messengerLock.lock();
		bForcePrint=true;
		itemAdded.signalAll();
	} finally {
		messengerLock.unlock();
	}
}


void messengerThread(){
	while (true){
		pthread_mutex_lock(&messengerArrayLock); 
		//Wait until list is full
		while (messageArrayIndx != LIST_SIZE && !bForcePrint)
			pthread_cond_wait(&itemAdded, &messengerArrayLock);
		//TODO check it returns the desired value
		int remaining = sem_getvalue(&messengerSemMutex);   //In case of a forcePrint
		printMessages();
		//Release
		for (int i = 0; i < LIST_SIZE-remaining; i++)
		{
			sem_post(&messengerSemMutex);
		}		
		bForcePrint=false;
		pthread_mutex_unlock(&messengerArrayLock);
	}
}









// Calculate the initial team combination.
TEquip 
GetEquipInicial()
{
	int p;
	TEquip equip=0, equip2=0;
	unsigned bitsPorters, bitsDefensors, bitsMitjos, bitsDelanters;
	
	bitsPorters = Log2(NPorters);
	bitsDefensors = Log2(NDefensors);
	bitsMitjos = Log2(NMitjos);
	bitsDelanters = Log2(NDelanters);	

	for (p=DPosDelanters-1;p>=0;p--)
	{
		equip+=p;
		equip = equip << bitsDelanters;
	}

	for (p=DPosMitjos-1;p>=0;p--)
	{
		equip+=p;
		equip = equip << bitsMitjos;
	}

	for (p=DPosDefensors-1;p>=0;p--)
	{
		equip+=p;
		equip = equip << bitsDefensors;
	}
	
	for (p=DPosPorters-1;p>0;p--)
	{
		equip+=p;
		equip = equip << bitsPorters;
	}

	return (equip);
}


// Convert team combinatio to an struct with all the player by position. 
// Returns false if the team is not valid.

TBoolean 
ObtenirJugadorsEquip (TEquip equip, PtrJugadorsEquip jugadors)
{
	int p;
	unsigned bitsPorters, bitsDefensors, bitsMitjos, bitsDelanters;
	
	bitsPorters = Log2(NPorters);
	bitsDefensors = Log2(NDefensors);
	bitsMitjos = Log2(NMitjos);
	bitsDelanters = Log2(NDelanters);
	
	for (p=0;p<DPosPorters;p++)		
	{
		jugadors->Porter[p]=(equip>>(bitsPorters*p)) & ((int)pow(2,bitsPorters)-1);
		if (jugadors->Porter[p]>=NPorters) 
			return False;
	}

	for (p=0;p<DPosDefensors;p++)	
	{
		jugadors->Defensors[p]=(equip>>((bitsPorters*DPosPorters)+(bitsDefensors*p))) & ((int)pow(2,bitsDefensors)-1);
		if (jugadors->Defensors[p]>=NDefensors) 
			return False;
	}
	
	for (p=0;p<DPosMitjos;p++)
	{
		jugadors->Mitjos[p]=(equip>>((bitsPorters*DPosPorters)+(bitsDefensors*DPosDefensors)+(bitsMitjos*p))) & ((int)pow(2,bitsMitjos)-1);
		if (jugadors->Mitjos[p]>=NMitjos) 
			return False;
	}
	
	for (p=0;p<DPosDelanters;p++)	
	{
		jugadors->Delanters[p]=(equip>>((bitsPorters*DPosPorters)+(bitsDefensors*DPosDefensors)+(bitsMitjos*DPosMitjos)+(bitsDelanters*p))) & ((int)pow(2,bitsDelanters)-1);
		if (jugadors->Delanters[p]>=NDelanters) 
			return False;
	}
	
	return True;
}


// Check if the team have any repeated player.
// Returns true if the team have repeated players.

TBoolean 
JugadorsRepetits(TJugadorsEquip jugadors)
{
	// Returns True if the equip have some repeated players (is not valid).
	int i,j;
	
	// Porters.
	for(i=0;i<DPosPorters-1;i++)
		for(j=i+1;j<=DPosPorters-1;j++)
			if (jugadors.Porter[i]==jugadors.Porter[j]) 
				return True;
			
	// Defensors.
	for(i=0;i<DPosDefensors-1;i++)
		for(j=i+1;j<=DPosDefensors-1;j++)
			if (jugadors.Defensors[i]==jugadors.Defensors[j]) 
				return True;

	// Mitjos.
	for(i=0;i<DPosMitjos-1;i++)
		for(j=i+1;j<=DPosMitjos-1;j++)
			if (jugadors.Mitjos[i]==jugadors.Mitjos[j]) 
				return True;

	// Delanters
	for(i=0;i<DPosDelanters-1;i++)
		for(j=i+1;j<=DPosDelanters-1;j++)
			if (jugadors.Delanters[i]==jugadors.Delanters[j]) 
				return True;

	return False; 
}


// Calculates the team cost adding the individual cost of all team players.
// Returns the cost.

int
CostEquip(TJugadorsEquip equip)
{
	int x;
	int cost=0;

	for(x=0;x<DPosPorters;x++)
		cost += GetPorter(equip.Porter[x]).cost;
	
	for(x=0;x<DPosDefensors;x++)
		cost += GetDefensor(equip.Defensors[x]).cost;

	for(x=0;x<DPosMitjos;x++)
		cost += GetMitg(equip.Mitjos[x]).cost;

	for(x=0;x<DPosDelanters;x++)
		cost += GetDelanter(equip.Delanters[x]).cost;

	return (cost);
}



// Calculates the team points adding the individual points of all team players.
// Returns the points.

int
PuntuacioEquip(TJugadorsEquip equip)
{
	int x;
	int punts=0;
	
	for(x=0;x<DPosPorters;x++)
		punts += GetPorter(equip.Porter[x]).punts;
	
	for(x=0;x<DPosDefensors;x++)
		punts += GetDefensor(equip.Defensors[x]).punts;

	for(x=0;x<DPosMitjos;x++)
		punts += GetMitg(equip.Mitjos[x]).punts;

	for(x=0;x<DPosDelanters;x++)
		punts += GetDelanter(equip.Delanters[x]).punts;
	
	return(punts);
}


// Prints an error message.ç

void error(char *str)
{
	char s[255];
	
	sprintf(s, "[%d] ManFut: %s (%s))\n", getpid(), str,strerror(errno));
	write(2, s, strlen(s));
	exit(1);
}


// Rounded log2 

unsigned int Log2(unsigned long long int n)
{
	return(ceil(log2((double)n)));
}


// Prints all market players information,

void PrintJugadors()
{
	int j;
	
	for(j=0;j<NJugadors;j++)
	{
		sprintf(cad,"Jugador: %s (%d), Posició: %d, Cost: %d, Puntuació: %d.\n", Jugadors[j].nom, Jugadors[j].id, Jugadors[j].tipus, Jugadors[j].cost, Jugadors[j].punts);
		write(1,cad,strlen(cad));
	}
}



// Prints team players.
void PrintEquipJugadors(TJugadorsEquip equip)
{
	int x;

	write(1,"   Porters: ",strlen("   Porters: "));
	for(x=0;x<DPosPorters;x++)
	{
		sprintf(cad,"%s (%d/%d), ",GetPorter(equip.Porter[x]).nom, GetPorter(equip.Porter[x]).cost, GetPorter(equip.Porter[x]).punts);
		write(1,cad,strlen(cad));
	}
	write(1,"\n",strlen("\n"));
	
	write(1,"   Defenses: ",strlen("   Defenses: "));
	for(x=0;x<DPosDefensors;x++)
	{
		sprintf(cad,"%s (%d/%d), ",GetDefensor(equip.Defensors[x]).nom, GetDefensor(equip.Defensors[x]).cost, GetDefensor(equip.Defensors[x]).punts);
		write(1,cad,strlen(cad));
	}
	write(1,"\n",strlen("\n"));
	
	write(1,"   Mitjos: ",strlen("   Mitjos: "));
	for(x=0;x<DPosMitjos;x++)
	{
		sprintf(cad,"%s (%d/%d), ",GetMitg(equip.Mitjos[x]).nom, GetMitg(equip.Mitjos[x]).cost, GetMitg(equip.Mitjos[x]).punts);
		write(1,cad,strlen(cad));
	}
	write(1,"\n",strlen("\n"));
	
	write(1,"   Delanters: ",strlen("   Delanters: "));
	for(x=0;x<DPosDelanters;x++)
	{
		sprintf(cad,"%s (%d/%d), ",GetDelanter(equip.Delanters[x]).nom, GetDelanter(equip.Delanters[x]).cost, GetDelanter(equip.Delanters[x]).punts);
		write(1,cad,strlen(cad));
	}
	write(1,"\n",strlen("\n"));
}
