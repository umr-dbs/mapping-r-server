#!/bin/bash

cd /app
exec /sbin/setuser www-data /app/mapping-r-server/target/bin/r_server >> /var/log/mapping-r-server.log 2>&1

