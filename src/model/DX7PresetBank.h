#pragma once

#include <string>
#include <vector>
#include <memory>
#include <cstdint>
#include <array>

namespace model {

// Constants for DX7 sysex format
constexpr int DX7_SYSEX_HEADER_SIZE = 6;    // F0 43 ch 09 20 00
constexpr int DX7_PACKED_PATCH_SIZE = 128;
constexpr int DX7_UNPACKED_PATCH_SIZE = 156;
constexpr int DX7_PATCHES_PER_BANK = 32;
constexpr int DX7_BANK_DATA_SIZE = DX7_PACKED_PATCH_SIZE * DX7_PATCHES_PER_BANK;  // 4096
constexpr int DX7_SYSEX_SIZE = DX7_SYSEX_HEADER_SIZE + DX7_BANK_DATA_SIZE + 2;    // 4104 (includes checksum + F7)

struct DX7Preset {
    std::string name;                                     // Patch name (10 chars max)
    std::string bankName;                                 // Source bank filename
    int patchIndex;                                       // Index within the bank (0-31)
    std::array<uint8_t, DX7_PACKED_PATCH_SIZE> packedData;  // 128-byte packed patch data
};

class DX7PresetBank {
public:
    DX7PresetBank();
    ~DX7PresetBank();

    // Load a single sysex file (32 patches)
    bool loadSysexFile(const std::string& filePath);

    // Scan a directory for all .syx files
    void scanDirectory(const std::string& dirPath, bool recursive = true);

    // Get all loaded presets
    const std::vector<DX7Preset>& getPresets() const { return presets_; }

    // Get preset by index
    const DX7Preset* getPreset(int index) const;

    // Get preset count
    int getPresetCount() const { return static_cast<int>(presets_.size()); }

    // Search presets by name (case-insensitive substring match)
    std::vector<int> searchPresets(const std::string& query) const;

    // Unpack a 128-byte packed patch to 156-byte unpacked format
    static void unpackPatch(const uint8_t* packed, uint8_t* unpacked);

    // Extract patch name from packed data
    static std::string extractPatchName(const uint8_t* packedData);

    // Clear all loaded presets
    void clear();

    // Load presets from the app bundle's resources directory
    void loadBundledPresets();

    // Load built-in factory presets (used when no external files are found)
    void loadBuiltInPresets();

private:
    std::vector<DX7Preset> presets_;

    bool validateSysexHeader(const uint8_t* data, size_t size);
    std::string sanitizePatchName(const std::string& name);
    std::string getFileBasename(const std::string& path);
};

} // namespace model
