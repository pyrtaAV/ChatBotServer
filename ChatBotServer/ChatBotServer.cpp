#include <iostream>
#include <uwebsockets/App.h>
#include <nlohmann/json.hpp>
using namespace std;
using json = nlohmann::json;

// Публичные сообщения 
// user13 => server: {"command": "public_msg", "text": "Всем приветы в этом чате"}
// server => all users {"command": "public_msg", "text": "...", user_from: 13}
// data, ws

void process_public_msg(auto data, auto* ws) {
    int user_id = ws->getUserData()->user_id;
    json payload = {
        {"command", data["command"]},
        {"text", data["text"]},
        {"user_from", user_id}
    };
    ws->publish("public", payload.dump());
    cout << "User sent Public Message " << user_id << endl;
}

// Приватные сообщения
// user10 => server {"command": "private_msg", "text": "...", user_to: 20}
// server => user20 {"command": "private_msg", "text": "...", user_from: 10}
void process_private_msg(auto data, auto* ws) {
    int user_id = ws->getUserData()->user_id;
    json payload = {
        {"command", data["command"]},
        {"text", data["text"]},
        {"user_from", user_id},
    };
    int user_to = data["user_to"];
    ws->publish("user" + to_string(user_to), payload.dump());
    cout << "User sent Private Message " << user_id << endl;
}


// {"command": "set_name", name:""}
void process_set_name(auto data, auto* ws) {
    int user_id = ws->getUserData()->user_id;
    ws->getUserData()->name = data["name"];
    cout << "User Set their Name " << user_id << endl;
}



// Возможность указать имя command:set_name
// Оповещение о подключениях command:status (список людей онлайн)
// 1. Подключение нового пользователя (public)
// 2. Отключение пользователя (public)
// 3. Свежеподключившимся пользователям сообщить о всех кто уже онлайн  (личный)
// 4. В случае смены имени (public)
// server => public {"command":"status", "user_id": 11, "online": True/False, "name": "Mike"}

string process_status(auto data, bool online) {
    json payload = {
        {"command","status"},
        {"user_id",data->user_id},
        {"name", data->name},
        {"online", online}
    };
    return payload.dump();
}

struct UserData {
    int user_id;
    string name;
};

map<int, UserData*> online_users;

int main()
{
    int latest_user_id = 10;


    uWS::App app = uWS::App().ws<UserData>("/*", {
        .idleTimeout = 300, // 5 min
        // Вызывается при новом подключении
        .open = [&latest_user_id](auto* ws) {
            UserData* data = ws->getUserData();
            data->user_id = latest_user_id++;
            data->name = "noname";

            cout << "New user connected: " << data->user_id << endl;
            ws->subscribe("public"); // подписываем всех на канал(топик) паблик
            ws->subscribe("user" + to_string(data->user_id)); // подписываем на персональны топик

            ws->publish("public", process_status(data, true));

            for (auto entry : online_users) {
                ws->send(process_status(entry.second, true), uWS::OpCode::TEXT);
            }

            online_users[data->user_id] = data;
        },
        // При получении данных от клиента
        .message = [](auto* ws, std::string_view data, uWS::OpCode opCode) {
            json parsed_data = json::parse(data); // ToDo: check format / handle exception

            if (parsed_data["command"] == "public_msg") {
                process_public_msg(parsed_data, ws);
            }

            if (parsed_data["command"] == "private_msg") {
                process_private_msg(parsed_data, ws);
            }

            if (parsed_data["command"] == "set_name") {
                process_set_name(parsed_data, ws);
                auto* data = ws->getUserData();
                ws->publish("public", process_status(data, true));
            }

        },
        .close = [](auto* ws, int /*code*/, std::string_view /*message*/) {
            UserData* data = ws->getUserData();
            online_users.erase(data->user_id);
            //app.publish("public", process_status(data, false), uWS::OpCode::TEXT);
        }
        }).listen(9001, [](auto* listen_socket) {
            if (listen_socket) {
                std::cout << "Listening on port " << 9001 << std::endl;
            } // http://localhost:9001/
            }).run();
}