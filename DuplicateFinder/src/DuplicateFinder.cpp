#include "DuplicateFinder.hpp"
#include "CUL/CULInterface.hpp"
#include "CUL/ITimer.hpp"
#include "CUL/Filesystem/Path.hpp"

DuplicateFinder::DuplicateFinder()
{
    m_culInterface.reset( CUL::CULInterface::createInstance() );
}

void DuplicateFinder::search( const std::string& path )
{
    m_culInterface->getLogger()->log( CUL::String( "Start search." ) );

    auto culFF = m_culInterface->getFS();

    std::vector<CUL::FS::Path> FilesInDir = culFF->ListAllFiles( path );

    const float fileSizes = (float)FilesInDir.size();

    while( !FilesInDir.empty() )
    {
        const CUL::FS::Path currentFile = *FilesInDir.rbegin();
        FilesInDir.pop_back();

        if( CUL::FS::FSApi::isDirectory( currentFile ) )
        {
            continue;
        }

        while( getCurrentThreadCount() >= m_maxThreadCount )
        {
            std::lock_guard<std::mutex> functionLock( m_tasksMtx );
            while( !m_tasks.empty() )
            {
                std::function<void()> task = *m_tasks.rbegin();
                m_tasks.pop_back();
                task();
            }
        }

        std::thread fileThread( &DuplicateFinder::addFile, this, currentFile.getPath().string() );
        std::thread::id threadId = fileThread.get_id();
        m_threads[threadId] = std::move(fileThread);

        const float currentSize = 100.f * FilesInDir.size();

        const int size = currentSize / fileSizes;
        if( m_last == size )
        {
        }
        else
        {
            m_culInterface->getLogger()->log( CUL::String( "DONE: " ) + CUL::String( size ) );
            m_last = size;
        }
    }

    for( const auto& sizesGroup : m_duplicates )
    {
        m_culInterface->getLogger()->log( CUL::String( "Sizes:" ) + CUL::String( sizesGroup.first ) );
        for( const auto& md5Group : sizesGroup.second )
        {
            m_culInterface->getLogger()->log( CUL::String( "MD5:" ) + CUL::String( md5Group.first ) );
            for( const auto& paths: md5Group.second )
            {
                m_culInterface->getLogger()->log( CUL::String( "Path:" ) + CUL::String( paths.getPath() ) );
            }
        }
    }

    m_culInterface->getLogger()->log( "\nDONE.");
}

void DuplicateFinder::addFile( const std::string& path )
{
    std::unique_ptr<CUL::FS::IFile> file;
    file.reset( m_culInterface->getFF()->createRegularFileRawPtr( path ) );

    const auto& md5 = file->getMD5();
    FileSize sizeBytes = file->getSizeBytes();

    std::lock_guard<std::mutex> lock( m_duplicatesMtx );

    const auto it = m_filesPathsMap.find( sizeBytes );
    if( it != m_filesPathsMap.end() )
    {
        std::map<MD5Value, CUL::FS::Path>& sameSizeFiles = it->second;
        auto md5It = sameSizeFiles.find( md5 );
        if( md5It != sameSizeFiles.end() )
        {
            CUL::String oldFile = md5It->second.getPath();
            m_duplicates[sizeBytes][md5].insert( oldFile );
            m_duplicates[sizeBytes][md5].insert( path );
        }
        else
        {
            sameSizeFiles[md5] = path;
        }
    }
    else
    {
        Value sameSizeFiles;
        sameSizeFiles[md5] = path;
        m_filesPathsMap[sizeBytes] = sameSizeFiles;
    }

    std::lock_guard<std::mutex> functionLock( m_tasksMtx );
    std::thread::id threadId = std::this_thread::get_id();
    m_tasks.push_back( [this, threadId]() {
        auto threadIt = m_threads.find( threadId );
        auto& thread = threadIt;
        thread->second.join();
        m_threads.erase( threadId );
    } );
}

size_t DuplicateFinder::getCurrentThreadCount()
{
    std::lock_guard<std::mutex> lockGuard( m_threadsMtx );
    return m_threads.size();
}

DuplicateFinder::~DuplicateFinder()
{
}