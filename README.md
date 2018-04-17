# MAPPING R Server

## Requirements
 * [mapping-core](https://github.com/umr-dbs/mapping-core)
 * [R](https://cran.r-project.org/web/packages/Rcpp/index.html)
 * [RInside](https://github.com/eddelbuettel/rinside)

## Building
```
cmake .
make
```

### Options
 * Debug Build
   * `-DCMAKE_BUILD_TYPE=Debug`
 * Release Build
   * `-DCMAKE_BUILD_TYPE=Release`
 * Specify the *mapping-core* path
   * it tries to find it automatically, e.g. at the parent directory
   * `-MAPPING_CORE_PATH=<path-to-mapping-core>` 
