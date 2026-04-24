#include "bank_system.h"

#include <climits>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <regex>
#include <sstream>
#include <utility>

namespace bank {

namespace {
bool nearlyZero(double v) { return std::fabs(v) < 0.005; }
}

Date::Date(int year, int month, int day) : y_(year), m_(month), d_(day) {}

int Date::year() const { return y_; }
int Date::month() const { return m_; }
int Date::day() const { return d_; }

bool Date::isLeapYear(int year) {
    return (year % 4 == 0 && year % 100 != 0) || (year % 400 == 0);
}

int Date::daysInMonth(int year, int month) {
    static const int kDays[] = {0, 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    if (month == 2 && isLeapYear(year)) return 29;
    return kDays[month];
}

bool Date::isValid(int year, int month, int day) {
    if (year <= 0 || month <= 0 || month > 12 || day <= 0) return false;
    return day <= daysInMonth(year, month);
}

long long Date::daysFromCivil(int y, unsigned m, unsigned d) {
    y -= m <= 2;
    const int era = (y >= 0 ? y : y - 399) / 400;
    const unsigned yoe = static_cast<unsigned>(y - era * 400);
    const unsigned doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1;
    const unsigned doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
    return era * 146097 + static_cast<int>(doe) - 719468;
}

int Date::weekdayIndex() const {
    long long delta = daysFromCivil(y_, m_, d_) - daysFromCivil(1970, 1, 1);
    return static_cast<int>((4 + (delta % 7 + 7) % 7) % 7);
}

std::string Date::toDisplayString() const {
    static const std::vector<std::string> kWeek = {"Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday"};
    static const std::vector<std::string> kMonth = {"", "January", "February", "March", "April", "May", "June", "July", "August", "September", "October", "November", "December"};
    std::ostringstream oss;
    oss << kWeek[weekdayIndex()] << ", " << kMonth[m_] << " " << d_ << ", " << y_;
    return oss.str();
}

bool Date::isAfter(const Date& rhs) const {
    if (y_ != rhs.y_) return y_ > rhs.y_;
    if (m_ != rhs.m_) return m_ > rhs.m_;
    return d_ > rhs.d_;
}

void Date::advanceOneDay() {
    ++d_;
    if (d_ > daysInMonth(y_, m_)) {
        d_ = 1;
        ++m_;
        if (m_ > 12) {
            m_ = 1;
            ++y_;
        }
    }
}

int Date::daysInCurrentYear() const { return isLeapYear(y_) ? 366 : 365; }

Account::Account(int id, AccountType type, std::string name, std::string owner, double initialAmountOrCredit)
    : id_(id), type_(type), name_(std::move(name)), owner_(std::move(owner)) {
    if (type_ == AccountType::Savings) {
        balance_ = initialAmountOrCredit;
    } else {
        credit_ = initialAmountOrCredit;
    }
}

int Account::id() const { return id_; }
AccountType Account::type() const { return type_; }
const std::string& Account::name() const { return name_; }
const std::string& Account::owner() const { return owner_; }
double Account::balance() const { return balance_; }
double Account::credit() const { return credit_; }

void Account::setName(const std::string& newName) { name_ = newName; }

bool Account::setCredit(double newCredit) {
    if (type_ != AccountType::Credit || newCredit < 0) return false;
    credit_ = newCredit;
    return true;
}

void Account::deposit(double amount) { balance_ += amount; }

bool Account::canWithdraw(double amount) const {
    if (amount < 0) return false;
    if (type_ == AccountType::Savings) return balance_ + 1e-9 >= amount;
    return balance_ + credit_ + 1e-9 >= amount;
}

bool Account::withdraw(double amount) {
    if (!canWithdraw(amount)) return false;
    balance_ -= amount;
    return true;
}

void Account::accrueDailyInterest(const Date& date) {
    if (type_ == AccountType::Savings) {
        pendingInterest_ += balance_ * (kSavingsAnnualRate / date.daysInCurrentYear());
    } else {
        if (balance_ < 0)
            pendingInterest_ += balance_ * kCreditNegativeDailyRate;
        else
            pendingInterest_ += balance_ * (kCreditAnnualRate / date.daysInCurrentYear());
    }
}

void Account::postMonthlyInterestIfNeeded(const Date& date) {
    if (date.day() != 1) return;
    balance_ += pendingInterest_;
    pendingInterest_ = 0;
}

bool Account::canClose() const { return nearlyZero(balance_); }

void Account::print(std::ostream& out) const {
    out << id_ << ' ' << (type_ == AccountType::Savings ? 'S' : 'C') << ' ' << name_ << ' ' << std::fixed
        << std::setprecision(2) << balance_;
    if (type_ == AccountType::Credit) out << ' ' << std::fixed << std::setprecision(2) << credit_;
    out << '\n';
}

BankSystem::BankSystem() { resetInitial(); }

void BankSystem::run() {
    std::string line;
    while (std::getline(std::cin, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (line.empty()) continue;
        execute(line, false, true);
    }
}

void BankSystem::resetInitial() {
    accounts_.clear();
    users_.clear();
    logs_.clear();
    users_.insert("default");
    users_.insert("admin");
    currentUser_ = "default";
    currentDate_ = Date(1970, 1, 1);
}

bool BankSystem::isAdmin() const { return currentUser_ == "admin"; }

bool BankSystem::canAccessOwnedAccount(int id) const {
    auto it = accounts_.find(id);
    return it != accounts_.end() && it->second.owner() == currentUser_;
}

bool BankSystem::parseInt(const std::string& s, int& out) {
    try {
        size_t p = 0;
        long long v = std::stoll(s, &p);
        if (p != s.size() || v < INT_MIN || v > INT_MAX) return false;
        out = static_cast<int>(v);
        return true;
    } catch (...) {
        return false;
    }
}

bool BankSystem::parseDouble(const std::string& s, double& out) {
    try {
        size_t p = 0;
        out = std::stod(s, &p);
        return p == s.size();
    } catch (...) {
        return false;
    }
}

bool BankSystem::isValidUsername(const std::string& username) {
    static const std::regex kRe("^[A-Za-z0-9]+$");
    return std::regex_match(username, kRe);
}

void BankSystem::outputBool(bool ok, bool silent) const {
    if (!silent) std::cout << (ok ? 1 : 0) << '\n';
}

void BankSystem::outputAccount(const Account& acc, bool silent) const {
    if (!silent) acc.print(std::cout);
}

void BankSystem::appendLogIfNeeded(const std::string& raw, bool changed, bool allowLog) {
    if (changed && allowLog) logs_.push_back(raw);
}

void BankSystem::settleOneDay() {
    for (auto& [_, acc] : accounts_) acc.accrueDailyInterest(currentDate_);
    currentDate_.advanceOneDay();
    for (auto& [_, acc] : accounts_) acc.postMonthlyInterestIfNeeded(currentDate_);
}

bool BankSystem::execute(const std::string& raw, bool silent, bool allowLog) {
    std::istringstream iss(raw);
    std::vector<std::string> tok;
    for (std::string t; iss >> t;) tok.push_back(t);
    if (tok.empty()) return false;

    bool ok = false;
    bool changed = false;
    const std::string& cmd = tok[0];

    if (cmd == "OPEN" && tok.size() == 5) {
        int id;
        double amount;
        if (parseInt(tok[1], id) && id > 0 && parseDouble(tok[4], amount) && amount >= 0 && !accounts_.count(id) &&
            (tok[2] == "S" || tok[2] == "C")) {
            AccountType type = tok[2] == "S" ? AccountType::Savings : AccountType::Credit;
            accounts_.emplace(id, Account(id, type, tok[3], currentUser_, amount));
            ok = changed = true;
        }
        outputBool(ok, silent);
    } else if (cmd == "CLOSE" && tok.size() == 2) {
        int id;
        if (parseInt(tok[1], id) && canAccessOwnedAccount(id) && accounts_[id].canClose()) {
            accounts_.erase(id);
            ok = changed = true;
        }
        outputBool(ok, silent);
    } else if (cmd == "MODIFY" && tok.size() == 4 && tok[1] == "NAME") {
        int id;
        if (parseInt(tok[2], id) && canAccessOwnedAccount(id)) {
            accounts_[id].setName(tok[3]);
            ok = changed = true;
        }
        outputBool(ok, silent);
    } else if (cmd == "MODIFY" && tok.size() == 4 && tok[1] == "CREDIT") {
        int id;
        double credit;
        if (parseInt(tok[2], id) && canAccessOwnedAccount(id) && parseDouble(tok[3], credit) && accounts_[id].setCredit(credit)) {
            ok = changed = true;
        }
        outputBool(ok, silent);
    } else if (cmd == "QUERY" && tok.size() == 2) {
        int id;
        if (parseInt(tok[1], id) && canAccessOwnedAccount(id)) {
            ok = true;
            outputAccount(accounts_[id], silent);
        } else {
            outputBool(false, silent);
        }
    } else if (cmd == "QUERYALL" && tok.size() == 1) {
        std::vector<int> ids;
        for (const auto& [id, acc] : accounts_) {
            if (acc.owner() == currentUser_) ids.push_back(id);
        }
        if (ids.empty()) {
            outputBool(false, silent);
        } else {
            ok = true;
            if (!silent) {
                for (int id : ids) outputAccount(accounts_[id], false);
            }
        }
    } else if (cmd == "DEPOSIT" && tok.size() == 3) {
        int id;
        double amount;
        if (parseInt(tok[1], id) && canAccessOwnedAccount(id) && parseDouble(tok[2], amount) && amount >= 0) {
            accounts_[id].deposit(amount);
            ok = changed = true;
        }
        outputBool(ok, silent);
    } else if (cmd == "WITHDRAW" && tok.size() == 3) {
        int id;
        double amount;
        if (parseInt(tok[1], id) && canAccessOwnedAccount(id) && parseDouble(tok[2], amount) && accounts_[id].withdraw(amount)) {
            ok = changed = true;
        }
        outputBool(ok, silent);
    } else if (cmd == "TRANSFER" && tok.size() == 4) {
        int srcId, dstId;
        double amount;
        if (parseInt(tok[1], srcId) && parseInt(tok[2], dstId) && parseDouble(tok[3], amount) && amount >= 0 && srcId != dstId &&
            canAccessOwnedAccount(srcId) && accounts_.count(dstId) && accounts_[srcId].canWithdraw(amount)) {
            accounts_[srcId].withdraw(amount);
            accounts_[dstId].deposit(amount);
            ok = changed = true;
        }
        outputBool(ok, silent);
    } else if (cmd == "SHOW_DATE" && tok.size() == 1) {
        ok = true;
        if (!silent) std::cout << currentDate_.toDisplayString() << '\n';
    } else if (cmd == "ADD_DAY" && tok.size() == 2) {
        int days;
        if (parseInt(tok[1], days) && days > 0) {
            for (int i = 0; i < days; ++i) settleOneDay();
            ok = changed = true;
        }
        outputBool(ok, silent);
    } else if (cmd == "SET_DATE" && tok.size() == 4) {
        int y, m, d;
        if (parseInt(tok[1], y) && parseInt(tok[2], m) && parseInt(tok[3], d) && Date::isValid(y, m, d)) {
            Date newDate(y, m, d);
            if (newDate.isAfter(currentDate_)) {
                while (newDate.isAfter(currentDate_)) settleOneDay();
                ok = changed = true;
            }
        }
        outputBool(ok, silent);
    } else if (cmd == "LOG" && tok.size() == 1) {
        ok = true;
        if (!silent) {
            if (logs_.empty()) {
                std::cout << 0 << '\n';
            } else {
                for (size_t i = 0; i < logs_.size(); ++i) std::cout << (i + 1) << ' ' << logs_[i] << '\n';
            }
        }
    } else if (cmd == "ROLLBACK" && tok.size() == 2) {
        int id;
        if (parseInt(tok[1], id) && id >= 0 && id <= static_cast<int>(logs_.size())) {
            std::vector<std::string> keep(logs_.begin(), logs_.begin() + id);
            resetInitial();
            for (const auto& c : keep) execute(c, true, true);
            ok = true;
        }
        outputBool(ok, silent);
    } else if (cmd == "SAVE" && tok.size() == 2) {
        std::ofstream ofs(tok[1]);
        if (ofs) {
            for (size_t i = 0; i < logs_.size(); ++i) ofs << (i + 1) << ' ' << logs_[i] << '\n';
            ok = true;
        }
        outputBool(ok, silent);
    } else if (cmd == "RESUME" && tok.size() == 2) {
        if (logs_.empty()) {
            std::ifstream ifs(tok[1]);
            std::vector<std::string> loaded;
            ok = static_cast<bool>(ifs);
            if (ok) {
                std::string line;
                int expected = 1;
                while (std::getline(ifs, line)) {
                    if (!line.empty() && line.back() == '\r') line.pop_back();
                    if (line.empty()) continue;
                    std::istringstream lis(line);
                    int idx;
                    if (!(lis >> idx) || idx != expected) {
                        ok = false;
                        break;
                    }
                    std::string rest;
                    std::getline(lis, rest);
                    if (!rest.empty() && rest[0] == ' ') rest.erase(rest.begin());
                    if (rest.empty()) {
                        ok = false;
                        break;
                    }
                    loaded.push_back(rest);
                    ++expected;
                }
            }
            if (ok) {
                resetInitial();
                for (const auto& c : loaded) {
                    if (!execute(c, true, true)) {
                        ok = false;
                        break;
                    }
                }
                if (!ok) resetInitial();
            }
        }
        outputBool(ok, silent);
    } else if (cmd == "CREATE_USER" && tok.size() == 2) {
        if (isAdmin() && isValidUsername(tok[1]) && !users_.count(tok[1])) {
            users_.insert(tok[1]);
            ok = changed = true;
        }
        outputBool(ok, silent);
    } else if (cmd == "DELETE_USER" && tok.size() == 2) {
        const std::string& username = tok[1];
        if (isAdmin() && username != "admin" && users_.count(username)) {
            bool hasAccount = false;
            for (const auto& [_, acc] : accounts_) {
                if (acc.owner() == username) {
                    hasAccount = true;
                    break;
                }
            }
            if (!hasAccount) {
                users_.erase(username);
                if (currentUser_ == username) currentUser_ = "default";
                ok = changed = true;
            }
        }
        outputBool(ok, silent);
    } else if (cmd == "QUERY_USER" && tok.size() == 2) {
        const std::string& username = tok[1];
        if (isAdmin() && users_.count(username)) {
            std::vector<int> ids;
            for (const auto& [id, acc] : accounts_) {
                if (acc.owner() == username) ids.push_back(id);
            }
            if (!ids.empty()) {
                ok = true;
                if (!silent) {
                    for (int id : ids) outputAccount(accounts_[id], false);
                }
            }
        }
        if (!ok) outputBool(false, silent);
    } else if (cmd == "QUERY_USERLIST" && tok.size() == 1) {
        if (!users_.empty()) {
            ok = true;
            if (!silent) {
                std::map<std::string, int> count;
                for (const auto& u : users_) count[u] = 0;
                for (const auto& [_, acc] : accounts_) count[acc.owner()]++;
                for (const auto& [u, c] : count) std::cout << "USER " << u << ' ' << c << '\n';
            }
        }
        if (!ok) outputBool(false, silent);
    } else if (cmd == "SWITCH" && tok.size() == 2) {
        if (users_.count(tok[1])) {
            currentUser_ = tok[1];
            ok = changed = true;
        }
        outputBool(ok, silent);
    } else if (cmd == "WHOAMI" && tok.size() == 1) {
        if (!currentUser_.empty()) {
            ok = true;
            if (!silent) std::cout << currentUser_ << '\n';
        }
        if (!ok) outputBool(false, silent);
    } else {
        outputBool(false, silent);
    }

    appendLogIfNeeded(raw, changed, allowLog);
    return ok;
}

}  // namespace bank
