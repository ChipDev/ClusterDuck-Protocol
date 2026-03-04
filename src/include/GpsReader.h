/**
 * @file GpsReader.h
 * @brief Non-blocking GPS reader backed by gpsd / libgps.
 *
 * Connects to the gpsd daemon over its local socket and polls for fresh
 * TPV (time-position-velocity) reports. Call poll() every loop iteration —
 * it is non-blocking and returns true when a valid fix has been received.
 *
 * Setup (run once on the Pi):
 *   sudo apt install gpsd gpsd-clients libgps-dev
 *   sudo gpsd /dev/ttyAMA0 -F /var/run/gpsd.sock
 *
 * Verify gpsd is feeding data:
 *   gpsmon        # live NMEA stream
 *   cgps -s       # parsed fix display
 *
 * Link with: -lgps
 */

#pragma once

#include <string>
#include <cstdio>
#include <cmath>
#include <gps.h>

class GpsReader {
public:
    GpsReader()
        : connected_(false)
        , gpsFix_(false)
        , latitude_(0.0)
        , longitude_(0.0)
        , altitude_(0.0)
    {}

    ~GpsReader() {
        if (connected_) {
            gps_stream(&gpsData_, WATCH_DISABLE, nullptr);
            gps_close(&gpsData_);
        }
    }

    /**
     * @brief Connect to the gpsd daemon and start the data stream.
     *
     * gpsd must already be running and have the GPS device open.
     * See file header for setup instructions.
     *
     * @return true on success, false if gpsd is not reachable
     */
    bool begin() {
        if (gps_open("localhost", DEFAULT_GPSD_PORT, &gpsData_) != 0) {
            fprintf(stderr, "[GPS] Cannot connect to gpsd: %s\n",
                    gps_errstr(errno));
            fprintf(stderr, "[GPS] Is gpsd running? "
                    "Try: sudo gpsd /dev/ttyAMA0 -F /var/run/gpsd.sock\n");
            return false;
        }

        gps_stream(&gpsData_, WATCH_ENABLE | WATCH_JSON, nullptr);
        connected_ = true;
        fprintf(stdout, "[GPS] Connected to gpsd\n");
        return true;
    }

    /**
     * @brief Check for a fresh GPS report from gpsd.
     *
     * Non-blocking — returns immediately if no new data is available.
     * Updates internal lat/lon/fix state when a valid TPV report arrives.
     *
     * @return true if a valid fix was received this call
     */
    bool poll() {
        if (!connected_) return false;

        // gps_waiting returns true if data is ready within 0 microseconds
        // (i.e. already buffered) — purely non-blocking.
        if (!gps_waiting(&gpsData_, 0)) return false;

        if (gps_read(&gpsData_, nullptr, 0) == -1) {
            fprintf(stderr, "[GPS] gps_read error: %s\n", gps_errstr(errno));
            return false;
        }

        // MODE_2D (2) or MODE_3D (3) means a valid fix regardless of gpsd version.
        if (gpsData_.fix.mode >= MODE_2D &&
            std::isfinite(gpsData_.fix.latitude) &&
            std::isfinite(gpsData_.fix.longitude)) {

            latitude_  = gpsData_.fix.latitude;
            longitude_ = gpsData_.fix.longitude;
            // Altitude is only valid in 3D fix; default to 0.0 for 2D
            altitude_  = (gpsData_.fix.mode >= MODE_3D &&
                          std::isfinite(gpsData_.fix.altitude)) ?
                          gpsData_.fix.altitude : 0.0;
            gpsFix_    = true;
            return true;
        }

        gpsFix_ = false;
        return false;
    }

    // ── Accessors ─────────────────────────────────────────────────────────────

    bool   hasFix()    const { return gpsFix_;    }
    double latitude()  const { return latitude_;  }
    double longitude() const { return longitude_; }
    double altitude()  const { return altitude_;  }

    /**
     * @brief Format a CDP payload string for the location topic.
     *
     * Format: "Lat:<decimal> Lng:<decimal> Alt:<decimal>"
     * Only call when hasFix() is true.
     */
    std::string formatMessage() const {
        char buf[80];
        snprintf(buf, sizeof(buf), "Lat:%.5f Lng:%.5f Alt:%.2f",
                 latitude_, longitude_, altitude_);
        return std::string(buf);
    }

private:
    struct gps_data_t gpsData_;
    bool   connected_;
    bool   gpsFix_;
    double latitude_;
    double longitude_;
    double altitude_;
};