//===- SVFBugReport.cpp -- Base class for statistics---------------------------------//
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


//
// Created by JoelYang on 2023/4/19.
//

#include "Util/SVFBugReport.h"
#ifndef NDEBUG
#include <cassert>
#endif
#include "Util/cJSON.h"
#include "Util/SVFUtil.h"
#include <sstream>

using namespace std;
using namespace SVF;

std::string GenericBug::getLoc()
{
    return bugInst->getSourceLoc();
}

std::string GenericBug::getFuncName()
{
    return bugInst->getFunction()->getName();
}

std::map<GenericBug::BugType, std::string> GenericBug::bugType2Str = {
    {BOA, "Buffer Overflow"},
    {MEMLEAK, "Memory Not Free"},
    {FILENOTCLOSE, "File Not Close"},
    {DOUBLEFREE, "Memory Double Free"}
};

cJSON *BufferOverflowBug::getBugDescription()
{
    cJSON *bugDescription = cJSON_CreateObject();
    cJSON *isfullBug = cJSON_CreateBool((int)isFull);
    cJSON *allocLB = cJSON_CreateNumber(allocLowerBound);
    cJSON *allocUB = cJSON_CreateNumber(allocUpperBound);
    cJSON *accessLB = cJSON_CreateNumber(accessLowerBound);
    cJSON *accessUB = cJSON_CreateNumber(accessUpperBound);

    cJSON_AddItemToObject(bugDescription, "IsFullBug", isfullBug);
    cJSON_AddItemToObject(bugDescription, "AllocLowerBound", allocLB);
    cJSON_AddItemToObject(bugDescription, "AllocUpperBound", allocUB);
    cJSON_AddItemToObject(bugDescription, "AccessLowerBound", accessLB);
    cJSON_AddItemToObject(bugDescription, "AccessUpperBound", accessUB);

    return bugDescription;
}

void BufferOverflowBug::printBugToTerminal()
{
    stringstream bugInfo;
    if(isFull){
        SVFUtil::errs() << SVFUtil::bugMsg1("\t Full Overflow :") <<  " accessing at : ("
                        << bugInst->getSourceLoc() << ")\n";

    }else{
        SVFUtil::errs() << SVFUtil::bugMsg1("\t Partial Overflow :") <<  " accessing at : ("
                        << bugInst->getSourceLoc() << ")\n";
    }
    bugInfo << "\t\t  allocate size : [" << allocLowerBound << ", " << allocUpperBound << "], ";
    bugInfo << "access size : [" << accessLowerBound << ", " << accessUpperBound << "]\n";
    SVFUtil::errs() << "\t\t Info : \n" << bugInfo.str();
    SVFUtil::errs() << "\t\t Events : \n";
    #ifndef NDEBUG
    assert(bugEventStack != nullptr);
    #endif
    if(bugEventStack != nullptr){
        auto eventIt = bugEventStack->begin();
        for(; eventIt != bugEventStack->end(); eventIt ++){
            switch((*eventIt)->getEventType()){
            case GenericEvent::CallSite:{
                SVFUtil::errs() << "\t\t  callsite at : ( " << (*eventIt)->getEventLoc() << " )\n";
                break;
            }
            default:{  // TODO: implement more events when needed
                break;
            }
            }
        }
    }
}

cJSON * SaberBugReport::getBugDescription()
{
    cJSON *bugDescription = cJSON_CreateObject();
    cJSON *isfullBug = cJSON_CreateBool((int)isFull);
    cJSON_AddItemToObject(bugDescription, "IsFullBug", isfullBug);

    if(isFull == false){
        cJSON *pathInfo = cJSON_CreateArray();
        auto pathIt = conditionPaths.begin();
        for(; pathIt != conditionPaths.end(); pathIt++){
            cJSON *newPath = cJSON_CreateString((*pathIt).c_str());
            cJSON_AddItemToArray(pathInfo, newPath);
        }
        cJSON_AddItemToObject(bugDescription, "PathConditions", pathInfo);
    }

    return bugDescription;
}

void SaberBugReport::printBugToTerminal()
{
    switch(bugType){
    case GenericBug::MEMLEAK:{
        if(isFull){
            SVFUtil::errs() << SVFUtil::bugMsg1("\t NeverFree :") <<  " memory allocation at : ("
                            << bugInst->getSourceLoc() << ")\n";
        }else{
            SVFUtil::errs() << SVFUtil::bugMsg2("\t PartialLeak :") <<  " memory allocation at : ("
                            << bugInst->getSourceLoc() << ")\n";

            SVFUtil::errs() << "\t\t conditional free path: \n";
            for(Set<std::string>::iterator iter = conditionPaths.begin(), eiter = conditionPaths.end();
                 iter!=eiter; ++iter)
            {
                SVFUtil::errs() << "\t\t  --> (" << *iter << ") \n";
            }
            SVFUtil::errs() << "\n";
        }
        break;
    }
    case GenericBug::DOUBLEFREE:{
        SVFUtil::errs() << SVFUtil::bugMsg2("\t Double Free :") <<  " memory allocation at : ("
                        << bugInst->getSourceLoc() << ")\n";

        SVFUtil::errs() << "\t\t double free path: \n";
        for(Set<std::string>::iterator iter = conditionPaths.begin(), eiter = conditionPaths.end();
             iter!=eiter; ++iter)
        {
            SVFUtil::errs() << "\t\t  --> (" << *iter << ") \n";
        }
        SVFUtil::errs() << "\n";
        break;
    }
    case GenericBug::FILENOTCLOSE:{
        if(isFull){
            SVFUtil::errs() << SVFUtil::bugMsg1("\t FileNeverClose :") <<  " file open location at : ("
                            << bugInst->getSourceLoc() << ")\n";
        }else{
            SVFUtil::errs() << SVFUtil::bugMsg2("\t PartialFileClose :") <<  " file open location at : ("
                            << bugInst->getSourceLoc() << ")\n";

            SVFUtil::errs() << "\t\t conditional file close path: \n";
            for(Set<std::string>::iterator iter = conditionPaths.begin(), eiter = conditionPaths.end();
                 iter!=eiter; ++iter)
            {
                SVFUtil::errs() << "\t\t  --> (" << *iter << ") \n";
            }
            SVFUtil::errs() << "\n";
        }
        break;
    }
    default:{
        break;
    }
    }
}

std::map<GenericEvent::EventType, std::string> GenericEvent::eventType2Str = {
    {CallSite, "call site"},
    {Caller, "caller"},
    {Loop, "loop"},
    {Branch, "branch"}
};

std::string CallSiteEvent::getFuncName()
{
    return callSite->getCallSite()->getFunction()->getName();
}

std::string CallSiteEvent::getEventLoc()
{
    return callSite->getCallSite()->getSourceLoc();
}

std::string CallSiteEvent::getEventDescription()
{
    std::string description("calls ");
    const SVFFunction *callee = SVFUtil::getCallee(callSite->getCallSite());
    if(callee == nullptr){
        description += "<unknown>";
    }else{
        description += callee->getName();
    }
    return description;
}

std::string BranchEvent::getEventDescription()
{
    return description;
}

std::string BranchEvent::getEventLoc()
{
    return branchStmt->getInst()->getSourceLoc();
}

std::string BranchEvent::getFuncName()
{
    return branchStmt->getInst()->getFunction()->getName();
}

SVFBugReport::~SVFBugReport()
{
    auto bugIt = bugVector.begin();
    for(;bugIt != bugVector.end(); bugIt ++){
        delete (*bugIt);
    }
    auto eventStackIt = eventStackVector.begin();
    for(;eventStackIt != eventStackVector.end(); eventStackIt++){
        delete(*eventStackIt);
    }
    auto eventIt = eventSet.begin();
    for(;eventIt != eventSet.end(); eventIt++){
        delete(*eventIt);
    }
}

void SVFBugReport::pushCallSite(const CallICFGNode* callSite)
{
    CallSiteEvent *callSiteEvent = new CallSiteEvent(callSite);
    eventStack.push_back(callSiteEvent);
    eventSet.push_back(callSiteEvent);
}

void SVFBugReport::popCallSite()
{
#ifndef NDEBUG
    assert(eventStack.size() != 0 && eventStack.back()->getEventType() == GenericEvent::CallSite);
#endif
    if(eventStack.size() != 0 && eventStack.back()->getEventType() == GenericEvent::CallSite){
        eventStack.pop_back();
    }
}

std::string SVFBugReport::dumpBug()
{
    cJSON *bugArray = cJSON_CreateArray();

    auto bugIt = bugVector.begin();

    for(; bugIt != bugVector.end(); bugIt ++){
        cJSON *singleBug = cJSON_CreateObject();

        // add bug information to json
        cJSON *bugType = cJSON_CreateString(GenericBug::bugType2Str[(*bugIt)->getBugType()].c_str());
        cJSON_AddItemToObject(singleBug, "DefectType", bugType);

        cJSON *bugLoc = cJSON_Parse((*bugIt)->getLoc().c_str());
        if(bugLoc == nullptr){
            bugLoc = cJSON_CreateObject();
        }
        cJSON_AddItemToObject(singleBug, "Location", bugLoc);

        cJSON *bugFunction = cJSON_CreateString((*bugIt)->getFuncName().c_str());
        cJSON_AddItemToObject(singleBug, "Function", bugFunction);

        cJSON_AddItemToObject(singleBug, "Description", (*bugIt)->getBugDescription());

        // add event information to json
        cJSON *eventList = cJSON_CreateArray();
        EventStack *bugEventStack = (*bugIt)->getEventStack();
        if(bugEventStack != nullptr){  // add only when bug is context sensitive
            auto eventIt = bugEventStack->begin();
            for(; eventIt != bugEventStack->end(); eventIt ++){
                cJSON *singleEvent = cJSON_CreateObject();

                //event type
                cJSON *eventType = cJSON_CreateString(GenericEvent::eventType2Str[(*eventIt)->getEventType()].c_str());
                cJSON_AddItemToObject(singleEvent, "EventType", eventType);
                //function name
                cJSON *eventFunc = cJSON_CreateString((*eventIt)->getFuncName().c_str());
                cJSON_AddItemToObject(singleEvent, "Function", eventFunc);
                //event loc
                cJSON *eventLoc = cJSON_Parse((*eventIt)->getEventLoc().c_str());
                if(eventLoc == nullptr){
                    eventLoc = cJSON_CreateObject();
                }
                cJSON_AddItemToObject(singleEvent, "Location", eventLoc);
                //event description
                cJSON *eventDescription = cJSON_CreateString((*eventIt)->getEventDescription().c_str());
                cJSON_AddItemToObject(singleEvent, "Description", eventDescription);

                cJSON_AddItemToArray(eventList, singleEvent);
            }
        }
        cJSON_AddItemToObject(singleBug, "Events", eventList);
        cJSON_AddItemToArray(bugArray, singleBug);
    }
    return cJSON_Print(bugArray);
}