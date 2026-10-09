// Swedish day and feast titles for the Greek, Antiochian and Slavic calendar tables.
// Terms follow Kristi Uppståndelses ortodoxa församling (Göteborg, Antiochia)
// where its texts use them; other titles use common Swedish Orthodox usage.
#include "core/model.hpp"
#include <map>
#include <regex>
namespace ortho {
namespace {
std::string ordinal(int n) {
    const int last = n % 10, tens = n % 100;
    return std::to_string(n) + ((last == 1 || last == 2) && tens != 11 && tens != 12 ? ":a" : ":e");
}
std::string weekday(const std::string& english) {
    static const std::map<std::string, std::string> days = {
        {"Monday", "Måndag"}, {"Tuesday", "Tisdag"},  {"Wednesday", "Onsdag"}, {"Thursday", "Torsdag"},
        {"Friday", "Fredag"}, {"Saturday", "Lördag"}, {"Sunday", "Söndag"}};
    return days.at(english);
}
int number(const std::string& word) {
    static const std::map<std::string, int> words = {{"First", 1},  {"Second", 2}, {"Third", 3},
                                                     {"Fourth", 4}, {"Fifth", 5},  {"Sixth", 6}};
    if (auto it = words.find(word); it != words.end())
        return it->second;
    return std::stoi(word);
}
const std::map<std::string, std::string>& names() {
    static const std::map<std::string, std::string> table = {
        // Paschal cycle
        {"Beginning of the Lenten Triodion", "Triodions början"},
        {"Sunday of the Publican and the Pharisee", "Publikanens och fariséns söndag"},
        {"Sunday of Zacchaeus", "Sackeus söndag"},
        {"Sunday of the Prodigal Son", "Den förlorade sonens söndag"},
        {"Sunday of Meatfare", "Domsöndagen · köttfastans söndag"},
        {"Sunday of the Last Judgment", "Domsöndagen"},
        {"Sunday of Cheesefare", "Ostavståendets söndag"},
        {"Forgiveness Sunday", "Förlåtelsens söndag"},
        {"Beginning of the Great Fast", "Stora fastans början"},
        {"Sunday of Orthodoxy", "Ortodoxins söndag"},
        {"Veneration of the Precious Cross", "Korsärandets söndag"},
        {"Great Canon of St Andrew of Crete", "Andreas av Kretas stora kanon"},
        {"Saturday of the Akathist to the Most-Holy Theotokos", "Akatistlördagen"},
        {"Lazarus Saturday", "Lazaroslördagen"},
        {"Palm Sunday", "Palmsöndagen"},
        {"Great and Holy Monday", "Stora måndagen"},
        {"Great and Holy Tuesday", "Stora tisdagen"},
        {"Great and Holy Wednesday", "Stora onsdagen"},
        {"Great and Holy Thursday", "Stora torsdagen"},
        {"Great and Holy Friday", "Stora fredagen"},
        {"Great and Holy Saturday", "Stora lördagen"},
        {"Holy Pascha", "Påsk"},
        {"The Resurrection of our Lord and Savior Jesus Christ", "Kristi uppståndelse"},
        {"The Life-Giving Spring of the Most-holy Theotokos", "Gudaföderskan, den livgivande källan"},
        {"Antipascha: 2nd Sunday of Pascha", "Thomassöndagen"},
        {"St Thomas Sunday", "Thomassöndagen"},
        {"Day of Rejoicing (Radonitsa)", "Glädjens dag (Radonitsa)"},
        {"Myrrhbearing Women", "De myrrabärande kvinnornas söndag"},
        {"Paralytic", "Den lame mannens söndag"},
        {"Midfeast of Pentecost", "Midpingst"},
        {"Samaritan Woman", "Samaritiska kvinnans söndag"},
        {"Blind Man", "Den blindföddes söndag"},
        {"Ascension of the Lord", "Kristi himmelsfärd"},
        {"The Ascension of our Lord, God, and Saviour Jesus Christ", "Kristi himmelsfärd"},
        {"Fathers of the 1st Six Ecumenical Councils", "Fäderna vid de sex första ekumeniska koncilierna"},
        {"Holy Fathers of the First Ecumenical Council", "Fäderna vid det första ekumeniska konciliet"},
        {"Fathers of the Seventh Ecumenical Council", "Fäderna vid det sjunde ekumeniska konciliet"},
        {"Memorial Saturday", "Själalördag"},
        {"Memorial (Demetrius) Saturday", "Själalördag före Demetrios"},
        {"Holy Pentecost", "Pingst"},
        {"Day of the Holy Spirit", "Den Helige Andes dag"},
        {"Third Day of the Trinity", "Tredje pingstdagen"},
        {"All Saints", "Alla helgons söndag"},
        {"All Saints of America, All Saints of Russia", "Alla Amerikas helgon · alla Rysslands helgon"},
        {"All Saints of Antioch", "Alla Antiokias helgon"},
        {"Commemoration of Departed Righteous Monastics", "Minnet av de avsomnade rättfärdiga monastikerna"},
        // Great feasts and their fore- and afterfeasts
        {"Church New Year", "Det kyrkliga nyåret"},
        {"Nativity of the Most-Holy Theotokos", "Gudaföderskans födelse"},
        {"Exaltation (Elevation) of the Precious Cross", "Det heliga korsets upphöjelse"},
        {"Entry of the Most-Holy Theotokos into the Temple", "Gudaföderskans införande i templet"},
        {"Nativity of Christ", "Kristi födelse"},
        {"Circumcision of Our Lord", "Herrens omskärelse"},
        {"Theophany of Our Lord and Savior Jesus Christ", "Teofania · Kristi dop"},
        {"Meeting of Christ in the Temple", "Kristi frambärande i templet"},
        {"Annunciation Most Holy Theotokos", "Gudaföderskans bebådelse"},
        {"Transfiguration of Our Lord", "Kristi förklaring"},
        {"Dormition of the Most-Holy Theotokos", "Gudaföderskans avsomnande"},
        {"Procession of the Lifegiving Cross", "Det livgivande korsets utbärande"},
        {"Forefeast of the Procession of the Lifegiving Cross",
         "Förfest till det livgivande korsets utbärande"},
        {"Forefeast of Entry", "Förfest till Gudaföderskans införande i templet"},
        {"Forefeast of Nativity", "Förfest till Kristi födelse"},
        {"Forefeast of Theophany", "Förfest till Teofania"},
        {"Forefeast of Annunciation", "Förfest till Gudaföderskans bebådelse"},
        {"Forefeast of Transfiguration", "Förfest till Kristi förklaring"},
        {"Forefeast of Dormition", "Förfest till Gudaföderskans avsomnande"},
        {"Afterfeast of the Transfiguration", "Efterfest till Kristi förklaring"},
        {"Eve of Nativity", "Julafton"},
        {"Eve of Theophany", "Teofanias afton"},
        {"Royal Hours of Nativity", "Kungliga timmarna före Kristi födelse"},
        {"Royal Hours of Theophany", "Kungliga timmarna före Teofania"},
        {"Leavetaking Exaltation", "Avslutning av korsets upphöjelse"},
        {"Leavetaking of the Entry", "Avslutning av Gudaföderskans införande i templet"},
        {"Leavetaking of the Nativity", "Avslutning av Kristi födelse"},
        {"Leavetaking of Theophany", "Avslutning av Teofania"},
        {"Leavetaking of Meeting", "Avslutning av Kristi frambärande i templet"},
        {"Leavetaking of Pascha", "Avslutning av påsken"},
        {"Forefeast of Ascension", "Förfest till Kristi himmelsfärd"},
        {"Leavetaking of Mid-Pentecost", "Avslutning av Midpingst"},
        {"Leavetaking of Ascension", "Avslutning av Kristi himmelsfärd"},
        {"Leavetaking of Pentecost", "Avslutning av pingst"},
        {"Leavetaking of Transfiguration", "Avslutning av Kristi förklaring"},
        {"Leavetaking of Dormition", "Avslutning av Gudaföderskans avsomnande"},
        // Sundays and Saturdays around the feasts
        {"Saturday before Elevation", "Lördagen före korsets upphöjelse"},
        {"Sunday before Elevation", "Söndagen före korsets upphöjelse"},
        {"Saturday after Elevation", "Lördagen efter korsets upphöjelse"},
        {"Sunday after Elevation", "Söndagen efter korsets upphöjelse"},
        {"Sunday of the Forefathers", "Förfädernas söndag"},
        {"Saturday before Nativity", "Lördagen före Kristi födelse"},
        {"Sunday before Nativity", "Söndagen före Kristi födelse"},
        {"Saturday after Nativity", "Lördagen efter Kristi födelse"},
        {"Sunday after Nativity", "Söndagen efter Kristi födelse"},
        {"Saturday before Theophany", "Lördagen före Teofania"},
        {"Sunday before Theophany", "Söndagen före Teofania"},
        {"Saturday after Theophany", "Lördagen efter Teofania"},
        {"Sunday after Theophany", "Söndagen efter Teofania"},
        // Saints
        {"1st and 2nd Finding Honorable Head of St John the Baptist",
         "Första och andra fyndet av Johannes Döparens huvud"},
        {"3rd Finding of the Head of St John the Baptist", "Tredje fyndet av Johannes Döparens huvud"},
        {"Conception of St John the Baptist", "Johannes Döparens avlelse"},
        {"Nativity of St John the Baptist", "Johannes Döparens födelse"},
        {"Beheading of St John the Baptist", "Johannes Döparens halshuggning"},
        {"Synaxis of St John the Baptist", "Johannes Döparens högtid"},
        {"Conception by St Anna of the Theotokos", "Gudaföderskans avlelse"},
        {"Synaxis of the Most-Holy Theotokos", "Gudaföderskans högtid"},
        {"Synaxis of Archangel Michael and the Bodiless Powers",
         "Ärkeängeln Mikael och alla himmelska makter"},
        {"Synaxis of the Holy Unmercenaries", "De heliga oavlönade läkarna"},
        {"Synaxis of the Twelve Apostles", "De tolv apostlarna"},
        {"Synaxis 3 Hierarchs: Basil the Great, Gregory the Theologian, John Chrysostom",
         "De tre hierarkerna: Basileios den store, Gregorios Teologen och Johannes Chrysostomos"},
        {"Image of Christ Not Made by Hands", "Kristi icke handgjorda ikon"},
        {"Founding of Church of the Holy Sepulchre", "Invigningen av Uppståndelsekyrkan i Jerusalem"},
        {"Protomartyr Stephen", "Protomartyren Stefanos"},
        {"Holy Apostle Andrew the First Called", "Aposteln Andreas den förstkallade"},
        {"Holy Apostle James, Brother of St John", "Aposteln Jakob, Johannes broder"},
        {"Holy Apostle James, Brother of the Lord", "Aposteln Jakob, Herrens broder"},
        {"Holy Apostle John the Theologian", "Aposteln Johannes Teologen"},
        {"Repose of St John the Theologian", "Aposteln Johannes Teologens avsomnande"},
        {"Holy Apostle Jude, Brother of the Lord", "Aposteln Judas, Herrens broder"},
        {"Holy Apostle Philip", "Aposteln Filippos"},
        {"Holy Apostle and Evangelist Luke", "Aposteln och evangelisten Lukas"},
        {"Holy Apostle and Evangelist Mark", "Aposteln och evangelisten Markus"},
        {"Holy Apostle and Evangelist Mathew", "Aposteln och evangelisten Matteus"},
        {"Holy Apostles Bartholomew and Barnabas", "Apostlarna Bartolomeus och Barnabas"},
        {"Holy Apostles Peter and Paul", "Apostlarna Petrus och Paulus"},
        {"Apostle Simon the Zealot", "Aposteln Simon seloten"},
        {"Holy Prophet Elijah", "Profeten Elia"},
        {"Holy Forty Martyrs of Sebaste", "De fyrtio martyrerna i Sebaste"},
        {"Holy Greatmartyr, Victorybearer and Wonderworker George",
         "Stormartyren Georgios, segerbäraren och undergöraren"},
        {"Greatmartyr Demetrius", "Stormartyren Demetrios"},
        {"Greatmartyr and Healer Panteleimon", "Stormartyren och läkaren Panteleimon"},
        {"SS Constantine and Helen, Equals-to-the-Apostles",
         "De heliga Konstantin och Helena, apostlarnas likar"},
        {"St Gregory the Theologian", "Den helige Gregorios Teologen"},
        {"St John Chrysostom, Archbishop of Constantinople",
         "Den helige Johannes Chrysostomos, ärkebiskop av Konstantinopel"},
        {"Transl. Relics of St John Chrysostom", "Återförandet av den helige Johannes Chrysostomos reliker"},
        {"St Nicholas the Wonderworker, Abp. of Myra in Lycia",
         "Den helige Nikolaos Undergöraren, ärkebiskop av Myra i Lykien"},
        {"Ven. Chariton the Confessor", "Den helige Chariton Bekännaren"},
        {"Ven. Euthymius the Great", "Den helige Euthymios den store"},
        {"Ven. Godbearing Anthony the Great", "Den helige Antonios den store"},
        {"Ven. Sabbas the Sanctified", "Den helige Sabbas den helgade"},
        {"New Martyrs and Confessors of Russia", "Rysslands nya martyrer och bekännare"},
        {"Repose St. Tikhon, Patriarch of Moscow, Enlightener N. America",
         "Den helige Tichon, patriark av Moskva och Nordamerikas upplysare"},
        {"St Raphael Bishop of Brooklyn", "Den helige Rafael, biskop av Brooklyn"},
        // Slavic tradition. Slavic saints keep their Russian name forms.
        {"Protection (Pokrov) of the Most-Holy Theotokos", "Gudaföderskans beskydd (Pokrov)"},
        {"Repose St Innocent, Metr. Moscow and Apostle to Americas",
         "Den helige Innokentij, metropolit av Moskva och Amerikas apostel"},
        {"Trans. Rel. Boris and Gleb", "Återförandet av de heliga Boris och Glebs reliker"},
        {"Martyrs Boris and Gleb, Passionbearers", "De heliga lidandesbärarna Boris och Gleb"},
        {"Ven. Theodosius, Abbot of the Kiev Caves", "Den helige Feodosij, abbot i Kievs grottkloster"},
        {"Ven. Anthony of the Kiev Caves", "Den helige Antonij av Kievs grottkloster"},
        {"SS Cyril and Methodius, Apostles to the Slavs",
         "De heliga Kyrillos och Methodios, slavernas apostlar"},
        {"Great Prince Vladimir, Equal-to-the-Apostles, Enlightener of the Lands of Rus",
         "Den helige storfursten Vladimir, apostlarnas like och upplysare av Rus"},
        {"Unc. Rel. Ven. Seraphim of Sarov", "Fyndet av den helige Serafim av Sarovs reliker"},
        {"Repose of St Jacob Netsvetov, Enlightener of the Peoples of Alaska",
         "Den helige Jakov Netsvetov, upplysare av Alaskas folk"},
        {"Ven. Job of Pochaev", "Den helige Iov av Potjajev"},
        {"Repose of Ven. Sergius of Radonezh", "Den helige Sergij av Radonezj"},
        {"Rt. Blv. Great Prince Alexander Nevsky", "Den helige storfursten Aleksandr Nevskij"},
        {"Repose Ven. Herman of Alaska, Wonderworker of All America",
         "Den helige German av Alaska, hela Amerikas undergörare"},
        {"Repose of St. John of Kronstadt", "Den helige Ioann av Kronstadt"},
    };
    return table;
}
std::optional<std::string> single(const std::string& english) {
    if (auto it = names().find(english); it != names().end())
        return it->second;
    static const std::string days = "(Monday|Tuesday|Wednesday|Thursday|Friday|Saturday)";
    static const std::regex pentecost_sunday(R"((\d+)(?:st|nd|rd|th) Sunday after Pentecost)"),
        pentecost_week(days + R"( of the (\d+)(?:st|nd|rd|th) week after Pentecost)"),
        pascha_sunday(R"((\d+)(?:st|nd|rd|th) Sunday of Pascha)"),
        pascha_week(days + R"( of the (\d+)(?:st|nd|rd|th) Sunday of Pascha)"), bright("Bright " + days),
        lent_sunday(R"((First|Second|Third|Fourth|Fifth) (Saturday|Sunday) of Lent)"),
        lent_week(days + R"( of the (First|Second|Third|Fourth|Fifth|Sixth) Week of Lent)"),
        meatfare(days + " of Meatfare"), cheesefare(days + " of Cheesefare"),
        cheesefare_day("Cheesefare " + days);
    std::smatch m;
    if (std::regex_match(english, m, pentecost_sunday))
        return ordinal(std::stoi(m[1])) + " söndagen efter pingst";
    if (std::regex_match(english, m, pentecost_week))
        return weekday(m[1]) + " i " + ordinal(std::stoi(m[2])) + " veckan efter pingst";
    if (std::regex_match(english, m, pascha_sunday))
        return "Påsktidens " + ordinal(std::stoi(m[1])) + " söndag";
    if (std::regex_match(english, m, pascha_week))
        return weekday(m[1]) + " efter påsktidens " + ordinal(std::stoi(m[2])) + " söndag";
    if (std::regex_match(english, m, bright))
        return weekday(m[1]) + " i ljusa veckan";
    if (std::regex_match(english, m, lent_sunday))
        return ordinal(number(m[1])) + (m[2] == "Sunday" ? " söndagen" : " lördagen") + " i stora fastan";
    if (std::regex_match(english, m, lent_week))
        return weekday(m[1]) + " i stora fastans " + ordinal(number(m[2])) + " vecka";
    if (std::regex_match(english, m, meatfare))
        return weekday(m[1]) + " i köttfasteveckan";
    if (std::regex_match(english, m, cheesefare) || std::regex_match(english, m, cheesefare_day))
        return weekday(m[1]) + " i ostveckan";
    return std::nullopt;
}
} // namespace
std::optional<std::string> swedish_title(const std::string& english) {
    if (auto whole = single(english))
        return whole;
    // Coinciding days are joined as "A – B" or "A / B" in the source table.
    for (const std::string separator : {" – ", " / "}) {
        const auto at = english.find(separator);
        if (at == std::string::npos)
            continue;
        auto first = single(english.substr(0, at)), second = single(english.substr(at + separator.size()));
        if (first && second)
            return *first + " · " + *second;
    }
    return std::nullopt;
}
} // namespace ortho
