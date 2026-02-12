/**
 * @file Neighbor.h
 * @brief This file is internal to CDP and sorts nearest neighbors
 * on a route path
 * @version
 * @date 2025-7-24
 *
 * @copyright
 */
#ifndef NEIGHBOR_H
#define NEIGHBOR_H
#include <ArduinoJson.h>

#include <list>
#include <optional>
#include <string>

#include "../CdpPacket.h"

#include "SignalScore.h"

class Neighbor {
    public:
      Neighbor(Duid devId, Duid nextHop, SignalScore signalInfo, unsigned long lastSeen) :
        DeviceId(devId),  
        lastSeen(lastSeen), 
        snr(signalInfo.snr), 
        rssi(signalInfo.rssi),
        routingScore(signalInfo.signalScore) 
    {
        // How to handle multiple next hops?
      }
        bool operator>(const Neighbor& other) const {
            return this->routingScore > other.routingScore;
        }
  
      [[nodiscard]] std::string getDeviceId() const { return duckutils::toString(DeviceId); }
      long getRoutingScore() const { return routingScore; }
      unsigned long getLastSeen() const { return lastSeen; }
      long getSnr() { return snr; }
      long getRssi() { return rssi; }
  private:
      Duid DeviceId;
      unsigned long lastSeen;
      float snr, rssi, routingScore;
  };
    
  #endif
