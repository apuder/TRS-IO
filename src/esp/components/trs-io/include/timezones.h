
#include <string>

// Maps a friendly timezone to a POSIX timezone, because the ESP32
// doesn't appear to embed this database. Returns an empty string
// if the friendly timezone is not in the database.
std::string friendlyTimezoneToPosix(const std::string &friendlyTimezone);
