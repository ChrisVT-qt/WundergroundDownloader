// == Weather Underground communications

// Delay for obtaining updates (every hour)
#define CHECK_FOR_UPDATES_DELAY 3600*1000
#define CHECK_START_TIME "06:00"
#define CHECK_END_TIME "22:00"

// But configuration
#define WU_PWS_NAME "your pws name"
#define WU_TOKEN "your wu api token"

// General bot directories
#define WU_ROOT QString("your wu root/")
#define WU_DATABASE_DIR (WU_ROOT + "Database/")
#define WU_DATABASE_FILE (WU_DATABASE_DIR + "Wunderground.sql")
