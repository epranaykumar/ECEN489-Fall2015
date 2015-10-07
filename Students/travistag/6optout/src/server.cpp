//
// server.cpp
// ~~~~~~~~~~
//
// Copyright (c) 2003-2008 Christopher M. Kohlhoff (chris at kohlhoff dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include <ctime>
#include <iostream>
#include <string>
#include <vector>
#include <boost/array.hpp>
#include <boost/asio.hpp>
#include <sqlite3.h>

using boost::asio::ip::udp;

std::string findString(std::string s){
  int openq = s.find_first_of("\"");
  std::string s2 = s.substr(openq+1);
  s2 = s2.substr(0, s2.find_first_of("\""));
  return s2;
}

std::string expect(std::string whole, std::string search){
  if(whole.find(search)!=std::string::npos){
    whole = whole.substr(whole.find_first_of(search)+search.length());
    //cout<<"Got "<<search<<"\n";
  }
  else{
    std::cout<<"\n\nImproperly formatted JSON! Did not find "<<search<<"\n\n";
    return "";
  }
  return whole;
}

std::vector<std::string> getDeviceInfo(std::string json){
  std::vector<std::string> ret;
  json = expect(json, "{");
  json = expect(json, "\"DeviceID\"");
  json = expect(json, ":");
  std::string fn = findString(json);
  ret.push_back(fn);
  json = expect(json, ",");
  json = expect(json, "\"DeviceType\"");
  json = expect(json, ":");
  ret.push_back(findString(json));
  json = expect(json, ",");
  json = expect(json, "\"Data\"");
  json = expect(json, ":");
  ret.push_back(findString(json));
  json = expect(json, ",");
  json = expect(json, "\"Timestamp\"");
  json = expect(json, ":");
  ret.push_back(findString(json));
  json = expect(json, ",");
  json = expect(json, "\"currentIP\"");
  json = expect(json, ":");
  ret.push_back(findString(json));
  json = expect(json, "}");
  return ret;
}

void addDeviceInfo(std::vector<std::string> inf, sqlite3 *db){
  std::string comm = "INSERT INTO PhotoData (ID, Type, Data, Time, IPAddress)";
  comm+=" SELECT "+inf[0]+", '"+inf[1]+"', "+inf[2]+", '"+inf[3]+"', '"+inf[4]+"'";
  comm+=" WHERE NOT EXISTS (SELECT ID FROM PhotoData WHERE ID="+inf[0]+" AND Type='"+inf[1]+"' AND Data="+inf[2]+" AND Time='"+inf[3]+"' AND IPAddress='"+inf[4]+"')";
  char *err_msg = 0;

  int rc = sqlite3_open("photodata.db", &db);

  if (rc != SQLITE_OK) {
    std::cout<<"Cannot open database: "<<sqlite3_errmsg(db);
    sqlite3_close(db);
  }

  rc = sqlite3_exec(db, comm.c_str(), 0, 0, &err_msg);
  if (rc != SQLITE_OK) {

    fprintf(stderr, "SQL error: %s\n", err_msg);

    sqlite3_free(err_msg);
  }
  sqlite3_close(db);
}

int main(int argc, char* argv[])
{

  if(argc<2){
    std::cout<<"\nPlease enter a port # to listen on!\n\n";
    return 1;  
  }

  sqlite3 *db;
  char *err_msg = 0;
  sqlite3_stmt *stmt;
  int rc = sqlite3_open("photodata.db", &db);

  if (rc != SQLITE_OK) {
    std::cout<<"Cannot open database: "<<sqlite3_errmsg(db);
    sqlite3_close(db);
    return 1;
  }

  std::string com = "CREATE TABLE IF NOT EXISTS PhotoData(ID INT, Type TEXT, Data INT, Time TEXT, IPAddress TEXT)";
  rc = sqlite3_exec(db, com.c_str(), 0, 0, &err_msg);
  if (rc != SQLITE_OK) {

    fprintf(stderr, "SQL error: %s\n", err_msg);

    sqlite3_free(err_msg);
  }
  sqlite3_close(db);

  try
  {
    boost::asio::io_service io_service;

    udp::socket socket(io_service, udp::endpoint(udp::v4(), atoi(argv[1])));


    while(true)
    {
      std::string devicejson="";
      std::vector<char> buf(2000);
      udp::endpoint remote_endpoint;
      boost::system::error_code error;
      int x =socket.receive_from(boost::asio::buffer(buf),remote_endpoint, 0, error);
      for(int i=0; i<x; i++){
        devicejson+=buf[i];
      }
      std::cout<<"Received following bytes: \n";
      std::cout<<devicejson<<"\n\n";
      std::vector<std::string> info = getDeviceInfo(devicejson);
      addDeviceInfo(info, db);

      if (error && error != boost::asio::error::message_size)
        throw boost::system::system_error(error);


      boost::system::error_code ignored_error;
    }
  }
  catch (std::exception& e)
  {
    std::cerr << e.what() << std::endl;
  }

  return 0;
}