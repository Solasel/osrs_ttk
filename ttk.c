#include <stdio.h>
#include <stdlib.h>

#define DIST_SIZE (229)
#define PRECISION (5)

/* Kill type 0 => weapon swing
   timer is not taken into account
   if the attack kills the opponent.
   Useful when killing a single enemy
   is all that matters.

   Kill type 1 => weapon swing timer
   is taken into account regardless.
   Useful for target-switching situations
   like nylocas waves. */
#define KILL_TYPE (1)

enum errs {
	SUCCESS,
	BAD_ARGS,
	MALLOC_FAIL,
	NOT_YET_IMP
};

enum wep_type {
	REGULAR,
	FANG,
	SCYTHE,
	SCYTHE_2H,
	KERIS,
	ZCB_DIAM_KAND,
	RCB_DIAM_KAND,
	ZCB_DIAM_NOKA,
	RCB_DIAM_NOKA,
	ZCB_RUBY_KAND,
	RCB_RUBY_KAND,
	ZCB_RUBY_NOKA,
	RCB_RUBY_NOKA
};

struct weapon
{
	char *name;
	int type, spd, max;
	double acc;
	double dist[DIST_SIZE];
};

int wep_init(struct weapon *wep, char *name, int spd, int max, double acc, int type);
double ttk_calc(struct weapon *wep, double *ttk_array, int hp);
int ttk_comp(int wepc, struct weapon *wepv, int hp);
void print_wep(struct weapon *wep);

int main(int argc, char **argv)
{
	int errno;
	struct weapon test[2];

	errno = wep_init(test, "4-way mage sang", 4, 42, .9608, REGULAR);
	if (errno) {
		printf("ERROR: Failed to initialize weapon 1: %d.\n", __LINE__);
		return errno;
	}

	/*
	errno = wep_init(test, "Max melee scy (2 hitsplats)", 5, 50, .9724, SCYTHE_2H);
	if (errno) {
		printf("ERROR: Failed to initialize weapon 1: %d.\n", __LINE__);
		return errno;
	}

	errno = wep_init(test + 1, "Max melee swift", 3, 34, .9524, REGULAR);
	if (errno) {
		printf("ERROR: Failed to initialize weapon 2: %d.\n", __LINE__);
		return errno;
	}
	*/

	// print_wep(test);
	// print_wep(test + 1);
	ttk_comp(1, test, 22);

	/*
	errno = wep_init(test, "Twisted Bow", 5, 88, 0.626, REGULAR); // 0.4779
	if (errno) {
		printf("ERROR: Failed to initialize weapon 1: %d.\n", __LINE__);
		return errno;
	}

	// print_wep(test);
	ttk_comp(1, test, 1350);
	*/

	return SUCCESS;
}

/* Populates a ttk array using dynamic programming,
   with ttk and best weapon at each hp. */
/* TODO
   Add an argument "goal" and make it calculate hp -> goal instead of hp -> 0 */
int ttk_comp(int wepc, struct weapon *wepv, int hp)
{
	int i, j, best_wep;
	double cand_ttk, best_ttk, *ttk_array;

	/* Allocate an array to keep track of the ttk at every given
	   hp for recursive calculations. We technically don't need 0
	   (ttk @ 0 = 0.0) but including it makes indexing easier. */
	ttk_array = malloc((hp + 1) * sizeof(double));
	if (!ttk_array) {
		printf("ERROR: Failed to malloc the ttk array: %d.\n", __LINE__);
		return MALLOC_FAIL;
	}

	ttk_array[0] = 0.0;

	/* Print a header. */
	printf("HP\t\tTTK\t\tWep\n0\t\t0.00\t\tN/A\n");

	/* The ttk at a given hp and for a given weapon is a weighted
	   sum of the ttks below it, determined by the weapon's hit
	   distribution - find the best weapon, record its value, and
	   print out the information. */
	for(i = 1; i < hp + 1; i++) {
		best_wep = 0;
		best_ttk = ttk_calc(wepv, ttk_array, i);

		/* Iterate over the list of weapons to find the best ttk. */
		for (j = 1; j < wepc; j++) {
			cand_ttk = ttk_calc(wepv + j, ttk_array, i);
			if (cand_ttk < best_ttk) {
				best_ttk = cand_ttk;
				best_wep = j;
			}
		}

		/* Record the ttk and print out the information. */
		printf("%d\t\t%.2f\t\t%s\n", i, best_ttk, wepv[best_wep].name);
		ttk_array[i] = best_ttk;
	}

	free(ttk_array);
	return SUCCESS;
}

/* Returns the expected ticks to kill given a weapon,
   hp value, and a ttk_array populated up to hp - 1. */
/* TODO
   Add an argument "goal" and make it calculate hp -> goal instead of hp -> 0 */
double ttk_calc(struct weapon *wep, double *ttk_array, int hp)
{
	int i, ruby_dmg, type, spd = wep->spd;
	double ruby_chance, p0, rem, rv = 0.0;
	p0 = wep->dist[0];
	rem = 1.0 - p0;

	/* If we're dealing with rubies, add in the ruby bolt proc. */
	type = wep->type;
	if (type >= ZCB_RUBY_KAND) {
		/* First, calculate the ruby damage. */
		if (type == ZCB_RUBY_KAND || type == ZCB_RUBY_NOKA) {
			ruby_dmg = 0.22 * hp;
			ruby_dmg = ruby_dmg > 110 ? 110 : ruby_dmg;
		} else {
			ruby_dmg = 0.2 * hp;
			ruby_dmg = ruby_dmg > 100 ? 100 : ruby_dmg;
		}

		/* Then the ruby chance. */
		ruby_chance = wep->type >= ZCB_RUBY_NOKA ? 0.06 : 0.066;
		rem -= ruby_chance;

		/* Rubies can never kill an enemy, so as long as they hit
		   non-zero damage, count them. If they do hit a zero, we
		   must take them into account at the very end.
		   This data will be preserved in p0. */
		if (ruby_dmg > 0) {
			rv += ruby_chance * (spd + ttk_array[hp - ruby_dmg]);
			ruby_chance = 0.0;
		} else {
			p0 += ruby_chance;
		}
	}

	/* For each possible (nonzero) damage value that does
	   not kill the monster, the ttk is the weapon's speed
	   plus the ttk at the resultant health. */
	for (i = 1; i < hp && i < DIST_SIZE; i++) {
		rem -= wep->dist[i];
		rv += wep->dist[i] * (spd + ttk_array[hp - i]);
	}

	/* Print the chance to kill. */
	printf("Chance to kill with '%s' at %d HP - %.2f\n", wep->name, hp, rem);

	/* For damage values that do kill the boss, we add them
	   only for kill type 1, and not for kill type 0. */
	if (KILL_TYPE)
		rv += rem * spd;

	/* Finally, we will account for the probability of hitting a 0. */
	rv += p0 * spd;
	return rv / (1 - p0);
}

/* Initializes a weapon struct with the given info. */
int wep_init(struct weapon *wep, char *name, int spd, int max, double acc, int type)
{
	int i, j, k, fang_off, max2, max3;
	double p_net, p_miss1, p_miss2, p_unif, p_unif2, p_unif3, p_unif12, p_unif13, p_unif23;

	/* First, check args and populate the basic members of the struct. */
	if (!wep) {
		printf("ERROR: NULL weapon struct provided: %d.\n", __LINE__);
		return BAD_ARGS;
	} else if (!name) {
		printf("ERROR: NULL name string provided: %d.\n", __LINE__);
		return BAD_ARGS;
	} else if (type < REGULAR || type > RCB_RUBY_NOKA) {
		printf("ERROR: Invalid weapon type %d: %d.\n", type, __LINE__);
		return BAD_ARGS;
	}

	wep->name = name;
	wep->spd = spd;
	wep->max = max;
	wep->acc = acc;
	wep->type = type;

	/* Now we populate the weapon's prior distribution.
	   There are four fundamental distributions:
	   Standard: That of a standard weapon - uniform across 1-max,
	   	with a spike at 0 to account for misses.
		- Ruby bolts are just this distribution scaled down, with
			the ruby proc added in post.
	   Fang: That of Osmumenten's fang - as a standard weapon, but
	   	with the non-0 hits cropped to the min/max hit of the fang.
	   Diamond / Keris: Weapons with proc chances that do not depend on enemy hp.
	   	these decompose to be the sum of two dependent distributions - one
		for the proc and one for the non-proc.
	   Scythe: The scythe is de-facto the sum of three independent
	   	regular distributions. This concentrates the distribution near the average
		hit and away from the edges, with a spike near 0. */

	if (type == REGULAR) {
		/* Regular weapons, on hit, are uniform between
		   0 and max. Find that uniform probability. */
		p_unif = acc / (max + 1);

		/* The chance of hitting a 0 is the chance of missing
		   plus the chance a successful hit rolls a 0. */
		wep->dist[0] = 1 - acc + p_unif;

		/* Every other non-zero hit is uniform as above. */
		for (i = 1; i < max + 1; i++)
			wep->dist[i] = p_unif;

		/* Finally, fill the rest of the dist with 0s. */
		for (; i < DIST_SIZE; i++)
			wep->dist[i] = 0.0;

	} else if (type == FANG) {
		/* The fang crops the hit distribution to between
		   0.15 and 0.85 of its max hit. Find that offset. */
		fang_off = 0.15 * max;

		/* Find the associated uniform probability. */
		p_unif = acc / (max - 2 * fang_off + 1);

		/* The chance of hitting a 0 is just the chance of missing. */
		wep->dist[0] = 1 - acc;


		/* Fill in 0s until the fang's min hit. */
		for (i = 1; i < fang_off; i++)
			wep->dist[i] = 0.0;

		/* Now fill in the fang's hit distribution. */
		for (; i < max - fang_off + 1; i++)
			wep->dist[i] = p_unif;

		/* And fill out the dist with 0s. */
		for (; i < DIST_SIZE; i++)
			wep->dist[i] = 0.0;

	} else if (type == SCYTHE) {
		/* A scythe's distribution is the sum of its three
		   independent hitsplats. Get ready for nested loops folks.
		   First, calculate the secondary max hits and some 
		   probability values for faster computation. */
		max2 = max / 2;
		max3 = max / 4;

		p_miss1 = (1 - acc);
		p_miss2 = p_miss1 * p_miss1;

		p_unif = acc / (max + 1);
		p_unif2 = acc / (max2 + 1);
		p_unif3 = acc / (max3 + 1);

		p_unif12 = p_unif * p_unif2;
		p_unif13 = p_unif * p_unif3;
		p_unif23 = p_unif2 * p_unif3;

		/* There are 8 possibilities, corresponding to which
		   hitsplats miss. We need a case for each one. Because this
		   process is additive, set all elements to 0. */
		for (i = 0; i < DIST_SIZE; i++)
			wep->dist[i] = 0.0;

		/* First, assume all three swings hit. */
		p_net = p_unif12 * p_unif3;
		for (i = 0; i < max + 1; i++) {
			for (j = 0; j < max2 + 1; j++) {
				for (k = 0; k < max3 + 1; k++)
					wep->dist[i + j + k] += p_net;
			}
		}

		/* Now assume two hits land, but one misses. */
		p_net = p_miss1 * p_unif12;
		for (i = 0; i < max + 1; i++) {
			for (j = 0; j < max2 + 1; j++)
				wep->dist[i + j] += p_net;
		}

		p_net = p_miss1 * p_unif13;
		for (i = 0; i < max + 1; i++) {
			for (k = 0; k < max3 + 1; k++)
				wep->dist[i + k] += p_net;
		}

		p_net = p_miss1 * p_unif23;
		for (j = 0; j < max2 + 1; j++) {
			for (k = 0; k < max3 + 1; k++)
				wep->dist[j + k] += p_net;
		}
		
		/* Now assume one hit lands, but the other two miss. */
		p_net = p_miss2 * p_unif;
		for (i = 0; i < max + 1; i++)
			wep->dist[i] += p_net;

		p_net = p_miss2 * p_unif2;
		for (j = 0; j < max2 + 1; j++)
			wep->dist[j] += p_net;

		p_net = p_miss2 * p_unif3;
		for (k = 0; k < max3 + 1; k++)
			wep->dist[k] += p_net;

		/* Finally, the case where all hits miss. */
		wep->dist[0] += p_miss1 * p_miss2;

	} else if (type == SCYTHE_2H) {
		/* Two hitsplat scythe is just the three hitsplat version
		   without the third hit. */
		max2 = max / 2;

		p_miss1 = (1 - acc);

		p_unif = acc / (max + 1);
		p_unif2 = acc / (max2 + 1);

		/* There are 4 possibilities, corresponding to which
		   hitsplats miss. We need a case for each one. Because this
		   process is additive, set all elements to 0. */
		for (i = 0; i < DIST_SIZE; i++)
			wep->dist[i] = 0.0;

		/* Assume both hits land. */
		p_net = p_unif * p_unif2;
		for (i = 0; i < max + 1; i++) {
			for (j = 0; j < max2 + 1; j++)
				wep->dist[i + j] += p_net;
		}

		/* Now assume one hit lands, but the other one misses. */
		p_net = p_miss1 * p_unif;
		for (i = 0; i < max + 1; i++)
			wep->dist[i] += p_net;

		p_net = p_miss1 * p_unif2;
		for (j = 0; j < max2 + 1; j++)
			wep->dist[j] += p_net;

		/* Finally, the case where both miss. */
		wep->dist[0] += p_miss1 * p_miss1;

	} else if (type == KERIS) {
		/* Keris distributions are the sum of the
		   non-crit and crit distributions, which
		   are fully dependent. */

		/* The probability any crit happens. */
		p_unif2 = acc / (51 * (max + 1));

		/* The probability any non-crit happens. */
		p_unif = 50 * p_unif2;

		/* Zero doesn't care if it's a crit or not. */
		wep->dist[0] = 1 - acc + p_unif + p_unif2;

		/* The non-crit case. */
		for (i = 1; i < max + 1; i++)
			wep->dist[i] = p_unif;

		/* Pad with zeros to prepare for additive crits. */
		for (; i < DIST_SIZE; i++)
			wep->dist[i] = 0.0;

		/* Now add in the crit distribution. */
		for (i = 1; i < max + 1; i++)
			wep->dist[3 * i] += p_unif2;

	} else if (type >= ZCB_RUBY_KAND) {
		/* Ruby bolts are just the normal distribution 
		   scaled down by a factor dependent on the
		   completion of the Kandarin diary. */
		p_miss1 = type >= ZCB_RUBY_NOKA ? 0.94 : 0.934;

		/* First, the probability of zero. */
		p_unif = acc / (max + 1);
		wep->dist[0] = p_miss1 * (1 - acc + p_unif);

		/* Now the rest. */
		p_unif *= p_miss1;
		for (i = 1; i < max + 1; i++)
			wep->dist[i] = p_unif;
		
		/* And setting other values to 0. */
		for (; i < DIST_SIZE; i++)
			wep->dist[i] = 0.0;
	} else {
		/* Diamond bolts are the sum of two
		   distributions, just like keris weapons. */
		p_miss1 = type >= ZCB_DIAM_NOKA ? 0.9 : 0.89;

		/* First, the non-proc distribution,
		   similarly to above. */
		p_unif = acc / (max + 1);
		wep->dist[0] = p_miss1 * (1 - acc + p_unif);

		p_unif *= p_miss1;
		for (i = 1; i < max + 1; i++)
			wep->dist[i] = p_unif;

		for (; i < DIST_SIZE; i++)
			wep->dist[i] = 0.0;

		/* Now the proc distribution. First calculate the new max hit,
		 which depends on if the crossbow is the ZCB or not. */
		max2 = max * (type == ZCB_DIAM_KAND || type == ZCB_DIAM_NOKA ? 1.26 : 1.15);

		/* Because diamond bolt procs skip accuracy,
		   we can skip the special case for 0. */
		p_unif = (1 - p_miss1) / (max2 + 1);
		for (i = 0; i < max2 + 1; i++)
			wep->dist[i] += p_unif;
	}

	return SUCCESS;
}	

/* Prints out a human-readable representation of a weapon structure. */
void print_wep(struct weapon *wep)
{
	int i, type = wep->type;
	double curr, sum = 0;

	/* Print out the header. */
	printf("Name: %s\nType: ", wep->name);

	/* Create a string for the type. */
	if (type == REGULAR)
		printf("Regular");
	else if (type == FANG)
		printf("Fang");
	else if (type == SCYTHE)
		printf("Scythe");
	else if (type == KERIS)
		printf("Keris");
	else if (type == ZCB_DIAM_KAND)
		printf("ZCB w/diamond bolts (e) and Kandarin hard");
	else if (type == ZCB_DIAM_NOKA)
		printf("ZCB w/diamond bolts (e) and NO Kandarin hard");
	else if (type == RCB_DIAM_KAND)
		printf("Non-ZCB crossbow w/diamond bolts (e) and Kandarin hard");
	else if (type == RCB_DIAM_NOKA)
		printf("Non-ZCB crossbow w/diamond bolts (e) and NO Kandarin hard");
	else if (type == ZCB_RUBY_KAND)
		printf("ZCB w/ruby bolts (e) and Kandarin hard");
	else if (type == ZCB_RUBY_NOKA)
		printf("ZCB w/ruby bolts (e) and NO Kandarin hard");
	else if (type == RCB_RUBY_KAND)
		printf("Non-ZCB crossbow w/ruby bolts (e) and Kandarin hard");
	else if (type == RCB_RUBY_NOKA)
		printf("Non-ZCB crossbow w/ruby bolts (e) and NO Kandarin hard");
	else
		printf("Unknown type!");

	/* Finish the header. */
	printf("\nSpeed: %dt\nMax Hit: %d\nAccuracy: %.2f\nDistribution:\n",
			wep->spd, wep->max, wep->acc);

	/* Print all nonzero entries of the distribution. */
	for (i = 0; i < DIST_SIZE; i++) {
		curr = wep->dist[i];
		if (curr != 0.0)
			printf("%d: %.*f\n", i, PRECISION, curr);
		sum += curr;
	}

	/* And the sum, for a sanity check. */
	printf("Sum of distribution: %.*f\n\n", PRECISION, sum);
	return;
}

