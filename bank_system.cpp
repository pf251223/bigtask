#include <algorithm>
#include <cmath>
#include <climits>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <map>
#include <regex>
#include <set>
#include <sstream>
#include <string>
#include <vector>

using namespace std;

namespace {
constexpr double SAVINGS_ANNUAL_RATE = 0.0115;
constexpr double CREDIT_ANNUAL_RATE = 0.0225;
constexpr double CREDIT_NEG_DAILY_RATE = 0.0005;

bool nearlyZero(double x) { return fabs(x) < 0.005; }

enum class AccountType { Savings, Credit };

struct SimpleDate {
    int y = 1970;
    int m = 1;
    int d = 1;
};

bool isLeap(int y) { return (y % 4 == 0 && y % 100 != 0) || (y % 400 == 0); }
int daysInYear(int y) { return isLeap(y) ? 366 : 365; }
int daysInMonth(int y, int m) {
    static const int kDays[] = {0,31,28,31,30,31,30,31,31,30,31,30,31};
    if (m == 2 && isLeap(y)) return 29;
    return kDays[m];
}

long long daysFromCivil(int y, unsigned m, unsigned d) {
    y -= m <= 2;
    const int era = (y >= 0 ? y : y - 399) / 400;
    const unsigned yoe = static_cast<unsigned>(y - era * 400);
    const unsigned doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1;
    const unsigned doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
    return era * 146097 + static_cast<int>(doe) - 719468;
}

int weekdayIndex(const SimpleDate& dt) {
    long long delta = daysFromCivil(dt.y, dt.m, dt.d) - daysFromCivil(1970, 1, 1);
    return static_cast<int>((4 + (delta % 7 + 7) % 7) % 7);
}

string showDateString(const SimpleDate& dt) {
    static const vector<string> kWeek = {"Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday"};
    static const vector<string> kMonth = {"", "January", "February", "March", "April", "May", "June", "July", "August", "September", "October", "November", "December"};
    ostringstream oss;
    oss << kWeek[weekdayIndex(dt)] << ", " << kMonth[dt.m] << " " << dt.d << ", " << dt.y;
    return oss.str();
}

bool validDate(int y, int m, int d) {
    if (y <= 0 || m <= 0 || m > 12 || d <= 0) return false;
    return d <= daysInMonth(y, m);
}

bool isAfter(const SimpleDate& a, const SimpleDate& b) {
    if (a.y != b.y) return a.y > b.y;
    if (a.m != b.m) return a.m > b.m;
    return a.d > b.d;
}

SimpleDate nextDay(SimpleDate x) {
    ++x.d;
    if (x.d > daysInMonth(x.y, x.m)) {
        x.d = 1;
        ++x.m;
        if (x.m > 12) {
            x.m = 1;
            ++x.y;
        }
    }
    return x;
}

struct Account {
    int id = 0;
    AccountType type = AccountType::Savings;
    string name;
    string owner;
    double balance = 0.0;
    double credit = 0.0;
    double pendingInterest = 0.0;
};

class BankSystem {
public:
    BankSystem() { resetInitial(); }

    void run() {
        string line;
        while (getline(cin, line)) {
            if (!line.empty() && line.back() == '\r') line.pop_back();
            if (line.empty()) continue;
            execute(line, false, true);
        }
    }

private:
    map<int, Account> accounts;
    set<string> users;
    vector<string> logs;
    string currentUser;
    SimpleDate currentDate;

    void resetInitial() {
        accounts.clear();
        users.clear();
        logs.clear();
        users.insert("default");
        users.insert("admin");
        currentUser = "default";
        currentDate = {1970, 1, 1};
    }

    bool isAdmin() const { return currentUser == "admin"; }

    void addLogIfNeeded(const string& raw, bool changed, bool allowLog) {
        if (changed && allowLog) logs.push_back(raw);
    }

    void printBool(bool ok, bool silent) {
        if (!silent) cout << (ok ? 1 : 0) << '\n';
    }

    void printAccount(const Account& a) {
        cout << a.id << ' ' << (a.type == AccountType::Savings ? 'S' : 'C') << ' '
             << a.name << ' ' << fixed << setprecision(2) << a.balance;
        if (a.type == AccountType::Credit) cout << ' ' << fixed << setprecision(2) << a.credit;
        cout << '\n';
    }

    void settleOneDayForAll() {
        for (auto& [_, a] : accounts) {
            if (a.type == AccountType::Savings) {
                a.pendingInterest += a.balance * (SAVINGS_ANNUAL_RATE / daysInYear(currentDate.y));
            } else {
                if (a.balance < 0) a.pendingInterest += a.balance * CREDIT_NEG_DAILY_RATE;
                else a.pendingInterest += a.balance * (CREDIT_ANNUAL_RATE / daysInYear(currentDate.y));
            }
        }
    }

    void advanceOneDay() {
        settleOneDayForAll();
        currentDate = nextDay(currentDate);
        if (currentDate.d == 1) {
            for (auto& [_, a] : accounts) {
                a.balance += a.pendingInterest;
                a.pendingInterest = 0.0;
            }
        }
    }

    bool parseDouble(const string& s, double& out) {
        try {
            size_t p = 0;
            out = stod(s, &p);
            return p == s.size();
        } catch (...) {
            return false;
        }
    }

    bool parseInt(const string& s, int& out) {
        try {
            size_t p = 0;
            long long v = stoll(s, &p);
            if (p != s.size() || v < INT_MIN || v > INT_MAX) return false;
            out = static_cast<int>(v);
            return true;
        } catch (...) {
            return false;
        }
    }

    bool canAccessOwned(int id) const {
        auto it = accounts.find(id);
        return it != accounts.end() && it->second.owner == currentUser;
    }

    bool execute(const string& raw, bool silent, bool allowLog) {
        istringstream iss(raw);
        vector<string> tok;
        for (string t; iss >> t;) tok.push_back(t);
        if (tok.empty()) return false;

        bool ok = false, changed = false;
        const string& cmd = tok[0];

        if (cmd == "OPEN" && tok.size() == 5) {
            int id; double amount;
            if (parseInt(tok[1], id) && id > 0 && parseDouble(tok[4], amount) && amount >= 0 && accounts.count(id) == 0 && (tok[2] == "S" || tok[2] == "C")) {
                Account a;
                a.id = id; a.name = tok[3]; a.owner = currentUser;
                if (tok[2] == "S") { a.type = AccountType::Savings; a.balance = amount; }
                else { a.type = AccountType::Credit; a.balance = 0; a.credit = amount; }
                accounts[id] = a;
                ok = changed = true;
            }
            printBool(ok, silent);
        } else if (cmd == "CLOSE" && tok.size() == 2) {
            int id;
            if (parseInt(tok[1], id) && canAccessOwned(id) && nearlyZero(accounts[id].balance)) {
                accounts.erase(id);
                ok = changed = true;
            }
            printBool(ok, silent);
        } else if (cmd == "MODIFY" && tok.size() == 4 && tok[1] == "NAME") {
            int id;
            if (parseInt(tok[2], id) && canAccessOwned(id)) {
                accounts[id].name = tok[3];
                ok = changed = true;
            }
            printBool(ok, silent);
        } else if (cmd == "MODIFY" && tok.size() == 4 && tok[1] == "CREDIT") {
            int id; double c;
            if (parseInt(tok[2], id) && canAccessOwned(id) && parseDouble(tok[3], c) && c >= 0 && accounts[id].type == AccountType::Credit) {
                accounts[id].credit = c;
                ok = changed = true;
            }
            printBool(ok, silent);
        } else if (cmd == "QUERY" && tok.size() == 2) {
            int id;
            if (parseInt(tok[1], id) && canAccessOwned(id)) {
                ok = true;
                if (!silent) printAccount(accounts[id]);
            } else printBool(false, silent);
        } else if (cmd == "QUERYALL" && tok.size() == 1) {
            vector<int> ids;
            for (const auto& [id, a] : accounts) if (a.owner == currentUser) ids.push_back(id);
            if (ids.empty()) printBool(false, silent);
            else {
                ok = true;
                if (!silent) for (int id : ids) printAccount(accounts[id]);
            }
        } else if (cmd == "DEPOSIT" && tok.size() == 3) {
            int id; double amt;
            if (parseInt(tok[1], id) && canAccessOwned(id) && parseDouble(tok[2], amt) && amt >= 0) {
                accounts[id].balance += amt;
                ok = changed = true;
            }
            printBool(ok, silent);
        } else if (cmd == "WITHDRAW" && tok.size() == 3) {
            int id; double amt;
            if (parseInt(tok[1], id) && canAccessOwned(id) && parseDouble(tok[2], amt) && amt >= 0) {
                auto &a = accounts[id];
                bool enough = (a.type == AccountType::Savings) ? (a.balance + 1e-9 >= amt) : (a.balance + a.credit + 1e-9 >= amt);
                if (enough) {
                    a.balance -= amt;
                    ok = changed = true;
                }
            }
            printBool(ok, silent);
        } else if (cmd == "TRANSFER" && tok.size() == 4) {
            int s, t; double amt;
            if (parseInt(tok[1], s) && parseInt(tok[2], t) && parseDouble(tok[3], amt) && amt >= 0 && s != t && canAccessOwned(s) && accounts.count(t)) {
                auto &src = accounts[s]; auto &dst = accounts[t];
                bool enough = (src.type == AccountType::Savings) ? (src.balance + 1e-9 >= amt) : (src.balance + src.credit + 1e-9 >= amt);
                if (enough) {
                    src.balance -= amt;
                    dst.balance += amt;
                    ok = changed = true;
                }
            }
            printBool(ok, silent);
        } else if (cmd == "SHOW_DATE" && tok.size() == 1) {
            ok = true;
            if (!silent) cout << showDateString(currentDate) << '\n';
        } else if (cmd == "ADD_DAY" && tok.size() == 2) {
            int n;
            if (parseInt(tok[1], n) && n > 0) {
                for (int i = 0; i < n; ++i) advanceOneDay();
                ok = changed = true;
            }
            printBool(ok, silent);
        } else if (cmd == "SET_DATE" && tok.size() == 4) {
            int y, m, d;
            if (parseInt(tok[1], y) && parseInt(tok[2], m) && parseInt(tok[3], d) && validDate(y, m, d)) {
                SimpleDate nd{y,m,d};
                if (isAfter(nd, currentDate)) {
                    while (isAfter(nd, currentDate)) advanceOneDay();
                    ok = changed = true;
                }
            }
            printBool(ok, silent);
        } else if (cmd == "LOG" && tok.size() == 1) {
            ok = true;
            if (!silent) {
                if (logs.empty()) cout << 0 << '\n';
                else {
                    for (size_t i = 0; i < logs.size(); ++i) cout << (i + 1) << ' ' << logs[i] << '\n';
                }
            }
        } else if (cmd == "ROLLBACK" && tok.size() == 2) {
            int id;
            if (parseInt(tok[1], id) && id >= 0 && id <= static_cast<int>(logs.size())) {
                vector<string> keep(logs.begin(), logs.begin() + id);
                resetInitial();
                for (const auto& c : keep) execute(c, true, true);
                ok = true;
                changed = false;
            }
            printBool(ok, silent);
        } else if (cmd == "SAVE" && tok.size() == 2) {
            ofstream ofs(tok[1]);
            if (ofs) {
                for (size_t i = 0; i < logs.size(); ++i) ofs << (i + 1) << ' ' << logs[i] << '\n';
                ok = true;
            }
            printBool(ok, silent);
        } else if (cmd == "RESUME" && tok.size() == 2) {
            if (logs.empty()) {
                ifstream ifs(tok[1]);
                vector<string> loaded;
                ok = static_cast<bool>(ifs);
                if (ok) {
                    string line;
                    int expected = 1;
                    while (getline(ifs, line)) {
                        if (!line.empty() && line.back() == '\r') line.pop_back();
                        if (line.empty()) continue;
                        istringstream lis(line);
                        int idx;
                        if (!(lis >> idx) || idx != expected) { ok = false; break; }
                        string rest;
                        getline(lis, rest);
                        if (!rest.empty() && rest[0] == ' ') rest.erase(rest.begin());
                        if (rest.empty()) { ok = false; break; }
                        loaded.push_back(rest);
                        ++expected;
                    }
                }
                if (ok) {
                    resetInitial();
                    for (const auto& c : loaded) {
                        if (!execute(c, true, true)) { ok = false; break; }
                    }
                    if (!ok) resetInitial();
                }
            }
            printBool(ok, silent);
        } else if (cmd == "CREATE_USER" && tok.size() == 2) {
            static const regex re("^[A-Za-z0-9]+$");
            if (isAdmin() && regex_match(tok[1], re) && !users.count(tok[1])) {
                users.insert(tok[1]);
                ok = changed = true;
            }
            printBool(ok, silent);
        } else if (cmd == "DELETE_USER" && tok.size() == 2) {
            const string& u = tok[1];
            if (isAdmin() && u != "admin" && users.count(u)) {
                bool hasAcc = false;
                for (const auto& [_, a] : accounts) if (a.owner == u) { hasAcc = true; break; }
                if (!hasAcc) {
                    users.erase(u);
                    if (currentUser == u) currentUser = "default";
                    ok = changed = true;
                }
            }
            printBool(ok, silent);
        } else if (cmd == "QUERY_USER" && tok.size() == 2) {
            const string& u = tok[1];
            if (isAdmin() && users.count(u)) {
                vector<int> ids;
                for (const auto& [id, a] : accounts) if (a.owner == u) ids.push_back(id);
                if (!ids.empty()) {
                    ok = true;
                    if (!silent) for (int id : ids) printAccount(accounts[id]);
                }
            }
            if (!ok) printBool(false, silent);
        } else if (cmd == "QUERY_USERLIST" && tok.size() == 1) {
            if (!users.empty()) {
                ok = true;
                if (!silent) {
                    map<string,int> cnt;
                    for (const auto& u : users) cnt[u] = 0;
                    for (const auto& [_, a] : accounts) cnt[a.owner]++;
                    for (const auto& [u, c] : cnt) cout << "USER " << u << ' ' << c << '\n';
                }
            }
            if (!ok) printBool(false, silent);
        } else if (cmd == "SWITCH" && tok.size() == 2) {
            if (users.count(tok[1])) {
                currentUser = tok[1];
                ok = changed = true;
            }
            printBool(ok, silent);
        } else if (cmd == "WHOAMI" && tok.size() == 1) {
            if (!currentUser.empty()) {
                ok = true;
                if (!silent) cout << currentUser << '\n';
            }
            if (!ok) printBool(false, silent);
        } else {
            printBool(false, silent);
        }

        addLogIfNeeded(raw, changed, allowLog);
        return ok;
    }
};
}  // namespace

int main() {
    ios::sync_with_stdio(false);
    cin.tie(nullptr);

    BankSystem sys;
    sys.run();
    return 0;
}
