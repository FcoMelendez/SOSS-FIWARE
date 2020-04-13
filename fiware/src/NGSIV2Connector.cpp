﻿/*
 * Copyright 2019 Proyectos y Sistemas de Mantenimiento SL (eProsima).
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
*/

#include "NGSIV2Connector.hpp"

#include <curlpp/cURLpp.hpp>
#include <curlpp/Exception.hpp>
#include <curlpp/Easy.hpp>
#include <curlpp/Options.hpp>

#include <iostream>
#include <sstream>

namespace soss {
namespace fiware {


using namespace std::placeholders;

NGSIV2Connector::NGSIV2Connector(
        const std::string& remote_host,
        uint16_t remote_port,
        const std::string& listener_host,
        uint16_t listener_port,
        const std::string& ngsi_entity_id,
        const std::string& ngsi_entity_type,
        const std::string& ngsi_entity_attribute)

    : host_{remote_host}
    , port_{remote_port}
    , listener_host_{listener_host}
    , listener_port_{listener_port}
    , listener_{listener_port, std::bind(&NGSIV2Connector::receive, this, _1)}
    , ngsi_entity_id_{ngsi_entity_id}
    , ngsi_entity_type_{ngsi_entity_type}
    , ngsi_entity_attribute_{ngsi_entity_attribute}
    , subscription_callbacks_{}
{
}

std::string NGSIV2Connector::register_subscription(
        const std::string& entity,
        const std::string& type,
        FiwareSubscriptionCallback callback)
{
    if (subscription_callbacks_.empty() && !listener_.is_running())
    {
        listener_port_ = listener_.run();
    }

    std::stringstream url;
    url << "http://" << listener_host_ << ":" << listener_port_;

    Json subscription_entity;
    subscription_entity["id"] = entity;
    subscription_entity["type"] = type;

    Json manifest;
    manifest["subject"]["entities"] = Json::array({ subscription_entity });
    manifest["notification"]["http"]["url"] = url.str();

    std::string response = request("POST", true, "subscriptions", manifest);

    std::string subscription_id;
    std::istringstream response_header(response);

    for (std::string line; std::getline(response_header, line); )
    {
        if (line.find("Location: /v2/subscriptions/") != std::string::npos)
        {
            subscription_id = line.substr(line.find_last_of("/") + 1);
            subscription_id.pop_back(); // Remove \r tail
            break;
        }
    }

    if (subscription_id.empty())
    {
        std::cerr << "[soss-fiware][connector]: error registering subscription, "
                     "response: " << std::endl << response << std::endl;

        return "";
    }

    std::unique_lock<std::mutex> lock(subscription_mutex_);
    subscription_callbacks_[subscription_id] = callback;
    lock.unlock();

    std::cout << "[soss-fiware][connector]: subscription registered. "
                 "ID: " << subscription_id << std::endl;

    return subscription_id;
}

bool NGSIV2Connector::unregister_subscription(
        const std::string& subscription_id)
{
    std::string response = request("DELETE", false, "subscriptions/" + subscription_id, Json{});

    if (!response.empty())
    {
        std::cerr << "[soss-fiware][connector]: unsubscription error, "
                     "response: " << std::endl << response << std::endl;

        return false;
    }


    std::unique_lock<std::mutex> lock(subscription_mutex_);
    subscription_callbacks_.erase(subscription_id);
    lock.unlock();

    std::cout <<  "[soss-fiware][connector]: subscription with ID " << subscription_id <<
                  " unregistered successfully." << std::endl;

    if (subscription_callbacks_.empty() && listener_.is_running())
    {
        listener_.stop();
    }

    return true;
}

bool NGSIV2Connector::update_entity(
        const std::string& ngsi_entity_id,
        const std::string& ngsi_entity_type,
        const std::string& ngsi_entity_attribute,
        const std::string& ros_topic_name,
        const std::string& ros_message_type,
        const Json& ros_message_content_in_json_format)
{
    // [FIWARE] mapping
    // For the time being,
    //   the topic_name is being used as 'entity'
    //   the message_type is being used as 'type'
    //   the fiware_message is translated into JSON and sent as "json_message"
    // A first approach consists of changing this to,
    //   - the conf param 'entity_id' being used as 'entity'
    //   - the conf param 'entity_type' being used as 'type'
    //   - A mapping between a list of 'NGSI Attribute names' and 'ROS topic names'should be provided also
    //     as part of the configuration file
    //   - The structure of NGSI attributes that encapsulate ROS messages should consist of:
    //      * "type" : "Ros2Message"
    //      * "value":
    //          · "type": message_type
    //          · "topic_name": ros_topic_name
    //          · "value": json_message
    //
    // Example:
    //   {
    //      "id": entity_id,
    //      "type": entity_type,
    //      "attribute_name": {
    //          "type": "Ros2Message",
    //          "value": {
    //              "type": message_type,
    //              "topic_name": topic_name,
    //              "json_data": json_message
    //              }
    //          }
    //      }
    //  }

    // [FIWARE] Original 'urn' and 'response' objects
    /*
     * std::string urn = "entities/" + ros_topic_name + "/attrs?type=" + ros_message_type;
     * std::string response = request("PUT", false, urn, ros_message_content_in_json_format);
    */
    // End of Original 'urn' and 'response' objects

    // [FIWARE] NGSI Entity Id and Type Management
    std::cout<< "Entity Id: " << ngsi_entity_id << " Type: " << ngsi_entity_type << std::endl;
    std::cout<< "The attribute to be updated is: " << ngsi_entity_attribute << std::endl;
    std::cout<< "The ROS topic associated with this update is: " << ros_topic_name <<" ("<< ros_message_type<<")" <<std::endl;


    //std::string urn = "entities/" + ngsi_entity_id + "/attrs?type=" + ngsi_entity_type;
    std::string urn = "entities/" + ngsi_entity_id + "/attrs";

    //const Json ngsi_json_message;
    //ngsi_json_message[]
    Json ngsi_json_message;
    ngsi_json_message[ngsi_entity_attribute]["value"]["type"] = "RosMessage";
    ngsi_json_message[ngsi_entity_attribute]["value"]["test_field"] = "/move_base/goal";
    ngsi_json_message[ngsi_entity_attribute]["value"]["RosMessageType"] = ros_message_type;
    ngsi_json_message[ngsi_entity_attribute]["value"]["RosTopicName"] = ros_topic_name;
    ngsi_json_message[ngsi_entity_attribute]["value"]["data"] = ros_message_content_in_json_format.at("data");

    std::string response = request("PUT", false, urn, ngsi_json_message);

    // End of NGSI Entity Id and Type Management

    if (!response.empty())
    {
        std::cerr << "[soss-fiware][connector]: update entity error, "
                     "response: " << std::endl << response << std::endl;
    }

    return response.empty();
}

std::string NGSIV2Connector::request(
        const std::string& method,
        bool response_header,
        const std::string& urn,
        const Json& json_message)
{
    try
    {
        curlpp::Cleanup cleaner;
        curlpp::Easy request;

        std::stringstream url;
        url << host_ << ":" << port_ << "/v2/" << urn;
        request.setOpt(new curlpp::options::Url(url.str()));
        //request.setOpt(new curlpp::options::Verbose(true)); //Enable for debugging purposes

        if (method != "DELETE")
        {
            std::list<std::string> header;
            header.push_back("Content-Type: application/json");
            request.setOpt(new curlpp::options::HttpHeader(header));
        }
        request.setOpt(new curlpp::options::Header(response_header));
        request.setOpt(new curlpp::options::CustomRequest(method));

        std::string payload;

        if (method != "DELETE")
        {
            payload = json_message.dump();
            request.setOpt(new curlpp::options::PostFields(payload));
            request.setOpt(new curlpp::options::PostFieldSize(static_cast<long>(payload.length())));
        }

        std::stringstream response;
        request.setOpt(new curlpp::options::WriteStream(&response));

        request.perform();

        std::cout << "[soss-fiware][connector]: request to fiware, "
        << "url: " << url.str() << ", "
        << "method: " << method;
        if (method != "DELETE")
        {
            std::cout << ", payload: " << std::endl
            << "  " << payload;
        }
        std::cout << std::endl;

        return response.str();
    }
    catch (curlpp::LogicError& e)
    {
        std::cerr << "[soss-fiware][connector]: curl logic error: " << e.what() << std::endl;
    }
    catch (curlpp::RuntimeError& e)
    {
        std::cerr << "[soss-fiware][connector]: curl runtime error: " << e.what() << std::endl;
    }

    return "";
}

void NGSIV2Connector::receive(
            const std::string& message)
{
    Json json = Json::parse(message.c_str() + message.find('{'));

    std::string subscription_id = json["subscriptionId"];
    Json topic_data = json["data"][0];
    topic_data.erase("id");
    topic_data.erase("type");

    std::cout << "[soss-fiware][connector] received message from subscription ID: " << subscription_id << " - ";


    FiwareSubscriptionCallback callback = nullptr;

    std::unique_lock<std::mutex> lock(subscription_mutex_);
    auto it = subscription_callbacks_.find(subscription_id);
    if (subscription_callbacks_.end() != it)
    {
        callback = it->second;
    }
    lock.unlock();

    if (callback != nullptr)
    {
        std::cout << "accepted" << std::endl;
        callback(topic_data);
    }
    else
    {
        std::cout << "skipping" << std::endl;
    }
}


} //namespace fiware
} //namespace soss

