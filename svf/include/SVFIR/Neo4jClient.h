//===- Neo4jClient.h -----------------------------------------------//
//
//                     SVF: Static Value-Flow Analysis
//
// Copyright (C) <2013->  <Yulei Sui>
//

// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU Affero General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.

// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU Affero General Public License for more details.

// You should have received a copy of the GNU Affero General Public License
// along with this program.  If not, see <http://www.gnu.org/licenses/>.
//
//===----------------------------------------------------------------------===//


#include <Python.h>
#ifndef INCLUDE_NEO4JCLIENT_H_
#define INCLUDE_NEO4JCLIENT_H_

namespace SVF{

class DbNode {
    
private:
    const char* nodetype;
    PyObject* properties;

public:
    // Constructor
    DbNode(const char* nodetype, PyObject* properties) {
        // Set the nodetype
        this->nodetype = nodetype;
        // Set the properties
        this->properties = properties;
    }

    // Destructor
    ~DbNode() {
        Py_DECREF(properties);
    }

    // Get the nodetype
    const char* getNodetype() const {
        return nodetype;
    }

    // Get the properties
    PyObject* getProperties() const {
        return properties;
    }
};

class DbEdge {

private:
    const char* edge_type;
    PyObject* edge_properties;

public:
    // Constructor
    DbEdge(const char* edge_type, PyObject* edge_properties) {
        // Set the edge_type
        this->edge_type = edge_type;
        // Set the edge_properties
        this->edge_properties = edge_properties;
    }

    // Destructor
    ~DbEdge() {
        Py_DECREF(edge_properties);
    }

    // Get the edge_type
    const char* getEdgeType() const {
        return edge_type;
    }

    // Get the edge_properties
    PyObject* getProperties() const {
        return edge_properties;
    }
};

class Neo4jClient {

private:
    PyObject* pInstance;

public:
    // Constructor
    Neo4jClient(const char* uri, const char* username, const char* password);
    // Destructor
    ~Neo4jClient();

    DbNode createNode(const char* graph_id, const char* nodetype, ...);

    DbEdge createEdge(const char* graph_id, const char* edge_type, ...);
    void writeNode(const DbNode& node);

    void writeEdge(const DbNode& node1, const DbNode& node2, const DbEdge& edge);

    void clearDatabase();
};


} // namespace SVF

#endif // INCLUDE_NEO4JCLIENT_H_