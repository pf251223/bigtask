#ifndef BANK_SYSTEM_H
#define BANK_SYSTEM_H

#include <map>
#include <set>
#include <string>
#include <vector>

namespace bank {

constexpr double SAVINGS_ANNUAL_RATE = 0.0115;
constexpr double CREDIT_ANNUAL_RATE = 0.0225;
constexpr double CREDIT_NEG_DAILY_RATE = 0.0005;

enum class AccountType { Savings, Credit };

struct SimpleDate {
    int y = 1970;
    int m = 1;
    int d = 1;
};

struct Account {
    int id = 0;
    AccountType type = AccountType::Savings;
    std::string name;
    std::string owner;
    double balance = 0.0;
    double credit = 0.0;
    double pendingInterest = 0.0;
};

class BankSystem {
public:
    BankSystem();
    void run();

private:
    std::map<int, Account> accounts;
    std::set<std::string> users;
    std::vector<std::string> logs;
    std::string currentUser;
    SimpleDate currentDate;

    void resetInitial();
    bool isAdmin() const;
    void addLogIfNeeded(const std::string& raw, bool changed, bool allowLog);
    void printBool(bool ok, bool silent);
    void printAccount(const Account& a);
    void settleOneDayForAll();
    void advanceOneDay();
    bool parseDouble(const std::string& s, double& out);
    bool parseInt(const std::string& s, int& out);
    bool canAccessOwned(int id) const;
    bool execute(const std::string& raw, bool silent, bool allowLog);
};

bool nearlyZero(double x);
bool isLeap(int y);
int daysInYear(int y);
int daysInMonth(int y, int m);
long long daysFromCivil(int y, unsigned m, unsigned d);
int weekdayIndex(const SimpleDate& dt);
std::string showDateString(const SimpleDate& dt);
bool validDate(int y, int m, int d);
bool isAfter(const SimpleDate& a, const SimpleDate& b);
SimpleDate nextDay(SimpleDate x);

}  // namespace bank

#endif
