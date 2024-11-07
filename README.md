# FIO Chronicle Project

FIO.Chronicle is a software application designed to process the history of the [FIO](https://github.com/fioprotocol/fio) blockchain and is a fork of the [EOSChronicle](https://github.com/EOSChronicleProject/eos-chronicle) project. For more detailed information regarding FIO.Chronicle and its origin, EOSChronicle, see the [overview](https://github.com/fioprotocol/fio.chronicle/blob/develop/docs/overview.md)

# FIO Protocol
The Foundation for Interwallet Operability (FIO) or, in short, the FIO Protocol, is an open-source project based on EOSIO 1.8+.

* For information on FIO Protocol, visit [FIO](https://fio.net).
* For information on the FIO Chain, API, and SDKs, including detailed clone, build and deploy instructions, visit [FIO Protocol Developer Hub](https://dev.fio.net).
* To get updates on the development roadmap, visit [FIO Improvement Proposals](https://github.com/fioprotocol/fips). Anyone is welcome and encouraged to contribute.
* To contribute, please review [Contributing to FIO](CONTRIBUTING.md)
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

## Ecosystem links

* [Chronicle Telegram chat](https://t.me/+TMWWcV1gBxQiqIkm)

* [Chronicle
  tutorial](https://github.com/EOSChronicleProject/chronicle-tutorial)
  explains the nodeos and Chronicle server installation in detail.

* [chronicle-consumer-npm](https://github.com/EOSChronicleProject/chronicle-consumer-npm)
  is a Node.js module that consumer processes can be based on.

* [chronicle-consumer-npm
  examples](https://github.com/EOSChronicleProject/chronicle-consumer-npm-examples)
  is a number of examples using the Node.js module.

* [Awesome
Chronicle](https://github.com/EOSChronicleProject/awesome-chronicle)
is a list of software projects and services using the software.

* [Docker file provided by EOS
  Tribe](https://github.com/EOSTribe/eos-chronicle-docker)

### Build Instructions

#### Build
Minimum requirements: Boost 1.80, Cmake 3.11, Clang 11.0.1, GCC 8.3.0, LLVM 7.1.0

Dependencies:
Boost version 1.80.0, Clang version 11.0.1, and LLVM, version 7.1.0 are all dependencies of fio.chronicle. The build script will automatically check for these versions in the provided direction and, if not found, will download, build and install them. This will take approximately 30 minutes, depending on the performance of your build system.

The build and install scripts are located in ./scripts directory. The build script takes one argument, the directory where to find or install the build products; boost, clang, and llvm. This could be anywhere writable by the ubuntu user, however, '/opt' is recommended. Note that any future builds, if given the same directory location, will reuse the build products.

To build fio.chronicle, execute the following command; `./scripts/build.sh /opt`

This will install any required OS dependencies, build and install the build products boost, clang and llvm if needed, create the build directory and build fio.chronicle

#### Install
To install fio.chronicle, execute the following command; `./scripts/install.sh`

#### Local Build and Install
See [FIO.Chronicle Local Install](https://github.com/fioprotocol/fio.chronicle/blob/develop/docs/install-local.md) document to install and deploy fio.chronicle locally.

### Nodeos Configuration

In order for Chronicle to function properly, both `trace-history` and `chain-state-history` should be enabled. Also if contract console
output needs to be present in Chronicle output, `trace-history-debug-mode` should be enabled too. The state history endpoint address
and port needs to be reachable from the host where Chronicle receiver is running.

Example `config.ini` for `nodeos`:

```
contracts-console = true
validation-mode = light
plugin = eosio::state_history_plugin
trace-history = true
chain-state-history = true
trace-history-debug-mode = true
state-history-endpoint = 0.0.0.0:8080
```

### FIO.Chronicle Configuration

Similarly to `nodeos`, `chronicle-receiver` needs a configuration
directory with `config.ini` in it, and a data directory where it stores
its internal state.

See the [Chronicle
tutorial](https://github.com/EOSChronicleProject/chronicle-tutorial)
for a more detailed and complete example.

Here's a minimal configuration for the receiver using Websocket
exporter. It connects to `nodeos` process running `state_history_plugin`
at `localhost:8080` and exports the data to a websocket server at
`localhost:8800`. In a production environment, hosts may be different
machines in the network.

The receiver would stop immediately if the websocket server is not
responding. For further tests, you need a consumer server ready.

The Perl script `testing/chronicle-ws-dumper.pl` can be used as a test
websocket server that dumps the input to standard output.

```
mkdir -p /srv/memento_wax1/chronicle-config
cat >/srv/memento_wax1/chronicle-config/config.ini <<'EOT'
host = 127.0.0.1
port = 8080
mode = scan
plugin = exp_ws_plugin
exp-ws-host = 127.0.0.1
exp-ws-port = 8800
exp-ws-bin-header = true
EOT

### FIO.Chronicle Execution

# Start the receiver to check that everything is working as
# expected. Use Ctrl-C to stop it.
/usr/local/bin/chronicle-receiver \
  --config-dir=/srv/memento_wax1/chronicle-config --data-dir=/srv/memento_wax1/chronicle-data

# install systemd unit file
cp /usr/local/share/chronicle_receiver\@.service /etc/systemd/system/
systemctl daemon-reload

# You may need to initialize the Chronicle database from the first block
# in the state history archive. See the Chronicle Tutorial for more
# details. You may point it to some other state history source during
# the initialization. Here we launch it in scan-noexport mode for faster initialization.
/usr/local/bin/chronicle-receiver --config-dir=/srv/memento_wax1/chronicle-config \
 --data-dir=/srv/memento_wax1/chronicle-data \
 --host=my.ship.host.domain.com --port=8080 \
 --start-block=186332760 --mode=scan-noexport --end-block=186332800

# Once it stops at the end block, launch the service
systemctl enable chronicle_receiver@memento_wax1
systemctl start chronicle_receiver@memento_wax1

# watch the log
journalctl -u memento_dbwriter@wax1 -f
```

### Command-line and configuration options

The following options are available from command-line only:

* `--data-dir=DIR` (mandatory): Directory containing program runtime
  data;

* `--config-dir=DIR` Directory containing configuration files such as
  config.ini. Defaults to `${HOME}/config-dir`;

* `-h [ --help ]`: Print help message and exit;

* `-v [ --version ]`:  Print version information;

* `--print-default-config`: Print default configuration template. The
  output will have empty `plugin` option, so you will need to add an
  exporter plugin to it.

* `--config=FILE` (=`config.ini`): Configuration file name relative to config-dir;

* `--logconf=FILE` (=`logging.json`): Logging configuration file
  name/path for library users. An example file that is only printing
  error messages is located in `examples/` folder.


The following options are available from command line and `config.ini`:

* `host = HOST` (=`localhost`): Host to connect to (nodeos with
  state_history_plugin);

* `port = PORT` (=`8080`): Port to connect to (nodeos with state-history
  plugin);

* `receiver-state-db-size = N` (=`1024`): State database size in MB;

* `mode = MODE`: mandatory receiver mode. Possible values:

  * `scan`: read state history blocks sequentially and export via export
    plugin.

  * `scan-noexport`: read state history blocks sequentially and skip any
    export. This is the fastest mode to collect ABI revisions so that
    interactive access can fetch required blocks.

  * `interactive`: interactive mode allows the consumer request random
    blocks. Irreversible-only mode is automatically set in this mode.

* `report-every = N` (=`10000`) Print informational messages every so
  many blocks;

* `max-queue-size = N` (=`10000`) If the asynchronous processing queue
  reaches this limit, the receiver will pause.

* `skip-block-events = true|false` (=`false`) Disable BLOCK events in
  export. This saves CPU time if you don't need block attributes, such
  as BP signatures and block ID.

* `skip-table-deltas = true|false` (=`false`) Disable table delta events
  in the export.

* `skip-traces = true|false` (=`false`) Disable transaction trace events
  in the export.

* `skip-account-info = true|false` (=`false`) Disable account
  permissions and metainformation in the export.

* `irreversible-only = true|false` (=`false`) fetch irreversible blocks
only

* `start-block = N` (=`0`) Initialize Chronicle state from given
  block. This is intended for starting Chronicle off a node that started
  from a portable snapshot. The snapshot has all table contents in
  the beginning, so Chronicle will process them all before continuing
  with the blocks. It may take some significant time. This option is
  only allowed when Chronicle data directory is empty.

* `end-block = N` (=`4294967295`)  Stop receiver before this block number

* `stale-deadline = N` (=`10000`) If there were no new blocks from
  state history socket within this time (in milliseconds),
  chronicle-receiver will stop and exit. The deadline timer is not
  used if the receiver is paused by a slow consumer.

* `enable-receiver-filter = true|false` (=`false`) if enabled,
  activates output filtering on traces matching the `include-receiver`
  filters.

* `include-receiver = NAME` If `enable-receivers-filter` is enabled,
  one or multiple `include-receiver` options specify the EOSIO account
  names that need to be matched in traces. The receiver looks for
  these names in receipt receivers of every action in trace, and
  outputs the trace only if at least one name matches. The smart
  contract executing an action is always receiving a receipt, so you
  can easily filter by contracts. Also in token transfers, normally
  payer and payee are receiving receipts.

* `enable-auth-filter = true|false` (=`false`) if enabled, activates
  output filtering on traces matching `include-auth` filters.

* `include-auth = NAME` If `enable-auth-filter` is enabled, one or
  multiple `include-auth` options specify the account names that are
  looked up in action authorizations. Only the traces matching at
  least one authorization will be included in the output.

* `blacklist-action = CONTRACT:ACTION` This option defines action
  names for specific contracts that are blocking the output of
  corresponding traces. Multiple (contract:action) tuples can be
  specified. By default, only `eosio:onblock` is blacklisted.

* `enable-tables-filter = true|false` (=`false`) if enabled, activates
  output filtering on table deltas for contracts matching
  `include-tables-contract` filters.

* `include-tables-contract = NAME` If `enable-tables-filter` is
  enabled, one or multiple `include-tables-contract` options specufy
  the contract names for which table deltas would be included in the
  output.

* `blacklist-tables-contract = NAME` This option allows excluding
  contract names from table deltas. Multiple options can be specified,
  and those contracts will be blacklisted from table deltas export.

If both `enable-receiver-filter` and `enable-auth-filter` are enabled,
the output will include traces matching any of the filters. The
blacklist has absolute precedence: regardless of filters
configuration, if a transaction matches the blacklist, it is dropped
from output.

Options for `exp_ws_plugin`:

* `exp-ws-host = HOST` (mandatory): Websocket server host to connect to;

* `exp-ws-port = PORT` (mandatory): Websocket server port to connect to;

* `exp-ws-path = PATH` (/): Websocket server URL path;

* `exp-ws-bin-header = true|false` (=`false`) Enable binary header mode
  (message type and options as binary integers, followed by JSON);

* `exp-ws-max-unack = N` (=1000): Receiver will pause at so many unacknowledged blocks;

* `exp-ws-max-queue = N` (=10000): Receiver will pause if outbound queue exceeds this limit.

