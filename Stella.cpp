/* (c) 2017-present Pttn and contributors (https://riecoin.xyz/rieMiner)
(c) 2018-2020 Michael Bell/Rockhawk (CPUID tools and Avx detection, assembly optimizations, improvements of work management between threads, and some more) (https://github.com/MichaelBell/) */

#include "Stella.hpp"

namespace Stella {
#if defined(__x86_64__) || defined(__i586__)
#include <cpuid.h>
#define CPUID
#endif
#if defined(__linux__)
#include <sys/sysinfo.h>
#elif defined(_WIN32)
#include <sysinfoapi.h>
#endif

SysInfo::SysInfo() : _os("Unknown/Unsupported"), _cpuArchitecture("Unknown"), _cpuBrand("Unknown"), _physicalMemory(0ULL), _avx(false), _avx2(false), _avx512(false) {
#if defined(__linux__)
	_os = "Linux";
	struct sysinfo si;
	if (sysinfo(&si) == 0)
		_physicalMemory = si.totalram;
#elif defined(_WIN32)
	_os = "Windows";
	MEMORYSTATUSEX statex;
	statex.dwLength = sizeof(statex);
	if (GlobalMemoryStatusEx(&statex) != 0)
		_physicalMemory = statex.ullTotalPhys;
#endif
#if defined(__x86_64__)
	_cpuArchitecture = "x64";
	_cpuBrand = "Unknown x64 CPU";
#elif defined(__i386__)
	_cpuArchitecture = "x86";
	_cpuBrand = "Unknown x86 CPU";
#elif defined(__aarch64__)
	_cpuArchitecture = "Arm64";
	_cpuBrand = "Unknown Arm64 CPU";
#elif defined(__arm__)
	_cpuArchitecture = "Arm";
	_cpuBrand = "Unknown Arm32 CPU";
#endif
#if defined(CPUID)
	if (__get_cpuid_max(0x80000004, nullptr)) {
		uint32_t brand[64];
		__get_cpuid(0x80000002, brand    , brand + 1, brand +  2, brand + 3);
		__get_cpuid(0x80000003, brand + 4, brand + 5, brand +  6, brand + 7);
		__get_cpuid(0x80000004, brand + 8, brand + 9, brand + 10, brand + 11);
		_cpuBrand = reinterpret_cast<char*>(brand);
	}
	
	uint32_t eax(0U), ebx(0U), ecx(0U), edx(0U);
	__get_cpuid(0U, &eax, &ebx, &ecx, &edx);
	if (eax >= 7) {
		__get_cpuid(1U, &eax, &ebx, &ecx, &edx);
		_avx = (ecx & (1 << 28)) != 0;
		// Must do this with inline assembly as __get_cpuid is unreliable for level 7 and __get_cpuid_count is not always available.
		//__get_cpuid_count(7, 0, &eax, &ebx, &ecx, &edx);
		uint32_t level(7), zero(0);
		asm ("cpuid\n\t"
		    : "=a"(eax), "=b"(ebx), "=c"(ecx), "=d"(edx)
		    : "0"(level), "2"(zero));
		_avx2 = (ebx & (1 << 5)) != 0;
		_avx512 = (ebx & (1 << 16)) != 0;
	}
#endif
}

std::vector<uint64_t> generatePrimeTable(const uint64_t limit) {
	if (limit < 2) return {};
	std::vector<uint64_t> compositeTable(limit/128ULL + 1ULL, 0ULL); // Booleans indicating whether an odd number is composite: 0000100100101100...
	for (uint64_t f(3ULL) ; f*f <= limit ; f += 2ULL) { // Eliminate f and its multiples m for odd f from 3 to square root of the limit
		if (compositeTable[f >> 7ULL] & (1ULL << ((f >> 1ULL) & 63ULL))) continue; // Skip if f is composite (f and its multiples were already eliminated)
		for (uint64_t m((f*f) >> 1ULL) ; m <= (limit >> 1ULL) ; m += f) // Start eliminating at f^2 (multiples of f below were already eliminated)
			compositeTable[m >> 6ULL] |= 1ULL << (m & 63ULL);
	}
	std::vector<uint64_t> primeTable(1, 2);
	for (uint64_t i(1ULL) ; (i << 1ULL) + 1ULL <= limit ; i++) { // Fill the prime table using the composite table
		if (!(compositeTable[i >> 6ULL] & (1ULL << (i & 63ULL))))
			primeTable.push_back((i << 1ULL) + 1ULL); // Add prime number 2i + 1
	}
	return primeTable;
}

#ifdef __SSE2__
#include "external/gmp_util.h"
extern "C" {
	void rie_mod_1s_4p_cps(uint64_t *cps, uint64_t p);
	mp_limb_t rie_mod_1s_4p(mp_srcptr ap, mp_size_t n, uint64_t ps, uint64_t cnt, uint64_t* cps);
	mp_limb_t rie_mod_1s_2p_4times(mp_srcptr ap, mp_size_t n, uint32_t* ps, uint32_t cnt, uint64_t* cps, uint64_t* remainders);
#ifdef __AVX2__
	mp_limb_t rie_mod_1s_2p_8times(mp_srcptr ap, mp_size_t n, uint32_t* ps, uint32_t cnt, uint64_t* cps, uint64_t* remainders);
#endif
}
#else
uint64_t mulMod(const uint64_t a, const uint64_t b, const uint64_t c) { // (ab) % c without assembly optimizations
#ifdef __SIZEOF_INT128__
	return (static_cast<__uint128_t>(a)*b) % c;
#else
	mpz_class tmp;
	mpz_set_ui(tmp.get_mpz_t(), a);
	mpz_mul_ui(tmp.get_mpz_t(), tmp.get_mpz_t(), b);
	return mpz_tdiv_ui(tmp.get_mpz_t(), c);
#endif
}
#endif

constexpr uint64_t nPrimesTo2p32(203280221);
constexpr int factorsCacheSize(16384);
constexpr uint16_t maxSieveWorkers(64); // There is a noticeable performance penalty using Std Vector or Arrays so we are using Raw Arrays.
thread_local uint64_t** factorsCache{nullptr};
thread_local uint64_t** factorsCacheCounts{nullptr};
thread_local uint16_t threadId(65535);

void Instance::init(const Configuration &configuration) {
	_initMessages = {};
	if (_inited) {
		_initMessages.push_back("The miner is already initialized\n"s);
		return;
	}
	
	_threads = configuration.threads;
	if (_threads == 0) {
		_threads = std::thread::hardware_concurrency();
		if (_threads == 0) {
			_initMessages.push_back("Could not detect number of Threads, setting to 1.\n"s);
			_threads = 1;
		}
	}
	
	_pattern = configuration.pattern;
#ifdef RIEMINERCUDA
	_gpuFermatTest = configuration.gpuFermatTest;
	_gpuFermatTestId = configuration.gpuFermatTestId;
#endif
	std::transform(_pattern.begin(), _pattern.end(), std::back_inserter(_halfPattern), [](uint64_t n) {return n >> 1;});
	_patternMin = configuration.patternMin;
	_primeCountTarget = configuration.primeCountTarget;
	_primeCountMin = configuration.primeCountMin;
	_primorialOffsetsU64 = configuration.primorialOffsets;
	if (_primorialOffsetsU64.size() == 0) { // Set the default Primorial Offsets if not chosen (must be chosen if the chosen pattern is not hardcoded)
		auto defaultPrimorialOffsetsIterator(std::find_if(defaultConstellationData.begin(), defaultConstellationData.end(), [this](const auto& constellationData) {return constellationData.first == _pattern;}));
		if (defaultPrimorialOffsetsIterator == defaultConstellationData.end()) {
			_initMessages.push_back("No hardcoded Constellation Offsets chosen and no Primorial Offset set.\n"s);
			return;
		}
		_primorialOffsetsU64 = defaultPrimorialOffsetsIterator->second;
		// Optimized Primorial Offsets if available.
		if (configuration.initialBits < 2180 && (_pattern == std::vector<uint64_t>{0, 2, 4, 2, 4, 6, 2} || _pattern == std::vector<uint64_t>{0, 2, 4, 2, 4, 6, 2, 6} || _pattern == std::vector<uint64_t>{0, 2, 4, 2, 4, 6, 2, 6, 4} || _pattern == std::vector<uint64_t>{0, 2, 4, 2, 4, 6, 2, 6, 4, 2}))
			_primorialOffsetsU64 = {114023297140211ULL, 114189340938131ULL, 114355384736051ULL, 114521428533971ULL, 145799289681161ULL, 145844141558231ULL, 145888993435301ULL, 145933845312371ULL, 220452326319761ULL, 220469307413891ULL, 220486288508021ULL, 220503269602151ULL, 235290683530361ULL, 235367427477641ULL, 235444171424921ULL, 235520915372201ULL, 294911989246991ULL, 295129811972111ULL, 295347634697231ULL, 295565457422351ULL, 345279548696891ULL, 345390797864921ULL, 345502047032951ULL, 345613296200981ULL, 353247262660751ULL, 353395658218061ULL, 353544053775371ULL, 353692449332681ULL, 393391977659951ULL, 393554263173341ULL, 393716548686731ULL, 393878834200121ULL, 437171043550511ULL, 437359375794611ULL, 437547708038711ULL, 437736040282811ULL, 547374026909441ULL, 547566452933231ULL, 547758878957021ULL, 547951304980811ULL, 624475220164331ULL, 624728375797061ULL, 624981531429791ULL, 625234687062521ULL, 727343907387941ULL, 727564067888501ULL, 727784228389061ULL, 728004388889621ULL, 770217042177041ULL, 770410293094901ULL, 770603544012761ULL, 770796794930621ULL, 942173182708601ULL, 942393875821241ULL, 942614568933881ULL, 942835262046521ULL, 987033700014821ULL, 987192352739051ULL, 987351005463281ULL, 987509658187511ULL, 992287041033131ULL, 992305282156031ULL, 992323523278931ULL, 992341764401831ULL, 1196266900300931ULL, 1196331655791731ULL, 1196396411282531ULL, 1196461166773331ULL, 1363806213708551ULL, 1363923696714191ULL, 1364041179719831ULL, 1364158662725471ULL, 1504145477032811ULL, 1504390303845041ULL, 1504635130657271ULL, 1504879957469501ULL, 1506606424567241ULL, 1506633383579321ULL, 1506660342591401ULL, 1506687301603481ULL, 1547277513006761ULL, 1547538621604511ULL, 1547799730202261ULL, 1548060838800011ULL, 1551543396451661ULL, 1551708809259221ULL, 1551874222066781ULL, 1552039634874341ULL, 1554535163864141ULL, 1554630942026621ULL, 1554726720189101ULL, 1554822498351581ULL};
		else if (configuration.initialBits < 2180 && _pattern == std::vector<uint64_t>{0, 2, 6, 4, 2, 4, 2})
			_primorialOffsetsU64 = {226193845148639ULL, 226374233346629ULL, 226554621544619ULL, 226735009742609ULL, 301570107719129ULL, 301713410008259ULL, 301856712297389ULL, 302000014586519ULL, 443216221842839ULL, 443355092154419ULL, 443493962465999ULL, 443632832777579ULL, 594353915119409ULL, 594554524107989ULL, 594755133096569ULL, 594955742085149ULL, 602484187049189ULL, 602527797215789ULL, 602571407382389ULL, 602615017548989ULL, 620850037212269ULL, 621267059407859ULL, 621684081603449ULL, 622101103799039ULL, 631774346860829ULL, 632202123099719ULL, 632629899338609ULL, 633057675577499ULL, 638409170551289ULL, 638417212765469ULL, 638425254979649ULL, 638433297193829ULL, 652691407502999ULL, 652768901389469ULL, 652846395275939ULL, 652923889162409ULL, 715627356082349ULL, 715866860827829ULL, 716106365573309ULL, 716345870318789ULL, 751355932060829ULL, 751594040050949ULL, 751832148041069ULL, 752070256031189ULL, 847107141137429ULL, 847125608236079ULL, 847144075334729ULL, 847162542433379ULL, 850067287283309ULL, 850306665932819ULL, 850546044582329ULL, 850785423231839ULL, 963140918896139ULL, 963391264321469ULL, 963641609746799ULL, 963891955172129ULL, 1322395947238349ULL, 1322624640823469ULL, 1322853334408589ULL, 1323082027993709ULL, 1426366146255299ULL, 1426606782441089ULL, 1426847418626879ULL, 1427088054812669ULL, 1450867151696099ULL, 1450935595771799ULL, 1451004039847499ULL, 1451072483923199ULL};
		else if (configuration.initialBits < 2180 && _pattern == std::vector<uint64_t>{0, 6, 2, 6, 4, 2, 4, 2})
			_primorialOffsetsU64 = {226193845148633ULL, 226374233346623ULL, 226554621544613ULL, 226735009742603ULL, 301570107719123ULL, 301713410008253ULL, 301856712297383ULL, 302000014586513ULL, 443216221842833ULL, 443355092154413ULL, 443493962465993ULL, 443632832777573ULL, 594353915119403ULL, 594554524107983ULL, 594755133096563ULL, 594955742085143ULL, 602484187049183ULL, 602527797215783ULL, 602571407382383ULL, 602615017548983ULL, 620850037212263ULL, 621267059407853ULL, 621684081603443ULL, 622101103799033ULL, 631774346860823ULL, 632202123099713ULL, 632629899338603ULL, 633057675577493ULL, 638409170551283ULL, 638417212765463ULL, 638425254979643ULL, 638433297193823ULL, 652691407502993ULL, 652768901389463ULL, 652846395275933ULL, 652923889162403ULL, 715627356082343ULL, 715866860827823ULL, 716106365573303ULL, 716345870318783ULL, 751355932060823ULL, 751594040050943ULL, 751832148041063ULL, 752070256031183ULL, 847107141137423ULL, 847125608236073ULL, 847144075334723ULL, 847162542433373ULL, 850067287283303ULL, 850306665932813ULL, 850546044582323ULL, 850785423231833ULL, 963140918896133ULL, 963391264321463ULL, 963641609746793ULL, 963891955172123ULL, 1322395947238343ULL, 1322624640823463ULL, 1322853334408583ULL, 1323082027993703ULL, 1426366146255293ULL, 1426606782441083ULL, 1426847418626873ULL, 1427088054812663ULL, 1450867151696093ULL, 1450935595771793ULL, 1451004039847493ULL, 1451072483923193ULL};
		else if (configuration.initialBits < 2180 && _pattern == std::vector<uint64_t>{0, 4, 6, 2, 6, 4, 2, 4, 2})
			_primorialOffsetsU64 = {226193845148629ULL, 226374233346619ULL, 226554621544609ULL, 226735009742599ULL, 301570107719119ULL, 301713410008249ULL, 301856712297379ULL, 302000014586509ULL, 443216221842829ULL, 443355092154409ULL, 443493962465989ULL, 443632832777569ULL, 594353915119399ULL, 594554524107979ULL, 594755133096559ULL, 594955742085139ULL, 602484187049179ULL, 602527797215779ULL, 602571407382379ULL, 602615017548979ULL, 620850037212259ULL, 621267059407849ULL, 621684081603439ULL, 622101103799029ULL, 631774346860819ULL, 632202123099709ULL, 632629899338599ULL, 633057675577489ULL, 638409170551279ULL, 638417212765459ULL, 638425254979639ULL, 638433297193819ULL, 652691407502989ULL, 652768901389459ULL, 652846395275929ULL, 652923889162399ULL, 715627356082339ULL, 715866860827819ULL, 716106365573299ULL, 716345870318779ULL, 751355932060819ULL, 751594040050939ULL, 751832148041059ULL, 752070256031179ULL, 847107141137419ULL, 847125608236069ULL, 847144075334719ULL, 847162542433369ULL, 850067287283299ULL, 850306665932809ULL, 850546044582319ULL, 850785423231829ULL, 963140918896129ULL, 963391264321459ULL, 963641609746789ULL, 963891955172119ULL, 1322395947238339ULL, 1322624640823459ULL, 1322853334408579ULL, 1323082027993699ULL, 1426366146255289ULL, 1426606782441079ULL, 1426847418626869ULL, 1427088054812659ULL, 1450867151696089ULL, 1450935595771789ULL, 1451004039847489ULL, 1451072483923189ULL};
		else if (configuration.initialBits < 2180 && _pattern == std::vector<uint64_t>{0, 2, 4, 6, 2, 6, 4, 2, 4, 2})
			_primorialOffsetsU64 = {226193845148627ULL, 226374233346617ULL, 226554621544607ULL, 226735009742597ULL, 301570107719117ULL, 301713410008247ULL, 301856712297377ULL, 302000014586507ULL, 443216221842827ULL, 443355092154407ULL, 443493962465987ULL, 443632832777567ULL, 594353915119397ULL, 594554524107977ULL, 594755133096557ULL, 594955742085137ULL, 602484187049177ULL, 602527797215777ULL, 602571407382377ULL, 602615017548977ULL, 620850037212257ULL, 621267059407847ULL, 621684081603437ULL, 622101103799027ULL, 631774346860817ULL, 632202123099707ULL, 632629899338597ULL, 633057675577487ULL, 638409170551277ULL, 638417212765457ULL, 638425254979637ULL, 638433297193817ULL, 652691407502987ULL, 652768901389457ULL, 652846395275927ULL, 652923889162397ULL, 715627356082337ULL, 715866860827817ULL, 716106365573297ULL, 716345870318777ULL, 751355932060817ULL, 751594040050937ULL, 751832148041057ULL, 752070256031177ULL, 847107141137417ULL, 847125608236067ULL, 847144075334717ULL, 847162542433367ULL, 850067287283297ULL, 850306665932807ULL, 850546044582317ULL, 850785423231827ULL, 963140918896127ULL, 963391264321457ULL, 963641609746787ULL, 963891955172117ULL, 1322395947238337ULL, 1322624640823457ULL, 1322853334408577ULL, 1323082027993697ULL, 1426366146255287ULL, 1426606782441077ULL, 1426847418626867ULL, 1427088054812657ULL, 1450867151696087ULL, 1450935595771787ULL, 1451004039847487ULL, 1451072483923187ULL};
	}
	_primorialOffsets = v64ToVMpz(_primorialOffsetsU64);
	
	const auto initialBits(configuration.initialBits);
	_sieveWorkers = configuration.sieveWorkers;
	const double primorialLog2Approx(static_cast<double>(configuration.initialTargetBits) - 40.);
	if (_sieveWorkers == 0) {
		double proportion(0.);
		if (_pattern.size() >= 10) proportion = std::exp(-0.00053*primorialLog2Approx);
		else if (_pattern.size() == 9) proportion = std::exp(-0.00065*primorialLog2Approx);
		else if (_pattern.size() == 8) proportion = std::exp(-0.00088*primorialLog2Approx);
		else if (_pattern.size() == 7) proportion = std::exp(-0.0013*primorialLog2Approx);
		else if (_pattern.size() == 6) proportion = std::exp(-0.0021*primorialLog2Approx);
		else if (_pattern.size() == 5) proportion = std::exp(-0.0037*primorialLog2Approx);
		else if (_pattern.size() == 4) proportion = std::exp(-0.0059*primorialLog2Approx);
		if (proportion > 1.) proportion = 1.;
		_sieveWorkers = std::ceil(proportion*static_cast<double>(_threads));
	}
	_sieveWorkers = std::min(static_cast<int>(_sieveWorkers), static_cast<int>(_threads) - 1);
	_sieveWorkers = std::max(static_cast<int>(_sieveWorkers), 1);
	_sieveWorkers = std::min(_sieveWorkers, maxSieveWorkers);
	_sieveWorkers = std::min(static_cast<int>(_sieveWorkers), static_cast<int>(_primorialOffsets.size()));
	
	_primeTableLimit = configuration.primeTableLimit;
	if (_primeTableLimit == 0) {
		uint64_t primeTableLimitMax(4294967296ULL);
		if (sysInfo.getPhysicalMemory() == 0ULL) // Could not detect Ram, reasonable default for a machine with 4 GiB.
			primeTableLimitMax = 1073741824ULL;
		else if (sysInfo.getPhysicalMemory() < 536870912ULL) // A Vintage Computer, maybe...
			primeTableLimitMax = 67108864ULL;
		else if (sysInfo.getPhysicalMemory() < 34359738368ULL)
			primeTableLimitMax = sysInfo.getPhysicalMemory()/8ULL;
		_primeTableLimit = std::pow(initialBits, 6.)/std::pow(2., 3.*static_cast<double>(_pattern.size()) + 7.);
		if (_threads > 32) {
			_primeTableLimit *= 32;
			_primeTableLimit /= static_cast<double>(_threads);
		}
		_primeTableLimit = std::min(_primeTableLimit, primeTableLimitMax);
	}
	
	std::vector<uint64_t> primes;
	uint64_t primeTableFileBytes, savedPrimes(0), largestSavedPrime;
	std::fstream file(primeTableFile);
	if (file) {
		file.seekg(0, std::ios::end);
		primeTableFileBytes = file.tellg();
		savedPrimes = primeTableFileBytes/sizeof(decltype(primes)::value_type);
		if (savedPrimes > 0) {
			file.seekg(-static_cast<int64_t>(sizeof(decltype(primes)::value_type)), std::ios::end);
			file.read(reinterpret_cast<char*>(&largestSavedPrime), sizeof(decltype(primes)::value_type));
		}
	}
	std::chrono::time_point<std::chrono::steady_clock> t0(std::chrono::steady_clock::now());
	_primeTableExtracted = false;
	if (savedPrimes > 0 && _primeTableLimit >= 1048576 && _primeTableLimit <= largestSavedPrime) {
		uint64_t nPrimesUpperBound(std::min(1.085*static_cast<double>(_primeTableLimit)/std::log(static_cast<double>(_primeTableLimit)), static_cast<double>(savedPrimes))); // 1.085 = max(π(p)log(p)/p) for p >= 2^20
		try {
			primes = std::vector<uint64_t>(nPrimesUpperBound);
		}
		catch (std::bad_alloc& ba) {
			_initMessages.push_back("Unable to allocate memory for the prime table. Try to reduce the PrimeTableLimit parameter.\n"s);
			return;
		}
		file.seekg(0, std::ios::beg);
		file.read(reinterpret_cast<char*>(primes.data()), nPrimesUpperBound*sizeof(decltype(primes)::value_type));
		file.close();
		for (auto i(primes.size() - 1) ; i > 0 ; i--) {
			if (primes[i] <= _primeTableLimit) {
				primes.resize(i + 1);
				break;
			}
		}
		_primeTableExtracted = true;
		_primeTableGenerationTime = timeSince(t0);
	}
	else {
		try {
			primes = generatePrimeTable(_primeTableLimit);
		}
		catch (std::bad_alloc& ba) {
			_initMessages.push_back("Unable to allocate memory for the prime table. Try to reduce the PrimeTableLimit parameter.\n"s);
			return;
		}
		_primeTableGenerationTime = timeSince(t0);
	}

	if (primes.size() % 2 == 1) // Needs to be even to use SIMD sieving optimizations
		primes.pop_back();
	
	try {
		_primes32.reserve(std::min(static_cast<decltype(_primes32)::value_type>(nPrimesTo2p32), static_cast<decltype(_primes32)::value_type>(primes.size())));
		if (primes.size() > nPrimesTo2p32) _primes64.reserve(primes.size() - nPrimesTo2p32);
	}
	catch (std::bad_alloc& ba) {
		_initMessages.push_back("Unable to allocate memory for the prime table. Try to reduce the PrimeTableLimit parameter.\n"s);
		return;
	}

	for (size_t i = 0; i < primes.size(); ++i) {
		if (primes[i] < (1ULL << 32)) _primes32.push_back(primes[i]);
		else _primes64.push_back(primes[i]);
	}
	
	_nPrimes = primes.size();
	_nPrimes32 = _primes32.size();
	primes.clear();

	_sieveBits = configuration.sieveBits;
	if (_sieveBits == 0) {
		if (sysInfo.getCpuArchitecture() == "x64")
			_sieveBits = _sieveWorkers <= 4 ? 25 : 24;
		else
			_sieveBits = _sieveWorkers <= 4 ? 23 : 22;
	}
	_sieveSize = 1 << _sieveBits;
	_sieveWords = _sieveSize/64;
	_sieveIterations = configuration.sieveIterations;
	if (_sieveIterations == 0)
		_sieveIterations = 16;
	_factorMax = _sieveIterations*_sieveSize;
	
	// The primorial times the maximum factor should be smaller than the allowed limit for the target offset.
	mpz_class primorialLimit(1);
	primorialLimit <<= configuration.initialTargetBits;
	primorialLimit--;
	primorialLimit /= u64ToMpz(_factorMax);
	if (primorialLimit == 0) {
		_initMessages.push_back("The Difficulty is too low. Try to increase it or decrease the Sieve Size/Iterations.\n"s);
		return;
	}
	mpz_set_ui(_primorial.get_mpz_t(), 1);
	for (uint64_t i(0) ; i < _primes32.size() ; i++) {
		if (_primorial*_primes32[i] >= primorialLimit) {
			_primorialNumber = i;
			break;
		}
		_primorial *= _primes32[i];
		if (i + 1 == _primes32.size())
			_primorialNumber = i + 1;
	}
	_primorialOffsetDiff.resize(_sieveWorkers - 1);
	_patternCumulative = std::vector<uint64_t>(_pattern.size(), 0);
	std::partial_sum(_pattern.begin(), _pattern.end(), _patternCumulative.begin(), std::plus<uint64_t>());
	const uint64_t constellationDiameter(_patternCumulative.back());
	for (int j(1) ; j < _sieveWorkers ; j++)
		_primorialOffsetDiff[j - 1] = _primorialOffsetsU64[j] - _primorialOffsetsU64[j - 1] - constellationDiameter;
	
	// Precomputing data used to speed up presieving computations.
	t0 = std::chrono::steady_clock::now();
#ifdef __SSE2__
	const uint64_t precompPrimes(std::min(_nPrimes, 5586502348UL)); // Precomputation only works up to p = 2^37
#endif
	try {
		_modularInverses32.resize(_primes32.size());
		_modularInverses64.resize(_primes64.size()); // Table of inverses of the primorial modulo a prime number in the table with index >= primorialNumber.
#ifdef __SSE2__
		_modPrecompute.resize(precompPrimes);
#endif
	}
	catch (std::bad_alloc& ba) {
		_initMessages.push_back("Unable to allocate memory for the precomputed data. Try to reduce the PrimeTableLimit parameter.\n"s);
		return;
	}
	const uint64_t blockSize((_nPrimes - _primorialNumber + _threads - 1)/_threads);
	std::thread threads[_threads];
	for (uint16_t j(0) ; j < _threads ; j++) {
		threads[j] = std::thread([&, j]() {
			mpz_class modularInverse, prime;
			const uint64_t endIndex(std::min(_primorialNumber + (j + 1)*blockSize, _nPrimes));
			for (uint64_t i(_primorialNumber + j*blockSize) ; i < endIndex ; i++) {
				uint64_t p(_getPrime(i));
				mpz_set_ui(prime.get_mpz_t(), p);
				mpz_invert(modularInverse.get_mpz_t(), _primorial.get_mpz_t(), prime.get_mpz_t()); // modularInverse*primorial ≡ 1 (mod prime)
				if (i < nPrimesTo2p32) _modularInverses32[i] = static_cast<uint32_t>(mpz_get_ui(modularInverse.get_mpz_t()));
				else _modularInverses64[i - nPrimesTo2p32] = mpz_get_ui(modularInverse.get_mpz_t());
#ifdef __SSE2__
				if (i < precompPrimes)
					rie_mod_1s_4p_cps(&_modPrecompute[i], p);
#endif
			}
		});
	}
	for (uint16_t j(0) ; j < _threads ; j++)
		threads[j].join();
	_modularInversesGenerationTime = timeSince(t0);
	
	uint64_t additionalFactorsCountEstimation(0); // tupleSize*factorMax*(sum of 1/p, for p in the prime table >= factorMax); it is the estimation of how many such p will eliminate a factor (factorMax/p being the probability of the modulo p being < factorMax)
	double sumInversesOfPrimes(0.);
	_primesIndexThreshold = 0; // Number of prime numbers smaller than factorMax in the table
	for (uint64_t i(0) ; i < _nPrimes ; i++) {
		const uint64_t p(_getPrime(i));
		if (p >= _factorMax) {
			if (_primesIndexThreshold == 0) {
				_primesIndexThreshold = i;
				if (_primesIndexThreshold % 2 == 1) // Needs to be even to use SIMD sieving optimizations
					_primesIndexThreshold--;
			}
			sumInversesOfPrimes += 1./static_cast<double>(p);
		}
	}
	if (_primesIndexThreshold == 0)
		_primesIndexThreshold = _nPrimes;
	const uint64_t factorsToEliminateEntries(_pattern.size()*_primesIndexThreshold); // PatternLength entries for every prime < factorMax
	additionalFactorsCountEstimation = _pattern.size()*ceil(static_cast<double>(_factorMax)*sumInversesOfPrimes);
	const uint64_t additionalFactorsEntriesPerIteration(17ULL*(additionalFactorsCountEstimation/_sieveIterations)/16ULL + 64ULL); // Have some margin
	try {
		_sieves = std::vector<Sieve>(_sieveWorkers);
		for (std::vector<Sieve>::size_type i(0) ; i < _sieves.size() ; i++) {
			_sieves[i].id = i;
			_sieves[i].additionalFactorsToEliminateCounts = new std::atomic<uint64_t>[_sieveIterations];
			_sieves[i].factorsTable = new uint64_t[_sieveWords];
#ifdef __SSE2__
			_sieves[i].factorsToEliminate = reinterpret_cast<uint32_t*>(new __m256i[(factorsToEliminateEntries + 7) / 8]);
#else
			_sieves[i].factorsToEliminate = new uint32_t[factorsToEliminateEntries];
#endif
			memset(_sieves[i].factorsToEliminate, 0, sizeof(uint32_t)*factorsToEliminateEntries);
			_sieves[i].additionalFactorsToEliminate = new uint32_t*[_sieveIterations];
			for (uint64_t j(0) ; j < _sieveIterations ; j++)
				_sieves[i].additionalFactorsToEliminate[j] = new uint32_t[additionalFactorsEntriesPerIteration];
		}
	}
	catch (std::bad_alloc& ba) {
		_initMessages.push_back("Unable to allocate memory for the sieves. Try to reduce the PrimeTableLimit parameter.\n"s);
		return;
	}
	// Initial guess at a value for the Target.
	_nRemainingCheckTasksTarget = 32U*_threads*_sieveWorkers;
	_inited = true;
}

void Instance::startThreads() {
	assert(_inited && !_running);
	_running = true;
	if (!_keepStats)
		_tupleCounts = std::vector<uint64_t>(_pattern.size() + 1, 0ULL);
	_keepStats = false;
	_masterThread = std::thread(&Instance::_manageTasks, this);
	for (uint16_t i(0) ; i < _threads ; i++)
		_workerThreads.push_back(std::thread(&Instance::_doTasks, this, i));
}

void Instance::stopThreads() {
	assert(_running);
	_running = false;
	invalidateWork();
	_tasksDoneInfos.push_front(TaskDoneInfo{Task::Type::Dummy, {}}); // Unblock if master thread stuck in blocking_pop_front().
	_masterThread.join();
	for (uint16_t i(0) ; i < _threads ; i++)
		_tasks.push_front(Task{Task::Type::Dummy, 0, {}}); // Unblock worker threads stuck in blocking_pop_front().
	for (auto &workerThread : _workerThreads)
		workerThread.join();
	_workerThreads.clear();
	_availableJobs.clear();
	_presieveTasks.clear();
	_tasks.clear();
	_tasksDoneInfos.clear();
	for (auto &work : _works) work.clear();
}

void Instance::clear() {
	assert(_inited && !_running);
	_inited = false;
	for (auto &sieve : _sieves) {
		delete[] sieve.factorsTable;
#ifdef __SSE2__
		delete[] reinterpret_cast<__m256i*>(sieve.factorsToEliminate);
#else
		delete[] sieve.factorsToEliminate;
#endif
		for (uint64_t j(0) ; j < _sieveIterations ; j++)
			delete[] sieve.additionalFactorsToEliminate[j];
		delete[] sieve.additionalFactorsToEliminate;
		delete[] sieve.additionalFactorsToEliminateCounts;
	}
	_sieves.clear();
	_primes32.clear();
	_primes64.clear();
	_modularInverses32.clear();
	_modularInverses64.clear();
#ifdef __SSE2__
	_modPrecompute.clear();
#endif
	_primorialOffsets.clear();
	_primorialOffsetsU64.clear();
	_pattern.clear();
	_halfPattern.clear();
	_primorialOffsetDiff.clear();
	_patternMin.clear();
}

void Instance::_addCachedAdditionalFactorsToEliminate(Sieve& sieve, uint64_t *factorsCache, uint64_t *factorsCacheCounts, const int factorsCacheTotalCount) {
	for (uint64_t i(0) ; i < _sieveIterations ; i++) // Initialize the counts for use as index and update the sieve's one
		factorsCacheCounts[i] = sieve.additionalFactorsToEliminateCounts[i].fetch_add(factorsCacheCounts[i]);
	for (int i(0) ; i < factorsCacheTotalCount ; i++) {
		const uint64_t factor(factorsCache[i]),
		               sieveIteration(factor >> _sieveBits),
		               indexInFactorsTable(factorsCacheCounts[sieveIteration]);
		sieve.additionalFactorsToEliminate[sieveIteration][indexInFactorsTable] = factor & (_sieveSize - 1); // factor % sieveSize
		factorsCacheCounts[sieveIteration]++;
	}
	for (uint64_t i(0) ; i < _sieveIterations ; i++)
		factorsCacheCounts[i] = 0;
}

void Instance::_doPresieveTask(const Task &task) {
	const uint64_t workIndex(task.workIndex), firstPrimeIndex(task.presieve.start), lastPrimeIndex(task.presieve.end);
	const mpz_class firstCandidate(_works[workIndex].primorialMultipleStart + _primorialOffsets[0]);
	std::array<int, maxSieveWorkers> factorsCacheTotalCounts{0};
	uint64_t** factorsCacheRef(factorsCache); // On Windows, caching these thread_local pointers on the stack makes a noticeable perf difference.
	uint64_t** factorsCacheCountsRef(factorsCacheCounts);
#ifdef __SSE2__
	const uint64_t precompLimit(_modPrecompute.size()), tupleSize(_pattern.size());
	uint64_t avxLimit(0);
#ifdef __AVX2__
	const uint64_t avxWidth(8);
#else
	const uint64_t avxWidth(4);
#endif
	if (sysInfo.hasAVX()) {
		avxLimit = nPrimesTo2p32 - avxWidth;
		avxLimit -= (avxLimit - firstPrimeIndex) & (avxWidth - 1);  // Must be enough primes in range to use AVX
	}
	uint64_t nextRemainder[8];
	uint64_t nextRemainderIndex(8);
#else
	const uint64_t tupleSize(_pattern.size());
#endif
	for (uint64_t i(firstPrimeIndex) ; i < lastPrimeIndex ; i++) {
		const uint64_t p(_getPrime(i));
		uint64_t mi[4];
		mi[0] = _getModularInverse(i); // Modular inverse of the primorial: mi[0]*primorial ≡ 1 (mod p). The modularInverses were precomputed in init().
		mi[1] = (mi[0] << 1); // mi[i] = (2*i*mi[0]) % p for i > 0.
		if (mi[1] >= p) mi[1] -= p;
		mi[2] = mi[1] << 1;
		if (mi[2] >= p) mi[2] -= p;
		mi[3] = mi[1] + mi[2];
		if (mi[3] >= p) mi[3] -= p;
		// Compute the first eliminated primorial factor for p fp, using precomputation speed up if available.
		// fp is the solution of firstCandidate + primorial*f ≡ 0 (mod p) for 0 <= f < p: fp = (p - (firstCandidate % p))*mi[0] % p.
		// In the sieving phase, numbers of the form firstCandidate + (p*i + fp)*primorial for 0 <= i < factorMax are eliminated as they are divisible by p.
		// This is for the first number of the constellation. Later, the mi[1-3] will be used to adjust fp for the other elements of the constellation.
#ifdef __SSE2__
		uint64_t fp, cnt(0ULL), ps(0ULL);
		if (i < precompLimit) { // Assembly optimized computation of fp by Michael Bell
			bool haveRemainder(false);
			if (nextRemainderIndex < avxWidth) {
				fp = nextRemainder[nextRemainderIndex++];
				cnt = __builtin_clzll(p);
				ps = p << cnt;
				haveRemainder = true;
			}
			else if (i < avxLimit) {
				cnt = __builtin_clz(static_cast<uint32_t>(p));
				if (__builtin_clz(static_cast<uint32_t>(_primes32[i + avxWidth - 1])) == cnt) {
					uint32_t ps32[8];
					for (uint64_t j(0) ; j < avxWidth; j++) {
						ps32[j] = static_cast<uint32_t>(_primes32[i + j]) << cnt;
						nextRemainder[j] = _modularInverses32[i + j];
					}
#ifdef __AVX2__
					rie_mod_1s_2p_8times(firstCandidate.get_mpz_t()->_mp_d, firstCandidate.get_mpz_t()->_mp_size, &ps32[0], cnt, &_modPrecompute[i], &nextRemainder[0]);
#else
					rie_mod_1s_2p_4times(firstCandidate.get_mpz_t()->_mp_d, firstCandidate.get_mpz_t()->_mp_size, &ps32[0], cnt, &_modPrecompute[i], &nextRemainder[0]);
#endif
					haveRemainder = true;
					fp = nextRemainder[0];
					nextRemainderIndex = 1;
					cnt += 32ULL;
					ps = static_cast<uint64_t>(ps32[0]) << 32ULL;
				}
			}
			
			if (!haveRemainder) {
				cnt = __builtin_clzll(p);
				ps = p << cnt;
				const uint64_t remainder(rie_mod_1s_4p(firstCandidate.get_mpz_t()->_mp_d, firstCandidate.get_mpz_t()->_mp_size, ps, cnt, &_modPrecompute[i]));
				const uint64_t pa(ps - remainder);
				uint64_t r, n[2];
				umul_ppmm(n[1], n[0], pa, mi[0]);
				udiv_rnnd_preinv(r, n[1], n[0], ps, _modPrecompute[i]);
				fp = r >> cnt;
			}
		}
		else { // Basic computation of fp
			const uint64_t remainder(mpz_tdiv_ui(firstCandidate.get_mpz_t(), p)), pa(p - remainder);
			uint64_t q, n[2];
			umul_ppmm(n[1], n[0], pa, mi[0]);
			udiv_qrnnd(q, fp, n[1], n[0], p);
		}
#else
		const uint64_t remainder(mpz_tdiv_ui(firstCandidate.get_mpz_t(), p)), pa(p - remainder);
		uint64_t fp(mulMod(pa, mi[0], p)); // (pa*mi[0]) % p
#endif

		// We use a macro here to ensure the compiler inlines the code, and also make it easier to early out of the function completely if the current height has changed.
#define addFactorsToEliminateForP(sieveWorkerIndex) {						                                                   \
			if (i < _primesIndexThreshold) {			                                                                       \
				_sieves[sieveWorkerIndex].factorsToEliminate[tupleSize*i] = fp;		                                           \
				for (std::vector<uint64_t>::size_type f(1) ; f < _halfPattern.size() ; f++) {		                           \
					if (fp < mi[_halfPattern[f]]) fp += p;	                                                                   \
					fp -= mi[_halfPattern[f]];	                                                                               \
					_sieves[sieveWorkerIndex].factorsToEliminate[tupleSize*i + f] = fp;	                                       \
				}		                                                                                                       \
			}			                                                                                                       \
			else {			                                                                                                   \
				if (factorsCacheTotalCounts[sieveWorkerIndex] + _halfPattern.size() >= factorsCacheSize) {		               \
					if (!_works[workIndex].current)	                                                                           \
						return;                                                                                                \
					_addCachedAdditionalFactorsToEliminate(_sieves[sieveWorkerIndex], factorsCacheRef[sieveWorkerIndex], factorsCacheCountsRef[sieveWorkerIndex], factorsCacheTotalCounts[sieveWorkerIndex]); \
					factorsCacheTotalCounts[sieveWorkerIndex] = 0;	                                                           \
				}		                                                                                                       \
				if (fp < _factorMax) {		                                                                                   \
					factorsCacheRef[sieveWorkerIndex][factorsCacheTotalCounts[sieveWorkerIndex]++] = fp;	                   \
					factorsCacheCountsRef[sieveWorkerIndex][fp >> _sieveBits]++;	                                           \
				}		                                                                                                       \
				for (std::vector<uint64_t>::size_type f(1) ; f < _halfPattern.size() ; f++) {		                           \
					if (fp < mi[_halfPattern[f]]) fp += p;	                                                                   \
					fp -= mi[_halfPattern[f]];	                                                                               \
					if (fp < _factorMax) {	                                                                                   \
						factorsCacheRef[sieveWorkerIndex][factorsCacheTotalCounts[sieveWorkerIndex]++] = fp;                   \
						factorsCacheCountsRef[sieveWorkerIndex][fp >> _sieveBits]++;                                           \
					}	                                                                                                       \
				}		                                                                                                       \
			}		                                                                                                           \
		};
		addFactorsToEliminateForP(0);
		if (_sieveWorkers == 1) continue;
		
		// Recompute fp to adjust to the PrimorialOffsets of other Sieve Workers.
#ifdef __SSE2__
		uint64_t r;
#define recomputeFp(sieveWorkerIndex) {				                                      \
			if (i < precompLimit && _primorialOffsetDiff[sieveWorkerIndex - 1] < p) {	  \
				uint64_t n[2];                                                            \
				uint64_t os(_primorialOffsetDiff[sieveWorkerIndex - 1] << cnt);           \
				umul_ppmm(n[1], n[0], os, mi[0]);                                         \
				udiv_rnnd_preinv(r, n[1], n[0], ps, _modPrecompute[i]);                   \
				r >>= cnt;                                                                \
			}	                                                                          \
			else {	                                                                      \
				uint64_t q, n[2];                                                         \
				umul_ppmm(n[1], n[0], _primorialOffsetDiff[sieveWorkerIndex - 1], mi[0]); \
				udiv_qrnnd(q, r, n[1], n[0], p);                                          \
			}                                                                             \
		}
		recomputeFp(1);
#else
		uint64_t r(mulMod(_primorialOffsetDiff[0], mi[0], p));
#endif
		if (fp < r) fp += p;
		fp -= r;
		addFactorsToEliminateForP(1);
		
		for (int j(2) ; j < _sieveWorkers ; j++) {
			if (_primorialOffsetDiff[j - 1] != _primorialOffsetDiff[j - 2])
#ifdef __SSE2__
				recomputeFp(j);
#else
				r = mulMod(_primorialOffsetDiff[j - 1], mi[0], p);
#endif
			if (fp < r) fp += p;
			fp -= r;
			addFactorsToEliminateForP(j);
		}
	}
	
	if (lastPrimeIndex > _primesIndexThreshold) {
		for (int j(0) ; j < _sieveWorkers ; j++) {
			if (factorsCacheTotalCounts[j] > 0) {
				_addCachedAdditionalFactorsToEliminate(_sieves[j], factorsCacheRef[j], factorsCacheCountsRef[j], factorsCacheTotalCounts[j]);
				factorsCacheTotalCounts[j] = 0;
			}
		}
	}
}

void Instance::_processSieve(uint64_t *factorsTable, uint32_t* factorsToEliminate, const uint64_t firstPrimeIndex, const uint64_t lastPrimeIndex) {
	const uint64_t tupleSize(_pattern.size());
	std::array<uint32_t, sieveCacheSize> sieveCache{0};
	uint64_t sieveCachePos(0);
	for (uint64_t i(firstPrimeIndex) ; i < lastPrimeIndex ; i++) {
		const uint32_t p(_primes32[i]);
		for (uint64_t f(0) ; f < tupleSize; f++) {
			while (factorsToEliminate[i*tupleSize + f] < _sieveSize) { // Eliminate primorial factors of the form p*m + fp for every m*p in the current table.
				_addToSieveCache(factorsTable, sieveCache, sieveCachePos, factorsToEliminate[i*tupleSize + f]);
				factorsToEliminate[i*tupleSize + f] += p; // Increment the m
			}
			factorsToEliminate[i*tupleSize + f] -= _sieveSize; // Prepare for the next iteration
		}
	}
	_endSieveCache(factorsTable, sieveCache);
}

#ifdef __SSE2__
void Instance::_processSieve6(uint64_t *factorsTable, uint32_t* factorsToEliminate, uint64_t firstPrimeIndex, const uint64_t lastPrimeIndex) { // Assembly optimized sieving for 6-tuples by Michael Bell
	assert(_pattern.size() == 6);
	assert((lastPrimeIndex & 1) == 0);
	// Already eliminate for the first prime to sieve if it is odd to align for the optimizations
	if ((firstPrimeIndex & 1) != 0) {
		for (uint64_t f(0) ; f < 6 ; f++) {
			while (factorsToEliminate[firstPrimeIndex*6 + f] < _sieveSize) {
				factorsTable[factorsToEliminate[firstPrimeIndex*6 + f] >> 6U] |= (1ULL << ((factorsToEliminate[firstPrimeIndex*6 + f] & 63U)));
				factorsToEliminate[firstPrimeIndex*6 + f] += _primes32[firstPrimeIndex];
			}
			factorsToEliminate[firstPrimeIndex*6 + f] -= _sieveSize;
		}
		firstPrimeIndex++;
	}
	xmmreg_t offsetmax;
	offsetmax.m128 = _mm_set1_epi32(_sieveSize);
	for (uint64_t i(firstPrimeIndex) ; i < lastPrimeIndex ; i += 2) {
		xmmreg_t p1, p2, p3;
		xmmreg_t factor1, factor2, factor3, nextIncr1, nextIncr2, nextIncr3;
		xmmreg_t cmpres1, cmpres2, cmpres3;
		p1.m128 = _mm_set1_epi32(_primes32[i]);
		p3.m128 = _mm_set1_epi32(_primes32[i + 1]);
		p2.m128 = _mm_castps_si128(_mm_shuffle_ps(_mm_castsi128_ps(p1.m128), _mm_castsi128_ps(p3.m128), _MM_SHUFFLE(0, 0, 0, 0)));
		factor1.m128 = _mm_load_si128(reinterpret_cast<__m128i const*>(&factorsToEliminate[i*6 + 0]));
		factor2.m128 = _mm_load_si128(reinterpret_cast<__m128i const*>(&factorsToEliminate[i*6 + 4]));
		factor3.m128 = _mm_load_si128(reinterpret_cast<__m128i const*>(&factorsToEliminate[i*6 + 8]));
		while (true) {
			cmpres1.m128 = _mm_cmpgt_epi32(offsetmax.m128, factor1.m128);
			cmpres2.m128 = _mm_cmpgt_epi32(offsetmax.m128, factor2.m128);
			cmpres3.m128 = _mm_cmpgt_epi32(offsetmax.m128, factor3.m128);
			const int mask1(_mm_movemask_epi8(cmpres1.m128));
			const int mask2(_mm_movemask_epi8(cmpres2.m128));
			const int mask3(_mm_movemask_epi8(cmpres3.m128));
			if ((mask1 == 0) && (mask2 == 0) && (mask3 == 0)) break;
			if (mask1 & 0x0008) factorsTable[factor1.v[0] >> 6] |= (1ULL << (factor1.v[0] & 63ULL));
			if (mask1 & 0x0080) factorsTable[factor1.v[1] >> 6] |= (1ULL << (factor1.v[1] & 63ULL));
			if (mask1 & 0x0800) factorsTable[factor1.v[2] >> 6] |= (1ULL << (factor1.v[2] & 63ULL));
			if (mask1 & 0x8000) factorsTable[factor1.v[3] >> 6] |= (1ULL << (factor1.v[3] & 63ULL));
			if (mask2 & 0x0008) factorsTable[factor2.v[0] >> 6] |= (1ULL << (factor2.v[0] & 63ULL));
			if (mask2 & 0x0080) factorsTable[factor2.v[1] >> 6] |= (1ULL << (factor2.v[1] & 63ULL));
			if (mask2 & 0x0800) factorsTable[factor2.v[2] >> 6] |= (1ULL << (factor2.v[2] & 63ULL));
			if (mask2 & 0x8000) factorsTable[factor2.v[3] >> 6] |= (1ULL << (factor2.v[3] & 63ULL));
			if (mask3 & 0x0008) factorsTable[factor3.v[0] >> 6] |= (1ULL << (factor3.v[0] & 63ULL));
			if (mask3 & 0x0080) factorsTable[factor3.v[1] >> 6] |= (1ULL << (factor3.v[1] & 63ULL));
			if (mask3 & 0x0800) factorsTable[factor3.v[2] >> 6] |= (1ULL << (factor3.v[2] & 63ULL));
			if (mask3 & 0x8000) factorsTable[factor3.v[3] >> 6] |= (1ULL << (factor3.v[3] & 63ULL));
			nextIncr1.m128 = _mm_and_si128(cmpres1.m128, p1.m128);
			nextIncr2.m128 = _mm_and_si128(cmpres2.m128, p2.m128);
			nextIncr3.m128 = _mm_and_si128(cmpres3.m128, p3.m128);
			factor1.m128 = _mm_add_epi32(factor1.m128, nextIncr1.m128);
			factor2.m128 = _mm_add_epi32(factor2.m128, nextIncr2.m128);
			factor3.m128 = _mm_add_epi32(factor3.m128, nextIncr3.m128);
		}
		factor1.m128 = _mm_sub_epi32(factor1.m128, offsetmax.m128);
		factor2.m128 = _mm_sub_epi32(factor2.m128, offsetmax.m128);
		factor3.m128 = _mm_sub_epi32(factor3.m128, offsetmax.m128);
		_mm_store_si128(reinterpret_cast<__m128i*>(&factorsToEliminate[i*6 + 0]), factor1.m128);
		_mm_store_si128(reinterpret_cast<__m128i*>(&factorsToEliminate[i*6 + 4]), factor2.m128);
		_mm_store_si128(reinterpret_cast<__m128i*>(&factorsToEliminate[i*6 + 8]), factor3.m128);
	}
}

void Instance::_processSieve7(uint64_t *factorsTable, uint32_t* factorsToEliminate, uint64_t firstPrimeIndex, const uint64_t lastPrimeIndex) { // Assembly optimized sieving for 7-tuples by Michael Bell
	assert(_pattern.size() == 7);
	std::array<uint32_t, sieveCacheSize> sieveCache{0};
	uint64_t sieveCachePos(0);
	xmmreg_t offsetmax;
	offsetmax.m128 = _mm_set1_epi32(_sieveSize);
	for (uint64_t i(firstPrimeIndex) ; i < lastPrimeIndex ; i += 1) {
		xmmreg_t p1;
		xmmreg_t factor1, factor2, nextIncr1, nextIncr2;
		xmmreg_t cmpres1, cmpres2;
		p1.m128 = _mm_set1_epi32(_primes32[i]);
		factor1.m128 = _mm_loadu_si128(reinterpret_cast<__m128i const*>(&factorsToEliminate[i*7 + 0]));
		factor2.m128 = _mm_loadu_si128(reinterpret_cast<__m128i const*>(&factorsToEliminate[i*7 + 3]));
		while (true) {
			cmpres1.m128 = _mm_cmpgt_epi32(offsetmax.m128, factor1.m128);
			cmpres2.m128 = _mm_cmpgt_epi32(offsetmax.m128, factor2.m128);
			const int mask1(_mm_movemask_epi8(cmpres1.m128));
			const int mask2(_mm_movemask_epi8(cmpres2.m128));
			if ((mask1 == 0) && (mask2 == 0)) break;
			if (mask1 & 0x0008) _addToSieveCache(factorsTable, sieveCache, sieveCachePos, factor1.v[0]);
			if (mask1 & 0x0080) _addToSieveCache(factorsTable, sieveCache, sieveCachePos, factor1.v[1]);
			if (mask1 & 0x0800) _addToSieveCache(factorsTable, sieveCache, sieveCachePos, factor1.v[2]);
			if (mask1 & 0x8000) _addToSieveCache(factorsTable, sieveCache, sieveCachePos, factor1.v[3]);
			if (mask2 & 0x0080) _addToSieveCache(factorsTable, sieveCache, sieveCachePos, factor2.v[1]);
			if (mask2 & 0x0800) _addToSieveCache(factorsTable, sieveCache, sieveCachePos, factor2.v[2]);
			if (mask2 & 0x8000) _addToSieveCache(factorsTable, sieveCache, sieveCachePos, factor2.v[3]);
			nextIncr1.m128 = _mm_and_si128(cmpres1.m128, p1.m128);
			nextIncr2.m128 = _mm_and_si128(cmpres2.m128, p1.m128);
			factor1.m128 = _mm_add_epi32(factor1.m128, nextIncr1.m128);
			factor2.m128 = _mm_add_epi32(factor2.m128, nextIncr2.m128);
		}
		factor1.m128 = _mm_sub_epi32(factor1.m128, offsetmax.m128);
		factor2.m128 = _mm_sub_epi32(factor2.m128, offsetmax.m128);
		_mm_storeu_si128(reinterpret_cast<__m128i*>(&factorsToEliminate[i*7 + 0]), factor1.m128);
		_mm_storeu_si128(reinterpret_cast<__m128i*>(&factorsToEliminate[i*7 + 3]), factor2.m128);
	}
	_endSieveCache(factorsTable, sieveCache);
}

void Instance::_processSieve8(uint64_t *factorsTable, uint32_t* factorsToEliminate, uint64_t firstPrimeIndex, const uint64_t lastPrimeIndex) { // Assembly optimized sieving for 8-tuples by Michael Bell
	assert(_pattern.size() == 8);
	std::array<uint32_t, sieveCacheSize> sieveCache{0};
	uint64_t sieveCachePos(0);
	xmmreg_t offsetmax;
	offsetmax.m128 = _mm_set1_epi32(_sieveSize);
	for (uint64_t i(firstPrimeIndex) ; i < lastPrimeIndex ; i += 1) {
		xmmreg_t p1;
		xmmreg_t factor1, factor2, nextIncr1, nextIncr2;
		xmmreg_t cmpres1, cmpres2;
		p1.m128 = _mm_set1_epi32(_primes32[i]);
		factor1.m128 = _mm_load_si128(reinterpret_cast<__m128i const*>(&factorsToEliminate[i*8 + 0]));
		factor2.m128 = _mm_load_si128(reinterpret_cast<__m128i const*>(&factorsToEliminate[i*8 + 4]));
		while (true) {
			cmpres1.m128 = _mm_cmpgt_epi32(offsetmax.m128, factor1.m128);
			cmpres2.m128 = _mm_cmpgt_epi32(offsetmax.m128, factor2.m128);
			const int mask1(_mm_movemask_epi8(cmpres1.m128));
			const int mask2(_mm_movemask_epi8(cmpres2.m128));
			if ((mask1 == 0) && (mask2 == 0)) break;
			if (mask1 & 0x0008) _addToSieveCache(factorsTable, sieveCache, sieveCachePos, factor1.v[0]);
			if (mask1 & 0x0080) _addToSieveCache(factorsTable, sieveCache, sieveCachePos, factor1.v[1]);
			if (mask1 & 0x0800) _addToSieveCache(factorsTable, sieveCache, sieveCachePos, factor1.v[2]);
			if (mask1 & 0x8000) _addToSieveCache(factorsTable, sieveCache, sieveCachePos, factor1.v[3]);
			if (mask2 & 0x0008) _addToSieveCache(factorsTable, sieveCache, sieveCachePos, factor2.v[0]);
			if (mask2 & 0x0080) _addToSieveCache(factorsTable, sieveCache, sieveCachePos, factor2.v[1]);
			if (mask2 & 0x0800) _addToSieveCache(factorsTable, sieveCache, sieveCachePos, factor2.v[2]);
			if (mask2 & 0x8000) _addToSieveCache(factorsTable, sieveCache, sieveCachePos, factor2.v[3]);
			nextIncr1.m128 = _mm_and_si128(cmpres1.m128, p1.m128);
			nextIncr2.m128 = _mm_and_si128(cmpres2.m128, p1.m128);
			factor1.m128 = _mm_add_epi32(factor1.m128, nextIncr1.m128);
			factor2.m128 = _mm_add_epi32(factor2.m128, nextIncr2.m128);
		}
		factor1.m128 = _mm_sub_epi32(factor1.m128, offsetmax.m128);
		factor2.m128 = _mm_sub_epi32(factor2.m128, offsetmax.m128);
		_mm_store_si128(reinterpret_cast<__m128i*>(&factorsToEliminate[i*8 + 0]), factor1.m128);
		_mm_store_si128(reinterpret_cast<__m128i*>(&factorsToEliminate[i*8 + 4]), factor2.m128);
	}
	_endSieveCache(factorsTable, sieveCache);
}

#ifdef __AVX2__
void Instance::_processSieve7_avx2(uint64_t *factorsTable, uint32_t* factorsToEliminate, uint64_t firstPrimeIndex, const uint64_t lastPrimeIndex) { // Assembly optimized sieving for 7-tuples by Michael Bell
	assert(_pattern.size() == 7);
	std::array<uint32_t, sieveCacheSize> sieveCache{0};
	uint64_t sieveCachePos(0);
	assert((lastPrimeIndex & 1) == 0);
	// Already eliminate for the first prime to sieve if it is odd to align for the optimizations
	if ((firstPrimeIndex & 1) != 0) {
		for (uint64_t f(0) ; f < 7 ; f++) {
			while (factorsToEliminate[firstPrimeIndex*7 + f] < _sieveSize) {
				_addToSieveCache(factorsTable, sieveCache, sieveCachePos, factorsToEliminate[firstPrimeIndex*7 + f]);
				factorsToEliminate[firstPrimeIndex*7 + f] += _primes32[firstPrimeIndex];
			}
			factorsToEliminate[firstPrimeIndex*7 + f] -= _sieveSize;
		}
		firstPrimeIndex++;
	}
	
	ymmreg_t offsetmax;
	offsetmax.m256 = _mm256_set1_epi32(_sieveSize);
	ymmreg_t storemask;
	storemask.m256 = _mm256_set1_epi32(0xffffffff);
	storemask.v[0] = 0;
	for (uint64_t i(firstPrimeIndex) ; i < lastPrimeIndex ; i += 2) {
		ymmreg_t p1, p2;
		ymmreg_t factor1, factor2, nextIncr1, nextIncr2;
		ymmreg_t cmpres1, cmpres2;
		p1.m256 = _mm256_set1_epi32(_primes32[i]);
		p2.m256 = _mm256_set1_epi32(_primes32[i + 1]);
		factor1.m256 = _mm256_loadu_si256(reinterpret_cast<__m256i const*>(&factorsToEliminate[i*7 + 0]));
		factor2.m256 = _mm256_loadu_si256(reinterpret_cast<__m256i const*>(&factorsToEliminate[i*7 + 6]));
		while (true) {
			cmpres1.m256 = _mm256_cmpgt_epi32(offsetmax.m256, factor1.m256);
			cmpres2.m256 = _mm256_cmpgt_epi32(offsetmax.m256, factor2.m256);
			const int mask1(_mm256_movemask_epi8(cmpres1.m256));
			const int mask2(_mm256_movemask_epi8(cmpres2.m256));
			if ((mask1 == 0) && (mask2 == 0)) break;
			if (mask1 & 0x00000008) _addToSieveCache(factorsTable, sieveCache, sieveCachePos, factor1.v[0]);
			if (mask1 & 0x00000080) _addToSieveCache(factorsTable, sieveCache, sieveCachePos, factor1.v[1]);
			if (mask1 & 0x00000800) _addToSieveCache(factorsTable, sieveCache, sieveCachePos, factor1.v[2]);
			if (mask1 & 0x00008000) _addToSieveCache(factorsTable, sieveCache, sieveCachePos, factor1.v[3]);
			if (mask1 & 0x00080000) _addToSieveCache(factorsTable, sieveCache, sieveCachePos, factor1.v[4]);
			if (mask1 & 0x00800000) _addToSieveCache(factorsTable, sieveCache, sieveCachePos, factor1.v[5]);
			if (mask1 & 0x08000000) _addToSieveCache(factorsTable, sieveCache, sieveCachePos, factor1.v[6]);
			if (mask2 & 0x00000080) _addToSieveCache(factorsTable, sieveCache, sieveCachePos, factor2.v[1]);
			if (mask2 & 0x00000800) _addToSieveCache(factorsTable, sieveCache, sieveCachePos, factor2.v[2]);
			if (mask2 & 0x00008000) _addToSieveCache(factorsTable, sieveCache, sieveCachePos, factor2.v[3]);
			if (mask2 & 0x00080000) _addToSieveCache(factorsTable, sieveCache, sieveCachePos, factor2.v[4]);
			if (mask2 & 0x00800000) _addToSieveCache(factorsTable, sieveCache, sieveCachePos, factor2.v[5]);
			if (mask2 & 0x08000000) _addToSieveCache(factorsTable, sieveCache, sieveCachePos, factor2.v[6]);
			if (mask2 & 0x80000000) _addToSieveCache(factorsTable, sieveCache, sieveCachePos, factor2.v[7]);
			nextIncr1.m256 = _mm256_and_si256(cmpres1.m256, p1.m256);
			nextIncr2.m256 = _mm256_and_si256(cmpres2.m256, p2.m256);
			factor1.m256 = _mm256_add_epi32(factor1.m256, nextIncr1.m256);
			factor2.m256 = _mm256_add_epi32(factor2.m256, nextIncr2.m256);
		}
		factor1.m256 = _mm256_sub_epi32(factor1.m256, offsetmax.m256);
		factor2.m256 = _mm256_sub_epi32(factor2.m256, offsetmax.m256);
		_mm256_storeu_si256(reinterpret_cast<__m256i*>(&factorsToEliminate[i*7 + 0]), factor1.m256);
		_mm256_maskstore_epi32(reinterpret_cast<int*>(&factorsToEliminate[i*7 + 6]), storemask.m256, factor2.m256);
	}
	_endSieveCache(factorsTable, sieveCache);
}

void Instance::_processSieve8_avx2(uint64_t *factorsTable, uint32_t* factorsToEliminate, uint64_t firstPrimeIndex, const uint64_t lastPrimeIndex) { // Assembly optimized sieving for 8-tuples by Michael Bell
	assert(_pattern.size() == 8);
	std::array<uint32_t, sieveCacheSize> sieveCache{0};
	uint64_t sieveCachePos(0);

	assert((lastPrimeIndex & 1) == 0);
	// Already eliminate for the first prime to sieve if it is odd to align for the optimizations
	if ((firstPrimeIndex & 1) != 0) {
		for (uint64_t f(0) ; f < 8 ; f++) {
			while (factorsToEliminate[firstPrimeIndex*8 + f] < _sieveSize) {
				_addToSieveCache(factorsTable, sieveCache, sieveCachePos, factorsToEliminate[firstPrimeIndex*8 + f]);
				factorsToEliminate[firstPrimeIndex*8 + f] += _primes32[firstPrimeIndex];
			}
			factorsToEliminate[firstPrimeIndex*8 + f] -= _sieveSize;
		}
		firstPrimeIndex++;
	}

	ymmreg_t offsetmax;
	offsetmax.m256 = _mm256_set1_epi32(_sieveSize);
	for (uint64_t i(firstPrimeIndex) ; i < lastPrimeIndex ; i += 2) {
		ymmreg_t p1, p2;
		ymmreg_t factor1, factor2, nextIncr1, nextIncr2;
		ymmreg_t cmpres1, cmpres2;
		p1.m256 = _mm256_set1_epi32(_primes32[i]);
		p2.m256 = _mm256_set1_epi32(_primes32[i + 1]);
		factor1.m256 = _mm256_load_si256(reinterpret_cast<__m256i const*>(&factorsToEliminate[i*8 + 0]));
		factor2.m256 = _mm256_load_si256(reinterpret_cast<__m256i const*>(&factorsToEliminate[i*8 + 8]));
		while (true) {
			cmpres1.m256 = _mm256_cmpgt_epi32(offsetmax.m256, factor1.m256);
			cmpres2.m256 = _mm256_cmpgt_epi32(offsetmax.m256, factor2.m256);
			const int mask1(_mm256_movemask_epi8(cmpres1.m256));
			const int mask2(_mm256_movemask_epi8(cmpres2.m256));
			if ((mask1 == 0) && (mask2 == 0)) break;
			if (mask1 & 0x00000008) _addToSieveCache(factorsTable, sieveCache, sieveCachePos, factor1.v[0]);
			if (mask1 & 0x00000080) _addToSieveCache(factorsTable, sieveCache, sieveCachePos, factor1.v[1]);
			if (mask1 & 0x00000800) _addToSieveCache(factorsTable, sieveCache, sieveCachePos, factor1.v[2]);
			if (mask1 & 0x00008000) _addToSieveCache(factorsTable, sieveCache, sieveCachePos, factor1.v[3]);
			if (mask1 & 0x00080000) _addToSieveCache(factorsTable, sieveCache, sieveCachePos, factor1.v[4]);
			if (mask1 & 0x00800000) _addToSieveCache(factorsTable, sieveCache, sieveCachePos, factor1.v[5]);
			if (mask1 & 0x08000000) _addToSieveCache(factorsTable, sieveCache, sieveCachePos, factor1.v[6]);
			if (mask1 & 0x80000000) _addToSieveCache(factorsTable, sieveCache, sieveCachePos, factor1.v[7]);
			if (mask2 & 0x00000008) _addToSieveCache(factorsTable, sieveCache, sieveCachePos, factor2.v[0]);
			if (mask2 & 0x00000080) _addToSieveCache(factorsTable, sieveCache, sieveCachePos, factor2.v[1]);
			if (mask2 & 0x00000800) _addToSieveCache(factorsTable, sieveCache, sieveCachePos, factor2.v[2]);
			if (mask2 & 0x00008000) _addToSieveCache(factorsTable, sieveCache, sieveCachePos, factor2.v[3]);
			if (mask2 & 0x00080000) _addToSieveCache(factorsTable, sieveCache, sieveCachePos, factor2.v[4]);
			if (mask2 & 0x00800000) _addToSieveCache(factorsTable, sieveCache, sieveCachePos, factor2.v[5]);
			if (mask2 & 0x08000000) _addToSieveCache(factorsTable, sieveCache, sieveCachePos, factor2.v[6]);
			if (mask2 & 0x80000000) _addToSieveCache(factorsTable, sieveCache, sieveCachePos, factor2.v[7]);
			nextIncr1.m256 = _mm256_and_si256(cmpres1.m256, p1.m256);
			nextIncr2.m256 = _mm256_and_si256(cmpres2.m256, p2.m256);
			factor1.m256 = _mm256_add_epi32(factor1.m256, nextIncr1.m256);
			factor2.m256 = _mm256_add_epi32(factor2.m256, nextIncr2.m256);
		}
		factor1.m256 = _mm256_sub_epi32(factor1.m256, offsetmax.m256);
		factor2.m256 = _mm256_sub_epi32(factor2.m256, offsetmax.m256);
		_mm256_store_si256(reinterpret_cast<__m256i*>(&factorsToEliminate[i*8 + 0]), factor1.m256);
		_mm256_store_si256(reinterpret_cast<__m256i*>(&factorsToEliminate[i*8 + 8]), factor2.m256);
	}
	_endSieveCache(factorsTable, sieveCache);
}
#endif
#endif

void Instance::_doSieveTask(Task task) {
	Sieve& sieve(_sieves[task.sieve.id]);
	std::unique_lock<std::mutex> presieveLock(sieve.presieveLock, std::defer_lock);
	const uint64_t workIndex(task.workIndex), sieveIteration(task.sieve.iteration), firstPrimeIndex(_primorialNumber);
	std::array<uint32_t, sieveCacheSize> sieveCache{0};
	uint64_t sieveCachePos(0);
	Task checkTask{Task::Type::Check, workIndex, {}};
	
	if (!_works[workIndex].current) // Abort Sieve Task if new block (but count as Task done)
		goto sieveEnd;
	
	memset(sieve.factorsTable, 0, sizeof(uint64_t)*_sieveWords);
	
	// Eliminate the p*i + fp factors (p < factorMax).
#ifdef __SSE2__
	if (_pattern.size() == 6)
		_processSieve6(sieve.factorsTable, sieve.factorsToEliminate, firstPrimeIndex, _primesIndexThreshold);
	else if (_pattern.size() == 7)
#ifdef __AVX2__
		_processSieve7_avx2(sieve.factorsTable, sieve.factorsToEliminate, firstPrimeIndex, _primesIndexThreshold);
#else
		_processSieve7(sieve.factorsTable, sieve.factorsToEliminate, firstPrimeIndex, _primesIndexThreshold);
#endif
	else if (_pattern.size() == 8)
#ifdef __AVX2__
		_processSieve8_avx2(sieve.factorsTable, sieve.factorsToEliminate, firstPrimeIndex, _primesIndexThreshold);
#else
		_processSieve8(sieve.factorsTable, sieve.factorsToEliminate, firstPrimeIndex, _primesIndexThreshold);
#endif
	else
		_processSieve(sieve.factorsTable, sieve.factorsToEliminate, firstPrimeIndex, _primesIndexThreshold);
#else
	_processSieve(sieve.factorsTable, sieve.factorsToEliminate, firstPrimeIndex, _primesIndexThreshold);
#endif
	
	if (!_works[workIndex].current)
		goto sieveEnd;
	
	// Wait for the presieve tasks that generate the additional factors to finish.
	if (sieveIteration == 0) presieveLock.lock();
	
	// Eliminate these factors.
	for (uint64_t i(0), count(sieve.additionalFactorsToEliminateCounts[sieveIteration]); i < count ; i++)
		_addToSieveCache(sieve.factorsTable, sieveCache, sieveCachePos, sieve.additionalFactorsToEliminate[sieveIteration][i]);
	_endSieveCache(sieve.factorsTable, sieveCache);
	
	if (!_works[workIndex].current)
		goto sieveEnd;
	
	checkTask.check.nCandidates = 0;
	checkTask.check.offsetId = sieve.id;
	checkTask.check.factorStart = sieveIteration*_sieveSize;
	// Extract candidates from the sieve and create verify tasks of up to maxCandidatesPerCheckTask candidates.
	for (uint32_t b(0) ; b < _sieveWords ; b++) {
		uint64_t sieveWord(~sieve.factorsTable[b]); // ~ is the Bitwise Not: ones then indicate the candidates and zeros the previously eliminated numbers.
		while (sieveWord != 0) {
			const uint32_t nEliminatedUntilNext(__builtin_ctzll(sieveWord)), candidateIndex((b*64) + nEliminatedUntilNext); // __builtin_ctzll returns the number of leading 0s.
			checkTask.check.factorOffsets[checkTask.check.nCandidates] = candidateIndex;
			checkTask.check.nCandidates++;
			if (checkTask.check.nCandidates == maxCandidatesPerCheckTask) {
				if (!_works[workIndex].current)
					goto sieveEnd;
				_tasks.push_back(checkTask);
				checkTask.check.nCandidates = 0;
				_works[workIndex].nRemainingCheckTasks++;
			}
			sieveWord &= sieveWord - 1; // Change the candidate's bit from 1 to 0.
		}
	}
	if (!_works[workIndex].current)
		goto sieveEnd;
	if (checkTask.check.nCandidates > 0) {
		_tasks.push_back(checkTask);
		_works[workIndex].nRemainingCheckTasks++;
	}
	if (sieveIteration + 1 < _sieveIterations) {
		if (_threads > 1)
			_tasks.push_front(Task::SieveTask(workIndex, sieve.id, sieveIteration + 1));
		else // Allow mining with 1 Thread without having to wait for all the blocks to be processed.
			_tasks.push_back(Task::SieveTask(workIndex, sieve.id, sieveIteration + 1));
		return; // Sieving still not finished, do not go to sieveEnd.
	}
sieveEnd:
	_tasksDoneInfos.push_back(TaskDoneInfo{Task::Type::Sieve, {}});
}

// Riecoin uses GMP's mpz_probab_prime_p for the PoW, but the Fermat Test is significantly faster and more suitable for the miner.
// n is probably prime if a^(n - 1) ≡ 1 (mod n) for one 0 < a < p or more.
static const mpz_class mpz2(2); // Here, we test with one a = 2.
bool isPrimeFermat(const mpz_class& n) {
	mpz_class r, nm1(n - 1);
	mpz_powm(r.get_mpz_t(), mpz2.get_mpz_t(), nm1.get_mpz_t(), n.get_mpz_t()); // r = 2^(n - 1) % n
	return r == 1;
}

// CPU Assembly optimized Fermat Test by Michael Bell
#if defined(__SSE2__) && defined(__AVX2__)
bool Instance::_testPrimesIspc(const std::array<uint32_t, maxCandidatesPerCheckTask> &factorOffsets, uint32_t primalityResults[maxCandidatesPerCheckTask], const mpz_class &candidateStart, mpz_class &candidate) {
	uint32_t M[maxCandidatesPerCheckTask*MAX_N_SIZE], bits(0), N_Size;
	uint32_t *mp(&M[0]);
	for (uint32_t i(0) ; i < maxCandidatesPerCheckTask ; i++) {
		candidate = candidateStart + _primorial*factorOffsets[i];
		if (bits == 0) {
			bits = mpz_sizeinbase(candidate.get_mpz_t(), 2);
			N_Size = (bits >> 5) + ((bits & 0x1f) > 0);
			if (N_Size > MAX_N_SIZE) return false;
		}
		else assert(bits == mpz_sizeinbase(candidate.get_mpz_t(), 2));
		memcpy(mp, candidate.get_mpz_t()->_mp_d, N_Size*4);
		mp += N_Size;
	}
	fermatTest(N_Size, maxCandidatesPerCheckTask, M, primalityResults, sysInfo.hasAVX512());
	return true;
}
#endif

// OpenCL Fermat Test, Todo...

// Cuda Fermat Test
#ifdef RIEMINERCUDA
bool Instance::_testPrimesCuda(const std::array<uint32_t, maxCandidatesPerCheckTask> &factorOffsets, std::vector<uint32_t> &primalityResults, const mpz_class &candidateStart, mpz_class &candidate, const uint16_t device) {
	const auto bits(mpz_sizeinbase(candidateStart.get_mpz_t(), 2));
	if (bits >= 2048)
		return false;
	std::vector<mpz_class> candidates;
	for (uint32_t i(0) ; i < primalityResults.size() ; i++) {
		candidate = candidateStart + _primorial*factorOffsets[i];
		candidates.push_back(candidate);
	}
	if (bits < 768)
		fermatCudaTest<768>(candidates, primalityResults.data(), device);
	else if (bits < 1024)
		fermatCudaTest<1024>(candidates, primalityResults.data(), device);
	else if (bits < 1280)
		fermatCudaTest<1280>(candidates, primalityResults.data(), device);
	else if (bits < 1536)
		fermatCudaTest<1536>(candidates, primalityResults.data(), device);
	else if (bits < 1792)
		fermatCudaTest<1792>(candidates, primalityResults.data(), device);
	else
		fermatCudaTest<2048>(candidates, primalityResults.data(), device);
	return true;
}
#endif

void Instance::_doCheckTask(Task task) {
	const uint16_t workIndex(task.workIndex);
	if (!_works[workIndex].current) return;
	std::vector<uint64_t> tupleCounts(_pattern.size() + 1, 0);
	mpz_class candidateStart, candidate;
	mpz_mul_ui(candidateStart.get_mpz_t(), _primorial.get_mpz_t(), task.check.factorStart);
	candidateStart += _works[workIndex].primorialMultipleStart;
	candidateStart += _primorialOffsets[task.check.offsetId];
	
	bool firstTestDone(false); // by GPU or Assembly Optimized Implementation
	const auto nCandidates(task.check.nCandidates);
#if defined(RIEMINERCUDA) || (defined(__SSE2__) && defined(__AVX2__))
	std::vector<uint32_t> primalityResults;
#endif
#if defined(RIEMINERCUDA)
	if (_gpuFermatTest) { // Test candidates + 0 primality with Nvidia GPU if available.
		primalityResults = std::vector<uint32_t>(task.check.nCandidates, 0);
		firstTestDone = _testPrimesCuda(task.check.factorOffsets, primalityResults, candidateStart, candidate, _gpuFermatTestId);
	}
#endif
#if defined(__SSE2__) && defined(__AVX2__)
	if (!firstTestDone && task.check.nCandidates == maxCandidatesPerCheckTask) { // Test candidates + 0 primality with assembly optimizations if possible.
		primalityResults = std::vector<uint32_t>(maxCandidatesPerCheckTask, 0);
		firstTestDone = _testPrimesIspc(task.check.factorOffsets, primalityResults.data(), candidateStart, candidate);
	}
#endif
	if (firstTestDone) {
		tupleCounts[0] += nCandidates;
		task.check.nCandidates = 0;
		for (uint32_t i(0) ; i < nCandidates ; i++) {
			if (primalityResults[i]) {
				task.check.factorOffsets[task.check.nCandidates++] = task.check.factorOffsets[i];
				tupleCounts[1]++;
			}
		}
	}
	
	for (uint32_t i(0) ; i < task.check.nCandidates ; i++) {
		if (!_works[workIndex].current) break;
		candidate = candidateStart + _primorial*task.check.factorOffsets[i];
		
		if (!firstTestDone) { // Test candidate + 0 primality without optimizations/GPU if not done before.
			tupleCounts[0]++;
			if (!isPrimeFermat(candidate)) continue;
			tupleCounts[1]++;
		}
		
		uint32_t primeCount(1), offsetSum(0);
		// Test primality of the other elements of the tuple if candidate + 0 is prime.
		for (std::vector<uint64_t>::size_type i(1) ; i < _pattern.size() ; i++) {
			offsetSum += _pattern[i];
			mpz_add_ui(candidate.get_mpz_t(), candidate.get_mpz_t(), _pattern[i]);
			if (isPrimeFermat(mpz_class(candidate))) {
				primeCount++;
				tupleCounts[primeCount]++;
			}
			else if (!_patternMin[i]) {
				int candidatesRemaining(_primeCountTarget - 1 - i);
				if ((primeCount + candidatesRemaining) < _primeCountMin) break; // No chance to met the requirement anymore.
			}
			else break;
		}
		// Submit if requirements satisfied (and Job still valid).
		if (_works[_currentWorkIndex].current && primeCount >= _primeCountMin) {
			std::string message;
			const mpz_class basePrime(candidate - offsetSum);
			_addResult({
				.jobId = _works[workIndex].job.id,
				.threadId = threadId,
				.result = basePrime,
				.primeCount = primeCount,
				.primorialNumber = static_cast<uint16_t>(_primorialNumber),
				.primorialFactor = task.check.factorStart + task.check.factorOffsets[i],
				.primorialOffset = _primorialOffsetsU64[task.check.offsetId]});
		}
	}
	std::lock_guard<std::mutex> lock(_countsLock);
	if (_tupleCounts.size() == tupleCounts.size()) // Do not update if Tuple Length changed meanwhile (in case of a Fork, or can be observed with the Test Server).
		std::transform(_tupleCounts.begin(), _tupleCounts.end(), tupleCounts.begin(), _tupleCounts.begin(), std::plus<uint64_t>());
}

void Instance::_doTasks(const uint16_t id) { // Worker Threads run here until the miner is stopped
	// Thread initialization.
	threadId = id;
	factorsCache = new uint64_t*[_sieveWorkers];
	factorsCacheCounts = new uint64_t*[_sieveWorkers];
	for (int i(0) ; i < _sieveWorkers ; i++) {
		factorsCache[i] = new uint64_t[factorsCacheSize];
		factorsCacheCounts[i] = new uint64_t[_sieveIterations];
		for (uint64_t j(0) ; j < _sieveIterations ; j++)
			factorsCacheCounts[i][j] = 0;
	}
	// Threads are fetching tasks from the queues. The first part of the constellation search is sieving to generate candidates, which is done by the Presieve and Sieve tasks.
	// Once the candidates were generated, they are tested whether they are indeed base primes of constellations using the Fermat Test.
	while (_running) {
		Task task;
		if (!_presieveTasks.try_pop_front(task)) // Presieve Tasks have priority
			task = _tasks.blocking_pop_front();
		
		const auto startTime(std::chrono::steady_clock::now());
		if (task.type == Task::Type::Presieve) {
			_doPresieveTask(task);
			_presieveTime += std::chrono::duration_cast<decltype(_presieveTime)>(std::chrono::steady_clock::now() - startTime);
			_tasksDoneInfos.push_back(TaskDoneInfo{Task::Type::Presieve, {task.presieve.start}});
		}
		if (task.type == Task::Type::Sieve) {
			_doSieveTask(task);
			_sieveTime += std::chrono::duration_cast<decltype(_sieveTime)>(std::chrono::steady_clock::now() - startTime);
			// The Sieve's Task Done Info is created in _doSieveTask
		}
		if (task.type == Task::Type::Check) {
			_doCheckTask(task);
			_verifyTime += std::chrono::duration_cast<decltype(_verifyTime)>(std::chrono::steady_clock::now() - startTime);
			_tasksDoneInfos.push_back(TaskDoneInfo{Task::Type::Check, {task.workIndex}});
		}
	}
	// Thread clean up.
	for (int i(0) ; i < _sieveWorkers ; i++) {
		delete[] factorsCacheCounts[i];
		delete[] factorsCache[i];
	}
	delete[] factorsCacheCounts;
	delete[] factorsCache;
}

void Instance::invalidateWork() {
	for (auto &work : _works)
		work.current = false;
}

void Instance::_manageTasks() {
	Stella::Job job;
	_currentWorkIndex = 0;
	while (_running) {
		// Pop next Job, wait if there is none.
		if (!_availableJobs.try_pop_front(job)) {
			std::this_thread::sleep_for(2ms);
			continue;
		}
		
		_works[_currentWorkIndex].job = job;
		if (job.clearPreviousJobs)
			invalidateWork();
		_works[_currentWorkIndex].current = true;
		_works[_currentWorkIndex].primorialMultipleStart = _works[_currentWorkIndex].job.target + _primorial - (_works[_currentWorkIndex].job.target % _primorial);
		
		// Reset Stats and Sieve State.
		_presieveTime = _presieveTime.zero();
		_sieveTime = _sieveTime.zero();
		_verifyTime = _verifyTime.zero();
		for (auto &sieve : _sieves) {
			for (uint64_t j(0) ; j < _sieveIterations ; j++)
				sieve.additionalFactorsToEliminateCounts[j] = 0;
		}
		// Create Presieve Tasks.
		uint64_t nPresieveTasks(_threads*8ULL);
		int32_t nRemainingNormalPresieveTasks(0), nRemainingAdditionalPresieveTasks(0);
		const uint32_t remainingTasks(_tasks.size());
		const uint64_t primesPerPresieveTask((_nPrimes - _primorialNumber)/nPresieveTasks + 1ULL);
		for (uint64_t start(_primorialNumber) ; start < _nPrimes ; start += primesPerPresieveTask) {
			const uint64_t end(std::min(_nPrimes, start + primesPerPresieveTask));
			_presieveTasks.push_back(Task::PresieveTask(_currentWorkIndex, start, end));
			_tasks.push_front(Task{Task::Type::Dummy, _currentWorkIndex, {}}); // Ensure a thread wakes up to grab the mod work.
			if (start < _primesIndexThreshold) nRemainingNormalPresieveTasks++;
			else nRemainingAdditionalPresieveTasks++;
		}
		
		// Sieve Tasks cannot be started until all Presieve Tasks are finished.
		while (nRemainingNormalPresieveTasks > 0) {
			const TaskDoneInfo taskDoneInfo(_tasksDoneInfos.blocking_pop_front());
			if (!_running) return; // Can happen if stopThreads is called while this Thread is stuck in this blocking_pop_front().
			if (taskDoneInfo.type == Task::Type::Presieve) {
				if (taskDoneInfo.firstPrimeIndex < _primesIndexThreshold) nRemainingNormalPresieveTasks--;
				else nRemainingAdditionalPresieveTasks--;
			}
			else if (taskDoneInfo.type == Task::Type::Check) _works[taskDoneInfo.workIndex].nRemainingCheckTasks--;
			else assert(false); // Unexpected Sieve Task done during Presieving. Should never happen but useful for debugging if trying to improve the Job Management.
		}
		assert(_works[_currentWorkIndex].nRemainingCheckTasks == 0);
		
		// Create Sieve Tasks
		for (std::vector<Sieve>::size_type i(0) ; i < _sieves.size() ; i++) {
			_sieves[i].presieveLock.lock();
			_tasks.push_front(Task::SieveTask(_currentWorkIndex, i, 0));
		}
		
		int nRemainingSieves(_sieveWorkers);
		while (nRemainingAdditionalPresieveTasks > 0) {
			const TaskDoneInfo taskDoneInfo(_tasksDoneInfos.blocking_pop_front());
			if (!_running) {
				for (auto &sieve : _sieves) sieve.presieveLock.unlock();
				return;
			}
			if (taskDoneInfo.type == Task::Type::Presieve) nRemainingAdditionalPresieveTasks--;
			else if (taskDoneInfo.type == Task::Type::Sieve) nRemainingSieves--;
			else _works[taskDoneInfo.workIndex].nRemainingCheckTasks--;
		}
		for (auto &sieve : _sieves) sieve.presieveLock.unlock();
		
		// Measure how low the number of pending Tasks have been and later adjust the Remaining Tasks Target in function of this.
		uint32_t nRemainingTasksMin(std::min(remainingTasks, _tasks.size()));
		while (nRemainingSieves > 0) {
			const TaskDoneInfo taskDoneInfo(_tasksDoneInfos.blocking_pop_front());
			if (!_running) return;
			if (taskDoneInfo.type == Task::Type::Sieve) nRemainingSieves--;
			else if (taskDoneInfo.type == Task::Type::Check) _works[taskDoneInfo.workIndex].nRemainingCheckTasks--;
			else assert(false); // Unexpected Presieve Task done during Sieving. Should never happen but useful for debugging if trying to improve the Job Management.
			nRemainingTasksMin = std::min(nRemainingTasksMin, _tasks.size());
		}
		
		// Adjust the Remaining Tasks Target.
		if (_works[_currentWorkIndex].current && !job.clearPreviousJobs) {
			// std::cout << "Min work outstanding during sieving: "s << nRemainingTasksMin << "\n"s;
			if (remainingTasks > _nRemainingCheckTasksTarget - _threads*2) {
				// If we are acheiving our work target, then adjust it towards the amount required to maintain a healthy minimum work queue length.
				if (nRemainingTasksMin == 0) // Need more, but don't know how much, try adding some.
					_nRemainingCheckTasksTarget += 4*_threads*_sieveWorkers;
				else { // Adjust towards target of min work = 4*threads.
					const uint32_t targetMaxWork((_nRemainingCheckTasksTarget - nRemainingTasksMin) + 8*_threads);
					_nRemainingCheckTasksTarget = (_nRemainingCheckTasksTarget + targetMaxWork)/2;
				}
			}
			else if (nRemainingTasksMin > 4u*_threads) { // Didn't make the target, but also didn't run out of work. Can still adjust target.
				const uint32_t targetMaxWork((remainingTasks - nRemainingTasksMin) + 10*_threads);
				_nRemainingCheckTasksTarget = (_nRemainingCheckTasksTarget + targetMaxWork)/2;
			}
			// Else not enough Check Tasks are produced and the CPU is possibly going to be underused. User can alleviate this by increasing the SieveWorkers, but no adjustment of the Target here would help. The Job management needs to be improved as a whole to fix the underlying issue for good.
			_nRemainingCheckTasksTarget = std::min(_nRemainingCheckTasksTarget, _tasksDoneInfos.size() - 9*_threads);
			// std::cout << "Work target before starting next block now: "s << _nRemainingCheckTasksTarget << "\n"s;
		}
		
		// Remove surplus Check Tasks in accordance with the Target.
		while (_works[_currentWorkIndex].nRemainingCheckTasks > _nRemainingCheckTasksTarget) {
			const TaskDoneInfo taskDoneInfo(_tasksDoneInfos.blocking_pop_front());
			if (!_running) return;
			if (taskDoneInfo.type == Task::Type::Check) _works[taskDoneInfo.workIndex].nRemainingCheckTasks--;
			else assert(false); // Only Check Tasks are expected here. Should never happen but useful for debugging if trying to improve the Job Management.
		}
		
		// Before starting next Job, ensure that any Check Tasks left of its work slot are finished.
		_currentWorkIndex = (_currentWorkIndex + 1) % nWorks;
		while (_works[_currentWorkIndex].nRemainingCheckTasks > 0) {
			const TaskDoneInfo taskDoneInfo(_tasksDoneInfos.blocking_pop_front());
			if (!_running) return;
			if (taskDoneInfo.type == Task::Type::Check) _works[taskDoneInfo.workIndex].nRemainingCheckTasks--;
			else assert(false); // Only Check Tasks are expected here. Should never happen but useful for debugging if trying to improve the Job Management.
		}
		// std::cout << "Job Timing: "s << _presieveTime.count() << "/"s << _sieveTime.count() << "/"s << _verifyTime.count() << ", tasks: "s << _works[0].nRemainingCheckTasks << ", "s << _works[1].nRemainingCheckTasks << "\n"s;
	}
}

bool Instance::hasAcceptedPatterns(const std::vector<std::vector<uint64_t>> &acceptedPatterns) const {
	for (const auto &acceptedPattern : acceptedPatterns) {
		bool compatible(true);
		for (uint16_t i(0) ; i < acceptedPattern.size() ; i++) {
			if (i >= _pattern.size() ? true : acceptedPattern[i] != _pattern[i]) {
				compatible = false;
				break;
			}
		}
		if (compatible)
			return true;
	}
	return false;
}

std::string doubleToString(const double d, const uint16_t precision) {
	std::ostringstream oss;
	if (precision == 0)
		oss << d;
	else
		oss << FIXED(precision) << d;
	return oss.str();
}

std::string formattedCounts(const std::vector<uint64_t> &counts, const uint64_t m) {
	std::ostringstream oss;
	oss << "(";
	for (uint64_t i(m) ; i < counts.size() ; i++) {
		oss << counts[i];
		if (i != counts.size() - 1) oss << " ";
	}
	oss << ")";
	return oss.str();
}

std::string formattedTime(const double &time) {
	std::ostringstream oss;
	const uint32_t timeInt(time*1000.);
	oss << "[" << timeInt/86400000 << ":" << leading0s(2) << (timeInt/3600000) % 24 << ":" << leading0s(2) << (timeInt/60000) % 60 << ":" << leading0s(2) << (timeInt/1000) % 60 << "." << (timeInt/100) % 10 << "]";
	return oss.str();
}
std::string formattedClockTimeNow() {
	const auto now(std::chrono::system_clock::now());
	const auto seconds(std::chrono::time_point_cast<std::chrono::seconds>(now));
	const auto milliseconds(std::chrono::duration_cast<std::chrono::milliseconds>(now - seconds));
	const std::time_t timeT(std::chrono::system_clock::to_time_t(now));
	const std::tm *timeTm(std::gmtime(&timeT));
	std::ostringstream oss;
	oss << "[" << std::put_time(timeTm, "%b %d %H:%M:%S") << "." << static_cast<uint32_t>(std::floor(milliseconds.count()))/100 << "]";
	return oss.str();
}
std::string formattedDuration(const double &duration) {
	std::ostringstream oss;
	if (duration < 0.001) oss << std::round(1000000.*duration) << " us";
	else if (duration < 1.) oss << std::round(1000.*duration) << " ms";
	else if (duration < 60.) oss << FIXED(2 + (duration < 10.)) << duration << " s";
	else if (duration < 3600.) oss << FIXED(2 + (duration/60. < 10.)) << duration/60. << " min";
	else if (duration < 86400.) oss << FIXED(2 + (duration/3600. < 10.)) << duration/3600. << " h";
	else if (duration < 31556952.) oss << FIXED(3) << duration/86400. << " d";
	else oss << FIXED(3) << duration/31556952. << " y";
	return oss.str();
}

}
