// my_predictor.h
// This file contains a sample my_predictor class.
// It is a simple 32,768-entry gshare with a history length of 15.
// Note that this predictor doesn't use the whole 32 kilobytes available
// for the CBP-2 contest; it is just an example.

#include <bitset>
#include <vector>

#define HISTORY_LENGTH  64
#define NUM_BANKS 3
#define NUM_PERCEPTRONS_PER_BANK 512 // number of different perceptrons (1 per unique branch)
#define MIN_WEIGHT -128
#define MAX_WEIGHT 127
constexpr int THETA = (int) (1.93 * HISTORY_LENGTH + 14); // Training threshold

class my_update : public branch_update {
	public:
		unsigned int index[NUM_BANKS];
	};

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
#define PC_HIST_LENGTH 2

	my_update u;
	branch_info bi;

	std::bitset<HISTORY_LENGTH> ghist; // Global branch history
	perceptron banks[NUM_BANKS][NUM_PERCEPTRONS_PER_BANK];

	unsigned int pc_hist[PC_HIST_LENGTH]; // recent PCs

	my_predictor (void) {

		for (int i = 0; i < PC_HIST_LENGTH; i++)
			pc_hist[i] = 0;
	}

	int get_index(unsigned int pc, int bank_id) {
		switch (bank_id) {
			case 0: return (pc ^ (ghist.to_ullong() & 0xFF)) % NUM_PERCEPTRONS_PER_BANK;
			case 1: return (pc ^ (ghist.to_ullong() & 0xFFFF)) % NUM_PERCEPTRONS_PER_BANK;
			case 2: return (pc ^ (ghist.to_ullong())) % NUM_PERCEPTRONS_PER_BANK;
		}
	}

	branch_update *predict (branch_info & b) {
		bi = b;
		if (b.br_flags & BR_CONDITIONAL) { // if conditional branch
			// u.index = b.address % NUM_PERCEPTRONS_PER_BANK;
			// u.index = (b.address ^ ghist.to_ullong()) % NUM_PERCEPTRONS_PER_BANK; // hash with ghist

			// unsigned int hash = pc_hist[0];
			// for (int i = 1; i < PC_HIST_LENGTH; i++)
			// 	hash ^= pc_hist[i];
			// u.index = (b.address ^ hash ^ ghist.to_ullong()) % NUM_PERCEPTRONS_PER_BANK; // hash with recent PCs (path) and ghist

			int votes = 0;

			for (int i = 0; i < NUM_BANKS; i++) {
				u.index[i] = get_index(b.address, i);
				perceptron& p = banks[i][u.index[i]];
				votes += (p.predict(ghist) >= 0 ? 1 : 0);
			}

			u.direction_prediction (votes >= NUM_BANKS / 2); // majority vote

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

			for (int i = 0; i < NUM_BANKS; i++) {
				perceptron& p = banks[i][((my_update*)u)->index[i]];
				int output = p.predict(ghist);

				if ((output >= 0) != taken || std::abs(output) <= THETA) {
					p.train(ghist, taken);
				}
			}
			
			ghist <<= 1;
			ghist.set(0, taken); // Sets LSB to taken
		}
	}
};