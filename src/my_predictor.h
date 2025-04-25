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

#define NUM_HASHES 16 // number of feature hashes
#define TABLE_SIZE 2048 // number of entries in each weight table
#define HISTORY_LENGTH  64
#define MIN_WEIGHT -128
#define MAX_WEIGHT 127
// constexpr int THETA = (int) (1.93 * HISTORY_LENGTH + 14); // Training threshold
constexpr int THETA = 40;

class perceptron {
public:
	int8_t W[NUM_HASHES][TABLE_SIZE]; // weight banks
	int bias;

	perceptron () {
		for (int i = 0; i < NUM_HASHES; i++) {
			for (int j = 0; j < TABLE_SIZE; j++) {
				W[i][j] = 0;
			}
		}
		bias = 0;
	}

	int hash(int i, unsigned int pc, const std::bitset<HISTORY_LENGTH>& ghist) const {
		uint64_t ghr = ghist.to_ullong();
		return ((pc >> i) ^ (ghist.to_ullong() >> (3 * i)) ^ i * 0x5bd1e995) & (TABLE_SIZE - 1); // 5.618
    }

	int predict(unsigned int pc, const std::bitset<HISTORY_LENGTH>& ghist) const {
        int y = bias;
        for (int i = 0; i < NUM_HASHES; i++) {
			// int input = ghist[i] ? 1 : -1;
            y += W[i][hash(i, pc, ghist)];
        }
        return y;
	}
	
	void train(unsigned int pc, const std::bitset<HISTORY_LENGTH>& ghist, bool taken) {
		for (int i = 0; i < NUM_HASHES; i++) {
			int correction = taken ? 1 : -1;
			W[i][hash(i, pc, ghist)] = std::max(MIN_WEIGHT, std::min(MAX_WEIGHT, W[i][hash(i, pc, ghist)] + correction));
		}
		bias = std::max(MIN_WEIGHT, std::min(MAX_WEIGHT, bias + (taken ? 1 : -1)));
	}
};

class my_predictor : public branch_predictor {
public:

	my_update u;
	branch_info bi;

	perceptron p;

	std::bitset<HISTORY_LENGTH> ghist; // Global branch history

	my_predictor (void) {

	}

	branch_update *predict (branch_info & b) {
		bi = b;
		if (b.br_flags & BR_CONDITIONAL) { // if conditional branch

			u.direction_prediction (p.predict(b.address, ghist) >= 0);

		} else {
			u.direction_prediction (true);
		}
		u.target_prediction (0);
		return &u;
	}

	void update (branch_update *u, bool taken, unsigned int target) {
		if (bi.br_flags & BR_CONDITIONAL) { // if conditional branch
			int output = p.predict(bi.address, ghist);

			if ((output >= 0) != taken || std::abs(output) <= THETA) {
				p.train(bi.address, ghist, taken);
			}
			
			ghist <<= 1;
			ghist.set(0, taken); // Sets LSB to taken
		}
	}
};