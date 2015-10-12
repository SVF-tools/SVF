//===- GraphUtil.h -- Graph template for debugging---------------------------//
//
//                     SVF: Static Value-Flow Analysis
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

/*
 * GraphUtil.h
 *
 *  Created on: Jul 12, 2013
 *      Author: yusui
 */

#ifndef GRAPHUTIL_H_
#define GRAPHUTIL_H_

#include <llvm/Support/Debug.h> 		// for debug
#include <llvm/ADT/GraphTraits.h>		// for Graphtraits
#include <llvm/Support/ToolOutputFile.h>
#include <llvm/Support/CommandLine.h> // for tool output file
#include <llvm/Support/GraphWriter.h>		// for graph write
#include <llvm/Support/FileSystem.h>		// for file open flag

namespace llvm {

/*
 * Dump and print the graph for debugging
 */
class GraphPrinter {

public:
    GraphPrinter() {
    }

    /*!
     *  Write the graph into dot file for debugging purpose
     */
    template<class GraphType>
    static void WriteGraphToFile(llvm::raw_ostream &O,
                                 const std::string &GraphName, const GraphType &GT, bool simple = false) {
        // Filename of the output dot file
        std::string Filename = GraphName + ".dot";
        O << "Writing '" << Filename << "'...";
        std::error_code ErrInfo;
        tool_output_file F(Filename.c_str(), ErrInfo, sys::fs::F_None);

        if (!ErrInfo) {
            // dump the ValueFlowGraph here
            WriteGraph(F.os(), GT, simple);
            F.os().close();
            if (!F.os().has_error()) {
                O << "\n";
                F.keep();
                return;
            }
        }
        O << "  error opening file for writing!\n";
        F.os().clear_error();
    }

    /*!
     * Print the graph to command line
     */
    template<class GraphType>
    static void PrintGraph(llvm::raw_ostream &O, const std::string &GraphName,
                           const GraphType &GT) {
        ///Define the GTraits and node iterator for printing
        typedef GraphTraits<GraphType> GTraits;
        typedef typename GTraits::NodeType NodeType;
        typedef typename GTraits::nodes_iterator node_iterator;
        typedef typename GTraits::ChildIteratorType child_iterator;

        O << "Printing VFG Graph" << "'...\n";
        // Print each node name and its edges
        node_iterator I = GTraits::nodes_begin(GT);
        node_iterator E = GTraits::nodes_end(GT);
        for (; I != E; ++I) {
            NodeType *Node = *I;
            O << "node :" << Node << "'\n";
            child_iterator EI = GTraits::child_begin(Node);
            child_iterator EE = GTraits::child_end(Node);
            for (unsigned i = 0; EI != EE && i != 64; ++EI, ++i) {
                O << "child :" << *EI << "'\n";
            }
        }
    }
};

}

#endif /* GRAPHUTIL_H_ */
