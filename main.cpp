// (c) 2017-present Pttn and contributors (https://riecoin.xyz/rieMiner)

#include <iomanip>
#include <nlohmann/json.hpp>
#include <unistd.h>
#ifndef _WIN32
	#include <arpa/inet.h>
	#include <netdb.h>
	#include <signal.h>
#else
	#include <winsock2.h>
#endif
#include "API.hpp"
#include "Client.hpp"
#include "main.hpp"
#include "Stella.hpp"
#include "tools.hpp"

std::string confPath("rieMiner.conf");
bool running(false);
std::shared_ptr<Stella::Instance> stellaInstance(nullptr);
std::shared_ptr<Client> client(nullptr);

std::optional<std::pair<std::string, std::string>> Configuration::_parseLine(const std::string &line, std::string &parsingMessages) const {
	if (line.size() == 0)
		return std::nullopt;
	if (line[0] == '#')
		return std::nullopt;
	const auto pos(line.find('='));
	if (pos != std::string::npos) {
		std::pair<std::string, std::string> option{line.substr(0, pos), line.substr(pos + 1, line.size() - pos - 1)};
		option.first.erase(std::find_if(option.first.rbegin(), option.first.rend(), [](unsigned char c) {return !std::isspace(c);}).base(), option.first.end()); // Trim spaces before =
		option.second.erase(option.second.begin(), std::find_if(option.second.begin(), option.second.end(), [](unsigned char c) {return !std::isspace(c);})); // Trim spaces after =
		return option;
	}
	else {
		parsingMessages += "Cannot find the delimiter '=' for: '"s + line + "'\n"s;
		return std::nullopt;
	}
}

bool Configuration::parse(const int argc, char** argv, std::string &parsingMessages) {
	if (argc >= 2)
		confPath = argv[1];
	
	if (confPath == "help") {
		logger.log("Configuration file line syntax: Key = Value\n"
		           "Same syntax for the command line options.\n"
		           "Available options are documented here. https://github.com/Pttn/rieMiner\n"
		           "Guides with configuration file examples can also be found here. https://riecoin.xyz/rieMiner\n"
		           "Use default rieMiner.conf configuration file\n"
		           "\t./rieMiner\n"
		           "Custom configuration file\n"
		           "\t./rieMiner existingConfigFile.conf\n"
		           "Custom configuration file and command line options\n"
		           "\t./rieMiner existingConfigFile.conf Option1=Value1 \"Option2 = Value2\" Option3=WeirdValue\\!\\!\n"
		           "No configuration file and command line options (a file named 'dummy' must not exist)\n"
		           "\t./rieMiner dummy Option1=Value1 \"Option2 = Value2\" Option3=WeirdValue\\!\\!\n"
		           "Examples:\n"
		           "\t./rieMiner Benchmark.conf\n"
		           "\t./rieMiner SoloMining.conf Threads=7\n"
		           "\t./rieMiner noconffile Mode=Pool Host=mining.example.com Port=5000 Username=username.worker Password=password\n"s);
		waitForUser();
		return false;
	}
	
	std::vector<std::string> lines;
	std::ifstream file(confPath, std::ios::in);
	if (file) {
		logger.log("Opening configuration file "s + confPath + "...\n"s);
		std::string line;
		while (std::getline(file, line))
			lines.push_back(line);
		file.close();
	}
	else if (argc <= 2) {
		logger.log(confPath + " not found or unreadable and no other arguments given. Please read the guides and README to learn how to configure rieMiner.\n"
		           "https://riecoin.xyz/rieMiner\n"
		           "https://github.com/Pttn/rieMiner/\n"s, MessageType::WARNING);
		waitForUser();
		return false;
	}
	
	if (argc > 2) {
		logger.log("Parsing "s + std::to_string(argc - 2) + " option(s) given by command line...\n"s);
		for (int i(2) ; i < argc ; i++)
			lines.push_back(argv[i]);
	}
	
	for (const auto &line : lines) {
		const std::optional<std::pair<std::string, std::string>> option(_parseLine(line, parsingMessages));
		if (!option.has_value())
			continue;
		std::string key(option.value().first), value(option.value().second);
		if (key == "Mode") {
			if (value == "Solo" || value == "Pool" || value == "Benchmark" || value == "Search")
				_options.mode = value;
			else {
				parsingMessages += "Invalid mode '"s + value + "'!\n"s
				                   "Possible values (case sensitive) are: Solo, Pool, Benchmark, Search\n";
			}
		}
		else if (key == "Host") _options.host = value;
		else if (key == "Port") {
			try {_options.port = std::stoi(value);}
			catch (...) {_options.port = 28332;}
		}
		else if (key == "Proxy") _options.proxy = value;
		else if (key == "Username") _options.username = value;
		else if (key == "Password") _options.password = value;
		else if (key == "Cookie") _options.cookie = value;
		else if (key == "PayoutAddress") _options.payoutAddress = value;
		else if (key == "Threads") {
			try {_options.stellaConfig.threads = std::stoi(value);}
			catch (...) {_options.stellaConfig.threads = 0;}
		}
		else if (key == "PrimeTableLimit") {
			try {_options.stellaConfig.primeTableLimit = std::stoll(value);}
			catch (...) {_options.stellaConfig.primeTableLimit = 0;}
		}
		else if (key == "GeneratePrimeTableFileUpTo"){
			try {_options.filePrimeTableLimit = std::stoll(value);}
			catch (...) {_options.filePrimeTableLimit = 0;}
		}
		else if (key == "SieveWorkers") {
			try {_options.stellaConfig.sieveWorkers = std::stoi(value);}
			catch (...) {_options.stellaConfig.sieveWorkers = 0;}
		}
		else if (key == "SieveBits") {
			try {_options.stellaConfig.sieveBits = std::stoi(value);}
			catch (...) {_options.stellaConfig.sieveBits = 0;}
		}
		else if (key == "SieveIterations") {
			try {_options.stellaConfig.sieveIterations = std::stoi(value);}
			catch (...) {_options.stellaConfig.sieveIterations = 0;}
		}
		else if (key == "DifficultyOffset") {
			try {_options.difficultyOffset = std::stoi(value);}
			catch (...) {_options.difficultyOffset = 0U;}
			if (_options.difficultyOffset > 2047U) _options.difficultyOffset = 2047U;
		}
		else if (key == "RestartDifficultyFactor") {
			try {_options.restartDifficultyFactor = std::stod(value);}
			catch (...) {_options.restartDifficultyFactor = 1.03;}
			if (_options.restartDifficultyFactor < 1.)
				_options.restartDifficultyFactor = 1.;
		}
		else if (key == "TupleLengthMin") {
			try {_options.tupleLengthMin = std::stoi(value);}
			catch (...) {_options.tupleLengthMin = 0;}
		}
		else if (key == "RefreshInterval") {
			try {_options.refreshInterval = std::stod(value);}
			catch (...) {_options.refreshInterval = 30.;}
		}
		else if (key == "Difficulty") {
			try {_options.difficulty = std::stod(value);}
			catch (...) {_options.difficulty = 1024.;}
			if (_options.difficulty < 128.) _options.difficulty = 128.;
			if (_options.difficulty > 4294967296.) _options.difficulty = 4294967296.;
		}
		else if (key == "BenchmarkBlockInterval") {
			try {_options.benchmarkBlockInterval = std::stod(value);}
			catch (...) {_options.benchmarkBlockInterval = 150.;}
		}
		else if (key == "BenchmarkTimeLimit") {
			try {_options.benchmarkTimeLimit = std::stod(value);}
			catch (...) {_options.benchmarkTimeLimit = 86400.;}
		}
		else if (key == "BenchmarkPrimeCountLimit") {
			try {_options.benchmarkPrimeCountLimit = std::stoll(value);}
			catch (...) {_options.benchmarkPrimeCountLimit = 1000000;}
		}
		else if (key == "TuplesFile")
			_options.tuplesFile = value;
		else if (key == "ConstellationPattern") {
			for (uint16_t i(0) ; i < value.size() ; i++) {if (value[i] == ',') value[i] = ' ';}
			std::stringstream offsetsSS(value);
			std::vector<uint64_t> offsets;
			uint64_t tmp;
			while (offsetsSS >> tmp) offsets.push_back(tmp);
			_options.stellaConfig.pattern = offsets;
		}
		else if (key == "PrimorialOffsets") {
			for (uint16_t i(0) ; i < value.size() ; i++) {if (value[i] == ',') value[i] = ' ';}
			std::stringstream offsets(value);
			std::vector<uint64_t> primorialOffsets;
			uint64_t tmp;
			while (offsets >> tmp) primorialOffsets.push_back(tmp);
			_options.stellaConfig.primorialOffsets = primorialOffsets;
		}
		else if (key == "Rules") {
			for (uint16_t i(0) ; i < value.size() ; i++) {if (value[i] == ',') value[i] = ' ';}
			std::stringstream offsets(value);
			_options.rules = std::vector<std::string>();
			std::string tmp;
			while (offsets >> tmp) _options.rules.push_back(tmp);
		}
		else if (key == "APIPort") {
			try {_options.apiPort = std::stoi(value);}
			catch (...) {_options.apiPort = 0U;}
		}
		else if (key == "RawOutput") {
			if (value == "Yes")
				logger.setRawMode(true);
			else
				logger.setRawMode(false);
		}
		else if (key == "LogDebug") {
			if (value == "Yes")
				_options.logDebug = true;
			else
				_options.logDebug = false;
		}
		else if (key == "KeepRunning") {
			if (value == "Yes")
				_options.keepRunning = true;
			else
				_options.keepRunning = false;
		}
		else
			parsingMessages += "Ignoring option with unused key '"s + key + "'\n"s;
	}
	if (_options.mode == "Benchmark" || _options.mode == "Search") { // Pick a default pattern if none was chosen
		if (_options.stellaConfig.pattern.size() == 0)
			_options.stellaConfig.pattern = {0, 2, 4, 2, 4, 6, 2};
	}
	return true;
}

#ifdef _WIN32
BOOL WINAPI signalHandler(DWORD signum) {
#else
void signalHandler(int signum) {
	if (signum == SIGPIPE) {
		logger.logDebug("\nIgnoring SigPipe Signal.\n"s);
		return;
	}
#endif
	logger.log("\nSignal "s + std::to_string(signum) + " received, stopping rieMiner.\n"s, MessageType::WARNING);
	if (stellaInstance == nullptr) exit(0);
	if (!stellaInstance->inited()) exit(0);
	stellaInstance->stop();
	running = false;
#ifdef _WIN32
	return true;
#endif
}

constexpr uint32_t countsRecentEntries(5);
int main(int argc, char** argv) {
#ifdef _WIN32
	SetPriorityClass(GetCurrentProcess(), BELOW_NORMAL_PRIORITY_CLASS); // Set lower priority, else the whole Windows system would lag a lot if using all threads
	if (!SetConsoleCtrlHandler((PHANDLER_ROUTINE) signalHandler, TRUE))
		logger.log("Could not set the Console Ctrl Handler\n", MessageType::ERROR);
#else
	struct sigaction sigAction;
	sigAction.sa_handler = signalHandler;
	sigemptyset(&sigAction.sa_mask);
	sigAction.sa_flags = 0;
	sigaction(SIGINT, &sigAction, NULL);
	sigaction(SIGPIPE, &sigAction, NULL);
#endif
	Configuration configuration;
	logger.log(std::string(versionString) + ", by the Riecoin Developers\n"s
	           "Project page: https://riecoin.xyz/rieMiner\n"s
	           "Riecoin Whitepaper: https://riecoin.xyz/Whitepaper\n"s
	           "Launch with 'help' as first argument for a quick usage guide\n"s
	           "Released under the GPLv3 - https://www.gnu.org/licenses/gpl-3.0.en.html\n"s);
	logger.hr();
	logger.log("Assembly code by Michael Bell (Rockhawk)\n"s
	           "G++ "s + std::to_string(__GNUC__) + "."s + std::to_string(__GNUC_MINOR__) + "."s + std::to_string(__GNUC_PATCHLEVEL__) + " - https://gcc.gnu.org/\n"s
	           "GMP "s + std::to_string(__GNU_MP_VERSION) + "."s + std::to_string(__GNU_MP_VERSION_MINOR) + "."s + std::to_string(__GNU_MP_VERSION_PATCHLEVEL) + " - https://gmplib.org/\n"s
	           "PicoSHA2 27fcf69 - https://github.com/okdshin/PicoSHA2\n"s
	           "Curl "s + LIBCURL_VERSION + " - https://curl.haxx.se/\n"s
	           "NLohmann Json "s + std::to_string(NLOHMANN_JSON_VERSION_MAJOR) + "."s + std::to_string(NLOHMANN_JSON_VERSION_MINOR) + "."s + std::to_string(NLOHMANN_JSON_VERSION_PATCH) + " - https://json.nlohmann.me/\n"s);
	logger.hr();
	logger.log("Build for: "s + Stella::sysInfo.getOs() + " on "s + Stella::sysInfo.getCpuArchitecture() + "\n"s
	           "Processor: "s + Stella::sysInfo.getCpuBrand() + "\n"s);
	if (Stella::sysInfo.getCpuArchitecture() == "x64") {
		logger.log("Best SIMD instructions supported by the CPU: "s);
		if (Stella::sysInfo.hasAVX512()) logger.log("AVX-512"s);
		else if (Stella::sysInfo.hasAVX2()) logger.log("AVX2"s);
		else if (Stella::sysInfo.hasAVX()) logger.log("AVX"s);
		else logger.log("None"s);
		logger.log("\n"s);
#ifdef __AVX2__
		logger.log("This build supports AVX2\n"s);
#else
		logger.log("This build does not support AVX2\n"s, MessageType::BOLD);
#endif
	}
	const double physicalMemory(Stella::sysInfo.getPhysicalMemory());
	logger.log("Physical Memory: "s);
	if (physicalMemory < 1.)
		logger.log("Detection Failed"s, MessageType::WARNING);
	else if (physicalMemory > 1099511627776.)
		logger.log(Stella::doubleToString(physicalMemory/1099511627776., 3) + " TiB"s);
	else if (physicalMemory > 1073741824.)
		logger.log(Stella::doubleToString(physicalMemory/1073741824., 3) + " GiB"s);
	else
		logger.log(Stella::doubleToString(physicalMemory/1048576., 3) + " MiB"s);
	logger.log("\n"s);
	logger.hr();
	
	std::string parsingMessages;
	if (!configuration.parse(argc, argv, parsingMessages))
		return 0;
	logger.setLogDebug(configuration.options().logDebug);
	if (configuration.options().logDebug) {
		logger.log("Debug file: "s + logger.getDebugFile() + "\n"s);
		logger.log("Can be disabled with 'LogDebug = No'.\n"s);
	}
	else
		logger.log("Debug File disabled.\n"s);
	logger.endStartupLog();
	if (parsingMessages.size() > 0ULL)
		logger.log(parsingMessages, MessageType::WARNING);
	
	if (configuration.options().filePrimeTableLimit > 1) {
		logger.log("Generating prime table up to "s + std::to_string(configuration.options().filePrimeTableLimit) + " and saving to "s + primeTableFile + "...\n"s);
		std::fstream file(primeTableFile, std::ios::out | std::ios::binary);
		if (file) {
			const auto primeTable(Stella::generatePrimeTable(configuration.options().filePrimeTableLimit));
			file.write(reinterpret_cast<const char*>(primeTable.data()), primeTable.size()*sizeof(decltype(primeTable)::value_type));
			file.close();
			logger.log("Table of "s + std::to_string(primeTable.size()) + " primes generated. Don't forget to disable the generation in "s + confPath + ".\n"s, MessageType::SUCCESS);
		}
		else
			logger.log("Could not open file  "s + primeTableFile + "!\n"s, MessageType::ERROR);
		return 0;
	}
	
	if (configuration.options().mode == "Benchmark") {
		logger.log("Benchmark Mode at difficulty "s + Stella::doubleToString(configuration.options().difficulty) + "\n"s);
		if (configuration.options().benchmarkBlockInterval > 0.)
			logger.log("\tBlock interval: " + Stella::doubleToString(configuration.options().benchmarkBlockInterval) + " s\n"s);
		if (configuration.options().benchmarkTimeLimit > 0.)
			logger.log("\tTime limit: " + Stella::doubleToString(configuration.options().benchmarkTimeLimit) + " s\n"s);
		if (configuration.options().benchmarkPrimeCountLimit != 0)
			logger.log("\tPrime (1-tuple) count limit: " + std::to_string(configuration.options().benchmarkPrimeCountLimit) + "\n"s);
	}
	else if (configuration.options().mode == "Search") {
		const double base10Exp(configuration.options().difficulty*0.301029996);
		logger.log("Search Mode at difficulty "s + Stella::doubleToString(configuration.options().difficulty) + " (numbers around "s + Stella::doubleToString(std::pow(10., base10Exp - std::floor(base10Exp))) + "*10^"s + Stella::doubleToString(std::floor(base10Exp)) + ") - Good luck!\n"s);
	}
	else {
		if (configuration.options().mode == "Solo")
			logger.log("Solo mining"s);
		else
			logger.log("Pooled mining"s);
		logger.log(" via host "s + configuration.options().host + ", port "s + std::to_string(configuration.options().port) + "\n"s);
		if (configuration.options().proxy != "")
			logger.log("Using Proxy: "s + configuration.options().proxy + "\n"s);
		if (configuration.options().mode == "Solo" && configuration.options().cookie != "")
			logger.log("Cookie: "s + configuration.options().cookie + "\n"s);
		else {
			logger.log("Username: "s + configuration.options().username + "\n"s);
			logger.log("Password: <"s + std::to_string(configuration.options().password.size()) + " character(s)>\n"s);
		}
		if (configuration.options().mode == "Solo") {
			std::vector<uint8_t> scriptPubKey(bech32ToScriptPubKey(configuration.options().payoutAddress));
			logger.log("Payout address: "s + configuration.options().payoutAddress + "\n"s);
			if (scriptPubKey.size() == 0) {
				logger.log("Invalid payout address! Please check it. Note that only Bech32M addresses are supported.\n"s, MessageType::ERROR);
				waitForUser();
				return false;
			}
			else
			logger.log("ScriptPubKey:   "s + v8ToHexStr(scriptPubKey) + "\n"s);
			if (configuration.options().rules.size() > 0)
				logger.log("Consensus Rules: "s + Stella::formatContainer(configuration.options().rules) + "\n"s);
		}
		if (configuration.options().difficultyOffset != 0)
			std::cout << "Difficulty Offset " << configuration.options().difficultyOffset << " (+ " << 32*configuration.options().difficultyOffset << ")" << std::endl;
		logger.log("Auto retune when the Difficulty varies by a factor "s + Stella::doubleToString(configuration.options().restartDifficultyFactor) + "\n"s);
	}
	if (configuration.options().refreshInterval > 0.)
		logger.log("Stats refresh interval: "s + Stella::doubleToString(configuration.options().refreshInterval) + " s\n"s);
	
	stellaInstance = std::make_shared<Stella::Instance>();
	if (configuration.options().mode == "Solo")
		client = std::make_shared<GBTClient>(configuration.options());
	else if (configuration.options().mode == "Pool")
		client = std::make_shared<StratumClient>(configuration.options());
	else if (configuration.options().mode == "Search")
		client = std::make_shared<SearchClient>(configuration.options());
	else
		client = std::make_shared<BMClient>(configuration.options());
	
	constexpr auto pollInterval(10ms);
	std::chrono::time_point<std::chrono::steady_clock> timer, miningStartTp;
	running = true;
	bool keepStats(false);
	uint64_t nBlocks(0ULL);
	uint32_t currentHeight(0U);
	if (client->isNetworked()) {
		std::shared_ptr<API> api(nullptr);
		if (configuration.options().apiPort != 0) {
			api = std::make_shared<API>(configuration.options().apiPort);
			api->setClient(client);
			api->start();
		}
		constexpr auto waitReconnect(10s); // Time to wait before auto reconnect.
		double initialDifficulty(0.);
		uint64_t countsRecentEntryPos(0ULL);
		std::vector<std::pair<std::chrono::time_point<std::chrono::steady_clock>, std::vector<uint64_t>>> tupleCountsRecent;
		logger.log("Connecting to Riecoin server...\n"s);
		std::dynamic_pointer_cast<NetworkedClient>(client)->connect();
		while (running) {
			// Get network context.
			const std::optional<ClientInfo> clientInfo(client->info());
			// If unable to get it, then there is a connection issue.
			if (!clientInfo.has_value()) {
				logger.log("Connection error, reconnecting in " + std::to_string(waitReconnect.count()) + " s...\n"s, MessageType::WARNING);
				keepStats = false;
				stellaInstance->stop(keepStats);
				std::this_thread::sleep_for(waitReconnect);
				std::dynamic_pointer_cast<NetworkedClient>(client)->connect();
				continue;
			}
			
			// (Re)Start Stella Instance if needed.
			if (!stellaInstance->inited()) {
				// Get User Configuration and complete it with network requirements.
				Stella::Configuration stellaConfig(configuration.options().stellaConfig);
				stellaConfig.pattern = Client::choosePatterns(clientInfo->acceptedPatterns, stellaConfig.pattern);
				stellaConfig.patternMin = clientInfo->patternMin;
				stellaConfig.primeCountTarget = clientInfo->primeCountTarget;
				stellaConfig.primeCountMin = clientInfo->primeCountMin;
				stellaConfig.initialBits = clientInfo->difficulty + 32*configuration.options().difficultyOffset;
				stellaConfig.initialTargetBits = clientInfo->targetOffsetBits + 32*configuration.options().difficultyOffset - std::ceil(stellaConfig.initialBits*(configuration.options().restartDifficultyFactor - 1.)); // Margin to take in account the Difficulty fluctuations
				logger.log("Initializing Stella Instance...\n"s);
				stellaInstance->init(stellaConfig);
				if (!stellaInstance->inited()) {
					logger.log("Something went wrong during the initialization, rieMiner cannot continue.\n"s, MessageType::ERROR);
					running = false;
					for (const auto &message : stellaInstance->initMessages())
						logger.log("\t"s + message, MessageType::ERROR);
					waitForUser();
					break;
				}
				logger.log("Done. Parameters:\n"s + stellaInstance->paramsStr());
				initialDifficulty = clientInfo->difficulty;
				currentHeight = clientInfo->height;
				if (!keepStats) {
					nBlocks = 0ULL;
					tupleCountsRecent = decltype(tupleCountsRecent)(countsRecentEntries, {std::chrono::steady_clock::now(), std::vector<uint64_t>(stellaConfig.pattern.size() + 1ULL, 0ULL)});
					miningStartTp = std::chrono::steady_clock::now();
					keepStats = true;
				}
				std::optional<Stella::Job> job(client->getJob());
				if (!job.has_value()) // Connection issue if it ever happens in such a short timespan will be handled in the next iteration.
					continue;
				job->clearPreviousJobs = true;
				stellaInstance->addJob(job.value());
				stellaInstance->startThreads();
				logger.hr();
				logger.log(Stella::formattedClockTimeNow() + " Started mining at block "s + std::to_string(clientInfo->height) + ", difficulty "s + Stella::doubleToString(clientInfo->difficulty + 32U*configuration.options().difficultyOffset, 3U)  + (32U*configuration.options().difficultyOffset > 0U ? " (offset +"s + std::to_string(32U*configuration.options().difficultyOffset) + ")"s : ""s)+ "\n"s, MessageType::BOLD);
				timer = std::chrono::steady_clock::now();
				continue;
			}
			
			// Handle event of a Block found by the network.
			if (currentHeight != clientInfo->height) {
				// Invalidate current Work Asap, no longer valid for the current Block.
				stellaInstance->invalidateWork();
				// Update Stats
				currentHeight = clientInfo->height;
				nBlocks++;
				const double averageBlockTime(nBlocks > 0ULL ? Stella::timeSince(miningStartTp)/static_cast<double>(nBlocks) : 0.);
				countsRecentEntryPos = (countsRecentEntryPos + 1) % countsRecentEntries;
				tupleCountsRecent[countsRecentEntryPos] = std::make_pair(std::chrono::steady_clock::now(), stellaInstance->getTupleCounts());
				// Notify the event
				logger.log(Stella::formattedClockTimeNow());
				logger.log(" Block "s + std::to_string(currentHeight) + ", average "s + Stella::doubleToString(averageBlockTime, 1) + " s, difficulty "s + Stella::doubleToString(clientInfo->difficulty + 32U*configuration.options().difficultyOffset, 3U)  + (32U*configuration.options().difficultyOffset > 0U ? " (offset +"s + std::to_string(32U*configuration.options().difficultyOffset) + ")"s : ""s)+ "\n"s);
				
				// Restart if needed to retune parameters.
				if (!stellaInstance->hasAcceptedPatterns(clientInfo->acceptedPatterns)) { // Pattern changed and no longer compatible with the current one.
					logger.log("Restarting Stella Instance due to pattern no longer accepted.\n");
					keepStats = false;
					stellaInstance->stop(keepStats);
				}
				else if (clientInfo->difficulty < initialDifficulty/configuration.options().restartDifficultyFactor
				      || clientInfo->difficulty > initialDifficulty*configuration.options().restartDifficultyFactor) { // Large Difficulty change.
					logger.log("Restarting Stella Instance to take in account Difficulty variations.\n");
					keepStats = true;
					stellaInstance->stop(keepStats);
				}
				else {
					// Add new Job if no restart needed (in the other cases, one will be added after the restart).
					std::optional<Stella::Job> job(client->getJob());
					if (!job.has_value()) // Connection issue will be handled in the next iteration.
						continue;
					job->clearPreviousJobs = true;
					stellaInstance->addJob(job.value());
				}
			}
			// Push new Job if needed.
			else if (stellaInstance->availableJobs() == 0ULL) {
				const std::optional<Stella::Job> job(client->getJob());
				if (!job.has_value()) // Connection issue will be handled in the next iteration.
					continue;
				stellaInstance->addJob(job.value());
			}
			
			// Handle submissions and let Client do its processing iteration.
			const std::vector<Stella::Result> pendingSubmissions(stellaInstance->getResults());
			for (const auto &submission : pendingSubmissions)
				client->handleResult(submission);
			client->process();
			
			// Print Stats.
			if (configuration.options().refreshInterval > 0. && Stella::timeSince(timer) > configuration.options().refreshInterval && stellaInstance->running()) {
				std::vector<uint64_t> tupleCounts(stellaInstance->getTupleCounts());
				const double duration(Stella::timeSince(tupleCountsRecent[(countsRecentEntryPos + 1) % countsRecentEntries].first));
				std::transform(tupleCounts.begin(), tupleCounts.end(), tupleCountsRecent[(countsRecentEntryPos + 1) % countsRecentEntries].second.begin(), tupleCounts.begin(), std::minus<uint64_t>());
				const double r(static_cast<double>(tupleCounts[0])/static_cast<double>(tupleCounts[1])),
							 cps(static_cast<double>(tupleCounts[0])/duration),
							 estimatedAverageBlockTime(std::pow(r, clientInfo->primeCountTarget)/cps),
							 earnings(86400.*(50./static_cast<double>(1 << currentHeight/840000))/estimatedAverageBlockTime);
				std::string message(Stella::formattedClockTimeNow() + " "s);
				if (tupleCounts[1] < 10ULL)
					message += "..."s;
				else if (configuration.options().mode == "Pool") {
					const uint64_t shares(std::dynamic_pointer_cast<StratumClient>(client)->shares()), rejectedShares(std::dynamic_pointer_cast<StratumClient>(client)->rejectedShares());
					message += Stella::doubleToString(earnings, 1U) + " RIC/d ("s + Stella::doubleToString(cps, 1U) + " c/s, r "s + Stella::doubleToString(r, 2U) + "), Sh: "s + std::to_string(shares - rejectedShares) + "/"s + std::to_string(shares);
					if (shares > 0ULL)
						message += " ("s + Stella::doubleToString(100.*(static_cast<double>(shares - rejectedShares)/static_cast<double>(shares)), 2U) + "%)"s;
				}
				else {
					const double estimatedAverageTupleTime(std::pow(r, tupleCounts.size() - 1ULL)/cps);
					message += Stella::doubleToString(earnings, 1U) + " RIC/d, " + Stella::formattedDuration(estimatedAverageTupleTime);
					message += " ("s + Stella::doubleToString(cps, 1U) + " c/s, r "s + Stella::doubleToString(r, 2U) + "), (1-"s + std::to_string(tupleCounts.size() - 1) + "t) = "s + Stella::formattedCounts(stellaInstance->getTupleCounts(), 1ULL);
				}
				logger.log(message + "\n"s);
				if (api)
					api->setStats(duration, r, cps, 86400./estimatedAverageBlockTime);
				timer = std::chrono::steady_clock::now();
			}
			std::this_thread::sleep_for(pollInterval);
		}
		if (api) {
			if (api->running())
				api->stop();
		}
	}
	else {
		// Non Networked Clients will never fail to provide the Info so no Optional needed.
		ClientInfo clientInfo(client->info().value());
		// Get User Configuration and complete it with network requirements.
		Stella::Configuration stellaConfig(configuration.options().stellaConfig);
		stellaConfig.patternMin = clientInfo.patternMin;
		stellaConfig.primeCountTarget = clientInfo.primeCountTarget;
		stellaConfig.primeCountMin = clientInfo.primeCountMin;
		stellaConfig.initialBits = clientInfo.difficulty;
		stellaConfig.initialTargetBits = clientInfo.targetOffsetBits;
		// Start Stella Instance.
		logger.log("Initializing Stella Instance...\n"s);
		stellaInstance->init(stellaConfig);
		if (!stellaInstance->inited()) {
			logger.log("Something went wrong during the initialization, rieMiner cannot continue.\n"s, MessageType::ERROR);
			for (const auto &message : stellaInstance->initMessages())
				logger.log("\t"s + message, MessageType::ERROR);
			waitForUser();
			return 0;
		}
		logger.log("Done. Parameters:\n"s + stellaInstance->paramsStr());
		currentHeight = clientInfo.height;
		miningStartTp = std::chrono::steady_clock::now();
		Stella::Job job(client->getJob().value());
		job.clearPreviousJobs = true;
		stellaInstance->addJob(job);
		stellaInstance->startThreads();
		logger.hr();
		logger.log(Stella::formattedTime(Stella::timeSince(miningStartTp)) + " Started "s + configuration.options().mode + ", difficulty "s + Stella::doubleToString(configuration.options().difficulty, 6U)  + "\n"s, MessageType::BOLD);
		timer = std::chrono::steady_clock::now();
		while (running) {
			// Get current Context.
			clientInfo = client->info().value();
			// Handle mock event of a Block found by the network for Benchmark Mode (height remains constant for Search).
			if (currentHeight != clientInfo.height) {
				// Invalidate current Work Asap, no longer valid for the current Block.
				stellaInstance->invalidateWork();
				job = client->getJob().value();
				job.clearPreviousJobs = true;
				stellaInstance->addJob(job);
				// Update Stats
				currentHeight = clientInfo.height;
				// Notify the event
				logger.log(Stella::formattedTime(Stella::timeSince(miningStartTp)));
				logger.log(" Block "s + std::to_string(currentHeight) + ", difficulty "s + Stella::doubleToString(clientInfo.difficulty, 6) + "\n"s);
			}
			// Get new Job if needed.
			else if (stellaInstance->availableJobs() == 0ULL) {
				job = client->getJob().value();
				stellaInstance->addJob(job);
			}
			
			// Handle submissions and let Client do its processing iteration.
			const std::vector<Stella::Result> pendingSubmissions(stellaInstance->getResults());
			for (const auto &submission : pendingSubmissions) {
				logger.log(Stella::formattedTime(Stella::timeSince(miningStartTp)) + " "s + std::to_string(submission.primeCount) + "-tuple found by worker thread "s + std::to_string(submission.threadId) + "\n"s, MessageType::BOLD);
				logger.log("Base prime: "s + submission.result.get_str() + "\n"s);
				// Stop number crunching if desired tuple found.
				if (configuration.options().mode == "Search") {
					if (!configuration.options().keepRunning) {
						if (submission.primeCount >= stellaConfig.pattern.size()) {
							const std::vector<uint64_t> tupleCounts(stellaInstance->getTupleCounts());
							const double duration(Stella::timeSince(miningStartTp)),
										 r(static_cast<double>(tupleCounts[0])/static_cast<double>(tupleCounts[1])),
										 cps(static_cast<double>(tupleCounts[0])/duration),
										 estimatedAverageBlockTime(std::pow(r, clientInfo.primeCountTarget)/cps);
							logger.log("Search finished after "s + Stella::doubleToString(duration, 6U) + " s and "s + Stella::formattedCounts(tupleCounts) + " tuples.\n"s +
										"Estimated average time between finds: "s + Stella::formattedDuration(estimatedAverageBlockTime) + " (1/2 chance: "s + Stella::formattedDuration(std::log(2.)*estimatedAverageBlockTime) + ").\n"s, MessageType::SUCCESS);
							stellaInstance->stop();
							running = false;
							client->handleResult(submission);
							break;
						}
					}
				}
				client->handleResult(submission);
			}
			client->process();
			
			if (configuration.options().mode == "Benchmark") {
				// Benchmark ended after meeting User Conditions.
				if ((configuration.options().benchmarkTimeLimit > 0. && Stella::timeSince(miningStartTp) >= configuration.options().benchmarkTimeLimit)
				 || (configuration.options().benchmarkPrimeCountLimit > 0ULL && stellaInstance->getTupleCounts()[1] >= configuration.options().benchmarkPrimeCountLimit)) {
					const std::vector<uint64_t> tupleCounts(stellaInstance->getTupleCounts());
					const double duration(Stella::timeSince(miningStartTp)),
					             r(static_cast<double>(tupleCounts[0])/static_cast<double>(tupleCounts[1])),
								 cps(static_cast<double>(tupleCounts[0])/duration),
								 estimatedAverageBlockTime(std::pow(r, clientInfo.primeCountTarget)/cps);
					logger.log("Benchmark finished after "s + Stella::doubleToString(Stella::timeSince(miningStartTp), 6U) + " s.\n"s +
								Stella::doubleToString(cps, 6U) + " candidates/s, ratio "s + Stella::doubleToString(r, 6U) + " -> "s + Stella::doubleToString(86400./estimatedAverageBlockTime, 6U) + " block(s)/day\n", MessageType::SUCCESS);
					std::string tuplesFoundStr("Tuples found: "s), tupleRatesStr("Tuples per s: "s), tupleRatiosStr("Tuple ratios: "s);
					for (uint64_t i(0) ; i < tupleCounts.size() ; i++) {
						tuplesFoundStr += std::to_string(tupleCounts[i]);
						tupleRatesStr += duration > 0. ? Stella::doubleToString(static_cast<double>(tupleCounts[i])/duration, 6U) : "-.------"s;
						if (i > 0)
							tupleRatiosStr += tupleCounts[i] > 0ULL ? Stella::doubleToString(static_cast<double>(tupleCounts[i - 1])/static_cast<double>(tupleCounts[i]), 6U) : "-.------"s;
						if (i != tupleCounts.size() - 1) {
							tuplesFoundStr += ", "s;
							tupleRatesStr += ", "s;
							if (i > 0) tupleRatiosStr += ", "s;
						}
					}
					logger.log(tuplesFoundStr + "\n"s + tupleRatesStr + "\n"s + tupleRatiosStr + "\n"s);
					stellaInstance->stop();
					running = false;
					break;
				}
			}
			
			// Print Stats.
			if (configuration.options().refreshInterval > 0. && Stella::timeSince(timer) > configuration.options().refreshInterval && stellaInstance->running()) {
				const std::vector<uint64_t> tupleCounts(stellaInstance->getTupleCounts());
				const double duration(Stella::timeSince(miningStartTp)),
							 r(static_cast<double>(tupleCounts[0])/static_cast<double>(tupleCounts[1])),
							 cps(static_cast<double>(tupleCounts[0])/duration),
							 estimatedAverageBlockTime(std::pow(r, clientInfo.primeCountTarget)/cps);
				std::string message(Stella::formattedTime(duration));
				message += " "s;
				if (tupleCounts[1] < 10ULL)
					message += "..."s;
				else {
					message += Stella::formattedDuration(estimatedAverageBlockTime);
					message += " ("s + Stella::doubleToString(cps, 1U) + " c/s, r "s + Stella::doubleToString(r, 2U) + "), (1-"s + std::to_string(tupleCounts.size() - 1) + "t) = "s + Stella::formattedCounts(tupleCounts, 1ULL);
				}
				logger.log(message + "\n"s);
				timer = std::chrono::steady_clock::now();
			}
			std::this_thread::sleep_for(pollInterval);
		}
	}
	return 0;
}
