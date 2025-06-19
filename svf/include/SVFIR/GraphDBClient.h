#ifndef INCLUDE_GRAPHDBCLIENT_H_
#define INCLUDE_GRAPHDBCLIENT_H_
#include "Graphs/CallGraph.h"
#include "Graphs/ICFGEdge.h"
#include "Graphs/ICFGNode.h"
#include "Graphs/CHG.h"
#include "SVFIR/SVFIR.h"
#include "SVFIR/SVFStatements.h"
#include "SVFIR/SVFType.h"
#include "Util/SVFUtil.h"
#include "Util/cJSON.h"
#include "lgraph/lgraph_rpc_client.h"
#include <errno.h>
#include <stdio.h>

namespace SVF
{
class RpcClient;
class ICFGEdge;
class ICFGNode;
class CallGraphEdge;
class CallGraphNode;
class SVFType;
class StInfo;
class SVFIR;
class SVFVar;
class SVFStmt;
class SVFBasicBlock;
class BasicBlockEdge;
class BasicBlockGraph;
class CHGraph;
class CHEdge;
class CHNode;
class GraphDBClient
{
private:
    lgraph::RpcClient* connection;

    GraphDBClient()
    {
        const char* url = "127.0.0.1:9090";
        connection = new lgraph::RpcClient(url, "admin", "qazwsx123");
    }

    ~GraphDBClient()
    {
        if (connection != nullptr)
        {
            connection = nullptr;
        }
    }

public:
    static GraphDBClient& getInstance()
    {
        static GraphDBClient instance;
        return instance;
    }

    GraphDBClient(const GraphDBClient&) = delete;
    GraphDBClient& operator=(const GraphDBClient&) = delete;

    lgraph::RpcClient* getConnection()
    {
        return connection;
    }

    bool loadSchema(lgraph::RpcClient* connection, const std::string& filepath,
                    const std::string& dbname);
    bool createSubGraph(lgraph::RpcClient* connection,
                        const std::string& graphname);
    bool addCallGraphNode2db(lgraph::RpcClient* connection,
                             const CallGraphNode* node,
                             const std::string& dbname);
    bool addCallGraphEdge2db(lgraph::RpcClient* connection,
                             const CallGraphEdge* edge,
                             const std::string& dbname);
    bool addICFGNode2db(lgraph::RpcClient* connection, const ICFGNode* node,
                        const std::string& dbname);
    bool addICFGEdge2db(lgraph::RpcClient* connection, const ICFGEdge* edge,
                        const std::string& dbname);

    /// pasre the directcallsIds/indirectcallsIds string to vector
    std::vector<int> stringToIds(const std::string& str);

    void insertICFG2db(const ICFG* icfg);
    void insertCallGraph2db(const CallGraph* callGraph);
    void insertPAG2db(const SVFIR* pag);
    void insertBasicBlockGraph2db(const BasicBlockGraph* bbGraph);
    void insertSVFTypeNodeSet2db(const Set<const SVFType*>* types,
                                 const Set<const StInfo*>* stInfos,
                                 std::string& dbname);

    /// @brief parse the CHG and generate the insert statements for CHG nodes and edges
    /// @param chg  
    void insertCHG2db(const CHGraph* chg);
    void insertCHNode2db(lgraph::RpcClient* connection, const CHNode* node, const std::string& dbname);
    void insertCHEdge2db(lgraph::RpcClient* connection, const CHEdge* edge, const std::string& dbname);
    std::string getCHNodeInsertStmt(const CHNode* node);
    std::string getCHEdgeInsertStmt(const CHEdge* edge);

    std::string getPAGNodeInsertStmt(const SVFVar* node);
    void insertPAGNode2db(lgraph::RpcClient* connection, const SVFVar* node,
                          const std::string& dbname);
    void insertPAGEdge2db(lgraph::RpcClient* connection, const SVFStmt* node,
                          const std::string& dbname);
    void insertBBNode2db(lgraph::RpcClient* connection,
                         const SVFBasicBlock* node, const std::string& dbname);
    void insertBBEdge2db(lgraph::RpcClient* connection,
                         const BasicBlockEdge* node, const std::string& dbname);
    std::string getPAGEdgeInsertStmt(const SVFStmt* edge);
    std::string getPAGNodeKindString(const SVFVar* node);

    /// parse ICFGNodes & generate the insert statement for ICFGNodes
    std::string getICFGNodeKindString(const ICFGNode* node);

    cJSON* queryFromDB(lgraph::RpcClient* connection, const std::string& dbname, std::string queryStatement);
    /// read SVFType from DB
    void readSVFTypesFromDB(lgraph::RpcClient* connection,
                            const std::string& dbname, SVFIR* pag);
    void addSVFTypeNodeFromDB(lgraph::RpcClient* connection,
                               const std::string& dbname, SVFIR* pag);

    /// read BasicBlockGraph from DB
    void readBasicBlockGraphFromDB(lgraph::RpcClient* connection, const std::string& dbname);
    void readBasicBlockNodesFromDB(lgraph::RpcClient* connection, const std::string& dbname, FunObjVar* funObjVar);
    void readBasicBlockEdgesFromDB(lgraph::RpcClient* connection, const std::string& dbname, FunObjVar* funObjVar);

    /// read ICFGNodes & ICFGEdge from DB
    ICFG* buildICFGFromDB(lgraph::RpcClient* connection, const std::string& dbname, SVFIR* pag);
    /// ICFGNodes
    void readICFGNodesFromDB(lgraph::RpcClient* connection, const std::string& dbname, std::string nodeType, ICFG* icfg, SVFIR* pag);
    ICFGNode* parseGlobalICFGNodeFromDBResult(const cJSON* node);
    ICFGNode* parseFunEntryICFGNodeFromDBResult(const cJSON* node, SVFIR* pag);
    ICFGNode* parseFunExitICFGNodeFromDBResult(const cJSON* node, SVFIR* pag);
    ICFGNode* parseRetICFGNodeFromDBResult(const cJSON* node, SVFIR* pag);
    ICFGNode* parseIntraICFGNodeFromDBResult(const cJSON* node, SVFIR* pag);
    ICFGNode* parseCallICFGNodeFromDBResult(const cJSON* node, SVFIR* pag);
    void parseSVFStmtsForICFGNodeFromDBResult(SVFIR* pag);
    void updateRetPE4RetCFGEdge();
    void updateCallPEs4CallCFGEdge();
    
    /// ICFGEdges
    void readICFGEdgesFromDB(lgraph::RpcClient* connection, const std::string& dbname, std::string edgeType, ICFG* icfg, SVFIR* pag);
    ICFGEdge* parseIntraCFGEdgeFromDBResult(const cJSON* edge, SVFIR* pag, ICFG* icfg);
    ICFGEdge* parseCallCFGEdgeFromDBResult(const cJSON* edge, SVFIR* pag, ICFG* icfg);
    ICFGEdge* parseRetCFGEdgeFromDBResult(const cJSON* edge, SVFIR* pag, ICFG* icfg);

    // read CallGraph Nodes & CallGraphEdge from DB
    CallGraph* buildCallGraphFromDB(lgraph::RpcClient* connection, const std::string& dbname, SVFIR* pag);
    CallGraphNode* parseCallGraphNodeFromDB(const cJSON* node);
    CallGraphEdge* parseCallGraphEdgeFromDB(const cJSON* edge, SVFIR* pag, CallGraph* callGraph);
    void readCallGraphNodesFromDB(lgraph::RpcClient* connection, const std::string& dbname, CallGraph* callGraph);
    void readCallGraphEdgesFromDB(lgraph::RpcClient* connection, const std::string& dbname, SVFIR* pag, CallGraph* callGraph);

    /// read PAGNodes from DB
    void readPAGNodesFromDB(lgraph::RpcClient* connection, const std::string& dbname, std::string nodeType, SVFIR* pag);
    void initialSVFPAGNodesFromDB(lgraph::RpcClient* connection, const std::string& dbname, SVFIR* pag);
    void updateSVFPAGNodesAttributesFromDB(lgraph::RpcClient* connection, const std::string& dbname, std::string nodeType, SVFIR* pag);
    void updatePAGNodesFromDB(lgraph::RpcClient* connection, const std::string& dbname, SVFIR* pag);
    void updateSVFValVarAtrributes(cJSON* properties, ValVar* var, SVFIR* pag);
    void updateGepValVarAttributes(cJSON* properties, GepValVar* var, SVFIR* pag);
    void updateSVFBaseObjVarAtrributes(cJSON* properties, BaseObjVar* var, SVFIR* pag);
    void updateFunObjVarAttributes(cJSON* properties, FunObjVar* var, SVFIR* pag);
    void loadSVFPAGEdgesFromDB(lgraph::RpcClient* connection, const std::string& dbname, SVFIR* pag);
    void readPAGEdgesFromDB(lgraph::RpcClient* connection, const std::string& dbname, std::string edgeType, SVFIR* pag);
    void parseAPIdxOperandPairsString(const std::string& ap_idx_operand_pairs, SVFIR* pag, AccessPath* ap);
    void parseOpVarString(std::string& op_var_node_ids, SVFIR* pag, std::vector<SVFVar*>& opVarNodes);

    ObjTypeInfo* parseObjTypeInfoFromDB(cJSON* properties, SVFIR* pag);

    template <typename Container>
    std::string extractNodesIds(const Container& nodes)
    {
        if (nodes.empty())
        {
            return "";
        }
        std::ostringstream nodesIds;
        auto it = nodes.begin();

        nodesIds << (*it)->getId();
        ++it;

        for (; it != nodes.end(); ++it)
        {
            nodesIds << "," << (*it)->getId();
        }

        return nodesIds.str();
    }

    std::string extractFuncVectors2String(std::vector<std::vector<const FunObjVar*>> vec) {
        std::ostringstream oss;
        for (size_t i = 0; i < vec.size(); ++i) {
            oss << "{" << extractNodesIds(vec[i]) << "}";
            if (i + 1 != vec.size()) {
                oss << ",";
            }
        }
        return oss.str();
    }

    template <typename Container>
    std::string extractEdgesIds(const Container& edges)
    {
        if (edges.empty())
        {
            return "";
        }
        std::ostringstream edgesIds;
        auto it = edges.begin();

        edgesIds << (*it)->getEdgeID();
        ++it;

        for (; it != edges.end(); ++it)
        {
            edgesIds << "," << (*it)->getEdgeID();
        }

        return edgesIds.str();
    }

    template <typename Container>
    std::string extractIdxs(const Container& idxVec)
    {
        if (idxVec.empty())
        {
            return "";
        }
        std::ostringstream idxVecStr;
        auto it = idxVec.begin();
    
        idxVecStr << *it;
        ++it;
    
        for (; it != idxVec.end(); ++it)
        {
            idxVecStr << "," << *it;
        }
    
        return idxVecStr.str();
    }

    template <typename Container>
    Container parseElements2Container(std::string& str)
    {
        str.erase(std::remove(str.begin(), str.end(), '\n'), str.end());
        str.erase(std::remove(str.begin(), str.end(), '\r'), str.end());
        Container idxVec;
        std::istringstream ss(str);
        std::string token;

        while (std::getline(ss, token, ','))
        {
            if constexpr (std::is_same<typename Container::value_type,
                                       int>::value)
            {
                if constexpr (std::is_same<Container, std::vector<int>>::value ||
                          std::is_same<Container, std::list<int>>::value)
                    idxVec.push_back(std::stoi(token));
                else
                    idxVec.insert(std::stoi(token));
            }
            else if constexpr (std::is_same<typename Container::value_type,
                                            uint32_t>::value)
            {
                if constexpr (std::is_same<Container, std::vector<uint32_t>>::value || 
                    std::is_same<Container, std::list<uint32_t>>::value)
                    idxVec.push_back(static_cast<uint32_t>(std::stoul(token)));
                else
                    idxVec.insert(static_cast<uint32_t>(std::stoul(token)));
            }
            else
            {
                if constexpr (std::is_same<Container, std::vector<float>>::value ||
                          std::is_same<Container, std::list<float>>::value)
                    idxVec.push_back(std::stof(token));
                 else
                    idxVec.insert(std::stof(token));
            }
        }

        return idxVec;
    }

    template <typename Container>
    std::string extractSVFTypes(const Container& types)
    {
        if (types.empty())
        {
            return "";
        }
    
        std::ostringstream typesStr;
        auto it = types.begin();
    
        
        typesStr << "{" << (*it)->toString() << "}";
        ++it;
    
        for (; it != types.end(); ++it)
        {
            typesStr << "," << "{" << (*it)->toString() << "}";
        }
    
        return typesStr.str();
    }

    template <typename Container>
    Container parseElementsToSVFTypeContainer(
        std::string& str, const Map<std::string, SVFType*>& typeMap)
    {
        str.erase(std::remove(str.begin(), str.end(), '\n'), str.end());
        str.erase(std::remove(str.begin(), str.end(), '\r'), str.end());
    
        Container resultContainer;
    
        size_t pos = 0;
        while ((pos = str.find('{')) != std::string::npos)
        {
            size_t endPos = str.find('}', pos);
            if (endPos == std::string::npos)
            {
                SVFUtil::outs() << "Warning: Mismatched brackets in input string\n";
                break;
            }
    
            std::string token = str.substr(pos + 1, endPos - pos - 1);
            
            token.erase(token.find_last_not_of(" \t") + 1);
            token.erase(0, token.find_first_not_of(" \t"));
    
            auto it = typeMap.find(token);
            if (it != typeMap.end())
            {
                resultContainer.insert(resultContainer.end(), it->second);
            }
            else
            {
                SVFUtil::outs()
                    << "Warning: No matching SVFType found for token '" << token
                    << "'\n";
            }
    
            str = str.substr(endPos + 1);
        }
    
        return resultContainer;
    }

    std::vector<std::string> parseSVFTypes(std::string typesStr)
    {
        typesStr.erase(std::remove(typesStr.begin(), typesStr.end(), '\n'),
                       typesStr.end());
        typesStr.erase(std::remove(typesStr.begin(), typesStr.end(), '\r'),
                       typesStr.end());
        std::vector<std::string> result;
        std::stringstream ss(typesStr);
        std::string token;

        while (std::getline(ss, token, ','))
        {
            if (!token.empty() && token.front() == '{')
            {
                token.erase(0, 1); 
            }
            if (!token.empty() && token.back() == '}')
            {
                token.erase(token.size() - 1, 1); 
            }
            result.push_back(token);
        }

        return result;
    }

    template <typename MapType>
    std::string extractFldIdx2TypeMap(const MapType& fldIdx2TypeMap)
    {
        if (fldIdx2TypeMap.empty())
        {
            return "";
        }
        
        std::ostringstream mapStr;
        auto it = fldIdx2TypeMap.begin();
    
        mapStr << "{" << it->first << ":" << it->second->toString() << "}";
        ++it;
    
        for (; it != fldIdx2TypeMap.end(); ++it)
        {
            mapStr << ",{" << it->first << ":" << it->second->toString() << "}";
        }
    
        return mapStr.str();
    }

    template <typename MapType>
    MapType parseStringToFldIdx2TypeMap(const std::string& str, const Map<std::string, SVFType*>& typeMap)
    {
        MapType resultMap;
    
        std::string cleanedStr = str;
        cleanedStr.erase(0, cleanedStr.find_first_not_of(" \t"));
        cleanedStr.erase(cleanedStr.find_last_not_of(" \t") + 1);
    
        size_t startPos = 0;
        size_t openBracePos = cleanedStr.find('{', startPos);
    
        while (openBracePos != std::string::npos) {
            size_t closeBracePos = cleanedStr.find('}', openBracePos);
            if (closeBracePos == std::string::npos) {
                break;  
            }
    
            std::string token = cleanedStr.substr(openBracePos + 1, closeBracePos - openBracePos - 1);
    
            size_t colonPos = token.find(':');
            if (colonPos != std::string::npos) {
                std::string keyStr = token.substr(0, colonPos);
                std::string typeStr = token.substr(colonPos + 1);

                u32_t key = static_cast<u32_t>(std::stoi(keyStr));
    
                auto it = typeMap.find(typeStr);
                if (it != typeMap.end()) {
                    resultMap[key] = it->second;
                } else {
                    SVFUtil::outs() << "Warning: No matching SVFType found for type: " << typeStr << "\n";
                }
            }
    
            startPos = closeBracePos + 1;
            openBracePos = cleanedStr.find('{', startPos);
        }
    
        return resultMap;
    }

    template <typename LabelMapType>
    std::string extractLabelMap2String(const LabelMapType& labelMap)
    {
        if (labelMap->empty())
        {
            return "";
        }
        std::ostringstream mapStr;

        for (auto it = labelMap->begin(); it != labelMap->end(); ++it)
        {
            if (it != labelMap->begin())
            {
                mapStr << ",";
            }
            mapStr << (it->first ? std::to_string(it->first->getId()) : "NULL")
                   << ":" << std::to_string(it->second);
        }

        return mapStr.str();
    }

    template <typename MapType>
    MapType parseLabelMapFromString(const std::string& str)
    {
        MapType result;
        if (str.empty()) return result;
    
        std::stringstream ss(str);
        std::string pairStr;
    
        while (std::getline(ss, pairStr, ','))
        {
            size_t colonPos = pairStr.find(':');
            if (colonPos == std::string::npos) continue;
    
            std::string keyStr = pairStr.substr(0, colonPos);
            std::string valStr = pairStr.substr(colonPos + 1);
    
            int key = (keyStr == "NULL") ? -1 : std::stoi(keyStr);
            int val = std::stoi(valStr);
    
            result[key] = val;
        }
    
        return result;
    }

    template <typename BBsMapWithSetType>
    std::string extractBBsMapWithSet2String(const BBsMapWithSetType& bbsMap)
    {
        if (bbsMap->empty())
        {
            return "";
        }
        std::ostringstream mapStr;
        auto it = bbsMap->begin();

        for (; it != bbsMap->end(); ++it)
        {
            mapStr << "[" << it->first->getId() << ":";
            mapStr << extractNodesIds(it->second);
            mapStr << "]";
        }

        return mapStr.str();
    }

    template <typename MapType>
    MapType parseBBsMapFromString(const std::string& str)
    {
        MapType result;
    
        using KeyType = typename MapType::key_type;
        using ValueContainer = typename MapType::mapped_type;
        using ValueType = typename ValueContainer::value_type;
    
        size_t pos = 0;
        while ((pos = str.find('[', pos)) != std::string::npos)
        {
            size_t end = str.find(']', pos);
            if (end == std::string::npos) break;
    
            std::string block = str.substr(pos + 1, end - pos - 1); // earse []
            pos = end + 1;
    
            size_t colonPos = block.find(':');
            if (colonPos == std::string::npos) continue;
    
            std::string keyStr = block.substr(0, colonPos);
            std::string valuesStr = block.substr(colonPos + 1);
    
            KeyType key = static_cast<KeyType>(std::stoi(keyStr));
            ValueContainer values;
    
            if (!valuesStr.empty())
            {
                std::stringstream ss(valuesStr);
                std::string token;
                while (std::getline(ss, token, ','))
                {
                    if (!token.empty())
                        values.insert(values.end(), static_cast<ValueType>(std::stoi(token)));
                }
            }
    
            result[key] = values;
        }
    
        return result;
    }

    template <typename BBsMapType>
    std::string extractBBsMap2String(const BBsMapType& bbsMap)
    {
        if (bbsMap->empty())
        {
            return "";
        }
        std::ostringstream mapStr;
        for (const auto& pair : *bbsMap)
        {
            if (mapStr.tellp() != std::streampos(0))
            {
                mapStr << ",";
            }
            if (pair.first != nullptr && pair.second != nullptr)
            {
                mapStr << pair.first->getId() << ":" << pair.second->getId();
            }
            else if (pair.first == nullptr)
            {
                mapStr << "NULL:" << pair.second->getId();
            }
            else if (pair.second == nullptr)
            {
                mapStr << pair.first->getId() << ":NULL";
            }
        }
        return mapStr.str();
    }
    

    template <typename MapType>
    MapType parseBB2PiMapFromString(const std::string& str)
    {
        MapType result;
        if (str.empty()) return result;
    
        std::stringstream ss(str);
        std::string pairStr;
    
        while (std::getline(ss, pairStr, ','))
        {
            size_t colonPos = pairStr.find(':');
            if (colonPos == std::string::npos) continue;
    
            std::string keyStr = pairStr.substr(0, colonPos);
            std::string valStr = pairStr.substr(colonPos + 1);
    
            int key = (keyStr == "NULL") ? -1 : std::stoi(keyStr);
            int val = (valStr == "NULL") ? -1 : std::stoi(valStr);
    
            result[key] = val;
        }
    
        return result;
    }

    template <typename MapType>
    std::string pagEdgeToSetMapTyToString(const MapType& map)
    {
        if (map.empty())
        {
            return "";
        }

        std::ostringstream oss;

        for (auto it = map.begin(); it != map.end(); ++it)
        {
            oss << "[" << it->first << ":";
            const SVFStmt::SVFStmtSetTy& set = it->second;
            for (auto setIt = set.begin(); setIt != set.end(); ++setIt)
            {
                if (setIt != set.begin())
                {
                    oss << ",";
                }
                oss << (*setIt)->getEdgeID();
            }
            oss << "]";

            if (std::next(it) != map.end())
            {
                oss << ",";
            }
        }

        return oss.str();
    }

    /// Convert IdxOperandPairs to string
    std::string IdxOperandPairsToString(
        const AccessPath::IdxOperandPairs* idxOperandPairs) const
    {
        if (idxOperandPairs->empty())
        {
            return "";
        }

        std::ostringstream oss;
        oss << "[";
        for (auto it = idxOperandPairs->begin(); it != idxOperandPairs->end();
             ++it)
        {
            if (it != idxOperandPairs->begin())
            {
                oss << ", ";
            }
            oss << IdxOperandPairToString(*it);
        }
        oss << "]";
        return oss.str();
    }

    std::string IdxOperandPairToString(
        const AccessPath::IdxOperandPair& pair) const
    {
        std::ostringstream oss;
        if (nullptr != pair.first && nullptr != pair.second)
        {
            oss << "{" << pair.first->getId() << ", " << pair.second->toString()
                << "}";
            return oss.str();
        }
        else if (nullptr == pair.second)
        {
            oss << "{" << pair.first->getId() << ", NULL}";
        }
        return "";
    }

    std::vector<std::pair<int, std::string>> parseIdxOperandPairsString(const std::string& str)
    {
        std::vector<std::pair<int, std::string>> result;

        if (str.empty() || str == "[]")
        {
            return result;
        }

        size_t pos = 0;
        std::string content = str.substr(1, str.size() - 2); // remove the outter []

        while ((pos = content.find('{')) != std::string::npos)
        {
            size_t end = content.find('}', pos);
            if (end == std::string::npos)
                break;

            std::string pairStr =
                content.substr(pos + 1, end - pos - 1); // extract the pair content
            content = content.substr(end + 1);          //processing the rest

            size_t comma = pairStr.find(',');
            if (comma == std::string::npos)
                continue;

            std::string idStr = pairStr.substr(0, comma);
            std::string operandStr = pairStr.substr(comma + 1);

            // erase the spaces
            idStr.erase(std::remove_if(idStr.begin(), idStr.end(), ::isspace),
                        idStr.end());
            operandStr.erase(
                std::remove_if(operandStr.begin(), operandStr.end(), ::isspace),
                operandStr.end());

            int id = std::stoi(idStr);
            result.emplace_back(id, operandStr);
        }

        return result;
    }

    std::string extractSuccessorsPairSet2String(
        const BranchStmt::SuccAndCondPairVec* vec)
    {
        std::ostringstream oss;
        for (auto it = vec->begin(); it != vec->end(); ++it)
        {
            if (it != vec->begin())
            {
                oss << ",";
            }
            oss << (*it).first->getId() << ":" << std::to_string((*it).second);
        }

        return oss.str();
    }

    std::vector<std::pair<int, s32_t>> parseSuccessorsPairSetFromString(const std::string& str)
    {
        std::vector<std::pair<int, s32_t>> result;
        if (str.empty())
            return result;
    
        std::stringstream ss(str);
        std::string pairStr;
    
        while (std::getline(ss, pairStr, ',')) {
            size_t colonPos = pairStr.find(':');
            if (colonPos != std::string::npos) {
                int first = std::stoi(pairStr.substr(0, colonPos));
                s32_t second = static_cast<s32_t>(std::stoi(pairStr.substr(colonPos + 1)));
                result.emplace_back(first, second);
            }
        }
    
        return result;
    }

    int parseBBId(const std::string& str) {
        size_t pos = str.find(':');
        int number = std::stoi(str.substr(0, pos));
        return number;
    }

    std::pair<int, int> parseBBIdPair(const std::string& idStr) {
        size_t colonPos = idStr.find(':');
        if (colonPos == std::string::npos) {
            return std::make_pair(-1, -1); 
        }
    
        int functionId = std::stoi(idStr.substr(0, colonPos));
        int bbId = std::stoi(idStr.substr(colonPos + 1));
        return std::make_pair(functionId, bbId);
    }

    std::string serializeAnnotations(const std::vector<std::string>& annotations)
    {
        std::string result;
        for (size_t i = 0; i < annotations.size(); ++i)
        {
            result += annotations[i].c_str();
            if (i < annotations.size() - 1)
                result += ", ";
        }
        return result;
    }

    std::vector<std::string> deserializeAnnotations(const std::string& annotationsStr)
    {
        std::vector<std::string> result;
        std::istringstream iss(annotationsStr);
        std::string token;
        while (std::getline(iss, token, ','))
        {
            token.erase(0, token.find_first_not_of(" \t"));
            token.erase(token.find_last_not_of(" \t") + 1);
            if (!token.empty())
                result.push_back(token);
        }
        return result;
    }
};

} // namespace SVF

#endif