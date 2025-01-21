//
//  DeviceManager.hpp
//  usbmuxd2
//
//  Created by tihmstar on 20.07.23.
//

#ifndef DeviceManager_hpp
#define DeviceManager_hpp

#include <libgeneral/Manager.hpp>
#include <libgeneral/Event.hpp>
#include <mutex>
#include <set>

class Muxer;
class WIFIDevice;

class DeviceManager : public tihmstar::Manager {
protected:
    Muxer *_mux; //not owned
    std::set<WIFIDevice *> _children;  // Add common members
    std::mutex _childrenLck;
    tihmstar::Event _childrenEvent;

public:
    DeviceManager(Muxer *mux);
    virtual ~DeviceManager();
    
    using tihmstar::Manager::stopLoop;

    friend class WIFIDevice;
};

#endif /* DeviceManager_hpp */
