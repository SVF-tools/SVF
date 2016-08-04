/**CFile***********************************************************************

  FileName    [testcudd.c]

  PackageName [cudd]

  Synopsis    [Sanity check tests for some CUDD functions.]

  Description [testcudd reads a matrix with real coefficients and
  transforms it into an ADD. It then performs various operations on
  the ADD and on the BDD corresponding to the ADD pattern. Finally,
  testcudd tests functions relate to Walsh matrices and matrix
  multiplication.]

  SeeAlso     []

  Author      [Fabio Somenzi]

  Copyright   [Copyright (c) 1995-2012, Regents of the University of Colorado

  All rights reserved.

  Redistribution and use in source and binary forms, with or without
  modification, are permitted provided that the following conditions
  are met:

  Redistributions of source code must retain the above copyright
  notice, this list of conditions and the following disclaimer.

  Redistributions in binary form must reproduce the above copyright
  notice, this list of conditions and the following disclaimer in the
  documentation and/or other materials provided with the distribution.

  Neither the name of the University of Colorado nor the names of its
  contributors may be used to endorse or promote products derived from
  this software without specific prior written permission.

  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
  "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
  LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
  FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
  COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
  INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
  BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
  LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
  CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
  LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
  ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
  POSSIBILITY OF SUCH DAMAGE.]

******************************************************************************/

#include "CUDD/util.h"
#include "CUDD/cuddInt.h"


/*---------------------------------------------------------------------------*/
/* Constant declarations                                                     */
/*---------------------------------------------------------------------------*/

#define TESTCUDD_VERSION	"TestCudd Version #1.0, Release date 3/17/01"

/*---------------------------------------------------------------------------*/
/* Variable declarations                                                     */
/*---------------------------------------------------------------------------*/

#ifndef lint
static char rcsid[] DD_UNUSED = "$Id: testcudd.c,v 1.23 2012/02/05 05:30:29 fabio Exp $";
#endif

static const char *onames[] = { "C", "M" }; /* names of functions to be dumped */

/**AutomaticStart*************************************************************/

/*---------------------------------------------------------------------------*/
/* Static function prototypes                                                */
/*---------------------------------------------------------------------------*/

static void usage (char * prog);
static FILE *open_file (char *filename, const char *mode);
static int testIterators (DdManager *dd, DdNode *M, DdNode *C, int pr);
static int testXor (DdManager *dd, DdNode *f, int pr, int nvars);
static int testHamming (DdManager *dd, DdNode *f, int pr);
static int testWalsh (DdManager *dd, int N, int cmu, int approach, int pr);
static int testSupport(DdManager *dd, DdNode *f, DdNode *g, int pr);

/**AutomaticEnd***************************************************************/


/**Function********************************************************************

  Synopsis    [Main function for testcudd.]

  Description []

  SideEffects [None]

  SeeAlso     []

******************************************************************************/
int
main(int argc, char * const *argv)
{
    FILE *fp;           /* pointer to input file */
    char *file = (char *) "";	/* input file name */
    FILE *dfp = NULL;	/* pointer to dump file */
    FILE *savefp = NULL;/* pointer to save current manager's stdout setting */
    char *dfile;	/* file for DD dump */
    DdNode *dfunc[2];	/* addresses of the functions to be dumped */
    DdManager *dd;	/* pointer to DD manager */
    DdNode *one;	/* fast access to constant function */
    DdNode *M;
    DdNode **x;		/* pointers to variables */
    DdNode **y;		/* pointers to variables */
    DdNode **xn;	/* complements of row variables */
    DdNode **yn_;	/* complements of column variables */
    DdNode **xvars;
    DdNode **yvars;
    DdNode *C;		/* result of converting from ADD to BDD */
    DdNode *ess;	/* cube of essential variables */
    DdNode *shortP;	/* BDD cube of shortest path */
    DdNode *largest;	/* BDD of largest cube */
    DdNode *shortA;	/* ADD cube of shortest path */
    DdNode *constN;	/* value returned by evaluation of ADD */
    DdNode *ycube;	/* cube of the negated y vars for c-proj */
    DdNode *CP;		/* C-Projection of C */
    DdNode *CPr;	/* C-Selection of C */
    int    length;	/* length of the shortest path */
    int    nx;			/* number of variables */
    int    ny;
    int    maxnx;
    int    maxny;
    int    m;
    int    n;
    int    N;
    int    cmu;			/* use CMU multiplication */
    int    pr;			/* verbose printout level */
    int    harwell;
    int    multiple;		/* read multiple matrices */
    int    ok;
    int    c;			/* variable to read in options */
    int    approach;		/* reordering approach */
    int    autodyn;		/* automatic reordering */
    int    groupcheck;		/* option for group sifting */
    int    profile;		/* print heap profile if != 0 */
    int    keepperm;		/* keep track of permutation */
    int    clearcache;		/* clear the cache after each matrix */
    int    blifOrDot;		/* dump format: 0 -> dot, 1 -> blif, ... */
    int    retval;		/* return value */
    int    i;			/* loop index */
    unsigned long startTime;	/* initial time */
    unsigned long   lapTime;
    int    size;
    unsigned int cacheSize, maxMemory;
    unsigned int nvars,nslots;

    startTime = util_cpu_time();

    approach = CUDD_REORDER_NONE;
    autodyn = 0;
    pr = 0;
    harwell = 0;
    multiple = 0;
    profile = 0;
    keepperm = 0;
    cmu = 0;
    N = 4;
    nvars = 4;
    cacheSize = 127;
    maxMemory = 0;
    nslots = CUDD_UNIQUE_SLOTS;
    clearcache = 0;
    groupcheck = CUDD_GROUP_CHECK7;
    dfile = NULL;
    blifOrDot = 0; /* dot format */

    /* Parse command line. */
    while ((c = getopt(argc, argv, "CDHMPS:a:bcd:g:hkmn:p:v:x:X:"))
	   != EOF) {
	switch(c) {
	case 'C':
	    cmu = 1;
	    break;
	case 'D':
	    autodyn = 1;
	    break;
	case 'H':
	    harwell = 1;
	    break;
	case 'M':
#ifdef MNEMOSYNE
	    (void) mnem_setrecording(0);
#endif
	    break;
	case 'P':
	    profile = 1;
	    break;
	case 'S':
	    nslots = atoi(optarg);
	    break;
	case 'X':
	    maxMemory = atoi(optarg);
	    break;
	case 'a':
	    approach = atoi(optarg);
	    break;
	case 'b':
	    blifOrDot = 1; /* blif format */
	    break;
	case 'c':
	    clearcache = 1;
	    break;
	case 'd':
	    dfile = optarg;
	    break;
	case 'g':
	    groupcheck = atoi(optarg);
	    break;
	case 'k':
	    keepperm = 1;
	    break;
	case 'm':
	    multiple = 1;
	    break;
	case 'n':
	    N = atoi(optarg);
	    break;
	case 'p':
	    pr = atoi(optarg);
	    break;
	case 'v':
	    nvars = atoi(optarg);
	    break;
	case 'x':
	    cacheSize = atoi(optarg);
	    break;
	case 'h':
	default:
	    usage(argv[0]);
	    break;
	}
    }

    if (argc - optind == 0) {
	file = (char *) "-";
    } else if (argc - optind == 1) {
	file = argv[optind];
    } else {
	usage(argv[0]);
    }
    if ((approach<0) || (approach>17)) {
	(void) fprintf(stderr,"Invalid approach: %d \n",approach);
	usage(argv[0]);
    }

    if (pr > 0) {
	(void) printf("# %s\n", TESTCUDD_VERSION);
	/* Echo command line and arguments. */
	(void) printf("#");
	for (i = 0; i < argc; i++) {
	    (void) printf(" %s", argv[i]);
	}
	(void) printf("\n");
	(void) fflush(stdout);
    }

    /* Initialize manager and provide easy reference to terminals. */
    dd = Cudd_Init(nvars,0,nslots,cacheSize,maxMemory);
    one = DD_ONE(dd);
    dd->groupcheck = (Cudd_AggregationType) groupcheck;
    if (autodyn) Cudd_AutodynEnable(dd,CUDD_REORDER_SAME);

    /* Open input file. */
    fp = open_file(file, "r");

    /* Open dump file if requested */
    if (dfile != NULL) {
	dfp = open_file(dfile, "w");
    }

    x = y = xn = yn_ = NULL;
    do {
	/* We want to start anew for every matrix. */
	maxnx = maxny = 0;
	nx = maxnx; ny = maxny;
	if (pr>0) lapTime = util_cpu_time();
	if (harwell) {
	    if (pr > 0) (void) printf(":name: ");
	    ok = Cudd_addHarwell(fp, dd, &M, &x, &y, &xn, &yn_, &nx, &ny,
	    &m, &n, 0, 2, 1, 2, pr);
	} else {
	    ok = Cudd_addRead(fp, dd, &M, &x, &y, &xn, &yn_, &nx, &ny,
	    &m, &n, 0, 2, 1, 2);
	    if (pr > 0)
		(void) printf(":name: %s: %d rows %d columns\n", file, m, n);
	}
	if (!ok) {
	    (void) fprintf(stderr, "Error reading matrix\n");
	    exit(1);
	}

	if (nx > maxnx) maxnx = nx;
	if (ny > maxny) maxny = ny;

	/* Build cube of negated y's. */
	ycube = DD_ONE(dd);
	Cudd_Ref(ycube);
	for (i = maxny - 1; i >= 0; i--) {
	    DdNode *tmpp;
	    tmpp = Cudd_bddAnd(dd,Cudd_Not(dd->vars[y[i]->index]),ycube);
	    if (tmpp == NULL) exit(2);
	    Cudd_Ref(tmpp);
	    Cudd_RecursiveDeref(dd,ycube);
	    ycube = tmpp;
	}
	/* Initialize vectors of BDD variables used by priority func. */
	xvars = ALLOC(DdNode *, nx);
	if (xvars == NULL) exit(2);
	for (i = 0; i < nx; i++) {
	    xvars[i] = dd->vars[x[i]->index];
	}
	yvars = ALLOC(DdNode *, ny);
	if (yvars == NULL) exit(2);
	for (i = 0; i < ny; i++) {
	    yvars[i] = dd->vars[y[i]->index];
	}

	/* Clean up */
	for (i=0; i < maxnx; i++) {
	    Cudd_RecursiveDeref(dd, x[i]);
	    Cudd_RecursiveDeref(dd, xn[i]);
	}
	FREE(x);
	FREE(xn);
	for (i=0; i < maxny; i++) {
	    Cudd_RecursiveDeref(dd, y[i]);
	    Cudd_RecursiveDeref(dd, yn_[i]);
	}
	FREE(y);
	FREE(yn_);

	if (pr>0) {(void) printf(":1: M"); Cudd_PrintDebug(dd,M,nx+ny,pr);}

	if (pr>0) (void) printf(":2: time to read the matrix = %s\n",
		    util_print_time(util_cpu_time() - lapTime));

	C = Cudd_addBddPattern(dd, M);
	if (C == 0) exit(2);
	Cudd_Ref(C);
	if (pr>0) {(void) printf(":3: C"); Cudd_PrintDebug(dd,C,nx+ny,pr);}

	/* Test iterators. */
	retval = testIterators(dd,M,C,pr);
	if (retval == 0) exit(2);

        if (pr > 0)
            cuddCacheProfile(dd,stdout);

	/* Test XOR */
	retval = testXor(dd,C,pr,nx+ny);
	if (retval == 0) exit(2);

	/* Test Hamming distance functions. */
	retval = testHamming(dd,C,pr);
	if (retval == 0) exit(2);

	/* Test selection functions. */
	CP = Cudd_CProjection(dd,C,ycube);
	if (CP == NULL) exit(2);
	Cudd_Ref(CP);
	if (pr>0) {(void) printf("ycube"); Cudd_PrintDebug(dd,ycube,nx+ny,pr);}
	if (pr>0) {(void) printf("CP"); Cudd_PrintDebug(dd,CP,nx+ny,pr);}

	if (nx == ny) {
	    CPr = Cudd_PrioritySelect(dd,C,xvars,yvars,(DdNode **)NULL,
		(DdNode *)NULL,ny,Cudd_Xgty);
	    if (CPr == NULL) exit(2);
	    Cudd_Ref(CPr);
	    if (pr>0) {(void) printf(":4: CPr"); Cudd_PrintDebug(dd,CPr,nx+ny,pr);}
	    if (CP != CPr) {
		(void) printf("CP != CPr!\n");
	    }
	    Cudd_RecursiveDeref(dd, CPr);
	}

	/* Test inequality generator. */
	{
	    int Nmin = ddMin(nx,ny);
	    int q;
	    DdGen *gen;
	    int *cube;
	    DdNode *f = Cudd_Inequality(dd,Nmin,2,xvars,yvars);
	    if (f == NULL) exit(2);
	    Cudd_Ref(f);
	    if (pr>0) {
		(void) printf(":4: ineq");
		Cudd_PrintDebug(dd,f,nx+ny,pr);
		if (pr>1) {
		    Cudd_ForeachPrime(dd,Cudd_Not(f),Cudd_Not(f),gen,cube) {
			for (q = 0; q < dd->size; q++) {
			    switch (cube[q]) {
			    case 0:
				(void) printf("1");
				break;
			    case 1:
				(void) printf("0");
				break;
			    case 2:
				(void) printf("-");
				break;
			    default:
				(void) printf("?");
			    }
			}
			(void) printf(" 1\n");
		    }
		    (void) printf("\n");
		}
	    }
	    Cudd_IterDerefBdd(dd, f);
	}
	FREE(xvars); FREE(yvars);

	Cudd_RecursiveDeref(dd, CP);

	/* Test functions for essential variables. */
	ess = Cudd_FindEssential(dd,C);
	if (ess == NULL) exit(2);
	Cudd_Ref(ess);
	if (pr>0) {(void) printf(":4: ess"); Cudd_PrintDebug(dd,ess,nx+ny,pr);}
	Cudd_RecursiveDeref(dd, ess);

	/* Test functions for shortest paths. */
	shortP = Cudd_ShortestPath(dd, M, NULL, NULL, &length);
	if (shortP == NULL) exit(2);
	Cudd_Ref(shortP);
	if (pr>0) {
	    (void) printf(":5: shortP"); Cudd_PrintDebug(dd,shortP,nx+ny,pr);
	}
	/* Test functions for largest cubes. */
	largest = Cudd_LargestCube(dd, Cudd_Not(C), &length);
	if (largest == NULL) exit(2);
	Cudd_Ref(largest);
	if (pr>0) {
	    (void) printf(":5b: largest");
	    Cudd_PrintDebug(dd,largest,nx+ny,pr);
	}
	Cudd_RecursiveDeref(dd, largest);

	/* Test Cudd_addEvalConst and Cudd_addIteConstant. */
	shortA = Cudd_BddToAdd(dd,shortP);
	if (shortA == NULL) exit(2);
	Cudd_Ref(shortA);
	Cudd_RecursiveDeref(dd, shortP);
	constN = Cudd_addEvalConst(dd,shortA,M);
	if (constN == DD_NON_CONSTANT) exit(2);
	if (Cudd_addIteConstant(dd,shortA,M,constN) != constN) exit(2);
	if (pr>0) {(void) printf("The value of M along the chosen shortest path is %g\n", cuddV(constN));}
	Cudd_RecursiveDeref(dd, shortA);

	shortP = Cudd_ShortestPath(dd, C, NULL, NULL, &length);
	if (shortP == NULL) exit(2);
	Cudd_Ref(shortP);
	if (pr>0) {
	    (void) printf(":6: shortP"); Cudd_PrintDebug(dd,shortP,nx+ny,pr);
	}

	/* Test Cudd_bddIteConstant and Cudd_bddLeq. */
	if (!Cudd_bddLeq(dd,shortP,C)) exit(2);
	if (Cudd_bddIteConstant(dd,Cudd_Not(shortP),one,C) != one) exit(2);
	Cudd_RecursiveDeref(dd, shortP);

        /* Experiment with support functions. */
        if (!testSupport(dd,M,ycube,pr)) {
            exit(2);
        }
	Cudd_RecursiveDeref(dd, ycube);

	if (profile) {
	    retval = cuddHeapProfile(dd);
	}

	size = dd->size;

	if (pr>0) {
	    (void) printf("Average distance: %g\n", Cudd_AverageDistance(dd));
	}

	/* Reorder if so requested. */
	if (approach != CUDD_REORDER_NONE) {
#ifndef DD_STATS
	    retval = Cudd_EnableReorderingReporting(dd);
	    if (retval == 0) {
		(void) fprintf(stderr,"Error reported by Cudd_EnableReorderingReporting\n");
		exit(3);
	    }
#endif
#ifdef DD_DEBUG
	    retval = Cudd_DebugCheck(dd);
	    if (retval != 0) {
		(void) fprintf(stderr,"Error reported by Cudd_DebugCheck\n");
		exit(3);
	    }
	    retval = Cudd_CheckKeys(dd);
	    if (retval != 0) {
		(void) fprintf(stderr,"Error reported by Cudd_CheckKeys\n");
		exit(3);
	    }
#endif
	    retval = Cudd_ReduceHeap(dd,(Cudd_ReorderingType)approach,5);
	    if (retval == 0) {
		(void) fprintf(stderr,"Error reported by Cudd_ReduceHeap\n");
		exit(3);
	    }
#ifndef DD_STATS
	    retval = Cudd_DisableReorderingReporting(dd);
	    if (retval == 0) {
		(void) fprintf(stderr,"Error reported by Cudd_DisableReorderingReporting\n");
		exit(3);
	    }
#endif
#ifdef DD_DEBUG
	    retval = Cudd_DebugCheck(dd);
	    if (retval != 0) {
		(void) fprintf(stderr,"Error reported by Cudd_DebugCheck\n");
		exit(3);
	    }
	    retval = Cudd_CheckKeys(dd);
	    if (retval != 0) {
		(void) fprintf(stderr,"Error reported by Cudd_CheckKeys\n");
		exit(3);
	    }
#endif
	    if (approach == CUDD_REORDER_SYMM_SIFT ||
	    approach == CUDD_REORDER_SYMM_SIFT_CONV) {
		Cudd_SymmProfile(dd,0,dd->size-1);
	    }

	    if (pr>0) {
		(void) printf("Average distance: %g\n", Cudd_AverageDistance(dd));
	    }

	    if (keepperm) {
		/* Print variable permutation. */
		(void) printf("Variable Permutation:");
		for (i=0; i<size; i++) {
		    if (i%20 == 0) (void) printf("\n");
		    (void) printf("%d ", dd->invperm[i]);
		}
		(void) printf("\n");
		(void) printf("Inverse Permutation:");
		for (i=0; i<size; i++) {
		    if (i%20 == 0) (void) printf("\n");
		    (void) printf("%d ", dd->perm[i]);
		}
		(void) printf("\n");
	    }

	    if (pr>0) {(void) printf("M"); Cudd_PrintDebug(dd,M,nx+ny,pr);}

	    if (profile) {
		retval = cuddHeapProfile(dd);
	    }

	}

	/* Dump DDs of C and M if so requested. */
	if (dfile != NULL) {
	    dfunc[0] = C;
	    dfunc[1] = M;
	    if (blifOrDot == 1) {
		/* Only dump C because blif cannot handle ADDs */
		retval = Cudd_DumpBlif(dd,1,dfunc,NULL,(char **)onames,
				       NULL,dfp,0);
	    } else {
		retval = Cudd_DumpDot(dd,2,dfunc,NULL,(char **)onames,dfp);
	    }
	    if (retval != 1) {
		(void) fprintf(stderr,"abnormal termination\n");
		exit(2);
	    }
	}

	Cudd_RecursiveDeref(dd, C);
	Cudd_RecursiveDeref(dd, M);

	if (clearcache) {
	    if (pr>0) {(void) printf("Clearing the cache... ");}
	    for (i = dd->cacheSlots - 1; i>=0; i--) {
		dd->cache[i].data = NULL;
	    }
	    if (pr>0) {(void) printf("done\n");}
	}
	if (pr>0) {
	    (void) printf("Number of variables = %6d\t",dd->size);
	    (void) printf("Number of slots     = %6u\n",dd->slots);
	    (void) printf("Number of keys      = %6u\t",dd->keys);
	    (void) printf("Number of min dead  = %6u\n",dd->minDead);
	}

    } while (multiple && !feof(fp));

    fclose(fp);
    if (dfile != NULL) {
	fclose(dfp);
    }

    /* Second phase: experiment with Walsh matrices. */
    if (!testWalsh(dd,N,cmu,approach,pr)) {
	exit(2);
    }

    /* Check variable destruction. */
    assert(cuddDestroySubtables(dd,3));
    if (pr == 0) {
        savefp = Cudd_ReadStdout(dd);
        Cudd_SetStdout(dd,fopen("/dev/null","a"));
    }
    assert(Cudd_DebugCheck(dd) == 0);
    assert(Cudd_CheckKeys(dd) == 0);
    if (pr == 0) {
        Cudd_SetStdout(dd,savefp);
    }

    retval = Cudd_CheckZeroRef(dd);
    ok = retval != 0;  /* ok == 0 means O.K. */
    if (retval != 0) {
	(void) fprintf(stderr,
	    "%d non-zero DD reference counts after dereferencing\n", retval);
    }

    if (pr > 0) {
	(void) Cudd_PrintInfo(dd,stdout);
    }

    Cudd_Quit(dd);

#ifdef MNEMOSYNE
    mnem_writestats();
#endif

    if (pr>0) (void) printf("total time = %s\n",
		util_print_time(util_cpu_time() - startTime));

    if (pr > 0) util_print_cpu_stats(stdout);
    return ok;
    /* NOTREACHED */

} /* end of main */


/*---------------------------------------------------------------------------*/
/* Definition of static functions                                            */
/*---------------------------------------------------------------------------*/


/**Function********************************************************************

  Synopsis    [Prints usage info for testcudd.]

  Description []

  SideEffects [None]

  SeeAlso     []

******************************************************************************/
static void
usage(char *prog)
{
    (void) fprintf(stderr, "usage: %s [options] [file]\n", prog);
    (void) fprintf(stderr, "   -C\t\tuse CMU multiplication algorithm\n");
    (void) fprintf(stderr, "   -D\t\tenable automatic dynamic reordering\n");
    (void) fprintf(stderr, "   -H\t\tread matrix in Harwell format\n");
    (void) fprintf(stderr, "   -M\t\tturns off memory allocation recording\n");
    (void) fprintf(stderr, "   -P\t\tprint BDD heap profile\n");
    (void) fprintf(stderr, "   -S n\t\tnumber of slots for each subtable\n");
    (void) fprintf(stderr, "   -X n\t\ttarget maximum memory in bytes\n");
    (void) fprintf(stderr, "   -a n\t\tchoose reordering approach (0-13)\n");
    (void) fprintf(stderr, "   \t\t\t0: same as autoMethod\n");
    (void) fprintf(stderr, "   \t\t\t1: no reordering (default)\n");
    (void) fprintf(stderr, "   \t\t\t2: random\n");
    (void) fprintf(stderr, "   \t\t\t3: pivot\n");
    (void) fprintf(stderr, "   \t\t\t4: sifting\n");
    (void) fprintf(stderr, "   \t\t\t5: sifting to convergence\n");
    (void) fprintf(stderr, "   \t\t\t6: symmetric sifting\n");
    (void) fprintf(stderr, "   \t\t\t7: symmetric sifting to convergence\n");
    (void) fprintf(stderr, "   \t\t\t8-10: window of size 2-4\n");
    (void) fprintf(stderr, "   \t\t\t11-13: window of size 2-4 to conv.\n");
    (void) fprintf(stderr, "   \t\t\t14: group sifting\n");
    (void) fprintf(stderr, "   \t\t\t15: group sifting to convergence\n");
    (void) fprintf(stderr, "   \t\t\t16: simulated annealing\n");
    (void) fprintf(stderr, "   \t\t\t17: genetic algorithm\n");
    (void) fprintf(stderr, "   -b\t\tuse blif as format for dumps\n");
    (void) fprintf(stderr, "   -c\t\tclear the cache after each matrix\n");
    (void) fprintf(stderr, "   -d file\tdump DDs to file\n");
    (void) fprintf(stderr, "   -g\t\tselect aggregation criterion (0,5,7)\n");
    (void) fprintf(stderr, "   -h\t\tprints this message\n");
    (void) fprintf(stderr, "   -k\t\tprint the variable permutation\n");
    (void) fprintf(stderr, "   -m\t\tread multiple matrices (only with -H)\n");
    (void) fprintf(stderr, "   -n n\t\tnumber of variables\n");
    (void) fprintf(stderr, "   -p n\t\tcontrol verbosity\n");
    (void) fprintf(stderr, "   -v n\t\tinitial variables in the unique table\n");
    (void) fprintf(stderr, "   -x n\t\tinitial size of the cache\n");
    exit(2);
} /* end of usage */


/**Function********************************************************************

  Synopsis    [Opens a file.]

  Description [Opens a file, or fails with an error message and exits.
  Allows '-' as a synonym for standard input.]

  SideEffects [None]

  SeeAlso     []

******************************************************************************/
static FILE *
open_file(char *filename, const char *mode)
{
    FILE *fp;

    if (strcmp(filename, "-") == 0) {
	return mode[0] == 'r' ? stdin : stdout;
    } else if ((fp = fopen(filename, mode)) == NULL) {
	perror(filename);
	exit(1);
    }
    return fp;

} /* end of open_file */


/**Function********************************************************************

  Synopsis    [Tests Walsh matrix multiplication.]

  Description [Tests Walsh matrix multiplication.  Return 1 if successful;
  0 otherwise.]

  SideEffects [May create new variables in the manager.]

  SeeAlso     []

******************************************************************************/
static int
testWalsh(
  DdManager *dd /* manager */,
  int N /* number of variables */,
  int cmu /* use CMU approach to matrix multiplication */,
  int approach /* reordering approach */,
  int pr /* verbosity level */)
{
    DdNode *walsh1, *walsh2, *wtw;
    DdNode **x, **v, **z;
    int i, retval;
    DdNode *one = DD_ONE(dd);
    DdNode *zero = DD_ZERO(dd);

    if (N > 3) {
	x = ALLOC(DdNode *,N);
	v = ALLOC(DdNode *,N);
	z = ALLOC(DdNode *,N);

	for (i = N-1; i >= 0; i--) {
	    Cudd_Ref(x[i]=cuddUniqueInter(dd,3*i,one,zero));
	    Cudd_Ref(v[i]=cuddUniqueInter(dd,3*i+1,one,zero));
	    Cudd_Ref(z[i]=cuddUniqueInter(dd,3*i+2,one,zero));
	}
	Cudd_Ref(walsh1 = Cudd_addWalsh(dd,v,z,N));
	if (pr>0) {(void) printf("walsh1"); Cudd_PrintDebug(dd,walsh1,2*N,pr);}
	Cudd_Ref(walsh2 = Cudd_addWalsh(dd,x,v,N));
	if (cmu) {
	    Cudd_Ref(wtw = Cudd_addTimesPlus(dd,walsh2,walsh1,v,N));
	} else {
	    Cudd_Ref(wtw = Cudd_addMatrixMultiply(dd,walsh2,walsh1,v,N));
	}
	if (pr>0) {(void) printf("wtw"); Cudd_PrintDebug(dd,wtw,2*N,pr);}

	if (approach != CUDD_REORDER_NONE) {
#ifdef DD_DEBUG
	    retval = Cudd_DebugCheck(dd);
	    if (retval != 0) {
		(void) fprintf(stderr,"Error reported by Cudd_DebugCheck\n");
		return(0);
	    }
#endif
	    retval = Cudd_ReduceHeap(dd,(Cudd_ReorderingType)approach,5);
	    if (retval == 0) {
		(void) fprintf(stderr,"Error reported by Cudd_ReduceHeap\n");
		return(0);
	    }
#ifdef DD_DEBUG
	    retval = Cudd_DebugCheck(dd);
	    if (retval != 0) {
		(void) fprintf(stderr,"Error reported by Cudd_DebugCheck\n");
		return(0);
	    }
#endif
	    if (approach == CUDD_REORDER_SYMM_SIFT ||
	    approach == CUDD_REORDER_SYMM_SIFT_CONV) {
		Cudd_SymmProfile(dd,0,dd->size-1);
	    }
	}
	/* Clean up. */
	Cudd_RecursiveDeref(dd, wtw);
	Cudd_RecursiveDeref(dd, walsh1);
	Cudd_RecursiveDeref(dd, walsh2);
	for (i=0; i < N; i++) {
	    Cudd_RecursiveDeref(dd, x[i]);
	    Cudd_RecursiveDeref(dd, v[i]);
	    Cudd_RecursiveDeref(dd, z[i]);
	}
	FREE(x);
	FREE(v);
	FREE(z);
    }
    return(1);

} /* end of testWalsh */

/**Function********************************************************************

  Synopsis    [Tests iterators.]

  Description [Tests iterators on cubes and nodes.]

  SideEffects [None]

  SeeAlso     []

******************************************************************************/
static int
testIterators(
  DdManager *dd,
  DdNode *M,
  DdNode *C,
  int pr)
{
    int *cube;
    CUDD_VALUE_TYPE value;
    DdGen *gen;
    int q;

    /* Test iterator for cubes. */
    if (pr>1) {
	(void) printf("Testing iterator on cubes:\n");
	Cudd_ForeachCube(dd,M,gen,cube,value) {
	    for (q = 0; q < dd->size; q++) {
		switch (cube[q]) {
		case 0:
		    (void) printf("0");
		    break;
		case 1:
		    (void) printf("1");
		    break;
		case 2:
		    (void) printf("-");
		    break;
		default:
		    (void) printf("?");
		}
	    }
	    (void) printf(" %g\n",value);
	}
	(void) printf("\n");
    }

    if (pr>1) {
	(void) printf("Testing prime expansion of cubes:\n");
	if (!Cudd_bddPrintCover(dd,C,C)) return(0);
    }

    if (pr>1) {
	(void) printf("Testing iterator on primes (CNF):\n");
	Cudd_ForeachPrime(dd,Cudd_Not(C),Cudd_Not(C),gen,cube) {
	    for (q = 0; q < dd->size; q++) {
		switch (cube[q]) {
		case 0:
		    (void) printf("1");
		    break;
		case 1:
		    (void) printf("0");
		    break;
		case 2:
		    (void) printf("-");
		    break;
		default:
		    (void) printf("?");
		}
	    }
	    (void) printf(" 1\n");
	}
	(void) printf("\n");
    }

    /* Test iterator on nodes. */
    if (pr>2) {
	DdNode *node;
	(void) printf("Testing iterator on nodes:\n");
	Cudd_ForeachNode(dd,M,gen,node) {
	    if (Cudd_IsConstant(node)) {
#if SIZEOF_VOID_P == 8
		(void) printf("ID = 0x%lx\tvalue = %-9g\n",
			      (ptruint) node /
			      (ptruint) sizeof(DdNode),
			      Cudd_V(node));
#else
		(void) printf("ID = 0x%x\tvalue = %-9g\n",
			      (ptruint) node /
			      (ptruint) sizeof(DdNode),
			      Cudd_V(node));
#endif
	    } else {
#if SIZEOF_VOID_P == 8
		(void) printf("ID = 0x%lx\tindex = %u\tr = %u\n",
			      (ptruint) node /
			      (ptruint) sizeof(DdNode),
			      node->index, node->ref);
#else
		(void) printf("ID = 0x%x\tindex = %u\tr = %u\n",
			      (ptruint) node /
			      (ptruint) sizeof(DdNode),
			      node->index, node->ref);
#endif
	    }
	}
	(void) printf("\n");
    }
    return(1);

} /* end of testIterators */


/**Function********************************************************************

  Synopsis    [Tests the functions related to the exclusive OR.]

  Description [Tests the functions related to the exclusive OR. It
  builds the boolean difference of the given function in three
  different ways and checks that the results is the same. Returns 1 if
  successful; 0 otherwise.]

  SideEffects [None]

  SeeAlso     []

******************************************************************************/
static int
testXor(DdManager *dd, DdNode *f, int pr, int nvars)
{
    DdNode *f1, *f0, *res1, *res2;
    int x;

    /* Extract cofactors w.r.t. mid variable. */
    x = nvars / 2;
    f1 = Cudd_Cofactor(dd,f,dd->vars[x]);
    if (f1 == NULL) return(0);
    Cudd_Ref(f1);

    f0 = Cudd_Cofactor(dd,f,Cudd_Not(dd->vars[x]));
    if (f0 == NULL) {
	Cudd_RecursiveDeref(dd,f1);
	return(0);
    }
    Cudd_Ref(f0);

    /* Compute XOR of cofactors with ITE. */
    res1 = Cudd_bddIte(dd,f1,Cudd_Not(f0),f0);
    if (res1 == NULL) return(0);
    Cudd_Ref(res1);

    if (pr>0) {(void) printf("xor1"); Cudd_PrintDebug(dd,res1,nvars,pr);}

    /* Compute XOR of cofactors with XOR. */
    res2 = Cudd_bddXor(dd,f1,f0);
    if (res2 == NULL) {
	Cudd_RecursiveDeref(dd,res1);
	return(0);
    }
    Cudd_Ref(res2);

    if (res1 != res2) {
	if (pr>0) {(void) printf("xor2"); Cudd_PrintDebug(dd,res2,nvars,pr);}
	Cudd_RecursiveDeref(dd,res1);
	Cudd_RecursiveDeref(dd,res2);
	return(0);
    }
    Cudd_RecursiveDeref(dd,res1);
    Cudd_RecursiveDeref(dd,f1);
    Cudd_RecursiveDeref(dd,f0);

    /* Compute boolean difference directly. */
    res1 = Cudd_bddBooleanDiff(dd,f,x);
    if (res1 == NULL) {
	Cudd_RecursiveDeref(dd,res2);
	return(0);
    }
    Cudd_Ref(res1);

    if (res1 != res2) {
	if (pr>0) {(void) printf("xor3"); Cudd_PrintDebug(dd,res1,nvars,pr);}
	Cudd_RecursiveDeref(dd,res1);
	Cudd_RecursiveDeref(dd,res2);
	return(0);
    }
    Cudd_RecursiveDeref(dd,res1);
    Cudd_RecursiveDeref(dd,res2);
    return(1);

} /* end of testXor */


/**Function********************************************************************

  Synopsis    [Tests the Hamming distance functions.]

  Description [Tests the Hammming distance functions. Returns
  1 if successful; 0 otherwise.]

  SideEffects [None]

  SeeAlso     []

******************************************************************************/
static int
testHamming(
  DdManager *dd,
  DdNode *f,
  int pr)
{
    DdNode **vars, *minBdd, *zero, *scan;
    int i;
    int d;
    int *minterm;
    int size = Cudd_ReadSize(dd);

    vars = ALLOC(DdNode *, size);
    if (vars == NULL) return(0);
    for (i = 0; i < size; i++) {
	vars[i] = Cudd_bddIthVar(dd,i);
    }

    minBdd = Cudd_bddPickOneMinterm(dd,Cudd_Not(f),vars,size);
    Cudd_Ref(minBdd);
    if (pr > 0) {
	(void) printf("Chosen minterm for Hamming distance test: ");
	Cudd_PrintDebug(dd,minBdd,size,pr);
    }

    minterm = ALLOC(int,size);
    if (minterm == NULL) {
	FREE(vars);
	Cudd_RecursiveDeref(dd,minBdd);
	return(0);
    }
    scan = minBdd;
    zero = Cudd_Not(DD_ONE(dd));
    while (!Cudd_IsConstant(scan)) {
	DdNode *R = Cudd_Regular(scan);
	DdNode *T = Cudd_T(R);
	DdNode *E = Cudd_E(R);
	if (R != scan) {
	    T = Cudd_Not(T);
	    E = Cudd_Not(E);
	}
	if (T == zero) {
	    minterm[R->index] = 0;
	    scan = E;
	} else {
	    minterm[R->index] = 1;
	    scan = T;
	}
    }
    Cudd_RecursiveDeref(dd,minBdd);

    d = Cudd_MinHammingDist(dd,f,minterm,size);

    if (pr > 0)
        (void) printf("Minimum Hamming distance = %d\n", d);

    FREE(vars);
    FREE(minterm);
    return(1);

} /* end of testHamming */


/**Function********************************************************************

  Synopsis    [Tests the support functions.]

  Description [Tests the support functions. Returns
  1 if successful; 0 otherwise.]

  SideEffects [None]

  SeeAlso     []

******************************************************************************/
static int
testSupport(
  DdManager *dd,
  DdNode *f,
  DdNode *g,
  int pr)
{
    DdNode *sb, *common, *onlyF, *onlyG;
    DdNode *F[2];
    int *support;
    int ret, ssize;
    int size = Cudd_ReadSize(dd);

    sb = Cudd_Support(dd, f);
    if (sb == NULL) return(0);
    Cudd_Ref(sb);
    if (pr > 0) {
	(void) printf("Support of f: ");
	Cudd_PrintDebug(dd,sb,size,pr);
    }
    Cudd_RecursiveDeref(dd, sb);

    ssize = Cudd_SupportIndices(dd, f, &support);
    if (ssize == CUDD_OUT_OF_MEM) return(0);
    if (pr > 0) {
	(void) printf("Size of the support of f: %d\n", ssize);
    }
    FREE(support);

    ssize = Cudd_SupportSize(dd, f);
    if (pr > 0) {
	(void) printf("Size of the support of f: %d\n", ssize);
    }

    F[0] = f;
    F[1] = g;
    sb = Cudd_VectorSupport(dd, F, 2);
    if (sb == NULL) return(0);
    Cudd_Ref(sb);
    if (pr > 0) {
	(void) printf("Support of f and g: ");
	Cudd_PrintDebug(dd,sb,size,pr);
    }
    Cudd_RecursiveDeref(dd, sb);

    ssize = Cudd_VectorSupportIndices(dd, F, 2, &support);
    if (ssize == CUDD_OUT_OF_MEM) return(0);
    if (pr > 0) {
	(void) printf("Size of the support of f and g: %d\n", ssize);
    }
    FREE(support);

    ssize = Cudd_VectorSupportSize(dd, F, 2);
    if (pr > 0) {
	(void) printf("Size of the support of f and g: %d\n", ssize);
    }

    ret = Cudd_ClassifySupport(dd, f, g, &common, &onlyF, &onlyG);
    if (ret == 0) return(0);
    Cudd_Ref(common); Cudd_Ref(onlyF); Cudd_Ref(onlyG);
    if (pr > 0) {
	(void) printf("Support common to f and g: ");
	Cudd_PrintDebug(dd,common,size,pr);
	(void) printf("Support private to f: ");
	Cudd_PrintDebug(dd,onlyF,size,pr);
	(void) printf("Support private to g: ");
	Cudd_PrintDebug(dd,onlyG,size,pr);
    }
    Cudd_RecursiveDeref(dd, common);
    Cudd_RecursiveDeref(dd, onlyF);
    Cudd_RecursiveDeref(dd, onlyG);

    return(1);

} /* end of testSupport */
