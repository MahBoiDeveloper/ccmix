#include "MixFile.hpp"

#include "cryptopp/sha.h"
#include "cryptopp/integer.h"
#include <cstdio>
#include <algorithm>
#include <filesystem>
#include <limits>
#include <print>

using CryptoPP::SHA1;

class MixFileIo
{
  public:
    static constexpr const char *TempFilePath = "~ccmix.tmp";
    static constexpr std::streamsize CopyBufferSize = 32 * 1024;

    static std::string JoinPath(const std::string &directory,
                                const std::string &name)
    {
        const std::filesystem::path joinedPath =
            std::filesystem::u8path(directory) / name;
        const std::u8string joinedPathText = joinedPath.u8string();
        return std::string(joinedPathText.begin(), joinedPathText.end());
    }

    static bool TryGetRegularFileSize(const std::filesystem::path &path,
                                      uint32_t &size)
    {
        std::error_code error;
        if (!std::filesystem::is_regular_file(path, error) || error)
        {
            return false;
        }

        const std::uintmax_t fileSize = std::filesystem::file_size(path, error);
        if (error || fileSize > std::numeric_limits<uint32_t>::max())
        {
            return false;
        }

        size = static_cast<uint32_t>(fileSize);
        return true;
    }

    static std::streamoff GetStreamSize(std::istream &stream)
    {
        stream.clear();
        stream.seekg(0, std::ios::end);
        const std::streamoff size = stream.tellg();
        stream.seekg(0, std::ios::beg);
        return size;
    }

    static bool CopyExact(std::istream &input, std::ostream &output,
                          std::streamoff size)
    {
        if (size <= 0)
        {
            return true;
        }

        std::vector<char> buffer(CopyBufferSize);
        std::streamoff remaining = size;

        while (remaining > 0)
        {
            const std::streamsize chunk = static_cast<std::streamsize>(
                std::min<std::streamoff>(remaining, CopyBufferSize));
            input.read(&buffer.at(0), chunk);
            const std::streamsize read = input.gcount();

            if (read <= 0)
            {
                return false;
            }

            output.write(&buffer.at(0), read);
            if (!output)
            {
                return false;
            }

            remaining -= read;
        }

        return true;
    }

    static bool CopyToEnd(std::istream &input, std::ostream &output)
    {
        std::vector<char> buffer(CopyBufferSize);

        while (input.good())
        {
            input.read(&buffer.at(0), CopyBufferSize);
            const std::streamsize read = input.gcount();
            if (read > 0)
            {
                output.write(&buffer.at(0), read);
                if (!output)
                {
                    return false;
                }
            }
        }

        const bool reachedEof = input.eof();
        input.clear();
        return reachedEof && output.good();
    }

    static bool CopyBodyWithSkips(std::istream &input, std::ostream &output,
                                  std::streamoff bodyOffset,
                                  std::streamoff bodyEnd,
                                  const std::map<uint32_t, uint32_t> &skips)
    {
        input.clear();
        input.seekg(bodyOffset, std::ios::beg);
        if (!input)
        {
            return false;
        }

        std::streamoff current = bodyOffset;
        for (std::map<uint32_t, uint32_t>::const_iterator it = skips.begin();
             it != skips.end(); ++it)
        {
            const std::streamoff skipBegin = bodyOffset + it->first;
            if (skipBegin > current &&
                !CopyExact(input, output, skipBegin - current))
            {
                return false;
            }

            current = skipBegin + it->second;
            input.clear();
            input.seekg(current, std::ios::beg);
            if (!input)
            {
                return false;
            }
        }

        if (bodyEnd > current)
        {
            return CopyExact(input, output, bodyEnd - current);
        }

        return true;
    }

    static void RemoveTempFile()
    {
        std::error_code error;
        std::filesystem::remove(std::filesystem::u8path(TempFilePath), error);
    }
};

class MixFileInfoSupport
{
  public:
    static const char *GameLabel(const Game game)
    {
        switch (game)
        {
        case Game::TD:
            return "TD";
        case Game::RA:
            return "RA";
        case Game::TS:
            return "TS";
        case Game::D2:
            return "D2";
        case Game::D2K:
            return "D2K";
        case Game::RA2:
            return "RA2";
        default:
            return "Unknown";
        }
    }

    static bool HasExtendedHeaderLayout(const MixHeader &header)
    {
        const uint32_t classicHeaderSize =
            6u + static_cast<uint32_t>(header.GetFileCount()) * 12u;
        return header.GetHeaderSize() != classicHeaderSize;
    }

    static const char *HeaderLayoutLabel(const MixHeader &header)
    {
        if (header.GetIsEncrypted())
        {
            return "extended encrypted";
        }
        if (HasExtendedHeaderLayout(header))
        {
            return "extended";
        }
        return "classic";
    }
};

std::string MixFile::BaseName(const std::string &pathname) const
{
    const std::filesystem::path path = std::filesystem::u8path(pathname);
    const std::u8string filename = path.filename().u8string();
    return std::string(filename.begin(), filename.end());
}

MixFile::MixFile(const std::string &gmd, Game game, const std::string &gmdCache)
    : m_header(game),
      m_global_db(),
      m_local_db(game),
      m_has_lmd(false),
      m_file_path()
{
    std::fill(m_checksum, m_checksum + 20, 0);

    const std::string cachePath = gmdCache.empty() ? "gmd.json" : gmdCache;
    if (!m_global_db.Load(gmd, cachePath, true))
    {
        std::println("Could not open global mix database.dat");
    }
}

MixFile::~MixFile()
{
    Close();
}

bool MixFile::Open(const std::string &path, const bool writeAccess)
{
    IndexInfo lmd;
    m_file_path = path;
    m_has_lmd = false;
    std::fill(m_checksum, m_checksum + 20, 0);

    if (fh.is_open())
    {
        Close();
    }

    const std::ios::openmode openMode =
        writeAccess ? std::fstream::in | std::fstream::out | std::fstream::binary :
                      std::fstream::in | std::fstream::binary;
    fh.open(std::filesystem::u8path(path), openMode);
    if (!fh.is_open())
    {
        std::println("File {} failed to open", path);
        return false;
    }

    const std::streamoff fsize = MixFileIo::GetStreamSize(fh);

    if (!m_header.ReadHeader(fh))
    {
        Close();
        return false;
    }

    if (fsize < static_cast<std::streamoff>(m_header.GetHeaderSize()))
    {
        Close();
        return false;
    }

    const std::streamoff available_body = fsize - m_header.GetHeaderSize();
    if (m_header.GetBodySize() >= available_body)
    {
        m_header.SetBodySize(static_cast<uint32_t>(available_body));
    }

    if (m_header.GetHasChecksum())
    {
        fh.seekg(-20, std::ios::end);
        fh.read(reinterpret_cast<char *>(m_checksum), 20);
    }

    //check if we have a local mix db and if its sane-ish
    lmd = m_header.GetEntry(MixId::IdGen(m_header.GetGame(), m_local_db.GetDbName()));
    if (lmd.size >= 52 && lmd.size < m_header.GetBodySize())
    {
        m_local_db.ReadDb(fh, lmd.offset + m_header.GetHeaderSize(), lmd.size);
        m_has_lmd = true;
    }

    return true;
}

bool MixFile::CheckFileName(const std::string &name) const
{
    IndexInfo rv = m_header.GetEntry(MixId::IdGen(m_header.GetGame(), name));
    //size of a valid name will be > 0 hence true;
    return rv.size != 0;
}

bool MixFile::ExtractAll(const std::string &outPath)
{
    std::string fname;
    bool rv = true;

    for (MixIndexIterator it = m_header.GetBegin(); it != m_header.GetEnd(); it++)
    {

        fname = m_local_db.GetName(it->first);

        if (fname.substr(0, 4) == "[id]")
        {
            fname = m_global_db.GetName(m_header.GetGame(), it->first);
        }

        if (it->second.size <= m_header.GetBodySize())
        {
            rv = ExtractFile(it->first, MixFileIo::JoinPath(outPath, fname)) && rv;
        }
    }

    return rv;
}

bool MixFile::ExtractFile(int32_t id, const std::string &out)
{
    IndexInfo entry;

    // find file index entry
    entry = m_header.GetEntry(id);

    if (entry.size)
    {
        std::vector<char> buffer(entry.size);
        fh.clear();
        fh.seekg(m_header.GetHeaderSize() + entry.offset);
        fh.read(&buffer.at(0), entry.size);
        if (static_cast<uint32_t>(fh.gcount()) != entry.size)
        {
            fh.clear();
            return false;
        }

        std::ofstream of;
        of.open(std::filesystem::u8path(out), std::ios_base::binary);
        if (!of.is_open())
        {
            return false;
        }
        of.write(&buffer.at(0), entry.size);

        fh.clear();

        return of.good();
    }

    return false;
}

bool MixFile::ExtractFile(const std::string &filename, const std::string &outpath)
{
    return ExtractFile(MixId::IdGen(m_header.GetGame(), filename), outpath);
}

bool MixFile::CreateMix(const std::string &fileName, const std::string &in_dir,
                        bool with_lmd, bool encrypted, bool checksum,
                        const std::string &key_src)
{
    (void)key_src;

    std::vector<std::string> filenames; // file names

    if (encrypted)
        m_header.SetIsEncrypted();
    if (checksum)
        m_header.SetHasChecksum();

    //cout << "Game we are building for is " << m_header.GetGame() << endl;
    //if(m_header.GetIsEncrypted()){
    //    cout << "Header will be encrypted." << endl;
    //}

    //make sure we can iterate the directory
    const std::filesystem::path inputDirectory = std::filesystem::u8path(in_dir);
    std::error_code directoryError;
    std::filesystem::directory_iterator iterator(inputDirectory, directoryError);
    if (directoryError)
    {
        std::println("Error opening {}", in_dir);
        return false;
    }

    //iterate through entries in directory, ignoring directories
    for (std::filesystem::directory_iterator end;
         iterator != end;
         iterator.increment(directoryError))
    {
        if (directoryError)
        {
            std::println("Error opening {}", in_dir);
            return false;
        }

        const std::filesystem::directory_entry &directoryEntry = *iterator;
        uint32_t fileSize = 0;
        if (MixFileIo::TryGetRegularFileSize(directoryEntry.path(), fileSize))
        {
            const std::u8string filenameText =
                directoryEntry.path().filename().u8string();
            const std::string filename(
                filenameText.begin(), filenameText.end());

            //check if we have an ID containing file name, if not add to localdb
            if (!MixId::IsIdName(filename))
            {
                if (!m_local_db.AddName(filename))
                {
                    std::println("Skipping {}, ID Collision", filename);
                    continue;
                }
            }

            //try adding entry to header, skip if failed
            if (!m_header.AddEntry(MixId::IdGen(m_header.GetGame(),
                                                filename),
                                   fileSize))
            {
                continue;
            }

            //finally add the filename to the list of files we will add
            filenames.push_back(filename);
        }
    }

    //are we wanting lmd? if so push header info for it here
    if (with_lmd)
    {
        filenames.push_back(m_local_db.GetDbName());
        m_header.AddEntry(MixId::IdGen(m_header.GetGame(), m_local_db.GetDbName()),
                          m_local_db.GetSize());
    }

    //cout << m_header.GetBodySize() << " files, total size " << m_header.GetFileCount() << " before writing header" << endl;

    //if we are encrypted, get a key source
    //if(m_header.GetIsEncrypted()){
    //    ifile.open(key_src.c_str(), ios::binary|ios::in);
    //    //ReadKeySource checks if file is actually open
    //    if(!m_header.ReadKeySource(ifile)){
    //        cout << "Could not open a key_source, encryption disabled" << endl;
    //        m_header.ClearIsEncrypted();
    //    } else {
    //        cout << "Header will be encrypted" << endl;
    //    }
    //    ifile.close();
    //}

    //time to start writing our new file
    fh.open(
        std::filesystem::u8path(fileName),
        std::fstream::in | std::fstream::out | std::fstream::binary |
            std::fstream::trunc);

    if (!fh.is_open())
    {
        std::println("Failed to create empty file");
        return false;
    }

    //write a header
    if (!m_header.WriteHeader(fh))
    {
        std::println("Failed to write header.");
        return false;
    }

    //cout << "Writing the body now" << endl;

    std::println("Writing {} files.", filenames.size());

    //write the body, offset is set in header based on when added so should match
    for (unsigned int i = 0; i < filenames.size(); i++)
    {
        //last entry is lmd if with_lmd so break for special handling
        if (with_lmd && i == filenames.size() - 1)
            break;

        const std::string input_path = MixFileIo::JoinPath(in_dir, filenames[i]);
        std::println("{}", input_path);
        std::fstream ifile;
        ifile.open(
            std::filesystem::u8path(input_path),
            std::fstream::in | std::fstream::binary);
        if (!ifile.is_open())
        {
            std::println("Could not open input file {}", filenames[i]);
            return false;
        }
        if (!MixFileIo::CopyToEnd(ifile, fh))
        {
            return false;
        }
    }

    //handle lmd writing here.
    if (with_lmd)
    {
        m_local_db.WriteDb(fh);
    }

    //just write the checksum, flag is set further up
    if (m_header.GetHasChecksum())
    {
        if (!WriteChecksum(fh))
        {
            fh.close();
            return false;
        }
    }

    //cout << "Offset to data is " << m_header.GetHeaderSize() << endl;

    fh.close();

    return true;
}

bool MixFile::AddFile(const std::string &name)
{
    std::fstream ofh;
    IndexInfo lmd;
    IndexInfo old;

    m_skip.clear();

    //get filename without path info
    const std::string basename = BaseName(name);

    std::println("Adding {}", basename);

    //save the old data offset from header before we started changing it.
    const std::streamoff old_offset = m_header.GetHeaderSize();
    const std::streamoff old_size = old_offset + m_header.GetBodySize();

    lmd = m_header.GetEntry(MixId::IdGen(m_header.GetGame(), m_local_db.GetDbName()));
    old = m_header.GetEntry(MixId::IdGen(m_header.GetGame(), basename));

    //add skip entry for lmd if we have one and remove from header
    if (lmd.size)
    {
        m_skip[lmd.offset] = lmd.size;
        m_header.RemoveEntry(MixId::IdGen(m_header.GetGame(),
                                          m_local_db.GetDbName()),
                             true);
    }

    //setup to skip over the file if it is replacing
    if (old.size)
    {
        m_skip[old.offset] = old.size;
        m_header.RemoveEntry(MixId::IdGen(m_header.GetGame(), basename), true);
        std::println("A file with the same ID exists and will be replaced.");
    }

    //stat file to add and check its not a directory
    uint32_t fileSize = 0;
    if (!MixFileIo::TryGetRegularFileSize(std::filesystem::u8path(name), fileSize))
    {
        std::println("Cannot add directory as a file");
        return false;
    }

    if (!old.size && !MixId::IsIdName(basename) && !m_local_db.AddName(basename))
    {
        return false;
    }
    m_header.AddEntry(MixId::IdGen(m_header.GetGame(), basename), fileSize);

    //if the lmd had a size before (thus existed), add it back to header now
    if (lmd.size)
    {
        m_header.AddEntry(MixId::IdGen(m_header.GetGame(), m_local_db.GetDbName()),
                          m_local_db.GetSize());
    }

    //open a temp file
    ofh.open(
        std::filesystem::u8path(MixFileIo::TempFilePath),
        std::ios::binary | std::ios::out | std::ios::trunc);
    if (!ofh.is_open())
    {
        std::println("Couldn't open a temporary file to buffer the changes");
        return false;
    }

    //write our new header
    if (!m_header.WriteHeader(ofh))
    {
        return false;
    }

    //copy the body of the old mix, skipping the old lmd and replaced file
    if (!MixFileIo::CopyBodyWithSkips(fh, ofh, old_offset, old_size, m_skip))
    {
        ofh.close();
        MixFileIo::RemoveTempFile();
        return false;
    }

    //open the file to add, if we can't open, bail and delete temp file
    std::fstream ifh;
    ifh.open(std::filesystem::u8path(name), std::ios::binary | std::ios::in);
    if (!ifh.is_open())
    {
        std::println("Failed to open file to add");
        ofh.close();
        MixFileIo::RemoveTempFile();
        return false;
    }

    //add new file to mix body
    if (!MixFileIo::CopyExact(ifh, ofh, fileSize))
    {
        ifh.close();
        ofh.close();
        MixFileIo::RemoveTempFile();
        return false;
    }

    ifh.close();

    //write lmd if needed
    if (lmd.size)
    {
        m_local_db.WriteDb(ofh);
    }

    //write checksum to end of file if required.
    if (m_header.GetHasChecksum())
    {
        if (!WriteChecksum(ofh, 0))
        {
            ofh.close();
            MixFileIo::RemoveTempFile();
            return false;
        }
    }

    //Replace the old file with our new one.
    ofh.close();
    return OverwriteOld(MixFileIo::TempFilePath);
}

bool MixFile::RemoveFile(const std::string &name)
{
    return RemoveFile(MixId::IdGen(m_header.GetGame(), BaseName(name)));
}

bool MixFile::RemoveFile(int32_t id)
{
    IndexInfo lmd;
    IndexInfo rem;
    std::fstream ofh;

    //empty the skip map
    m_skip.clear();

    //save the old data offset from header before we started changing it.
    const std::streamoff old_offset = m_header.GetHeaderSize();
    const std::streamoff old_size = old_offset + m_header.GetBodySize();

    //set up to skip copying the lmd if the mix contains one
    lmd = m_header.GetEntry(MixId::IdGen(m_header.GetGame(), m_local_db.GetDbName()));
    rem = m_header.GetEntry(id);

    if (!rem.size)
        return false;

    //add skip entry for lmd if we have one and remove from header
    if (lmd.size)
    {
        m_skip[lmd.offset] = lmd.size;
        m_header.RemoveEntry(MixId::IdGen(m_header.GetGame(),
                                          m_local_db.GetDbName()),
                             true);
    }

    //add our file to the skip map and remove it
    m_skip[rem.offset] = rem.size;
    m_header.RemoveEntry(id, true);
    m_local_db.DeleteName(id);

    //re-add our lmd entry if we had one will recalc its position
    if (lmd.size)
    {
        m_header.AddEntry(MixId::IdGen(m_header.GetGame(), m_local_db.GetDbName()),
                          m_local_db.GetSize());
    }

    //open a temp file
    ofh.open(
        std::filesystem::u8path(MixFileIo::TempFilePath),
        std::ios::binary | std::ios::out | std::ios::trunc);
    if (!ofh.is_open())
    {
        std::println("Couldn't open a temporary file to buffer the changes");
        return false;
    }

    //write our new header
    if (!m_header.WriteHeader(ofh))
    {
        return false;
    }

    //copy the body of the old mix, skipping files in m_skip
    if (!MixFileIo::CopyBodyWithSkips(fh, ofh, old_offset, old_size, m_skip))
    {
        ofh.close();
        MixFileIo::RemoveTempFile();
        return false;
    }

    //write lmd if needed
    if (lmd.size)
    {
        m_local_db.WriteDb(ofh);
    }

    //write checksum to end of file if required. will have to overwrite old one
    if (m_header.GetHasChecksum())
    {
        if (!WriteChecksum(ofh, 0))
        {
            ofh.close();
            MixFileIo::RemoveTempFile();
            return false;
        }
    }

    ofh.close();
    return OverwriteOld(MixFileIo::TempFilePath);
}

bool MixFile::AddChecksum()
{
    //check if we think this file is checksummed already
    if (m_header.GetHasChecksum())
    {
        std::println("File is already flagged as having a checksum");
        return false;
    }

    //toggle flag for checksum and then write it
    m_header.SetHasChecksum();
    fh.seekp(0, std::ios::beg);
    m_header.WriteHeader(fh);

    //write the actual checksum
    return WriteChecksum(fh);
}

bool MixFile::RemoveChecksum()
{
    std::fstream ofh;
    const std::streamoff old_offset = m_header.GetHeaderSize();
    const std::streamoff old_size = old_offset + m_header.GetBodySize();

    //check if we think this file is checksummed already
    if (!m_header.GetHasChecksum())
    {
        std::println("File is already flagged as not having a checksum");
        return false;
    }

    ofh.open(
        std::filesystem::u8path(MixFileIo::TempFilePath),
        std::ios::binary | std::ios::out | std::ios::trunc);
    if (!ofh.is_open())
    {
        std::println("Couldn't open a temporary file to buffer the changes");
        return false;
    }

    //toggle flag for checksum and then write it
    m_header.ClearHasChecksum();
    m_header.WriteHeader(ofh);

    fh.clear();
    fh.seekg(old_offset, std::ios::beg);
    if (!MixFileIo::CopyExact(fh, ofh, old_size - old_offset))
    {
        ofh.close();
        MixFileIo::RemoveTempFile();
        return false;
    }

    ofh.close();
    return OverwriteOld(MixFileIo::TempFilePath);
}

bool MixFile::WriteChecksum(std::fstream &fh, int32_t pos)
{
    SHA1 sha1;
    const std::size_t BufferSize = 144 * 7 * 1024;
    std::vector<uint8_t> buffer(BufferSize);
    uint8_t hash[20];

    //read data into sha1 algo from dataoffset
    fh.clear();
    fh.seekg(m_header.GetHeaderSize(), std::ios::beg);

    while (fh.good())
    {
        fh.read(reinterpret_cast<char *>(&buffer.at(0)),
                static_cast<std::streamsize>(BufferSize));
        const std::streamsize numBytesRead = fh.gcount();
        if (numBytesRead > 0)
        {
            sha1.Update(&buffer.at(0), static_cast<unsigned int>(numBytesRead));
        }
    }

    //clear stream
    fh.clear();

    // get our hash and print it to console as well
    sha1.Final(hash);
    std::copy(hash, hash + 20, m_checksum);
    std::println("Checksum is {}",
                 MixId::IdStr(reinterpret_cast<const char *>(hash), 20));

    //write checksum, pos is position from end to start at.
    fh.seekp(pos, std::ios::end);
    fh.write(reinterpret_cast<const char *>(hash), 20);

    return fh.good();
}

void MixFile::PrintFileList()
{
    MixIndexIterator it = m_header.GetBegin();
    while (it != m_header.GetEnd())
    {
        const std::string fname = ResolveName(it->first);
        std::println("{:>24}{:>10}{:>12}{:>12}",
                     fname, MixId::IdStr(it->first), it->second.offset, it->second.size);
        it++;
    }
}

std::string MixFile::ResolveName(const int32_t id) const
{
    std::string name = m_local_db.GetName(id);
    if (name.substr(0, 4) == "[id]")
    {
        name = m_global_db.GetName(m_header.GetGame(), id);
    }

    return name;
}

std::vector<int32_t> MixFile::CollectUnknownIds()
{
    std::vector<int32_t> ids;
    for (MixIndexIterator it = m_header.GetBegin(); it != m_header.GetEnd(); ++it)
    {
        if (ResolveName(it->first).substr(0, 4) == "[id]")
        {
            ids.push_back(it->first);
        }
    }

    return ids;
}

void MixFile::PrintInfo()
{
    const bool hasExtendedLayout = MixFileInfoSupport::HasExtendedHeaderLayout(m_header);
    const bool isEncrypted = m_header.GetIsEncrypted();
    const uint32_t fileCount = static_cast<uint32_t>(m_header.GetFileCount());
    const uint32_t plainIndexSize = fileCount * 12u;

    std::println("Archive: {}", m_file_path);
    std::println("Header:");
    std::println("  Game format:        {}",
                 MixFileInfoSupport::GameLabel(m_header.GetGame()));
    std::println("  Layout:             {}",
                 MixFileInfoSupport::HeaderLayoutLabel(m_header));
    std::println("  Header size:        {} bytes", m_header.GetHeaderSize());
    std::println("  Body offset:        {} bytes", m_header.GetHeaderSize());
    std::println("  File count:         {}", m_header.GetFileCount());
    std::println("  Body size:          {} bytes", m_header.GetBodySize());
    std::println("  Plain index size:   {} bytes", plainIndexSize);

    if (isEncrypted)
    {
        std::println("  Flags block:        4 bytes");
        std::println("  Header flags:       {:#010x}", m_header.GetHeaderFlags());
        std::println("  Key source block:   80 bytes");
        std::println("  Encrypted blob:     {} bytes",
                     m_header.GetHeaderSize() - 84u);
    }
    else
    {
        std::println("  Flags block:        {}",
                     hasExtendedLayout ? "present" : "absent");
        if (hasExtendedLayout)
        {
            std::println("  Header flags:       {:#010x}", m_header.GetHeaderFlags());
            std::println("  Metadata block:     10 bytes");
            std::println("  Index block:        {} bytes",
                         m_header.GetHeaderSize() - 10u);
        }
        else
        {
            std::println("  Metadata block:     6 bytes");
            std::println("  Index block:        {} bytes",
                         m_header.GetHeaderSize() - 6u);
        }
    }

    std::println("  Encrypted:          {}", isEncrypted ? "yes" : "no");
    std::println("  Has checksum:       {}",
                 m_header.GetHasChecksum() ? "yes" : "no");

    if (m_header.GetIsEncrypted())
    {
        std::println("");
        std::println("Encryption:");
        std::println("  Blowfish key:       {}",
                     MixId::IdStr(m_header.GetKey(), 56));
        std::println("  Key source:         {}",
                     MixId::IdStr(m_header.GetKeySource(), 80));
    }
    if (m_header.GetHasChecksum())
    {
        std::println("");
        std::println("Checksum:");
        std::println("  SHA1:               {}",
                     MixId::IdStr(reinterpret_cast<const char *>(m_checksum), 20));
    }
}

bool MixFile::Decrypt()
{
    std::fstream ofh;

    //are we already decrypted?
    if (!m_header.GetIsEncrypted())
        return false;

    //get some info on original file and then set header to decrypted
    uint32_t dataoffset = m_header.GetHeaderSize();
    m_header.ClearIsEncrypted();

    ofh.open(
        std::filesystem::u8path(MixFileIo::TempFilePath),
        std::fstream::out | std::fstream::binary | std::fstream::trunc);
    if (!m_header.WriteHeader(ofh))
    {
        return false;
    }

    fh.seekg(dataoffset);

    if (!MixFileIo::CopyToEnd(fh, ofh))
    {
        ofh.close();
        MixFileIo::RemoveTempFile();
        return false;
    }

    ofh.close();

    return OverwriteOld(MixFileIo::TempFilePath);
}

bool MixFile::Encrypt()
{
    std::fstream ofh;

    //are we already encrypted?
    if (m_header.GetIsEncrypted())
        return false;

    //get some info on original file and then set header to decrypted
    uint32_t dataoffset = m_header.GetHeaderSize();
    m_header.SetIsEncrypted();

    ofh.open(
        std::filesystem::u8path(MixFileIo::TempFilePath),
        std::fstream::out | std::fstream::binary | std::fstream::trunc);
    if (!m_header.WriteHeader(ofh))
    {
        return false;
    }

    fh.seekg(dataoffset);

    if (!MixFileIo::CopyToEnd(fh, ofh))
    {
        ofh.close();
        MixFileIo::RemoveTempFile();
        return false;
    }

    ofh.close();

    return OverwriteOld(MixFileIo::TempFilePath);
}

bool MixFile::OverwriteOld(const std::string &temp)
{
    Close();

    std::error_code error;
    if (!std::filesystem::remove(std::filesystem::u8path(m_file_path), error) ||
        error)
    {
        std::println("Failed to remove the original mix file");
        return false;
    }

    error.clear();
    std::filesystem::rename(
        std::filesystem::u8path(temp), std::filesystem::u8path(m_file_path),
        error);
    if (error)
    {
        std::println("Failed to rename the temporary mix file");
        return false;
    }

    return Open(m_file_path);
}

void MixFile::Close()
{
    if (fh.is_open())
    {
        fh.close();
    }
    fh.clear();
    m_has_lmd = false;
}
