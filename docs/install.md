# Install MAPPING
You need to install the MAPPING dependencies.

## Install R/Rcpp
sudo apt install r-cran-rcpp

### Install RInside with CALLBacks
```
cd ~/Downloads
wget https://cran.r-project.org/src/contrib/Archive/RInside/RInside_0.2.13.tar.gz
tar xvf RInside_0.2.13.tar.gz
vim RInside/inst/include/RInsideConfig.h
```
Uncomment `RINSIDE_CALLBACKS`
 
`sudo R CMD INSTALL ~/Downloads/RInside`
 
### R packages
`sudo r install.packages(c("caret", "ggplot2", "randomForest", "raster", "sp"), repos='http://cran.us.r-project.org')`

## mapping.conf 
`operators.r.location=tcp:127.0.0.1:12349`