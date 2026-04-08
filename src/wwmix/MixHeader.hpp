// File: MixHeader.hpp
// Author: fbsagr
// Created on June 5, 2014, 4:37 PM

#pragma once

#include "MixId.hpp"

#include <string>
#include <fstream>
#include <map>

struct IndexInfo
{
    uint32_t offset; // offset from start of body
    uint32_t size;   // size of this internal file
};

typedef std::map<int32_t, IndexInfo> MixIndex;
typedef std::pair<int32_t, IndexInfo> MixEntry;
typedef std::map<int32_t, IndexInfo>::iterator MixIndexIterator;

// Mix archive header.
//
// Mix archive starts with archive header storing information about number of files
// included and their total size. Some archives (RA/TS) starts with 4b flags, which
// is followed by header.
//
// old mix format header
//  - 2B - number of files
//  - 4B - total content's size
//
// RA/TS mix format header
//  - 4B - flags
//  - 2B - number of files
//  - 4B - total content's size
//
// determine what type archive is
// Now here comes part which might be hard to understand. Data are stored in
// little endian format, which means little ending is stored first and big ending
// last. Number 0x11223344, where little ending is 44, will be stored like this
// 44 33 22 11 - bytes are stored in reverse direction as you can see.
//
// Flags are stored only in LAST 2 bytes, that means if file starts with flags,
// FIRST 2 bytes are 00 00, and other 2 bytes contains flag. If there are no flags,
// archive can not start with 00 00, because that would mean it contains no files.
//
// So we read header no matter what type archive is and check for first two bytes,
// which are stored in c_files. If c_files == 0, we take first 4 bytes, which are
// stored in flags as flag information, move file pointer to 4th byte and repeat
// reading of header (6b). Now c_files contains number of files and size contains
// size of all files.

// Included file header.
//
// Right after mix file header comes file index. Every included file has it's entry
// there storing information about it's CRC ID (mix archive doesn't store information
// about file names, but CRC ID can be calculated from file name), offset in body
// block and size. Remember that you have to add body offset to file offset before
// you read a file data.
//
// structure of index entry
// - 4B - CRC id
// - 4B - file offset
// - 4B - file size
//
// count body offset
// To get body offset, you have to count size of information before body.
// - does archive contain flags? (4B if so)
// - mix archive header (6B)
// - file index (c_files * 12B)
// - is archive encrypted? (80B if so)

class MixHeader
{
  public:
    /// @brief Construct a header helper for a specific game's MIX format.
    MixHeader(Game game);

    /// @brief Read header metadata from an archive stream.
    bool ReadHeader(std::fstream &fh);

    /// @brief Read and decode the encrypted-header key source block.
    bool ReadKeySource(std::fstream &fh);

    /// @brief Write the current header state to an archive stream.
    bool WriteHeader(std::fstream &fh);

    /// @brief Append an index entry for a file in the archive body.
    bool AddEntry(int32_t id, uint32_t size);

    /// @brief Remove an index entry and optionally compact later offsets.
    bool RemoveEntry(int32_t id, bool adjust);

    /// @brief Return the index entry for a file ID, or an empty entry when missing.
    IndexInfo GetEntry(int32_t id) const;

    /// @brief Return whether the header checksum flag is set.
    bool GetHasChecksum() const
    {
        return m_has_checksum;
    }

    /// @brief Enable checksum support for the header.
    void SetHasChecksum();

    /// @brief Disable checksum support for the header.
    void ClearHasChecksum();

    /// @brief Return whether the header encryption flag is set.
    bool GetIsEncrypted() const
    {
        return m_is_encrypted;
    }

    /// @brief Enable header encryption and prepare key material.
    void SetIsEncrypted();

    /// @brief Disable header encryption and restore plain-header sizing.
    void ClearIsEncrypted();

    /// @brief Return the game format associated with this header.
    Game GetGame() const
    {
        return m_game_type;
    }
    //void printContents();

    /// @brief Return the serialized header size in bytes.
    uint32_t GetHeaderSize() const
    {
        return m_header_size;
    }

    /// @brief Return the raw extended-header flag word.
    uint32_t GetHeaderFlags() const
    {
        return m_header_flags;
    }

    /// @brief Return the archive body size in bytes.
    uint32_t GetBodySize() const
    {
        return m_body_size;
    }

    /// @brief Override the stored archive body size.
    void SetBodySize(uint32_t size)
    {
        m_body_size = size;
    }

    /// @brief Return the number of indexed files in the archive.
    uint16_t GetFileCount() const
    {
        return m_file_count;
    }

    /// @brief Return an iterator to the first index entry.
    MixIndexIterator GetBegin()
    {
        return m_index.begin();
    }

    /// @brief Return an iterator past the last index entry.
    MixIndexIterator GetEnd()
    {
        return m_index.end();
    }

    /// @brief Return the derived Blowfish key for encrypted headers.
    const char *GetKey() const
    {
        return m_key;
    }

    /// @brief Return the raw 80-byte key source block for encrypted headers.
    const char *GetKeySource() const
    {
        return m_keysource;
    }

  private:
    /// @brief Derive the Blowfish key from the stored key source block.
    void SetKey();

    /// @brief Generate and encode a key source block for encrypted headers.
    void SetKeySource();
    const int32_t mix_checksum;
    const int32_t mix_encrypted;

    Game m_game_type;
    uint16_t m_file_count;
    uint32_t m_body_size;
    uint32_t m_header_flags;
    uint32_t m_header_size;
    bool m_has_checksum;
    bool m_is_encrypted;
    MixIndex m_index;
    MixIndex m_old_index;
    char m_keysource[80];
    char m_key[56];

    /// @brief Reset the header state to a clean empty archive.
    void Reset();

    /// @brief Read an encrypted header and decrypt its index entries.
    bool ReadEncrypted(std::fstream &fh);

    /// @brief Write the header in encrypted form.
    bool WriteEncrypted(std::fstream &fh);

    /// @brief Read a plain-text header and its index entries.
    bool ReadUnencrypted(std::fstream &fh);

    /// @brief Write the header in plain-text form.
    bool WriteUnencrypted(std::fstream &fh);
    //void setGame(Game game); //{ m_game_type = game; }
};
