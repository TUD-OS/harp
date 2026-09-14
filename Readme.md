# HARP: Heterogeneity-aware Adaptive Resource Partitioning

**Note**: HARP was previously named TETRiS. Some code, binaries, and configuration variables still use the old TETRiS naming.

## Build

Use the following commands to build TETRiS

```bash
mkdir build && cd build
cmake ..
make
```


## Run

To be able to manage an application with TETRiS two steps are necessary.
First of all you need to start the TETRiS-server and give it the path
to the mappings for each program. The server executable is located in the
'bin'-directory.

After the server started up, it is now possible to run other applications and
let the TETRiS server manage them. To achieve this, one has to add the
TETRiS-client library to the program. This can be achieved by preloading the
library with the LD_PRELOAD primitive. The library is located in the 
'lib'-directory.

## Settings

### Server

See the help message of the server for information about how the server can be
tweaked.

In addition the server also parses the following environment variables:

#### TETRIS_LOGLEVEL

With this environment variable you can control how much information the TETRiS
server outputs. Possible values are DEBUG, INFO, WARNING and ERROR.


### Client

The client reacts to multiple environment variables:

#### TETRIS_LOGLEVEL

With this environment variable you can control how much information the TETRiS
client library outputs. Possible values are DEBUG, INFO, WARNING and ERROR.

#### TETRIS_PLATFORM (required)

The path to the platform file which the TETRiS library should use. This has to be
the same as the one provided to the TETRiS-server.

#### TETRIS_MAPPING (required)

The path to the mapping file for the application. This can also be an empty file.
In this case, TETRiS will automatically generate the mapping database by itself.

#### TETRIS_LIBS (required)

Base directory of the scaler libraries that TETRiS uses to auto-scale various applications.

#### TETRIS_IGNORE

Used for overhead measurements. When this environment variable is set, the TETRiS library will
ignore all resource allocation messages from the TETRiS server, thus falling back to Linux' default
resource management.


## Creating your own TETRiS Client

It is also possible to write your custom-tailored TETRiS client for one particular application. In this
case, one need to link the application against the libtetris library, instantiate an instance of the 
`tetris::ConcreteClient` class and supply all the necessary features (subclasses of `tetris::Feature` or
`tetris::MappingFeature`) by binding them to the client instance. Registered features need to react to
resource allocation messages and can adjust the application accordingly or even add additional characteristics
to the mapping information.
