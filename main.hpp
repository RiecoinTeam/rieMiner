// (c) 2017-2021 Pttn and contributors (https://riecoin.dev/en/rieMiner)

#ifndef HEADER_main_hpp
#define HEADER_main_hpp

#include <algorithm>
#include <array>
#include <chrono>
#include <fstream>
#include <iomanip>
#include <mutex>
#include <string>
#include <thread>
#include <unistd.h>
#include <vector>
#include "tools.hpp"

using namespace std::string_literals;

#define versionString	"rieMiner 0.93alpha1"
#define primeTableFile	"PrimeTable64.bin"

extern int DEBUG;
extern std::string confPath;

#define DBG(x) if (DEBUG) {x;};
#define DBG_VERIFY(x) if (DEBUG > 1) { x; };
#define ERRORMSG(message) std::cerr << __func__ << ": " << message << " :| !" << std::endl

static const std::vector<std::pair<std::vector<uint64_t>, std::vector<uint64_t>>> defaultConstellationData = {
	// 1-tuples
	{{0}, {380284918609481ULL, 437163765888581ULL, 701889794782061ULL, 980125031081081ULL, 1277156391416021ULL, 1487854607298791ULL, 1833994713165731ULL, 2115067287743141ULL, 2325810733931801ULL, 3056805353932061ULL, 3252606350489381ULL, 3360877662097841ULL, 3501482688249431ULL, 3595802556731501ULL, 3843547642594391ULL, 5000014653723821ULL, 5861268883004651ULL, 7486645325734691ULL, 7933248530182091ULL, 8760935349271991ULL, 8816939536219931ULL, 8871465225933041ULL, 9354490866900911ULL, 11096062107205961ULL, 11096434547374061ULL, 13764730155211151ULL, 13884748604026031ULL, 17438667992681051ULL, 20362378935668501ULL, 20471700514990841ULL, 20475715985020181ULL, 20614750499829371ULL, 21465425387541251ULL, 21628360938574121ULL, 21817283854511261ULL, 22238558064758921ULL, 22318056296221571ULL, 22733842556089781ULL, 22849881428489231ULL, 23382987892499351ULL, 23417442472403711ULL, 25964083184094941ULL, 26515897161980111ULL, 29512383574028471ULL, 30074756036270831ULL, 30310618347929651ULL, 30402250951007051ULL, 30413977411117031ULL, 33502273017038711ULL, 33508988966488151ULL, 33976718302847051ULL, 34783522781262371ULL, 37564605737538611ULL, 37606024583356961ULL, 39138758504100371ULL, 40205947750578341ULL, 40257009922154141ULL, 40392614725338761ULL, 40504121267225501ULL, 41099072498143391ULL, 41289201480321911ULL, 41543933848913381ULL, 42218492028808211ULL, 43938526447515431ULL}}, // OEIS A213645 (12-tuples)
	// 2-tuples
	{{0, 2}, {380284918609481ULL, 437163765888581ULL, 701889794782061ULL, 980125031081081ULL, 1277156391416021ULL, 1487854607298791ULL, 1833994713165731ULL, 2115067287743141ULL, 2325810733931801ULL, 3056805353932061ULL, 3252606350489381ULL, 3360877662097841ULL, 3501482688249431ULL, 3595802556731501ULL, 3843547642594391ULL, 5000014653723821ULL, 5861268883004651ULL, 7486645325734691ULL, 7933248530182091ULL, 8760935349271991ULL, 8816939536219931ULL, 8871465225933041ULL, 9354490866900911ULL, 11096062107205961ULL, 11096434547374061ULL, 13764730155211151ULL, 13884748604026031ULL, 17438667992681051ULL, 20362378935668501ULL, 20471700514990841ULL, 20475715985020181ULL, 20614750499829371ULL, 21465425387541251ULL, 21628360938574121ULL, 21817283854511261ULL, 22238558064758921ULL, 22318056296221571ULL, 22733842556089781ULL, 22849881428489231ULL, 23382987892499351ULL, 23417442472403711ULL, 25964083184094941ULL, 26515897161980111ULL, 29512383574028471ULL, 30074756036270831ULL, 30310618347929651ULL, 30402250951007051ULL, 30413977411117031ULL, 33502273017038711ULL, 33508988966488151ULL, 33976718302847051ULL, 34783522781262371ULL, 37564605737538611ULL, 37606024583356961ULL, 39138758504100371ULL, 40205947750578341ULL, 40257009922154141ULL, 40392614725338761ULL, 40504121267225501ULL, 41099072498143391ULL, 41289201480321911ULL, 41543933848913381ULL, 42218492028808211ULL, 43938526447515431ULL}}, // OEIS A213645 (12-tuples)
	// 3-tuples
	{{0, 2, 4}, {380284918609481ULL, 437163765888581ULL, 701889794782061ULL, 980125031081081ULL, 1277156391416021ULL, 1487854607298791ULL, 1833994713165731ULL, 2115067287743141ULL, 2325810733931801ULL, 3056805353932061ULL, 3252606350489381ULL, 3360877662097841ULL, 3501482688249431ULL, 3595802556731501ULL, 3843547642594391ULL, 5000014653723821ULL, 5861268883004651ULL, 7486645325734691ULL, 7933248530182091ULL, 8760935349271991ULL, 8816939536219931ULL, 8871465225933041ULL, 9354490866900911ULL, 11096062107205961ULL, 11096434547374061ULL, 13764730155211151ULL, 13884748604026031ULL, 17438667992681051ULL, 20362378935668501ULL, 20471700514990841ULL, 20475715985020181ULL, 20614750499829371ULL, 21465425387541251ULL, 21628360938574121ULL, 21817283854511261ULL, 22238558064758921ULL, 22318056296221571ULL, 22733842556089781ULL, 22849881428489231ULL, 23382987892499351ULL, 23417442472403711ULL, 25964083184094941ULL, 26515897161980111ULL, 29512383574028471ULL, 30074756036270831ULL, 30310618347929651ULL, 30402250951007051ULL, 30413977411117031ULL, 33502273017038711ULL, 33508988966488151ULL, 33976718302847051ULL, 34783522781262371ULL, 37564605737538611ULL, 37606024583356961ULL, 39138758504100371ULL, 40205947750578341ULL, 40257009922154141ULL, 40392614725338761ULL, 40504121267225501ULL, 41099072498143391ULL, 41289201480321911ULL, 41543933848913381ULL, 42218492028808211ULL, 43938526447515431ULL}}, // OEIS A213645 (12-tuples)
	{{0, 4, 2}, {1418575498573ULL, 27899359258003ULL, 34460918582323ULL, 76075560855373ULL, 186460616596333ULL, 218021188549243ULL, 234280497145543ULL, 282854319391723ULL, 345120905374093ULL, 346117552180633ULL, 604439135284063ULL, 727417501795063ULL, 1041814617748753ULL, 1090754719898923ULL, 1539765965257753ULL, 3152045700948223ULL, 3323127757029313ULL, 3449427485143873ULL, 4422879865247923ULL, 4525595253334003ULL, 4730773080017833ULL, 5462875671033013ULL, 6147764065076713ULL, 6205707895751443ULL, 6308411019731053ULL, 7582919852522863ULL, 7791180222409663ULL, 9162887985581563ULL, 9305359177794913ULL, 10096106139749863ULL, 10349085616714693ULL, 10744789916260633ULL, 10932016019429353ULL, 11140102475962693ULL, 12448240792011103ULL, 14727257011031413ULL, 16892267700442213ULL, 17963729763800053ULL, 18908121647739403ULL, 19028992697498863ULL, 19756696515786463ULL, 20252223877980943ULL, 20429666791949263ULL, 21680774776901473ULL, 21682173462980263ULL, 23076668788453513ULL, 24036602580170413ULL, 24101684579532793ULL, 25053289894907353ULL, 25309078073142943ULL, 25662701041982083ULL, 25777719656829373ULL, 26056424604564433ULL, 26315911419972253ULL, 26866456999592443ULL, 26887571851660753ULL, 27303559129791793ULL, 27839080743588193ULL, 28595465291933773ULL, 29137316070747733ULL, 30824439453812083ULL, 31395828815154883ULL, 31979851757518513ULL, 32897714831936803ULL}}, // OEIS A213601 (12-tuples) + 6
	// 4-tuples
	{{0, 2, 4, 2}, {380284918609481ULL, 437163765888581ULL, 701889794782061ULL, 980125031081081ULL, 1277156391416021ULL, 1487854607298791ULL, 1833994713165731ULL, 2115067287743141ULL, 2325810733931801ULL, 3056805353932061ULL, 3252606350489381ULL, 3360877662097841ULL, 3501482688249431ULL, 3595802556731501ULL, 3843547642594391ULL, 5000014653723821ULL, 5861268883004651ULL, 7486645325734691ULL, 7933248530182091ULL, 8760935349271991ULL, 8816939536219931ULL, 8871465225933041ULL, 9354490866900911ULL, 11096062107205961ULL, 11096434547374061ULL, 13764730155211151ULL, 13884748604026031ULL, 17438667992681051ULL, 20362378935668501ULL, 20471700514990841ULL, 20475715985020181ULL, 20614750499829371ULL, 21465425387541251ULL, 21628360938574121ULL, 21817283854511261ULL, 22238558064758921ULL, 22318056296221571ULL, 22733842556089781ULL, 22849881428489231ULL, 23382987892499351ULL, 23417442472403711ULL, 25964083184094941ULL, 26515897161980111ULL, 29512383574028471ULL, 30074756036270831ULL, 30310618347929651ULL, 30402250951007051ULL, 30413977411117031ULL, 33502273017038711ULL, 33508988966488151ULL, 33976718302847051ULL, 34783522781262371ULL, 37564605737538611ULL, 37606024583356961ULL, 39138758504100371ULL, 40205947750578341ULL, 40257009922154141ULL, 40392614725338761ULL, 40504121267225501ULL, 41099072498143391ULL, 41289201480321911ULL, 41543933848913381ULL, 42218492028808211ULL, 43938526447515431ULL}}, // OEIS A213645
	// 5-tuples
	{{0, 2, 4, 2, 4}, {380284918609481ULL, 437163765888581ULL, 701889794782061ULL, 980125031081081ULL, 1277156391416021ULL, 1487854607298791ULL, 1833994713165731ULL, 2115067287743141ULL, 2325810733931801ULL, 3056805353932061ULL, 3252606350489381ULL, 3360877662097841ULL, 3501482688249431ULL, 3595802556731501ULL, 3843547642594391ULL, 5000014653723821ULL, 5861268883004651ULL, 7486645325734691ULL, 7933248530182091ULL, 8760935349271991ULL, 8816939536219931ULL, 8871465225933041ULL, 9354490866900911ULL, 11096062107205961ULL, 11096434547374061ULL, 13764730155211151ULL, 13884748604026031ULL, 17438667992681051ULL, 20362378935668501ULL, 20471700514990841ULL, 20475715985020181ULL, 20614750499829371ULL, 21465425387541251ULL, 21628360938574121ULL, 21817283854511261ULL, 22238558064758921ULL, 22318056296221571ULL, 22733842556089781ULL, 22849881428489231ULL, 23382987892499351ULL, 23417442472403711ULL, 25964083184094941ULL, 26515897161980111ULL, 29512383574028471ULL, 30074756036270831ULL, 30310618347929651ULL, 30402250951007051ULL, 30413977411117031ULL, 33502273017038711ULL, 33508988966488151ULL, 33976718302847051ULL, 34783522781262371ULL, 37564605737538611ULL, 37606024583356961ULL, 39138758504100371ULL, 40205947750578341ULL, 40257009922154141ULL, 40392614725338761ULL, 40504121267225501ULL, 41099072498143391ULL, 41289201480321911ULL, 41543933848913381ULL, 42218492028808211ULL, 43938526447515431ULL}}, // OEIS A213645 (12-tuples)
	{{0, 4, 2, 4, 2}, {1418575498597ULL, 27899359258027ULL, 34460918582347ULL, 76075560855397ULL, 186460616596357ULL, 218021188549267ULL, 234280497145567ULL, 282854319391747ULL, 345120905374117ULL, 346117552180657ULL, 604439135284087ULL, 727417501795087ULL, 1041814617748777ULL, 1090754719898947ULL, 1539765965257777ULL, 3152045700948247ULL, 3323127757029337ULL, 3449427485143897ULL, 4422879865247947ULL, 4525595253334027ULL, 4730773080017857ULL, 5462875671033037ULL, 6147764065076737ULL, 6205707895751467ULL, 6308411019731077ULL, 7582919852522887ULL, 7791180222409687ULL, 9162887985581587ULL, 9305359177794937ULL, 10096106139749887ULL, 10349085616714717ULL, 10744789916260657ULL, 10932016019429377ULL, 11140102475962717ULL, 12448240792011127ULL, 14727257011031437ULL, 16892267700442237ULL, 17963729763800077ULL, 18908121647739427ULL, 19028992697498887ULL, 19756696515786487ULL, 20252223877980967ULL, 20429666791949287ULL, 21680774776901497ULL, 21682173462980287ULL, 23076668788453537ULL, 24036602580170437ULL, 24101684579532817ULL, 25053289894907377ULL, 25309078073142967ULL, 25662701041982107ULL, 25777719656829397ULL, 26056424604564457ULL, 26315911419972277ULL, 26866456999592467ULL, 26887571851660777ULL, 27303559129791817ULL, 27839080743588217ULL, 28595465291933797ULL, 29137316070747757ULL, 30824439453812107ULL, 31395828815154907ULL, 31979851757518537ULL, 32897714831936827ULL}}, // OEIS A213601 (12-tuples) + 30
	// 6-tuples
	{{0, 4, 2, 4, 2, 4}, {1091257ULL, 1615837ULL, 1954357ULL, 2822707ULL, 2839927ULL, 3243337ULL, 3400207ULL, 6005887ULL, 6503587ULL, 7187767ULL, 7641367ULL, 8061997ULL, 8741137ULL, 10526557ULL, 11086837ULL, 11664547ULL, 14520547ULL, 14812867ULL, 14834707ULL, 14856757ULL, 16025827ULL, 16094707ULL, 18916477ULL, 19197247ULL, 19634047ULL, 19800367ULL, 20112217ULL, 20247037ULL, 21321187ULL, 21850177ULL, 22587277ULL, 24786397ULL, 25009417ULL, 25524127ULL, 27305557ULL, 29153557ULL, 31563937ULL, 31875577ULL, 32836747ULL, 33575947ULL, 36319177ULL, 36985297ULL, 37055647ULL, 40660717ULL, 41214067ULL, 41763427ULL, 41927437ULL, 44842867ULL, 45974557ULL, 47204737ULL, 48660247ULL, 49157737ULL, 50685907ULL, 50943787ULL, 51255637ULL, 53204857ULL, 53266387ULL, 55057897ULL, 56431927ULL, 57812467ULL, 59877397ULL, 61052347ULL, 62757967ULL, 63655717ULL}}, // OEIS A022013
	// 7-tuples
	{{0, 2, 4, 2, 4, 6, 2}, {114023297140211ULL, 114189340938131ULL, 114355384736051ULL, 114521428533971ULL, 145799289681161ULL, 145844141558231ULL, 145888993435301ULL, 145933845312371ULL, 220452326319761ULL, 220469307413891ULL, 220486288508021ULL, 220503269602151ULL, 235290683530361ULL, 235367427477641ULL, 235444171424921ULL, 235520915372201ULL, 294911989246991ULL, 295129811972111ULL, 295347634697231ULL, 295565457422351ULL, 345279548696891ULL, 345390797864921ULL, 345502047032951ULL, 345613296200981ULL, 353247262660751ULL, 353395658218061ULL, 353544053775371ULL, 353692449332681ULL, 393391977659951ULL, 393554263173341ULL, 393716548686731ULL, 393878834200121ULL, 437171043550511ULL, 437359375794611ULL, 437547708038711ULL, 437736040282811ULL, 547374026909441ULL, 547566452933231ULL, 547758878957021ULL, 547951304980811ULL, 624475220164331ULL, 624728375797061ULL, 624981531429791ULL, 625234687062521ULL, 727343907387941ULL, 727564067888501ULL, 727784228389061ULL, 728004388889621ULL, 770217042177041ULL, 770410293094901ULL, 770603544012761ULL, 770796794930621ULL, 942173182708601ULL, 942393875821241ULL, 942614568933881ULL, 942835262046521ULL, 987033700014821ULL, 987192352739051ULL, 987351005463281ULL, 987509658187511ULL, 992287041033131ULL, 992305282156031ULL, 992323523278931ULL, 992341764401831ULL, 1196266900300931ULL, 1196331655791731ULL, 1196396411282531ULL, 1196461166773331ULL, 1363806213708551ULL, 1363923696714191ULL, 1364041179719831ULL, 1364158662725471ULL, 1504145477032811ULL, 1504390303845041ULL, 1504635130657271ULL, 1504879957469501ULL, 1506606424567241ULL, 1506633383579321ULL, 1506660342591401ULL, 1506687301603481ULL, 1547277513006761ULL, 1547538621604511ULL, 1547799730202261ULL, 1548060838800011ULL, 1551543396451661ULL, 1551708809259221ULL, 1551874222066781ULL, 1552039634874341ULL, 1554535163864141ULL, 1554630942026621ULL, 1554726720189101ULL, 1554822498351581ULL}}, // 10-tuples in groups of 4, valid up to primorial max 1400, difficulty around 2190.
	{{0, 2, 6, 4, 2, 4, 2}, {226193845148639ULL, 226374233346629ULL, 226554621544619ULL, 226735009742609ULL, 301570107719129ULL, 301713410008259ULL, 301856712297389ULL, 302000014586519ULL, 443216221842839ULL, 443355092154419ULL, 443493962465999ULL, 443632832777579ULL, 594353915119409ULL, 594554524107989ULL, 594755133096569ULL, 594955742085149ULL, 602484187049189ULL, 602527797215789ULL, 602571407382389ULL, 602615017548989ULL, 620850037212269ULL, 621267059407859ULL, 621684081603449ULL, 622101103799039ULL, 631774346860829ULL, 632202123099719ULL, 632629899338609ULL, 633057675577499ULL, 638409170551289ULL, 638417212765469ULL, 638425254979649ULL, 638433297193829ULL, 652691407502999ULL, 652768901389469ULL, 652846395275939ULL, 652923889162409ULL, 715627356082349ULL, 715866860827829ULL, 716106365573309ULL, 716345870318789ULL, 751355932060829ULL, 751594040050949ULL, 751832148041069ULL, 752070256031189ULL, 847107141137429ULL, 847125608236079ULL, 847144075334729ULL, 847162542433379ULL, 850067287283309ULL, 850306665932819ULL, 850546044582329ULL, 850785423231839ULL, 963140918896139ULL, 963391264321469ULL, 963641609746799ULL, 963891955172129ULL, 1322395947238349ULL, 1322624640823469ULL, 1322853334408589ULL, 1323082027993709ULL, 1426366146255299ULL, 1426606782441089ULL, 1426847418626879ULL, 1427088054812669ULL, 1450867151696099ULL, 1450935595771799ULL, 1451004039847499ULL, 1451072483923199ULL}}, // 10-tuples + 12 in groups of 4, valid up to primorial max 1400, difficulty around 2190.
	// 8-tuples
	{{0, 2, 4, 2, 4, 6, 2, 6}, {114023297140211ULL, 114189340938131ULL, 114355384736051ULL, 114521428533971ULL, 145799289681161ULL, 145844141558231ULL, 145888993435301ULL, 145933845312371ULL, 220452326319761ULL, 220469307413891ULL, 220486288508021ULL, 220503269602151ULL, 235290683530361ULL, 235367427477641ULL, 235444171424921ULL, 235520915372201ULL, 294911989246991ULL, 295129811972111ULL, 295347634697231ULL, 295565457422351ULL, 345279548696891ULL, 345390797864921ULL, 345502047032951ULL, 345613296200981ULL, 353247262660751ULL, 353395658218061ULL, 353544053775371ULL, 353692449332681ULL, 393391977659951ULL, 393554263173341ULL, 393716548686731ULL, 393878834200121ULL, 437171043550511ULL, 437359375794611ULL, 437547708038711ULL, 437736040282811ULL, 547374026909441ULL, 547566452933231ULL, 547758878957021ULL, 547951304980811ULL, 624475220164331ULL, 624728375797061ULL, 624981531429791ULL, 625234687062521ULL, 727343907387941ULL, 727564067888501ULL, 727784228389061ULL, 728004388889621ULL, 770217042177041ULL, 770410293094901ULL, 770603544012761ULL, 770796794930621ULL, 942173182708601ULL, 942393875821241ULL, 942614568933881ULL, 942835262046521ULL, 987033700014821ULL, 987192352739051ULL, 987351005463281ULL, 987509658187511ULL, 992287041033131ULL, 992305282156031ULL, 992323523278931ULL, 992341764401831ULL, 1196266900300931ULL, 1196331655791731ULL, 1196396411282531ULL, 1196461166773331ULL, 1363806213708551ULL, 1363923696714191ULL, 1364041179719831ULL, 1364158662725471ULL, 1504145477032811ULL, 1504390303845041ULL, 1504635130657271ULL, 1504879957469501ULL, 1506606424567241ULL, 1506633383579321ULL, 1506660342591401ULL, 1506687301603481ULL, 1547277513006761ULL, 1547538621604511ULL, 1547799730202261ULL, 1548060838800011ULL, 1551543396451661ULL, 1551708809259221ULL, 1551874222066781ULL, 1552039634874341ULL, 1554535163864141ULL, 1554630942026621ULL, 1554726720189101ULL, 1554822498351581ULL}}, // 10-tuples in groups of 4, valid up to primorial max 1400, difficulty around 2190.
	{{0, 2, 4, 6, 2, 6, 4, 2}, {1418575498577ULL, 27899359258007ULL, 34460918582327ULL, 76075560855377ULL, 186460616596337ULL, 218021188549247ULL, 234280497145547ULL, 282854319391727ULL, 345120905374097ULL, 346117552180637ULL, 604439135284067ULL, 727417501795067ULL, 1041814617748757ULL, 1090754719898927ULL, 1539765965257757ULL, 3152045700948227ULL, 3323127757029317ULL, 3449427485143877ULL, 4422879865247927ULL, 4525595253334007ULL, 4730773080017837ULL, 5462875671033017ULL, 6147764065076717ULL, 6205707895751447ULL, 6308411019731057ULL, 7582919852522867ULL, 7791180222409667ULL, 9162887985581567ULL, 9305359177794917ULL, 10096106139749867ULL, 10349085616714697ULL, 10744789916260637ULL, 10932016019429357ULL, 11140102475962697ULL, 12448240792011107ULL, 14727257011031417ULL, 16892267700442217ULL, 17963729763800057ULL, 18908121647739407ULL, 19028992697498867ULL, 19756696515786467ULL, 20252223877980947ULL, 20429666791949267ULL, 21680774776901477ULL, 21682173462980267ULL, 23076668788453517ULL, 24036602580170417ULL, 24101684579532797ULL, 25053289894907357ULL, 25309078073142947ULL, 25662701041982087ULL, 25777719656829377ULL, 26056424604564437ULL, 26315911419972257ULL, 26866456999592447ULL, 26887571851660757ULL, 27303559129791797ULL, 27839080743588197ULL, 28595465291933777ULL, 29137316070747737ULL, 30824439453812087ULL, 31395828815154887ULL, 31979851757518517ULL, 32897714831936807ULL}}, // OEIS A213601 (12-tuples) + 10
	{{0, 6, 2, 6, 4, 2, 4, 2}, {226193845148633ULL, 226374233346623ULL, 226554621544613ULL, 226735009742603ULL, 301570107719123ULL, 301713410008253ULL, 301856712297383ULL, 302000014586513ULL, 443216221842833ULL, 443355092154413ULL, 443493962465993ULL, 443632832777573ULL, 594353915119403ULL, 594554524107983ULL, 594755133096563ULL, 594955742085143ULL, 602484187049183ULL, 602527797215783ULL, 602571407382383ULL, 602615017548983ULL, 620850037212263ULL, 621267059407853ULL, 621684081603443ULL, 622101103799033ULL, 631774346860823ULL, 632202123099713ULL, 632629899338603ULL, 633057675577493ULL, 638409170551283ULL, 638417212765463ULL, 638425254979643ULL, 638433297193823ULL, 652691407502993ULL, 652768901389463ULL, 652846395275933ULL, 652923889162403ULL, 715627356082343ULL, 715866860827823ULL, 716106365573303ULL, 716345870318783ULL, 751355932060823ULL, 751594040050943ULL, 751832148041063ULL, 752070256031183ULL, 847107141137423ULL, 847125608236073ULL, 847144075334723ULL, 847162542433373ULL, 850067287283303ULL, 850306665932813ULL, 850546044582323ULL, 850785423231833ULL, 963140918896133ULL, 963391264321463ULL, 963641609746793ULL, 963891955172123ULL, 1322395947238343ULL, 1322624640823463ULL, 1322853334408583ULL, 1323082027993703ULL, 1426366146255293ULL, 1426606782441083ULL, 1426847418626873ULL, 1427088054812663ULL, 1450867151696093ULL, 1450935595771793ULL, 1451004039847493ULL, 1451072483923193ULL}}, // 10-tuples + 6 in groups of 4, valid up to primorial max 1400, difficulty around 2190.
	// 9-tuples
	{{0, 2, 4, 2, 4, 6, 2, 6, 4}, {114023297140211ULL, 114189340938131ULL, 114355384736051ULL, 114521428533971ULL, 145799289681161ULL, 145844141558231ULL, 145888993435301ULL, 145933845312371ULL, 220452326319761ULL, 220469307413891ULL, 220486288508021ULL, 220503269602151ULL, 235290683530361ULL, 235367427477641ULL, 235444171424921ULL, 235520915372201ULL, 294911989246991ULL, 295129811972111ULL, 295347634697231ULL, 295565457422351ULL, 345279548696891ULL, 345390797864921ULL, 345502047032951ULL, 345613296200981ULL, 353247262660751ULL, 353395658218061ULL, 353544053775371ULL, 353692449332681ULL, 393391977659951ULL, 393554263173341ULL, 393716548686731ULL, 393878834200121ULL, 437171043550511ULL, 437359375794611ULL, 437547708038711ULL, 437736040282811ULL, 547374026909441ULL, 547566452933231ULL, 547758878957021ULL, 547951304980811ULL, 624475220164331ULL, 624728375797061ULL, 624981531429791ULL, 625234687062521ULL, 727343907387941ULL, 727564067888501ULL, 727784228389061ULL, 728004388889621ULL, 770217042177041ULL, 770410293094901ULL, 770603544012761ULL, 770796794930621ULL, 942173182708601ULL, 942393875821241ULL, 942614568933881ULL, 942835262046521ULL, 987033700014821ULL, 987192352739051ULL, 987351005463281ULL, 987509658187511ULL, 992287041033131ULL, 992305282156031ULL, 992323523278931ULL, 992341764401831ULL, 1196266900300931ULL, 1196331655791731ULL, 1196396411282531ULL, 1196461166773331ULL, 1363806213708551ULL, 1363923696714191ULL, 1364041179719831ULL, 1364158662725471ULL, 1504145477032811ULL, 1504390303845041ULL, 1504635130657271ULL, 1504879957469501ULL, 1506606424567241ULL, 1506633383579321ULL, 1506660342591401ULL, 1506687301603481ULL, 1547277513006761ULL, 1547538621604511ULL, 1547799730202261ULL, 1548060838800011ULL, 1551543396451661ULL, 1551708809259221ULL, 1551874222066781ULL, 1552039634874341ULL, 1554535163864141ULL, 1554630942026621ULL, 1554726720189101ULL, 1554822498351581ULL}},  // 10-tuples in groups of 4, valid up to primorial max 1400, difficulty around 2190.
	{{0, 2, 4, 6, 2, 6, 4, 2, 4}, {1418575498577ULL, 27899359258007ULL, 34460918582327ULL, 76075560855377ULL, 186460616596337ULL, 218021188549247ULL, 234280497145547ULL, 282854319391727ULL, 345120905374097ULL, 346117552180637ULL, 604439135284067ULL, 727417501795067ULL, 1041814617748757ULL, 1090754719898927ULL, 1539765965257757ULL, 3152045700948227ULL, 3323127757029317ULL, 3449427485143877ULL, 4422879865247927ULL, 4525595253334007ULL, 4730773080017837ULL, 5462875671033017ULL, 6147764065076717ULL, 6205707895751447ULL, 6308411019731057ULL, 7582919852522867ULL, 7791180222409667ULL, 9162887985581567ULL, 9305359177794917ULL, 10096106139749867ULL, 10349085616714697ULL, 10744789916260637ULL, 10932016019429357ULL, 11140102475962697ULL, 12448240792011107ULL, 14727257011031417ULL, 16892267700442217ULL, 17963729763800057ULL, 18908121647739407ULL, 19028992697498867ULL, 19756696515786467ULL, 20252223877980947ULL, 20429666791949267ULL, 21680774776901477ULL, 21682173462980267ULL, 23076668788453517ULL, 24036602580170417ULL, 24101684579532797ULL, 25053289894907357ULL, 25309078073142947ULL, 25662701041982087ULL, 25777719656829377ULL, 26056424604564437ULL, 26315911419972257ULL, 26866456999592447ULL, 26887571851660757ULL, 27303559129791797ULL, 27839080743588197ULL, 28595465291933777ULL, 29137316070747737ULL, 30824439453812087ULL, 31395828815154887ULL, 31979851757518517ULL, 32897714831936807ULL}}, // OEIS A213601 (12-tuples) + 10
	{{0, 4, 2, 4, 6, 2, 6, 4, 2}, {1418575498573ULL, 27899359258003ULL, 34460918582323ULL, 76075560855373ULL, 186460616596333ULL, 218021188549243ULL, 234280497145543ULL, 282854319391723ULL, 345120905374093ULL, 346117552180633ULL, 604439135284063ULL, 727417501795063ULL, 1041814617748753ULL, 1090754719898923ULL, 1539765965257753ULL, 3152045700948223ULL, 3323127757029313ULL, 3449427485143873ULL, 4422879865247923ULL, 4525595253334003ULL, 4730773080017833ULL, 5462875671033013ULL, 6147764065076713ULL, 6205707895751443ULL, 6308411019731053ULL, 7582919852522863ULL, 7791180222409663ULL, 9162887985581563ULL, 9305359177794913ULL, 10096106139749863ULL, 10349085616714693ULL, 10744789916260633ULL, 10932016019429353ULL, 11140102475962693ULL, 12448240792011103ULL, 14727257011031413ULL, 16892267700442213ULL, 17963729763800053ULL, 18908121647739403ULL, 19028992697498863ULL, 19756696515786463ULL, 20252223877980943ULL, 20429666791949263ULL, 21680774776901473ULL, 21682173462980263ULL, 23076668788453513ULL, 24036602580170413ULL, 24101684579532793ULL, 25053289894907353ULL, 25309078073142943ULL, 25662701041982083ULL, 25777719656829373ULL, 26056424604564433ULL, 26315911419972253ULL, 26866456999592443ULL, 26887571851660753ULL, 27303559129791793ULL, 27839080743588193ULL, 28595465291933773ULL, 29137316070747733ULL, 30824439453812083ULL, 31395828815154883ULL, 31979851757518513ULL, 32897714831936803ULL}}, // OEIS A213601 (12-tuples) + 6
	{{0, 4, 6, 2, 6, 4, 2, 4, 2}, {226193845148629ULL, 226374233346619ULL, 226554621544609ULL, 226735009742599ULL, 301570107719119ULL, 301713410008249ULL, 301856712297379ULL, 302000014586509ULL, 443216221842829ULL, 443355092154409ULL, 443493962465989ULL, 443632832777569ULL, 594353915119399ULL, 594554524107979ULL, 594755133096559ULL, 594955742085139ULL, 602484187049179ULL, 602527797215779ULL, 602571407382379ULL, 602615017548979ULL, 620850037212259ULL, 621267059407849ULL, 621684081603439ULL, 622101103799029ULL, 631774346860819ULL, 632202123099709ULL, 632629899338599ULL, 633057675577489ULL, 638409170551279ULL, 638417212765459ULL, 638425254979639ULL, 638433297193819ULL, 652691407502989ULL, 652768901389459ULL, 652846395275929ULL, 652923889162399ULL, 715627356082339ULL, 715866860827819ULL, 716106365573299ULL, 716345870318779ULL, 751355932060819ULL, 751594040050939ULL, 751832148041059ULL, 752070256031179ULL, 847107141137419ULL, 847125608236069ULL, 847144075334719ULL, 847162542433369ULL, 850067287283299ULL, 850306665932809ULL, 850546044582319ULL, 850785423231829ULL, 963140918896129ULL, 963391264321459ULL, 963641609746789ULL, 963891955172119ULL, 1322395947238339ULL, 1322624640823459ULL, 1322853334408579ULL, 1323082027993699ULL, 1426366146255289ULL, 1426606782441079ULL, 1426847418626869ULL, 1427088054812659ULL, 1450867151696089ULL, 1450935595771789ULL, 1451004039847489ULL, 1451072483923189ULL}}, // 10-tuples + 2 in groups of 4, valid up to primorial max 1400, difficulty around 2190.
	// 10-tuples
	{{0, 2, 4, 2, 4, 6, 2, 6, 4, 2}, {114023297140211ULL, 114189340938131ULL, 114355384736051ULL, 114521428533971ULL, 145799289681161ULL, 145844141558231ULL, 145888993435301ULL, 145933845312371ULL, 220452326319761ULL, 220469307413891ULL, 220486288508021ULL, 220503269602151ULL, 235290683530361ULL, 235367427477641ULL, 235444171424921ULL, 235520915372201ULL, 294911989246991ULL, 295129811972111ULL, 295347634697231ULL, 295565457422351ULL, 345279548696891ULL, 345390797864921ULL, 345502047032951ULL, 345613296200981ULL, 353247262660751ULL, 353395658218061ULL, 353544053775371ULL, 353692449332681ULL, 393391977659951ULL, 393554263173341ULL, 393716548686731ULL, 393878834200121ULL, 437171043550511ULL, 437359375794611ULL, 437547708038711ULL, 437736040282811ULL, 547374026909441ULL, 547566452933231ULL, 547758878957021ULL, 547951304980811ULL, 624475220164331ULL, 624728375797061ULL, 624981531429791ULL, 625234687062521ULL, 727343907387941ULL, 727564067888501ULL, 727784228389061ULL, 728004388889621ULL, 770217042177041ULL, 770410293094901ULL, 770603544012761ULL, 770796794930621ULL, 942173182708601ULL, 942393875821241ULL, 942614568933881ULL, 942835262046521ULL, 987033700014821ULL, 987192352739051ULL, 987351005463281ULL, 987509658187511ULL, 992287041033131ULL, 992305282156031ULL, 992323523278931ULL, 992341764401831ULL, 1196266900300931ULL, 1196331655791731ULL, 1196396411282531ULL, 1196461166773331ULL, 1363806213708551ULL, 1363923696714191ULL, 1364041179719831ULL, 1364158662725471ULL, 1504145477032811ULL, 1504390303845041ULL, 1504635130657271ULL, 1504879957469501ULL, 1506606424567241ULL, 1506633383579321ULL, 1506660342591401ULL, 1506687301603481ULL, 1547277513006761ULL, 1547538621604511ULL, 1547799730202261ULL, 1548060838800011ULL, 1551543396451661ULL, 1551708809259221ULL, 1551874222066781ULL, 1552039634874341ULL, 1554535163864141ULL, 1554630942026621ULL, 1554726720189101ULL, 1554822498351581ULL}},  // 10-tuples in groups of 4, valid up to primorial max 1400, difficulty around 2190.
	{{0, 2, 4, 6, 2, 6, 4, 2, 4, 2}, {226193845148627ULL, 226374233346617ULL, 226554621544607ULL, 226735009742597ULL, 301570107719117ULL, 301713410008247ULL, 301856712297377ULL, 302000014586507ULL, 443216221842827ULL, 443355092154407ULL, 443493962465987ULL, 443632832777567ULL, 594353915119397ULL, 594554524107977ULL, 594755133096557ULL, 594955742085137ULL, 602484187049177ULL, 602527797215777ULL, 602571407382377ULL, 602615017548977ULL, 620850037212257ULL, 621267059407847ULL, 621684081603437ULL, 622101103799027ULL, 631774346860817ULL, 632202123099707ULL, 632629899338597ULL, 633057675577487ULL, 638409170551277ULL, 638417212765457ULL, 638425254979637ULL, 638433297193817ULL, 652691407502987ULL, 652768901389457ULL, 652846395275927ULL, 652923889162397ULL, 715627356082337ULL, 715866860827817ULL, 716106365573297ULL, 716345870318777ULL, 751355932060817ULL, 751594040050937ULL, 751832148041057ULL, 752070256031177ULL, 847107141137417ULL, 847125608236067ULL, 847144075334717ULL, 847162542433367ULL, 850067287283297ULL, 850306665932807ULL, 850546044582317ULL, 850785423231827ULL, 963140918896127ULL, 963391264321457ULL, 963641609746787ULL, 963891955172117ULL, 1322395947238337ULL, 1322624640823457ULL, 1322853334408577ULL, 1323082027993697ULL, 1426366146255287ULL, 1426606782441077ULL, 1426847418626867ULL, 1427088054812657ULL, 1450867151696087ULL, 1450935595771787ULL, 1451004039847487ULL, 1451072483923187ULL}}, // 10-tuples in groups of 4, valid up to primorial max 1400, difficulty around 2190.
	// 11-tuples
	{{0, 2, 4, 2, 4, 6, 2, 6, 4, 2, 4}, {380284918609481ULL, 437163765888581ULL, 701889794782061ULL, 980125031081081ULL, 1277156391416021ULL, 1487854607298791ULL, 1833994713165731ULL, 2115067287743141ULL, 2325810733931801ULL, 3056805353932061ULL, 3252606350489381ULL, 3360877662097841ULL, 3501482688249431ULL, 3595802556731501ULL, 3843547642594391ULL, 5000014653723821ULL, 5861268883004651ULL, 7486645325734691ULL, 7933248530182091ULL, 8760935349271991ULL, 8816939536219931ULL, 8871465225933041ULL, 9354490866900911ULL, 11096062107205961ULL, 11096434547374061ULL, 13764730155211151ULL, 13884748604026031ULL, 17438667992681051ULL, 20362378935668501ULL, 20471700514990841ULL, 20475715985020181ULL, 20614750499829371ULL, 21465425387541251ULL, 21628360938574121ULL, 21817283854511261ULL, 22238558064758921ULL, 22318056296221571ULL, 22733842556089781ULL, 22849881428489231ULL, 23382987892499351ULL, 23417442472403711ULL, 25964083184094941ULL, 26515897161980111ULL, 29512383574028471ULL, 30074756036270831ULL, 30310618347929651ULL, 30402250951007051ULL, 30413977411117031ULL, 33502273017038711ULL, 33508988966488151ULL, 33976718302847051ULL, 34783522781262371ULL, 37564605737538611ULL, 37606024583356961ULL, 39138758504100371ULL, 40205947750578341ULL, 40257009922154141ULL, 40392614725338761ULL, 40504121267225501ULL, 41099072498143391ULL, 41289201480321911ULL, 41543933848913381ULL, 42218492028808211ULL, 43938526447515431ULL}}, // OEIS A213645 (12-tuples)
	{{0, 4, 2, 4, 6, 2, 6, 4, 2, 4, 2}, {1418575498573ULL, 27899359258003ULL, 34460918582323ULL, 76075560855373ULL, 186460616596333ULL, 218021188549243ULL, 234280497145543ULL, 282854319391723ULL, 345120905374093ULL, 346117552180633ULL, 604439135284063ULL, 727417501795063ULL, 1041814617748753ULL, 1090754719898923ULL, 1539765965257753ULL, 3152045700948223ULL, 3323127757029313ULL, 3449427485143873ULL, 4422879865247923ULL, 4525595253334003ULL, 4730773080017833ULL, 5462875671033013ULL, 6147764065076713ULL, 6205707895751443ULL, 6308411019731053ULL, 7582919852522863ULL, 7791180222409663ULL, 9162887985581563ULL, 9305359177794913ULL, 10096106139749863ULL, 10349085616714693ULL, 10744789916260633ULL, 10932016019429353ULL, 11140102475962693ULL, 12448240792011103ULL, 14727257011031413ULL, 16892267700442213ULL, 17963729763800053ULL, 18908121647739403ULL, 19028992697498863ULL, 19756696515786463ULL, 20252223877980943ULL, 20429666791949263ULL, 21680774776901473ULL, 21682173462980263ULL, 23076668788453513ULL, 24036602580170413ULL, 24101684579532793ULL, 25053289894907353ULL, 25309078073142943ULL, 25662701041982083ULL, 25777719656829373ULL, 26056424604564433ULL, 26315911419972253ULL, 26866456999592443ULL, 26887571851660753ULL, 27303559129791793ULL, 27839080743588193ULL, 28595465291933773ULL, 29137316070747733ULL, 30824439453812083ULL, 31395828815154883ULL, 31979851757518513ULL, 32897714831936803ULL}}, // OEIS A213601 (12-tuples) + 6
	// 12-tuples
	{{0, 2, 4, 2, 4, 6, 2, 6, 4, 2, 4, 6}, {380284918609481ULL, 437163765888581ULL, 701889794782061ULL, 980125031081081ULL, 1277156391416021ULL, 1487854607298791ULL, 1833994713165731ULL, 2115067287743141ULL, 2325810733931801ULL, 3056805353932061ULL, 3252606350489381ULL, 3360877662097841ULL, 3501482688249431ULL, 3595802556731501ULL, 3843547642594391ULL, 5000014653723821ULL, 5861268883004651ULL, 7486645325734691ULL, 7933248530182091ULL, 8760935349271991ULL, 8816939536219931ULL, 8871465225933041ULL, 9354490866900911ULL, 11096062107205961ULL, 11096434547374061ULL, 13764730155211151ULL, 13884748604026031ULL, 17438667992681051ULL, 20362378935668501ULL, 20471700514990841ULL, 20475715985020181ULL, 20614750499829371ULL, 21465425387541251ULL, 21628360938574121ULL, 21817283854511261ULL, 22238558064758921ULL, 22318056296221571ULL, 22733842556089781ULL, 22849881428489231ULL, 23382987892499351ULL, 23417442472403711ULL, 25964083184094941ULL, 26515897161980111ULL, 29512383574028471ULL, 30074756036270831ULL, 30310618347929651ULL, 30402250951007051ULL, 30413977411117031ULL, 33502273017038711ULL, 33508988966488151ULL, 33976718302847051ULL, 34783522781262371ULL, 37564605737538611ULL, 37606024583356961ULL, 39138758504100371ULL, 40205947750578341ULL, 40257009922154141ULL, 40392614725338761ULL, 40504121267225501ULL, 41099072498143391ULL, 41289201480321911ULL, 41543933848913381ULL, 42218492028808211ULL, 43938526447515431ULL}}, // OEIS A213645
	{{0, 6, 4, 2, 4, 6, 2, 6, 4, 2, 4, 2}, {1418575498567ULL, 27899359257997ULL, 34460918582317ULL, 76075560855367ULL, 186460616596327ULL, 218021188549237ULL, 234280497145537ULL, 282854319391717ULL, 345120905374087ULL, 346117552180627ULL, 604439135284057ULL, 727417501795057ULL, 1041814617748747ULL, 1090754719898917ULL, 1539765965257747ULL, 3152045700948217ULL, 3323127757029307ULL, 3449427485143867ULL, 4422879865247917ULL, 4525595253333997ULL, 4730773080017827ULL, 5462875671033007ULL, 6147764065076707ULL, 6205707895751437ULL, 6308411019731047ULL, 7582919852522857ULL, 7791180222409657ULL, 9162887985581557ULL, 9305359177794907ULL, 10096106139749857ULL, 10349085616714687ULL, 10744789916260627ULL, 10932016019429347ULL, 11140102475962687ULL, 12448240792011097ULL, 14727257011031407ULL, 16892267700442207ULL, 17963729763800047ULL, 18908121647739397ULL, 19028992697498857ULL, 19756696515786457ULL, 20252223877980937ULL, 20429666791949257ULL, 21680774776901467ULL, 21682173462980257ULL, 23076668788453507ULL, 24036602580170407ULL, 24101684579532787ULL, 25053289894907347ULL, 25309078073142937ULL, 25662701041982077ULL, 25777719656829367ULL, 26056424604564427ULL, 26315911419972247ULL, 26866456999592437ULL, 26887571851660747ULL, 27303559129791787ULL, 27839080743588187ULL, 28595465291933767ULL, 29137316070747727ULL, 30824439453812077ULL, 31395828815154877ULL, 31979851757518507ULL, 32897714831936797ULL}} // OEIS A213601
};

struct MinerParameters {
	uint16_t threads, sieveWorkers, tupleLengthMin;
	uint64_t primorialNumber, primeTableLimit;
	bool useAvx2;
	uint64_t sieveBits, sieveSize, sieveWords, sieveIterations;
	std::vector<uint64_t> pattern, primorialOffsets;
	double restartDifficultyFactor;
	
	MinerParameters() :
		threads(0), sieveWorkers(0), tupleLengthMin(0),
		primorialNumber(0), primeTableLimit(0),
		useAvx2(false),
		sieveBits(0), sieveSize(0), sieveWords(0), sieveIterations(0),
		pattern{}, primorialOffsets{},
		restartDifficultyFactor(1.03) {}
};

class Options {
	MinerParameters _minerParameters;
	std::string _host, _username, _password, _mode, _payoutAddress, _secret, _tuplesFile;
	uint64_t _filePrimeTableLimit;
	uint16_t _debug, _port, _threads, _donate;
	double _refreshInterval, _difficulty, _benchmarkBlockInterval, _benchmarkTimeLimit;
	uint64_t _benchmarkPrimeCountLimit;
	std::vector<std::string> _rules;
	std::vector<std::string> _options;
	
	void _parseLine(std::string, std::string&, std::string&) const;
	void _stopConfig() const;
	
	public:
	Options() : // Default options: Standard Benchmark with 8 threads
		_host("127.0.0.1"),
		_username(""),
		_password(""),
		_mode("Benchmark"),
		_payoutAddress("ric1qr3yxckxtl7lacvtuzhrdrtrlzvlydane2h37ja"),
		_secret("/rM0.93a/"),
		_tuplesFile("Tuples.txt"),
		_filePrimeTableLimit(0),
		_debug(0),
		_port(28332),
		_donate(2),
		_refreshInterval(30.),
		_difficulty(1024.),
		_benchmarkBlockInterval(150.),
		_benchmarkTimeLimit(86400.),
		_benchmarkPrimeCountLimit(1000000),
		_rules{"segwit"},
		_options{} {}
	
	void loadFileOptions(const std::string&, const bool);
	void loadCommandOptions(const int, char**);
	void parseOptions();
	
	MinerParameters minerParameters() const {return _minerParameters;}
	std::string mode() const {return _mode;}
	std::string host() const {return _host;}
	uint16_t port() const {return _port;}
	std::string username() const {return _username;}
	std::string password() const {return _password;}
	std::string payoutAddress() const {return _payoutAddress;}
	std::string secret() const {return _secret;}
	std::string tuplesFile() const {return _tuplesFile;}
	uint64_t filePrimeTableLimit() const {return _filePrimeTableLimit;}
	uint16_t donate() const {return _donate;}
	double refreshInterval() const {return _refreshInterval;}
	double difficulty() const {return _difficulty;}
	double benchmarkBlockInterval() const {return _benchmarkBlockInterval;}
	double benchmarkTimeLimit() const {return _benchmarkTimeLimit;}
	uint64_t benchmarkPrimeCountLimit() const {return _benchmarkPrimeCountLimit;}
	std::vector<std::string> rules() const {return _rules;}
};

#endif
