#ifndef __ARGVFLOW_H
#define __ARGVFLOW_H

#include "SABER/LeakChecker.h"

class ArgvFlow : public LeakChecker {

public:

    /// Pass ID
    static char ID;

    /// Constructor
    ArgvFlow(char id = ID): LeakChecker(ID) {
    }

    /// Destructor
    virtual ~ArgvFlow() {
    }

	std::vector<std::string> sourceFunctions = {
		"GETARGV"
	};
	std::vector<std::string> sinkFunctions = {
		"open",
		"fopen"
	};

    /// Initialize analysis
	void initialize(llvm::Module& module) {
        ptaCallGraph = new PTACallGraph(&module);
        AndersenWaveDiff* ander =  \
		  AndersenWaveDiff::createAndersenWaveDiff(module);

		memSSA.addPruneSource("GETARGV", 0);
		memSSA.addPruneSink("open", 0);
		memSSA.addPruneSink("fopen", 0);

        svfg =  memSSA.buildSVFG(ander);
        setGraph(memSSA.getSVFG());
        //AndersenWaveDiff::releaseAndersenWaveDiff();
        /// allocate control-flow graph branch conditions
        getPathAllocator()->allocate(module);

        initSrcs();
        initSnks();
    }

    /// We start from here
    virtual bool runOnModule(llvm::Module& module) {
        /// start analysis
        analyze(module);
        return false;
    }

    /// Get pass name
    virtual llvm::StringRef getPassName() const {
        return "argv[i] flow";
    }

    /// Pass dependence
    virtual void getAnalysisUsage(llvm::AnalysisUsage& au) const {
        /// do not intend to change the IR in this pass,
        au.setPreservesAll();
    }

	
    inline bool isSourceLikeFun(const llvm::Function* fun) {
        if (!fun->hasName()) {
            return false;
        }
        std::string s = fun->getName().str();
        for (auto it = sourceFunctions.begin(); it != sourceFunctions.end();
          ++it) {
            std::string ts = *it;
            if (s == ts) {
                return true;
            }
        }
        return false;
    }
    inline bool isSinkLikeFun(const llvm::Function* fun) {
		if (!fun->hasName()) {
			return false;
		}
		std::string s = fun->getName().str();
		for (auto it = sinkFunctions.begin(); it != sinkFunctions.end();
		  ++it) {
			std::string ts = *it;
			if (s == ts) {
				return true;
			}
		}
		return false;
    }

    /// Report file/close bugs
    void reportBug(ProgSlice* slice);
};


#endif
