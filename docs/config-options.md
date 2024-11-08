# Command-line and configuration options

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
