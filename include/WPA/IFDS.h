/*
 * SICFG.h
 *
 */

#ifndef INCLUDE_UTIL_SICFG_H_
#define INCLUDE_UTIL_SICFG_H_

#include "Util/ICFG.h"

class IFDS {

private:
	ICFG* icfg;

public:
	inline VFG* getVFG() const{
		return icfg->getVFG();
	}

	inline ICFG* getICFG() const{
		return icfg;
	}

	IFDS(ICFG* i): icfg(i){
	}

};



#endif /* INCLUDE_UTIL_SICFG_H_ */
