// Copyright 2009-2015 Sandia Corporation. Under the terms
// of Contract DE-AC04-94AL85000 with Sandia Corporation, the U.S.
// Government retains certain rights in this software.
// 
// Copyright (c) 2009-2015, Sandia Corporation
// All rights reserved.
// 
// This file is part of the SST software package. For license
// information, see the LICENSE file in the top level directory of the
// distribution.

#include <sst_config.h>
#include <sst/core/serialization.h>

#include <sst/core/simulation.h>
#include <sst/core/timeConverter.h>

#include <sst/core/oneshot.h>

namespace SST {

OneShot::OneShot(TimeConverter* timeDelay, int priority) :
    Action(),
    m_timeDelay(timeDelay),
    m_scheduled(false)
{
    setPriority(priority);
} 

OneShot::~OneShot()
{
    HandlerList_t*        ptrHandlerList;
    OneShot::HandlerBase* handler;

//    std::cout << "OneShot (destructor) #" << m_timeDelay->getFactor() << " Destructor;" << std::endl;

    // For all the entries in the map, find each vector set
    for (HandlerVectorMap_t::iterator m_it = m_HandlerVectorMap.begin(); m_it != m_HandlerVectorMap.end(); ++m_it ) {
        ptrHandlerList = m_it->second;
        
        // Delete all handlers in the list
        for (HandlerList_t::iterator v_it = ptrHandlerList->begin(); v_it != ptrHandlerList->end(); ++v_it ) {
            handler = *v_it;
            delete handler;
        }
        ptrHandlerList->clear();
        delete ptrHandlerList;
    }
    m_HandlerVectorMap.clear();
}
    
void OneShot::registerHandler(OneShot::HandlerBase* handler)
{
    HandlerList_t* ptrHandlerList;
    
    // Determine when this handler should be scheduled
    Simulation * sim = Simulation::getSimulation();
    SimTime_t nextEventTime = sim->getCurrentSimCycle() + m_timeDelay->getFactor();

    // Check to see if the nextEventTime is already in our map     
    if (m_HandlerVectorMap.find(nextEventTime) == m_HandlerVectorMap.end()) {
        // HandlerList with the specific nextEventTime not found,
        // create a new one and add it to the map of Vectors
        ptrHandlerList = new HandlerList_t(); 
//        std::cout << "OneShot (registerHandler) Creating new HandlerList for target cycle = " << nextEventTime << "; current cycle = " << Simulation::getSimulation()->getCurrentSimCycle() << std::endl;
    } else {
//        std::cout << "OneShot (registerHandler) Using existing HandlerList for target cycle = " << nextEventTime << "; current cycle = " << Simulation::getSimulation()->getCurrentSimCycle() << std::endl;
        ptrHandlerList = m_HandlerVectorMap[nextEventTime];
    }

    // Add the handler to the list of handlers
    ptrHandlerList->push_back(handler);
    
    // Set the handlerList to the map of the specific time
    m_HandlerVectorMap[nextEventTime] = ptrHandlerList; 

    // If this OneShot is not scheduled, schedule it now
    if (!m_scheduled) scheduleOneShot(nextEventTime);
}

void OneShot::scheduleOneShot(SimTime_t nextEventTime)
{
    // Add this one shot to the Activity queue, and mark this OneShot at scheduled 
    Simulation * sim = Simulation::getSimulation();
    sim->insertActivity(nextEventTime, this);
    m_scheduled = true;
}

void OneShot::execute(void) 
{
    // Execute the OneShot when the TimeVortex tells us to go. 
    // This will call all registered callbacks. 
    OneShot::HandlerBase* handler;
    HandlerList_t*        ptrHandlerList;

    Simulation* sim = Simulation::getSimulation();
    
    // Figure out the current sim time 
    SimTime_t currentEventTime = sim->getCurrentSimCycle();
    
//    std::cout << "OneShot (execute) #" << m_timeDelay->getFactor() << " Executing; At current Event cycle = " << currentEventTime << std::endl;
    
    // See if there is a handler map entry for the current time 
    if (m_HandlerVectorMap.find(currentEventTime) == m_HandlerVectorMap.end()) {
        // No entry in the map for this time, just return
        //std::cout << "OneShot (execute) No HandlerVectorMap Found - Returning" << std::endl;
        return;
    } else {
        // Get the List of Handlers for this time
        ptrHandlerList = m_HandlerVectorMap[currentEventTime];
        
        // See the list of handlers is empty
        if (ptrHandlerList->empty()) {
            // We need to clear out this vector and the map entry and return
            //std::cout << "OneShot (execute) HandlerList is Empty - Returning" << std::endl;
            delete ptrHandlerList;
            m_HandlerVectorMap.erase(currentEventTime);
            return;
        }
    }    

    // ptrHandlerList now contains all the handlers that we are supposed to callback
    
    // Walk the list of all handlers, and call them.  
    for (HandlerList_t::iterator it = ptrHandlerList->begin(); it != ptrHandlerList->end(); ++it ) {
    	handler = *it;

    	// Call the registered Callback handlers 
    	((*handler)());
    }

    // Delete the Handler list and remove it from the map
    delete ptrHandlerList;
    m_HandlerVectorMap.erase(currentEventTime);
    
    // Schedule the next callback if needed
    if (!m_HandlerVectorMap.empty()) {
        // Determine the next set of handlers (in time) that should be scheduled
        SimTime_t nextEventTime = m_HandlerVectorMap.begin()->first;
        scheduleOneShot(nextEventTime);
    } else {
        m_scheduled = false;
    }
}

void OneShot::print(const std::string& header, Output &out) const
{
    out.output("%s OneShot Activity with time delay of %" PRIu64 " to be delivered at %" PRIu64
               " with priority %d\n",
               header.c_str(), m_timeDelay->getFactor(), getDeliveryTime(), getPriority());
}

} // namespace SST

BOOST_CLASS_EXPORT_IMPLEMENT(SST::OneShot);

