//===- SVFBugRecoder.cpp -- Base class for statistics---------------------------------//
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

#include "Util/SVFBugRecoder.h"
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
    {BOA, "Buffer Overflow"}
};

std::string BufferOverflowBug::getBugDescription()
{
    stringstream description;
    description << "allocated: ["
                << allocLowerBound << ", "
                << allocUpperBound << "], access: ["
                << accessLowerBound << ", "
                << accessUpperBound << "]";
    return description.str();
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

SVFBugRecoder::~SVFBugRecoder()
{
    auto bugIt = bugVector.begin();
    for(;bugIt != bugVector.end(); bugIt ++){
        delete (*bugIt);
    }
    auto eventStackIt = eventStackVector.begin();
    for(;eventStackIt != eventStackVector.end(); eventStackIt++){
        delete(*eventStackIt);
    }
    auto eventIt = eventStack.begin();
    for(;eventIt != eventStack.end(); eventIt++){
        delete(*eventIt);
    }
}

void SVFBugRecoder::pushCallSite(const CallICFGNode* callSite)
{
    CallSiteEvent *callSiteEvent = new CallSiteEvent(callSite);
    eventStack.push_back(callSiteEvent);
}

void SVFBugRecoder::popCallSite()
{
#ifndef NDEBUG
    assert(eventStack.size() != 0 && eventStack.back()->getEventType() == GenericEvent::CallSite);
#endif
    if(eventStack.size() != 0 && eventStack.back()->getEventType() == GenericEvent::CallSite){
        eventStack.pop_back();
    }
}

std::string SVFBugRecoder::dumpBug()
{
    cJSON *bugArray = cJSON_CreateArray();

    auto bugIt = bugVector.begin();
    auto eventStackIt = eventStackVector.begin();

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


        cJSON *bugDescription = cJSON_CreateString((*bugIt)->getBugDescription().c_str());
        cJSON_AddItemToObject(singleBug, "Description", bugDescription);

        // add event information to json
        cJSON *eventList = cJSON_CreateArray();
        auto eventIt = (*eventStackIt)->begin();
        for(; eventIt != (*eventStackIt)->end(); eventIt ++){
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
        cJSON_AddItemToObject(singleBug, "Events", eventList);
        cJSON_AddItemToArray(bugArray, singleBug);
        eventStackIt ++;
    }
    return cJSON_Print(bugArray);
}