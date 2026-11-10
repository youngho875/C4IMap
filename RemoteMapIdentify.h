#pragma once
#include "MapSources.h"
#include <atomic>
#include <condition_variable>
#include <cpl_http.h>
#include <cpl_json.h>
#include <cpl_string.h>
#include <iomanip>
#include <mutex>
#include <ogrsf_frmts.h>
#include <sstream>
#include <thread>

struct RemoteMapHit
{
public:
    std::string title;
    std::vector<std::string> fields, values;
};

// One background worker; a new click replaces an older request/result.
class RemoteMapIdentify
{
private:
    struct Request
    {
    public:
        std::vector<MapSourceLayer> sources;
        double west = 0, south = 0, east = 0, north = 0;
        unsigned serial = 0;
    };
    std::mutex mutex;
    std::condition_variable cv;
    std::atomic<unsigned> serial{0};
    bool stop = false, pending = false, busy = false, ready = false;
    unsigned activeSerial = 0;
    Request request;
    RemoteMapHit result;
    std::thread worker;

    static std::string Encode(const std::string &value);
    static std::string Number(double value);
    static void AppendFeature(RemoteMapHit &hit, const MapSourceLayer &source,
                              const std::string &id, const std::vector<std::string> &names,
                              const std::vector<std::string> &values);
    static std::string QueryWms(const MapSourceLayer &source, const Request &q, RemoteMapHit &hit);
    std::string QueryWfs(const MapSourceLayer &source, const Request &q, RemoteMapHit &hit);
    void Run();

public:
    RemoteMapIdentify();
    ~RemoteMapIdentify();
    void Cancel();
    void Start(std::vector<MapSourceLayer> sources, double west, double south, double east,
               double north);
    bool Pending();
    bool Poll(RemoteMapHit &hit);
};
