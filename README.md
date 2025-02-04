# FIO Chronicle Project
FIO.Chronicle is a software application designed to process the state and trace history of the [FIO](https://github.com/fioprotocol/fio) blockchain and is a fork of the [EOSChronicle](https://github.com/EOSChronicleProject/eos-chronicle) project. For more detailed information regarding FIO.Chronicle and its origin, EOSChronicle, see the [FIO.Chronicle Overview](https://github.com/fioprotocol/fio.chronicle/blob/develop/docs/overview.md).

# FIO Protocol
The Foundation for Interwallet Operability (FIO) or, in short, the FIO Protocol, is an open-source project based on EOSIO 1.8+.

* For information on FIO Protocol, visit [FIO](https://fio.net).
* For information on the FIO Chain, API, and SDKs, including detailed clone, build and deploy instructions, visit [FIO Protocol Developer Hub](https://dev.fio.net).
* To get updates on the development roadmap, visit [FIO Improvement Proposals](https://github.com/fioprotocol/fips). Anyone is welcome and encouraged to contribute.
* To contribute, please review [Contributing to FIO](https://dev.fio.net/docs/contributing-to-fio)
* To join the community, visit [Discord](https://discord.com/invite/pHBmJCc)

## Licenses and Copyrights
[FIO License](https://github.com/fioprotocol/fio/blob/master/LICENSE)

[FIO.Chronicle License](https://github.com/fioprotocol/fio.chronicle/blob/develop/LICENSE.txt)

Source code repository: https://github.com/fioprotocol/fio.chronicle

Forked Source code repository: https://github.com/EOSChronicleProject/eos-chronicle

Copyright 2018-2023 cc32d9@gmail.com

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.

## Releases

See the [release notes](https://github.com/fioprotocol/fio.chronicle/blob/develop/docs/releases.md) for information regarding FIO/EOS Chronicle releases.

## Relevant Documentation (Tutorial, Chat, Clients, Uses)

* [Chronicle Telegram chat](https://t.me/+TMWWcV1gBxQiqIkm)

* [Chronicle Tutorial](https://github.com/EOSChronicleProject/chronicle-tutorial) explains the nodeos and Chronicle server installation in detail.

* [Chronicle Consumer Module](https://github.com/EOSChronicleProject/chronicle-consumer-npm) is a Node.js module that consumer processes can be based on.

* [Chronicle Consumer Module Examples](https://github.com/EOSChronicleProject/chronicle-consumer-npm-examples) is a number of examples using the Node.js module.

* [Awesome Chronicle](https://github.com/EOSChronicleProject/awesome-chronicle) is a list of software projects and services using the software.

* [Docker File](https://github.com/EOSTribe/eos-chronicle-docker) provided by EOS Tribe

### Cloning the repository
To clone the FIO.Chronicle repository, execute the command; `git clone --recursive https://github.com/fioprotocol/fio.chronicle.git`

### Build and Install Instructions
Minimum build requirements: Cmake 3.11, GCC 8.3.0

Dependencies:
* Boost, version 1.80.0
* Clang, version 11.0.1
* CMake, version 3.31.2
* LLVM, version 7.1.0
* PostgreSQL, version 16*

**Currently PostgreSQL is a mandatory dependency despite the optional use of plugins that may require it. To install PostgreSQL execute the following commands (taken from https://www.postgresql.org/download/linux/ubuntu/)**
```shell
sudo apt install curl ca-certificates
sudo install -d /usr/share/postgresql-common/pgdg
sudo curl -o /usr/share/postgresql-common/pgdg/apt.postgresql.org.asc --fail https://www.postgresql.org/media/keys/ACCC4CF8.asc
sudo sh -c 'echo "deb [signed-by=/usr/share/postgresql-common/pgdg/apt.postgresql.org.asc] https://apt.postgresql.org/pub/repos/apt $(lsb_release -cs)-pgdg main" > /etc/apt/sources.list.d/pgdg.list'
sudo apt update
sudo apt -y install postgresql-16
```

#### Build
The build script takes one argument, the directory where to find or install the build dependencies including Boost, Clang, and LLVM. It is recommended to use a non-system level directory such as '/opt'. Note that any future builds, if given the same directory, will reuse those build dependencies.

To build fio.chronicle, execute the following commands;
```shell
cd fio.chronicle
./scripts/build.sh /opt
```

#### Install
To install fio.chronicle along with the default configuration to '/opt/fio-chronicle', execute the following command;
```shell
./scripts/install.sh
```

If desired, you may install the executable to '/usr/local/bin' by doing the following;
```shell
cd build
sudo make install
```
Note that the latter command will not install a default configuration; to do that, copy the [config.ini.sample](./config/config.ini.sample) to '/opt/fio-chronicle/config/config.ini'. See the following configuration overview for more insight into the default configuration as well as how to customize it.

##### Configuration Overview
The configuration of FIO.Chronicle is designated via options specified on the command-line as well as captured in a config.ini that is read as part of start up. The configuration options include, but are not limited to, the following;
Command-Line Options;
* --config-dir=\<directory where to find the config.ini\>
* --data-dir=\<directory where to store data\>

Config.Ini Options;
* host = \<the nodeos state history host (upstream connection to FIO.Nodeos state history api endpoint)\>
* port = \<the nodeos state history api port (upstream connection to FIO.Nodeos state history api endpoint port)\>
* plugin = \<the active plugin (max 1 active); options include 'exp-ws-plugin', 'exp-ws-plugin' \>
* exp-ws-host = \<the websocket server host (the downstream connnection to a web socket server host)\>
* exp-ws-port = \<the websocket server port (the downstream connnection to a web socket server port)\>
* exp-relic-host = \<the FIO.Relic database server host, valid for 'exp-ws-plugin' only (the downstream connnection to a PostgreSQL server host)\>
* exp-relic-port = \<the FIO.Relic database port, valid for 'exp-ws-plugin' only (the downstream connnection to a PostgreSQL server port)\>
* exp-relic-username = \<the FIO.Relic database user name, valid for 'exp-ws-plugin' only\>
* exp-relic-password = \<the FIO.Relic database password, valid for 'exp-ws-plugin' only\>

For more advanced configuration options review the [Advanced Configuration Options](docs/advanced-config.md).

Based on the configuration options above, using the websocket exporter plugin, the config.ini is as follows;
```shell
host = 127.0.0.1
port = 8080
mode = scan
plugin = exp_ws_plugin
exp-ws-host = 127.0.0.1
exp-ws-port = 8891
```

These options will allow FIO.Chronicle to connect to a FIO State History node at `127.0.0.1:8080` (host:port) and exports processed data, as json, to a websocket server at `127.0.0.1:8891` (url: exp-ws-host:exp-ws-port).

If using the relic exporter plugin, the config.ini would be as follows;
```shell
host = 127.0.0.1
port = 8080
mode = scan
plugin = exp_relic_plugin
exp-relic-host = 127.0.0.1
exp-relic-port = 5432
exp-relic-username = chronicle_user
exp-relic-password = password123!
```

These options will allow FIO.Chronicle to connect to a FIO State History node at `127.0.0.1:8080` (host:port) and will export processed data to the FIO.Relic PostgreSQL DB server running on host `127.0.0.1` and port `5432`.

##### Start a local FIO State History Node and a local web socket server (test only)
For the purposes of confirming end-to-end connectivity refer to the following documents to start a local FIO state history node as well as a local web socket server
* [LocalNet Deployment Guide - Start FIO Nodeos](https://github.com/fioprotocol/fio.relic/blob/develop/docs/localnet-standup.md#start-fio-nodeos)
* [LocalNet Deployment Guide - Start FIO Nodeos History Node](https://github.com/fioprotocol/fio.relic/blob/develop/docs/localnet-standup.md#start-fio-nodoes-state-history-nodeos)
* [LocalNet Deployment Guide - Start FIO.Chronicle Web Socket Server](https://github.com/fioprotocol/fio.relic/blob/develop/docs/localnet-standup.md#start-fio-chronicle-test-web-socket-server)

#### Starting the application
In the following command both a start and an end block number is specified to limit block processing. Note that the start block will default to 1 so only an end block is neccessary. As the `--end-block` parameter is required, specify a very large number to process blocks for the foreseeable future.

Start the fio-chronicle-receiver
```shell
/opt/fio-chronicle/chronicle-receiver --config-dir=/opt/fio-chronicle/config --data-dir=/opt/fio-chronicle/data --start-block=1 --end-block=10000
```
