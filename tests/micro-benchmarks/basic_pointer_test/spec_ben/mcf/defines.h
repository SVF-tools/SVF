/**************************************************************************
FILE: defines.h

AUTHOR: Andreas Loebel

    Konrad-Zuse-Zentrum fuer Informationstechnik Berlin (ZIB)
    Takustr. 7
    14195 Berlin-Dahlem

Copyright (c) 1998,1999   ZIB Berlin   All Rights Reserved
**************************************************************************/
/*  LAST EDIT: Fri Feb 26 00:24:53 1999 by Andreas Loebel (alf)  */


#ifndef _DEFINES_H
#define _DEFINES_H

#include <stdio.h>
#ifndef _WIN32
#include <unistd.h>
#endif
#include <math.h>
#include <stdlib.h>
#include <limits.h>
#include <string.h>
#include <errno.h>
#include <ctype.h>

#ifdef INTERNAL_TIMING
#include <time.h>
#include <sys/times.h>
#include <sys/time.h>
#endif


#include "prototyp.h"


#define UNBOUNDED        1000000000
#define ZERO             0
#define MAX_ART_COST     (long)(100000000L)
#define ARITHMETIC_TYPE "I"



#define FIXED           -1
#define BASIC            0
#define AT_LOWER         1
#define AT_UPPER         2
/* #define AT_ZERO           3  NOT ALLOWED FOR THE SPEC VERSION */
#undef AT_ZERO


#define UP    1
#define DOWN  0



typedef long flow_t;
typedef long cost_t;




#ifndef NULL
#define NULL 0
#endif


#ifndef ABS
#define ABS( x ) ( ((x) >= 0) ? ( x ) : -( x ) )
#endif


#ifndef MAX
#define MAX(a,b) (((a) > (b)) ? (a) : (b))
#endif


#ifndef SET_ZERO
#define SET_ZERO( vec, n ) if( vec ) memset( (void *)vec, 0, (size_t)n )
#endif


#ifndef FREE
#define FREE( vec ) if( vec ) free( (void *)vec )
#endif



typedef struct node
{
    long number;
    char *ident;
    struct node *pred, *child, *sibling, *sibling_prev;     
    long depth; 
    long orientation; 
    struct arc *basic_arc; 
    struct arc *firstout, *firstin;
    cost_t potential; 
    flow_t flow;
    size_t mark;
    long time;
} node_t;



typedef struct arc
{
    node_t *tail, *head;
    struct arc *nextout, *nextin;
    cost_t cost, org_cost;
    flow_t flow;
    long ident;
} arc_t;



typedef struct network
{
    char inputfile[200];
    char clustfile[200];
    long n, n_trips;
    long max_m, m, m_org, m_impl;

    long primal_unbounded;
    long dual_unbounded;
    long perturbed;
    long feasible;
    long eps;
    long opt_tol;
    long feas_tol;
    long pert_val;
    long bigM;
    double optcost;  
    cost_t ignore_impl;
    node_t *nodes, *stop_nodes;
    arc_t *arcs, *stop_arcs;
    arc_t *dummy_arcs, *stop_dummy; 
    long iterations;
    long bound_exchanges;
    long checksum;
} network_t;


#endif
