/*
 * This file is part of mapping-r-server (https://github.com/umr-dbs/mapping-r-server).
 * Copyright (c) 2018 Database Research Group of the University of Marburg.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, version 3.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
*/

#pragma once

#include <Rcpp.h>

#include "datatypes/raster.h"
#include "datatypes/raster/raster_priv.h"
#include "datatypes/pointcollection.h"
#include "datatypes/linecollection.h"
#include "datatypes/polygoncollection.h"

namespace Rcpp {

    /**
     * Helper function that generate an R `CRS` type out of an CrsId.
     * @param crsId
     * @return `CRS` type of R
     */
    auto create_crs(const CrsId &crsId) -> Rcpp::S4 {
        Rcpp::S4 crs("CRS");
        crs.slot("projargs") = crsId.to_string();

        return crs;
    }

    auto create_bbox(const SimpleFeatureCollection &collection) -> Rcpp::NumericMatrix {
        auto minimum_bounding_rectangle = collection.getCollectionMBR();

        Rcpp::NumericMatrix bbox{2, 2};
        bbox(0, 0) = minimum_bounding_rectangle.x1;
        bbox(0, 1) = minimum_bounding_rectangle.x2;
        bbox(1, 0) = minimum_bounding_rectangle.y1;
        bbox(1, 1) = minimum_bounding_rectangle.y2;

        return bbox;
    }

    /**
     * Helper function that generate an R `DataFrame` out of the attributes of a feature collection.
     * @param collection
     * @return `DataFrame` of R with attributes
     */
    auto create_attribute_data_frame(const SimpleFeatureCollection &collection) -> Rcpp::DataFrame {
        const size_t size = collection.getFeatureCount();

        Rcpp::DataFrame data;

        auto numeric_keys = collection.feature_attributes.getNumericKeys();
        for (auto &key : numeric_keys) {
            Rcpp::NumericVector vec(size);
            for (size_t i = 0; i < size; i++) {
                double value = collection.feature_attributes.numeric(key).get(i);
                vec[i] = value;
            }
            data[key] = vec;
        }

        auto string_keys = collection.feature_attributes.getTextualKeys();
        for (const auto &key : string_keys) {
            Rcpp::StringVector vec(size);
            for (size_t i = 0; i < size; i++) {
                auto &value = collection.feature_attributes.textual(key).get(i);
                vec[i] = value;
            }
            data[key] = vec;
        }

        // append temporal information
        if (collection.hasTime()) {
            // TODO: find a way to rely on names
            std::string time_start_key = "time_start";
            while (std::find(std::begin(numeric_keys), std::end(numeric_keys), time_start_key) !=
                   std::end(numeric_keys) ||
                   std::find(std::begin(string_keys), std::end(string_keys), time_start_key) != std::end(string_keys)) {
                // append underscore until unique
                time_start_key.push_back('_');
            }
            std::string time_end_key = "time_end";
            while (std::find(std::begin(numeric_keys), std::end(numeric_keys), time_end_key) !=
                   std::end(numeric_keys) ||
                   std::find(std::begin(string_keys), std::end(string_keys), time_end_key) != std::end(string_keys)) {
                // append underscore until unique
                time_end_key.push_back('_');
            }

            Rcpp::NumericVector time_start(size);
            Rcpp::NumericVector time_end(size);
            for (size_t i = 0; i < size; i++) {
                const auto &time_interval = collection.time[i];
                time_start[i] = time_interval.t1;
                time_end[i] = time_interval.t2;
            }
            data[time_start_key] = time_start;
            data[time_end_key] = time_end;
        }

        return data;
    }

    /**
     * Convert QueryRectangle to R list
     */
    template<>
    SEXP wrap(const QueryRectangle &rect) {
        //Profiler::Profiler p("Rcpp: wrapping qrect");
        Rcpp::List list;

        list["t1"] = rect.t1;
        list["t2"] = rect.t2;
        list["x1"] = rect.x1;
        list["y1"] = rect.y1;
        list["x2"] = rect.x2;
        list["y2"] = rect.y2;
        if (rect.restype == QueryResolution::Type::PIXELS) {
            list["xres"] = rect.xres;
            list["yres"] = rect.xres;
        } else if (rect.restype == QueryResolution::Type::NONE) {
            list["xres"] = 0;
            list["yres"] = 0;
        } else
            throw ArgumentException("Rcpp::wrap(): cannot convert a QueryRectangle with unknown resolution type");
        list["crs"] = rect.crsId.to_string();

        return Rcpp::wrap(list);
    }

    /**
     * Convert R list to QueryRectangle
     * @param sexp
     * @return QueryRectangle
     */
    template<>
    QueryRectangle as(SEXP sexp) {
        //Profiler::Profiler p("Rcpp: unwrapping qrect");
        auto list = Rcpp::as<Rcpp::List>(sexp);

        auto xres = static_cast<uint32_t>((int) list["xres"]);
        auto yres = static_cast<uint32_t>((int) list["yres"]);

        return QueryRectangle(
                SpatialReference(CrsId::from_srs_string(list["crs"]), list["x1"], list["y1"], list["x2"], list["y2"]),
                TemporalReference(TIMETYPE_UNIX, list["t1"], list["t2"]),
                (xres > 0 && yres > 0) ? QueryResolution::pixels(xres, yres) : QueryResolution::none()
        );
    }

    /**
     * Convert GenericRaster to R RasterLayer
     * @param raster
     * @return RasterLayer
     */
    template<>
    SEXP wrap(const GenericRaster &raster) {
        /*
        class	   : RasterLayer
        dimensions  : 180, 360, 64800  (nrow, ncol, ncell)
        resolution  : 1, 1  (x, y)
        extent	  : -180, 180, -90, 90  (xmin, xmax, ymin, ymax)
        coord. ref. : +proj=longlat +datum=WGS84

        attributes(r)
        $history: list()
        $file: class .RasterFile
        $data: class .SingleLayerData  (hat haveminmax, min, max!)
        $legend: class .RasterLegend
        $title: character(0)
        $extent: class Extent (xmin, xmax, ymin, ymax)
        $rotated: FALSE
        $rotation: class .Rotation
        $ncols: int
        $nrows: int
        $crs: CRS arguments: +proj=longlat +datum=WGS84
        $z: list()
        $class: c("RasterLayer", "raster")

         */
        Profiler::Profiler {"Rcpp: wrapping raster"};
        int width = raster.width;
        int height = raster.height;

        Rcpp::NumericVector pixels(raster.getPixelCount());
        int pos = 0;
        for (int y = 0; y < height; y++) {
            for (int x = 0; x < width; x++) {
                double val = raster.getAsDouble(x, y);
                if (raster.dd.is_no_data(val))
                    pixels[pos++] = NAN;
                else
                    pixels[pos++] = val;
            }
        }

        Rcpp::S4 data(".SingleLayerData");
        data.slot("values") = pixels;
        data.slot("inmemory") = true;
        data.slot("fromdisk") = false;
        data.slot("haveminmax") = true;
        data.slot("min") = raster.dd.unit.getMin();
        data.slot("max") = raster.dd.unit.getMax();

        // TODO: how exactly would R like the Extent to be?
        Rcpp::S4 extent("Extent");
        extent.slot("xmin") = raster.stref.x1;
        extent.slot("ymin") = raster.stref.y1;
        extent.slot("xmax") = raster.stref.x2;
        extent.slot("ymax") = raster.stref.y2;

        Rcpp::S4 crs("CRS");
        crs.slot("projargs") = raster.stref.crsId.to_string();

        Rcpp::S4 rasterlayer("RasterLayer");
        rasterlayer.slot("data") = data;
        rasterlayer.slot("extent") = extent;
        rasterlayer.slot("crs") = crs;
        rasterlayer.slot("ncols") = raster.width;
        rasterlayer.slot("nrows") = raster.height;

        return Rcpp::wrap(rasterlayer);
    }

    /**
     * Convert GenericRaster to R RasterLayer
     * @param raster
     * @return RasterLayer
     */
    template<>
    SEXP wrap(const std::unique_ptr<GenericRaster> &raster) {
        return Rcpp::wrap(*raster);
    }

    template<>
    std::unique_ptr<GenericRaster> as(SEXP sexp) {
        Profiler::Profiler {"Rcpp: unwrapping raster"};

        Rcpp::S4 rasterlayer(sexp);
        if (!rasterlayer.is("RasterLayer"))
            throw OperatorException("Result is not a RasterLayer");

        int width = rasterlayer.slot("ncols");
        int height = rasterlayer.slot("nrows");

        Rcpp::S4 crs = rasterlayer.slot("crs");
        std::string crs_string = crs.slot("projargs");
        CrsId crsId = CrsId::from_srs_string(crs_string);

        Rcpp::S4 extent = rasterlayer.slot("extent");
        double xmin = extent.slot("xmin"), ymin = extent.slot("ymin"), xmax = extent.slot("xmax"), ymax = extent.slot(
                "ymax");

        SpatioTemporalReference stref(
                SpatialReference(crsId, xmin, ymin, xmax, ymax),
                TemporalReference::unreferenced()
        );

        Rcpp::S4 data = rasterlayer.slot("data");
        if (!(bool) data.slot("inmemory"))
            throw OperatorException("Result raster not inmemory");
        if (!(bool) data.slot("haveminmax"))
            throw OperatorException("Result raster does not have min/max");

        double min = data.slot("min");
        double max = data.slot("max");

        Unit u = Unit::unknown();
        u.setMinMax(min, max);
        DataDescription dd(GDT_Float32, u);
        dd.addNoData();
        dd.verify();
        auto raster_out = GenericRaster::create(dd, stref, static_cast<uint32_t>(width), static_cast<uint32_t>(height),
                                                GenericRaster::Representation::CPU);
        auto &raster2d = dynamic_cast<Raster2D<float> &>(*raster_out);

        Rcpp::NumericVector pixels = data.slot("values");
        int pos = 0;
        for (int y = 0; y < height; y++) {
            for (int x = 0; x < width; x++) {
                float val = pixels[pos++];
                raster2d.set(x, y, val);
            }
        }
        return raster_out;
    }

    // PointCollection
    template<>
    SEXP wrap(const PointCollection &points) {
        /*
        new("SpatialPointsDataFrame"
            , data = structure(list(), .Names = character(0), row.names = integer(0), class = "data.frame")
            , coords.nrs = numeric(0)
            , coords = structure(NA, .Dim = c(1L, 1L))
            , bbox = structure(NA, .Dim = c(1L, 1L))
            , proj4string = new("CRS"
            , projargs = NA_character_
        )
        */
        Profiler::Profiler {"Rcpp: wrapping pointcollection"};

        Rcpp::S4 SPDF{"SpatialPointsDataFrame"};

        size_t size = points.coordinates.size();

        Rcpp::DataFrame data = create_attribute_data_frame(points);

        Rcpp::NumericMatrix coords(static_cast<const int &>(size), 2);
        for (decltype(size) i = 0; i < size; i++) {
            const Coordinate &p = points.coordinates[i];
            coords(i, 0) = p.x;
            coords(i, 1) = p.y;
        }

        Rcpp::NumericMatrix bbox = create_bbox(points);

        Rcpp::S4 crs("CRS");
        crs.slot("projargs") = points.stref.crsId.to_string();


        SPDF.slot("data") = data;
        SPDF.slot("coords.nrs") = true;
        SPDF.slot("coords") = coords;
        SPDF.slot("bbox") = bbox;
        SPDF.slot("proj4string") = crs;

        return SPDF;
    }

    template<>
    SEXP wrap(const std::unique_ptr<PointCollection> &points) {
        return Rcpp::wrap(*points);
    }

    template<>
    std::unique_ptr<PointCollection> as(SEXP sexp) {
        Profiler::Profiler {"Rcpp: unwrapping pointcollection"};

        Rcpp::S4 SPDF(sexp);
        if (!SPDF.is("SpatialPointsDataFrame"))
            throw OperatorException("Result is not a SpatialPointsDataFrame");

        auto nrs = Rcpp::as<bool>(SPDF.slot("coords.nrs"));
        if (!nrs)
            throw OperatorException("Result has nrs = false, cannot convert");

        Rcpp::S4 crs = SPDF.slot("proj4string");
        std::string crs_string = crs.slot("projargs");

        CrsId crsId = CrsId::from_srs_string(crs_string);

        auto points = std::make_unique<PointCollection>(SpatioTemporalReference(crsId, TIMETYPE_UNIX));

        auto coords = Rcpp::as<Rcpp::NumericMatrix>(SPDF.slot("coords"));

        auto size = static_cast<size_t>(coords.nrow());
        points->coordinates.reserve(size);
        for (size_t i = 0; i < size; i++) {
            double x = coords(i, 0);
            double y = coords(i, 1);
            points->addSinglePointFeature(Coordinate(x, y));
        }

        auto data = Rcpp::as<Rcpp::DataFrame>(SPDF.slot("data"));

        Rcpp::Function attributes("attributes");
        Rcpp::List attrs = attributes(data);

        Rcpp::StringVector a = attrs["names"];
        auto len = a.length();
        for (int i = 0; i < len; i++) {
            auto attr = Rcpp::as<std::string>(a[i]);
            try {
                Rcpp::NumericVector rvec = data[attr];
                auto &vec = points->feature_attributes.addNumericAttribute(attr, Unit::unknown());
                vec.reserve(size);
                for (size_t j = 0; j < size; j++)
                    vec.set(j, (double) rvec[j]);
            }
            catch (const Rcpp::not_compatible &e) {
                Rcpp::StringVector rvec = data[attr];
                auto &vec = points->feature_attributes.addTextualAttribute(attr, Unit::unknown());
                vec.reserve(size);
                for (size_t j = 0; j < size; j++)
                    vec.set(j, (std::string) rvec[j]);
            }
        }

        // TODO: convert time vector

        return points;
    }


    auto create_polygon(const PolygonCollection::PolygonRingReference<const PolygonCollection> &ring,
                        bool is_hole) -> Rcpp::S4 {
        const auto number_of_rows = static_cast<const int>(ring.size());
        Rcpp::NumericMatrix r_coordinates{number_of_rows, 2};

        int index = 0;
        for (const Coordinate &coordinate : ring) {
            r_coordinates(index, 0) = coordinate.x;
            r_coordinates(index, 1) = coordinate.y;
            ++index;
        }

        Rcpp::S4 r_polygon("Polygon");
        r_polygon.slot("coords") = r_coordinates;
        r_polygon.slot("hole") = is_hole;

        return r_polygon;
    }

    auto
    create_polygons(const PolygonCollection::PolygonPolygonReference<const PolygonCollection> &polygon) -> Rcpp::S4 {
        Rcpp::S4 r_polygons("Polygons");
        Rcpp::List r_polygon_list;

        bool is_hole = false; // all but the first one are holes
        for (const auto &ring : polygon) {
            r_polygon_list.push_back(create_polygon(ring, is_hole));

            is_hole = true;
        }

        r_polygons.slot("Polygons") = r_polygon_list;

        return r_polygons;
    }

    auto create_spatial_polygons(
            const PolygonCollection::PolygonFeatureReference<const PolygonCollection> &feature) -> Rcpp::S4 {
        Rcpp::S4 r_spatial_polygons("SpatialPolygons");
        Rcpp::List r_polygons_list;

        for (const auto &polygon : feature) {
            r_polygons_list.push_back(create_polygons(polygon));
        }

        r_spatial_polygons.slot("polygons") = r_polygons_list;

        return r_spatial_polygons;
    }

    /**
     * Convert a PolygonCollection to a SpatialPolygonsDataFrame
     * @param polygonCollection
     * @return A SpatialPolygonsDataFrame
     */
    template<>
    SEXP wrap(const PolygonCollection &polygonCollection) {
        Profiler::Profiler {"Rcpp: wrapping PolygonCollection"};

        Rcpp::List r_spatial_polygons_list;
        for (const auto &feature : polygonCollection) {
            r_spatial_polygons_list.push_back(create_spatial_polygons(feature));
        }

        Rcpp::S4 r_spatial_polygons_data_frame("SpatialPolygonsDataFrame");
        r_spatial_polygons_data_frame.slot("data") = create_attribute_data_frame(polygonCollection);
        r_spatial_polygons_data_frame.slot("polygons") = r_spatial_polygons_list;
        r_spatial_polygons_data_frame.slot("bbox") = create_bbox(polygonCollection);
        r_spatial_polygons_data_frame.slot("proj4string") = create_crs(polygonCollection.stref.crsId);

        return r_spatial_polygons_data_frame;
    }

    /**
     * Convert a PolygonCollection to a SpatialPolygonsDataFrame
     * @param polygonCollection
     * @return A SpatialPolygonsDataFrame
     */
    template<>
    SEXP wrap(const std::unique_ptr<PolygonCollection> &polygonCollection) {
        return Rcpp::wrap(*polygonCollection);
    }

    auto create_line(const LineCollection::LineLineReference<const LineCollection> &line) -> Rcpp::S4 {
        const auto number_of_rows = static_cast<const int>(line.size());
        Rcpp::NumericMatrix r_coordinates{number_of_rows, 2};

        int index = 0;
        for (const Coordinate &coordinate : line) {
            r_coordinates(index, 0) = coordinate.x;
            r_coordinates(index, 1) = coordinate.y;
            ++index;
        }

        Rcpp::S4 r_line("Line");
        r_line.slot("coords") = r_coordinates;

        return r_line;
    }

    auto create_lines(const LineCollection::LineLineReference<const LineCollection> &line) -> Rcpp::S4 {
        Rcpp::S4 r_lines("Lines");
        Rcpp::List r_lines_list;

        r_lines_list.push_back(create_line(line));

        r_lines.slot("Lines") = r_lines_list;

        return r_lines;
    }

    auto create_spatial_lines(const LineCollection::LineFeatureReference<const LineCollection> &feature) -> Rcpp::S4 {
        Rcpp::S4 r_spatial_lines("SpatialLines");
        Rcpp::List r_lines_list;

        for (const auto &line : feature) {
            r_lines_list.push_back(create_lines(line));
        }

        r_spatial_lines.slot("lines") = r_lines_list;

        return r_spatial_lines;
    }

    /**
     * Convert a LineCollection to a SpatialLinesDataFrame
     * @param lineCollection
     * @return A SpatialLinesDataFrame
     */
    template<>
    SEXP wrap(const LineCollection &lineCollection) {
        Profiler::Profiler {"Rcpp: wrapping LineCollection"};

        Rcpp::List r_spatial_lines_list;
        for (const auto &feature : lineCollection) {
            r_spatial_lines_list.push_back(create_spatial_lines(feature));
        }

        Rcpp::S4 r_spatial_lines_data_frame("SpatialLinesDataFrame");
        r_spatial_lines_data_frame.slot("data") = create_attribute_data_frame(lineCollection);
        r_spatial_lines_data_frame.slot("lines") = r_spatial_lines_list;
        r_spatial_lines_data_frame.slot("bbox") = create_bbox(lineCollection);
        r_spatial_lines_data_frame.slot("proj4string") = create_crs(lineCollection.stref.crsId);

        return r_spatial_lines_data_frame;
    }

    /**
     * Convert a LineCollection to a SpatialLinesDataFrame
     * @param lineCollection
     * @return A SpatialLinesDataFrame
     */
    template<>
    SEXP wrap(const std::unique_ptr<LineCollection> &lineCollection) {
        return Rcpp::wrap(*lineCollection);
    }

}
