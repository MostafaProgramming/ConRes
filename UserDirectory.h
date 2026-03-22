#pragma once

#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

struct UserRecord {
    int id;
    std::string username;
};

class UserDirectory {
public:
    UserDirectory()
        : records_{
              {1, "engineer_alex"},
              {2, "engineer_bella"},
              {3, "engineer_carter"},
              {4, "engineer_dina"},
              {5, "engineer_ethan"},
              {6, "engineer_farah"},
              {7, "engineer_gabriel"},
              {8, "engineer_hana"},
              {9, "engineer_isaac"},
              {10, "engineer_jade"},
          }
    {
        for (const auto& record : records_)
            byId_.emplace(record.id, record);
    }

    std::optional<UserRecord> authenticate(int uid, const std::string& username) const
    {
        auto it = byId_.find(uid);
        if (it == byId_.end() || it->second.username != username)
            return std::nullopt;

        return it->second;
    }

    const std::vector<UserRecord>& records() const
    {
        return records_;
    }

private:
    std::vector<UserRecord> records_;
    std::unordered_map<int, UserRecord> byId_;
};
