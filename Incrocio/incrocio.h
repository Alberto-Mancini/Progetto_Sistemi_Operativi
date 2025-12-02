#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <semaphore.h>
#include <sys/time.h>

#define NESSUNA_AUTO		-1
#define NUM_STRADE			4

int GetDistanceFromStreet(int iStreet, int iDirezione) {
	if(iDirezione == 0)
		iDirezione = 4;
	return(iDirezione - iStreet);
}

int StreetOnTheLeft(int iMyStrada, int iDistance) {
	 if(iDistance < 1 || iDistance > 3) {
		 fprintf(stderr, "DISTANZA ERRATA: %d!!!\n", iDistance);
		 exit(EXIT_FAILURE);
	 }
	 //printf("%d: MyStrada = %d, incremento = %d\n", getpid(), iMyStrada, iDistance);
	 return (iMyStrada + iDistance) % NUM_STRADE;
 }
 
/**********************************************************************************************************************/
// Funzione che estrae "a sorte" la strada in cui dovrà svoltare l'automobile proveniente dalla strada ricevuta
// come argomento. La strada estratta è ritornata al chiamante in base 0 (da 0 a (NUM_STRADE - 1))
/**********************************************************************************************************************/
 int EstraiDirezione(int iMyStreet) {
	int iDirezione = iMyStreet;
	struct timeval tv;
	long l;

	while(iDirezione == iMyStreet) {
		gettimeofday(&tv, NULL);
		l = tv.tv_usec;
		l = l % NUM_STRADE;
		iDirezione = (int)l;
	}
	 
	return iDirezione;
 }
 

/**********************************************************************************************************************/
// Questa funzione simula le precedenze tra le auto secondo il codice della strada.... più o meno...
// Ritorna il numero dell'auto che può passare, uno alla volta.
//
// Riceve come argomento un array di 4 elementi interi. Ogni elemento contiene il numero della strada in cui deve
// svoltare l'auto i-esima, o il valore -1 se dall'i-esima strada non proviene enessuna macchina. Cioè:
// - il primo elemento è relativo all'auto 0, proveniente dalla strada 0, e contiene il numero della strada in cui
//   l'auto 0 deve svoltare (o -1 se nessun'auto proviene dalla strada 0)
// - il secondo è relativo all'auto 1, proveniente dalla strada 1, e contiene il numero della strada in cui
//   l'auto 1 deve svoltare (o -1 se nessun'auto proviene dalla strada 1)
// - e così via
//
// Per esempio: l'array contenente i valori [3, -1, 1, 2] sta a significare che l'auto 0 (primo elemento dell'array),
// proveniente dalla strada 0, deve svoltare nella strada 3; dalla strada 1 non proviene nessuna auto; l'auto 2,
// proveniente dalla strada 2, deve svoltare nella strada 1; e così via.
//
// In questo esempio la funzione, se richiamata più volte, ritorna nell'ordine: 0, 3, 2
/**********************************************************************************************************************/
int GetNextCar(int *piDirezioni) {
	int iAuto = NESSUNA_AUTO;
	 
	for(int i = 0; i < NUM_STRADE; i++) {
		int iMyStreet = i;
		
		if(piDirezioni[iMyStreet] != NESSUNA_AUTO) {
			if(piDirezioni[iMyStreet] == StreetOnTheLeft(iMyStreet, 1)) {
				// L'auto i-esima svolta subito a DX
				iAuto = i;
				break;
			}
			else {
				if(piDirezioni[StreetOnTheLeft(iMyStreet, 1)] != NESSUNA_AUTO)
					// L'auto i-esima non ha la  DX libera
					continue;
				else {
					// L'auto i-esima ha la DX libera.
					// Controlliamo se, svoltando, l'auto i-esima troverà sulla sua DX
					// l'auto che proviene dalla strda di fronte
					int iStradaDiFronte = StreetOnTheLeft(iMyStreet, 2);
					if(piDirezioni[iStradaDiFronte] == NESSUNA_AUTO) {
						iAuto = i;
						break;
					}
					else {
						int iDoveVaDiFronte = piDirezioni[iStradaDiFronte], iDoveVaIesima = piDirezioni[iMyStreet];
						if(GetDistanceFromStreet(iMyStreet, iDoveVaIesima) <= GetDistanceFromStreet(iMyStreet, iStradaDiFronte) 
						   ||
						   (GetDistanceFromStreet(iMyStreet, piDirezioni[iStradaDiFronte]) < GetDistanceFromStreet(iMyStreet, piDirezioni[iMyStreet]))) {
							iAuto = i;
							break;
						}
					}
				}
			}
		}
	}
	
	if(iAuto == NESSUNA_AUTO) {
		for(int i = 0; i < NUM_STRADE; i++) {
			if(piDirezioni[i] != NESSUNA_AUTO) {
				iAuto = i;
				break;
			}
		}
	}
	 
	return iAuto;
}
