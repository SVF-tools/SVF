#ifndef INCLUDE_MEMORYMODEL_ICFGBUILDERFROMFILE_H_
#define INCLUDE_MEMORYMODEL_ICFGBUILDERFROMFILE_H_

#include "Util/ICFG.h"
#include "llvm/Support/JSON.h"


/*!
 * Build ICFG from a user specified file 
 */
class ICFGBuilderFromFile{

private:
    ICFG* icfg;
    std::string file;

public: 
    //Constructor
    ICFGBuilderFromFile(std::string f){}

    // Destructor
    ~ICFGBuilderFromFile() {}

    // Return ICFG
    ICFG* getICFG() const{
        return icfg;
    }

    std::string getFileName() const {
        return file;
    }

    //Start building
    ICFG* build();

    void addNode(NodeID nodeID, std::string nodeType);

    void addEdge(llvm::json::Object* edge_obj);
};

#endif /* INCLUDE_MEMORYMODEL_ICFGBUILDERFROMFILE_H_ */