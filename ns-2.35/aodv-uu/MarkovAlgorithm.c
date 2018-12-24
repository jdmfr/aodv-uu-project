#include<stdio.h>
#include<stdint.h>
#include"MarkovPrediction.h"
#include"ns-2/aodv-uu.h"

double markov_algorithm(int stabilityA[5], int stabilityB[5]) {
	int i;
	int count_1 = 0, count_2 = 0;
	double prediction_value = 0, p_k = 0, p_k1 = 0;

	for (i = 0; i < CHAINLENGTH - 1; i++) {
		if (stabilityA[i] == stabilityA[CHAINLENGTH - 2]
			&& stabilityB[i] == stabilityB[CHAINLENGTH - 2]
			&& stabilityA[i + 1] == stabilityA[CHAINLENGTH - 1]
			&& stabilityB[i + 1] == stabilityA[CHAINLENGTH - 1]) {
			count_1 += 1;
		}
	}
	p_k = (count_1 * 1.0 / (CHAINLENGTH - 1));
	if (p_k == 0) {
		return 0;
	}

	for (i = 0; i < CHAINLENGTH - 2; i++) {
		if (stabilityA[i] == stabilityA[CHAINLENGTH - 2]
			&& stabilityB[i] == stabilityB[CHAINLENGTH - 2]
			&& stabilityA[i + 1] == stabilityA[CHAINLENGTH - 1]
			&& stabilityB[i + 1] == stabilityA[CHAINLENGTH - 1]
			&& (stabilityA[i + 2] == -1 || stabilityB[i + 2] == -1)) {
			count_2 += 1;
		}
	}
	p_k1 = (count_2 * 1.0 / (CHAINLENGTH - 2));

	prediction_value = p_k1 / p_k;

	return prediction_value;
}

int transfer_to_int(double prediction_value) {
	if (prediction_value <= 0.2) {
		return 0;
	}
	else if (prediction_value <= 0.4) {
		return 1;
	}
	else if (prediction_value <= 0.6) {
		return 2;
	}
	else if (prediction_value <= 0.8) {
		return 3;
	}
	else {
		return 4;
	}
}

int NS_CLASS markov_prediction(int* stabilityA, int* stabilityB) {
	double prediction_value;
	int result;

	prediction_value = markov_algorithm(stabilityA, stabilityB);

	result = transfer_to_int(prediction_value);

	return result;
}
