### 1. STAGE (BUILD)
# Ubuntu 16.04 LTS with Baseimage and Runit
FROM phusion/baseimage:0.10.1 AS builder

# mapping core git branch
ARG MAPPING_CORE_VERSION=master

WORKDIR /app

# copy files
COPY cmake /app/mapping-r-server/cmake
COPY conf /app/mapping-r-server/conf
COPY docker-files /app/mapping-r-server/docker-files
COPY src /app/mapping-r-server/src
COPY CMakeLists.txt /app/mapping-r-server/

# set terminal to noninteractive
ARG DEBIAN_FRONTEND=noninteractive

# update packages and upgrade system
RUN apt-get update && \
    apt-get upgrade --yes -o Dpkg::Options::="--force-confold"

# install git and grab mapping-core
RUN apt-get install --yes git && \
    git clone --depth 1 --branch $MAPPING_CORE_VERSION https://github.com/umr-dbs/mapping-core.git

# install OpenCL
RUN chmod +x mapping-core/docker-files/install-opencl-build.sh && \
    mapping-core/docker-files/install-opencl-build.sh

# install MAPPING dependencies
RUN chmod +x mapping-core/docker-files/ppas.sh && \
    mapping-core/docker-files/ppas.sh && \
    python3 mapping-core/docker-files/read_dependencies.py mapping-core/docker-files/dependencies.csv "build dependencies" \
        | xargs -d '\n' -- apt-get install --yes

# install MAPPING R server dependencies
RUN apt-get install --yes r-cran-rcpp && \
    wget --no-verbose https://cran.r-project.org/src/contrib/Archive/RInside/RInside_0.2.13.tar.gz && \
    tar -xvf RInside_0.2.13.tar.gz && \
    cp mapping-r-server/docker-files/RInsideConfig.h RInside/inst/include/RInsideConfig.h && \
    R CMD INSTALL RInside

# Build MAPPING R server
RUN cd mapping-r-server && \
    cmake -DCMAKE_BUILD_TYPE=Release . && \
    make -j$(cat /proc/cpuinfo | grep processor | wc -l)


### 2. STAGE (RUNTIME)
# Ubuntu 16.04 LTS with Baseimage and Runit
FROM phusion/baseimage:0.10.1

# R packages to install
ARG RPACKAGES='"caret","ggplot2","randomForest","raster","sp"'
ENV RPACKAGES ${RPACKAGES}

WORKDIR /app

COPY --from=builder /app/mapping-r-server/target/bin /app
COPY --from=builder \
    /app/mapping-core/docker-files \
    /app/mapping-r-server/docker-files \
    /app/docker-files/

# set terminal to noninteractive
ARG DEBIAN_FRONTEND=noninteractive

RUN \
    # update packages and upgrade system
    apt-get update && \
    apt-get upgrade --yes -o Dpkg::Options::="--force-confold" && \
    # install OpenCL
    chmod +x docker-files/install-opencl-runtime.sh && \
    docker-files/install-opencl-runtime.sh && \
    # install MAPPING dependencies
    chmod +x docker-files/ppas.sh && \
    docker-files/ppas.sh && \
    python3 docker-files/read_dependencies.py docker-files/dependencies.csv "runtime dependencies" \
        | xargs -d '\n' -- apt-get install --yes && \
    # install R and mapping-r-server dependencies
    apt-get install --yes r-cran-rcpp && \
    wget --no-verbose https://cran.r-project.org/src/contrib/Archive/RInside/RInside_0.2.13.tar.gz && \
    tar -xvf RInside_0.2.13.tar.gz && \
    cp docker-files/RInsideConfig.h RInside/inst/include/RInsideConfig.h && \
    R CMD INSTALL RInside && \
    R -e 'install.packages(c('$RPACKAGES'), repos="http://cran.us.r-project.org", Ncpus='$(cat /proc/cpuinfo | grep processor | wc -l)')' && \
    # Make service available
    mkdir --parents /etc/service/mapping-r-server/ && \
    mv docker-files/mapping-r-server-service.sh /etc/service/mapping-r-server/run && \
    chmod +x /etc/service/mapping-r-server/run && \
    cp docker-files/settings.toml /etc/mapping.conf && \
    ln -sfT /dev/stderr /var/log/mapping-r-server.log && \
    # Clean APT and install scripts
    apt-get clean && \
    rm -rf /var/lib/apt/lists/* /tmp/* /var/tmp/* /app/docker-files

# Make port 10200 available to the world outside this container
EXPOSE 10200

# Link to Config File
# VOLUME /app/conf/settings.toml

# Use baseimage-docker's init system.
CMD ["/sbin/my_init"]
