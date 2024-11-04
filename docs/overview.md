# Overview

## chronicle-receiver

The receiver is designed to work with `state_history_plugin` of
`nodeos`. It connects to the websocket endpoint provided by
the state history plugin and starts reading its data from a specific
block number.

The receiver can be compiled with a number of exporter plugins, and only
one exporter plugin can be enabled in its configuration. Exporter
plugins organize the data export to their respective consumers.

At the moment only one exporter plugin is implemented in the core
Chronicle package: `exp_ws_plugin`. Also, a new project called
"[chronos](https://github.com/EOSChronicleProject/chronos)" implments
an exporter plugin that writes the blockchain updates directly in its
database.

Exporters must work in bidirectional mode: the exporter expects that
the consumer acknowledges block numbers that it has processed and
stored. Should `chronicle-receiver` stop, it will start from the block
number next after acknowledged or last known irreversible, whichever
is lower.

The communication between exporter and consumer is performed
asynchronously: the receiver starts with a parameter indicating the
maximum number of unacknowledged blocks (1000 by default), and it
continues retrieving data from `nodeos` as long as the consumer
confirms the blocks within this limit. Received and decoded data is
kept in a queue that is fed to the consumer, allowing the consumer to
process the data at its own pace. If the number of unacknowledged
blocks reaches the maximum, the reader pauses itself with an
increasing timer, varying from 0.05 to 0.5 seconds. If the pause
exceeds 500 milliseconds, an informational message is printed on
console output.

If `nodeos` stops or restarts, `chronicle-receiver` will automatically
stop and close its downstream connection. Also if the downstream
connection closes, the receiver will stop itself and close the
connection to `nodeos`. The package includes a systemd unit file which
would restart the receiver automatically in this case.

### Scanning mode

When `mode` option is set to `scan`, `chronicle-receiver` operates as
follows:

* it reads all available blocks sequentially from state history.

* it monitors account changes, and as soon as a new ABI is set on a
  contract, it stores a copy of the ABI in its state memory. The state
  memory keeps all revisions of every contract ABI.

* upon receiving transaction traces or table deltas, it tries to
  decode the raw data using the available contract ABI.

* all data received from `state_history_plugin` is converted to JSON
  format. If there's no valid ABI for decoding raw data, it's presented
  as a hex string. In this case, an ABI decoder error event is
  generated.

* it feeds all JSON data and all error events to the exporter plugin,
  and the exporter plugin pushes the JSON data to its consumer. As
  described above, the consumer must send acknowledgements for processed
  block numbers.

In `scan-noexport` mode, the receiver requests the blocks from state
history sequentially and stores all revisions of contract ABI in its
database. This allows the ABIs to be quickly available for the
interactive mode.

### Interactive mode

In `interactive` mode, `chronicle-receiver` uses the state database
populated by another receiver process that is running in scanning
mode. Only one process is allowed to run in scanning mode, and multiple
processes can be started in interactive mode.

The exporter plugin, or probably some other plugin, receives a request
for particular block number or a range of blocks. This request is passed
to the receiver and requested from `state_history_plugin`. If a range is
specified, blocks up to the last before the end block are exported.

During request processing, the decoder retrieves the required contract
ABI from its ABI history, so that it's the latest copy from a block
number that is below the requested block.

Then, the same way as in scanning mode, decoded data is translated into
JSON and passed to the exporter plugin.

The receiver does not expect any acknowledgements in interactive mode.

Only irreversible blocks are available for interactive mode.

Note that in case of `exp_ws_plugin`, you need to specify a different
TCP port of the websocket server, so that it does not interfere with the
websocket communication in scanning mode when export is enabled.

### State database

`chronicle-receiver` utilizes `chainbase`, the same shared-memory
database library that is used by `nodeos`, to store its state. This
results in the same behavior as with `nodeos`:

* pre-allocated shared memory file is sparse and mostly empty;

* in case of abnormal termination, the shared memory file becomes dirty
  and unusable.

The state database keeps track of block numbers being processed, and it
stores also ABI for all contracts that it detects from `setabi`
actions. Chainbase is maintaining the history of revisions down to the
unacknowledged or irreversible block, in order to be able to roll back
in case of a fork or in case of receiver restart.

### Websocket exporter plugin

`exp_ws_plugin` exports the data to a websocket server.

The plugin connects to a specified websocket host and port and opens a
binary stream.

The plugin works in one of two possible modes:

In JSON mode (`exp-ws-bin-header=false`), each outgoing message is a
JSON object with two keys: `msgtype` indicates the type of the
message, and `data` contains the corresponding JSON object, such as
transaction trace or table delta. This mode is deprecated and will be
removed from future releases, as the binary mode is about 15% faster.

In binary header mode (`exp-ws-bin-header=true`), each message
consists of a binary 64-bit header and JSON data: the header consists
of two 32-bit unsigned integers indicating message type and options,
and the rest of the message is JSON data. Message type values are
available in `chronicle_msgtypes.h` header file. The second integer,
options, is currently always set to zero.

In scanning mode, the exporter expects that the server sends back
block number acknowledgements as decimal numbers in text format, each
number in an individual binary message.

In interactive mode, the exporter expects that the server sends each
request as a single binary message. The content of each message is
either one block number in decimal text notation, or two decimal
integers separated by minus sign (-) indicating a range of blocks.

### Portable snapshots

As of Chronicle versions 2.7 and 3.2, two new command-line options
`--save-snapshot` and `--restore-snapshot` allow saving a Chronicle
state database to a snapshot file or restoring it from such a snapshot
file. Both options can only be used when the chronicle-receiver
process is stopped.

The snapshots are compatible with versions 2.7 or higher and 3.2 or
higher.

Snapshots for some public networks are available for downloading:

https://snapshots.eosamsterdam.net/public/chronicle_snapshots/

An example of initializing Chronicle data from a snapshot:

```
cd /var/local
wget https://snapshots.eosamsterdam.net/public/chronicle_snapshots/chronicle_snapshot_wax_253338878.gz
gzip -d chronicle_snapshot_wax_253338878.gz

chronicle-receiver --config-dir=/srv/memento_wax1/chronicle-config --data-dir=/srv/memento_wax1/chronicle-data --restore-snapshot=chronicle_snapshot_wax_253338878

systemctl enable chronicle_receiver@memento_wax1
systemctl start chronicle_receiver@memento_wax1
```

Portable snapshots can be utilized for upgrading Chronicle from version 2.x to 3.x. 

```
# stop all runing Chronicle processes
systemctl stop -a  'chronicle_receiver@*'

# download and install the 2.7 package
cd /var/local
apt install ./eosio-chronicle-2.7-Clang-11.0.1-ubuntu20.04-x86_64.deb

# save the current chronicle state for all instances
chronicle-receiver --config-dir=/srv/memento_wax1/chronicle-config --data-dir=/srv/memento_wax1/chronicle-data --save-snapshot=wax.snapshot
chronicle-receiver --config-dir=/srv/memento_eos1/chronicle-config --data-dir=/srv/memento_eos1/chronicle-data --save-snapshot=eos.snapshot
chronicle-receiver --config-dir=/srv/memento_proton1/chronicle-config --data-dir=/srv/memento_proton1/chronicle-data --save-snapshot=proton.snapshot
chronicle-receiver --config-dir=/srv/memento_telos1/chronicle-config --data-dir=/srv/memento_telos1/chronicle-data --save-snapshot=telos.snapshot

# uninstall Chronicle 2.7, download and install Chronicle 3.2
apt remove eosio-chronicle
apt install ./antelope-chronicle-3.2-Clang-11.0.1-ubuntu20.04-x86_64.deb

# remove 2.x Chronicle data
rm -r /srv/*/chronicle-data

# restore the Chronicle data from snapshots
chronicle-receiver --config-dir=/srv/memento_eos1/chronicle-config --data-dir=/srv/memento_eos1/chronicle-data --restore-snapshot=eos.snapshot
chronicle-receiver --config-dir=/srv/memento_proton1/chronicle-config --data-dir=/srv/memento_proton1/chronicle-data --restore-snapshot=proton.snapshot
chronicle-receiver --config-dir=/srv/memento_telos1/chronicle-config --data-dir=/srv/memento_telos1/chronicle-data --restore-snapshot=telos.snapshot
chronicle-receiver --config-dir=/srv/memento_wax1/chronicle-config --data-dir=/srv/memento_wax1/chronicle-data --restore-snapshot=wax.snapshot

# start all Chronicle processes
systemctl start -a  'chronicle_receiver@*'

# check the consumer health
journalctl -u memento_dbwriter@wax1 -f
```

