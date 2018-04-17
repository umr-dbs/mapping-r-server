#define RSERVER_FORKED_MODE true // specify whether the server runs forked or threaded

#include "util/exceptions.h"
#include "util/binarystream.h"
#include "util/server_nonblocking.h"
#include "util/log.h"
#include "util/configuration.h"

#include "operators/operator.h"
#include "operators/processing/scripting/r_script.h"

#include "datatypes/raster.h"
#include "raster/profiler.h"

#include <iostream>
#include <fstream>

#include <csignal>


#ifdef __clang__ // Prevent GCC from complaining about unknown pragmas.
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunused-parameter" // silence the warnings of the Rcpp headers
#endif // __clang__

#include <Rcpp.h>
#include <RInside.h>

#pragma clang diagnostic pop // ignored "-Wunused-parameter"

#include "rcpp_wrapper.h"
#include "rinside_callbacks.h"


// Set to true while you're sending. If an exception happens when not sending, an error message can be returned
std::atomic<bool> is_sending(false);


std::unique_ptr<GenericRaster> query_raster_source(BinaryStream &stream, int childidx, const QueryRectangle &rect) {
    Profiler::Profiler{"requesting Raster"};

    Log::debug("requesting raster %d with rect (%f,%f -> %f,%f)", childidx, rect.x1, rect.y1, rect.x2, rect.y2);
    is_sending = true;
    BinaryWriteBuffer response;
    response.write<const char &>(RSERVER_TYPE_RASTER);
    response.write<int &>(childidx);
    response.write<const QueryRectangle &>(rect);
    stream.write(response);
    is_sending = false;

    BinaryReadBuffer new_request;
    stream.read(new_request);
    auto raster = GenericRaster::deserialize(new_request);
    raster->setRepresentation(GenericRaster::Representation::CPU);
    return raster;
}

Rcpp::NumericVector query_raster_source_as_array(BinaryStream &stream, int childidx, const QueryRectangle &rect) {
    auto raster = query_raster_source(stream, childidx, rect);

    // convert to vector
    raster->setRepresentation(GenericRaster::Representation::CPU);
    int width = raster->width;
    int height = raster->height;
    Rcpp::NumericVector pixels(raster->getPixelCount());
    int pos = 0;
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            double val = raster->getAsDouble(x, y);
            if (raster->dd.is_no_data(val))
                pixels[pos++] = NAN;
            else
                pixels[pos++] = val;
        }
    }
    return pixels;
}

std::unique_ptr<PointCollection> query_points_source(BinaryStream &stream, int childidx, const QueryRectangle &rect) {
    Profiler::Profiler{"requesting Points"};

    Log::debug("requesting points %d with rect (%f,%f -> %f,%f)", childidx, rect.x1, rect.y1, rect.x2, rect.y2);
    is_sending = true;
    BinaryWriteBuffer response;
    response.write<const char &>(RSERVER_TYPE_POINTS);
    response.write<int &>(childidx);
    response.write<const QueryRectangle &>(rect);
    stream.write(response);
    is_sending = false;

    BinaryReadBuffer new_request;
    stream.read(new_request);
    auto points = make_unique<PointCollection>(new_request);
    return points;
}

std::unique_ptr<LineCollection> query_lines_source(BinaryStream &stream, int childidx, const QueryRectangle &rect) {
    Profiler::Profiler{"requesting Lines"};
    Log::debug("requesting lines %d with rect (%f,%f -> %f,%f)", childidx, rect.x1, rect.y1, rect.x2, rect.y2);

    BinaryWriteBuffer response;
    response.write<const char &>(RSERVER_TYPE_LINES);
    response.write<int &>(childidx);
    response.write<const QueryRectangle &>(rect);

    is_sending = true;
    stream.write(response);
    is_sending = false;

    BinaryReadBuffer new_request;
    stream.read(new_request);

    return make_unique<LineCollection>(new_request);
}

std::unique_ptr<PolygonCollection>
query_polygons_source(BinaryStream &stream, int childidx, const QueryRectangle &rect) {
    Profiler::Profiler{"requesting Polygons"};
    Log::debug("requesting polygons %d with rect (%f,%f -> %f,%f)", childidx, rect.x1, rect.y1, rect.x2, rect.y2);

    BinaryWriteBuffer response;
    response.write<const char &>(RSERVER_TYPE_POLYGONS);
    response.write<int &>(childidx);
    response.write<const QueryRectangle &>(rect);

    is_sending = true;
    stream.write(response);
    is_sending = false;

    BinaryReadBuffer new_request;
    stream.read(new_request);

    return make_unique<PolygonCollection>(new_request);
}


static std::string read_file_as_string(const std::string &filename) {
    std::ifstream in(filename, std::ios::in | std::ios::binary);
    if (in) {
        std::string contents;
        in.seekg(0, std::ios::end);
        contents.resize(static_cast<unsigned long>(in.tellg()));
        in.seekg(0, std::ios::beg);
        in.read(&contents[0], contents.size());
        in.close();
        return contents;
    }
    throw PlatformException("Could not read plot file");
}


void signal_handler(int signum) {
    Log::error("Caught signal %d, exiting", signum);
    exit(signum);
}


class RServerConnection : public NonblockingServer::Connection {
    public:
        RServerConnection(NonblockingServer &server, int fd, int id);

        ~RServerConnection() override;

    private:
        void processData(std::unique_ptr<BinaryReadBuffer> request) override;

        auto processDataForked(BinaryStream stream) -> void override;

        auto processDataAsync(BinaryStream stream) -> void override;

        std::string source;
        char expected_result;
        int rastersourcecount;
        int pointssourcecount;
        int linessourcecount;
        int polygonssourcecount;
        QueryRectangle qrect;

        size_t plot_width;
        size_t plot_height;
};

class RServer : public NonblockingServer {
    public:
        RServer(RInside *R, RInsideCallbacks *callbacks) : NonblockingServer(),
                                                           R(R), callbacks(callbacks) {
        }

        ~RServer() override = default;

    private:
        std::unique_ptr<Connection> createConnection(int fd, int id) override;

        RInside *R;
        RInsideCallbacks *callbacks;

        friend class RServerConnection;
};


RServerConnection::RServerConnection(NonblockingServer &server, int fd, int id) : Connection(server, fd, id),
                                                                                  source(""), expected_result(-1),
                                                                                  rastersourcecount(-1),
                                                                                  pointssourcecount(-1),
                                                                                  linessourcecount(-1),
                                                                                  polygonssourcecount(-1),
                                                                                  plot_width(0),
                                                                                  plot_height(0),
                                                                                  qrect(SpatialReference::unreferenced(),
                                                                                        TemporalReference::unreferenced(),
                                                                                        QueryResolution::none()) {
    Log::info("%d: connected", id);
}

RServerConnection::~RServerConnection() = default;

void RServerConnection::processData(std::unique_ptr<BinaryReadBuffer> request) {
    auto magic = request->read<int>();
    if (magic != RSERVER_MAGIC_NUMBER)
        throw PlatformException("Client sent the wrong magic number");
    expected_result = request->read<char>();
    Log::info("Requested type: %d", expected_result);
    request->read(&source);
    rastersourcecount = request->read<int>();
    pointssourcecount = request->read<int>();
    linessourcecount = request->read<int>();
    polygonssourcecount = request->read<int>();
    Log::info("Requested counts: %d %d %d %d", rastersourcecount, pointssourcecount, linessourcecount,
              polygonssourcecount);
    qrect = QueryRectangle(*request);
    Log::info("rectangle is rect (%f,%f -> %f,%f)", qrect.x1, qrect.y1, qrect.x2, qrect.y2);

    auto timeout = request->read<int>();

    if (expected_result == RSERVER_TYPE_PLOT) {
        plot_width = request->read<size_t>();
        plot_height = request->read<size_t>();
    }

#if RSERVER_FORKED_MODE
    forkAndProcess(timeout);
#else
    enqueueForAsyncProcessing();
#endif
}

auto RServerConnection::processDataAsync(BinaryStream stream) -> void {
    processDataForked(std::move(stream));
}


auto RServerConnection::processDataForked(BinaryStream stream) -> void {
    auto &rserver = (RServer &) server;
    RInside &R = *(rserver.R);

    Log::info("Here's our client!");

    if (expected_result == RSERVER_TYPE_PLOT) {
        R.parseEval(R"(rserver_plot_tempfile = tempfile("rs_plot", fileext=".png"))");
        R.parseEval(concat("png(rserver_plot_tempfile, width=", plot_width, ", height=", plot_height,
                           ", bg=\"transparent\")"));
        fprintf(stderr, "width: %zu, height: %zu\n", plot_width, plot_height);
    }

    R["mapping.rastercount"] = rastersourcecount;
    std::function<std::unique_ptr<GenericRaster>(int, const QueryRectangle &)> bound_raster_source = [&stream](
            int childidx, const QueryRectangle &rect) -> std::unique_ptr<GenericRaster> {
        return query_raster_source(stream, childidx, rect);
    };
    R["mapping.loadRaster"] = Rcpp::InternalFunction(bound_raster_source);

    std::function<Rcpp::NumericVector(int, const QueryRectangle &)> bound_raster_source_as_array = [&stream](
            int childidx, const QueryRectangle &rect) -> Rcpp::NumericVector {
        return query_raster_source_as_array(stream, childidx, rect);
    };
    R["mapping.loadRasterAsVector"] = Rcpp::InternalFunction(bound_raster_source_as_array);

    std::function<std::unique_ptr<PointCollection>(int, const QueryRectangle &)> bound_points_source = [&stream](
            int childidx, const QueryRectangle &rect) -> std::unique_ptr<PointCollection> {
        return query_points_source(stream, childidx, rect);
    };
    R["mapping.pointscount"] = pointssourcecount;
    R["mapping.loadPoints"] = Rcpp::InternalFunction(bound_points_source);

    std::function<std::unique_ptr<LineCollection>(int, const QueryRectangle &)> bound_lines_source = [&stream](
            int childidx, const QueryRectangle &rect) -> std::unique_ptr<LineCollection> {
        return query_lines_source(stream, childidx, rect);
    };
    R["mapping.linessourcecount"] = linessourcecount;
    R["mapping.loadLines"] = Rcpp::InternalFunction(bound_lines_source);

    std::function<std::unique_ptr<PolygonCollection>(int, const QueryRectangle &)> bound_polygons_source = [&stream](
            int childidx, const QueryRectangle &rect) -> std::unique_ptr<PolygonCollection> {
        return query_polygons_source(stream, childidx, rect);
    };
    R["mapping.polygonssourcecount"] = polygonssourcecount;
    R["mapping.loadPolygons"] = Rcpp::InternalFunction(bound_polygons_source);

    R["mapping.qrect"] = qrect;

    Profiler::start("running R script");
    try {
        std::string delimiter = "\n\n";
        size_t start = 0;
        size_t end = 0;
        while (true) {
            end = source.find(delimiter, start);
            if (end == std::string::npos)
                break;
            std::string line = source.substr(start, end - start);
            start = end + delimiter.length();
            Log::debug("src: %s", line.c_str());
            R.parseEval(line);
        }
        std::string lastline = source.substr(start);
        Log::info("src: %s", lastline.c_str());
        auto result = R.parseEval(lastline);
        Profiler::stop("running R script");

        BinaryWriteBuffer response;
        // types for keeping objects alive
        std::unique_ptr<SpatioTemporalResult> spatio_temporal_result;
        std::string string_result;
        switch (expected_result) {
            case RSERVER_TYPE_RASTER: {
                auto raster = Rcpp::as<std::unique_ptr<GenericRaster>>(result);
                response.write<char>(-RSERVER_TYPE_RASTER);
                response.write<GenericRaster &>(*raster, true);
                spatio_temporal_result = std::move(raster);
                break;
            }

            case RSERVER_TYPE_POINTS: {
                auto points = Rcpp::as<std::unique_ptr<PointCollection>>(result);
                response.write<char>(-RSERVER_TYPE_POINTS);
                response.write<PointCollection &>(*points, true);
                spatio_temporal_result = std::move(points);
            }
                break;

            case RSERVER_TYPE_LINES: {
                // TODO: implement
                throw PlatformException("Requesting lines from R is not yet supported");
            }

            case RSERVER_TYPE_POLYGONS: {
                // TODO: implement
                throw PlatformException("Requesting polygons from R is not yet supported");
            }

            case RSERVER_TYPE_STRING: {
                std::string output = rserver.callbacks->getConsoleOutput();
                response.write<char>(-RSERVER_TYPE_STRING);
                response.write<std::string &>(output, true);
                string_result = std::move(output);
                break;
            }

            case RSERVER_TYPE_PLOT: {
                R.parseEval("dev.off()");
                auto filename = Rcpp::as<std::string>(R["rserver_plot_tempfile"]);
                std::string output = read_file_as_string(filename);
                std::remove(filename.c_str());
                response.write<char>(-RSERVER_TYPE_PLOT);
                response.write<std::string &>(output, true);
                string_result = std::move(output);
                break;
            }

            default:
                throw PlatformException("Unknown result type requested");
        }

        is_sending = true;
        stream.write(response);
        is_sending = false;
    }
    catch (const NetworkException &e) {
        // do not do anything
        throw;
    }
    catch (const std::exception &e) {
        // We are already in the middle of sending something to the client.
        // We cannot send more data in the middle of a packet.
        if (is_sending) {
            throw;
        }

        auto what = e.what();
        Log::warn("Exception: %s", what);
        std::string msg(what);
        BinaryWriteBuffer response;
        response.write<char>(-RSERVER_TYPE_ERROR);
        response.write<std::string &>(msg);
        stream.write(response);
        return;
    }
}


std::unique_ptr<NonblockingServer::Connection> RServer::createConnection(int fd, int id) {
    return make_unique<RServerConnection>(*this, fd, id);
}


int main() {
    Configuration::loadFromDefaultPaths();

    auto portnr = Configuration::get<int>("rserver.port");

    Log::logToStream(Configuration::get<std::string>("rserver.loglevel", "info"), &std::cerr);

    // Signal handlers
    int signals[] = {SIGHUP, SIGINT, 0};
    for (int i = 0; signals[i]; i++) {
        if (signal(signals[i], signal_handler) == SIG_ERR) {
            Log::error("Cannot install signal handler: %s", strerror(errno));
            exit(1);
        } else
            Log::debug("Signal handler for %d installed", signals[i]);
    }
    signal(SIGPIPE, SIG_IGN);

    // Initialize R environment
    auto *Rcallbacks = new RInsideCallbacks();
    Log::info("...loading R");
    RInside R;
    R.set_callbacks(Rcallbacks);

    Log::info("...loading packages");

    std::vector<std::string> packages = Configuration::getVector<std::string>("rserver.packages");
    for (auto &package : packages) {
        Log::debug("Loading package '%s'", package.c_str());
        std::string command = "library(\"" + package + "\")";
        try {
            R.parseEvalQ(command);
        } catch (const std::exception &e) {
            Log::error("error loading package: %s", e.what());
            Log::error("R's output:\n", Rcallbacks->getConsoleOutput().c_str());
            exit(5);
        }
    }

    Rcallbacks->resetConsoleOutput();

    Log::info("R is ready, starting server..");

    RServer server(&R, Rcallbacks);
    // server.listen(rserver_socket, 0777);
    server.listen(portnr);
#if RSERVER_FORKED_MODE
    server.setWorkerThreads(0);
    server.allowForking();
#else
    server.setWorkerThreads(1);
#endif
    server.start();

    return 0;
}
