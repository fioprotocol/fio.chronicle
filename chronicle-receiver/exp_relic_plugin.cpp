

// copyright defined in LICENSE.txt

#include "exp_relic_plugin.hpp"
#include "decoder_plugin.hpp"
#include "receiver_plugin.hpp"
#include "chronicle_msgtypes.h"
#include "keyops.hpp"

#include <queue>
#include <boost/beast/websocket.hpp>
#include <boost/beast/core.hpp>
#include <stdexcept>
#include <limits>
#include <cstdint>




#include <fc/log/logger.hpp>
#include <fc/exception/exception.hpp>

using boost::beast::flat_buffer;
using boost::system::error_code;


static auto _exp_relic_plugin = app().register_plugin<exp_relic_plugin>();

namespace {
  const char* RELIC_HOST_OPT = "exp-relic-host";
  const char* RELIC_PORT_OPT = "exp-relic-port";
  const char* RELIC_PATH_OPT = "exp-relic-path";
  const char* RELIC_MAXUNACK_OPT = "exp-relic-max-unack";
  const char* RELIC_MAXQUEUE_OPT = "exp-reic-max-queue";
  const char* RELIC_BINHDR = "exp-relic-bin-header";
}

class exp_relic_plugin_impl : std::enable_shared_from_this<exp_relic_plugin_impl> {
public:
  chronicle::channels::js_forks::channel_type::handle               _js_forks_subscription;
  chronicle::channels::js_block_started::channel_type::handle       _js_block_started_subscription;
  chronicle::channels::js_blocks::channel_type::handle              _js_blocks_subscription;
  chronicle::channels::js_transaction_traces::channel_type::handle  _js_transaction_traces_subscription;
  chronicle::channels::js_abi_updates::channel_type::handle         _js_abi_updates_subscription;
  chronicle::channels::js_abi_removals::channel_type::handle        _js_abi_removals_subscription;
  chronicle::channels::js_abi_errors::channel_type::handle          _js_abi_errors_subscription;
  chronicle::channels::js_table_row_updates::channel_type::handle   _js_table_row_updates_subscription;
  chronicle::channels::js_permission_updates::channel_type::handle  _js_permission_updates_subscription;
  chronicle::channels::js_permission_link_updates::channel_type::handle  _js_permission_link_updates_subscription;
  chronicle::channels::js_account_metadata_updates::channel_type::handle  _js_account_metadata_updates_subscription;
  chronicle::channels::js_receiver_pauses::channel_type::handle     _js_receiver_pauses_subscription;
  chronicle::channels::js_block_completed::channel_type::handle     _js_block_completed_subscription;
  chronicle::channels::js_abi_decoder_errors::channel_type::handle  _js_abi_decoder_errors_subscription;

  chronicle::channels::interactive_requests::channel_type&          _interactive_requests_chan;

  string relic_host;
  string relic_port;
  string relic_path;
  bool use_bin_headers;
  uint32_t maxunack;

  //using wstream = boost::beast::websocket::stream<boost::asio::ip::tcp::socket>;
 // std::shared_ptr<wstream> ws;
  const int relic_priority = 60;
  const int relic_order = 1000;

  rapidjson::StringBuffer json_buffer;
  rapidjson::Writer<rapidjson::StringBuffer> json_writer;

  using msgbuf = std::vector<unsigned char>;
  std::queue<std::shared_ptr<msgbuf>> async_queue;
  std::shared_ptr<msgbuf> async_msg; // this to prevent deallocation during async write
  uint32_t queue_hwm;
  uint32_t queue_lwm;
  boost::asio::const_buffer async_out_buffer;
  std::shared_ptr<boost::asio::deadline_timer> mytimer;

  uint32_t pause_time_msec = 0;
  uint32_t msg_report_counter = 1000;

  PGconn *conn;

  exp_relic_plugin_impl() :
    _interactive_requests_chan(app().get_channel<chronicle::channels::interactive_requests>())
  {};

  void init() {
    mytimer = std::make_shared<boost::asio::deadline_timer>(app().get_io_service());

    //connect to postgres relic db
    const char *conninfo = "dbname=relicdb user=chronicle_user password=relicchronicle1@0@2 host=localhost port=5432";

    conn = PQconnectdb(conninfo);

    if (PQstatus(conn) != CONNECTION_OK) {
        ilog("failed to connect to relic database!");
        PQfinish(conn);
        return;
    }

    ilog ("connected to relic db!!");

    if (use_bin_headers) {
      _js_forks_subscription =
        app().get_channel<chronicle::channels::js_forks>().subscribe
        ([this](std::shared_ptr<string> event){ on_event_bin(CHRONICLE_MSGTYPE_FORK, 0, event); });

      _js_block_started_subscription =
        app().get_channel<chronicle::channels::js_block_started>().subscribe
        ([this](std::shared_ptr<string> event){ on_event_bin(CHRONICLE_MSGTYPE_BLOCK_STARTED, 0, event); });

      _js_blocks_subscription =
        app().get_channel<chronicle::channels::js_blocks>().subscribe
        ([this](std::shared_ptr<string> event){ on_event_bin(CHRONICLE_MSGTYPE_BLOCK, 0, event); });

      _js_transaction_traces_subscription =
        app().get_channel<chronicle::channels::js_transaction_traces>().subscribe
        ([this](std::shared_ptr<string> event){ on_event_bin(CHRONICLE_MSGTYPE_TX_TRACE, 0, event); });

      _js_abi_updates_subscription =
        app().get_channel<chronicle::channels::js_abi_updates>().subscribe
        ([this](std::shared_ptr<string> event){ on_event_bin(CHRONICLE_MSGTYPE_ABI_UPD, 0, event); });

      _js_abi_removals_subscription =
        app().get_channel<chronicle::channels::js_abi_removals>().subscribe
        ([this](std::shared_ptr<string> event){ on_event_bin(CHRONICLE_MSGTYPE_ABI_REM, 0, event); });

      _js_abi_errors_subscription =
        app().get_channel<chronicle::channels::js_abi_errors>().subscribe
        ([this](std::shared_ptr<string> event){ on_event_bin(CHRONICLE_MSGTYPE_ABI_ERR, 0, event); });

      _js_table_row_updates_subscription =
        app().get_channel<chronicle::channels::js_table_row_updates>().subscribe
        ([this](std::shared_ptr<string> event){ on_event_bin(CHRONICLE_MSGTYPE_TBL_ROW, 0, event); });

      _js_permission_updates_subscription =
        app().get_channel<chronicle::channels::js_permission_updates>().subscribe
        ([this](std::shared_ptr<string> event){ on_event_bin(CHRONICLE_MSGTYPE_PERMISSION, 0, event); });

      _js_permission_link_updates_subscription =
        app().get_channel<chronicle::channels::js_permission_link_updates>().subscribe
        ([this](std::shared_ptr<string> event){ on_event_bin(CHRONICLE_MSGTYPE_PERMISSION_LINK, 0, event); });

      _js_account_metadata_updates_subscription =
        app().get_channel<chronicle::channels::js_account_metadata_updates>().subscribe
        ([this](std::shared_ptr<string> event){ on_event_bin(CHRONICLE_MSGTYPE_ACC_METADATA, 0, event); });

      _js_abi_decoder_errors_subscription =
        app().get_channel<chronicle::channels::js_abi_decoder_errors>().subscribe
        ([this](std::shared_ptr<string> event){ on_event_bin(CHRONICLE_MSGTYPE_ENCODER_ERR, 0, event); });

      _js_receiver_pauses_subscription =
        app().get_channel<chronicle::channels::js_receiver_pauses>().subscribe
        ([this](std::shared_ptr<string> event){ on_event_bin(CHRONICLE_MSGTYPE_RCVR_PAUSE, 0, event); });

      _js_block_completed_subscription =
        app().get_channel<chronicle::channels::js_block_completed>().subscribe
        ([this](std::shared_ptr<string> event){ on_event_bin(CHRONICLE_MSGTYPE_BLOCK_COMPLETED, 0, event); });
    }
    else {
      json_buffer.Reserve(1024*256);

      _js_forks_subscription =
        app().get_channel<chronicle::channels::js_forks>().subscribe
        ([this](std::shared_ptr<string> event){ on_event_json("FORK", event); });

      _js_block_started_subscription =
        app().get_channel<chronicle::channels::js_block_started>().subscribe
        ([this](std::shared_ptr<string> event){ on_event_json("BLOCK_STARTED", event); });

      _js_blocks_subscription =
        app().get_channel<chronicle::channels::js_blocks>().subscribe
        ([this](std::shared_ptr<string> event){ on_event_json("BLOCK", event); });

      _js_transaction_traces_subscription =
        app().get_channel<chronicle::channels::js_transaction_traces>().subscribe
        ([this](std::shared_ptr<string> event){ on_event_json("TX_TRACE", event); });

      _js_abi_updates_subscription =
        app().get_channel<chronicle::channels::js_abi_updates>().subscribe
        ([this](std::shared_ptr<string> event){ on_event_json("ABI_UPD", event); });

      _js_abi_removals_subscription =
        app().get_channel<chronicle::channels::js_abi_removals>().subscribe
        ([this](std::shared_ptr<string> event){ on_event_json("ABI_REM", event); });

      _js_abi_errors_subscription =
        app().get_channel<chronicle::channels::js_abi_errors>().subscribe
        ([this](std::shared_ptr<string> event){ on_event_json("ABI_ERR", event); });

      _js_table_row_updates_subscription =
        app().get_channel<chronicle::channels::js_table_row_updates>().subscribe
        ([this](std::shared_ptr<string> event){ on_event_json("TBL_ROW", event); });

      _js_permission_updates_subscription =
        app().get_channel<chronicle::channels::js_permission_updates>().subscribe
        ([this](std::shared_ptr<string> event){ on_event_json("PERMISSION", event); });

      _js_permission_link_updates_subscription =
        app().get_channel<chronicle::channels::js_permission_link_updates>().subscribe
        ([this](std::shared_ptr<string> event){ on_event_json("PERMISSION_LINK", event); });

      _js_account_metadata_updates_subscription =
        app().get_channel<chronicle::channels::js_account_metadata_updates>().subscribe
        ([this](std::shared_ptr<string> event){ on_event_json("ACC_METADATA", event); });

      _js_receiver_pauses_subscription =
        app().get_channel<chronicle::channels::js_receiver_pauses>().subscribe
        ([this](std::shared_ptr<string> event){ on_event_json("RCVR_PAUSE", event); });

      _js_block_completed_subscription =
        app().get_channel<chronicle::channels::js_block_completed>().subscribe
        ([this](std::shared_ptr<string> event){ on_event_json("BLOCK_COMPLETED", event); });

      _js_abi_decoder_errors_subscription =
        app().get_channel<chronicle::channels::js_abi_decoder_errors>().subscribe
        ([this](std::shared_ptr<string> event){ on_event_json("ENCODER_ERR", event); });
    }
  }


  void start() {
    if (!is_interactive_mode())
      exporter_will_ack_blocks(maxunack);

    //TODO -- modify exporter required parameters and rename the exporter
    //bin headers
    //examine arguments to see which apply for relic processing
   
    async_send_events();
  }




//send events to the relic database here.
//relic
  void async_send_events() {
    
    if( async_queue.empty() ) {
      
      if( pause_time_msec == 0 ) {
        pause_time_msec = 5;
      }
      else if( pause_time_msec < 256 ) {
        pause_time_msec *= 2;
      }

      mytimer->expires_from_now(boost::posix_time::milliseconds(pause_time_msec));
      mytimer->async_wait(app().executor().get_priority_queue().wrap(relic_priority, relic_order, [this](const error_code ec) {
            async_send_events();
          }));
    }
    else {
      
      pause_time_msec = 0;
      if( async_queue.size() >= queue_hwm ) {
        slowdown_receiver(true);
      }
      else if( async_queue.size() < queue_lwm ) {
        slowdown_receiver(false);
      }
      async_msg = async_queue.front();
      async_queue.pop();
      async_out_buffer = boost::asio::const_buffer(async_msg->data(), async_msg->size());

      rapidjson::Document document;
      document.Parse((const char*)async_msg->data(),async_msg->size());

    // Check if parsing was successful
    if (document.HasParseError()) {
        std::cerr << "   JSON parsing error: " << document.GetParseError() << std::endl;
    }
    if (document.HasMember("msgtype") && 
        document["msgtype"].IsString() &&
        document.HasMember("data") && 
        document["data"].IsObject() 
     ){

         //output the msg
         rapidjson::StringBuffer buffer;
         rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
        // Serialize the document to JSON
        document.Accept(writer);
        ilog (buffer.GetString());

        
        const string msgtype = document["msgtype"].GetString();
        const rapidjson::Value& dataj = document["data"];
        if (
            dataj.HasMember("block_num") &&
            dataj["block_num"].IsString()
    
        ) {
          uint32_t bnum =static_cast<uint32_t>(std::stoul( document["data"]["block_num"].GetString()));
          string bnums =document["data"]["block_num"].GetString();
          if (msgtype == "BLOCK_COMPLETED"){
          //ack block on BLOCK_COMPLETED
          ack_block(bnum -1);
          }
          else if (msgtype == "BLOCK"){
           string btimestamp = getjsonstring(UNKNOWN_TIMESTAMP,document["data"]["block"]["timestamp"],DISALLOW_EMPTY_VALUES);
           string bproducer = getjsonstring(UNKNOWN_STRING,document["data"]["block"]["producer"],DISALLOW_EMPTY_VALUES);
           string bschedv = getjsonstring(UNKNOWN_STRING,document["data"]["block"]["schedule_version"],DISALLOW_EMPTY_VALUES);
           string bid = getjsonstring(UNKNOWN_STRING,document["data"]["block_id"],DISALLOW_EMPTY_VALUES);
          
           string insertQuery = "SELECT insblocks("+bnums+",'"+btimestamp+"','"+bid+"','"+bproducer+"','"+bschedv+"');";
            ilog("EDEDEDEDEDEDEDED ins block ${s}",("s",insertQuery));
           
            PGresult *res = PQexec(conn, insertQuery.c_str());
            ilog("ins block result status ${r} ",("r",PQresultStatus(res)));
            if (PQresultStatus(res) != PGRES_TUPLES_OK) {
              ilog("insert into blocks failed ");
                PQclear(res);
                PQfinish(conn);
                return;
            }

            PQclear(res);
          }
           else if (msgtype == "TX_TRACE"){

            string blocktimestamp =getjsonstring(UNKNOWN_TIMESTAMP,document["data"]["block_timestamp"],DISALLOW_EMPTY_VALUES);
          
            //gotta parse the transaction info.
            string trid =getjsonstring(UNKNOWN_STRING,document["data"]["trace"]["id"],DISALLOW_EMPTY_VALUES);
            string status =getjsonstring(UNKNOWN_STRING,document["data"]["trace"]["status"],DISALLOW_EMPTY_VALUES);
            //insTransactions
           
           

             //gotta parse the list of action traces
             /*
             {"action_ordinal":"1",
              "receiver":"fio.address",
              "act":{"account":"fio.address","name":"regdomain","authorization":[{"actor":"qhh25sqpktwh","permission":"active"}],
              "data":{"fio_domain":"fiotestnet","owner_fio_public_key":"","max_fee":"40000000000","actor":"qhh25sqpktwh","tpid":""}},"context_free":"false","elapsed":"2267","console":"","account_ram_deltas":[{"account":"qhh25sqpktwh","delta":"564"}],"except":"","error_code":null},
             
              PKActionTraceId bigint PRIMARY KEY GENERATED BY DEFAULT AS IDENTITY,
     FKTransactionId bigint references Transactions(PKTransactionId),
     FKActionAccountId bigint REFERENCES Accounts(PKAccountId),
     FKAccountId bigint REFERENCES Accounts(PKAccountId),
     FKReceiverAccountId bigint REFERENCES Accounts(PKAccountId),
    ActionOrdinal int,
    ActionName char(12).
    Tpid  varchar(64),
    Fee  integer,
    RequestData text,
    ResponseData text,
    Status varchar(120)
             
             
             */


             const rapidjson::Value& dvtrace = document["data"]["trace"]["action_traces"];
             int64_t fktransactionid = -1; //index of transactionid
              for (const auto& object : dvtrace.GetArray()) {
                if (!object.IsObject()) {
                    std::cerr << "Error: Element in array is not an object." << std::endl;
                    continue;
                }
                string actionordinal = getjsonstring(UNKNOWN_NUMBER,(rapidjson::Value&)object["action_ordinal"],DISALLOW_EMPTY_VALUES);
                int64_t iactordinal =  -1;
                if(!(actionordinal == UNKNOWN_NUMBER)){
                  iactordinal = atoi(actionordinal.c_str());
                }
                string receiveraccount = getjsonstring(UNKNOWN_STRING,(rapidjson::Value&)object["receiver"],DISALLOW_EMPTY_VALUES);
                string contractaccount = getjsonstring(UNKNOWN_STRING,(rapidjson::Value&)object["act"]["account"],DISALLOW_EMPTY_VALUES);
                string actionname = getjsonstring(UNKNOWN_STRING,(rapidjson::Value& )object["act"]["name"],DISALLOW_EMPTY_VALUES);
                const rapidjson::Value& actdata = object["act"]["data"];

                if (actdata.IsObject())
                {
                  ilog("EDEDEEEDEDEDEDED actdata is object!!");
                   string actdatastr = getjsonstring(UNKNOWN_STRING,(rapidjson::Value&)actdata,ALLOW_EMPTY_VALUES);
               ilog("EDEDEDEDEDEDEDED actdata looks like ${d}",("d",actdatastr));
              
                }
                const rapidjson::Value& arrayauth = object["act"]["authorization"];
                rapidjson::Value& firstauth = (rapidjson::Value&)object;
                
                 if (arrayauth.IsArray() && (arrayauth.Size() > 0)){
                  ilog("EDEDEEEDEDEDEDEDEDEDED found the act authorization!!!!");
                   firstauth = (rapidjson::Value&)arrayauth[0];
                 }

                 
                 string actionaccount = getjsonstring(UNKNOWN_STRING,(rapidjson::Value&)firstauth["actor"],DISALLOW_EMPTY_VALUES);
                 string tpid = "UNKNOWN";
                 if(actdata.IsObject()){
                  tpid = getjsonstring(UNKNOWN_STRING,(rapidjson::Value&)actdata["fio_address"],ALLOW_EMPTY_VALUES);
                 }
                 string requestdata = getjsonstring(UNKNOWN_STRING,(rapidjson::Value&)actdata,ALLOW_EMPTY_VALUES);
                 string maxfee = getjsonstring(UNKNOWN_STRING,(rapidjson::Value&)firstauth["actor"],DISALLOW_EMPTY_VALUES);
                 uint64_t feecollected = 0;
                 string responsedata = "NOT YET IMPLEMENTED integrate response into history";
                 //TODO: add response from fio to history node and parse this!!!!
  
                if((iactordinal == 1)&&!(actionname == "onblock")&&!(actionname == "nonce")){
                  //select instransactions(1,'2024-01-01','transid','edacct','edacct','actnm','thistpd',300,'requestd','responsed','ok');
                  string insertQuery = "SELECT instransactions("+
                      bnums+",'"+
                      blocktimestamp+"','"+
                      trid+"','"+
                      contractaccount+"','"+
                       actionaccount+"','"+
                        actionname+"','"+
                        tpid+"',"+
                        boost::lexical_cast<std::string>(feecollected)+",'"+
                        requestdata+"','"+
                        responsedata+"','"+
                        status+"');";

                        

                      //last trid+"');";
                  ilog("EDEDEDEDEDEDEDED ins transaction ${s}",("s",insertQuery));
           
                  PGresult *res = PQexec(conn, insertQuery.c_str());
                  ilog("ins transaction result status ${r} ",("r",PQresultStatus(res)));
                  if (PQresultStatus(res) != PGRES_TUPLES_OK) {
                    ilog("insert into transactions failed ");
                    PQclear(res);
                    PQfinish(conn);
                    return;
                  }

            
                  if (PQgetvalue(res, 0, 0)) {
                      fktransactionid = atoi(PQgetvalue(res, 0, 0));
                  }

                  PQclear(res);

                 //do relic fio transaction relating actions.
               
                 if ((actionname == "trnsfiopubky")||(actionname == "trnloctoks")){
                  string payeracct = getjsonstring(UNKNOWN_STRING,(rapidjson::Value&)actdata["actor"],ALLOW_EMPTY_VALUES);                 
                  string pubkey = getjsonstring(UNKNOWN_STRING,(rapidjson::Value&)actdata["payee_public_key"],ALLOW_EMPTY_VALUES);
                  string payeeacct = fioio::key_to_account(pubkey);
                  string TRNSTYPETRANSFER = "transfer";
                  string TRNSTYPETRANSFERLOCKED = "transfer_locked";
                  string trnstype = TRNSTYPETRANSFER;
                  if(actionname == "trnloctoks") {
                    trnstype = TRNSTYPETRANSFERLOCKED;
                  }
                  string sufamount = getjsonstring(UNKNOWN_STRING,(rapidjson::Value&)actdata["amount"],ALLOW_EMPTY_VALUES);
                  ilog("EDEDEDEDEDEDEDED key_to_account account ${s}",("s",payeeacct));
                  string insertQuery = "SELECT instokentransfers("+
                       boost::lexical_cast<std::string>(fktransactionid)+","+
                      bnums+",'"+
                      payeracct+"','"+
                      payeeacct+"',"+
                      sufamount+",'"+
                      trnstype +"','"+
                      +"UNKNOWN','"+
                      blocktimestamp+"');";
                      
                  ilog("EDEDEDEDEDEDEDED ins tokentransfers ${s}",("s",insertQuery));
                  PGresult *res = PQexec(conn, insertQuery.c_str());
                  ilog("ins tokentransfers result status ${r} ",("r",PQresultStatus(res)));
                  if (PQresultStatus(res) != PGRES_TUPLES_OK) {
                    ilog("insert into token transfers failed ");
                    PQclear(res);
                    PQfinish(conn);
                    return;
                  }
                  PQclear(res);
                } //end if action is trnsfiopubky trnsloctok
                else if ((actionname == "regdomain")){
                   string domainname = getjsonstring(UNKNOWN_STRING,(rapidjson::Value&)actdata["fio_domain"],ALLOW_EMPTY_VALUES);                                
                   string pubkey = getjsonstring(UNKNOWN_STRING,(rapidjson::Value&)actdata["owner_fio_public_key"],ALLOW_EMPTY_VALUES);
                  string owneracct = fioio::key_to_account(pubkey);
                  string ispublic = "false";
                  string expirationtimestamp = "1978-01-02"; //TODO integrate response from state history!!!!!
                  //TODO integrate response!!!!!
                  //TODO integrate response!!!!!
                  string domainstatus = "active";
                  string insertQuery = "SELECT insdomains("+
                      bnums+",'"+
                      domainname+"','"+
                      owneracct+"','"+
                      ispublic+"','"+
                      expirationtimestamp+"','"+
                      domainstatus +"');";
                      
                  ilog("EDEDEDEDEDEDEDED ins domains ${s}",("s",insertQuery));
                  PGresult *res = PQexec(conn, insertQuery.c_str());
                  ilog("ins domains result status ${r} ",("r",PQresultStatus(res)));
                  if (PQresultStatus(res) != PGRES_TUPLES_OK) {
                    ilog("insert into domains failed ");
                    PQclear(res);
                    PQfinish(conn);
                    return;
                  }
                  PQclear(res);

                  //insert domain activities
                  string DOMAINACTIVITYREGISTER = "register";
                  insertQuery = "SELECT insdomainactivities("+
                  boost::lexical_cast<std::string>(fktransactionid)+","+
                      bnums+",'"+
                      domainname+"','"+
                      expirationtimestamp+"','"+
                      DOMAINACTIVITYREGISTER+"','"+
                      blocktimestamp+"');";
                      
                  ilog("EDEDEDEDEDEDEDED ins domainactivities ${s}",("s",insertQuery));
                  res = PQexec(conn, insertQuery.c_str());
                  ilog("ins domainactivities result status ${r} ",("r",PQresultStatus(res)));
                  if (PQresultStatus(res) != PGRES_TUPLES_OK) {
                    ilog("insert into domainactivities failed ");
                    PQclear(res);
                    PQfinish(conn);
                    return;
                  }
                  PQclear(res);
                } //end if action is regdomain
                else if ((actionname == "wraptokens")){
                   string payeracct = getjsonstring(UNKNOWN_STRING,(rapidjson::Value&)actdata["actor"],ALLOW_EMPTY_VALUES);                                
                  string payeeacct = "fio.oracle";
                  string memo = "UNKNOWN";
                  string TRNSTYPEWRAP = "wrap";
                  string sufamount = getjsonstring(UNKNOWN_STRING,(rapidjson::Value&)actdata["amount"],DISALLOW_EMPTY_VALUES);

                  string insertQuery = "SELECT instokentransfers("+
                       boost::lexical_cast<std::string>(fktransactionid)+","+
                      bnums+",'"+
                      payeracct+"','"+
                      payeeacct+"',"+
                      sufamount+",'"+
                      TRNSTYPEWRAP +"','"+
                      memo+"','"+
                      blocktimestamp+"');";
                      
                  ilog("EDEDEDEDEDEDEDED ins tokentransfers ${s}",("s",insertQuery));
                  PGresult *res = PQexec(conn, insertQuery.c_str());
                  ilog("ins tokentransfers result status ${r} ",("r",PQresultStatus(res)));
                  if (PQresultStatus(res) != PGRES_TUPLES_OK) {
                    ilog("insert into token transfers failed ");
                    PQclear(res);
                    PQfinish(conn);
                    return;
                  }
                  PQclear(res);
                } //end if action is wraptokens
                 else if ((actionname == "stakefio")){
                   string stakingacct = getjsonstring(UNKNOWN_STRING,(rapidjson::Value&)actdata["actor"],ALLOW_EMPTY_VALUES);                                
                  string sufamount = getjsonstring(UNKNOWN_STRING,(rapidjson::Value&)actdata["amount"],DISALLOW_EMPTY_VALUES);
                  string insertQuery = "SELECT instokenstakings("+
                       boost::lexical_cast<std::string>(fktransactionid)+","+
                      bnums+",'"+
                      stakingacct+"','"+
                      sufamount+"');";
                      
                  ilog("EDEDEDEDEDEDEDED ins tokenstakings ${s}",("s",insertQuery));
                  PGresult *res = PQexec(conn, insertQuery.c_str());
                  ilog("ins tokenstakings result status ${r} ",("r",PQresultStatus(res)));
                  if (PQresultStatus(res) != PGRES_TUPLES_OK) {
                    ilog("insert into token stakings failed ");
                    PQclear(res);
                    PQfinish(conn);
                    return;
                  }
                  PQclear(res);
                } //end if action is stakefio
                 else if ((actionname == "unstakefio")){
                   string stakingacct = getjsonstring(UNKNOWN_STRING,(rapidjson::Value&)actdata["actor"],ALLOW_EMPTY_VALUES);                                
                  string sufamount = getjsonstring(UNKNOWN_STRING,(rapidjson::Value&)actdata["amount"],DISALLOW_EMPTY_VALUES);
                  string insertQuery = "SELECT instokenstakings("+
                       boost::lexical_cast<std::string>(fktransactionid)+","+
                      bnums+",'"+
                      stakingacct+"','"+
                      "-"+sufamount+"');";
                      
                  ilog("EDEDEDEDEDEDEDED ins tokenstakings ${s}",("s",insertQuery));
                  PGresult *res = PQexec(conn, insertQuery.c_str());
                  ilog("ins tokenstakings result status ${r} ",("r",PQresultStatus(res)));
                  if (PQresultStatus(res) != PGRES_TUPLES_OK) {
                    ilog("insert into token stakings failed ");
                    PQclear(res);
                    PQfinish(conn);
                    return;
                  }
                  PQclear(res);
                } //end if action is unstakefio
                 else if ((actionname == "retire")){
                  string payeracct = getjsonstring(UNKNOWN_STRING,(rapidjson::Value&)actdata["actor"],ALLOW_EMPTY_VALUES);                                
                  string payeeacct = "";
                  string TRNSTYPERETIRE = "retire";
                  string memo = getjsonstring(UNKNOWN_STRING,(rapidjson::Value&)actdata["memo"],DISALLOW_EMPTY_VALUES);
                  string sufamount = getjsonstring(UNKNOWN_STRING,(rapidjson::Value&)actdata["quantity"],DISALLOW_EMPTY_VALUES);
                  string insertQuery = "SELECT instokentransfers("+
                       boost::lexical_cast<std::string>(fktransactionid)+","+
                      bnums+",'"+
                      payeracct+"','"+
                      payeeacct+"',"+
                      sufamount+",'"+
                      TRNSTYPERETIRE +"','"+
                      memo+"','"+
                      blocktimestamp+"');";
                       
                  ilog("EDEDEDEDEDEDEDED ins tokentransfers ${s}",("s",insertQuery));
                  PGresult *res = PQexec(conn, insertQuery.c_str());
                  ilog("ins tokentransfers result status ${r} ",("r",PQresultStatus(res)));
                  if (PQresultStatus(res) != PGRES_TUPLES_OK) {
                    ilog("insert into token transfers failed ");
                    PQclear(res);
                    PQfinish(conn);
                    return;
                  }
                  PQclear(res);
                } //end if action is retire


                } //end action ordinal is 1
                else if ((iactordinal > 1)&&(fktransactionid > -1)) { //action ordinal >1, and a valid tx 
                  //insert into traces
                  string insertQuery = "SELECT instraces("+
                    boost::lexical_cast<std::string>(fktransactionid)+","+
                    bnums+",'"+
                    actionaccount+"','"+
                    receiveraccount+"',"+
                    actionordinal+",'"+
                    actionname+"','"+
                    requestdata+"','"+
                    blocktimestamp+"');";
                
                  ilog("EDEDEDEDEDEDEDED ins traces ${s}",("s",insertQuery));
           
                  PGresult *res = PQexec(conn, insertQuery.c_str());
                  ilog("ins traces result status ${r} ",("r",PQresultStatus(res)));
                  if (PQresultStatus(res) != PGRES_TUPLES_OK) {
                    ilog("insert into traces failed ");
                    PQclear(res);
                    PQfinish(conn);
                    return;
                  }

                  PQclear(res);
                

                }


               if ((actionname == "transfer")&&(receiveraccount == "fio.token")){
                  string payeracct = getjsonstring(UNKNOWN_STRING,(rapidjson::Value&)actdata["from"],DISALLOW_EMPTY_VALUES);                 
                  string payeeacct = getjsonstring(UNKNOWN_STRING,(rapidjson::Value&)actdata["to"],DISALLOW_EMPTY_VALUES);
                  string memo = getjsonstring(UNKNOWN_STRING,(rapidjson::Value&)actdata["memo"],DISALLOW_EMPTY_VALUES);
                  string trnstype = "transfer";

                  string FIOFEESTR = "FIO fee";
                  string FIOTPIDSTR = "Paying TPID from treasury";
                  string FIOSTAKINGSTR = "Paying Staking Rewards";
                  string FIOPRODUCERSTR = "Paying producer from treasury";
                  string FIOFOUNDATIONSTR = "Paying foundation from treasury";
                  string FIOWRAPPINGSTR = "Token Wrapping Oracle Fee";
                  string FIOUNWRAPPINGSTR = "Token Unwrapping";

                  string TRNSTYPEBLOCKCHAINFEE = "blockchain_fee";
                  string TRNSTYPETPIDREWARD = "tpid_reward";
                  string TRNSTYPESTAKINGREWARD = "staking_reward";
                  string TRNSTYPEBPREWARD = "bp_reward";
                  string TRNSTYPEFOUNDATIONREWARD = "foundation_reward";
                  string TRNSTYPEORACLEFEE = "oracle_fee";
                  string TRNSTYPEUNWRAP = "unwrap";
                  
                  if (memo.find(FIOFEESTR)!= std::string::npos){
                      trnstype = TRNSTYPEBLOCKCHAINFEE;
                  }else if (memo.find(FIOTPIDSTR)!= std::string::npos){
                    trnstype = TRNSTYPETPIDREWARD;
                  }else if (memo.find(FIOSTAKINGSTR)!= std::string::npos){
                    trnstype = TRNSTYPESTAKINGREWARD;
                  }else if (memo.find(FIOPRODUCERSTR)!= std::string::npos){
                    trnstype = TRNSTYPEBPREWARD;
                  }else if (memo.find(FIOFOUNDATIONSTR)!= std::string::npos){
                    trnstype = TRNSTYPEFOUNDATIONREWARD;
                  }else if (memo.find(FIOWRAPPINGSTR)!= std::string::npos){
                    trnstype = TRNSTYPEORACLEFEE;
                  }else if (memo.find(FIOUNWRAPPINGSTR)!= std::string::npos){
                    trnstype = TRNSTYPEUNWRAP;
                  } 
                  string sufamount = getjsonstring(UNKNOWN_STRING,(rapidjson::Value&)actdata["quantity"],DISALLOW_EMPTY_VALUES);
                  sufamount.erase(std::remove(sufamount.begin(), sufamount.end(), '.'), sufamount.end());
                  string substringToRemove = " FIO";
                  size_t position = sufamount.find(substringToRemove);
                  if (position != std::string::npos) {
                      sufamount.erase(position, substringToRemove.length());
                  }
 ilog("EDEDEDEDEDEDEDED key_to_account account ${s}",("s",payeeacct));
                  string insertQuery = "SELECT instokentransfers("+
                       boost::lexical_cast<std::string>(fktransactionid)+","+
                      bnums+",'"+
                      payeracct+"','"+
                      payeeacct+"',"+
                      sufamount+",'"+
                      trnstype +"','"+
                      memo+"','"+
                      blocktimestamp+"');";
                      
                  ilog("EDEDEDEDEDEDEDED ins tokentransfers ${s}",("s",insertQuery));
                  PGresult *res = PQexec(conn, insertQuery.c_str());
                  ilog("ins tokentransfers result status ${r} ",("r",PQresultStatus(res)));
                  if (PQresultStatus(res) != PGRES_TUPLES_OK) {
                    ilog("insert into token transfers failed ");
                    PQclear(res);
                    PQfinish(conn);
                    return;
                  }
                  PQclear(res);
                } //end if action is transfer
                 else if ((actionname == "issue")&&(receiveraccount == "fio.token")){
                  string payeracct = "eosio";               
                  string payeeacct = getjsonstring(UNKNOWN_STRING,(rapidjson::Value&)actdata["to"],DISALLOW_EMPTY_VALUES);
                  string memo = getjsonstring(UNKNOWN_STRING,(rapidjson::Value&)actdata["memo"],DISALLOW_EMPTY_VALUES);
                  string TRNSTYPETOKENMINT = "token_mint";
                  string sufamount = getjsonstring(UNKNOWN_STRING,(rapidjson::Value&)actdata["quantity"],DISALLOW_EMPTY_VALUES);
                  sufamount.erase(std::remove(sufamount.begin(), sufamount.end(), '.'), sufamount.end());
                  string substringToRemove = " FIO";
                  size_t position = sufamount.find(substringToRemove);
                  if (position != std::string::npos) {
                      sufamount.erase(position, substringToRemove.length());
                  }
 ilog("EDEDEDEDEDEDEDED key_to_account account ${s}",("s",payeeacct));
                  string insertQuery = "SELECT instokentransfers("+
                       boost::lexical_cast<std::string>(fktransactionid)+","+
                      bnums+",'"+
                      payeracct+"','"+
                      payeeacct+"',"+
                      sufamount+",'"+
                      TRNSTYPETOKENMINT +"','"+
                      memo+"','"+
                      blocktimestamp+"');";
                      
                  ilog("EDEDEDEDEDEDEDED ins tokentransfers ${s}",("s",insertQuery));
                  PGresult *res = PQexec(conn, insertQuery.c_str());
                  ilog("ins tokentransfers result status ${r} ",("r",PQresultStatus(res)));
                  if (PQresultStatus(res) != PGRES_TUPLES_OK) {
                    ilog("insert into token transfers failed ");
                    PQclear(res);
                    PQfinish(conn);
                    return;
                  }
                  PQclear(res);
                } //end if action is issue
               
                else if(actionname ==  "newaccount"){
                   string insertQuery = "SELECT insupdaccounts("+
                      bnums+",'"+
                      actionaccount+"','"+
                      "UNKNOWN','"+
                      blocktimestamp+"');";
                      
                  ilog("EDEDEDEDEDEDEDED ins accounts ${s}",("s",insertQuery));
                  PGresult *res = PQexec(conn, insertQuery.c_str());
                  ilog("ins accounts result status ${r} ",("r",PQresultStatus(res)));
                  if (PQresultStatus(res) != PGRES_TUPLES_OK) {
                    ilog("insert into accounts failed ");
                    PQclear(res);
                    PQfinish(conn);
                    return;
                  }
                  PQclear(res);

                }
                else if (actionname == "bind2eosio"){
                  string accountnm = getjsonstring(UNKNOWN_STRING,(rapidjson::Value&)actdata["account"],ALLOW_EMPTY_VALUES);                 
                  string pubkey = getjsonstring(UNKNOWN_STRING,(rapidjson::Value&)actdata["client_key"],ALLOW_EMPTY_VALUES);
                  string insertQuery = "SELECT insupdaccounts("+
                      bnums+",'"+
                      accountnm+"','"+
                      pubkey+"','"+
                      blocktimestamp+"');";
                      
                  ilog("EDEDEDEDEDEDEDED ins accounts ${s}",("s",insertQuery));
                  PGresult *res = PQexec(conn, insertQuery.c_str());
                  ilog("ins transaction result status ${r} ",("r",PQresultStatus(res)));
                  if (PQresultStatus(res) != PGRES_TUPLES_OK) {
                    ilog("insert into accounts failed ");
                    PQclear(res);
                    PQfinish(conn);
                    return;
                  }
                  PQclear(res);

                  insertQuery = "SELECT insaccountactivities("+
                      boost::lexical_cast<std::string>(fktransactionid)+","+
                      bnums+",'"+
                      accountnm+"','"+
                      +"receiver');";
                      
                  ilog("EDEDEDEDEDEDEDED ins account activities ${s}",("s",insertQuery));
                  res = PQexec(conn, insertQuery.c_str());
                  ilog("ins account activities result status ${r} ",("r",PQresultStatus(res)));
                  if (PQresultStatus(res) != PGRES_TUPLES_OK) {
                    ilog("insert into account activities failed ");
                    PQclear(res);
                    PQfinish(conn);
                    return;
                  }
                  PQclear(res);
                } //end if action is bind2eosio

                string outs = "traceid '"+boost::lexical_cast<std::string>(fktransactionid)+"'"+
                "actionordinal '"+actionordinal+"'"+
                "receiveraccount '"+receiveraccount+"'"+
                "contractaccount '"+contractaccount+"'"+
                "actionname '"+actionname+"'"+
                "actionaccount '"+actionaccount+"'";
              //  "tpid '"+tpid+"'"+
              //  "requestdata '"+requestdata+"'";

               ilog("EDEDEDEDEDEDEDEDEDEDEDEDED    ${s}",("s",outs));
               //now check the action name, do relic action triggers for the action name.


            }
          }
          else if(msgtype == "FORK" ) {
                //TODO:  rollback code goes here!!!!
                 string insertQuery = "SELECT rbfork("+bnums+");";
            ilog("EDEDEDEDEDEDEDED del fork ${s}",("s",insertQuery));
           
            PGresult *res = PQexec(conn, insertQuery.c_str());
            ilog("fork delete result status ${r} ",("r",PQresultStatus(res)));
            if (PQresultStatus(res) != PGRES_TUPLES_OK) {
                PQclear(res);
                PQfinish(conn);
                return;
            }

            PQclear(res);
                ack_block(bnum - 1);
                ilog("EDEDEDEDEDED ack block called ");
          }
          else { //TODO check if the event is in the list of events handled by RELIC then process as per relic{
            ilog("UnAcknowledged EVENT ${e}",("e",msgtype));
          }
        } 
      }
       async_send_events();
    }

   

  }


  inline void push_msg(std::shared_ptr<msgbuf> buf) {
    async_queue.push(buf);
    if( pause_time_msec > 0 )
      mytimer->cancel();
    msg_report_counter--;
    if( msg_report_counter == 0 ) {
      ilog("exp_relic_plugin queue_size=${q}", ("q",async_queue.size()));
      msg_report_counter = 10000;
    }
  }


  void on_event_json(const char* msgtype, std::shared_ptr<string> event) {
    try {
      try {
        json_buffer.Clear();
        json_writer.Reset(json_buffer);
        json_writer.StartObject();
        json_writer.Key("msgtype");
        json_writer.String(msgtype);
        json_writer.Key("data");
        json_writer.RawValue(event->data(), event->length(), rapidjson::kObjectType);
        json_writer.EndObject();

        size_t sz = json_buffer.GetSize();
        string msg(json_buffer.GetString());
        auto buf = std::make_shared<msgbuf>(sz);
        memcpy(buf->data(), msg.data(), sz);
        push_msg(buf);
      }
      FC_LOG_AND_RETHROW();
    }
    catch (...) {
      abort_receiver();
    }
  }


  void on_event_bin(int32_t msgtype, int32_t msgopts, std::shared_ptr<string> event) {
    try {
      try {
        auto buf = std::make_shared<msgbuf>(event->length()+sizeof(msgtype)+sizeof(msgopts));
        unsigned char *ptr = buf->data();
        memcpy(ptr, &msgtype, sizeof(msgtype));
        ptr += sizeof(msgtype);
        memcpy(ptr, &msgopts, sizeof(msgopts));
        ptr += sizeof(msgopts);
        memcpy(ptr, event->data(), event->length());
        push_msg(buf);
      }
      FC_LOG_AND_RETHROW();
    }
    catch (...) {
      abort_receiver();
    }
  }

};



exp_relic_plugin::exp_relic_plugin() :my(new exp_relic_plugin_impl){
}

exp_relic_plugin::~exp_relic_plugin(){
}


void exp_relic_plugin::set_program_options( options_description& cli, options_description& cfg ) {
  cfg.add_options()
    (RELIC_HOST_OPT, bpo::value<string>(), "Websocket server host to connect to")
    (RELIC_PORT_OPT, bpo::value<string>(), "Websocket server port to connect to")
    (RELIC_PATH_OPT, bpo::value<string>()->default_value("/"), "Websocket server URL path")
    (RELIC_MAXUNACK_OPT, bpo::value<uint32_t>()->default_value(1000),
     "Receiver will pause at so many unacknowledged blocks")
    (RELIC_MAXQUEUE_OPT, bpo::value<uint32_t>()->default_value(10000),
     "Receiver will pause if outbound queue exceeds this limit")
    (RELIC_BINHDR, bpo::value<bool>()->default_value(false),
     "Start export messages with 32-bit native msgtype,msgopt")
    ;
}


void exp_relic_plugin::plugin_initialize( const variables_map& options ) {
  if (is_noexport_opt(options))
    return;

  try {
    app().get_plugin("decoder_plugin").initialize(options);
    donot_start_receiver_before(this, "exp_relic_plugin");

    bool opt_missing = false;
    if( options.count(RELIC_HOST_OPT) != 1 ) {
      elog("${o} not specified, as required by exp_relic_plugin", ("o",RELIC_HOST_OPT));
      opt_missing = true;
    }
    if( options.count(RELIC_PORT_OPT) != 1 ) {
      elog("${o} not specified, as required by exp_relic_plugin", ("o",RELIC_PORT_OPT));
      opt_missing = true;
    }

    if( opt_missing )
      throw std::runtime_error("Mandatory option missing");

    my->relic_host = options.at(RELIC_HOST_OPT).as<string>();
    my->relic_port = options.at(RELIC_PORT_OPT).as<string>();
    my->relic_path = options.at(RELIC_PATH_OPT).as<string>();

    my->maxunack = options.at(RELIC_MAXUNACK_OPT).as<uint32_t>();
    if( my->maxunack == 0 )
      throw std::runtime_error("Maximum unacked blocks must be a positive integer");

    my->queue_hwm = options.at(RELIC_MAXQUEUE_OPT).as<uint32_t>();
    if( my->queue_hwm == 0 )
      throw std::runtime_error("Maximum queue size must be a positive integer");
    my->queue_lwm = my->queue_hwm * 3 / 4;

    my->use_bin_headers = options.at(RELIC_BINHDR).as<bool>();

    my->init();
    ilog("Initialized exp_relic_plugin");
    exporter_initialized();
  }
  FC_LOG_AND_RETHROW();
}


void exp_relic_plugin::plugin_startup(){
  if (!is_noexport_mode()) {
    my->start();
    ilog("Started exp_relic_plugin");
  }
}

void exp_relic_plugin::plugin_shutdown() {
  if (!is_noexport_mode()) {
    ilog("exp_relic_plugin stopped");
  }
}
