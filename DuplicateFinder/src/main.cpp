#include "CUL/CULInterface.hpp"
#include "CUL/Filesystem/FS.hpp"
#include "CUL/Filesystem/IFile.hpp"
#include "CUL/STL_IMPORTS/STD_map.hpp"
#include "CUL/STL_IMPORTS/STD_vector.hpp"

int main( int argc, char** argv )
{
    CUL::CULInterface* cul = CUL::CULInterface::createInstance();
    auto culFF = cul->getFS();
    using FileSize = unsigned;
    using MD5Value = CUL::String;

    auto FilesInDir = culFF->ListAllFiles( CUL::FS::Path( "." ) );

    using Value = std::map<MD5Value, CUL::FS::Path>;
  
    std::map<FileSize, Value> filesPathsMap;

    std::map<FileSize, std::map<MD5Value, std::vector<CUL::FS::Path>>> duplicates;

    for( const auto& path : FilesInDir )
    {
        if( CUL::FS::FSApi::isDirectory( path ) )
        {
            continue;
        }

        CUL::FS::IFile* file = cul->getFF()->createFileFromPath( path );

        const auto& md5 = file->getMD5();
        FileSize sizeBytes = file->getSizeBytes();
        const auto it = filesPathsMap.find( sizeBytes );
        if( it != filesPathsMap.end() )
        {
            std::map<MD5Value, CUL::FS::Path>& sameSizeFiles = it->second;
            auto md5It = sameSizeFiles.find(md5);
            if( md5It != sameSizeFiles.end() )
            {
                CUL::String oldFile = md5It->second.getPath();
                cul->getLogger()->log( CUL::String( "FOUND COLISSION, OLD FILE: " ) + oldFile + ", second file: " + CUL::String( path ) );
            }
            else
            {
                sameSizeFiles[md5] = path.getPath();
            }
        }
        else
        {
            Value sameSizeFiles;
            sameSizeFiles[md5] = path.getPath();
            filesPathsMap[sizeBytes] = sameSizeFiles;
        }
    }

    delete cul;
    return 0;
}