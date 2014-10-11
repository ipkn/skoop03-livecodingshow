#include "crow_all.h"
#include <string>
#include <vector>
#include <chrono>
#include <set>
#include <cstdlib>
#include "MurmurHash3.h"

using namespace std;

set<string> names;

string current_script;

vector<vector<string>> msgs;
vector<pair<crow::response*, decltype(chrono::steady_clock::now())>> pending_responses;

void tag_cookie(string& in)
{
    char buf[16];
    MurmurHash3_x86_128(in.c_str(), in.size(), 0x9385f14a, buf);
    in += string(buf, buf+16);
}

pair<bool, string> validate_cookie(const string& in)
{
    if (in.size() < 9)
        return {false, ""};
    pair<bool ,string> ret;
    ret.second = in.substr(0, in.size()-16);
    string retag = ret.second;
    tag_cookie(retag);
    ret.first = retag == in;
    return ret;
}

char to_hex(int i)
{
    if (i < 10)
        return '0' + i;
    return 'a' + i-10;
}
string hex_encode(const string& in)
{
    string ret;
    for(char c : in)
    {
        ret += to_hex(((unsigned char)c)/16);
        ret += to_hex(((unsigned char)c)%16);
    }
    return ret;
}
string hex_decode(const string& in)
{
    string err = "* error *";
    if (in.size() % 2 == 1 || in.empty())
        return err;
    string ret;
    int v = 0;
    int flag = 0;
    for(char c : in)
    {
        v *= 16;
        if (c >= '0' && c <= '9')
            v += c - '0';
        else if (c >= 'a' && c <= 'f')
            v += c - 'a' + 10;
        else
            return err;
        flag ++;
        if (flag == 2)
        {
            ret += char(v);
            v = 0;
            flag = 0;
        }
    }
    return ret;
}

string read_file(const string& filename)
{
    ifstream inf(filename);
    if (!inf)
        return {};
    return {istreambuf_iterator<char>(inf), istreambuf_iterator<char>()};
}

void broadcast(const string& type, const string& msg)
{
    msgs.emplace_back(vector<string>{type, msg});
    crow::json::wvalue x;
    x["msgs"][0][0] = msgs.back()[0];
    x["msgs"][0][1] = msgs.back()[1];
    x["last"] = msgs.size();
    string body = crow::json::dump(x);
    for(auto p:pending_responses)
    {
        auto* res = p.first;
        res->end(body);
    }
    pending_responses.clear();
}

template <typename App, typename Req>
pair<bool, string> get_username(App& app, Req& req)
{
    auto& ctx = app.template get_context<crow::CookieParser>(req);
    return validate_cookie(hex_decode(ctx.get_cookie("username")));
}

int main()
{
    srand(time(NULL));
    string key = read_file("key");
    if (key.empty())
    {
        for(int i = 0; i < 16; i ++)
        {
            key += (char)random()%256;
        }
        key = hex_encode(key);
        CROW_LOG_INFO << "Key size: " << key.size();
        ofstream ouf("key");
        ouf.write(key.data(), key.size());
    }
    current_script = read_file("script.js");
    msgs.emplace_back(vector<string>{"script", current_script});
    crow::App<crow::CookieParser> app;
    crow::mustache::set_base(".");

    CROW_ROUTE(app, "/")
    ([]{
        crow::mustache::context ctx;
        return crow::mustache::load("index.html").render();
    });

    CROW_ROUTE(app, "/logs")
    ([]{
        bool has_script = false;
        crow::json::wvalue x;
        int start = max(0, (int)msgs.size()-100);
        for(int i = start; i < (int)msgs.size(); i++)
        {
            x["msgs"][i-start][0] = msgs[i][0];
            x["msgs"][i-start][1] = msgs[i][1];
            if (msgs[i][0] == "script")
                has_script = true;
        }
        if (!has_script && !current_script.empty())
        {
            x["msgs"][start-1][0] = "script";
            x["msgs"][start-1][1] = current_script;
        }
        x["last"] = msgs.size();
        return x;
    });

    CROW_ROUTE(app, "/logs/<int>")
    ([](const crow::request& req, crow::response& res, int after){
        if (after < (int)msgs.size())
        {
            crow::json::wvalue x;
            for(int i = after; i < (int)msgs.size(); i ++)
            {
                x["msgs"][i-after][0] = msgs[i][0];
                x["msgs"][i-after][1] = msgs[i][1];
            }
            x["last"] = msgs.size();

            res.write(crow::json::dump(x));
            res.end();
        }
        else
        {
            vector<pair<crow::response*, decltype(chrono::steady_clock::now())>> filtered;
            for(auto& p : pending_responses)
            {
                if (p.first->is_alive() && chrono::steady_clock::now() - p.second < chrono::seconds(30))
                    filtered.push_back(p);
                else
                    p.first->end();
            }
            pending_responses.swap(filtered);
            pending_responses.push_back({&res, chrono::steady_clock::now()});
        }
    });

    {
        ifstream inf("names");
        while(inf)
        {
            string name;
            inf >> name;
            names.insert(name);
        }
        names.insert("<SYSTEM>");
    }


    CROW_ROUTE(app, "/set_username")
    ([&](const crow::request& req)
    {
        CROW_LOG_INFO << "SET_USERNAME: " << req.body;
        string name = req.body;

        for(int i = 0; i < name.size(); i++)
            if (isspace(name[i]) || name[i] == ':')
                name[i] = '_';
        if (name.empty() || names.count(name))
            return crow::response(409);

        names.insert(name);
        {
            ofstream ouf("names", fstream::app);
            ouf << name << endl;
        }
        auto& ctx = app.get_context<crow::CookieParser>(req);
        tag_cookie(name);
        ctx.set_cookie("username", hex_encode(name)+";max-age=86400");
        return crow::response(200);
    });

    CROW_ROUTE(app, "/send")
    ([&](const crow::request& req)
    {
        auto& ctx = app.get_context<crow::CookieParser>(req);
        auto pp = validate_cookie(hex_decode(ctx.get_cookie("username")));
        if (!pp.first)
        {
            pp.second = "";
            ctx.set_cookie("username", "");
        }
        else
            broadcast("chat", pp.second + " : " + req.body);
        return "";
    });

    CROW_ROUTE(app, "/cookies.js")
    ([]
    {
        string script = read_file("cookies.js");
        return script;
    });

    CROW_ROUTE(app, "/reload")
    ([&](const crow::request& req)
    {
        if (req.body != key)
            return crow::response(403);
        string script = read_file("script.js");
        current_script = script;
        broadcast("script", script);
        return crow::response("");
    });

    crow::logger::setLogLevel(crow::LogLevel::DEBUG);

    app.port(40080)
        //.multithreaded()
        .run();
}
