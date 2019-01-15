#pragma once

#include <future>

#include "httplib.hpp"
#include "picosha2.hpp"
#include "json.hpp"

class TebexPushCommands {
public:
	TebexPushCommands(std::string secret, std::function<void(std::string)> logFn,
	                  std::function<bool(std::string)> execFn);
	bool startServer(std::string host, int port);
	void Ping();
	void PushListener();
	bool HashValid(const httplib::Request& req);

private:
	httplib::Server svr;
	std::function<void(std::string)> logFunction;
	std::function<bool(std::string)> execFunction;
	std::string secretKey;
};

TebexPushCommands::TebexPushCommands(std::string secret, std::function<void(std::string)> logFn,
                                     std::function<bool(std::string)> execFn) {
	this->secretKey = secret;
	this->logFunction = logFn;
	this->execFunction = execFn;
	logFunction("Starting Tebex Push Command Server");
}

bool TebexPushCommands::startServer(std::string host, int port) {

	this->Ping();
	this->PushListener();
	this->logFunction("Starting server on " + host + ":" + std::to_string(port));

	std::thread([this, host, port]() {
		this->svr.listen(host.c_str(), port);
	}).detach();

	return true;
}

void TebexPushCommands::Ping() {
	this->svr.Post("/ping", [this](const httplib::Request& req, httplib::Response& res) {
		this->logFunction("POST /ping");
		res.set_content("Connection Established!", "text/plain");
	});
}

void TebexPushCommands::PushListener() {
	this->svr.Post("/", [this](const httplib::Request& req, httplib::Response& res) {
		if (!this->HashValid(req)) {
			res.set_content("Invalid signature", "text/plain");
			res.status = 422;
		}
		else {
			nlohmann::basic_json commands = nlohmann::json::parse("{}");
			try {
				commands = nlohmann::json::parse(req.body);
			}
			catch (nlohmann::detail::parse_error ex) {
				this->logFunction("Unable to parse JSON");
				res.set_content("Invalid JSON", "text/plain");
				res.status = 422;
				return;
			}

			if (this->execFunction(req.body)) {
				this->logFunction("Exec Done");
				res.set_content("Commands executed", "text/plain");
				res.status = 200;
			}
			else {
				res.set_content("Error executing commands", "text/plain");
				res.status = 500;
			}

		}
	});
}

bool TebexPushCommands::HashValid(const httplib::Request& req) {
	std::string sig = req.get_header_value("X-Signature");
	std::transform(sig.begin(), sig.end(), sig.begin(), ::tolower);

	std::string strToHash = req.body + this->secretKey;
	std::vector<unsigned char> hash(picosha2::k_digest_size);
	picosha2::hash256(strToHash.begin(), strToHash.end(), hash.begin(), hash.end());

	std::string hex_str = picosha2::bytes_to_hex_string(hash.begin(), hash.end());

	return hex_str == sig;
}
