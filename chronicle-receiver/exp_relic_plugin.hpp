#include <appbase/application.hpp>
#include "rapidjson/document.h"
#include "rapidjson/reader.h"
#include "rapidjson/stringbuffer.h"
#include "rapidjson/writer.h"
#include <boost/algorithm/string/replace.hpp> 
#include <libpq-fe.h>
#include <iostream>
#include <fc/log/logger.hpp>

using namespace appbase;

class exp_relic_plugin : public appbase::plugin<exp_relic_plugin>
{
public:
  APPBASE_PLUGIN_REQUIRES();
  exp_relic_plugin();
  virtual ~exp_relic_plugin();
  virtual void set_program_options(options_description& cli, options_description& cfg) override;  
  void plugin_initialize(const variables_map& options);
  void plugin_startup();
  void plugin_shutdown();
  
private:
  std::unique_ptr<class exp_relic_plugin_impl> my;
};


const static string UNKNOWN_TIMESTAMP =  "";
  const static string UNKNOWN_STRING = "";
  const static string UNKNOWN_NUMBER_NULL = "NULL";
  const static string UNKNOWN_NUMBER_0 = "0";
  const static bool ALLOW_EMPTY_VALUES = true;
  const static bool DISALLOW_EMPTY_VALUES = false;

inline string getjsonstring(string defaultv, rapidjson::Value& v, bool allowempty){
  string retval = defaultv;

  if (!v.IsNull()){
    if (v.IsString()){
      std::string tv = v.GetString();
      if(allowempty)
      {
        return tv;
      }
      else{
        if (!tv.empty()){
          return tv;
        }
      }
    }else if (v.IsObject()){
        rapidjson::StringBuffer buffer;
        rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);

        // Write the Value to the buffer
        v.Accept(writer);

        // Get the JSON string from the buffer
        std::string tv = buffer.GetString();
        return tv;

    }else if (v.IsNumber()){
       rapidjson::StringBuffer buffer;
        rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
        v.Accept(writer);
        std::string numberString2 = buffer.GetString();
        return  numberString2;
    }
  }
  return retval;
}

// Function to escape SQL special characters in a string using boost::replace_all
inline std::string escapesqlstring(const std::string &input, PGconn *conn) {
    char *escaped = PQescapeLiteral(conn, input.c_str(), input.length());

    if (escaped == nullptr) {
        std::cerr << "Error escaping string: " << PQerrorMessage(conn) << std::endl;
        return "";
    }

    // Convert the escaped char* back to a C++ string
    std::string escapedStr(escaped);
    PQfreemem(escaped);  // Free the memory allocated by PQescapeLiteral
    return escapedStr;
}

inline string getjsonsqlescapedstring(string defaultv, rapidjson::Value& v, bool allowempty, PGconn *conn){
  string retval = getjsonstring(defaultv, v, allowempty);
         std::string tv =   escapesqlstring(retval, conn); 
  return tv;
}



