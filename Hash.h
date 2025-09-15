#include "string"
class Hash {
public:
    Hash();
    
    std::string Hashing(std::string password);
    bool Compare(std::string DBpassword, std::string password);
};