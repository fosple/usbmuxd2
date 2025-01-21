//
//  WIFIDeviceManager-direct.hpp
//  usbmuxd2
//
//  Created by tihmstar on 27.05.19.
//  Copyright © 2019 tihmstar. All rights reserved.
//

#ifndef WIFIDeviceManager_direct_hpp
#define WIFIDeviceManager_direct_hpp

#include "DeviceManager.hpp"
#include "../Muxer.hpp"
#include "../Devices/WIFIDevice.hpp"

#include <libgeneral/DeliveryEvent.hpp>
#include <string>

class WIFIDeviceManager_direct : public DeviceManager {
private:
    std::set<WIFIDevice *> _children;  //raw ptr to shared objec
    std::mutex _childrenLck;
    tihmstar::Event _childrenEvent;
    std::thread _devReaperThread;
    tihmstar::DeliveryEvent<std::shared_ptr<WIFIDevice>> _reapDevices;
    
    std::string _targetIP;
    bool _isConnected;
    int _wakePipe[2];
    bool _shouldStop;
    bool _hasAttemptedFirstConnection;
    
    std::string _pairRecordId;

    virtual bool loopEvent() override;
    virtual void stopAction() noexcept override;

    void reaper_runloop();
    void tryConnect();
    bool isValidIPAddress(const std::string& ip);

public:
    WIFIDeviceManager_direct(Muxer *mux, const std::string &ipAddress, const std::string &pairRecordId);
    virtual ~WIFIDeviceManager_direct() override;

    void device_add(std::shared_ptr<WIFIDevice> dev, bool notify = true);

    friend WIFIDevice;
};

#endif /* WIFIDeviceManager_direct_hpp */ 