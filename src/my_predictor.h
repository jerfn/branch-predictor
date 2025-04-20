// my_predictor.h
// This file contains a sample my_predictor class.
// It is a simple 32,768-entry gshare with a history length of 15.
// Note that this predictor doesn't use the whole 32 kilobytes available
// for the CBP-2 contest; it is just an example.

// class my_update : public branch_update {
// public:
// 	unsigned int index;
// };

// class my_predictor : public branch_predictor {
// public:
// #define HISTORY_LENGTH	15
// #define TABLE_BITS	15
// 	my_update u;
// 	branch_info bi;
// 	unsigned int history;
// 	unsigned char tab[1<<TABLE_BITS]; // array

// 	my_predictor (void) : history(0) { // constructor, initializes history to 0
// 		memset (tab, 0, sizeof (tab)); // zeroes out prediction table
// 	}

// 	branch_update *predict (branch_info & b) {
// 		bi = b;
// 		if (b.br_flags & BR_CONDITIONAL) { // if a conditional jump (br instead of j)
// 			u.index = 
// 				  (history << (TABLE_BITS - HISTORY_LENGTH)) 
// 				^ (b.address & ((1<<TABLE_BITS)-1));
// 			u.direction_prediction (tab[u.index] >> 1);
// 		} else {
// 			u.direction_prediction (true);
// 		}
// 		u.target_prediction (0);
// 		return &u;
// 	}

// 	void update (branch_update *u, bool taken, unsigned int target) {
// 		if (bi.br_flags & BR_CONDITIONAL) {
// 			unsigned char *c = &tab[((my_update*)u)->index];
// 			if (taken) {
// 				if (*c < 3) (*c)++;
// 			} else {
// 				if (*c > 0) (*c)--;
// 			}
// 			history <<= 1;
// 			history |= taken;
// 			history &= (1<<HISTORY_LENGTH)-1;
// 		}
// 	}
// };

#include <cstdint>
#include <math.h>
#include <deque>

class my_update : public branch_update {
	public: 
		unsigned int index;
};

#define BTABLE_SIZE 1024 // size of bimodal table
#define HYSTSHIFT 2 // shift for hysteresis bit
#define NUM_TABLES 12 // number of global history table
#define GTABLE_SIZE 1024 // size of each global history table
#define CTR_WIDTH 3 // 3 bits for saturating predictor counter
#define USE_WDITH 2 // 2 bits for saturating usefulness counter

#define LOGTICK 19 // period for tag resetting

class my_predictor : public branch_predictor {
public:

	my_update u;
	branch_info bi;

	class bentry {
		public: 
		int8_t hyst;
		int8_t pred;
		bentry() {
			hyst = 1;
			pred = 0;
		}
	};

	class gentry {
	public:
		int8_t ctr;
		int8_t use;
		uint16_t tag;

		gentry() {
			ctr = 0;
			use = 0;
			tag = 0;
		}
	};

	const int HIST_LEN[NUM_TABLES+1] = {0, 4, 6, 10, 16, 25, 40, 64, 101, 160, 254, 403, 640};
	std::deque<bool> ghistory;

	bentry *btable;
	gentry *gtable[NUM_TABLES+1]; 

	int hit_idx; // index of the table that hit
	// int alt_idx; 
	bool pred;

	int TICK = (1 << (LOGTICK - 1)); // initialize tick counter to half of period

	my_predictor (void) { // constructor
		// allocate predictor table
		btable = new bentry[BTABLE_SIZE];
		for (int i = 1; i <= NUM_TABLES; i++) {
			gtable[i] = new gentry[GTABLE_SIZE];
		}
	}

	int compute_index (unsigned int address, const std::deque<bool>& ghistory, int table) {
		uint32_t h = 0;
		for (int i = 0; i < HIST_LEN[table] && i < int(ghistory.size()); i++) {
			h = (h << 1) | ghistory[i];
		}
		return (address^h) % GTABLE_SIZE;
	}

	uint16_t compute_tag (unsigned int address, const std::deque<bool>& ghistory, int table) {
		uint32_t h = 0;
		for (int i = 0; i < HIST_LEN[table] && i < int(ghistory.size()); i++) {
			h = (h << 1) | ghistory[i];
		}
		return static_cast<uint16_t>((address^h) & 0XFFFF);
	}

	void update_ctr (int8_t& ctr, bool taken, int nbits) {
		if (taken) {
			if (ctr < ((1 << (nbits - 1)) - 1)) ctr++;
		} else {
			if (ctr > -(1 << (nbits - 1))) ctr--;
		}
	}

	bool predict_bim (unsigned int address) {
		int bindex = address & (BTABLE_SIZE-1);	// get index in bimodal table
		return (btable[bindex].pred > 0);
	}

	branch_update *predict (branch_info & b) {
		bi = b;
		unsigned int address = b.address;

		// predict using TAGE on conditional branches
		if (b.br_flags & BR_CONDITIONAL) {
			pred = predict_bim (address);
			hit_idx = 0;
			// alt_idx = 0;

			for (int i = NUM_TABLES; i > 0; i--) {
				int gidx = compute_index (address, ghistory, i); 
				gentry& entry = gtable[i][gidx];
				uint16_t tag = compute_tag (address, ghistory, i); 

				if (entry.tag == tag) {
					hit_idx = i; // found a matching entry
					pred = (entry.ctr >= 0); 
					break;
				}
			}

			// for (int i = hit_idx - 1; i > 0; i--) {
			// 	int gidx = compute_index (address, ghistory, i);
			// 	gentry& entry = gtable[i][gidx];
			// 	uint16_t tag = compute_tag (address, ghistory, i);
			// 	if (entry.tag == tag) {
			// 		alt_idx = i; // found an alternative entry
			// 		break;
			// 	}
			// }
			// if (hit_idx > 0) {
			// 	if (alt_idx > 0) 
			// }

			u.direction_prediction (pred); // set direction prediction
		} else {
			u.direction_prediction (true); // always predict taken for unconditional branches
		}

		u.target_prediction (0); 
		return &u;
	}

	void update_bim (unsigned int address, bool taken) { 	// update bimodal predictor
		int bindex = address & (BTABLE_SIZE-1);	// get index in bimodal table
		int pred = (btable[bindex].pred << 1) + btable[bindex >> HYSTSHIFT].hyst; 
		if (taken && pred < 3) pred++;
		else if (pred > 0) pred--;
		btable[bindex].pred = pred >> 1; // update pred in table
		btable[bindex >> HYSTSHIFT].hyst = (pred & 1); // update hyst in table
	}

	void update_ghist (bool taken) {
		ghistory.push_front(taken); // push new prediction to history
		if (int(ghistory.size()) > HIST_LEN[NUM_TABLES]) ghistory.pop_back(); // pop oldest prediction
	}

	void update (branch_update *u, bool taken, unsigned int target) {
		unsigned int address = bi.address;
		
		// update TAGE predictor
		if (bi.br_flags & BR_CONDITIONAL)
		{
			int gidx;
			bool allocate = ((pred != taken) && (hit_idx < NUM_TABLES)); // allocate new entry if prediction wrong
			if (allocate)
			{
				int8_t min = 1;

				// search for non-useful entry in tables
				for (int i = NUM_TABLES; i > hit_idx; i--)
				{
					gidx = compute_index (address, ghistory, i); 
					if (gtable[i][gidx].use < min)
						min = gtable[i][gidx].use;
				}
				
				// if cannot find entry, forces one to be available
				if (min > 0)
					gtable[gidx+1][compute_index (address, ghistory, gidx+1)].use = 0;
				
				// allocate one entry
				for (int i = gidx+1; i <= NUM_TABLES; i++)
				{
					gidx = compute_index (address, ghistory, i);
					if (gtable[i][gidx].use == 0)
					{
						gtable[i][gidx].tag = compute_tag (address, ghistory, i); // update tag
						gtable[i][gidx].ctr = (taken) ? 0 : -1; // reset prediction counter
						gtable[i][gidx].use = 0; // reset usefulness counter
						break;
					}
				}
			}
			
 			// update predictor counter
			gidx = compute_index (address, ghistory, hit_idx);
			if (hit_idx > 0)
			{
				update_ctr (gtable[hit_idx][gidx].ctr, taken, CTR_WIDTH); // update prediction counter
				if (gtable[hit_idx][gidx].use == 0)
					update_bim (address, taken);
			} 
			else 
			{
				update_bim (address, taken); 
			}

			// update usefulness counter
			if (pred == taken) 
			{
				if (gtable[hit_idx][gidx].use < 3) 
					gtable[hit_idx][gidx].use++;
				else if (gtable[hit_idx][gidx].use > 0) 
					gtable[hit_idx][gidx].use--; 
			}

			// periodically reset usefulness counter
			TICK++;
			if ((TICK & ((1 << LOGTICK) - 1)) == 0)
				for (int i = 1; i <= NUM_TABLES; i++)
					for (int j = 0; j < GTABLE_SIZE; j++)
						gtable[i][j].use = gtable[i][j].use >> 1; // shift usefulness counter

		}

		// update histories
		update_ghist (taken); // update global history

	}

};
