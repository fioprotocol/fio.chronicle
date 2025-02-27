

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
  const char* RELIC_USER_OPT = "exp-relic-username";
  const char* RELIC_PASSWORD_OPT = "exp-relic-password";
  const char* RELIC_DB_OPT = "exp-relic-db";
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
  string relic_user;
  string relic_password;
  string relic_db;
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
  std::vector<string> domainjsons;
  std::vector<string> handlejsons;
  std::vector<std::string> burnaddresses;
  bool burnexpiredthisblock = false;
  uint64_t burnexpiredtrid = 0;
  string burnexpiredtimestamp = "";

  uint32_t pause_time_msec = 0;
  uint32_t msg_report_counter = 1000;

  PGconn *conn;

  exp_relic_plugin_impl() :
    _interactive_requests_chan(app().get_channel<chronicle::channels::interactive_requests>())
  {};

  void init() {
    mytimer = std::make_shared<boost::asio::deadline_timer>(app().get_io_service());

    //connect to postgres relic db
    string conninfo = "dbname="+relic_db+" user="+relic_user+" password="+relic_password+" host="+relic_host+" port="+relic_port;
    conn = PQconnectdb(conninfo.c_str());

    if (PQstatus(conn) != CONNECTION_OK) {
        ilog("failed to connect to relic database exp_relic_plugin will now terminate.");
        PQfinish(conn);
        return;
    }

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



  void  terminalerror(
      string uniqueident,
      string querystr,
      PGconn *dbconn,
      PGresult *results){
      ilog ("Error unique ident -- ${i}",("i",uniqueident));
      ilog("Error during -- ${s}",("s",querystr));
      ilog("Error result status -- ${r} ",("r",boost::lexical_cast<std::string>(PQresultStatus(results))));
      ilog("Error -- exp_relic_plugin execution will be terminated. "); 
      PQclear(results);
      PQfinish(dbconn);
      throw(new std::runtime_error("Unexpected error in exporter, receiver will shut down."));
  }

//send events to the relic database here.
//relic
  void async_send_events() {
    
    try {
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
    //  rapidjson::StringBuffer strbuf;
    //   strbuf.Clear();

    //   rapidjson::Writer<rapidjson::StringBuffer> writer(strbuf);
    //   document.Accept(writer);
    //   std::string dString = strbuf.GetString();
    //   ilog(" document looks like ${s}",("s",dString));

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
       //  rapidjson::StringBuffer buffer;
       //  rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
        // Serialize the document to JSON
       // document.Accept(writer);
        const string msgtype = document["msgtype"].GetString();
        const rapidjson::Value& dataj = document["data"];
        if (
            dataj.HasMember("block_num") &&
            dataj["block_num"].IsString()
    
        ) {
        //  ilog(" msgtype ${m}",("m",msgtype));
          uint32_t bnum =static_cast<uint32_t>(std::stoul( document["data"]["block_num"].GetString()));
          string bnums =document["data"]["block_num"].GetString();
          if (msgtype == "BLOCK_COMPLETED"){

               if (burnexpiredthisblock){
                   //first process the list of domains that may have been burnt
                 if(!(domainjsons.empty()))     {     
                   for (const string& datastr : domainjsons) {
                     rapidjson::Document object;
                      object.Parse((const char*)datastr.c_str(),datastr.length());
                    
                      string domname = getjsonstring(UNKNOWN_STRING,(rapidjson::Value&)object["kvo"]["value"]["name"],ALLOW_EMPTY_VALUES);
                      string ispublic = getjsonstring(UNKNOWN_STRING,(rapidjson::Value&)object["kvo"]["value"]["is_public"],ALLOW_EMPTY_VALUES);                                
                      string expiration = getjsonstring(UNKNOWN_TIMESTAMP,(rapidjson::Value&)object["kvo"]["value"]["expiration"],ALLOW_EMPTY_VALUES);
                      string domainbnum = getjsonstring(UNKNOWN_STRING,(rapidjson::Value&)object["block_num"],ALLOW_EMPTY_VALUES);
                 
                    if(bnums == domainbnum){ 
                          int64_t updburntres = 0;
                          string insertQuery = "SELECT upddomainburnt('"+
                              domname +"');";
                              
                          PGresult *res = PQexec(conn, insertQuery.c_str());
                          if (PQresultStatus(res) != PGRES_TUPLES_OK) {
                             terminalerror("burndomain",insertQuery,conn,res);
                             return;
                          }
                           if (PQgetvalue(res, 0, 0)) {
                                    updburntres = atoi(PQgetvalue(res, 0, 0));
                                }
                                
                          PQclear(res);

                          if (updburntres == 1){
                           
                              string DOMAINACTIVITYAUTOBURN = "auto_burn";
                              insertQuery = "SELECT insdomainactivities("+
                              boost::lexical_cast<std::string>(burnexpiredtrid)+","+
                                  bnums+",'"+
                                  domname+"','"+
                                  DOMAINACTIVITYAUTOBURN+"','"+
                                  burnexpiredtimestamp+"');";
                                  
                              res = PQexec(conn, insertQuery.c_str());
                              if (PQresultStatus(res) != PGRES_TUPLES_OK) {
                               terminalerror("burndomainactivity",insertQuery,conn,res);
                                return;
                              }
                              PQclear(res);
                          }
                      }
                     
                   }
                  }
                  

                if(! (handlejsons.empty())){
                  for (const string& datastr : handlejsons) {

                      rapidjson::Document object;
                      object.Parse((const char*)datastr.c_str(),datastr.length());
                    
                      string handle = getjsonstring(UNKNOWN_STRING,(rapidjson::Value&)object["kvo"]["value"]["name"],ALLOW_EMPTY_VALUES);
                      string expiration = getjsonstring(UNKNOWN_TIMESTAMP,(rapidjson::Value&)object["kvo"]["value"]["expiration"],ALLOW_EMPTY_VALUES);
                      string bundlecount = getjsonstring(UNKNOWN_STRING,(rapidjson::Value&)object["kvo"]["value"]["bundleeligiblecountdown"],ALLOW_EMPTY_VALUES);
                      string handlebnum = getjsonstring(UNKNOWN_STRING,(rapidjson::Value&)object["block_num"],ALLOW_EMPTY_VALUES);

                    //if this handle is burnaddress then skip it
                    if (std::find(burnaddresses.begin(), burnaddresses.end(), handle) != burnaddresses.end()){
                      //this handle was burnt with burnaddress, do not process.
                      continue;
                    }
                    if(bnums == handlebnum){ 

                           //process array of addresses.
                            const rapidjson::Value& dvaddresses = document["kvo"]["value"]["addresses"];
                            bool makehandleburnt = true;
                            for (const auto& object : dvaddresses.GetArray()) {
                               if (!object.IsObject()) {
                                  std::cerr << "Error pub addresses: Element in array is not an object." << std::endl;
                                  continue;
                                }
                                string tokencode = getjsonstring(UNKNOWN_NUMBER,(rapidjson::Value&)object["token_code"],DISALLOW_EMPTY_VALUES);
                                string chaincode = getjsonstring(UNKNOWN_NUMBER,(rapidjson::Value&)object["chain_code"],DISALLOW_EMPTY_VALUES);
                                string publicaddress = getjsonstring(UNKNOWN_NUMBER,(rapidjson::Value&)object["public_address"],DISALLOW_EMPTY_VALUES);
                             

                                int64_t hasparesult = 0;
                                string insertQuery = "SELECT existspubaddress('"+
                                    handle +"','" +
                                    chaincode +"','" +
                                    tokencode +"','" +
                                    publicaddress +"');";
                                    
                                PGresult *res = PQexec(conn, insertQuery.c_str());
                                if (PQresultStatus(res) != PGRES_TUPLES_OK) {
                                 terminalerror("burndomainpubaddrexists",insertQuery,conn,res);
                                 
                                  return;
                                }

                                if (PQgetvalue(res, 0, 0)) {
                                    hasparesult = atoi(PQgetvalue(res, 0, 0));
                                }
                                if (hasparesult == 0){
                                  //do not burn this handle!!!
                                  makehandleburnt = false;
                                  break;
                                }
                                PQclear(res);
                          } //end for addresses

                          if (makehandleburnt) {
                             int64_t updhandleres =0;
                             string insertQuery = "SELECT updhandleburnt("+
                                    bnums+",'"+
                                    handle  +"');";
                                    
                                PGresult *res = PQexec(conn, insertQuery.c_str());
                                if (PQresultStatus(res) != PGRES_TUPLES_OK) {
                                  terminalerror("burnhandle",insertQuery,conn,res);
                                  return;
                                }

                                if (PQgetvalue(res, 0, 0)) {
                                    updhandleres = atoi(PQgetvalue(res, 0, 0));
                                }
                               
                                PQclear(res);
                                 if (updhandleres == 1){
                                    string HANDLECTIVITYAUTOBURN = "auto_burn";
                                     insertQuery = "SELECT inshandleactivities("+
                                      boost::lexical_cast<std::string>(burnexpiredtrid)+","+
                                          bnums+",'"+
                                          handle+"','"+
                                          HANDLECTIVITYAUTOBURN+"','"+
                                          burnexpiredtimestamp+"');";
                                          
                                      res = PQexec(conn, insertQuery.c_str());
                                      if (PQresultStatus(res) != PGRES_TUPLES_OK) {
                                        terminalerror("burnhandleactivity",insertQuery,conn,res);
                                        return;
                                      }
                                      PQclear(res);
                                }
                          } 
                      }
                   }
                  }
                  
          }

           domainjsons.clear();
           handlejsons.clear();
           burnexpiredthisblock = false;
           burnexpiredtrid = 0;
           burnexpiredtimestamp = "";
           burnaddresses.clear();



           
         
          ack_block(bnum -1);
          }
          else if (msgtype == "BLOCK"){
           string btimestamp = getjsonstring(UNKNOWN_TIMESTAMP,document["data"]["block"]["timestamp"],DISALLOW_EMPTY_VALUES);
           string bproducer = getjsonstring(UNKNOWN_STRING,document["data"]["block"]["producer"],DISALLOW_EMPTY_VALUES);
           string bschedv = getjsonstring(UNKNOWN_STRING,document["data"]["block"]["schedule_version"],DISALLOW_EMPTY_VALUES);
           string bid = getjsonstring(UNKNOWN_STRING,document["data"]["block_id"],DISALLOW_EMPTY_VALUES);
          
           string insertQuery = "SELECT insblocks("+bnums+",'"+btimestamp+"','"+bid+"','"+bproducer+"','"+bschedv+"');";
           
            PGresult *res = PQexec(conn, insertQuery.c_str());
            if (PQresultStatus(res) != PGRES_TUPLES_OK) {
              terminalerror("insblock",insertQuery,conn,res);
                return;
            }

            PQclear(res);
          }
           else if (msgtype == "TX_TRACE"){

            string blocktimestamp =getjsonstring(UNKNOWN_TIMESTAMP,document["data"]["block_timestamp"],DISALLOW_EMPTY_VALUES);
          
            //gotta parse the transaction info.
            string trid =getjsonstring(UNKNOWN_STRING,document["data"]["trace"]["id"],DISALLOW_EMPTY_VALUES);
            string status =getjsonstring(UNKNOWN_STRING,document["data"]["trace"]["status"],DISALLOW_EMPTY_VALUES);
           
            const rapidjson::Value& dvtrace = document["data"]["trace"]["action_traces"];
            // string tracesstr = getjsonstring(UNKNOWN_STRING,(rapidjson::Value&)document["data"]["trace"],ALLOW_EMPTY_VALUES);
             //    ilog(" actdata looks like ${d}",("d",tracesstr)); 
             int64_t fktransactionid = -1; //index of transactionid
              for (const auto& object : dvtrace.GetArray()) {
                if (!object.IsObject()) {
                    std::cerr << "Error action traces: Element in array is not an object." << std::endl;
                    continue;
                }

                string actionordinal = getjsonstring(UNKNOWN_NUMBER,(rapidjson::Value&)object["action_ordinal"],DISALLOW_EMPTY_VALUES);
                string creatoractionordinal = getjsonstring(UNKNOWN_NUMBER,(rapidjson::Value&)object["creator_action_ordinal"],DISALLOW_EMPTY_VALUES);
               
                string response = getjsonstring(UNKNOWN_STRING,(rapidjson::Value&)object["receipt"]["response"],ALLOW_EMPTY_VALUES);
             // ilog(" act ordinal ${o}",("o",actionordinal));
                int64_t iactordinal =  -1;
                if(!(actionordinal == UNKNOWN_NUMBER)){
                  iactordinal = atoi(actionordinal.c_str());
                }
                 int64_t icactordinal =  -1;
                if(!(creatoractionordinal == UNKNOWN_NUMBER)){
                  icactordinal = atoi(creatoractionordinal.c_str());
                }
                string receiveraccount = getjsonstring(UNKNOWN_STRING,(rapidjson::Value&)object["receiver"],DISALLOW_EMPTY_VALUES);
                string contractaccount = getjsonstring(UNKNOWN_STRING,(rapidjson::Value&)object["act"]["account"],DISALLOW_EMPTY_VALUES);
                string actionname = getjsonstring(UNKNOWN_STRING,(rapidjson::Value& )object["act"]["name"],DISALLOW_EMPTY_VALUES);
                const rapidjson::Value& actdata = object["act"]["data"];
                rapidjson::Document respdoc;
                respdoc.Parse((const char*)response.c_str());
                string feeamount = getjsonstring(UNKNOWN_NUMBER,(rapidjson::Value&)respdoc["fee_collected"],ALLOW_EMPTY_VALUES);
              
                const rapidjson::Value& arrayauth = object["act"]["authorization"];
                rapidjson::Value& firstauth = (rapidjson::Value&)object;
                
                 if (arrayauth.IsArray() && (arrayauth.Size() > 0)){
                   firstauth = (rapidjson::Value&)arrayauth[0];
                 }

                 
                 string actionaccount = getjsonstring(UNKNOWN_STRING,(rapidjson::Value&)firstauth["actor"],DISALLOW_EMPTY_VALUES);
                 string tpid = "UNKNOWN";
                 if(actdata.IsObject()){
                  tpid = getjsonstring(UNKNOWN_STRING,(rapidjson::Value&)actdata["fio_address"],ALLOW_EMPTY_VALUES);
                 }
                 string requestdata = getjsonstring(UNKNOWN_STRING,(rapidjson::Value&)actdata,ALLOW_EMPTY_VALUES);
                 string maxfee = getjsonstring(UNKNOWN_STRING,(rapidjson::Value&)firstauth["actor"],DISALLOW_EMPTY_VALUES);
  
                if((icactordinal == 0)&&!(actionname == "onblock")&&!(actionname == "nonce")){
                
               
                  string insertQuery = "SELECT instransactions("+
                      bnums+",'"+
                      blocktimestamp+"','"+
                      trid+"','"+
                      contractaccount+"','"+
                       actionaccount+"','"+
                        actionname+"','"+
                        tpid+"',"+
                        feeamount+",'"+
                        requestdata+"','"+
                        response+"','"+
                        status+"');";
                
                  PGresult *res = PQexec(conn, insertQuery.c_str());
                  if (PQresultStatus(res) != PGRES_TUPLES_OK) {
                    terminalerror("instransactions",insertQuery,conn,res);
                    return;
                  }

         
                  if (PQgetvalue(res, 0, 0)) {
                      fktransactionid = atoi(PQgetvalue(res, 0, 0));
                  }

                  PQclear(res);

                 //do relic fio transaction relating actions.
               
                 if ((actionname == "trnsfiopubky")||(actionname == "trnsloctoks")){
                  string payeracct = getjsonstring(UNKNOWN_STRING,(rapidjson::Value&)actdata["actor"],ALLOW_EMPTY_VALUES);                 
                  string pubkey = getjsonstring(UNKNOWN_STRING,(rapidjson::Value&)actdata["payee_public_key"],ALLOW_EMPTY_VALUES);
                  string payeeacct = fioio::key_to_account(pubkey);
                  string TRNSTYPETRANSFER = "transfer";
                  string TRNSTYPETRANSFERLOCKED = "transfer_locked";
                  string trnstype = TRNSTYPETRANSFER;
                  if(actionname == "trnsloctoks") {
                    trnstype = TRNSTYPETRANSFERLOCKED;
                  }
                  string sufamount = getjsonstring(UNKNOWN_STRING,(rapidjson::Value&)actdata["amount"],ALLOW_EMPTY_VALUES);
                  string insertQuery = "SELECT instokentransfers("+
                       boost::lexical_cast<std::string>(fktransactionid)+","+
                      bnums+",'"+
                      payeracct+"','"+
                      payeeacct+"',"+
                      sufamount+",'"+
                      trnstype +"','"+
                      +"UNKNOWN','"+
                      blocktimestamp+"');";
                      
                  PGresult *res = PQexec(conn, insertQuery.c_str());
                  if (PQresultStatus(res) != PGRES_TUPLES_OK) {
                    terminalerror("locktokentransfer",insertQuery,conn,res);
                    return;
                  }
                  PQclear(res);
                } //end if action is trnsfiopubky trnsloctok
                else if ((actionname == "regdomain") || (actionname == "regdomadd") ){
                   string domainname;
                   string handle;
                   if(actionname == "regdomain") {
                     handle = "";
                     domainname = getjsonstring(UNKNOWN_STRING,(rapidjson::Value&)actdata["fio_domain"],ALLOW_EMPTY_VALUES);    
                   } else {
                    handle = getjsonstring(UNKNOWN_STRING,(rapidjson::Value&)actdata["fio_address"],ALLOW_EMPTY_VALUES);                                
                 
                    size_t pos = handle.find('@');
                    if (pos != string::npos) {
                      domainname =  handle.substr(pos + 1); 
                    }
                   }                           
                   string actoraccount = getjsonstring(UNKNOWN_STRING,(rapidjson::Value&)actdata["actor"],ALLOW_EMPTY_VALUES);                                
                   
                   string pubkey = getjsonstring(UNKNOWN_STRING,(rapidjson::Value&)actdata["owner_fio_public_key"],ALLOW_EMPTY_VALUES);
                  string owneracct = fioio::key_to_account(pubkey);
                  string expirationtimestamp = getjsonstring(UNKNOWN_TIMESTAMP,(rapidjson::Value&)respdoc["expiration"],ALLOW_EMPTY_VALUES);
                  string ispublic = "false";
                  string domainstatus = "active";
                  string insertQuery = "SELECT insdomains("+
                      bnums+",'"+
                      domainname+"','"+
                      owneracct+"','"+
                      ispublic+"','"+
                      expirationtimestamp+"','"+
                      domainstatus +"');";
                      
                  PGresult *res = PQexec(conn, insertQuery.c_str());
                  if (PQresultStatus(res) != PGRES_TUPLES_OK) {
                    terminalerror("regdomain",insertQuery,conn,res);
                    return;
                  }
                  PQclear(res);

                  //insert domain activities
                  string DOMAINACTIVITYREGISTER = "register";
                  insertQuery = "SELECT insdomainactivities("+
                  boost::lexical_cast<std::string>(fktransactionid)+","+
                      bnums+",'"+
                      domainname+"','"+
                      DOMAINACTIVITYREGISTER+"','"+
                      blocktimestamp+"');";
                      
                  res = PQexec(conn, insertQuery.c_str());
                  if (PQresultStatus(res) != PGRES_TUPLES_OK) {
                    terminalerror("regdomainactivity",insertQuery,conn,res);
                    return;
                  }
                  PQclear(res);


                if(actionname == "regdomadd") {
                   string actoraccount = getjsonstring(UNKNOWN_STRING,(rapidjson::Value&)actdata["actor"],ALLOW_EMPTY_VALUES);                                
                  string pubkey = getjsonstring(UNKNOWN_STRING,(rapidjson::Value&)actdata["owner_fio_public_key"],ALLOW_EMPTY_VALUES);
                  string owneracct = fioio::key_to_account(pubkey);
                  string encryptkeyisset = "false";
                  string bundledtxcount = "100";
                  string expirationtimestamp = getjsonstring(UNKNOWN_TIMESTAMP,(rapidjson::Value&)respdoc["expiration"],ALLOW_EMPTY_VALUES);
                  string HANDLESTATUSACTIVE = "active";
                  string chaincode = "FIO";
                  string tokencode = "FIO";
                  string insertQuery = "SELECT insupdhandles("+
                      bnums+",'"+
                      domainname +"','" +
                      owneracct +"','" +
                      handle +"','" +
                      pubkey +"','" +
                      encryptkeyisset +"'," +
                      bundledtxcount +",'" +
                      expirationtimestamp +"','" +
                      HANDLESTATUSACTIVE +"');";
                      
                  PGresult *res = PQexec(conn, insertQuery.c_str());
                  if (PQresultStatus(res) != PGRES_TUPLES_OK) {
                   terminalerror("regaddress",insertQuery,conn,res);
                    return;
                  }
                  PQclear(res);

                  string HANDLEACTIVITYTYPEREGISTER = "register";
                  
                  insertQuery = "SELECT inshandleactivities("+
                  boost::lexical_cast<std::string>(fktransactionid)+","+
                      bnums+",'"+
                      handle+"','"+
                      HANDLEACTIVITYTYPEREGISTER+"','"+
                      blocktimestamp+"');";
                      
                  res = PQexec(conn, insertQuery.c_str());
                  if (PQresultStatus(res) != PGRES_TUPLES_OK) {
                  terminalerror("regaddresshandleact",insertQuery,conn,res);
                    return;
                  }
                  PQclear(res);

                  insertQuery = "SELECT insupdpubaddresses("+
                      bnums+",'"+
                      handle+"','"+
                       chaincode+"','"+
                        tokencode+"','"+
                         pubkey+"');";
                      
                  res = PQexec(conn, insertQuery.c_str());
                  if (PQresultStatus(res) != PGRES_TUPLES_OK) {
                   terminalerror("regaddressinspubaddr",insertQuery,conn,res);
                    return;
                  }
                  PQclear(res);
                   }   



                  if(!(actoraccount == owneracct)){ //insert account activity. 
                      insertQuery = "SELECT insaccountactivities("+
                      boost::lexical_cast<std::string>(fktransactionid)+","+
                      bnums+",'"+
                      owneracct+"','"+
                      +"receiver');";
                      
                      res = PQexec(conn, insertQuery.c_str());
                      if (PQresultStatus(res) != PGRES_TUPLES_OK) {
                        terminalerror("regdomainaccountactivity",insertQuery,conn,res);
                        return;
                      }
                      PQclear(res);
                  } //end if actor is owner.
                } //end if action is regdomain, regdomadd
                else if ((actionname == "renewdomain")){
                  string domainname = getjsonstring(UNKNOWN_STRING,(rapidjson::Value&)actdata["fio_domain"],ALLOW_EMPTY_VALUES);                                
                 string expirationtimestamp = getjsonstring(UNKNOWN_TIMESTAMP,(rapidjson::Value&)respdoc["expiration"],ALLOW_EMPTY_VALUES);
                
                  string insertQuery = "SELECT upddomainexp('"+
                      domainname+"','"+
                      expirationtimestamp +"');";
                      
                  PGresult *res = PQexec(conn, insertQuery.c_str());
                  if (PQresultStatus(res) != PGRES_TUPLES_OK) {
                    terminalerror("renewdomain",insertQuery,conn,res);
                   return;
                  }
                  PQclear(res);

                  //insert domain activities
                  string DOMAINACTIVITYRENEW = "renew";
                  insertQuery = "SELECT insdomainactivities("+
                  boost::lexical_cast<std::string>(fktransactionid)+","+
                      bnums+",'"+
                      domainname+"','"+
                      DOMAINACTIVITYRENEW+"','"+
                      blocktimestamp+"');";
                      
                  res = PQexec(conn, insertQuery.c_str());
                  if (PQresultStatus(res) != PGRES_TUPLES_OK) {
                    terminalerror("renewdomainactivity",insertQuery,conn,res);
                    return;
                  }
                  PQclear(res);
                } //end if action is renewdomain
                 else if ((actionname == "xferdomain")){
                  string domainname = getjsonstring(UNKNOWN_STRING,(rapidjson::Value&)actdata["fio_domain"],ALLOW_EMPTY_VALUES);                                
                  string pubkey = getjsonstring(UNKNOWN_STRING,(rapidjson::Value&)actdata["new_owner_fio_public_key"],ALLOW_EMPTY_VALUES);
                  string owneracct = fioio::key_to_account(pubkey);
                 
                  string insertQuery = "SELECT upddomainowner('"+
                      domainname+"','"+
                      owneracct +"');";
                      
                  PGresult *res = PQexec(conn, insertQuery.c_str());
                  if (PQresultStatus(res) != PGRES_TUPLES_OK) {
                    terminalerror("xferdomain",insertQuery,conn,res);
                    return;
                  }
                  PQclear(res);

                  //insert domain activities
                  string DOMAINACTIVITYTRANSFER = "transfer";
                  insertQuery = "SELECT insdomainactivities("+
                  boost::lexical_cast<std::string>(fktransactionid)+","+
                      bnums+",'"+
                      domainname+"','"+
                      DOMAINACTIVITYTRANSFER+"','"+
                      blocktimestamp+"');";
                      
                  res = PQexec(conn, insertQuery.c_str());
                  if (PQresultStatus(res) != PGRES_TUPLES_OK) {
                    terminalerror("xferdomainactivity",insertQuery,conn,res);
                    return;
                  }
                  PQclear(res);

                   insertQuery = "SELECT insaccountactivities("+
                      boost::lexical_cast<std::string>(fktransactionid)+","+
                      bnums+",'"+
                      owneracct+"','"+
                      +"receiver');";
                      
                  res = PQexec(conn, insertQuery.c_str());
                  if (PQresultStatus(res) != PGRES_TUPLES_OK) {
                   terminalerror("xferdomainaccountactivity",insertQuery,conn,res);
                    return;
                  }
                  PQclear(res);
                } //end if action is xferdomain
                 else if ((actionname == "setdomainpub")){
                  string domainname = getjsonstring(UNKNOWN_STRING,(rapidjson::Value&)actdata["fio_domain"],ALLOW_EMPTY_VALUES);                                
                  string publicstr = getjsonstring(UNKNOWN_STRING,(rapidjson::Value&)actdata["is_public"],ALLOW_EMPTY_VALUES);
                  
                 
                  string insertQuery = "SELECT upddomainispublic('"+
                      domainname+"','"+
                      publicstr +"');";
                      
                  PGresult *res = PQexec(conn, insertQuery.c_str());
                  if (PQresultStatus(res) != PGRES_TUPLES_OK) {
                   terminalerror("setdomainpub",insertQuery,conn,res);
                    return;
                  }
                  PQclear(res);

                  //insert domain activities
                  string DOMAINACTIVITYPUBLIC = "public";
                   string DOMAINACTIVITYNONPUBLIC = "non-public";
                   string activitytype = DOMAINACTIVITYPUBLIC;
                   if(publicstr =="0"){
                    activitytype = DOMAINACTIVITYNONPUBLIC;
                   }
                  insertQuery = "SELECT insdomainactivities("+
                  boost::lexical_cast<std::string>(fktransactionid)+","+
                      bnums+",'"+
                      domainname+"','"+
                      activitytype+"','"+
                      blocktimestamp+"');";
                      
                  res = PQexec(conn, insertQuery.c_str());
                  if (PQresultStatus(res) != PGRES_TUPLES_OK) {
                   terminalerror("setdomainpubdomainactivity",insertQuery,conn,res);
                    return;
                  }
                  PQclear(res);
                } //end if action is setdomainpub
                 else if ((actionname == "wrapdomain")){
                  string domainname = getjsonstring(UNKNOWN_STRING,(rapidjson::Value&)actdata["fio_domain"],ALLOW_EMPTY_VALUES);                                
                  string acctstr = "fio.oracle";
                 
                  string insertQuery = "SELECT upddomainowner('"+
                      domainname+"','"+
                      acctstr +"');";
                      
                  PGresult *res = PQexec(conn, insertQuery.c_str());
                  if (PQresultStatus(res) != PGRES_TUPLES_OK) {
                   terminalerror("wrapdomain",insertQuery,conn,res);
                    return;
                  }
                  PQclear(res);

                  //insert domain activities
                  string DOMAINACTIVITYWRAP = "wrap";
                  
                  insertQuery = "SELECT insdomainactivities("+
                  boost::lexical_cast<std::string>(fktransactionid)+","+
                      bnums+",'"+
                      domainname+"','"+
                      DOMAINACTIVITYWRAP+"','"+
                      blocktimestamp+"');";
                      
                  res = PQexec(conn, insertQuery.c_str());
                  if (PQresultStatus(res) != PGRES_TUPLES_OK) {
                    terminalerror("wrapdomainactivity",insertQuery,conn,res);
                    return;
                  }
                  PQclear(res);

                  insertQuery = "SELECT insaccountactivities("+
                      boost::lexical_cast<std::string>(fktransactionid)+","+
                      bnums+",'"+
                      acctstr+"','"+
                      +"receiver');";
                      
                  res = PQexec(conn, insertQuery.c_str());
                  if (PQresultStatus(res) != PGRES_TUPLES_OK) {
                   terminalerror("wrapdomainaccountactivity",insertQuery,conn,res);
                    return;
                  }
                  PQclear(res);
                } //end if action is wrapdomain
                  else if ((actionname == "xferescrow")){
                  string domainname = getjsonstring(UNKNOWN_STRING,(rapidjson::Value&)actdata["fio_domain"],ALLOW_EMPTY_VALUES);                                
                  string pubkey = getjsonstring(UNKNOWN_STRING,(rapidjson::Value&)actdata["public_key"],ALLOW_EMPTY_VALUES);
                  string owneracct = fioio::key_to_account(pubkey);
                  string oracleacct = "fio.oracle";
                  string insertQuery = "SELECT upddomainowner('"+
                      domainname+"','"+
                      owneracct +"');";
                      
                  PGresult *res = PQexec(conn, insertQuery.c_str());
                  if (PQresultStatus(res) != PGRES_TUPLES_OK) {
                  terminalerror("xferescrow",insertQuery,conn,res);
                    return;
                  }
                  PQclear(res);

                  //insert domain activities
                  string DOMAINACTIVITYUNWRAP = "unwrap";
                  
                  insertQuery = "SELECT insdomainactivities("+
                  boost::lexical_cast<std::string>(fktransactionid)+","+
                      bnums+",'"+
                      domainname+"','"+
                      DOMAINACTIVITYUNWRAP+"','"+
                      blocktimestamp+"');";
                      
                  res = PQexec(conn, insertQuery.c_str());
                  if (PQresultStatus(res) != PGRES_TUPLES_OK) {
                   terminalerror("xferescrowdomainactivity",insertQuery,conn,res);
                    return;
                  }
                  PQclear(res);

                  insertQuery = "SELECT insaccountactivities("+
                      boost::lexical_cast<std::string>(fktransactionid)+","+
                      bnums+",'"+
                      owneracct+"','"+
                      +"receiver');";
                      
                  res = PQexec(conn, insertQuery.c_str());
                  if (PQresultStatus(res) != PGRES_TUPLES_OK) {
                  terminalerror("xferescrowowneraccountactivity",insertQuery,conn,res);
                    return;
                  }
                  PQclear(res);

                   insertQuery = "SELECT insaccountactivities("+
                      boost::lexical_cast<std::string>(fktransactionid)+","+
                      bnums+",'"+
                      oracleacct+"','"+
                      +"sender');";
                      
                  res = PQexec(conn, insertQuery.c_str());
                  if (PQresultStatus(res) != PGRES_TUPLES_OK) {
                   terminalerror("xferescroworacleaccountactivity",insertQuery,conn,res);
                    return;
                  }
                  PQclear(res);
                } //end if action is xferescrow
                  else if ((actionname == "updcryptkey")){
                  string handle = getjsonstring(UNKNOWN_STRING,(rapidjson::Value&)actdata["fio_address"],ALLOW_EMPTY_VALUES);                                
                  string pubkey = getjsonstring(UNKNOWN_STRING,(rapidjson::Value&)actdata["encrypt_public_key"],ALLOW_EMPTY_VALUES);
                  string insertQuery = "SELECT updhandlessetencryptkey("+
                      bnums+",'"+
                      handle +"','" +
                      pubkey +"');";
                      
                  PGresult *res = PQexec(conn, insertQuery.c_str());
                  if (PQresultStatus(res) != PGRES_TUPLES_OK) {
                   terminalerror("updcryptkey",insertQuery,conn,res);
                    return;
                  }
                  PQclear(res);

                  string HANDLEACTIVITYTYUPDATEENCRYPTKEY = "upd_encryptkey";
                  
                  insertQuery = "SELECT inshandleactivities("+
                  boost::lexical_cast<std::string>(fktransactionid)+","+
                      bnums+",'"+
                      handle+"','"+
                      HANDLEACTIVITYTYUPDATEENCRYPTKEY+"','"+
                      blocktimestamp+"');";
                      
                  res = PQexec(conn, insertQuery.c_str());
                  if (PQresultStatus(res) != PGRES_TUPLES_OK) {
                   terminalerror("updcryptkeyhandleactivity",insertQuery,conn,res);
                    return;
                  }
                  PQclear(res);
                } //end if action is updcryptkey
                 else if ((actionname == "burnexpired")){
                  burnexpiredthisblock = true;
                  burnexpiredtimestamp = blocktimestamp;
                  burnexpiredtrid = fktransactionid;
                 }
                 else if ((actionname == "burnaddress")){
                  string handle = getjsonstring(UNKNOWN_STRING,(rapidjson::Value&)actdata["fio_address"],ALLOW_EMPTY_VALUES);                                
                  string pubkey = getjsonstring(UNKNOWN_STRING,(rapidjson::Value&)actdata["encrypt_public_key"],ALLOW_EMPTY_VALUES);
                  burnaddresses.push_back(handle);
                  string HANDLESTATUSBURNT = "burnt";
                  string insertQuery = "SELECT updhandlesstatus("+
                      bnums+",'"+
                      handle +"','" +
                      HANDLESTATUSBURNT +"');";
                      
                  PGresult *res = PQexec(conn, insertQuery.c_str());
                  if (PQresultStatus(res) != PGRES_TUPLES_OK) {
                   terminalerror("burnaddress",insertQuery,conn,res);
                    return;
                  }
                  PQclear(res);

                  string HANDLEACTIVITYTYSELFBURN = "self_burn";
                  
                  insertQuery = "SELECT inshandleactivities("+
                  boost::lexical_cast<std::string>(fktransactionid)+","+
                      bnums+",'"+
                      handle+"','"+
                      HANDLEACTIVITYTYSELFBURN+"','"+
                      blocktimestamp+"');";
                      
                  res = PQexec(conn, insertQuery.c_str());
                  if (PQresultStatus(res) != PGRES_TUPLES_OK) {
                   terminalerror("burnaddresshandleactivity",insertQuery,conn,res);
                    return;
                  }
                  PQclear(res);

                   insertQuery = "SELECT delpubaddresses('"+
                      handle+"');";
                      
                  res = PQexec(conn, insertQuery.c_str());
                  if (PQresultStatus(res) != PGRES_TUPLES_OK) {
                   terminalerror("burnaddressdelpubaddresses",insertQuery,conn,res);
                    return;
                  }
                  PQclear(res);

                   insertQuery = "SELECT delnftsignatures('"+
                      handle+"');";
                      
                  res = PQexec(conn, insertQuery.c_str());
                  if (PQresultStatus(res) != PGRES_TUPLES_OK) {
                   terminalerror("burnaddressdelnftsigs",insertQuery,conn,res);
                    return;
                  }
                  PQclear(res);



                } //end if action is burnaddress
                 else if ((actionname == "newfundsreq")){
                   string actoraccount = getjsonstring(UNKNOWN_STRING,(rapidjson::Value&)actdata["actor"],ALLOW_EMPTY_VALUES);                                
                  string payerhandle = getjsonstring(UNKNOWN_STRING,(rapidjson::Value&)actdata["payer_fio_address"],ALLOW_EMPTY_VALUES);  
                  string payeehandle = getjsonstring(UNKNOWN_STRING,(rapidjson::Value&)actdata["payee_fio_address"],ALLOW_EMPTY_VALUES);  
                   string content = getjsonstring(UNKNOWN_STRING,(rapidjson::Value&)actdata["content"],ALLOW_EMPTY_VALUES);  
                  string REQUESTSTATUSPENDING = "pending";
                  string HANDLEACTIVITYTYNEWREQUEST = "new_request";
                  string fiochainrequestid = getjsonstring(UNKNOWN_NUMBER,(rapidjson::Value&)respdoc["fio_request_id"],ALLOW_EMPTY_VALUES);
                  
                  string insertQuery = "SELECT inshandleactivities("+
                  boost::lexical_cast<std::string>(fktransactionid)+","+
                      bnums+",'"+
                      payeehandle+"','"+
                      HANDLEACTIVITYTYNEWREQUEST+"','"+
                      blocktimestamp+"');";
                      
                  PGresult *res = PQexec(conn, insertQuery.c_str());
                  if (PQresultStatus(res) != PGRES_TUPLES_OK) {
                   terminalerror("newfundsreq",insertQuery,conn,res);
                    return;
                  }
                  PQclear(res);

                       insertQuery = "SELECT insaccountactivitieshandle("+
                      boost::lexical_cast<std::string>(fktransactionid)+","+
                      bnums+",'"+
                      payerhandle+"','"+
                      +"receiver');";
                      
                      res = PQexec(conn, insertQuery.c_str());
                      if (PQresultStatus(res) != PGRES_TUPLES_OK) {
                       terminalerror("newfundsreqinsaccountacthandle",insertQuery,conn,res);
                        return;
                      }
                      PQclear(res);

                       insertQuery = "SELECT insfiorequests("+
                      boost::lexical_cast<std::string>(fktransactionid)+","+
                      bnums+","+
                      fiochainrequestid+",'"+
                      payerhandle+"','"+
                      payeehandle+"','"+
                      content+"','"+
                      REQUESTSTATUSPENDING+"','"+
                       blocktimestamp+"');";
                     
                      
                      res = PQexec(conn, insertQuery.c_str());
                      if (PQresultStatus(res) != PGRES_TUPLES_OK) {
                        terminalerror("newfundsreqinfiorequests",insertQuery,conn,res);
                        return;
                      }
                      PQclear(res);
                 
                } //end if action is newfundsreq
                 else if ((actionname == "recordobt")){
                  string actoraccount = getjsonstring(UNKNOWN_STRING,(rapidjson::Value&)actdata["actor"],ALLOW_EMPTY_VALUES);                                
                  string payerhandle = getjsonstring(UNKNOWN_STRING,(rapidjson::Value&)actdata["payer_fio_address"],ALLOW_EMPTY_VALUES);  
                  string payeehandle = getjsonstring(UNKNOWN_STRING,(rapidjson::Value&)actdata["payee_fio_address"],ALLOW_EMPTY_VALUES);  
                  string content = getjsonstring(UNKNOWN_STRING,(rapidjson::Value&)actdata["content"],ALLOW_EMPTY_VALUES);  
                  string REQUESTSTATUSSENTTOBC = "sent_to_blockchain";
                  string HANDLEACTIVITYTYRECORDOBT = "record_obt";
                  string fiochainrequestid = getjsonstring("NULL",(rapidjson::Value&)actdata["fio_request_id"],DISALLOW_EMPTY_VALUES);                                
                  string insertQuery = "SELECT inshandleactivities("+
                  boost::lexical_cast<std::string>(fktransactionid)+","+
                      bnums+",'"+
                      payerhandle+"','"+
                      HANDLEACTIVITYTYRECORDOBT+"','"+
                      blocktimestamp+"');";
                      
                  PGresult *res = PQexec(conn, insertQuery.c_str());
                  if (PQresultStatus(res) != PGRES_TUPLES_OK) {
                   terminalerror("recordobt",insertQuery,conn,res);
                    return;
                  }
                  PQclear(res);

                       insertQuery = "SELECT insaccountactivitieshandle("+
                      boost::lexical_cast<std::string>(fktransactionid)+","+
                      bnums+",'"+
                      payeehandle+"','"+
                      +"receiver');";
                      
                      res = PQexec(conn, insertQuery.c_str());
                      if (PQresultStatus(res) != PGRES_TUPLES_OK) {
                       terminalerror("recordobtaccountacthandle",insertQuery,conn,res);
                        return;
                      }
                      PQclear(res);

                       insertQuery = "SELECT insfiodatas("+
                      boost::lexical_cast<std::string>(fktransactionid)+","+
                      bnums+","+
                      fiochainrequestid+",'"+
                      payerhandle+"','"+
                      payeehandle+"','"+
                      content+"','"+
                      REQUESTSTATUSSENTTOBC+"','"+
                       blocktimestamp+"');";
                     
                      
                      res = PQexec(conn, insertQuery.c_str());
                      if (PQresultStatus(res) != PGRES_TUPLES_OK) {
                      terminalerror("recordobtfiodatas",insertQuery,conn,res);
                        return;
                      }
                      PQclear(res);

                 insertQuery = "SELECT updfiorequestsstatus("+
                    fiochainrequestid+",'"+
                    REQUESTSTATUSSENTTOBC+"');";

                  res = PQexec(conn, insertQuery.c_str());
                  if (PQresultStatus(res) != PGRES_TUPLES_OK) {
                   terminalerror("recordobtfioreqstatus",insertQuery,conn,res);
                    return;
                  }
                  PQclear(res);
                } //end if action is recordobt
                 else if ((actionname == "cancelfndreq")){
                    string REQUESTSTATUSCANCEL = "cancelled";
                  string HANDLEACTIVITYTYCANCELREQUEST = "cancel_request";
                  string fiochainrequestid = getjsonstring(UNKNOWN_STRING,(rapidjson::Value&)actdata["fio_request_id"],ALLOW_EMPTY_VALUES);  
                            
                  string insertQuery = "SELECT inshandleactivitiesfiorequest("+
                  boost::lexical_cast<std::string>(fktransactionid)+","+
                      bnums+","+
                      fiochainrequestid+",'"+
                      HANDLEACTIVITYTYCANCELREQUEST+"','"+
                      blocktimestamp+"');";

                   //   ilog(" ins handle activities looks like ${d}",("d",insertQuery));  
                  PGresult *res = PQexec(conn, insertQuery.c_str());
                  if (PQresultStatus(res) != PGRES_TUPLES_OK) {
                   terminalerror("cancelfndreqhandleact",insertQuery,conn,res);
                    return;
                  }
                  PQclear(res);
                
                  
                  insertQuery = "SELECT updfiorequestsstatus("+
                    fiochainrequestid+",'"+
                    REQUESTSTATUSCANCEL+"');";
                  res = PQexec(conn, insertQuery.c_str());
                  if (PQresultStatus(res) != PGRES_TUPLES_OK) {
                   terminalerror("cancelfundreqfioreqstatus",insertQuery,conn,res);
                    return;
                  }
                  PQclear(res);
                 
                } //end if action is cancelfndreq
                 else if ((actionname == "regaddress")){
                  string handle = getjsonstring(UNKNOWN_STRING,(rapidjson::Value&)actdata["fio_address"],ALLOW_EMPTY_VALUES);                                
                 string domain = "";
                 size_t pos = handle.find('@');
                  if (pos != string::npos) {
                   domain =  handle.substr(pos + 1); 
                  }
                  string actoraccount = getjsonstring(UNKNOWN_STRING,(rapidjson::Value&)actdata["actor"],ALLOW_EMPTY_VALUES);                                
                  string pubkey = getjsonstring(UNKNOWN_STRING,(rapidjson::Value&)actdata["owner_fio_public_key"],ALLOW_EMPTY_VALUES);
                  string owneracct = fioio::key_to_account(pubkey);
                  string encryptkeyisset = "false";
                  string bundledtxcount = "100";
                  string expirationtimestamp = getjsonstring(UNKNOWN_TIMESTAMP,(rapidjson::Value&)respdoc["expiration"],ALLOW_EMPTY_VALUES);
                  string HANDLESTATUSACTIVE = "active";
                  string chaincode = "FIO";
                  string tokencode = "FIO";
                  string insertQuery = "SELECT insupdhandles("+
                      bnums+",'"+
                      domain +"','" +
                      owneracct +"','" +
                      handle +"','" +
                      pubkey +"','" +
                      encryptkeyisset +"'," +
                      bundledtxcount +",'" +
                      expirationtimestamp +"','" +
                      HANDLESTATUSACTIVE +"');";
                      
                  PGresult *res = PQexec(conn, insertQuery.c_str());
                  if (PQresultStatus(res) != PGRES_TUPLES_OK) {
                   terminalerror("regaddress",insertQuery,conn,res);
                    return;
                  }
                  PQclear(res);

                  string HANDLEACTIVITYTYPEREGISTER = "register";
                  
                  insertQuery = "SELECT inshandleactivities("+
                  boost::lexical_cast<std::string>(fktransactionid)+","+
                      bnums+",'"+
                      handle+"','"+
                      HANDLEACTIVITYTYPEREGISTER+"','"+
                      blocktimestamp+"');";
                      
                  res = PQexec(conn, insertQuery.c_str());
                  if (PQresultStatus(res) != PGRES_TUPLES_OK) {
                  terminalerror("regaddresshandleact",insertQuery,conn,res);
                    return;
                  }
                  PQclear(res);
                
                   if(!(actoraccount == owneracct)){ //insert account activity. 
                      insertQuery = "SELECT insaccountactivities("+
                      boost::lexical_cast<std::string>(fktransactionid)+","+
                      bnums+",'"+
                      owneracct+"','"+
                      +"receiver');";
                      
                      res = PQexec(conn, insertQuery.c_str());
                      if (PQresultStatus(res) != PGRES_TUPLES_OK) {
                       terminalerror("regaddressaccountact",insertQuery,conn,res);
                        return;
                      }
                      PQclear(res);
                  } //end if actor is owner.

                
                   insertQuery = "SELECT insupdpubaddresses("+
                      bnums+",'"+
                      handle+"','"+
                       chaincode+"','"+
                        tokencode+"','"+
                         pubkey+"');";
                      
                  res = PQexec(conn, insertQuery.c_str());
                  if (PQresultStatus(res) != PGRES_TUPLES_OK) {
                   terminalerror("regaddressinspubaddr",insertQuery,conn,res);
                    return;
                  }
                  PQclear(res);
                } //end if action is regaddress
                else if ((actionname == "renewaddress")){
                  string handle = getjsonstring(UNKNOWN_STRING,(rapidjson::Value&)actdata["fio_address"],ALLOW_EMPTY_VALUES);                                
                  string expirationtimestamp = getjsonstring(UNKNOWN_TIMESTAMP,(rapidjson::Value&)respdoc["expiration"],ALLOW_EMPTY_VALUES);
                  string HANDLEACTIVITYRENEW = "renew";
                  string chaincode = "FIO";
                  string tokencode = "FIO";
                  string insertQuery = "SELECT updhandlesrenewbundles("+
                       bnums+",'"+
                      handle +"','" +
                      expirationtimestamp +"');";
                      
                  PGresult *res = PQexec(conn, insertQuery.c_str());
                  if (PQresultStatus(res) != PGRES_TUPLES_OK) {
                   terminalerror("renewaddr",insertQuery,conn,res);
                    return;
                  }
                  PQclear(res);

                  string HANDLEACTIVITYTYPEREGISTER = "register";
                  
                  insertQuery = "SELECT inshandleactivities("+
                  boost::lexical_cast<std::string>(fktransactionid)+","+
                      bnums+",'"+
                      handle+"','"+
                      HANDLEACTIVITYRENEW+"','"+
                      blocktimestamp+"');";
                      
                  res = PQexec(conn, insertQuery.c_str());
                  if (PQresultStatus(res) != PGRES_TUPLES_OK) {
                   terminalerror("renewaddrhandleact",insertQuery,conn,res);
                    return;
                  }
                  PQclear(res);
                } //end if action is renewaddress
                 else if ((actionname == "addbundles")){
                  string handle = getjsonstring(UNKNOWN_STRING,(rapidjson::Value&)actdata["fio_address"],ALLOW_EMPTY_VALUES);                                
                  string bundlesetss = getjsonstring(UNKNOWN_STRING,(rapidjson::Value&)actdata["bundle_sets"],ALLOW_EMPTY_VALUES);                                
                 
                 
                  string HANDLEACTIVITYADDBUNDLES = "add_bundles";
                  string chaincode = "FIO";
                  string tokencode = "FIO";
                  string insertQuery = "SELECT updhandlesaddbundles("+
                       bnums+",'"+
                      handle +"'," +
                      bundlesetss +");";
                      
                  PGresult *res = PQexec(conn, insertQuery.c_str());
                  if (PQresultStatus(res) != PGRES_TUPLES_OK) {
                   terminalerror("addbundles",insertQuery,conn,res);
                    return;
                  }
                  PQclear(res);
                  
                  insertQuery = "SELECT inshandleactivities("+
                  boost::lexical_cast<std::string>(fktransactionid)+","+
                      bnums+",'"+
                      handle+"','"+
                      HANDLEACTIVITYADDBUNDLES+"','"+
                      blocktimestamp+"');";
                      
                  res = PQexec(conn, insertQuery.c_str());
                  if (PQresultStatus(res) != PGRES_TUPLES_OK) {
                   terminalerror("addbundleshandleact",insertQuery,conn,res);
                    return;
                  }
                  PQclear(res);
                } //end if action is addbundles
                else if ((actionname == "addnft")){
                  string handle = getjsonstring(UNKNOWN_STRING,(rapidjson::Value&)actdata["fio_address"],ALLOW_EMPTY_VALUES);                                
                  
                  string HANDLEACTIVITYADDNFT = "add_nft";
                  string chaincode = getjsonstring(UNKNOWN_STRING,(rapidjson::Value&)actdata["fio_address"],ALLOW_EMPTY_VALUES);                                
                  
                  string tokencode = "FIO";


                  string insertQuery = "SELECT inshandleactivities("+
                  boost::lexical_cast<std::string>(fktransactionid)+","+
                      bnums+",'"+
                      handle+"','"+
                      HANDLEACTIVITYADDNFT+"','"+
                      blocktimestamp+"');";
                      
                  PGresult *res = PQexec(conn, insertQuery.c_str());
                  if (PQresultStatus(res) != PGRES_TUPLES_OK) {
                    terminalerror("addnft",insertQuery,conn,res);
                    return;
                  }
                  PQclear(res);
                
                   const rapidjson::Value& dvtrace = actdata["nfts"];
                  for (const auto& object : dvtrace.GetArray()) {
                      if (!object.IsObject()) {
                         std::cerr << "Error nfts: Element in array is not an object." << std::endl;
                         PQfinish(conn); //TODO -- cleaner exit.
                      }
                      const rapidjson::Value& nftdata = object;

                    if (nftdata.IsObject())
                    {
                      
                      string nftdatastr = getjsonstring(UNKNOWN_STRING,(rapidjson::Value&)nftdata,ALLOW_EMPTY_VALUES);
                      string chaincode = getjsonstring(UNKNOWN_STRING,(rapidjson::Value&)nftdata["chain_code"],ALLOW_EMPTY_VALUES);                                
                      string contractaddress = getjsonstring(UNKNOWN_STRING,(rapidjson::Value&)nftdata["contract_address"],ALLOW_EMPTY_VALUES);                                
                      string tokenid = getjsonstring(UNKNOWN_STRING,(rapidjson::Value&)nftdata["token_id"],ALLOW_EMPTY_VALUES);                                
                    string url = getjsonstring(UNKNOWN_STRING,(rapidjson::Value&)nftdata["url"],ALLOW_EMPTY_VALUES);                                
                    string hash = getjsonstring(UNKNOWN_STRING,(rapidjson::Value&)nftdata["hash"],ALLOW_EMPTY_VALUES);                                
                    string metadata = getjsonstring(UNKNOWN_STRING,(rapidjson::Value&)nftdata["metadata"],ALLOW_EMPTY_VALUES);                                
 
                        insertQuery = "SELECT insupdnftsignatures("+
                          bnums+",'"+
                          handle+"','"+
                          chaincode+"','"+
                          contractaddress+"','"+
                            tokenid+"','"+
                            url+"','"+
                            hash+"','"+
                            metadata+"');";
                          
                      res = PQexec(conn, insertQuery.c_str());
                      if (PQresultStatus(res) != PGRES_TUPLES_OK) {
                       terminalerror("addnftinsupnftsigs",insertQuery,conn,res);
                        return;
                      }
                      PQclear(res);

                      
                    }else {
                      ilog ("FATAL error -- nfts parse error!!!");
                      PQfinish(conn);
                    }
                  } //end loop over nfts.
                } //end if action is addnft
                 else if ((actionname == "remnft")){
                  string handle = getjsonstring(UNKNOWN_STRING,(rapidjson::Value&)actdata["fio_address"],ALLOW_EMPTY_VALUES);                                
                  string HANDLEACTIVITYREMNFT = "rem_nft";
                  string insertQuery = "SELECT inshandleactivities("+
                  boost::lexical_cast<std::string>(fktransactionid)+","+
                      bnums+",'"+
                      handle+"','"+
                      HANDLEACTIVITYREMNFT+"','"+
                      blocktimestamp+"');";
                      
                  PGresult *res = PQexec(conn, insertQuery.c_str());
                  if (PQresultStatus(res) != PGRES_TUPLES_OK) {
                   terminalerror("remnft",insertQuery,conn,res);
                    return;
                  }
                  PQclear(res);

                   const rapidjson::Value& dvtrace = actdata["nfts"];
                  for (const auto& object : dvtrace.GetArray()) {
                      if (!object.IsObject()) {
                         std::cerr << "Error remnft: Element in array is not an object." << std::endl;
                         PQfinish(conn); //TODO -- cleaner exit.
                      }
                      const rapidjson::Value& nftdata = object;

                    if (nftdata.IsObject())
                    {
                      
                      string nftdatastr = getjsonstring(UNKNOWN_STRING,(rapidjson::Value&)nftdata,ALLOW_EMPTY_VALUES);
                      string chaincode = getjsonstring(UNKNOWN_STRING,(rapidjson::Value&)nftdata["chain_code"],ALLOW_EMPTY_VALUES);                                
                      string contractaddress = getjsonstring(UNKNOWN_STRING,(rapidjson::Value&)nftdata["contract_address"],ALLOW_EMPTY_VALUES);                                
                      string tokenid = getjsonstring(UNKNOWN_STRING,(rapidjson::Value&)nftdata["token_id"],ALLOW_EMPTY_VALUES);                                
                    
                        insertQuery = "SELECT delnftsignature('"+
                          handle+"','"+
                          chaincode+"','"+
                          contractaddress+"','"+
                            tokenid+"');";
                          
                      res = PQexec(conn, insertQuery.c_str());
                      if (PQresultStatus(res) != PGRES_TUPLES_OK) {
                        terminalerror("remnftdelnftsig",insertQuery,conn,res);
                        return;
                      }
                      PQclear(res);

                      
                    }else {
                      ilog ("fatal error remnft-- nfts parse error!!!");
                      PQfinish(conn);
                    }
                  } //end loop over nfts.
                } //end if action is remnft
                 else if ((actionname == "remallnfts")){
                  string handle = getjsonstring(UNKNOWN_STRING,(rapidjson::Value&)actdata["fio_address"],ALLOW_EMPTY_VALUES);                                
                  string HANDLEACTIVITYREMALLNFT = "rem_all_nft";
                  string insertQuery = "SELECT inshandleactivities("+
                  boost::lexical_cast<std::string>(fktransactionid)+","+
                      bnums+",'"+
                      handle+"','"+
                      HANDLEACTIVITYREMALLNFT+"','"+
                      blocktimestamp+"');";
                      
                  PGresult *res = PQexec(conn, insertQuery.c_str());
                  if (PQresultStatus(res) != PGRES_TUPLES_OK) {
                   terminalerror("remallnfts",insertQuery,conn,res);
                    return;
                  }
                  PQclear(res);

                  insertQuery = "SELECT delnftsignatures('"+
                      handle+"');";
                      
                  res = PQexec(conn, insertQuery.c_str());
                  if (PQresultStatus(res) != PGRES_TUPLES_OK) {
                   terminalerror("remallnftsdelnftsigs",insertQuery,conn,res);
                    return;
                  }
                  PQclear(res);

                 
                } //end if action is remallnfts
                else if ((actionname == "xferaddress")){
                  string handle = getjsonstring(UNKNOWN_STRING,(rapidjson::Value&)actdata["fio_address"],ALLOW_EMPTY_VALUES);                                
                  string pubkey = getjsonstring(UNKNOWN_STRING,(rapidjson::Value&)actdata["new_owner_fio_public_key"],ALLOW_EMPTY_VALUES);
                  string owneracct = fioio::key_to_account(pubkey);
                  string encryptkeyisset = "false";
                  string HANDLEACTIVITYTRANSFER = "transfer";
                  string chaincode = "FIO";
                  string tokencode = "FIO";
                  string insertQuery = "SELECT updhandlesxferowner("+
                       bnums+",'"+
                      handle +"','" +
                       owneracct +"','" +
                        pubkey +"','" +
                         encryptkeyisset +"');";
                      
                  PGresult *res = PQexec(conn, insertQuery.c_str());
                  if (PQresultStatus(res) != PGRES_TUPLES_OK) {
                  terminalerror("xferaddress",insertQuery,conn,res);
                    return;
                  }
                  PQclear(res);
                  
                  insertQuery = "SELECT inshandleactivities("+
                  boost::lexical_cast<std::string>(fktransactionid)+","+
                      bnums+",'"+
                      handle+"','"+
                      HANDLEACTIVITYTRANSFER+"','"+
                      blocktimestamp+"');";
                      
                  res = PQexec(conn, insertQuery.c_str());
                  if (PQresultStatus(res) != PGRES_TUPLES_OK) {
                   terminalerror("xferaddresshandleact",insertQuery,conn,res);
                    return;
                  }
                  PQclear(res);
                
                  insertQuery = "SELECT insaccountactivities("+
                  boost::lexical_cast<std::string>(fktransactionid)+","+
                  bnums+",'"+
                  owneracct+"','"+
                  +"receiver');";
                  
                  res = PQexec(conn, insertQuery.c_str());
                  if (PQresultStatus(res) != PGRES_TUPLES_OK) {
                   terminalerror("xferaddressaccountact",insertQuery,conn,res);
                    return;
                  }
                  PQclear(res);

                   insertQuery = "SELECT delpubaddresses('"+
                      handle+"');";
                      
                  res = PQexec(conn, insertQuery.c_str());
                  if (PQresultStatus(res) != PGRES_TUPLES_OK) {
                   terminalerror("xferaddressdelpubaddrs",insertQuery,conn,res);
                    return;
                  }
                  PQclear(res);
                

                
                   insertQuery = "SELECT insupdpubaddresses("+
                      bnums+",'"+
                      handle+"','"+
                       chaincode+"','"+
                        tokencode+"','"+
                         pubkey+"');";
                      
                  res = PQexec(conn, insertQuery.c_str());
                  if (PQresultStatus(res) != PGRES_TUPLES_OK) {
                   terminalerror("xferaddressinspubaddrs",insertQuery,conn,res);
                    return;
                  }
                  PQclear(res);

                   insertQuery = "SELECT delnftsignatures('"+
                      handle+"');";
                      
                  res = PQexec(conn, insertQuery.c_str());
                  if (PQresultStatus(res) != PGRES_TUPLES_OK) {
                   terminalerror("xferaddressdelnftsigs",insertQuery,conn,res);
                    return;
                  }
                  PQclear(res);
                } //end if action is xferaddress
                 else if ((actionname == "remalladdr")){
                  string handle = getjsonstring(UNKNOWN_STRING,(rapidjson::Value&)actdata["fio_address"],ALLOW_EMPTY_VALUES);                                
                  string HANDLEACTIVITYREMALLPUBADDR = "rem_all_pubadd";
                  
                 
                  
                  string insertQuery = "SELECT inshandleactivities("+
                  boost::lexical_cast<std::string>(fktransactionid)+","+
                      bnums+",'"+
                      handle+"','"+
                      HANDLEACTIVITYREMALLPUBADDR+"','"+
                      blocktimestamp+"');";
                      
                  PGresult *res = PQexec(conn, insertQuery.c_str());
                  if (PQresultStatus(res) != PGRES_TUPLES_OK) {
                   terminalerror("remalladdr",insertQuery,conn,res);
                    return;
                  }
                  PQclear(res);
                
                
                   insertQuery = "SELECT delpubaddresses('"+
                      handle+"');";
                      
                  res = PQexec(conn, insertQuery.c_str());
                  if (PQresultStatus(res) != PGRES_TUPLES_OK) {
                   terminalerror("remalladdrdelpubaddrs",insertQuery,conn,res);
                    return;
                  }
                  PQclear(res);
                
                } //end if action is remalladdr
                else if ((actionname == "addaddress")){
                  string handle = getjsonstring(UNKNOWN_STRING,(rapidjson::Value&)actdata["fio_address"],ALLOW_EMPTY_VALUES);                                
                  const rapidjson::Value& dvtrace = actdata["public_addresses"];
                  for (const auto& object : dvtrace.GetArray()) {
                      if (!object.IsObject()) {
                         std::cerr << "Error addaddress: Element in array is not an object." << std::endl;
                         PQfinish(conn); //TODO -- cleaner exit.
                      }
                      const rapidjson::Value& addressdata = object;

                    if (addressdata.IsObject())
                    {
                      string HANDLEACTIVITYADDPUBADD = "add_pubadd";
                      string addressdatastr = getjsonstring(UNKNOWN_STRING,(rapidjson::Value&)addressdata,ALLOW_EMPTY_VALUES);
                      string chaincode = getjsonstring(UNKNOWN_STRING,(rapidjson::Value&)addressdata["chain_code"],ALLOW_EMPTY_VALUES);                                
                      string tokencode = getjsonstring(UNKNOWN_STRING,(rapidjson::Value&)addressdata["token_code"],ALLOW_EMPTY_VALUES);                                
                      string pubaddress = getjsonstring(UNKNOWN_STRING,(rapidjson::Value&)addressdata["public_address"],ALLOW_EMPTY_VALUES);                                
                    
                      string insertQuery = "SELECT inshandleactivities("+
                      boost::lexical_cast<std::string>(fktransactionid)+","+
                          bnums+",'"+
                          handle+"','"+
                          HANDLEACTIVITYADDPUBADD+"','"+
                          blocktimestamp+"');";
                          
                      PGresult *res = PQexec(conn, insertQuery.c_str());
                      if (PQresultStatus(res) != PGRES_TUPLES_OK) {
                       terminalerror("addaddress",insertQuery,conn,res);
                        return;
                      }
                      PQclear(res);

                        insertQuery = "SELECT insupdpubaddresses("+
                          bnums+",'"+
                          handle+"','"+
                          chaincode+"','"+
                            tokencode+"','"+
                            pubaddress+"');";
                          
                      res = PQexec(conn, insertQuery.c_str());
                      if (PQresultStatus(res) != PGRES_TUPLES_OK) {
                        terminalerror("addaddressinspubaddr",insertQuery,conn,res);
                        return;
                      }
                      PQclear(res);

                      if (chaincode == "FIO" && (tokencode == "FIO" || tokencode == "*")){
                          insertQuery = "SELECT updhandlesencryptkey("+
                           bnums+",'"+
                          handle +"','" +
                          pubaddress +"');";
                          
                        res = PQexec(conn, insertQuery.c_str());
                        if (PQresultStatus(res) != PGRES_TUPLES_OK) {
                         terminalerror("addaddressupdcryptkey",insertQuery,conn,res);
                          return;
                        }
                        PQclear(res);
                      }
                    }else {
                      ilog ("fatal error -- pub addresses parse error!!!");
                      PQfinish(conn);
                    }
                  } //end loop over pub addresses.
                    
                } //end if action is addaddress
                 else if ((actionname == "remaddress")){
                  string handle = getjsonstring(UNKNOWN_STRING,(rapidjson::Value&)actdata["fio_address"],ALLOW_EMPTY_VALUES);                                
                  const rapidjson::Value& dvtrace = actdata["public_addresses"];
                  for (const auto& object : dvtrace.GetArray()) {
                      if (!object.IsObject()) {
                         std::cerr << "Error remaddress: Element in array is not an object." << std::endl;
                         PQfinish(conn); //TODO -- cleaner exit.
                      }
                      const rapidjson::Value& addressdata = object;

                    if (addressdata.IsObject())
                    {
                      string HANDLEACTIVITYREMPUBADD = "rem_pubadd";
                      string addressdatastr = getjsonstring(UNKNOWN_STRING,(rapidjson::Value&)addressdata,ALLOW_EMPTY_VALUES);
                      string chaincode = getjsonstring(UNKNOWN_STRING,(rapidjson::Value&)addressdata["chain_code"],ALLOW_EMPTY_VALUES);                                
                      string tokencode = getjsonstring(UNKNOWN_STRING,(rapidjson::Value&)addressdata["token_code"],ALLOW_EMPTY_VALUES);                                
                      string pubaddress = getjsonstring(UNKNOWN_STRING,(rapidjson::Value&)addressdata["public_address"],ALLOW_EMPTY_VALUES);                                
                    
                      string insertQuery = "SELECT inshandleactivities("+
                      boost::lexical_cast<std::string>(fktransactionid)+","+
                          bnums+",'"+
                          handle+"','"+
                          HANDLEACTIVITYREMPUBADD+"','"+
                          blocktimestamp+"');";
                          
                      PGresult *res = PQexec(conn, insertQuery.c_str());
                      if (PQresultStatus(res) != PGRES_TUPLES_OK) {
                        terminalerror("remaddress",insertQuery,conn,res);
                        return;
                      }
                      PQclear(res);

                        insertQuery = "SELECT delpubaddress('"+
                          handle+"','"+
                          chaincode+"','"+
                            tokencode+"','"+
                            pubaddress+"');";
                          
                      res = PQexec(conn, insertQuery.c_str());
                      if (PQresultStatus(res) != PGRES_TUPLES_OK) {
                         terminalerror("remaddressdelpubaddr",insertQuery,conn,res);
                        return;
                      }
                      PQclear(res);
                    }else {
                      ilog ("fatal error--remaddress pub addresses parse error!!!");
                      PQfinish(conn);
                    }
                  } //end loop over pub addresses.
                    
                } //end if action is remaddress
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
                      
                  PGresult *res = PQexec(conn, insertQuery.c_str());
                  if (PQresultStatus(res) != PGRES_TUPLES_OK) {
                    terminalerror("wraptokens",insertQuery,conn,res);
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
                      
                  PGresult *res = PQexec(conn, insertQuery.c_str());
                  if (PQresultStatus(res) != PGRES_TUPLES_OK) {
                    terminalerror("stkefio",insertQuery,conn,res);
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
                      
                  PGresult *res = PQexec(conn, insertQuery.c_str());
                  if (PQresultStatus(res) != PGRES_TUPLES_OK) {
                     terminalerror("unstakefio",insertQuery,conn,res);
                    return;
                  }
                  PQclear(res);
                } //end if action is unstakefio
                 else if ((actionname == "retire")){
                  string payeracct = getjsonstring(UNKNOWN_STRING,(rapidjson::Value&)actdata["actor"],ALLOW_EMPTY_VALUES);                                
                  string payeeacct = "";
                  string TRNSTYPERETIRE = "retire";
                  string memo = getjsonstring(UNKNOWN_STRING,(rapidjson::Value&)actdata["memo"],ALLOW_EMPTY_VALUES);
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
                       
                  PGresult *res = PQexec(conn, insertQuery.c_str());
                  if (PQresultStatus(res) != PGRES_TUPLES_OK) {
                    terminalerror("retire",insertQuery,conn,res);
                    return;
                  }
                  PQclear(res);
                } //end if action is retire


                } //end creator_action ordinal is 0
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
                
           
                  PGresult *res = PQexec(conn, insertQuery.c_str());
                  if (PQresultStatus(res) != PGRES_TUPLES_OK) {
                    terminalerror("instraces",insertQuery,conn,res);
                    return;
                  }

                  PQclear(res);
                

                }


               if ((actionname == "transfer")&&(receiveraccount == "fio.token")){
                // string actdatastr = getjsonstring(UNKNOWN_STRING,(rapidjson::Value&)actdata,ALLOW_EMPTY_VALUES);
               //  ilog(" actdata looks like ${d}",("d",actdatastr)); 
               //   ilog(" action ordinal looks like ${d}",("d",actionordinal)); 
                
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
 
                  string insertQuery = "SELECT instokentransfers("+
                       boost::lexical_cast<std::string>(fktransactionid)+","+
                      bnums+",'"+
                      payeracct+"','"+
                      payeeacct+"',"+
                      sufamount+",'"+
                      trnstype +"','"+
                      memo+"','"+
                      blocktimestamp+"');";
                      
                  PGresult *res = PQexec(conn, insertQuery.c_str());
                  if (PQresultStatus(res) != PGRES_TUPLES_OK) {
                    terminalerror("instokentransfer",insertQuery,conn,res);
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
                  string insertQuery = "SELECT instokentransfers("+
                       boost::lexical_cast<std::string>(fktransactionid)+","+
                      bnums+",'"+
                      payeracct+"','"+
                      payeeacct+"',"+
                      sufamount+",'"+
                      TRNSTYPETOKENMINT +"','"+
                      memo+"','"+
                      blocktimestamp+"');";
                      
                  PGresult *res = PQexec(conn, insertQuery.c_str());
                  if (PQresultStatus(res) != PGRES_TUPLES_OK) {
                     terminalerror("issue",insertQuery,conn,res);
                    return;
                  }
                  PQclear(res);
                } //end if action is issue
                else if (actionname == "bind2eosio"){
                  string accountnm = getjsonstring(UNKNOWN_STRING,(rapidjson::Value&)actdata["account"],ALLOW_EMPTY_VALUES);                 
                  string pubkey = getjsonstring(UNKNOWN_STRING,(rapidjson::Value&)actdata["client_key"],ALLOW_EMPTY_VALUES);
                  string insertQuery = "SELECT insupdaccounts("+
                      bnums+",'"+
                      accountnm+"','"+
                      pubkey+"','"+
                      blocktimestamp+"');";
                      
                  PGresult *res = PQexec(conn, insertQuery.c_str());
                  if (PQresultStatus(res) != PGRES_TUPLES_OK) {
                    terminalerror("bind2eosio",insertQuery,conn,res);
                    return;
                  }
                  PQclear(res);

                  insertQuery = "SELECT insaccountactivities("+
                      boost::lexical_cast<std::string>(fktransactionid)+","+
                      bnums+",'"+
                      accountnm+"','"+
                      +"receiver');";
                      
                  res = PQexec(conn, insertQuery.c_str());
                  if (PQresultStatus(res) != PGRES_TUPLES_OK) {
                     terminalerror("bind2eosioaccountact",insertQuery,conn,res);
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
             

            }
          }
          else if (msgtype == "TBL_ROW"){
            //check that the added false and the table is domains.
             string dataadded =getjsonstring(UNKNOWN_TIMESTAMP,document["data"]["added"],DISALLOW_EMPTY_VALUES);
             string kvotable =getjsonstring(UNKNOWN_TIMESTAMP,document["data"]["kvo"]["table"],DISALLOW_EMPTY_VALUES);
            const string datavs = getjsonstring(UNKNOWN_STRING,document["data"],DISALLOW_EMPTY_VALUES);
            

           if(dataadded == "false" && kvotable == "domains"){              
            //take the message and put it into a list of them...process these when we see
            //the burnexpired action....clear the list on end block
             domainjsons.push_back(datavs);
           }else  if(dataadded == "false" && kvotable == "fiohandles"){
            //take the message and put it into a list of them...process these when we see
            //the burnexpired action....clear the list on end block
            handlejsons.push_back(datavs);
           }


          }
          else if(msgtype == "FORK" ) {
                //TODO:  rollback code goes here!!!!
                 string insertQuery = "SELECT rbfork("+bnums+");";
           
            PGresult *res = PQexec(conn, insertQuery.c_str());
            if (PQresultStatus(res) != PGRES_TUPLES_OK) {
                 terminalerror("fork",insertQuery,conn,res);
                return;
            }

            PQclear(res);
                ack_block(bnum-1);
          }
          else { //TODO check if the event is in the list of events handled by RELIC then process as per relic{
           // ilog("UnAcknowledged EVENT ${e}",("e",msgtype));
          }
        } 
      }
       async_send_events();
    }
    }catch(...){ 
      PQfinish(conn);
      abort_receiver();
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
    (RELIC_HOST_OPT, bpo::value<string>()->default_value("localhost"), "postgres db server host to connect to")
    (RELIC_PORT_OPT, bpo::value<string>()->default_value("5432"), "postgres db port to connect to")
     (RELIC_USER_OPT, bpo::value<string>()->default_value("chronicle_user"), "postgres account to use for relic db")
    (RELIC_PASSWORD_OPT, bpo::value<string>()->default_value("1234!"), "postgres account pwd to use for relic db")
    (RELIC_DB_OPT, bpo::value<string>()->default_value("relicdb"), "postgres relic db name")
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
    my->relic_user = options.at(RELIC_USER_OPT).as<string>();
    my->relic_password = options.at(RELIC_PASSWORD_OPT).as<string>();
    my->relic_db = options.at(RELIC_DB_OPT).as<string>();

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
