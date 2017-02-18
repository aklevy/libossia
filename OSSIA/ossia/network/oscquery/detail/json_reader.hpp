#pragma once
#include <ossia/network/oscquery/detail/json_reader_detail.hpp>
#include <ossia/network/oscquery/detail/attributes.hpp>
#include <memory>
namespace ossia
{
namespace oscquery
{

//! Utilities to parse various JSON oscquery messages
class json_parser
{
public:
  static std::shared_ptr<rapidjson::Document> parse(std::string& message);

  static int getPort(const rapidjson::Value& obj);

  static message_type messageType(const rapidjson::Value& obj);

  static void parse_namespace(ossia::net::node_base& root, const rapidjson::Value& obj);
  static void parse_value(ossia::net::address_base& addr, const rapidjson::Value& obj);
  static void parse_address_value(ossia::net::node_base& root, const rapidjson::Value& obj);
  static void parse_path_added(ossia::net::node_base& map, const rapidjson::Value& obj);
  static void parse_path_removed(ossia::net::node_base& map, const rapidjson::Value& obj);
  static void parse_path_changed(ossia::net::node_base& map, const rapidjson::Value& mess);
  static void parse_attributes_changed(ossia::net::node_base& map, const rapidjson::Value& obj);

};


}
}
