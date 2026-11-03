// Backup file, pure and host-testable:
//   # gbsc-backup 1
//   [file cfg/device.cfg]     settings lines (secret/device-only keys left out)
//   [file cfg/current.cfg]
//   [profiles]                marker: a restore with it replaces every profile
//   [raw cfg/profiles.cfg]    lines copied verbatim
//   [profile pNN]             one profile file, verbatim
//   [adv]                     marker: a restore with it replaces every ADV preset and macro
//   [adv preset pNN]          /adv/presets/pNN.yaml verbatim (checked by advYamlFeed, not key=value)
//   [adv macro mNN]           /adv/macros/mNN.yaml verbatim
#pragma once
#include <stdint.h>
#include <stddef.h>
#include "../settings/settings-fs.h"
#include "../settings/settings-store.h"

struct userOptions;

static const size_t BACKUP_MAX_BYTES = 655360;   // 640 KB upload limit
#define RESTORE_TMP_PATH "/tmp-restore.txt"

enum BackupError : uint8_t { BACKUP_OK, BACKUP_ERR_HEADER, BACKUP_ERR_SECTION, BACKUP_ERR_SYNTAX, BACKUP_ERR_EMPTY, BACKUP_ERR_READ };

// Resumable producer for a chunked HTTP response: fill() returns 0 only at the end
class BackupStreamer {
public:
    void begin(SettingsFs* fs, const char* const* profileIds, uint8_t profileCount);   // ids copied
    // After begin: the [adv] marker and these ADV files (ids copied); without it the backup has no [adv]
    void beginAdv(const char* const* presetIds, uint8_t presetCount, const char* const* macroIds, uint8_t macroCount);
    size_t fill(char* buf, size_t len);
    size_t total() const { return total_; }   // bytes produced so far (the page warns above BACKUP_MAX_BYTES)
private:
    bool nextSection();               // loads the next existing section's header into pending
    size_t fillSome(char* buf, size_t len);
    SettingsFs* fs_ = nullptr;
    char ids_[36][4];
    uint8_t idCount_ = 0;
    bool adv_ = false;
    char advIds_[48][4];              // presets then macros
    uint8_t presetCount_ = 0, macroCount_ = 0;
    size_t total_ = 0;
    char lastByte_ = '\n';           // last byte of a verbatim file: a missing final newline is added
    int section_ = -1;                // -1 = header line not sent yet
    bool inFile_ = false;
    bool filtered_ = false;
    char path_[24];
    size_t offset_ = 0;
    char line_[130];
    size_t lineLen_ = 0;
    bool lineOverflow_ = false;
    char pending_[160];
    size_t pendingLen_ = 0, pendingPos_ = 0;
    char readBuf_[64];
    size_t readLen_ = 0, readPos_ = 0;
};

struct RestorePlan { bool hasProfiles; uint8_t profileCount; uint32_t profileBlocks; bool hasAdv; uint8_t advCount; uint32_t advBlocks; };
inline bool backupTooBig(size_t total) { return total > BACKUP_MAX_BYTES; }   // a restore would refuse it
BackupError backupValidate(SettingsFs& fs, const char* path, RestorePlan* plan);
const char* backupErrorName(BackupError e);
// Settings sections into u (excluded keys keep their value); with [profiles], deleteProfiles(ctx) runs at the
// marker, then raw and profile sections are written through fs.openWriter(); the caller saves the settings
// false when a file could not be written (the restore reports error:write)
// With [adv], deleteAdv(advCtx) runs at that marker and the ADV sections are written the same way
bool backupApply(SettingsFs& fs, const char* path, userOptions& u, void (*deleteProfiles)(void*), void* ctx,
                 void (*deleteAdv)(void*) = nullptr, void* advCtx = nullptr);
// Blocks: profiles need neededBlocks + 2 spare, from the free blocks plus those the current profiles release
bool restoreSpaceOk(uint32_t freeBlocks, uint32_t currentProfileBlocks, uint32_t neededBlocks);
