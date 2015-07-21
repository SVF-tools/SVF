/*
 * Partial leak
 *
 *  Created on: Jan 3, 2012
 *      Author: Yulei Sui
 */

#include "aliascheck.h"

typedef enum {SOURCE, SINK, IPIN, OPIN, CHANX, CHANY} t_rr_type;
int nx, ny, io_rat, pins_per_clb;
int num_rr_nodes;

struct s_rr_node {short xlow; short xhigh; short ylow; short yhigh;
     short ptc_num; short num_edges;  t_rr_type type; int *edges;
     short *switches; float R; float C; };

struct s_rr_node *rr_node;                      /* [0..num_rr_nodes-1] */

#define max(a,b) (((a) > (b))? (a) : (b))


void count_routing_transistors (int num_switch, float R_minW_nmos,
            float R_minW_pmos) {



 int *num_inputs_to_cblock;  /* [0..num_rr_nodes-1], but all entries not    */
                             /* corresponding to IPINs will be 0.           */

 int *cblock_counted;          /* [0..max(nx,ny)] -- 0th element unused. */
 float *shared_buffer_trans;       /* [0..max_nx,ny)] */
 float *unsharable_switch_trans, *sharable_switch_trans; /* [0..num_switch-1] */

 t_rr_type from_rr_type, to_rr_type;
 int from_node, to_node, iedge, num_edges, maxlen;
 int iswitch, i, j, iseg, max_inputs_to_cblock;
 float ntrans_sharing, ntrans_no_sharing, shared_opin_buffer_trans;
 float input_cblock_trans;

 const float trans_sram_bit = 6.;



 float trans_track_to_cblock_buf;
 float trans_cblock_to_lblock_buf;

 ntrans_sharing = 0.;
 ntrans_no_sharing = 0.;
 max_inputs_to_cblock = 0;



 num_inputs_to_cblock = PLKMALLOC(1);

 maxlen = max (nx, ny) + 1;
 cblock_counted = (int *) PLKMALLOC(1);
 shared_buffer_trans = (float *) PLKMALLOC(1);

 for (from_node=0;from_node<num_rr_nodes;from_node++) {


    switch (from_rr_type) {

    case CHANX: case CHANY:

       for (iedge=0;iedge<num_edges;iedge++) {


          switch (to_rr_type) {

          case CHANX: case CHANY:
             iswitch = rr_node[from_node].switches[iedge];

             if (num_edges) {

             }
             else if (from_node < to_node) {


             }
             break;

          case IPIN:

             break;

          default:

             return;
             break;

          }   /* End switch on to_rr_type. */

       }   /* End for each edge. */

      /* Now add in the shared buffer transistors, and reset some flags. */

       if (from_rr_type == CHANX) {
          for (i=rr_node[from_node].xlow-1;i<=rr_node[from_node].xhigh;i++) {

          }

          for (i=rr_node[from_node].xlow;i<=rr_node[from_node].xhigh;i++)
             cblock_counted[i] = 0;

       }
       else {  /* CHANY */
          for (j=rr_node[from_node].ylow-1;j<=rr_node[from_node].yhigh;j++) {

          }

          for (j=rr_node[from_node].ylow;j<=rr_node[from_node].yhigh;j++)
             cblock_counted[j] = 0;

       }
       break;





    }  /* End switch on from_rr_type */
 }  /* End for all nodes */

 free (cblock_counted);
 free (shared_buffer_trans);
 free (unsharable_switch_trans);
 free (sharable_switch_trans);


 free (num_inputs_to_cblock);

}

int main(){

	count_routing_transistors(1,1,1);
}
