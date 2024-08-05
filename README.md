# everest-core

This is the main part of EVerest containing the actual charge controller logic included in a large set of modules.

All documentation and the issue tracking can be found in our main repository here: https://github.com/EVerest/everest

### Prerequisites:

#### Hardware recommendations

It is recommended to have at least 4GB of RAM available to build EVerest.
More CPU cores will optionally boost the build process, while requiring more RAM accordingly.

#### Ubuntu 22.04

> :warning: Ubuntu 20.04 is not supported anymore. Please use Ubuntu 22.04 or newer.

```bash
sudo apt update
sudo apt install -y python3-pip git rsync wget cmake doxygen graphviz build-essential clang-tidy cppcheck openjdk-17-jdk npm docker docker-compose libboost-all-dev nodejs libssl-dev libsqlite3-dev clang-format curl rfkill libpcap-dev libevent-dev pkg-config libcap-dev
```

#### OpenSuse
```bash
zypper update && zypper install -y sudo shadow
zypper install -y --type pattern devel_basis
zypper install -y git rsync wget cmake doxygen graphviz clang-tools cppcheck boost-devel libboost_filesystem-devel libboost_log-devel libboost_program_options-devel libboost_system-devel libboost_thread-devel java-17-openjdk java-17-openjdk-devel nodejs nodejs-devel npm python3-pip gcc-c++ libopenssl-devel sqlite3-devel libpcap-devel libevent-devel libcap-devel
```

#### Fedora 38, 39 & 40
```bash
sudo dnf update
sudo dnf install make automake gcc gcc-c++ kernel-devel python3-pip python3-devel git rsync wget cmake doxygen graphviz clang-tools-extra cppcheck java-17-openjdk java-17-openjdk-devel boost-devel nodejs nodejs-devel npm openssl openssl-devel libsqlite3x-devel curl rfkill libpcap-devel libevent-devel libcap-devel
```

### Build & Install:

It is required that you have uploaded your public [ssh key](https://www.atlassian.com/git/tutorials/git-ssh) to [github](https://github.com/settings/keys).

To install the [Everest Dependency Manager](https://github.com/EVerest/everest-dev-environment/blob/main/dependency_manager/README.md), follow these steps:

Install required python packages:
```bash
python3 -m pip install --upgrade pip setuptools wheel jstyleson jsonschema
```
Get EDM source files and change into the directory:
```bash
git clone git@github.com:EVerest/everest-dev-environment.git
cd everest-dev-environment/dependency_manager
```
Install EDM:
```bash
python3 -m pip install .
```
We need to add */home/USER/.local/bin* and *CPM_SOURCE_CACHE* to *$PATH*:
```bash
export PATH=$PATH:/home/$(whoami)/.local/bin
export CPM_SOURCE_CACHE=$HOME/.cache/CPM
```

Now setup EVerest workspace: 
```bash
cd everest-dev-environment/dependency_manager
edm init --workspace ~/checkout/everest-workspace
```

This sets up a workspace based on the most recent EVerest release. If you want to check out the most recent main you can use the following command:
```bash
cd everest-dev-environment/dependency_manager
edm init main --workspace ~/checkout/everest-workspace
```

Install [ev-cli](https://github.com/EVerest/everest-utils/tree/main/ev-dev-tools):

Change the directory and install ev-cli:
```bash
cd ~/checkout/everest-workspace/everest-utils/ev-dev-tools
python3 -m pip install .
```

Change the directory and install the required packages for ISO15118 communication:

```bash
cd ~/checkout/everest-workspace/Josev
python3 -m pip install -r requirements.txt
```

For ISO15118 communication including Plug&Charge you need to install the required CA certificates inside [config/certs/ca](config/certs/ca) and client certificates, private keys and password files inside [config/certs/client](config/certs/client/). For an more seamless development experience, these are automatically generated for you, but you can set the ISO15118_2_GENERATE_AND_INSTALL_CERTIFICATES cmake option to OFF to disable this auto-generation for production use.

Now we can build EVerest!

```bash
mkdir -p ~/checkout/everest-workspace/everest-core/build
cd ~/checkout/everest-workspace/everest-core/build
cmake ..
make install
```

(Optional) In case you have more than one CPU core and more RAM availble you can use the following command to significantly speed up the build process:
```bash
make -j$(nproc) install
```
*$(nproc)* puts out the core count of your machine, so it is using all available CPU cores!
You can also specify any number of CPU cores you like.
 
Done!

<!--- WIP: [everest-cpp - Init Script](https://github.com/EVerest/everest-utils/tree/main/everest-cpp) -->

### Simulation

In order to test your build of Everest you can simulate the code on your local machine! Check out the different configuration files to run EVerest and the corresponding nodered flows in the [config folder](config/).

 Check out this [guide for using EVerest SIL](https://everest.github.io/nightly/tutorials/run_sil/index.html)


### Troubleshoot

**1. Problem:** "make install" fails with complaining about missing header files.

**Cause:** Most probably your *clang-format* version is older than 11 and *ev-cli* is not able to generate the header files during the build process.

**Solution:** Install newer clang-format version and make Ubuntu using the new version e.g.:
```bash
sudo apt install clang-format-12
sudo update-alternatives --install /usr/bin/clang-format clang-format /usr/bin/clang-format-12 100
```
Verify clang-format version:
```bash
clang-format --version
Ubuntu clang-format version 12.0.0-3ubuntu1~20.04.4
```
To retry building EVerest delete the entire everest-core/**build** folder and recreate it. 
Start building EVerest using *cmake ..* and *make install* again.


**2. Problem:** Build speed is very slow.

**Cause:** *cmake* and *make* are only utilizing one CPU core.

**Solution:** use 
```bash
cmake -j$(nproc) .. 
```
and 
```bash
make -j$(nproc) install
```
to use all available CPU cores.
Be aware that this will need roughly an additional 1-2GB of RAM per core.
Alternatively you can also use any number between 2 and your maximum core count instead of *$(nproc)*.

RDB:
To build both sides (EV and EVSE) of the ACDP implementation, you can do the following:
Tested on a clean Raspberry Pi 5, 4gb RAM

# Install the dev tools you need
sudo apt update
sudo apt install -y python3-pip git rsync wget cmake doxygen graphviz build-essential clang-tidy cppcheck openjdk-17-jdk npm docker docker-compose libboost-all-dev nodejs libssl-dev libsqlite3-dev clang-format curl rfkill libpcap-dev libevent-dev pkg-config libcap-dev

# The UWBPPD uses libserial - install this (should be in dependencies, but isn't yet)
sudo apt install libserial-dev

# Create a checkout directory in home
mkdir checkout
cd checkout

# Install the Everest Dependency Manager
git clone https://github.com/EVerest/everest-dev-environment
cd everest-dev-environment/dependency_manager
# Has to be sudo, otherwise it won't install correctly
sudo python3 -m pip install .
# If you get the dreaded "error: externally-managed-environment", you can do this hack, though apparently not recommended
sudo python3 -m pip install . --break-system-packages

# Create a new folder under checkout, i.e. everest_exi
# cd to that directory
cd everest_exi

# Clone everest-core from the fork in cienporcien/everest-core
git clone https://github.com/cienporcien/everest-core.git

# Clone the forked libiso15118 from cienporcien/libiso15118
git clone https://github.com/cienporcien/libiso15118.git

# Clone the evlibiso15118 from cienporcien/evlibiso15118
git clone https://github.com/cienporcien/evlibiso15118.git

# Select the correct branches for ACDP
cd everest-core
git checkout testing/iso15118-20-ACDP
cd ../libiso15118
git checkout dash-20-poc-acdp
cd ../evlibiso15118
git checkout evlibiso15118_with_ACDP

# From here build. There are two ways, with vscode (using ninja) or continue in a command line (using make)
# From the command line:
# To get debug symbols in the executable for debugging later, add this line:
export CMAKE_BUILD_TYPE=Debug
cd ../everest-core
mkdir build
cd build
cmake ..
# if you have plenty of RAM, make using -j to build faster:
# make -j$(nproc) install
# Note: on a 4gb rpi, using all 4 cores causes Raspbian (Raspberry Pi OS) to crash eventually, so use this instead though it takes much longer:
make install 

# for testing, we need everest-utils
cd ~/checkout
git clone https://github.com/EVerest/everest-utils.git

# start up the docker container for mqtt
# note, you may need to run this first:
# docker network create --driver bridge --ipv6  --subnet fd00::/80 infranet_network --attachablels
cd ~/checkout/everest-utils/docker
sudo docker-compose up -d mqtt-server

# Start up nodered to see the progress of the charging visually
cd ~/checkout/everest_exi_ev/everest-core/build/run-scripts
sudo sh nodered-sil-dc.sh

# In a browser:
http://localhost:1880/ui/

# Start up a new terminal. Run the charger and the ev together.
# It will run automatically, you don't have to click on anything in the browser.
cd ~/checkout/everest_exi/everest-core/build/dist
./bin/manager --config config-dash-20-sil-EV20 --

For debugging js, use the following


{
    // Use IntelliSense to learn about possible attributes.
    // Hover to view descriptions of existing attributes.
    // For more information, visit: https://go.microsoft.com/fwlink/?linkid=830387
    "version": "0.2.0",
    "configurations": [
        {
            "name": "JsEvManager",
            "program": "/home/evcc/checkout/everest_exi_ev/everest-core/build/dist/libexec/everest/modules/JsEvManager/index.js",
            "request": "launch",
            "cwd": "/home/evcc/checkout/everest_exi_ev",
            "skipFiles": [
                "<node_internals>/**"
            ],
            "env":{
                "NODE_PATH": "/home/evcc/checkout/everest_exi_ev/everest-core/build/dist/lib/everest/node_modules/",
                "EV_MODULE": "ev_manager",
                "EV_PREFIX": "/home/evcc/checkout/everest_exi_ev/everest-core/build/dist/",
                "EV_CONF_FILE": "config-sil-EV-ACDP"
            },
            "type": "node"
        },
        {
            "name": "JsSlacSimulator",
            "program": "/home/evcc/checkout/everest_exi_ev/everest-core/build/dist/libexec/everest/modules/JsSlacSimulator/index.js",
            "request": "launch",
            "cwd": "/home/evcc/checkout/everest_exi_ev",
            "skipFiles": [
                "<node_internals>/**"
            ],
            "env":{
                "NODE_PATH": "/home/evcc/checkout/everest_exi_ev/everest-core/build/dist/lib/everest/node_modules/",
                "EV_MODULE": "slac",
                "EV_PREFIX": "/home/evcc/checkout/everest_exi_ev/everest-core/build/dist/",
                "EV_CONF_FILE": "config-sil-EV-ACDP"
            },
            "type": "node"
        },
        {
            "name": "CbexiEV",
            "type": "cppdbg",
            "request": "launch",
            "program": "/home/evcc/checkout/everest_exi_ev/everest-core/build/modules/CbexiEV/CbexiEV",
            "args": [
                "--config",
                "config-sil-EV-ACDP",
                "--module",
                "iso15118_car"
            ],
            "stopAtEntry": false,
            "cwd": "/home/evcc/checkout/everest_exi_ev",
            "environment": [],
            "externalConsole": false,
            "MIMode": "gdb",
            "setupCommands": [
                {
                    "description": "Enable pretty-printing for gdb",
                    "text": "-enable-pretty-printing",
                    "ignoreFailures": true
                },
                {
                    "description": "Set Disassembly Flavor to Intel",
                    "text": "-gdb-set disassembly-flavor intel",
                    "ignoreFailures": true
                },
                { "text": "set output-radix 16" }
            ]
        }
    ]
}