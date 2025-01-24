//
//  WIFIDevice.cpp
//  usbmuxd2
//
//  Created by tihmstar on 21.06.19.
//  Copyright © 2019 tihmstar. All rights reserved.
//

#include <libgeneral/macros.h>

#ifdef HAVE_LIBIMOBILEDEVICE
#include "WIFIDevice.hpp"
#include "../Muxer.hpp"
#include "../sysconf/sysconf.hpp"

#ifdef HAVE_WIFI_AVAHI
#   include "../Manager/WIFIDeviceManager-avahi.hpp"
#elif HAVE_WIFI_MDNS
#   include "../Manager/WIFIDeviceManager-mDNS.hpp"
#endif //HAVE_AVAHI

#include <plist/plist.h>

#include <assert.h>
#include <string.h>

#if defined(HAVE_WIFI_AVAHI) || defined(HAVE_WIFI_MDNS)

WIFIDevice::WIFIDevice(Muxer *mux, DeviceManager *parent, const std::string &uuid,
                       const std::vector<std::string> &addresses,
                       const std::string &serviceName,
                       int interfaceIndex)
: Device(mux, MUXCONN_WIFI), tihmstar::Manager()
, _parent(parent), _selfref()
, _ipaddr(addresses), _serviceName(serviceName)
, _interfaceIndex(interfaceIndex)
, _hbclient(NULL), _hbrsp(NULL), _idev(NULL)
{
    strncpy(_serial, uuid.c_str(), sizeof(_serial)-1);
    _serial[sizeof(_serial)-1] = '\0';
}

WIFIDevice::~WIFIDevice() {
    debug("deleting device %s", _serial);
    {
        std::unique_lock<std::mutex> ul(_parent->_childrenLck);
        _parent->_children.erase(this);
        _parent->_childrenEvent.notifyAll();
        _parent = NULL;
    }
#ifdef HAVE_LIBIMOBILEDEVICE
    safeFreeCustom(_hbclient, heartbeat_client_free);
    safeFreeCustom(_idev, idevice_free);
#endif //HAVE_LIBIMOBILEDEVICE
    safeFreeCustom(_hbrsp, plist_free);
}

bool WIFIDevice::loopEvent(){
#ifndef HAVE_LIBIMOBILEDEVICE
    reterror("Compiled without libimobiledevice");
#else
    plist_t hbeat = NULL;
    cleanup([&]{
        safeFreeCustom(hbeat, plist_free);
    });
    heartbeat_error_t hret = HEARTBEAT_E_SUCCESS;

    try {
        retassure((hret = heartbeat_receive_with_timeout(_hbclient,&hbeat,15000)) == HEARTBEAT_E_SUCCESS, "[WIFIDevice] failed to recv heartbeat with error=%d",hret);
        retassure((hret = heartbeat_send(_hbclient,_hbrsp)) == HEARTBEAT_E_SUCCESS,"[WIFIDevice] failed to send heartbeat");
    } catch (const std::exception& e) {
        error("Lost connection to device, trying to reconnect...");

        debug("[WIFIDevice] Exception caught: %s", e.what());
        return false; // Exit the loop
    }

    return true;
#endif //HAVE_LIBIMOBILEDEVICE
}

void WIFIDevice::beforeLoop(){
    retassure(_hbclient, "Not starting loop, because we don't have a _hbclient");
}

void WIFIDevice::afterLoop() noexcept{
    kill();
}

void WIFIDevice::kill() noexcept {
    debug("Killing WIFIDevice %s", _serial);
    
    try {

        if (auto p = dynamic_cast<WIFIDeviceManager*>(_parent)) {
        p->_reapDevices.post(std::static_pointer_cast<WIFIDevice>(_selfref.lock()));
        } else if (auto p = dynamic_cast<WIFIDeviceManager_direct*>(_parent)) {
            p->_reapDevices.post(std::static_pointer_cast<WIFIDevice>(_selfref.lock()));
        }
    } catch (...) {
        debug("Failed to kill WIFIDevice %s", _serial);
    }

}


void WIFIDevice::deconstruct() noexcept{
    debug("[Deconstructing] WIFIDevice %s",_serial);
    std::shared_ptr<WIFIDevice> selfref = _selfref.lock();
    stopLoop();
    _mux->delete_device(selfref);
}

void WIFIDevice::startLoop(){
#ifndef HAVE_LIBIMOBILEDEVICE
    reterror("Compiled without libimobiledevice");
#else
    heartbeat_error_t hret = HEARTBEAT_E_SUCCESS;
    _loopState = tihmstar::LOOP_STOPPED;
    
    assure(_hbrsp = plist_new_dict());
    plist_dict_set_item(_hbrsp, "Command", plist_new_string("Polo"));
    
    assure(!idevice_new_with_options(&_idev,_serial, IDEVICE_LOOKUP_NETWORK));

    retassure((hret = heartbeat_client_start_service(_idev, &_hbclient, "usbmuxd2")) == HEARTBEAT_E_SUCCESS,"[WIFIDevice] Failed to start heartbeat service with error=%d",hret);

    _loopState = tihmstar::LOOP_UNINITIALISED;
    Manager::startLoop();
#endif //HAVE_LIBIMOBILEDEVICE
}


void WIFIDevice::start_connect(uint16_t dport, std::shared_ptr<Client> cli){
    reterror("Legacy connection proxying is currently not implemented");
}

#endif //defined(HAVE_WIFI_AVAHI) || defined(HAVE_WIFI_MDNS)
#endif //HAVE_LIBIMOBILEDEVICE
