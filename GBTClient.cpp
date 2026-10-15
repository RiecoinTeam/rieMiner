// (c) 2018-present Pttn (https://riecoin.xyz/rieMiner)

#include "Client.hpp"
#include "Stella.hpp"
#include "main.hpp"

const std::string cbMsg("/rM6a/");
static std::vector<uint8_t> coinbaseGen(const std::vector<uint8_t> &scriptPubKey, const uint32_t height, const uint64_t coinbasevalue, const std::vector<uint8_t> &dwc) {
	std::vector<uint8_t> coinbase;
	// Version (01000000)
	coinbase.insert(coinbase.end(), {0x01, 0x00, 0x00, 0x00});
	// Marker (00) and Flag (01) for SegWit
	if (dwc.size() > 0)
		coinbase.insert(coinbase.end(), {0x00, 0x01});
	// Input Count (01)
	coinbase.push_back(1);
	// Input TXID (0000000000000000000000000000000000000000000000000000000000000000)
	for (uint32_t i(0) ; i < 32 ; i++) coinbase.push_back(0x00);
	// Input VOUT (FFFFFFFF)
	for (uint32_t i(0) ; i < 4 ; i++) coinbase.push_back(0xFF);
	// (ScriptSig Construction)
		std::vector<uint8_t> scriptSig;
		// Block Height (Bip 34)
		if (height < 17)
			scriptSig.push_back(80 + height);
		else if (height < 128) {
			scriptSig.push_back(1);
			scriptSig.push_back(height);
		}
		else if (height < 32768) {
			scriptSig.push_back(2);
			scriptSig.push_back(height % 256);
			scriptSig.push_back((height/256) % 256);
		}
		else {
			scriptSig.push_back(3);
			scriptSig.push_back(height % 256);
			scriptSig.push_back((height/256) % 256);
			scriptSig.push_back((height/65536) % 256);
		}
		// Coinbase Message
		for (uint32_t i(0) ; i < cbMsg.size() ; i++) scriptSig.push_back(cbMsg[i]);
		// Randomization to avoid having 2 threads working on the same problem
		for (uint32_t i(0) ; i < 4 ; i++) scriptSig.push_back(rand(0x00, 0xFF));
	// ScriptSig Size
	coinbase.push_back(scriptSig.size());
	// ScriptSig
	coinbase.insert(coinbase.end(), scriptSig.begin(), scriptSig.end());
	// Input Sequence (FFFFFFFF)
	coinbase.insert(coinbase.end(), {0xFF, 0xFF, 0xFF, 0xFF});
	// Output Count
	coinbase.push_back(1U);
	if (dwc.size() > 0) coinbase.back()++; // Dummy Output for SegWit
	// Output Value
	uint64_t reward(coinbasevalue);
	for (uint32_t i(0) ; i < 8 ; i++) {
		coinbase.push_back(reward % 256);
		reward /= 256;
	}
	// Output/ScriptPubKey Length
	coinbase.push_back(scriptPubKey.size());
	// ScriptPubKey (for the payout address)
	coinbase.insert(coinbase.end(), scriptPubKey.begin(), scriptPubKey.end());
	// Dummy output and witness for SegWit
	if (dwc.size() > 0) {
		for (uint32_t i(0) ; i < 8 ; i++) coinbase.push_back(0); // No reward
		coinbase.push_back(dwc.size()); // Output Length
		coinbase.insert(coinbase.end(), dwc.begin(), dwc.end()); // Default Witness Commitment
		coinbase.push_back(1); // Number of Witnesses/stack items
		coinbase.push_back(32); // Witness Length
		for (uint32_t i(0) ; i < 32 ; i++) coinbase.push_back(0x00); // Witness of the Coinbase Input (0000000000000000000000000000000000000000000000000000000000000000)
	}
	// Lock Time (00000000)
	coinbase.insert(coinbase.end(), {0x00, 0x00, 0x00, 0x00});
	return coinbase;
}

std::array<uint8_t, 32> coinbaseTxId(const std::vector<uint8_t> &coinbase) {
	std::vector<uint8_t> coinbase2;
	for (uint32_t i(0) ; i < 4 ; i++) coinbase2.push_back(coinbase[i]); // nVersion
	for (uint32_t i(6) ; i < coinbase.size() - 38 ; i++) coinbase2.push_back(coinbase[i]); // txins . txouts
	for (uint32_t i(coinbase.size() - 4) ; i < coinbase.size() ; i++) coinbase2.push_back(coinbase[i]); // nLockTime
	return sha256sha256(coinbase2.data(), coinbase2.size());
}

static std::array<uint8_t, 32> calculateMerkleRoot(const std::vector<std::array<uint8_t, 32>> &txHashes) {
	std::array<uint8_t, 32> merkleRoot{};
	if (txHashes.size() == 0)
		logger.log("No transaction to hash!\n"s, MessageType::ERROR);
	else if (txHashes.size() == 1)
		return txHashes[0];
	else {
		std::vector<std::array<uint8_t, 32>> txHashes2;
		for (uint32_t i(0) ; i < txHashes.size() ; i += 2) {
			std::array<uint8_t, 64> concat;
			std::copy(txHashes[i].begin(), txHashes[i].end(), concat.begin());
			if (i == txHashes.size() - 1) // Concatenation of the last element with itself for an odd number of transactions
				std::copy(txHashes[i].begin(), txHashes[i].end(), concat.begin() + 32);
			else
				std::copy(txHashes[i + 1].begin(), txHashes[i + 1].end(), concat.begin() + 32);
			txHashes2.push_back(sha256sha256(concat.data(), 64));
		}
		// Process the next step
		merkleRoot = calculateMerkleRoot(txHashes2);
	}
	return merkleRoot;
}

static size_t curlWriteCallback(void *data, size_t size, size_t nmemb, std::string *s) {
	s->append((char*) data, size*nmemb);
	return size*nmemb;
}
nlohmann::json GBTClient::_sendRequestToWallet(const std::string &method, const nlohmann::json &params) const {
	static uint64_t id(0ULL);
	nlohmann::json jsonObj;
	if (_curl) {
		std::string s;
		const nlohmann::json request{{"jsonrpc", "2.0"}, {"method", method}, {"params", params}, {"id", id++}};
		const std::string requestStr(request.dump());
		curl_easy_setopt(_curl, CURLOPT_URL, _url.c_str());
		if (_proxy != "")
			curl_easy_setopt(_curl, CURLOPT_PROXY, _proxy.c_str());
		curl_easy_setopt(_curl, CURLOPT_POSTFIELDSIZE, requestStr.size());
		curl_easy_setopt(_curl, CURLOPT_POSTFIELDS, requestStr.c_str());
		curl_easy_setopt(_curl, CURLOPT_WRITEFUNCTION, curlWriteCallback);
		curl_easy_setopt(_curl, CURLOPT_WRITEDATA, &s);
		curl_easy_setopt(_curl, CURLOPT_USERPWD, _credentials.c_str());
		curl_easy_setopt(_curl, CURLOPT_TIMEOUT, 10);
		const CURLcode cc(curl_easy_perform(_curl));
		if (cc != CURLE_OK)
			logger.log("Curl_easy_perform() failed: "s + curl_easy_strerror(cc) + "\n"s, MessageType::ERROR);
		else {
			try {jsonObj = nlohmann::json::parse(s);}
			catch (nlohmann::json::parse_error &e) {
				if (s.size() == 0)
					logger.log("Nothing was received from the server!\n"s, MessageType::ERROR);
				else {
					logger.log("Received bad JSON object!\n"s
					           "Server message was: "s + s + "\n"s, MessageType::ERROR);
				}
			}
		}
	}
	return jsonObj;
}

bool GBTClient::_fetchJob() {
	nlohmann::json getblocktemplate, getblocktemplateResult;
	try {
		getblocktemplate = _sendRequestToWallet("getblocktemplate", {{{"rules", _rules}}});
		getblocktemplateResult = getblocktemplate["result"];
		if (getblocktemplateResult == nullptr) {
			nlohmann::json error(getblocktemplate["error"]);
			logger.log("GetBlockTemplate Error "s + std::to_string(int(error["code"])) +": "s + std::string(error["message"]) + "\n"s, MessageType::ERROR);
			return false;
		}
	}
	catch (...) {
		logger.log("GetBlockTemplate Unknown Error\n"s, MessageType::ERROR);
		return false;
	}
	JobTemplate newJobTemplate;
	try { // Extract and build GetBlockTemplate data
		newJobTemplate.job.bh.version = getblocktemplateResult["version"];
		newJobTemplate.job.bh.previousblockhash = v8ToA8(reverse(hexStrToV8(getblocktemplateResult["previousblockhash"])));
		newJobTemplate.job.bh.curtime = getblocktemplateResult["curtime"];
		newJobTemplate.job.bh.bits = std::stoll(std::string(getblocktemplateResult["bits"]), nullptr, 16);
		newJobTemplate.coinbasevalue = getblocktemplateResult["coinbasevalue"];
		std::vector<std::array<uint8_t, 32>> wTxIds{std::array<uint8_t, 32>{}};
		for (const auto &transaction : getblocktemplateResult["transactions"]) {
			const std::vector<uint8_t> txId(reverse(hexStrToV8(transaction["txid"])));
			newJobTemplate.job.transactionsHex += transaction["data"];
			newJobTemplate.txHashes.push_back(v8ToA8(txId));
			wTxIds.push_back(v8ToA8(reverse(hexStrToV8(transaction["hash"]))));
		}
		newJobTemplate.default_witness_commitment = "6a24aa21a9ed" + v8ToHexStr(a8ToV8(calculateMerkleRoot({calculateMerkleRoot(wTxIds), std::array<uint8_t, 32>{}})));
		newJobTemplate.job.txCount = newJobTemplate.txHashes.size() + 1; // Include Coinbase
		ClientInfo newClientParams;
		newClientParams.height = getblocktemplateResult["height"];
		newClientParams.powVersion = getblocktemplateResult["powversion"];
		if (newClientParams.powVersion != 1) {
			logger.log("Unsupported PoW Version "s + std::to_string(newClientParams.powVersion) + ", your rieMiner version is likely outdated!\n"s, MessageType::ERROR);
			return false;
		}
		newClientParams.acceptedPatterns = getblocktemplateResult["patterns"].get<decltype(newClientParams.acceptedPatterns)>();
		if (newClientParams.acceptedPatterns.size() == 0) {
			logger.log("Empty or invalid accepted patterns list!\n"s, MessageType::ERROR);
			return false;
		}
		newClientParams.patternMin = std::vector<bool>(newClientParams.acceptedPatterns[0].size(), true);
		newClientParams.primeCountTarget = newClientParams.acceptedPatterns[0].size();
		newClientParams.primeCountMin = newClientParams.primeCountTarget;
		newClientParams.difficulty = decodeBits(newJobTemplate.job.bh.bits, newClientParams.powVersion);
		newClientParams.targetOffsetBits = static_cast<uint32_t>(newClientParams.difficulty) - 264U;
		newJobTemplate.clientInfo = newClientParams;
	}
	catch (...) {
		logger.log("Received GetBlockTemplate Data with invalid parameters!\n"s
		           "Json Object was: "s + getblocktemplate.dump() + "\n"s, MessageType::ERROR);
		return false;
	}
	std::lock_guard<std::mutex> lock(_jobMutex);
	_currentJobTemplate = newJobTemplate;
	return true;
}

void GBTClient::connect() {
	if (!_connected) {
		if (_cookie != "") {
			std::ifstream file(_cookie, std::ios::in);
			if (!file) {
				logger.log("Could not open Cookie '"s + _cookie + "'!\n"s
				"Check that the Server is running, that the Cookie does exist at this path, and that this instance of rieMiner can read it.\n"s, MessageType::ERROR);
				return;
			}
			std::getline(file, _credentials);
		}
		_currentJobTemplate = JobTemplate();
		_pendingBlocks = {};
		process();
		if (!_currentJobTemplate.clientInfo.has_value()) {
			logger.log("Could not get a first job from the server!\n"s, MessageType::ERROR);
			return;
		}
		_connected = true;
	}
}

void GBTClient::process() {
	// Process pending submissions
	_submitMutex.lock();
	if (_pendingBlocks.size() > 0) {
		for (const auto &block : _pendingBlocks) {
			std::lock_guard<std::mutex> lock(_jobMutex);
			logger.log(Stella::formattedClockTimeNow() + " "s + std::to_string(block.primeCount) + "-tuple found by worker thread "s + std::to_string(block.threadId) + "\n"s, MessageType::BOLD);
			logger.log("Base prime: "s + block.result.get_str() + "\n"s);
			for (const auto &job : _currentJobs) {
				if (job.id == block.jobId) { // Sends a pending result via submitblock
					logger.log("Submitting block with "s + std::to_string(job.txCount) + " transaction(s) (including coinbase)...\n"s, MessageType::BOLD);
					BlockHeader bh(job.bh);
					bh.nOffset = encodedOffset(block, _difficultyOffset);
					std::ostringstream oss;
					oss << v8ToHexStr(bh.toV8());
					// Variable Length Integer Format
					if (job.txCount < 0xFD)
						oss << std::setfill('0') << std::setw(2) << std::hex << job.txCount;
					else
						oss << "fd" << std::setfill('0') << std::setw(2) << std::hex << job.txCount % 256 << std::setw(2) << job.txCount/256;
					oss << job.transactionsHex;
					try {
						nlohmann::json submitblock(_sendRequestToWallet("submitblock", {oss.str()})), result(submitblock["result"]), error(submitblock["error"]);
						if (result == nullptr && error == nullptr)
							logger.log("Submission accepted :D !\n"s, MessageType::SUCCESS);
						else {
							if (result != nullptr)
								logger.log("Submission rejected :| ! Reason: "s + std::string(result) + "\n"s, MessageType::WARNING);
							else
								logger.log("SubmitBlock Error "s + std::to_string(int(error["code"])) +" :| : "s + std::string(error["message"]) + "\n"s, MessageType::ERROR);
						}
					}
					catch (std::exception &e) {
						logger.log("Unknown SubmitBlock Error :| !\n"s, MessageType::ERROR);
						return;
					}
					break;
				} // If not found then Job was obsoleted due to a new Block, do not submit as it would be orphaned (but still show the Tuple).
			}
		}
		_pendingBlocks.clear();
	}
	_submitMutex.unlock();
	// Update work
	if (!_fetchJob()) {
		_connected = false; // If _fetchJob() failed, this means that the client is disconnected
		std::lock_guard<std::mutex> lock(_jobMutex);
		_currentJobTemplate.clientInfo = {};
	}
}

std::optional<Stella::Job> GBTClient::getJob() {
	std::lock_guard<std::mutex> lock(_jobMutex);
	Job job(_currentJobTemplate.job);
	if (!_currentJobTemplate.clientInfo.has_value())
		return {};
	Stella::Job stellaJob;
	job.id = _currentJobId;
	job.height = _currentJobTemplate.clientInfo->height;
	const std::vector<uint8_t> coinbase(coinbaseGen(_scriptPubKey, _currentJobTemplate.clientInfo->height, _currentJobTemplate.coinbasevalue, hexStrToV8(_currentJobTemplate.default_witness_commitment)));
	job.transactionsHex = v8ToHexStr(coinbase) + job.transactionsHex;
	std::vector<std::array<uint8_t, 32>> txHashesWithCoinbase{coinbaseTxId(coinbase)};
	txHashesWithCoinbase.insert(txHashesWithCoinbase.end(), _currentJobTemplate.txHashes.begin(), _currentJobTemplate.txHashes.end());
	_jobMutex.unlock();
	job.bh.merkleRoot = calculateMerkleRoot(txHashesWithCoinbase);
	if (_currentJobs.size() == 0)
		_currentJobs = {job};
	else if (_currentJobTemplate.clientInfo->height != _currentJobs.back().height)
		_currentJobs = {job};
	else
		_currentJobs.push_back(job);
	stellaJob.id = _currentJobId;
	stellaJob.target = job.bh.target(_currentJobTemplate.clientInfo->powVersion, _difficultyOffset);
	_currentJobId++;
	return stellaJob;
}

std::optional<ClientInfo> GBTClient::info() const {
	return _currentJobTemplate.clientInfo;
}
