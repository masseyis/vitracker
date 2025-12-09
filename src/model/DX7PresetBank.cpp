#include "DX7PresetBank.h"
#include <JuceHeader.h>
#include <fstream>
#include <algorithm>
#include <cctype>
#include <filesystem>

namespace fs = std::filesystem;

namespace model {

DX7PresetBank::DX7PresetBank() = default;
DX7PresetBank::~DX7PresetBank() = default;

bool DX7PresetBank::validateSysexHeader(const uint8_t* data, size_t size)
{
    if (size < DX7_SYSEX_SIZE) {
        return false;
    }

    // Check sysex header: F0 43 xx 09 20 00
    if (data[0] != 0xF0) return false;       // Sysex start
    if (data[1] != 0x43) return false;       // Yamaha ID
    // data[2] is MIDI channel, can be 0x00-0x0F
    if (data[3] != 0x09) return false;       // Format: 32-voice bulk dump
    if (data[4] != 0x20) return false;       // Byte count MSB
    if (data[5] != 0x00) return false;       // Byte count LSB

    // Check sysex end
    if (data[DX7_SYSEX_SIZE - 1] != 0xF7) return false;

    return true;
}

std::string DX7PresetBank::sanitizePatchName(const std::string& name)
{
    std::string result = name;

    // Trim trailing spaces
    while (!result.empty() && result.back() == ' ') {
        result.pop_back();
    }

    // Replace non-printable characters with spaces
    for (char& c : result) {
        if (c < 32 || c > 126) {
            c = ' ';
        }
    }

    return result;
}

std::string DX7PresetBank::getFileBasename(const std::string& path)
{
    fs::path p(path);
    return p.stem().string();
}

std::string DX7PresetBank::extractPatchName(const uint8_t* packedData)
{
    // Patch name is the last 10 bytes of the 128-byte packed patch
    std::string name;
    name.reserve(10);

    for (int i = 0; i < 10; ++i) {
        char c = static_cast<char>(packedData[118 + i]);
        if (c < 32 || c > 126) {
            c = ' ';
        }
        name += c;
    }

    // Trim trailing spaces
    while (!name.empty() && name.back() == ' ') {
        name.pop_back();
    }

    return name;
}

bool DX7PresetBank::loadSysexFile(const std::string& filePath)
{
    std::ifstream file(filePath, std::ios::binary);
    if (!file) {
        return false;
    }

    // Get file size
    file.seekg(0, std::ios::end);
    size_t fileSize = file.tellg();
    file.seekg(0, std::ios::beg);

    // Read file contents
    std::vector<uint8_t> data(fileSize);
    file.read(reinterpret_cast<char*>(data.data()), fileSize);
    file.close();

    // Validate header
    if (!validateSysexHeader(data.data(), fileSize)) {
        return false;
    }

    // Get bank name from filename
    std::string bankName = getFileBasename(filePath);

    // Extract 32 patches
    const uint8_t* patchData = data.data() + DX7_SYSEX_HEADER_SIZE;

    for (int i = 0; i < DX7_PATCHES_PER_BANK; ++i) {
        const uint8_t* patch = patchData + (i * DX7_PACKED_PATCH_SIZE);

        DX7Preset preset;
        preset.name = sanitizePatchName(extractPatchName(patch));
        preset.bankName = bankName;
        preset.patchIndex = i;

        // Copy packed data
        std::copy(patch, patch + DX7_PACKED_PATCH_SIZE, preset.packedData.begin());

        // Skip empty/init patches (heuristic: all zeros or default name)
        if (!preset.name.empty() && preset.name != "INIT VOICE") {
            presets_.push_back(std::move(preset));
        }
    }

    return true;
}

void DX7PresetBank::scanDirectory(const std::string& dirPath, bool recursive)
{
    try {
        if (recursive) {
            for (const auto& entry : fs::recursive_directory_iterator(dirPath)) {
                if (entry.is_regular_file()) {
                    std::string ext = entry.path().extension().string();
                    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
                    if (ext == ".syx") {
                        loadSysexFile(entry.path().string());
                    }
                }
            }
        } else {
            for (const auto& entry : fs::directory_iterator(dirPath)) {
                if (entry.is_regular_file()) {
                    std::string ext = entry.path().extension().string();
                    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
                    if (ext == ".syx") {
                        loadSysexFile(entry.path().string());
                    }
                }
            }
        }
    } catch (const fs::filesystem_error&) {
        // Ignore filesystem errors (directory doesn't exist, permissions, etc.)
    }
}

const DX7Preset* DX7PresetBank::getPreset(int index) const
{
    if (index < 0 || index >= static_cast<int>(presets_.size())) {
        return nullptr;
    }
    return &presets_[index];
}

std::vector<int> DX7PresetBank::searchPresets(const std::string& query) const
{
    std::vector<int> results;

    // Convert query to lowercase for case-insensitive search
    std::string lowerQuery = query;
    std::transform(lowerQuery.begin(), lowerQuery.end(), lowerQuery.begin(), ::tolower);

    for (size_t i = 0; i < presets_.size(); ++i) {
        std::string lowerName = presets_[i].name;
        std::transform(lowerName.begin(), lowerName.end(), lowerName.begin(), ::tolower);

        if (lowerName.find(lowerQuery) != std::string::npos) {
            results.push_back(static_cast<int>(i));
        }
    }

    return results;
}

void DX7PresetBank::unpackPatch(const uint8_t* packed, uint8_t* unpacked)
{
    // DX7 sysex packed format to unpacked format conversion
    // Based on the DX7 voice structure

    // Process 6 operators (packed format has them in reverse order: op6 first)
    for (int op = 0; op < 6; ++op) {
        const uint8_t* src = &packed[op * 17];  // Each packed operator is 17 bytes
        uint8_t* dst = &unpacked[op * 21];      // Each unpacked operator is 21 bytes

        // EG rates (4 bytes, direct copy)
        dst[0] = src[0];   // R1
        dst[1] = src[1];   // R2
        dst[2] = src[2];   // R3
        dst[3] = src[3];   // R4

        // EG levels (4 bytes, direct copy)
        dst[4] = src[4];   // L1
        dst[5] = src[5];   // L2
        dst[6] = src[6];   // L3
        dst[7] = src[7];   // L4

        // Keyboard level scaling
        dst[8] = src[8];                    // BP (breakpoint)
        dst[9] = src[9];                    // LD (left depth)
        dst[10] = src[10];                  // RD (right depth)
        dst[11] = src[11] & 0x03;           // LC (left curve)
        dst[12] = (src[11] >> 2) & 0x03;    // RC (right curve)

        // Rate/detune
        dst[13] = src[12] & 0x07;           // RS (rate scaling)
        dst[14] = (src[12] >> 3) & 0x0F;    // DET (detune, actually 0-14)

        // AMS/KVS
        dst[15] = src[13] & 0x03;           // AMS (amp mod sens)
        dst[16] = (src[13] >> 2) & 0x07;    // KVS (key velocity sens)

        // Output level
        dst[17] = src[14];                  // OL (output level)

        // Frequency
        dst[18] = src[15] & 0x01;           // PM (osc mode: 0=ratio, 1=fixed)
        dst[19] = (src[15] >> 1) & 0x1F;    // FC (freq coarse)
        dst[20] = src[16];                  // FF (freq fine)
    }

    // Pitch EG (8 bytes starting at packed offset 102)
    const uint8_t* pitchSrc = &packed[102];
    uint8_t* pitchDst = &unpacked[126];
    for (int i = 0; i < 8; ++i) {
        pitchDst[i] = pitchSrc[i];
    }

    // Algorithm, feedback, osc sync (packed byte 110)
    unpacked[134] = packed[110] & 0x1F;           // Algorithm (0-31)
    unpacked[135] = packed[111] & 0x07;           // Feedback (0-7)
    unpacked[136] = (packed[111] >> 3) & 0x01;    // OSC key sync

    // LFO (6 bytes starting at packed offset 112)
    const uint8_t* lfoSrc = &packed[112];
    uint8_t* lfoDst = &unpacked[137];
    lfoDst[0] = lfoSrc[0];                        // LFO speed
    lfoDst[1] = lfoSrc[1];                        // LFO delay
    lfoDst[2] = lfoSrc[2];                        // LFO PMD
    lfoDst[3] = lfoSrc[3];                        // LFO AMD
    lfoDst[4] = lfoSrc[4] & 0x01;                 // LFO sync
    lfoDst[5] = (lfoSrc[4] >> 1) & 0x07;          // LFO wave

    // PMS
    unpacked[143] = (packed[116] >> 4) & 0x07;    // PMS (pitch mod sens)

    // Transpose
    unpacked[144] = packed[117];                   // Transpose

    // Patch name (10 bytes starting at packed offset 118)
    for (int i = 0; i < 10; ++i) {
        unpacked[145 + i] = packed[118 + i];
    }
}

void DX7PresetBank::clear()
{
    presets_.clear();
}

void DX7PresetBank::loadBundledPresets()
{
    // Get the app bundle's Resources directory
    juce::File appBundle = juce::File::getSpecialLocation(juce::File::currentApplicationFile);

#if JUCE_MAC
    // On macOS, resources are in Contents/Resources/dx7/
    juce::File resourcesDir = appBundle.getChildFile("Contents/Resources/dx7");
#else
    // On other platforms, look for dx7 folder next to the executable
    juce::File resourcesDir = appBundle.getParentDirectory().getChildFile("dx7");
#endif

    if (!resourcesDir.isDirectory()) {
        // Fallback to built-in presets if resources not found
        loadBuiltInPresets();
        return;
    }

    // Find all .syx files in the resources directory
    juce::Array<juce::File> syxFiles;
    resourcesDir.findChildFiles(syxFiles, juce::File::findFiles, false, "*.syx");

    // Sort files alphabetically for consistent ordering
    syxFiles.sort();

    // Load each sysex file
    for (const auto& file : syxFiles) {
        loadSysexFile(file.getFullPathName().toStdString());
    }

    // If no presets were loaded, fall back to built-in presets
    if (presets_.empty()) {
        loadBuiltInPresets();
    }
}

void DX7PresetBank::loadBuiltInPresets()
{
    // Classic DX7 factory presets - 128 bytes packed format each
    // These are iconic sounds from the original DX7 ROM cartridges

    // E.PIANO 1 - The classic FM electric piano (Seinfeld bass, countless recordings)
    static const uint8_t epiano1[128] = {
        // Op6 (carrier)
        99, 99, 99, 99, 99, 99, 99, 0, 0, 0, 0, 0, 0, 0, 7, 0, 0,
        // Op5
        95, 50, 35, 78, 99, 98, 99, 0, 0, 0, 0, 0, 0, 0, 7, 0, 0,
        // Op4
        95, 35, 25, 78, 99, 75, 95, 0, 41, 0, 19, 0, 0, 3, 0, 2, 0,
        // Op3
        95, 29, 25, 78, 99, 75, 95, 0, 41, 0, 19, 0, 0, 3, 0, 2, 0,
        // Op2
        95, 20, 29, 78, 99, 75, 95, 0, 0, 0, 0, 0, 0, 0, 7, 0, 0,
        // Op1
        96, 25, 25, 78, 99, 75, 95, 0, 0, 0, 0, 0, 0, 0, 7, 0, 0,
        // Pitch EG: PR1-4, PL1-4
        84, 95, 95, 60, 50, 50, 50, 50,
        // Algorithm, feedback byte
        4, 6,
        // LFO: speed, delay, PMD, AMD, sync+wave
        35, 0, 0, 0, 1,
        // Transpose
        24,
        // Name
        'E', '.', 'P', 'I', 'A', 'N', 'O', ' ', ' ', '1'
    };

    // BRASS 1 - Classic FM brass
    static const uint8_t brass1[128] = {
        95, 50, 49, 99, 95, 96, 99, 0, 0, 0, 0, 0, 0, 0, 1, 0, 3,
        86, 76, 85, 90, 96, 96, 99, 0, 0, 0, 0, 0, 0, 0, 1, 0, 3,
        90, 65, 65, 90, 99, 96, 99, 0, 0, 0, 0, 0, 0, 0, 1, 0, 3,
        71, 99, 88, 96, 98, 91, 99, 0, 39, 0, 0, 0, 0, 0, 3, 0, 0,
        93, 60, 80, 90, 96, 93, 99, 0, 39, 0, 0, 0, 0, 0, 1, 0, 3,
        90, 40, 80, 90, 99, 93, 99, 0, 39, 0, 0, 0, 0, 0, 3, 0, 0,
        50, 50, 50, 50, 50, 50, 50, 50,
        21, 5,
        35, 0, 0, 0, 1,
        24,
        'B', 'R', 'A', 'S', 'S', ' ', ' ', ' ', ' ', '1'
    };

    // STRINGS 1 - Lush string pad
    static const uint8_t strings1[128] = {
        49, 99, 28, 68, 98, 98, 99, 0, 39, 0, 0, 0, 0, 0, 3, 0, 0,
        50, 99, 27, 68, 98, 98, 99, 0, 39, 0, 0, 0, 0, 0, 3, 0, 0,
        49, 99, 28, 68, 98, 98, 99, 0, 39, 0, 0, 0, 0, 0, 3, 0, 0,
        50, 99, 27, 68, 98, 98, 99, 0, 39, 0, 0, 0, 0, 0, 3, 0, 0,
        50, 99, 27, 68, 98, 98, 99, 0, 39, 0, 0, 0, 0, 0, 3, 0, 0,
        50, 99, 27, 68, 98, 98, 99, 0, 39, 0, 0, 0, 0, 0, 3, 0, 0,
        50, 50, 50, 50, 50, 50, 50, 50,
        31, 0,
        34, 33, 0, 7, 5,
        24,
        'S', 'T', 'R', 'I', 'N', 'G', 'S', ' ', ' ', '1'
    };

    // BASS 1 - Punchy FM bass
    static const uint8_t bass1[128] = {
        49, 99, 45, 50, 96, 99, 99, 0, 0, 0, 0, 0, 0, 0, 3, 0, 0,
        97, 99, 0, 0, 99, 99, 99, 0, 39, 0, 0, 0, 0, 7, 7, 0, 0,
        97, 99, 0, 0, 99, 99, 99, 0, 39, 0, 0, 0, 0, 7, 7, 0, 0,
        97, 99, 0, 0, 99, 99, 99, 0, 39, 0, 0, 0, 0, 7, 7, 0, 0,
        97, 99, 0, 0, 99, 99, 99, 0, 39, 0, 0, 0, 0, 7, 7, 0, 0,
        97, 99, 45, 0, 96, 99, 99, 0, 39, 0, 0, 0, 0, 7, 7, 0, 0,
        50, 50, 50, 50, 50, 50, 50, 50,
        31, 7,
        35, 0, 0, 0, 1,
        12,
        'B', 'A', 'S', 'S', ' ', ' ', ' ', ' ', ' ', '1'
    };

    // ORGAN 1 - Classic FM organ
    static const uint8_t organ1[128] = {
        95, 29, 20, 50, 99, 95, 99, 0, 0, 0, 0, 0, 0, 0, 7, 0, 0,
        95, 20, 20, 50, 99, 95, 99, 0, 0, 0, 0, 0, 0, 0, 7, 4, 0,
        95, 29, 20, 50, 99, 95, 99, 0, 0, 0, 0, 0, 0, 0, 7, 1, 0,
        95, 29, 20, 50, 99, 95, 99, 0, 0, 0, 0, 0, 0, 0, 7, 2, 0,
        95, 29, 20, 50, 99, 95, 99, 0, 0, 0, 0, 0, 0, 0, 7, 2, 0,
        95, 20, 20, 50, 99, 95, 99, 0, 0, 0, 0, 0, 0, 0, 7, 0, 0,
        50, 50, 50, 50, 50, 50, 50, 50,
        31, 0,
        50, 0, 0, 0, 1,
        24,
        'O', 'R', 'G', 'A', 'N', ' ', ' ', ' ', ' ', '1'
    };

    // MARIMBA - Mallet percussion
    static const uint8_t marimba[128] = {
        96, 32, 0, 0, 99, 0, 99, 0, 39, 0, 0, 0, 0, 0, 3, 0, 0,
        96, 32, 0, 0, 99, 0, 99, 0, 39, 0, 0, 0, 0, 0, 3, 14, 0,
        99, 0, 0, 0, 99, 99, 99, 0, 39, 0, 0, 0, 0, 7, 3, 0, 0,
        99, 0, 0, 0, 99, 99, 99, 0, 39, 0, 0, 0, 0, 7, 3, 0, 0,
        99, 0, 0, 0, 99, 99, 99, 0, 39, 0, 0, 0, 0, 7, 3, 0, 0,
        99, 0, 0, 0, 99, 99, 99, 0, 39, 0, 0, 0, 0, 7, 3, 0, 0,
        50, 50, 50, 50, 50, 50, 50, 50,
        4, 4,
        35, 0, 0, 0, 1,
        24,
        'M', 'A', 'R', 'I', 'M', 'B', 'A', ' ', ' ', ' '
    };

    // FLUTE 1 - Breathy flute
    static const uint8_t flute1[128] = {
        90, 55, 30, 75, 99, 90, 99, 0, 0, 0, 0, 0, 0, 0, 3, 0, 0,
        90, 55, 30, 75, 99, 99, 99, 0, 27, 0, 0, 0, 0, 2, 3, 0, 0,
        99, 0, 0, 0, 99, 99, 99, 0, 39, 0, 0, 0, 0, 7, 3, 0, 0,
        99, 0, 0, 0, 99, 99, 99, 0, 39, 0, 0, 0, 0, 7, 3, 0, 0,
        99, 0, 0, 0, 99, 99, 99, 0, 39, 0, 0, 0, 0, 7, 3, 0, 0,
        99, 0, 0, 0, 99, 99, 99, 0, 39, 0, 0, 0, 0, 7, 3, 0, 0,
        50, 50, 50, 50, 50, 50, 50, 50,
        31, 0,
        40, 0, 5, 3, 5,
        24,
        'F', 'L', 'U', 'T', 'E', ' ', ' ', ' ', ' ', '1'
    };

    // HARPSICH - Harpsichord
    static const uint8_t harpsich[128] = {
        95, 50, 25, 25, 99, 75, 99, 0, 0, 0, 0, 0, 0, 0, 0, 2, 0,
        95, 50, 25, 25, 99, 80, 99, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0,
        95, 50, 25, 25, 99, 75, 99, 0, 0, 0, 0, 0, 0, 0, 0, 4, 0,
        95, 50, 25, 25, 99, 75, 99, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0,
        96, 25, 25, 67, 99, 75, 99, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        95, 50, 25, 25, 99, 75, 99, 0, 0, 0, 0, 0, 0, 0, 0, 2, 0,
        50, 50, 50, 50, 50, 50, 50, 50,
        31, 0,
        35, 0, 0, 0, 1,
        24,
        'H', 'A', 'R', 'P', 'S', 'I', 'C', 'H', ' ', ' '
    };

    // VIBES - Vibraphone
    static const uint8_t vibes[128] = {
        95, 65, 45, 75, 99, 85, 99, 0, 39, 0, 0, 0, 0, 0, 0, 14, 0,
        95, 65, 45, 75, 99, 90, 99, 0, 39, 0, 0, 0, 0, 0, 3, 0, 0,
        99, 0, 0, 0, 99, 99, 99, 0, 39, 0, 0, 0, 0, 7, 3, 0, 0,
        99, 0, 0, 0, 99, 99, 99, 0, 39, 0, 0, 0, 0, 7, 3, 0, 0,
        99, 0, 0, 0, 99, 99, 99, 0, 39, 0, 0, 0, 0, 7, 3, 0, 0,
        99, 0, 0, 0, 99, 99, 99, 0, 39, 0, 0, 0, 0, 7, 3, 0, 0,
        50, 50, 50, 50, 50, 50, 50, 50,
        4, 0,
        65, 20, 0, 5, 3,
        24,
        'V', 'I', 'B', 'E', 'S', ' ', ' ', ' ', ' ', ' '
    };

    // SYNTH PAD - Ambient pad
    static const uint8_t synthpad[128] = {
        35, 99, 15, 45, 99, 99, 99, 0, 39, 0, 0, 0, 0, 0, 0, 1, 0,
        35, 99, 15, 45, 99, 99, 99, 0, 39, 0, 0, 0, 0, 0, 0, 1, 0,
        35, 99, 15, 45, 99, 95, 99, 0, 39, 0, 0, 0, 0, 0, 0, 2, 0,
        35, 99, 15, 45, 99, 95, 99, 0, 39, 0, 0, 0, 0, 0, 0, 4, 0,
        35, 99, 15, 45, 99, 95, 99, 0, 39, 0, 0, 0, 0, 0, 0, 1, 0,
        35, 99, 15, 45, 99, 99, 99, 0, 39, 0, 0, 0, 0, 0, 0, 1, 0,
        50, 50, 50, 50, 50, 50, 50, 50,
        31, 0,
        30, 40, 5, 10, 3,
        24,
        'S', 'Y', 'N', 'T', 'H', ' ', 'P', 'A', 'D', ' '
    };

    // Array of built-in patches
    struct BuiltInPatch {
        const uint8_t* data;
        const char* name;
    };

    static const BuiltInPatch builtInPatches[] = {
        { epiano1, "E.PIANO 1" },
        { brass1, "BRASS 1" },
        { strings1, "STRINGS 1" },
        { bass1, "BASS 1" },
        { organ1, "ORGAN 1" },
        { marimba, "MARIMBA" },
        { flute1, "FLUTE 1" },
        { harpsich, "HARPSICH" },
        { vibes, "VIBES" },
        { synthpad, "SYNTH PAD" }
    };

    // Add all built-in patches
    for (const auto& patch : builtInPatches) {
        DX7Preset preset;
        preset.name = patch.name;
        preset.bankName = "Factory";
        preset.patchIndex = static_cast<int>(presets_.size());
        std::copy(patch.data, patch.data + DX7_PACKED_PATCH_SIZE, preset.packedData.begin());
        presets_.push_back(std::move(preset));
    }
}

} // namespace model
