#include "defines.h"
long getfree( 
		                    network_t *net
							                                )   
{  
	        FREE( net->nodes );
			            FREE( net->arcs );
						                FREE( net->dummy_arcs );
										                    net->nodes = net->stop_nodes = NULL;
															                        net->arcs = net->stop_arcs = NULL;
																					                            net->dummy_arcs = net->stop_dummy = NULL;

																												                                return 0;
}
                                                                                                                                                      
int main(){
	getfree(NULL);
}

