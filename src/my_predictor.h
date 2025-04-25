// my_predictor.h

/* STORAGE REQUIREMENT
 * 2048 perceptrons * (64 weights + 1 bias) * 8 bits
 * 64 bits global history
 * TOTAL = 130 KB + 8 B
 */

#include <bitset>
#include <vector>

class my_update : public branch_update {
public:
	unsigned int index;
};

#define HISTORY_LENGTH  64
#define PC_HIST_LENGTH 2
#define NUM_PERCEPTRONS 2048 // number of different perceptron predictors
#define MIN_WEIGHT -128
#define MAX_WEIGHT 127
constexpr int THETA = (int) (1.93 * HISTORY_LENGTH + 14); // Training threshold

class perceptron {
public:
	int8_t weights[HISTORY_LENGTH]; // using int8_t for weights, -128 to 127
	int8_t bias;

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
	std::vector<perceptron> table; // Table of perceptrons

	unsigned int pc_hist[PC_HIST_LENGTH]; // recent PCs

	my_predictor (void) {
		table.resize(NUM_PERCEPTRONS);

		for (int i = 0; i < PC_HIST_LENGTH; i++)
			pc_hist[i] = 0;
	}

	branch_update *predict (branch_info & b) {
		bi = b;
		if (b.br_flags & BR_CONDITIONAL) { // if conditional branch
			// Hash with PC only
			// u.index = b.address % NUM_PERCEPTRONS;

			// Hash with PC and global history
			u.index = (b.address ^ ghist.to_ullong()) % NUM_PERCEPTRONS;

			// Hash with recent PCs (path) and global history
			// unsigned int hash = b.address;
			// for (int i = 0; i < PC_HIST_LENGTH; i++)
			// 	hash ^= pc_hist[i];
			// u.index = (hash ^ ghist.to_ullong()) % NUM_PERCEPTRONS;

			// Index into perceptron table and predict
			perceptron& p = table[u.index];
			u.direction_prediction (p.predict(ghist) >= 0);

			// Update PC history array
			for (int i = PC_HIST_LENGTH - 1; i > 0; i--)
				pc_hist[i] = pc_hist[i - 1];
			pc_hist[0] = b.address;

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

			// Train if mispredict or output is less than threshold
			if ((output >= 0) != taken || std::abs(output) <= THETA) {
				p.train(ghist, taken);
			}
			
			// Update ghist
			ghist <<= 1;
			ghist.set(0, taken); // Sets LSB to taken
		}
	}
};