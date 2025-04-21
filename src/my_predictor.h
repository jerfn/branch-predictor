// my_predictor.h
// This file contains a sample my_predictor class.
// It is a simple 32,768-entry gshare with a history length of 15.
// Note that this predictor doesn't use the whole 32 kilobytes available
// for the CBP-2 contest; it is just an example.

#include <bitset>
#include <vector>

class my_update : public branch_update {
public:
	unsigned int index;
};

/*
class my_predictor : public branch_predictor {
public:
#define HISTORY_LENGTH	15
#define TABLE_BITS	15
	my_update u;
	branch_info bi;
	unsigned int history;
	unsigned char tab[1<<TABLE_BITS]; // array

	my_predictor (void) : history(0) { // constructor, initializes history to 0
		memset (tab, 0, sizeof (tab)); // zeroes out prediction table
	}

	branch_update *predict (branch_info & b) {
		bi = b;
		if (b.br_flags & BR_CONDITIONAL) { // if a conditional jump (br instead of j)
			u.index = 
				  (history << (TABLE_BITS - HISTORY_LENGTH)) 
				^ (b.address & ((1<<TABLE_BITS)-1));
			u.direction_prediction (tab[u.index] >> 1);
		} else {
			u.direction_prediction (true);
		}
		u.target_prediction (0);
		return &u;
	}

	void update (branch_update *u, bool taken, unsigned int target) {
		if (bi.br_flags & BR_CONDITIONAL) {
			unsigned char *c = &tab[((my_update*)u)->index];
			if (taken) {
				if (*c < 3) (*c)++;
			} else {
				if (*c > 0) (*c)--;
			}
			history <<= 1;
			history |= taken;
			history &= (1<<HISTORY_LENGTH)-1;
		}
	}
};
*/

#define HISTORY_LENGTH  64
#define NUM_PERCEPTRONS 2048 // number of different perceptrons (1 per unique branch)
#define MIN_WEIGHT -128
#define MAX_WEIGHT 127
constexpr int THETA = (int) (1.93 * HISTORY_LENGTH + 14); // Training threshold

class perceptron {
public:
	int8_t weights[HISTORY_LENGTH]; // using int8_t for weights, -128 to 127
	int bias;

	perceptron () {
		for (int i = 0; i < HISTORY_LENGTH; i++)
			weights[i] = 0;
		bias = 0;
	}

	int predict(const std::bitset<HISTORY_LENGTH>& ghist) const {
        int y = bias;
        for (int i = 0; i < HISTORY_LENGTH; i++) {
			int input = ghist[i] ? 1 : -1;
            y += weights[i] * input;
        }
        return y;
	}
	
	void train(const std::bitset<HISTORY_LENGTH>& ghist, bool taken) {
		for (int i = 0; i < HISTORY_LENGTH; i++) {
			int correction = (ghist[i] == taken ? 1 : -1); // increment if branch outcome agrees w/ ith history input, else decrement
			weights[i] = std::max(MIN_WEIGHT, std::min(MAX_WEIGHT, weights[i] + correction));
		}
		bias = std::max(MIN_WEIGHT, std::min(MAX_WEIGHT, bias + (taken ? 1 : -1)));
	}
};

class my_predictor : public branch_predictor {
public:

	my_update u;
	branch_info bi;

	std::bitset<HISTORY_LENGTH> ghist; // Global branch history
	std::vector<perceptron> table;

	my_predictor (void) {
		table.resize(NUM_PERCEPTRONS);
	}

	branch_update *predict (branch_info & b) {
		bi = b;
		if (b.br_flags & BR_CONDITIONAL) { // if conditional branch
			// u.index = b.address % NUM_PERCEPTRONS;
			u.index = (b.address ^ ghist.to_ulong()) % NUM_PERCEPTRONS;
			perceptron& p = table[u.index];
			u.direction_prediction (p.predict(ghist) >= 0);
		} else {
			u.direction_prediction (true);
		}
		u.target_prediction (0);
		return &u;
	}

	void update (branch_update *u, bool taken, unsigned int target) {
		if (bi.br_flags & BR_CONDITIONAL) { // if conditional branch
			perceptron& p = table[((my_update*)u)->index];
			int output = p.predict(ghist);

			if ((output >= 0) != taken || std::abs(output) <= THETA) {
				p.train(ghist, taken);
			}
			
			ghist <<= 1;
			ghist.set(0, taken); // Sets LSB to taken
		}
	}
};