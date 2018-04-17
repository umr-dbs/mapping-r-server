# Configuration

The configuration for this module has to be specified as described in mapping-configuration.md in the mapping-core module.

| Key        | Values           | Default | Description  |
| ------------- |-------------| -----| ----- |
| rserver.port | \<integer\> || The port for the rserver to listen |
| rserver.loglevel | off \| error \| warn \| info \| debug \| trace | info | The log level for the rserver |
| rserver.packages | \<string\>,\<string\>,...|| The R packages that are loaded when starting the rserver. |
