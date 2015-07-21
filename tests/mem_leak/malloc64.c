/*
 * malloc64.c
 *
 *  SAFEMALLOC and Never free
 *  Test saber handling assert functions in nested loops
 *  Created on: May 1, 2014
 *      Author: Yulei Sui
 */

/// From parsec ferret benchmark tp.c
#include "aliascheck.h"

#include <assert.h>
#include <stdlib.h>
//#include <values.h>
//#include <cass.h>

#define type_alloc(type)        ((type*)malloc(sizeof(type)))
#define type_calloc(type,col)       ((type*)calloc(sizeof(type), col))
#define type_realloc(type,ptr, col) ((type*)realloc(ptr, sizeof(type)*col))
#define matrix_check(matrix)        __matrix_check((void **)matrix)
#define type_matrix_row(matrix,type)    __matrix_row((void **)matrix)
#define type_matrix_col(matrix,type)    __matrix_col((void **)matrix, sizeof(type))
#define type_matrix_alloc(type,row,col) (type **)__matrix_alloc(row, col, sizeof (type))


//#define BINSET

#ifdef BINSET
#define SET_TYPE        unsigned long long
#define SET_INIT(set, size) do { set = 0; assert(size <= 8 * sizeof(unsigned long long));} while (0)
#define SET_TEST(set,mem)   ((set) & ((unsigned long long)1 << (mem)))
#define SET_ADD(set,mem)    do { set |= (unsigned long long )1 << (mem); } while (0)
#define SET_CLEANUP(set)
#else
#define SET_TYPE        char *
#define SET_INIT(set, size) do { set = (char *)type_calloc(char, size); } while(0)
#define SET_TEST(set,mem)   (set[mem])
#define SET_ADD(set,mem)    do { set[mem] = 1; } while (0)
#define SET_CLEANUP(set)    free(set)

#endif


void **__matrix_alloc (size_t row, size_t col, size_t size)
{
	size_t i;
	void **idx = (void **)malloc((row + 1)* sizeof (void *));
	assert(idx != 0);
	idx[0] = NFRMALLOC(row * col);
	assert(idx[0] != 0);
	for (i = 1; i <= row; i++)
	{
		idx[i] = idx[i-1] + col * size;
	}
	assert(idx[row] - idx[0] == row * col * size);
	idx[row] = 0;
	return idx;
}

struct sol
{
    int i, j;
    float value;
    int flow, dir;
    float sigma;
    struct sol *next, *prev;
};

/* find  initial solution using Vogel approximation */
/* find  initial solution using Vogel approximation */
static void tp_init_vogel (int nrow, float *row, int ncol, float *col, float **cost, struct sol ***_sol)
{
	SET_TYPE r_del;
	SET_TYPE c_del;

	int i, j, cnt;
	int lr, lc;

	float max;
	int mrow, mcol;

	struct sol **sol;
	r_del = SAFEMALLOC(nrow);
	c_del = SAFEMALLOC(ncol);

	sol = type_matrix_alloc(struct sol, nrow, ncol);
	for (i = 0; i < nrow; i++)
		for (j = 0; j < ncol; j++)
		{
			sol[i][j].i = i;
			sol[i][j].j = j;
			sol[i][j].flow = 0;
		}

	lr = nrow;
	lc = ncol;


	while (lr + lc > 2)
	{
		max = 0.0;
		mrow = mcol = -1;

		for (i = 0; i < nrow; i++) if (!SET_TEST(r_del, i))
		{
			float m1, m2;	/* m1 smallest, m2 2nd smallest */
			int m1_idx, m2_idx;
			m1 = m2 = 0.0;
			m1_idx = m2_idx = -1;
			for (j = 0; j < ncol; j++) if (!SET_TEST(c_del, j))
			{
				if ((m2_idx < 0) || (cost[i][j] < m2))
				{
					if ((m1_idx < 0) || cost[i][j] < m1)
					{
						m2 = m1;
						m2_idx = m1_idx;
						m1 = cost[i][j];
						m1_idx = j;
					}
					else
					{
						m2 = cost[i][j];
						m2_idx = j;
					}
				}
			}

			assert(m1_idx >= 0);
			if (m2_idx < 0) continue;

			if ((mrow < 0) || (m2 - m1 > max))
			{
				max = m2 - m1;
				mrow = i;
				mcol = m1_idx;
			}
		}

		for (i = 0; i < ncol; i++) if (!SET_TEST(c_del, i))
		{
			float m1, m2;	/* m1 smallest, m2 2nd smallest */
			int m1_idx, m2_idx;
			m1 = m2 = 0;
			m1_idx = m2_idx = -1;
			for (j = 0; j < nrow; j++) if (!SET_TEST(r_del, j))
			{
				if ((m2_idx < 0) || (cost[j][i] < m2))
				{
					if ((m1_idx < 0) || (cost[j][i] < m1))
					{
						m2 = m1;
						m2_idx = m1_idx;
						m1 = cost[j][i];
						m1_idx = j;
					}
					else
					{
						m2 = cost[j][i];
						m2_idx = j;
					}
				}
			}

			assert(m1_idx >= 0);
			if (m2_idx < 0) continue;

			if ((mrow < 0) || (m2 - m1 > max))
			{
				max = m2 - m1;
				mrow = m1_idx;
				mcol = i;
			}
		}

		assert(mrow >= 0);
		assert(mcol >= 0);
		/*
		assert(mrow >= 0);
		assert(mrow < nrow);
		assert(mcol >= 0);
		assert(mcol < ncol);
		*/
		assert(!SET_TEST(r_del, mrow));
		assert(!SET_TEST(c_del, mcol));

		if ((lr > 1) && ((row[mrow] <= col[mcol]) || (lc <= 1)))
		{
			sol[mrow][mcol].flow = 1;
			sol[mrow][mcol].value = row[mrow];
			col[mcol] -= row[mrow];
			row[mrow] = 0.0;
			if (col[mcol] < 0) col[mcol] = 0.0;
			SET_ADD(r_del, mrow);
			lr--;
		}
		else
		{
			assert(lc > 1);
			sol[mrow][mcol].flow = 1;
			sol[mrow][mcol].value = col[mcol];
			row[mrow] -= col[mcol];
			col[mcol] = 0.0;
			SET_ADD(c_del, mcol);
			lc--;
		}

	}

	assert(lc == 1);
	assert(lr == 1);

	for (;;)
	{
		mrow = -1;
		for (i = 0; i < nrow; i++) if (!SET_TEST(r_del, i))
		{
			mcol = -1;
			for (j = 0; j < ncol; j++) if (!SET_TEST(c_del, j))
			{
				mcol = j;
				break;
			}
			if (mcol >= 0)
			{
				mrow = i;
				break;
			}
		}
		if (mrow < 0 || mcol < 0) break;
		assert(!SET_TEST(r_del, mrow));
		assert(!SET_TEST(c_del, mcol));

		cnt++;

		if (row[mrow] < col[mcol])
		{
			sol[mrow][mcol].flow = 1;
			sol[mrow][mcol].value = row[mrow];
			col[mcol] -= row[mrow];
			row[mrow] = 0.0;
			SET_ADD(r_del, mrow);
		}
		else
		{
			sol[mrow][mcol].flow = 1;
			sol[mrow][mcol].value = col[mcol];
			row[mrow] -= col[mcol];
			col[mcol] = 0.0;
			SET_ADD(c_del, mcol);
		}
	}


	SET_CLEANUP(r_del);
	SET_CLEANUP(c_del);

	*_sol = sol;
}
int foo(){
 tp_init_vogel(0,0,0,0,0,0);
}
