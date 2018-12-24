#ifndef _MARKOV_PREDICTION_H
#define _MARKOV_PREDICTION_H

#ifndef NS_NO_GLOBALS
#include "ns-2/aodv-uu.h"
#endif

#ifndef NS_NO_DECLARATIONS

#define  CHAINLENGTH  5

int markov_prediction(int* stabilityA, int* stabilityB);

#endif				/* NS_NO_DECLARATIONS */


#endif