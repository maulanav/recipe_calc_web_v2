#include <iostream>
#include <iomanip>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <map>
#include <limits>
#include <cstdlib>
#include <cctype>

using namespace std;

// ============================================================
//  Token / Pilihan menu
// ============================================================
const string RACING   = "00";

// ============================================================
//  Parser JSON minimal (tanpa library eksternal)
//  Cukup untuk data resep: objek, array, string, angka, boolean.
// ============================================================
struct Json {
    enum Type { NUL, OBJ, ARR, STR, NUM, BOOL } type = NUL;
    map<string, Json> obj;
    vector<Json>      arr;
    string            str;
    double            num = 0;
    bool              boolean = false;

    bool isObject() const { return type == OBJ; }
    bool isArray()  const { return type == ARR; }
    bool isString() const { return type == STR; }

    const Json& operator[](const string &key) const {
        static const Json nullJson;
        auto it = obj.find(key);
        if (it != obj.end()) return it->second;
        return nullJson;
    }

    double asDouble() const {
        if (type == NUM) return num;
        if (type == STR) { try { return stod(str); } catch (...) { return 0.0; } }
        if (type == BOOL) return boolean ? 1.0 : 0.0;
        return 0.0;
    }
    string asString() const { return (type == STR) ? str : ""; }
    bool   asBool()   const { return (type == BOOL) ? boolean : (type == NUM ? num != 0 : false); }
};

struct JsonParser {
    const string &s;
    size_t pos = 0;
    explicit JsonParser(const string &src) : s(src) {}

    void skipWs() { while (pos < s.size() && isspace((unsigned char)s[pos])) pos++; }

    Json parse() {
        skipWs();
        if (pos >= s.size()) return Json();
        char c = s[pos];
        if (c == '{') return parseObject();
        if (c == '[') return parseArray();
        if (c == '"') return parseString();
        if (c == 't' || c == 'f') return parseBool();
        if (c == 'n') { pos += 4; return Json(); }
        return parseNumber();
    }

    Json parseObject() {
        Json j; j.type = Json::OBJ;
        pos++; skipWs();
        if (pos < s.size() && s[pos] == '}') { pos++; return j; }
        while (true) {
            skipWs();
            if (pos >= s.size() || s[pos] != '"') break;
            Json key = parseString();
            skipWs();
            if (pos < s.size() && s[pos] == ':') pos++;
            Json val = parse();
            j.obj[key.str] = val;
            skipWs();
            if (pos < s.size() && s[pos] == ',') { pos++; continue; }
            if (pos < s.size() && s[pos] == '}') { pos++; break; }
            break;
        }
        return j;
    }

    Json parseArray() {
        Json j; j.type = Json::ARR;
        pos++; skipWs();
        if (pos < s.size() && s[pos] == ']') { pos++; return j; }
        while (true) {
            j.arr.push_back(parse());
            skipWs();
            if (pos < s.size() && s[pos] == ',') { pos++; continue; }
            if (pos < s.size() && s[pos] == ']') { pos++; break; }
            break;
        }
        return j;
    }

    Json parseString() {
        Json j; j.type = Json::STR;
        pos++;
        string out;
        while (pos < s.size()) {
            char c = s[pos++];
            if (c == '"') break;
            if (c == '\\' && pos < s.size()) {
                char e = s[pos++];
                switch (e) {
                    case 'n': out += '\n'; break;
                    case 't': out += '\t'; break;
                    case 'r': out += '\r'; break;
                    case '\\': out += '\\'; break;
                    case '"': out += '"'; break;
                    case '/': out += '/'; break;
                    default: out += e; break;
                }
            } else { out += c; }
        }
        j.str = out;
        return j;
    }

    Json parseBool() {
        Json j; j.type = Json::BOOL;
        if (s.compare(pos, 4, "true") == 0)      { j.boolean = true;  pos += 4; }
        else if (s.compare(pos, 5, "false") == 0) { j.boolean = false; pos += 5; }
        return j;
    }

    Json parseNumber() {
        Json j; j.type = Json::NUM;
        size_t start = pos;
        if (pos < s.size() && (s[pos] == '-' || s[pos] == '+')) pos++;
        while (pos < s.size() && (isdigit((unsigned char)s[pos]) || s[pos] == '.')) pos++;
        j.num = strtod(s.substr(start, pos - start).c_str(), nullptr);
        return j;
    }
};

static bool jsonParse(const string &text, Json &out) {
    if (text.empty()) return false;
    JsonParser p(text);
    out = p.parse();
    return out.type == Json::OBJ;
}

// ============================================================
//  ASCII Art Header "GACOAN"
// ============================================================
void showHeader(bool racingMode = false) {
    cout << R"(
   ____    _    ____ ___    _    _   _ 
  / ___|  / \  / ___/ _ \  / \  | \ | |
 | |  _  / _ \| |  | | | |/ _ \ |  \| |
 | |_| |/ ___ \ |__| |_| / ___ \| |\  |
  \____/_/   \_\____\___/_/   \_\_| \_|
)";
    if (racingMode) { cout << "   =====  MODE RACING  =====\n"; }
    cout << "\n";
}

// ============================================================
//  Data bahan resep (dimuat dari JSON pada saat boot)
// ============================================================
struct Resep {
    string nama;
    string labelPowder;
    bool  bisaRacing;
    double powder;
    double airPanas;
    double airSuhuRuang;
    double creamer;
    double skm;
    double susuEvaporasi;
    double simpleSyrup;
};

struct BiangPorsi { double porsi, base, air, simple; };
struct BiangTehConfig { double pctBase=0, pctAir=0, pctSimple=0, gramPerPorsi=0; };

vector<Resep>      daftarResep;
vector<BiangPorsi> biangPorsiAcuan;
BiangTehConfig     biangTeh;

// ============================================================
//  Hardcoded fallback — dipakai bila JSON tidak bisa dimuat.
// ============================================================
static void useFallbackData() {
    daftarResep = {
        {"MILO",           "Powder Milo",           true,  1200, 2000, 570, 0,   0,   0,   260},
        {"THAI GREEN TEA", "Powder Thai Green Tea", false, 150,  2000, 480, 220, 660, 660, 560},
        {"LEMON TEA",      "Powder Lemon Tea",      true,  600,  2000, 1400,0,   0,   0,   0  },
        {"ORANGE",         "Powder Orange",         true,  380,  2000, 1620,0,   0,   0,   0  }
    };
    biangPorsiAcuan = {
        {20, 1818, 970, 1212},
        {30, 2727, 1454.4, 1818},
        {33, 3000, 1600, 2000},
    };
    biangTeh = { 0.455, 0.242, 0.303, 200.0 };
}

// ============================================================
//  Muat data dari env RESEP_JSON (prioritas), lalu file resep.json,
//  lalu fallback hardcoded.
// ============================================================
static bool fileExists(const string &path) {
    ifstream f(path.c_str());
    return f.good();
}

static bool applyJson(const Json &root) {
    const Json &arr = root["resep"];
    if (!arr.isArray()) return false;
    daftarResep.clear();
    for (const Json &r : arr.arr) {
        Resep x;
        x.nama        = r["nama"].asString();
        x.labelPowder = r["labelPowder"].isString() ? r["labelPowder"].asString()
                        : (x.nama == "MILO" ? "Powder Milo"
                           : x.nama == "THAI GREEN TEA" ? "Powder Thai Green Tea"
                           : x.nama == "LEMON TEA" ? "Powder Lemon Tea"
                           : "Powder Orange");
        x.bisaRacing    = r["bisaRacing"].asBool();
        x.powder        = r["powder"].asDouble();
        x.airPanas      = r["airPanas"].asDouble();
        x.airSuhuRuang  = r["airSuhuRuang"].asDouble();
        x.creamer       = r["creamer"].asDouble();
        x.skm           = r["skm"].asDouble();
        x.susuEvaporasi = r["susuEvaporasi"].asDouble();
        x.simpleSyrup   = r["simpleSyrup"].asDouble();
        daftarResep.push_back(x);
    }

    const Json &bt = root["biangTeh"];
    const Json &persen = bt["persen"];
    biangTeh.pctBase   = persen["base"].asDouble();
    biangTeh.pctAir    = persen["air"].asDouble();
    biangTeh.pctSimple = persen["simple"].asDouble();
    biangTeh.gramPerPorsi = bt["gramPerPorsi"].asDouble();

    biangPorsiAcuan.clear();
    const Json &pa = bt["porsiAcuan"];
    if (pa.isArray()) {
        for (const Json &e : pa.arr) {
            BiangPorsi p;
            p.porsi  = e["porsi"].asDouble();
            p.base   = e["base"].asDouble();
            p.air    = e["air"].asDouble();
            p.simple = e["simple"].asDouble();
            biangPorsiAcuan.push_back(p);
        }
    }
    return !daftarResep.empty();
}

static void loadData() {
    bool ok = false;

    // 1) Env RESEP_JSON (dipakai saat dijalankan di container sandbox)
    const char *env = getenv("RESEP_JSON");
    if (env && *env) {
        Json root;
        ok = jsonParse(string(env), root) && applyJson(root);
        if (ok) cerr << "[info] data dimuat dari env RESEP_JSON\n";
    }

    // 2) File resep.json (lokal)
    if (!ok) {
        const char *paths[] = { "/usr/local/share/gacoan/resep.json", "resep.json", "./resep.json" };
        for (const char *p : paths) {
            if (fileExists(p)) {
                ifstream f(p);
                stringstream ss; ss << f.rdbuf();
                Json root;
                ok = jsonParse(ss.str(), root) && applyJson(root);
                if (ok) { cerr << "[info] data dimuat dari file " << p << "\n"; break; }
            }
        }
    }

    // 3) Fallback hardcoded
    if (!ok) {
        useFallbackData();
        cerr << "[peringatan] tidak ada JSON valid — memakai data default bawaan\n";
    }
}

// ============================================================
//  Tampilkan satu baris bahan
// ============================================================
void tampilkanBaris(const string &label, double gram, bool hilangkan = false) {
    if (hilangkan || gram == 0.0) return;
    cout << "   - " << left << setw(22) << label
         << right << setprecision(4) << setw(10) << gram
         << " gr\n";
}

// ============================================================
//  Tampilkan resep
// ============================================================
void tampilkanResep(const Resep &r, double jumlah, bool racingMode) {
    double pengali = jumlah;
    double totalAir = (r.airPanas + r.airSuhuRuang) * pengali;

    cout << "\n=========================================\n";
    cout << "        RESEP " << r.nama;
    if (racingMode) cout << "  (MODE RACING)";
    cout << "\n=========================================\n";
    cout << "   Jumlah resep : " << setprecision(4) << jumlah << " resep\n";
    cout << "   ( " << setprecision(4) << (jumlah * 20.0) << " porsi )\n";
    cout << "-----------------------------------------\n";

    tampilkanBaris(r.labelPowder, r.powder * pengali);
    if (r.creamer > 0)          tampilkanBaris("Creamer bubuk", r.creamer * pengali);
    if (r.skm > 0)              tampilkanBaris("SKM", r.skm * pengali);
    if (r.susuEvaporasi > 0)    tampilkanBaris("Susu evaporasi", r.susuEvaporasi * pengali);
    if (r.simpleSyrup > 0)      tampilkanBaris("Simple syrup", r.simpleSyrup * pengali);

    if (racingMode) {
        tampilkanBaris("Air suhu ruang", totalAir);
    } else {
        tampilkanBaris("Air panas", r.airPanas * pengali);
        if (r.airSuhuRuang > 0) tampilkanBaris("Air suhu ruang", r.airSuhuRuang * pengali);
    }
    cout << "-----------------------------------------\n";
}

// ============================================================
//  Input angka positif
// ============================================================
double inputAngkaPositif(const string &pesan) {
    double nilai;
    while (true) {
        cout << "   " << pesan;
        cin >> nilai;
        if (cin.fail() || nilai <= 0) {
            cin.clear();
            cin.ignore(numeric_limits<streamsize>::max(), '\n');
            cout << "   [!!] Input tidak valid. Masukkan angka positif.\n";
        } else {
            cin.ignore(numeric_limits<streamsize>::max(), '\n');
            return nilai;
        }
    }
}

double inputJumlah() { return inputAngkaPositif("Masukkan jumlah resep : "); }

string inputPilihan() {
    string input;
    cout << "   Pilih menu: ";
    cin >> input;
    cin.ignore(numeric_limits<streamsize>::max(), '\n');
    return input;
}

void alurSetelahResep(const Resep &r, double jumlah, bool racingMode) {
    system("clear");
    showHeader(racingMode);
    tampilkanResep(r, jumlah, racingMode);

    while (true) {
        cout << "\n   ====== PILIHAN ======\n";
        cout << "   1. Hitung lagi\n   2. Menu utama\n   3. Keluar program\n   Pilih: ";
        int pilihan; cin >> pilihan;
        cin.ignore(numeric_limits<streamsize>::max(), '\n');
        if (pilihan == 1) {
            system("clear"); showHeader(racingMode);
            cout << "   Resep " << r.nama << ":\n";
            double lagi = inputJumlah();
            system("clear"); showHeader(racingMode);
            tampilkanResep(r, lagi, racingMode);
            jumlah = lagi;
        } else if (pilihan == 2) { return; }
        else if (pilihan == 3) { system("clear"); cout << "\n   Terima kasih sudah menggunakan GACOAN!\n\n"; exit(0); }
        else { cout << "   [!!] Pilihan tidak valid.\n"; }
    }
}

void modeRacing() {
    while (true) {
        system("clear");
        showHeader(true);
        vector<int> urutan;
        for (size_t i = 0; i < daftarResep.size(); i++)
            if (daftarResep[i].bisaRacing) urutan.push_back((int)i);

        cout << "   Resep yang tersedia pada mode racing:\n";
        for (size_t i = 0; i < urutan.size(); i++)
            cout << "   " << (i + 1) << ". " << daftarResep[urutan[i]].nama << "\n";
        cout << "   0. Kembali ke menu utama\n\n";

        string pilihan = inputPilihan();
        if (pilihan == "0") return;

        int indeks = -1;
        for (size_t i = 0; i < urutan.size(); i++)
            if (std::to_string(i + 1) == pilihan) { indeks = urutan[i]; break; }

        if (indeks != -1) { double jumlah = inputJumlah(); alurSetelahResep(daftarResep[indeks], jumlah, true); }
        else { cout << "   [!!] Pilihan tidak valid.\n   Tekan Enter..."; cin.get(); }
    }
}

void tampilkanResepBiangTeh(double baseTeh, double air, double simple,
                            double totalBerat, double porsiPerkiraan) {
    cout << "\n=========================================\n";
    cout << "        RESEP BIANG TEH\n";
    cout << "=========================================\n";
    cout << "   ( ± " << setprecision(4) << porsiPerkiraan << " porsi )\n";
    cout << "   ( "  << setprecision(4) << totalBerat << " gr total )\n";
    cout << "-----------------------------------------\n";
    tampilkanBaris("Base Teh", baseTeh);
    tampilkanBaris("Air", air);
    tampilkanBaris("Simple syrup", simple);
    cout << "-----------------------------------------\n";
}

bool hitungBiangTeh(unsigned int &opsi, double &base, double &air,
                    double &simple, double &total, double &porsi);

void alurSetelahBiangTeh(double baseTeh, double air, double simple,
                         double totalBerat, double porsiPerkiraan) {
    system("clear");
    showHeader(false);
    tampilkanResepBiangTeh(baseTeh, air, simple, totalBerat, porsiPerkiraan);

    while (true) {
        cout << "\n   ====== PILIHAN ======\n";
        cout << "   1. Hitung lagi\n   2. Menu utama\n   3. Keluar program\n   Pilih: ";
        int pilihan; cin >> pilihan;
        cin.ignore(numeric_limits<streamsize>::max(), '\n');
        if (pilihan == 1) {
            system("clear"); showHeader(false);
            unsigned int opsi = 0; double b,a,s,tot,por;
            if (hitungBiangTeh(opsi,b,a,s,tot,por)) {
                system("clear"); showHeader(false);
                tampilkanResepBiangTeh(b,a,s,tot,por);
                baseTeh=b; air=a; simple=s; totalBerat=tot; porsiPerkiraan=por;
            }
        } else if (pilihan == 2) return;
        else if (pilihan == 3) { system("clear"); cout << "\n   Terima kasih sudah menggunakan GACOAN!\n\n"; exit(0); }
        else { cout << "   [!!] Pilihan tidak valid.\n"; }
    }
}

bool hitungBiangTeh(unsigned int &opsi, double &base, double &air,
                    double &simple, double &total, double &porsi) {
    system("clear");
    showHeader(false);
    cout << "   ====== BIANG TEH ======\n";
    cout << "   Komposisi: Base Teh " << biangTeh.pctBase * 100.0
         << "% | Air " << biangTeh.pctAir * 100.0
         << "% | Simple Syrup " << biangTeh.pctSimple * 100.0 << "%\n\n";
    cout << "   1. Hitung berdasarkan acuan satu bahan\n";
    cout << "   2. Hitung berdasarkan porsi\n   Pilih: ";
    int pilih; cin >> pilih;
    cin.ignore(numeric_limits<streamsize>::max(), '\n');
    if (pilih != 1 && pilih != 2) return false;

    if (pilih == 1) {
        cout << "\n   Bahan acuan:\n   1. Base Teh\n   2. Air\n   3. Simple Syrup\n";
        int bahan = (int)inputAngkaPositif("Pilih bahan acuan (1-3) : ");
        while (bahan < 1 || bahan > 3) {
            cout << "   [!!] Pilih 1-3.\n";
            bahan = (int)inputAngkaPositif("Pilih bahan acuan (1-3) : ");
        }
        double beratBahan = inputAngkaPositif("Masukkan berat bahan acuan (gr) : ");

        bool pakaiExact = false;
        for (size_t i = 0; i < biangPorsiAcuan.size(); i++) {
            double nilaiKolom;
            switch (bahan) {
                case 1: nilaiKolom = biangPorsiAcuan[i].base;   break;
                case 2: nilaiKolom = biangPorsiAcuan[i].air;    break;
                default: nilaiKolom = biangPorsiAcuan[i].simple; break;
            }
            if (beratBahan == nilaiKolom) {
                base = biangPorsiAcuan[i].base;
                air  = biangPorsiAcuan[i].air;
                simple = biangPorsiAcuan[i].simple;
                total  = base + air + simple;
                porsi  = biangPorsiAcuan[i].porsi;
                pakaiExact = true;
                break;
            }
        }

        if (!pakaiExact) {
            double persenBahan;
            switch (bahan) {
                case 1: persenBahan = biangTeh.pctBase;   break;
                case 2: persenBahan = biangTeh.pctAir;    break;
                default:persenBahan = biangTeh.pctSimple; break;
            }
            total  = beratBahan / persenBahan;
            base   = total * biangTeh.pctBase;
            air    = total * biangTeh.pctAir;
            simple = total * biangTeh.pctSimple;
            porsi  = total / biangTeh.gramPerPorsi;
        }
    } else {
        double inporsi = inputAngkaPositif("Masukkan jumlah porsi : ");
        bool pakaiAcuan = false;
        for (size_t i = 0; i < biangPorsiAcuan.size(); i++) {
            if (inporsi == biangPorsiAcuan[i].porsi) {
                base = biangPorsiAcuan[i].base;
                air  = biangPorsiAcuan[i].air;
                simple = biangPorsiAcuan[i].simple;
                total  = base + air + simple;
                porsi  = inporsi;
                pakaiAcuan = true;
                break;
            }
        }
        if (!pakaiAcuan) {
            double acuanBase=3000.0, acuanAir=1600.0, acuanSimple=2000.0;
            for (size_t i = 0; i < biangPorsiAcuan.size(); i++)
                if (biangPorsiAcuan[i].porsi == 33.0) { acuanBase=biangPorsiAcuan[i].base; acuanAir=biangPorsiAcuan[i].air; acuanSimple=biangPorsiAcuan[i].simple; break; }
            double skala = inporsi / 33.0;
            base = acuanBase * skala;
            air  = acuanAir * skala;
            simple = acuanSimple * skala;
            total  = (acuanBase + acuanAir + acuanSimple) * skala;
            porsi  = inporsi;
        }
    }
    opsi = pilih;
    return true;
}

void modeBiangTeh() {
    unsigned int opsi; double base, air, simple, total, porsi;
    if (hitungBiangTeh(opsi, base, air, simple, total, porsi))
        alurSetelahBiangTeh(base, air, simple, total, porsi);
    else { cout << "   [!!] Pilihan tidak valid.\n   Tekan Enter..."; cin.get(); }
}

void menuUtama() {
    while (true) {
        system("clear");
        showHeader(false);
        cout << "   ====== MENU ======\n";
        for (size_t i = 0; i < daftarResep.size(); i++)
            cout << "   " << (i + 1) << ". " << daftarResep[i].nama << "\n";
        cout << "   " << (daftarResep.size() + 1) << ". Biang Teh\n";
        cout << "   0. Keluar\n\n";

        string pilihan = inputPilihan();

        if (pilihan == RACING) { system("clear"); cout << "\n   [MASUK KE MODE RACING]\n"; modeRacing(); continue; }
        if (pilihan == "0") { system("clear"); cout << "\n   Terima kasih sudah menggunakan GACOAN!\n\n"; return; }
        if (pilihan == std::to_string(daftarResep.size() + 1)) { modeBiangTeh(); continue; }

        int idx = -1;
        for (size_t i = 0; i < daftarResep.size(); i++)
            if (std::to_string(i + 1) == pilihan) { idx = (int)i; break; }

        if (idx != -1) { double jumlah = inputJumlah(); alurSetelahResep(daftarResep[idx], jumlah, false); }
        else { cout << "   [!!] Pilihan tidak valid.\n   Tekan Enter..."; cin.get(); }
    }
}

int main() {
    loadData();
    menuUtama();
    return 0;
}
