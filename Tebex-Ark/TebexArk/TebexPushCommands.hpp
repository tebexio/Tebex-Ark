#pragma once

#include <future>
#include <queue>

#include "httplib.hpp"
#include "picosha2.hpp"
#include "json.hpp"

class TebexPushCommands {
public:
	TebexPushCommands(std::string secret, std::function<void(std::string)> logFn,
	                  std::function<bool(std::string)> execFn);
	bool startServer(std::string host, int port);
	void Ping();
	void Timer(float);
	void PushListener();
	bool HashValid(const httplib::Request& req) const;

private:
	httplib::Server svr;
	std::function<void(std::string)> logFunction;
	std::function<bool(std::string)> execFunction;
	std::queue<std::string> requestsQueue;
	std::string secretKey;
	int timer{};
};

inline TebexPushCommands::TebexPushCommands(std::string secret, std::function<void(std::string)> logFn,
                                     std::function<bool(std::string)> execFn) {
	this->secretKey = secret;
	this->logFunction = logFn;
	this->execFunction = execFn;
	logFunction("Starting Tebex Push Command Server");
}

inline bool TebexPushCommands::startServer(std::string host, int port) {

	this->Ping();
	this->PushListener();
	this->logFunction("Starting server on " + host + ":" + std::to_string(port));

	std::thread([this, host, port]() {
		this->svr.listen(host.c_str(), port);
	}).detach();

	//ArkApi::GetCommands().AddOnTimerCallback("TebexTimer", std::bind(&TebexPushCommands::Timer, this));
	ArkApi::GetCommands().AddOnTickCallback("TebexTimer", std::bind(&TebexPushCommands::Timer, this, std::placeholders::_1));

	return true;
}

inline void TebexPushCommands::Ping() {
	this->svr.Post("/ping", [this](const httplib::Request& req, httplib::Response& res) {
		this->logFunction("POST /ping");
		res.set_content("Connection Established!", "text/plain");
	});
}

inline void TebexPushCommands::Timer(float) {
	if (timer++ < 60)
		return;

	timer = 0;

	while (!requestsQueue.empty()) {
		if (this->execFunction(requestsQueue.front())) {
			this->logFunction("Exec Done");
		}
		else {
			this->logFunction("Exec Failed");
		}

		requestsQueue.pop();
	}
}

inline void TebexPushCommands::PushListener() {
	this->svr.Post("/", [this](const httplib::Request& req, httplib::Response& res) {
		if (!this->HashValid(req)) {
			res.set_content("Invalid signature", "text/plain");
			res.status = 422;
		}
		else {
			try {
				nlohmann::json::parse(req.body);
			}
			catch (const nlohmann::detail::parse_error&) {
				this->logFunction("Unable to parse JSON");
				res.set_content("Invalid JSON", "text/plain");
				res.status = 422;
				return;
			}

			requestsQueue.push(req.body);

			this->logFunction("Added to queue");
			//this->logFunction(req.body);
			 
			res.set_content("Command added", "text/plain");
			res.status = 200;
		}
	});
}

inline bool TebexPushCommands::HashValid(const httplib::Request& req) const {
	std::string sig = req.get_header_value("X-Signature");
	std::transform(sig.begin(), sig.end(), sig.begin(), ::tolower);

	std::string strToHash = req.body + this->secretKey;
	std::vector<unsigned char> hash(picosha2::k_digest_size);
	picosha2::hash256(strToHash.begin(), strToHash.end(), hash.begin(), hash.end());

	std::string hex_str = picosha2::bytes_to_hex_string(hash.begin(), hash.end());

	return hex_str == sig;
}
