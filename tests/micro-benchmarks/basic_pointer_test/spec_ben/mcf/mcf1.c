/*
 * mcf1.c
 *
 *  Created on: Jun 29, 2011
 *      Author: rockysui
 */

//#include "psimplex.h"
#include "defines.h"
#define K 300
#define B 100

#define TEST_MIN( nod, ex, comp, op ) \
{ \
      if( *delta op (comp) ) \
      { \
            iminus = nod; \
            *delta = (comp); \
            *xchange = ex; \
      } \
}

#ifdef _PROTO_
cost_t bea_compute_red_cost( arc_t *arc )
#else
cost_t bea_compute_red_cost( arc )
    arc_t *arc;
#endif
{
    return arc->cost - arc->tail->potential + arc->head->potential;
}







#ifdef _PROTO_
int bea_is_dual_infeasible( arc_t *arc, cost_t red_cost )
#else
int bea_is_dual_infeasible( arc, red_cost )
    arc_t *arc;
    cost_t red_cost;
#endif
{
    return(    (red_cost < 0 && arc->ident == AT_LOWER)
            || (red_cost > 0 && arc->ident == AT_UPPER) );
}







typedef struct basket
{
    arc_t *a;
    cost_t cost;
    cost_t abs_cost;
} BASKET;

static long basket_size;
static BASKET basket[B+K+1];
static BASKET *perm[B+K+1];



#ifdef _PROTO_
void sort_basket( long min, long max )
#else
void sort_basket( min, max )
    long min, max;
#endif
{
    long l, r;
    cost_t cut;
    BASKET *xchange;

    l = min; r = max;

    cut = perm[ (long)( (l+r) / 2 ) ]->abs_cost;

    do
    {
        while( perm[l]->abs_cost > cut )
            l++;
        while( cut > perm[r]->abs_cost )
            r--;

        if( l < r )
        {
            xchange = perm[l];
            perm[l] = perm[r];
            perm[r] = xchange;
        }
        if( l <= r )
        {
            l++; r--;
        }

    }
    while( l <= r );

    if( min < r )
        sort_basket( min, r );
    if( l < max && l <= B )
        sort_basket( l, max );
}






static long nr_group;
static long group_pos;


static long initialize = 1;


#ifdef _PROTO_
arc_t *primal_bea_mpp( long m,  arc_t *arcs, arc_t *stop_arcs,
                              cost_t *red_cost_of_bea )
#else
arc_t *primal_bea_mpp( m, arcs, stop_arcs, red_cost_of_bea )
    long m;
    arc_t *arcs;
    arc_t *stop_arcs;
    cost_t *red_cost_of_bea;
#endif
{
    long i, next, old_group_pos;
    arc_t *arc;
    cost_t red_cost;

    if( initialize )
    {
        for( i=1; i < K+B+1; i++ )
            perm[i] = &(basket[i]);
        nr_group = ( (m-1) / K ) + 1;
        group_pos = 0;
        basket_size = 0;
        initialize = 0;
    }
    else
    {
        for( i = 2, next = 0; i <= B && i <= basket_size; i++ )
        {
            arc = perm[i]->a;
            red_cost = bea_compute_red_cost( arc );
            if( bea_is_dual_infeasible( arc, red_cost ) )
            {
                next++;
                perm[next]->a = arc;
                perm[next]->cost = red_cost;
                perm[next]->abs_cost = ABS(red_cost);
            }
                }
        basket_size = next;
        }

    old_group_pos = group_pos;

NEXT:
    /* price next group */
    arc = arcs + group_pos;
    for( ; arc < stop_arcs; arc += nr_group )
    {
        if( arc->ident > BASIC )
        {
            red_cost = bea_compute_red_cost( arc );
            if( bea_is_dual_infeasible( arc, red_cost ) )
            {
                basket_size++;
                perm[basket_size]->a = arc;
                perm[basket_size]->cost = red_cost;
                perm[basket_size]->abs_cost = ABS(red_cost);
            }
        }

    }

    if( ++group_pos == nr_group )
        group_pos = 0;

    if( basket_size < B && group_pos != old_group_pos )
        goto NEXT;

    if( basket_size == 0 )
    {
        initialize = 1;
        *red_cost_of_bea = 0;
        return NULL;
    }

    sort_basket( 1, basket_size );

    *red_cost_of_bea = perm[1]->cost;
    return( perm[1]->a );
}


#ifdef _PROTO_
long refresh_potential( network_t *net )
#else
long refresh_potential( net )
    network_t *net;
#endif
{
    node_t *stop = net->stop_nodes;
    node_t *node, *tmp;
    node_t *root = net->nodes;
    long checksum = 0;

    for( node = root, stop = net->stop_nodes; node < (node_t*)stop; node++ )
        node->mark = 0;

    root->potential = (cost_t) -MAX_ART_COST;
    tmp = node = root->child;
    while( node != root )
    {
        while( node )
        {
            if( node->orientation == UP )
                node->potential = node->basic_arc->cost + node->pred->potential;
            else /* == DOWN */
            {
                node->potential = node->pred->potential - node->basic_arc->cost;
                checksum++;
            }

            tmp = node;
            node = node->child;
        }

        node = tmp;

        while( node->pred )
        {
            tmp = node->sibling;
            if( tmp )
            {
                node = tmp;
                break;
            }
            else
                node = node->pred;
        }
    }

    return checksum;
}

#ifdef _PROTO_
void primal_update_flow(
                 node_t *iplus,
                 node_t *jplus,
                 node_t *w
                 )
#else
void primal_update_flow( iplus, jplus, w )
    node_t *iplus, *jplus;
    node_t *w;
#endif
{
    for( ; iplus != w; iplus = iplus->pred )
    {
        if( iplus->orientation )
            iplus->flow = (flow_t)0;
        else
            iplus->flow = (flow_t)1;
    }

    for( ; jplus != w; jplus = jplus->pred )
    {
        if( jplus->orientation )
            jplus->flow = (flow_t)1;
        else
            jplus->flow = (flow_t)0;
    }
}


#ifdef _PROTO_
node_t *primal_iminus(
                      flow_t *delta,
                      long *xchange,
                      node_t *iplus,
                      node_t*jplus,
                      node_t **w
                    )
#else
node_t *primal_iminus( delta, xchange, iplus, jplus, w )
    flow_t *delta;
    long *xchange;
    node_t *iplus, *jplus;
    node_t **w;
#endif

{
    node_t *iminus = NULL;


    while( iplus != jplus )
    {
        if( iplus->depth < jplus->depth )
        {
            if( iplus->orientation )
                TEST_MIN( iplus, 0, iplus->flow, > )
            else if( iplus->pred->pred )
                TEST_MIN( iplus, 0, (flow_t)1 - iplus->flow, > )
            iplus = iplus->pred;
        }
        else
        {
            if( !jplus->orientation )
                TEST_MIN( jplus, 1, jplus->flow, >= )
            else if( jplus->pred->pred )
                TEST_MIN( jplus, 1, (flow_t)1 - jplus->flow, >= )
            jplus = jplus->pred;
        }
    }

    *w = iplus;

    return iminus;
}

#ifdef _PROTO_
long primal_net_simplex( network_t *net )
#else
long primal_net_simplex(  net )
    network_t *net;
#endif
{
    flow_t        delta;
    flow_t        new_flow;
    long          opt = 0;
    long          xchange;
    long          new_orientation;
    node_t        *iplus;
    node_t        *jplus;
    node_t        *iminus;
    node_t        *jminus;
    node_t        *w;
    arc_t         *bea;
    arc_t         *bla;
    arc_t         *arcs          = net->arcs;
    arc_t         *stop_arcs     = net->stop_arcs;
    node_t        *temp;
    long          m = net->m;
    long          new_set;
    cost_t        red_cost_of_bea;
    long          *iterations = &(net->iterations);
    long          *bound_exchanges = &(net->bound_exchanges);
    long          *checksum = &(net->checksum);


    while( !opt )
    {
        if( (bea = primal_bea_mpp( m, arcs, stop_arcs, &red_cost_of_bea )) )
        {
            (*iterations)++;

#ifdef DEBUG
            printf( "it %ld: bea = (%ld,%ld), red_cost = %ld\n",
                    *iterations, bea->tail->number, bea->head->number,
                    red_cost_of_bea );
#endif

            if( red_cost_of_bea > ZERO )
            {
                iplus = bea->head;
                jplus = bea->tail;
            }
            else
            {
                iplus = bea->tail;
                jplus = bea->head;
            }

            delta = (flow_t)1;
            iminus = primal_iminus( &delta, &xchange, iplus,
                    jplus, &w );

            if( !iminus )
            {
                (*bound_exchanges)++;

                if( bea->ident == AT_UPPER)
                    bea->ident = AT_LOWER;
                else
                    bea->ident = AT_UPPER;

                if( delta )
                    primal_update_flow( iplus, jplus, w );
            }
            else
            {
                if( xchange )
                {
                    temp = jplus;
                    jplus = iplus;
                    iplus = temp;
                }

                jminus = iminus->pred;

                bla = iminus->basic_arc;

                if( xchange != iminus->orientation )
                    new_set = AT_LOWER;
                else
                    new_set = AT_UPPER;

                if( red_cost_of_bea > 0 )
                    new_flow = (flow_t)1 - delta;
                else
                    new_flow = delta;

                if( bea->tail == iplus )
                    new_orientation = UP;
                else
                    new_orientation = DOWN;

//                update_tree( !xchange, new_orientation,
//                            delta, new_flow, iplus, jplus, iminus,
//                            jminus, w, bea, red_cost_of_bea,
//                            (flow_t)net->feas_tol );

                bea->ident = BASIC;
                bla->ident = new_set;

                if( !((*iterations-1) % 20) )
                {
                    *checksum += refresh_potential( net );
                    if( *checksum > 2000000000l )
                    {
                        printf("%ld\n",*checksum);
                        fflush(stdout);
                    }
                }
            }
        }

    }
}

#ifdef _PROTO_
long getfree( 
		            network_t *net
					            )
#else
long getfree( net )
	     network_t *net;
#endif
{  
	    FREE( net->nodes );
		    FREE( net->arcs );
			    FREE( net->dummy_arcs );
				    net->nodes = net->stop_nodes = NULL;
					    net->arcs = net->stop_arcs = NULL;
						    net->dummy_arcs = net->stop_dummy = NULL;

							    return 0;
}

int main(){}
