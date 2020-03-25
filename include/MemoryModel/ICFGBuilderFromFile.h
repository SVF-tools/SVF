#ifndef INCLUDE_MEMORYMODEL_ICFGBUILDERFROMFILE_H_
#define INCLUDE_MEMORYMODEL_ICFGBUILDERFROMFILE_H_

#include "Util/ICFG.h"
#include "MemoryModel/PAG.h"
#include "llvm/Support/JSON.h"


/*!
 * Build ICFG from a user specified file 
 */
class ICFGBuilderFromFile{

private:
    ICFG* icfg;
    std::string file;
    PAG pag;

public: 
    //Constructor
    ICFGBuilderFromFile(std::string f, PAG pag){
        this->pag = pag;
    }

    // Destructor
    ~ICFGBuilderFromFile() {}

    // Return ICFG
    ICFG* getICFG() const{
        return icfg;
    }

    std::string getFileName() const {
        return file;
    }

    PAG getPAG() const{
        return pag;
    }
    //Start building
    ICFG* build();

    void addNode(NodeID nodeID, std::string nodeType, llvm::json::Array* pagEdges);

    void addEdge(llvm::json::Object* edge_obj);
};

#endif /* INCLUDE_MEMORYMODEL_ICFGBUILDERFROMFILE_H_ */