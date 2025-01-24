//
//  WIFIDeviceManager-direct.cpp
//  usbmuxd2
//
//  Created by tihmstar on 27.05.19.
//  Copyright © 2019 tihmstar. All rights reserved.
//

#include <libgeneral/macros.h>
#include "WIFIDeviceManager-direct.hpp"
#include <sysconf/sysconf.hpp>
#include <unistd.h>
#include <poll.h>
#include <string.h>
#include <arpa/inet.h>

WIFIDeviceManager_direct::WIFIDeviceManager_direct(Muxer *mux, const std::string &ipAddress, const std::string &pairRecordId)
: DeviceManager(mux), _targetIP(ipAddress), _pairRecordId(pairRecordId), _isConnected(false), _shouldStop(false), _hasAttemptedFirstConnection(false)
{
    debug("WIFIDeviceManager direct-connect to IP: %s with PairRecordID: %s", _targetIP.c_str(), _pairRecordId.c_str());
    
    assure(!pipe(_wakePipe));
    
    _devReaperThread = std::thread([this]{
        reaper_runloop();
    });
}

WIFIDeviceManager_direct::~WIFIDeviceManager_direct(){
    debug("WIFIDeviceManager direct destructor starting");
    stopLoop();
    if (_children.size()) {
        debug("waiting for wifi children to die...");
        std::unique_lock<std::mutex> ul(_childrenLck);
        while (size_t s = _children.size()) {
            debug("Need to kill %zu wifi children",s);
            for (auto c : _children) {
                debug("Killing child device");
                c->kill();
            }
            uint64_t wevent = _childrenEvent.getNextEvent();
            ul.unlock();
            _childrenEvent.waitForEvent(wevent);
            ul.lock();
        }
    }
    debug("All children killed, cleaning up reaper thread");
    _reapDevices.kill();
    _devReaperThread.join();
    
    debug("Closing pipes");
    safeClose(_wakePipe[0]);
    safeClose(_wakePipe[1]);
    debug("WIFIDeviceManager direct destructor completed");
}

void WIFIDeviceManager_direct::device_add(std::shared_ptr<WIFIDevice> dev, bool notify) {
    debug("Adding new WIFIDevice to manager (notify=%s)", notify ? "true" : "false");
    try {
        std::unique_lock<std::mutex> ul(_childrenLck);
        dev->_selfref = dev;
        _children.insert(dev.get());
        debug("Device added to children list, total devices: %zu", _children.size());
        _mux->add_device(dev, notify);
        debug("Device successfully registered with muxer");
    } catch (const tihmstar::exception& e) {
        error("Failed to add device: %s", e.what());
        std::unique_lock<std::mutex> ul(_childrenLck);
        _children.erase(dev.get());
        throw;
    }
}

bool WIFIDeviceManager_direct::loopEvent(){
    if (_shouldStop) {
        debug("Loop stopping due to stop flag");
        return false;
    }
    
    if (!_hasAttemptedFirstConnection) {
        _hasAttemptedFirstConnection = true;
        tryConnect();
        return !_shouldStop;
    }

    struct pollfd pfds[] = {
        {
            .fd = _wakePipe[0],
            .events = POLLIN
        }
    };
    
    int res = poll(pfds, 1, 10000); // 10 second timeout
    
    if (res < 0) {
        if (errno != EINTR) {
            error("Poll failed with error: %s", strerror(errno));
        }
        return !_shouldStop;
    }
    
    if (res == 0) {
        // Timeout
        if (!_isConnected && !_shouldStop) {
            debug("Poll timeout, attempting reconnect");
            tryConnect();
        }
    } else if (res > 0) {
        if (pfds[0].revents & (POLLHUP | POLLERR)) {
            debug("Poll detected pipe closed");
            return false;
        }
        if (pfds[0].revents & POLLIN) {
            char buf[2];
            if (read(_wakePipe[0], buf, 1) == 1) {
                debug("Received wake signal");
            }
        }
    }
    
    return !_shouldStop;
}

void WIFIDeviceManager_direct::stopAction() noexcept{
    debug("Stop action triggered");
    _shouldStop = true;
    // Wake up the poll() call
    char c = 'q';
    if (write(_wakePipe[1], &c, 1) != 1) {
        error("Failed to write to wake pipe");
    }
    safeClose(_wakePipe[1]);
}

void WIFIDeviceManager_direct::reaper_runloop(){
    debug("Reaper thread starting");
    while (true) {
        std::shared_ptr<WIFIDevice> dev;
        try {
            dev = _reapDevices.wait();
            debug("Reaper received device to clean up");
        } catch (...) {
            debug("Reaper thread interrupted, exiting");
            break;
        }
        
        {
            std::unique_lock<std::mutex> ul(_childrenLck);
            _children.erase(dev.get());
            _childrenEvent.notifyAll();
        }
        
        _isConnected = false;
        debug("Deconstructing device");
        dev->deconstruct();
        debug("Device cleanup completed");
    }
}

void WIFIDeviceManager_direct::tryConnect(){
    try {
        if (_targetIP.empty() || _targetIP.length() > 45) {
            debug("Connection attempt failed: IP address is empty or too long (%zu chars)", _targetIP.length());
            reterror("Invalid IP address");
        }
        
        if (!isValidIPAddress(_targetIP)) {
            debug("Connection attempt failed: IP validation check failed for address: %s", _targetIP.c_str());
            reterror("Malformed IP address: %s", _targetIP.c_str());
        }

        std::vector<std::string> addrs = {_targetIP};
        std::string serviceName = "direct-" + _targetIP;
        std::string uuid = _pairRecordId;
        
        debug("Attempting direct connection with parameters:");
        debug(" - Target IP: %s", _targetIP.c_str());
        debug(" - Service Name: %s", serviceName.c_str());
        debug(" - Device UUID/PairRecordID: %s", uuid.c_str());
        
        if (_mux->have_wifi_device_with_ip(addrs)) {
            debug("Connection attempt skipped: Device at %s is already connected and registered", _targetIP.c_str());
            return;
        }
        
        info("Initiating connection to device at %s...", _targetIP.c_str());
        
        std::shared_ptr<WIFIDevice> dev = std::make_shared<WIFIDevice>(
            _mux,
            this,
            uuid,
            addrs,
            serviceName,
            0  // interfaceIndex
        );
        
        debug("Successfully created WIFIDevice object, adding to device manager...");
        try {
            device_add(dev, true);
            info("Successfully connected to device at %s", _targetIP.c_str());
            debug("Connection details:");
            debug(" - Device UUID: %s", uuid.c_str());
            debug(" - Service: %s", serviceName.c_str());
            _isConnected = true;
        } catch (const std::exception& e) {
            _isConnected = false;
            throw; // Re-throw to ensure the outer catch block handles it
        }
        
    } catch (tihmstar::exception &e) {
        error("Connection attempt to %s failed with error code %d: %s", _targetIP.c_str(), e.code(), e.what());
        debug("Full connection error details: code=%d, msg='%s'", e.code(), e.what());
    }
}

bool WIFIDeviceManager_direct::isValidIPAddress(const std::string& ip) {
    struct sockaddr_in sa;
    struct sockaddr_in6 sa6;
    
    debug("Validating IP address: %s", ip.c_str());
    
    // Try IPv4
    if (inet_pton(AF_INET, ip.c_str(), &(sa.sin_addr)) == 1) {
        debug("IP validation successful: Valid IPv4 address");
        return true;
    }
    
    // Try IPv6
    if (inet_pton(AF_INET6, ip.c_str(), &(sa6.sin6_addr)) == 1) {
        debug("IP validation successful: Valid IPv6 address");
        return true;
    }
    
    debug("IP validation failed: Address is neither valid IPv4 nor IPv6");
    return false;
} 