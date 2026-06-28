#pragma once

#include "Import.hpp"
#include "CUL/String/StringWrapper.hpp"
#include "CUL/Filesystem/FileDatabase.hpp"
#include "CUL/GenericUtils/NonCopyable.hpp"
#include "CUL/STL_IMPORTS/STD_vector.hpp"
#include "CUL/STL_IMPORTS/STD_mutex.hpp"

namespace CUL
{
class CULInterface;
}


namespace CUL::FS
{
enum class ELockType : std::uint8_t;
}

using String = CUL::StringWr;
using PathList = std::vector<CUL::StringWr>;
using FileInfo = CUL::FS::FileInfo;

class DuplicateFinderBase
{
public:
    DUP_FIN_BASE_API DuplicateFinderBase();

    DUP_FIN_BASE_API void startDBLoad();
    DUP_FIN_BASE_API String getModTimeFromDb( const String& filePath );
    DUP_FIN_BASE_API void getListOfSizes( std::vector<uint64_t>& out ) const;
    DUP_FIN_BASE_API void getFiles( uint64_t inSize, std::vector<FileInfo>& out ) const;
    DUP_FIN_BASE_API void getFiles( uint64_t size, const CUL::StringWr& md5, std::vector<FileInfo>& out ) const;
    DUP_FIN_BASE_API std::optional<FileInfo> getFileInfo_Impl( CUL::FS::ELockType inLockType, const CUL::StringWr& path ) const;
    DUP_FIN_BASE_API void removeFileFromDB( const String& pathRaw );

    DUP_FIN_BASE_API void addPath( const String& inPath );
    DUP_FIN_BASE_API void removePath( const String& inPath );
    DUP_FIN_BASE_API void removeLast();

    DUP_FIN_BASE_API bool isLoadingDb() const;
    DUP_FIN_BASE_API CUL::FS::CacheUsage getCacheUsage() const;

    DUP_FIN_BASE_API const PathList getPaths() const;
    DUP_FIN_BASE_API ~DuplicateFinderBase();

protected:
private:
    CUL::CULInterface& m_interface;
    mutable std::mutex m_searchPathsMtx;
    PathList m_searchPaths;

    std::atomic_bool m_loadingDb = false;
    bool m_initialDbFilesUpdated = false;
    std::unique_ptr<CUL::FS::FileDatabase> m_fileDb;

public:
    CUL_NONCOPYABLE( DuplicateFinderBase );
};