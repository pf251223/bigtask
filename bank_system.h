#ifndef BANK_SYSTEM_H
#define BANK_SYSTEM_H

#include <iosfwd>
#include <map>
#include <set>
#include <string>
#include <vector>

namespace bank {

class Date {
public:
    Date(int year = 1970, int month = 1, int day = 1);

    int year() const;
    int month() const;
    int day() const;

    bool isAfter(const Date& rhs) const;
    void advanceOneDay();
    std::string toDisplayString() const;
    int daysInCurrentYear() const;

    static bool isValid(int year, int month, int day);

private:
    int y_;
    int m_;
    int d_;

    static bool isLeapYear(int year);
    static int daysInMonth(int year, int month);
    static long long daysFromCivil(int y, unsigned m, unsigned d);
    int weekdayIndex() const;
};

enum class AccountType { Savings, Credit };

class Account {
public:
    static constexpr double kSavingsAnnualRate = 0.0115;
    static constexpr double kCreditAnnualRate = 0.0225;
    static constexpr double kCreditNegativeDailyRate = 0.0005;

    Account() = default;
    Account(int id, AccountType type, std::string name, std::string owner, double initialAmountOrCredit);

    int id() const;
    AccountType type() const;
    const std::string& name() const;
    const std::string& owner() const;
    double balance() const;
    double credit() const;

    void setName(const std::string& newName);
    bool setCredit(double newCredit);

    void deposit(double amount);
    bool withdraw(double amount);
    bool canWithdraw(double amount) const;

    void accrueDailyInterest(const Date& date);
    void postMonthlyInterestIfNeeded(const Date& date);

    bool canClose() const;
    void print(std::ostream& out) const;

private:
    int id_ = 0;
    AccountType type_ = AccountType::Savings;
    std::string name_;
    std::string owner_;
    double balance_ = 0.0;
    double credit_ = 0.0;
    double pendingInterest_ = 0.0;
};

class BankSystem {
public:
    BankSystem();
    void run();

private:
    std::map<int, Account> accounts_;
    std::set<std::string> users_;
    std::vector<std::string> logs_;
    std::string currentUser_;
    Date currentDate_;

    void resetInitial();
    bool isAdmin() const;
    bool canAccessOwnedAccount(int id) const;

    static bool parseInt(const std::string& s, int& out);
    static bool parseDouble(const std::string& s, double& out);
    static bool isValidUsername(const std::string& username);

    void outputBool(bool ok, bool silent) const;
    void outputAccount(const Account& acc, bool silent) const;
    void appendLogIfNeeded(const std::string& raw, bool changed, bool allowLog);

    void settleOneDay();

    bool execute(const std::string& raw, bool silent, bool allowLog);
};

}  // namespace bank

#endif
