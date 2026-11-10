#include "pch.h"
#include "RemoteMapIdentify.h"
#include "MapBuildOptions.h"
std::string RemoteMapIdentify::Encode(const std::string &value)
{
    char *escaped = CPLEscapeString(value.c_str(), -1, CPLES_URL);
    std::string output = escaped ? escaped : "";
    CPLFree(escaped);
    return output;
}

std::string RemoteMapIdentify::Number(double value)
{
    std::ostringstream out;
    out.imbue(std::locale::classic());
    out << std::setprecision(16) << value;
    return out.str();
}

void RemoteMapIdentify::AppendFeature(RemoteMapHit &hit, const MapSourceLayer &source,
                                      const std::string &id, const std::vector<std::string> &names,
                                      const std::vector<std::string> &values)
{
    hit.fields.push_back(source.name + " / " + id);
    hit.values.push_back(source.layer);
    hit.fields.insert(hit.fields.end(), names.begin(), names.end());
    hit.values.insert(hit.values.end(), values.begin(), values.end());
}

std::string RemoteMapIdentify::QueryWms(const MapSourceLayer &source, const Request &q,
                                        RemoteMapHit &hit)
{
    std::string url = source.identifyEndpoint.empty() ? source.endpoint : source.identifyEndpoint;
    url += url.find('?') == std::string::npos ? "?"
                                              : (url.back() == '?' || url.back() == '&' ? "" : "&");
    auto layer = source.identifyLayer.empty() ? source.layer : source.identifyLayer;
    bool v13 = source.version == "1.3.0";
    std::string bbox =
        v13 ? Number(q.south) + "," + Number(q.west) + "," + Number(q.north) + "," + Number(q.east)
            : Number(q.west) + "," + Number(q.south) + "," + Number(q.east) + "," + Number(q.north);
    url += "SERVICE=WMS&REQUEST=GetFeatureInfo&VERSION=" + Encode(source.version) +
           "&LAYERS=" + Encode(layer) + "&QUERY_LAYERS=" + Encode(layer) +
           "&STYLES=" + Encode(source.style) +
           "&FORMAT=image%2Fpng&INFO_FORMAT=application%2Fjson&FEATURE_COUNT=10&WIDTH=15&HEIGHT=15&"
           "BBOX=" +
           bbox + (v13 ? "&CRS=EPSG%3A4326&I=7&J=7" : "&SRS=EPSG%3A4326&X=7&Y=7");
    const char *options[] = {"TIMEOUT=5", "CONNECTTIMEOUT=3", "MAX_RETRY=0", nullptr};
    auto response = CPLHTTPFetch(url.c_str(), options);
    if (!response)
        return "WMS connection failed";
    std::string error;
    if (response->nStatus || response->pszErrBuf)
        error = response->pszErrBuf ? response->pszErrBuf : "WMS HTTP error";
    else if (response->nDataLen > 8 * 1024 * 1024)
        error = "WMS response exceeds 8 MB";
    // MapServer returns an empty application/json body for zero query hits.
    else if (response->nDataLen > 0)
    {
        CPLJSONDocument document;
        if (!response->pabyData || !document.LoadMemory(response->pabyData, response->nDataLen))
            error = "WMS GetFeatureInfo requires application/json (GeoJSON)";
        else
        {
            auto features = document.GetRoot().GetArray("features");
            if (!features.IsValid())
                error = "WMS response has no GeoJSON features array";
            for (int i = 0; i < features.Size() && i < 10; ++i)
            {
                auto feature = features[i];
                std::vector<std::string> fields, values;
                for (auto property : feature.GetObj("properties").GetChildren())
                {
                    fields.push_back(property.GetName());
                    values.push_back(property.GetType() == CPLJSONObject::Type::String
                                         ? property.ToString()
                                         : property.Format(CPLJSONObject::PrettyFormat::Plain));
                }
                AppendFeature(hit, source, feature.GetString("id", std::to_string(i + 1)), fields,
                              values);
            }
        }
    }
    CPLHTTPDestroyResult(response);
    return error;
}

std::string RemoteMapIdentify::QueryWfs(const MapSourceLayer &source, const Request &q,
                                        RemoteMapHit &hit)
{
    auto endpoint = source.identifyEndpoint.empty() ? source.endpoint : source.identifyEndpoint;
    auto name = source.identifyLayer.empty() ? source.layer : source.identifyLayer;
    const char *drivers[] = {"WFS", nullptr};
    std::unique_ptr<GDALDataset, decltype(&GDALClose)> ds(
        static_cast<GDALDataset *>(GDALOpenEx(("WFS:" + endpoint).c_str(),
                                              GDAL_OF_VECTOR | GDAL_OF_READONLY, drivers, nullptr,
                                              nullptr)),
        GDALClose);
    if (!ds)
        return "WFS open failed: " + std::string(CPLGetLastErrorMsg());
    auto layer = ds->GetLayerByName(name.c_str());
    if (!layer || !layer->GetSpatialRef())
        return "WFS layer/SRS not found: " + name;
    OGRSpatialReference wgs;
    wgs.SetWellKnownGeogCS("WGS84");
    wgs.SetAxisMappingStrategy(OAMS_TRADITIONAL_GIS_ORDER);
    auto native = std::unique_ptr<OGRSpatialReference>(layer->GetSpatialRef()->Clone());
    native->SetAxisMappingStrategy(OAMS_TRADITIONAL_GIS_ORDER);
    auto toNative =
        std::unique_ptr<OGRCoordinateTransformation, decltype(&OCTDestroyCoordinateTransformation)>(
            OGRCreateCoordinateTransformation(&wgs, native.get()),
            OCTDestroyCoordinateTransformation);
    auto toWgs =
        std::unique_ptr<OGRCoordinateTransformation, decltype(&OCTDestroyCoordinateTransformation)>(
            OGRCreateCoordinateTransformation(native.get(), &wgs),
            OCTDestroyCoordinateTransformation);
    double x[] = {q.west, q.east, q.east, q.west}, y[] = {q.south, q.south, q.north, q.north};
    if (!toNative || !toWgs || !toNative->Transform(4, x, y))
        return "WFS coordinate conversion failed";
    layer->SetSpatialFilterRect(*std::min_element(x, x + 4), *std::min_element(y, y + 4),
                                *std::max_element(x, x + 4), *std::max_element(y, y + 4));
    OGRLinearRing ring;
    ring.addPoint(q.west, q.south);
    ring.addPoint(q.east, q.south);
    ring.addPoint(q.east, q.north);
    ring.addPoint(q.west, q.north);
    ring.addPoint(q.west, q.south);
    OGRPolygon box;
    box.addRing(&ring);
    int count = 0, scanned = 0;
    CPLErrorReset();
    while (q.serial == serial && count < 10 && scanned++ < 1000)
    {
        auto feature = std::unique_ptr<OGRFeature, decltype(&OGRFeature::DestroyFeature)>(
            layer->GetNextFeature(), OGRFeature::DestroyFeature);
        if (!feature)
            break;
        auto raw = feature->GetGeometryRef();
        if (!raw)
            continue;
        auto geometry = std::unique_ptr<OGRGeometry>(raw->clone());
        if (geometry->transform(toWgs.get()) != OGRERR_NONE || !geometry->Intersects(&box))
            continue;
        std::vector<std::string> fields, values;
        for (int i = 0; i < feature->GetFieldCount(); ++i)
        {
            fields.push_back(feature->GetFieldDefnRef(i)->GetNameRef());
            values.push_back(feature->IsFieldSetAndNotNull(i) ? feature->GetFieldAsString(i)
                                                              : "NULL");
        }
        AppendFeature(hit, source, std::to_string(feature->GetFID()), fields, values);
        ++count;
    }
    return CPLGetLastErrorType() >= CE_Failure ? CPLGetLastErrorMsg() : "";
}

void RemoteMapIdentify::Run()
{
    CPLSetThreadLocalConfigOption("GDAL_HTTP_TIMEOUT", "5");
    CPLSetThreadLocalConfigOption("GDAL_HTTP_CONNECTTIMEOUT", "3");
    CPLSetThreadLocalConfigOption("GDAL_HTTP_MAX_RETRY", "0");
    CPLPushErrorHandler(CPLQuietErrorHandler);
    for (;;)
    {
        Request q;
        {
            std::unique_lock<std::mutex> lock(mutex);
            cv.wait(lock, [&] { return stop || pending; });
            if (stop)
                break;
            q = request;
            activeSerial = q.serial;
            pending = false;
            busy = true;
        }
        RemoteMapHit hit;
        std::vector<std::string> errors;
        for (auto it = q.sources.rbegin(); it != q.sources.rend() && q.serial == serial; ++it)
        {
            CPLErrorReset();
            const auto &source = *it;
            const bool wfs = source.type == "WFS" || source.identifyType == "WFS";
            auto error = wfs ? QueryWfs(source, q, hit) : QueryWms(source, q, hit);
            if (!error.empty())
                errors.push_back(source.name + ": " + error);
        }
        hit.title =
            hit.fields.empty() ? "No feature at selected location" : "Map feature attributes";
        for (const auto &error : errors)
        {
            hit.fields.push_back("Query error");
            hit.values.push_back(error);
        }
        {
            std::lock_guard<std::mutex> lock(mutex);
            busy = false;
            if (q.serial == serial)
            {
                result = std::move(hit);
                ready = true;
            }
        }
    }
    CPLPopErrorHandler();
}

RemoteMapIdentify::RemoteMapIdentify() : worker([this] { Run(); })
{
}

RemoteMapIdentify::~RemoteMapIdentify()
{
    {
        std::lock_guard<std::mutex> lock(mutex);
        stop = true;
        ++serial;
        cv.notify_all();
    }
    worker.join();
}

void RemoteMapIdentify::Cancel()
{
    std::lock_guard<std::mutex> lock(mutex);
    ++serial;
    pending = false;
    ready = false;
}

void RemoteMapIdentify::Start(std::vector<MapSourceLayer> sources, double west, double south,
                              double east, double north)
{
    std::lock_guard<std::mutex> lock(mutex);
    request = {std::move(sources), west, south, east, north, ++serial};
    pending = true;
    ready = false;
    cv.notify_one();
}

bool RemoteMapIdentify::Pending()
{
    std::lock_guard<std::mutex> lock(mutex);
    return pending || (busy && activeSerial == serial);
}

bool RemoteMapIdentify::Poll(RemoteMapHit &hit)
{
    std::lock_guard<std::mutex> lock(mutex);
    if (!ready)
        return false;
    hit = std::move(result);
    ready = false;
    return true;
}
