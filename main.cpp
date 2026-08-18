#include <iostream>
#include <iomanip>
#include <string>
#include <limits>
#include <cstdlib>

using namespace std;

// ============================================================
//  Token / Pilihan menu
// ============================================================
const int M_MILO      = 1;
const int M_THAI      = 2;
const int M_LEMON     = 3;
const int M_ORANGE    = 4;
const string RACING   = "00";

// ============================================================
//  ASCII Art Header "GACOAN" (gaya miring/serong, ukuran 1-2 baris)
//  Lebar sekitar ~40 kolom → tetap muat rapi di layar mobile.
//  Dikopikan dari header.txt. Memakai raw string agar backslash aman.
// ============================================================
void showHeader(bool racingMode = false) {
    cout << R"(
   ____    _    ____ ___    _    _   _ 
  / ___|  / \  / ___/ _ \  / \  | \ | |
 | |  _  / _ \| |  | | | |/ _ \ |  \| |
 | |_| |/ ___ \ |__| |_| / ___ \| |\  |
  \____/_/   \_\____\___/_/   \_\_| \_|
)";
    if (racingMode) {
        cout << "   =====  MODE RACING  =====\n";
    }
    cout << "\n";
}

// ============================================================
//  Bahan dasar resep (per 1 resep = 20 porsi)
// ============================================================
struct Resep {
    string nama;
    bool  bisaRacing;          // apakah mendukung mode racing
    double powder;             // gram
    double airPanas;           // gram
    double airSuhuRuang;       // gram
    double creamer;            // gram
    double skm;                // gram
    double susuEvaporasi;      // gram
    double simpleSyrup;        // gram
};

Resep daftarResep[] = {
    {"MILO",          true,  1200, 2000, 570, 0,   0,   0,   260},
    {"THAI GREEN TEA",false, 150,  2000, 480, 220, 660, 660, 560},
    {"LEMON TEA",     true,  600,  2000, 1400,0,   0,   0,   0  },
    {"ORANGE",        true,  380,  2000, 1620,0,   0,   0,   0  }
};

const int JUMLAH_RESE = 4;

// ============================================================
//  Resep BIANG TEH — struktur berbeda dari resep lain.
//  Komposisi berbasis PERSENTASE dari total berat:
//    Base Teh 45,5% | Air 24,2% | Simple Syrup 30,3%
//  Berat satu porsi = 200 gr (dari acuan 20 porsi = 4000 gr).
//
//  Tabel porsi acuan (angka presisi dari data tabel):
//    20 porsi -> 4000 gr    : Base 1818 | Air 970  | Simple 1212
//    30 porsi -> 5999,4 gr  : Base 2727 | Air 1454 | Simple 1818
//    33 porsi -> 6600 gr    : Base 3000 | Air 1600 | Simple 2000
//  Porsi selain 20/30/33 memakai acuan 20 porsi (proporsional).
// ============================================================
const double BIANG_PCT_BASE    = 0.455;   // 45,5%
const double BIANG_PCT_AIR     = 0.242;   // 24,2%
const double BIANG_PCT_SIMPLE  = 0.303;   // 30,3%
const double BIANG_GRAM_PER_PORSI = 200.0;

// Data presisi untuk tiap porsi acuan: porsi, berat total, base, air, simple
struct BiangPorsi {
    double porsi;
    double base;
    double air;
    double simple;
};
const BiangPorsi biangPorsiAcuan[] = {
    {20, 1818, 970, 1212},
    {30, 2727, 1454.4, 1818},
    {33, 3000, 1600, 2000},
};
const int JUMLAH_BIANG_ACUAN = 3;

// ============================================================
//  Tampilkan satu baris bahan (dengan format gram)
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

    // 1 resep = 20 porsi, pengali langsung = jumlah resep

    // Hitung total air (untuk mode racing)
    double totalAir = (r.airPanas + r.airSuhuRuang) * pengali;

    cout << "\n=========================================\n";
    cout << "        RESEP " << r.nama;
    if (racingMode) cout << "  (MODE RACING)";
    cout << "\n=========================================\n";
    cout << "   Jumlah resep : " << setprecision(4) << jumlah << " resep\n";
    cout << "   ( " << setprecision(4) << (jumlah * 20.0) << " porsi )\n";
    cout << "-----------------------------------------\n";

    // Tampilkan semua bahan. Pada mode racing, air panas + air suhu ruang
    // digabung menjadi satu "Air suhu ruang".
    tampilkanBaris(r.nama == "MILO" ? "Powder Milo"
                 : r.nama == "THAI GREEN TEA" ? "Powder Thai Green Tea"
                 : r.nama == "LEMON TEA" ? "Powder Lemon Tea"
                 : "Powder Orange",
                 r.powder * pengali);
    if (r.creamer > 0)          tampilkanBaris("Creamer bubuk", r.creamer * pengali);
    if (r.skm > 0)              tampilkanBaris("SKM", r.skm * pengali);
    if (r.susuEvaporasi > 0)    tampilkanBaris("Susu evaporasi", r.susuEvaporasi * pengali);
    if (r.simpleSyrup > 0)      tampilkanBaris("Simple syrup", r.simpleSyrup * pengali);

    if (racingMode) {
        // Mode racing: gabung air panas + air suhu ruang -> hanya air suhu ruang
        tampilkanBaris("Air suhu ruang", totalAir);
    } else {
        tampilkanBaris("Air panas", r.airPanas * pengali);
        if (r.airSuhuRuang > 0) tampilkanBaris("Air suhu ruang", r.airSuhuRuang * pengali);
    }
    cout << "-----------------------------------------\n";
}

// ============================================================
//  Ambil input angka positif (desimal) dengan validasi
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

// ============================================================
//  Ambil jumlah resep (desimal) — wrapper agar pesan tetap sama
// ============================================================
double inputJumlah() {
    return inputAngkaPositif("Masukkan jumlah resep : ");
}

// ============================================================
//  Ambil pilihan menu (angka atau "00")
// ============================================================
string inputPilihan() {
    string input;
    cout << "   Pilih menu: ";
    cin >> input;
    cin.ignore(numeric_limits<streamsize>::max(), '\n');
    return input;
}

// ============================================================
//  Alur setelah resep ditampilkan (hitung lagi / menu / keluar)
// ============================================================
void alurSetelahResep(const Resep &r, double jumlah, bool racingMode) {
    // Layar clear lalu tampilkan resep
    system("clear");
    showHeader(racingMode);
    tampilkanResep(r, jumlah, racingMode);

    while (true) {
        cout << "\n";
        cout << "   ====== PILIHAN ======\n";
        cout << "   1. Hitung lagi\n";
        cout << "   2. Menu utama\n";
        cout << "   3. Keluar program\n";
        cout << "   Pilih: ";
        int pilihan;
        cin >> pilihan;
        cin.ignore(numeric_limits<streamsize>::max(), '\n');

        if (pilihan == 1) {
            system("clear");
            showHeader(racingMode);
            cout << "   Resep " << r.nama << ":\n";
            double lagi = inputJumlah();
            system("clear");
            showHeader(racingMode);
            tampilkanResep(r, lagi, racingMode);
            jumlah = lagi;   // lanjutkan loop dengan jumlah baru
        } else if (pilihan == 2) {
            return;          // kembali ke menu utama
        } else if (pilihan == 3) {
            system("clear");
            cout << "\n   Terima kasih sudah menggunakan GACOAN!\n\n";
            exit(0);
        } else {
            cout << "   [!!] Pilihan tidak valid.\n";
        }
    }
}

// ============================================================
//  Mode RACING (rahasia)
// ============================================================
void modeRacing() {
    while (true) {
        system("clear");
        showHeader(true);

        // Kumpulkan indeks resep yang mendukung mode racing
        // (secara tampilan diurutkan 1..n, bebas urutan asli array)
        int urutan[JUMLAH_RESE];
        int jumlahRacing = 0;
        for (int i = 0; i < JUMLAH_RESE; i++) {
            if (daftarResep[i].bisaRacing) urutan[jumlahRacing++] = i;
        }

        cout << "   Resep yang tersedia pada mode racing:\n";
        for (int i = 0; i < jumlahRacing; i++) {
            cout << "   " << (i + 1) << ". " << daftarResep[urutan[i]].nama << "\n";
        }
        cout << "   0. Kembali ke menu utama\n";
        cout << "\n";

        string pilihan = inputPilihan();

        if (pilihan == "0") {
            return;   // kembali ke menu utama
                }

        // Cari pilihan di daftar racing
        int indeks = -1;
        for (int i = 0; i < jumlahRacing; i++) {
            if (std::to_string(i + 1) == pilihan) {
                indeks = urutan[i];
                break;
            }
        }

        if (indeks != -1) {
            double jumlah = inputJumlah();
            alurSetelahResep(daftarResep[indeks], jumlah, true);
        } else {
            cout << "   [!!] Pilihan tidak valid.\n";
            cout << "   Tekan Enter untuk lanjut...";
            cin.get();
        }
    }
}

// ============================================================
//  Tampilkan hasil resep BIANG TEH
// ============================================================
void tampilkanResepBiangTeh(double baseTeh, double air, double simple,
                            double totalBerat, double porsiPerkiraan) {
    cout << "\n=========================================\n";
    cout << "        RESEP BIANG TEH\n";
    cout << "=========================================\n";
    cout << "   ( ± " << setprecision(4) << porsiPerkiraan << " porsi )\n";
    cout << "   ( "  << setprecision(4) << totalBerat << " gr total )\n";
    cout << "-----------------------------------------\n";
    tampilkanBaris("Base Teh",       baseTeh);
    tampilkanBaris("Air",            air);
    tampilkanBaris("Simple syrup",   simple);
    cout << "-----------------------------------------\n";
}

// ============================================================
//  Sub-menu BIANG TEH — 2 opsi hitung + kembali
//  (forward declaration agar bisa dipanggil dari alurSetelahBiangTeh)
// ============================================================
bool hitungBiangTeh(unsigned int &opsi, double &base, double &air,
                    double &simple, double &total, double &porsi);

void alurSetelahBiangTeh(double baseTeh, double air, double simple,
                         double totalBerat, double porsiPerkiraan) {
    system("clear");
    showHeader(false);
    tampilkanResepBiangTeh(baseTeh, air, simple, totalBerat, porsiPerkiraan);

    while (true) {
        cout << "\n";
        cout << "   ====== PILIHAN ======\n";
        cout << "   1. Hitung lagi\n";
        cout << "   2. Menu utama\n";
        cout << "   3. Keluar program\n";
        cout << "   Pilih: ";
        int pilihan;
        cin >> pilihan;
        cin.ignore(numeric_limits<streamsize>::max(), '\n');

        if (pilihan == 1) {
            // Ulangi proses hitung biang teh
            system("clear");
            showHeader(false);
            unsigned int opsi = 0;
            double b, a, s, tot, por;
            if (hitungBiangTeh(opsi, b, a, s, tot, por)) {
                system("clear");
                showHeader(false);
                tampilkanResepBiangTeh(b, a, s, tot, por);
                // update variabel lokal
                baseTeh = b; air = a; simple = s;
                totalBerat = tot; porsiPerkiraan = por;
            }
        } else if (pilihan == 2) {
            return;          // kembali ke menu utama
        } else if (pilihan == 3) {
            system("clear");
            cout << "\n   Terima kasih sudah menggunakan GACOAN!\n\n";
            exit(0);
        } else {
            cout << "   [!!] Pilihan tidak valid.\n";
        }
    }
}

// ============================================================
//  Mode BIANG TEH (menu utama ke-5)
//  - opsi: hitung berdasarkan acuan satu bahan, atau berdasarkan porsi
// ============================================================
bool hitungBiangTeh(unsigned int &opsi, double &base, double &air,
                    double &simple, double &total, double &porsi) {
    system("clear");
    showHeader(false);

    cout << "   ====== BIANG TEH ======\n";
    cout << "   Komposisi: Base Teh 45,5% | Air 24,2% | Simple Syrup 30,3%\n";
    cout << "\n";
    cout << "   1. Hitung berdasarkan acuan satu bahan\n";
    cout << "   2. Hitung berdasarkan porsi\n";
    cout << "   Pilih: ";
    int pilih;
    cin >> pilih;
    cin.ignore(numeric_limits<streamsize>::max(), '\n');

    if (pilih != 1 && pilih != 2) {
        return false;
    }

    if (pilih == 1) {
        // ---- Hitung berdasarkan acuan satu bahan ----
        cout << "\n   Bahan acuan:\n";
        cout << "   1. Base Teh\n";
        cout << "   2. Air\n";
        cout << "   3. Simple Syrup\n";
        int bahan = (int)inputAngkaPositif("Pilih bahan acuan (1-3) : ");
        while (bahan < 1 || bahan > 3) {
            cout << "   [!!] Pilih 1-3.\n";
            bahan = (int)inputAngkaPositif("Pilih bahan acuan (1-3) : ");
        }

        double beratBahan = inputAngkaPositif("Masukkan berat bahan acuan (gr) : ");

        // Cek apakah berat input PERSIS sama dengan nilai kolom bahan
        // yang dipilih pada salah satu porsi acuan (20/30/33).
        // Jika cocok -> pakai seluruh nilai baris tersebut secara exact.
        bool pakaiExact = false;
        for (int i = 0; i < JUMLAH_BIANG_ACUAN; i++) {
            double nilaiKolom;
            switch (bahan) {
                case 1: nilaiKolom = biangPorsiAcuan[i].base;   break;
                case 2: nilaiKolom = biangPorsiAcuan[i].air;    break;
                default: nilaiKolom = biangPorsiAcuan[i].simple; break;
            }
            if (beratBahan == nilaiKolom) {
                base   = biangPorsiAcuan[i].base;
                air    = biangPorsiAcuan[i].air;
                simple = biangPorsiAcuan[i].simple;
                total  = base + air + simple;
                porsi  = biangPorsiAcuan[i].porsi;
                pakaiExact = true;
                break;   // prioritas urutan 20 -> 30 -> 33
            }
        }

        if (!pakaiExact) {
            // Tidak cocok dengan porsi acuan -> hitung persentase proporsional
            double persenBahan;
            switch (bahan) {
                case 1: persenBahan = BIANG_PCT_BASE;   break;
                case 2: persenBahan = BIANG_PCT_AIR;    break;
                default:persenBahan = BIANG_PCT_SIMPLE; break;
            }

            total = beratBahan / persenBahan;
            base   = total * BIANG_PCT_BASE;
            air    = total * BIANG_PCT_AIR;
            simple = total * BIANG_PCT_SIMPLE;
            porsi  = total / BIANG_GRAM_PER_PORSI;
        }

    } else {
        // ---- Hitung berdasarkan porsi ----
        double inporsi = inputAngkaPositif("Masukkan jumlah porsi : ");

        // Cek porsi acuan 20/30/33
        bool pakaiAcuan = false;
        for (int i = 0; i < JUMLAH_BIANG_ACUAN; i++) {
            if (inporsi == biangPorsiAcuan[i].porsi) {
                base   = biangPorsiAcuan[i].base;
                air    = biangPorsiAcuan[i].air;
                simple = biangPorsiAcuan[i].simple;
                total  = base + air + simple;
                porsi  = inporsi;
                pakaiAcuan = true;
                break;
            }
        }

        if (!pakaiAcuan) {
            // Acuan 33 porsi (total 6600 gr; bahan 3000/1600/2000),
            // proporsional untuk porsi lainnya
            double skala = inporsi / 33.0;
            base   = 3000.0 * skala;
            air    = 1600.0 * skala;
            simple = 2000.0 * skala;
            total  = 6600.0 * skala;
            porsi  = inporsi;
        }
    }

    opsi = pilih;
    return true;
}

void modeBiangTeh() {
    unsigned int opsi;
    double base, air, simple, total, porsi;
    if (hitungBiangTeh(opsi, base, air, simple, total, porsi)) {
        alurSetelahBiangTeh(base, air, simple, total, porsi);
    } else {
        cout << "   [!!] Pilihan tidak valid.\n";
        cout << "   Tekan Enter untuk lanjut...";
        cin.get();
    }
}

// ============================================================
//  MENU UTAMA
// ============================================================
void menuUtama() {
    while (true) {
        system("clear");
        showHeader(false);

        cout << "   ====== MENU ======\n";
        cout << "   1. Milo\n";
        cout << "   2. Thai Green Tea\n";
        cout << "   3. Lemon Tea\n";
        cout << "   4. Orange\n";
        cout << "   5. Biang Teh\n";
        cout << "   0. Keluar\n";
        cout << "\n";

        string pilihan = inputPilihan();

        // Mode racing rahasia: "00"
        if (pilihan == RACING) {
            system("clear");
            cout << "\n   [MASUK KE MODE RACING]\n";
            // tampilkan sekilas lalu masuk mode racing
            modeRacing();
            continue;
        }

        if (pilihan == "0") {
            system("clear");
            cout << "\n   Terima kasih sudah menggunakan GACOAN!\n\n";
            return;
        }

        // Menu 5 = Biang Teh (sub-menu khusus)
        if (pilihan == "5") {
            modeBiangTeh();
            continue;
        }

        int idx = -1;
        for (int i = 0; i < JUMLAH_RESE; i++) {
            if (std::to_string(i + 1) == pilihan) {
                idx = i;
                break;
            }
        }

        if (idx != -1) {
            double jumlah = inputJumlah();
            alurSetelahResep(daftarResep[idx], jumlah, false);
        } else {
            cout << "   [!!] Pilihan tidak valid.\n";
            cout << "   Tekan Enter untuk lanjut...";
            cin.get();
        }
    }
}

// ============================================================
//  MAIN
// ============================================================
int main() {
    menuUtama();
    return 0;
}
