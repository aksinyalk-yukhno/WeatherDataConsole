#include <iostream>
#include <fstream>
#include <boost/asio.hpp>
#include "json-develop/single_include/nlohmann/json.hpp"
#include "cpp-httplib-master/httplib.h"

using json = nlohmann::json;
using namespace boost::asio;

std::string data_buffer;
serial_port* port;
int message_count = 0;

void process_message();

void read_from_port() {
    char data[1024];
    boost::system::error_code error;

    size_t len = port->read_some(buffer(data), error);
    if (!error) {
        data_buffer.append(data, len);
        process_message();
    }
    else {
        std::cout << "Error reading from the port: " << error.message() << std::endl;
    }
}

void process_message() {
    size_t start = data_buffer.find('$');
    size_t end = data_buffer.find("\r\n", start);

    if (start != std::string::npos && end != std::string::npos) {
        std::string message = data_buffer.substr(start + 1, end - start - 1);
        data_buffer.erase(0, end + 2);

        size_t comma_pos = message.find(',');
        if (comma_pos != std::string::npos) {
            std::string wind_speed = message.substr(0, comma_pos);
            std::string wind_direction = message.substr(comma_pos + 1);

            json weather_data;
            weather_data["sensor"] = "WMT700";
            weather_data["wind_speed"] = wind_speed;
            weather_data["wind_direction"] = wind_direction;
            weather_data["timestamp"] = std::time(nullptr);

            std::ofstream file("weather_data.json", std::ios::app);
            file << weather_data.dump() << std::endl;
            file.close();

            message_count++;

            std::cout << "\nMessage " << message_count << " processed:" << std::endl;
            std::cout << "Wind speed: " << wind_speed << std::endl;
            std::cout << "Wind direction: " << wind_direction << std::endl;
            std::cout << "Message saved to weather_data.json" << std::endl;

            if (message_count >= 10) {
                std::cout << "Processed 10 messages. Exiting..." << std::endl;
                exit(0);
            }
        }
    } else {
        std::cout << "Message not found in buffer." << std::endl;
    }
}

void start_http_server() {
    httplib::Server server;

    server.Get("/weather", [](const httplib::Request& req, httplib::Response& res) {
        std::ifstream file("weather_data.json");
        std::string json_data((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
        res.set_content(json_data, "application/json");
        });
    server.listen("localhost", 8080);
}

int main() {
    try {
        io_service io;
        port = new serial_port(io);

        port->open("COM6");
        port->set_option(serial_port_base::baud_rate(2400));
        port->set_option(serial_port_base::character_size(8));
        port->set_option(serial_port_base::stop_bits(serial_port_base::stop_bits::one));
        std::cout << "Connection established on COM6" << std::endl;
        
        std::thread http_thread(start_http_server);

        while (true) {
            read_from_port();
        }
        http_thread.join();
    }
    catch (boost::system::system_error& e) {
        std::cout << "Error working with COM port: " << e.what() << std::endl;
    }
    return 0;
}
