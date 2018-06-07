# Mapping Dependencies Image
FROM umrdbs/mapping-r-server-dependencies:latest

# Setup directories
WORKDIR /app

# Get mapping-core
ARG MAPPING_CORE_VERSION=master

COPY . /app/mapping-r-server
RUN git clone --depth 1 --branch $MAPPING_CORE_VERSION https://github.com/umr-dbs/mapping-core.git

# Build Server and create symbolic link for logs
RUN cd mapping-r-server && \
    cmake -DCMAKE_BUILD_TYPE=Release . && \
    make -j$(cat /proc/cpuinfo | grep processor | wc -l) && \
    ln -sfT /dev/stderr /var/log/mapping-r-server.log

# Create Service and Copy Config File and Create Symbolic Link
COPY docker-files/mapping-r-server-service.sh /etc/service/mapping-r-server/run
COPY docker-files/settings.toml /etc/mapping.conf
#COPY docker-files/settings.toml  /app/conf/settings.toml
RUN chmod +x /etc/service/mapping-r-server/run && \
    mkdir /app/conf/ && \
    cp /app/mapping-r-server/target/bin/conf/settings-default.toml /app/conf/settings-default.toml && \
    chown -R www-data:www-data /app/conf/ && \
    chown www-data:www-data /etc/mapping.conf

# Update System, Clean up Scripts and APT when done.
RUN apt-get update && \
    apt-get upgrade -y -o Dpkg::Options::="--force-confold" && \
    apt-get clean && \
    rm -rf /var/lib/apt/lists/* /tmp/* /var/tmp/*

# Make port 10200 available to the world outside this container
EXPOSE 10200

# Link to Config File
#VOLUME /app/conf/settings.toml

# Use baseimage-docker's init system.
CMD ["/sbin/my_init"]
