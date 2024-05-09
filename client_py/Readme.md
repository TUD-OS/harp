# PyTetrisClient

A Python version of the Tetris Client.

## Dependencies

Before installing `pytetrisclient`, you need to install the `protobuf-compiler`.
This is necessary for compiling the `.proto` files included with the client.
On Ubuntu, you can install `protobuf-compiler` using the following command:

```bash
sudo apt-get install protobuf-compiler
```

All Python dependencies will be automatically installed during the package installation process.

## Installation

It is recommended to install pytetrisclient within a virtual environment to
 avoid conflicting with system-wide packages.

You can create a new environment using virtualenv:
```bash
virtualenv -p python3 ~/virtualenvs/pytetris
```
Or, if you use `virtualenv_wrapper`, you can do:
```bash
mkvirtualenv -p python3 pytetris
```
Note that you can adjust the path and the name of the virtual environment according to your needs.

You can activate the previously created environment as follows:
```bash
source ~/virtualenvs/pytetris/bin/activate
# or, if you use 'virtualenv_wrapper'
workon pytetris
```

Then you can install pytetrisclient and its dependencies from the `client_py` directory:
```bash
pip install .
```

If you plan to modify the code base, you should install the package in
development mode:
```bash
pip install -e .
```


