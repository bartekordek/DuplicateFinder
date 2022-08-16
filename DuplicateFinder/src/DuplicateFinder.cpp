#include "DuplicateFinder.hpp"
#include "CUL/CULInterface.hpp"
#include "CUL/ITimer.hpp"
#include "CUL/Filesystem/Path.hpp"
#include "CUL/IMPORT_sqlite3.hpp"
#include "CUL/Filesystem/FSApi.hpp"
#include "CUL/ITimer.hpp"
#include "CUL/Filesystem/FileFactory.hpp"

DuplicateFinder* DuplicateFinder::s_instance = nullptr;

DuplicateFinder::DuplicateFinder()
{
    s_instance = this;

    m_culInterface.reset( CUL::CULInterface::createInstance() );

    initDb();
}

void DuplicateFinder::initDb()
{
    int rc = sqlite3_open( "FilesList.db", &m_db );

    std::string sqlQuery = "SELECT * \
    FROM FILES";

    auto callback = []( void* , int , char** , char** )
    {
        //DuplicateFinder::s_instance->callback(NotUsed, argc, argv, azColName);
        return 0;
    };

    char* zErrMsg = nullptr;
    std::lock_guard<std::mutex> sqliteLock( m_sqliteMtx );
    rc = sqlite3_exec( m_db, sqlQuery.c_str(), nullptr, 0, &zErrMsg );

    if( rc != SQLITE_OK )
    {
        sqlQuery =
            "CREATE TABLE FILES (\
        PATH varchar(1024),\
        SIZE varchar(512),\
        MD5 varchar(1024),\
        LAST_MODIFICATION varchar(1024)\
        );";
        std::string errorResult( zErrMsg );
        rc = sqlite3_exec( m_db, sqlQuery.c_str(), callback, 0, &zErrMsg );
    }
}

int DuplicateFinder::callback( void*, int argc, char** argv, char** azColName )
{
    m_culInterface->getLogger()->log( "\n\n---------------------------------------------------------------------------------" );
    for( int i = 0; i < argc; ++i )
    {
        char* colName = azColName[i];
        CUL::String logVal = CUL::String( colName ) + CUL::String( " = " ) + CUL::String( argv[i] ? argv[i] : "NULL" );
        m_culInterface->getLogger()->log( logVal );
    }
    m_culInterface->getLogger()->log( "---------------------------------------------------------------------------------\n\n" );
    return 0;
}

void DuplicateFinder::search( const std::string& path, const std::string& summaryFilePath )
{
    m_culInterface->getLogger()->log( CUL::String( "Start search." ) );

    auto culFF = m_culInterface->getFS();

    std::vector<CUL::FS::Path> filesInDir = culFF->ListAllFiles( path );

    const auto fileSizes = filesInDir.size();
    m_filesCount = (float)fileSizes;
    m_filesLeft = m_filesCount;

    for( size_t i = 0; i < fileSizes; ++i)
    {
        const CUL::FS::Path& file = filesInDir[i];
        if( file.getIsDir())
        {
            continue;
        }

        addTask( [this,file]() {
            addFile(file.getPath().string());
        } );
    }

    for( size_t i = 0; i < m_maxThreadCount; ++i )
    {
        std::thread fileThread( &DuplicateFinder::workerThreadMethod, this );
        std::thread::id threadId = fileThread.get_id();
        m_workers[threadId] = std::move( fileThread );
    }

    for( auto& thread : m_workers )
    {
        if( thread.second.joinable() )
        {
            thread.second.join();
        }
    }

    CUL::String filePath;
    if( summaryFilePath.empty() )
    {
        filePath = "Result.txt";
    }
    else
    {
        filePath = summaryFilePath;
    }

    auto file = m_culInterface->getFF()->createRegularFileRawPtr( CUL::FS::Path( filePath ) );

    CUL::String text;
    for( const auto& sizesGroup : m_duplicates )
    {
        text = CUL::String( "Sizes: " ) + CUL::String( sizesGroup.first );
        m_culInterface->getLogger()->log( text );
        file->addLine( text );

        for( const auto& md5Group : sizesGroup.second )
        {
            text = CUL::String( "MD5: " ) + CUL::String( md5Group.first );
            m_culInterface->getLogger()->log( text );
            file->addLine( text );
            for( const auto& paths: md5Group.second )
            {
                m_culInterface->getLogger()->log( paths.getPath() );
                file->addLine( text );
            }
        }
    }
    file->saveFile();

    delete file;

    printCurrentMean();

    m_culInterface->getLogger()->log( "\nDONE.");
}

void DuplicateFinder::addFile( const CUL::String& path )
{
    CUL::ITimer* m_frameTimer = CUL::TimerFactory::getChronoTimer();
    m_frameTimer->start();

    std::unique_ptr<CUL::FS::IFile> file;
    file.reset( m_culInterface->getFF()->createRegularFileRawPtr( path ) );

    auto modTime = file->getLastModificationTime();
    auto modTimeFromDb = getModTimeFromDb( path );
    const auto modTimeFromFS = modTime.toString();

    CUL::String md5;
    CUL::String sizeBytes;

    if( modTimeFromFS != modTimeFromDb )
    {
        md5 = file->getMD5();
        sizeBytes = file->getSizeBytes();
        addFileToDb( md5, path, CUL::String( sizeBytes ), modTime.toString() );
    }
    else
    {
        m_culInterface->getLogger()->log( CUL::String( "Path: " ) + path + " found on cache." );
        std::lock_guard<std::mutex> lockGuard( m_filesFromDbMtx );
        auto it = m_filesFromDb.find( path );
        md5 = it->second.md5;
        sizeBytes = it->second.size;
    }

    {
        std::lock_guard<std::mutex> lock( m_duplicatesMtx );

        std::lock_guard<std::mutex> lockMap( m_filesPathsMapMtx );
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
    }

    float tasksLeft = (float)getTasksLeft();
    float percentage = 100.f * ( 1 - ( tasksLeft / m_filesCount ) );
    m_culInterface->getLogger()->log( CUL::String(percentage) + "%, " + CUL::String( "Path: " ) + path + " Done." );
    m_frameTimer->stop();
    const auto& elapsed = m_frameTimer->getElapsed();
    const unsigned elapsedUi = elapsed.getMs();
    m_fileAddTasksDuration.push_back( elapsedUi );
    delete m_frameTimer;
    printCurrentMean();
}

CUL::String DuplicateFinder::getModTimeFromDb( const CUL::String& filePath )
{
    getParametersFromDb( filePath );
    CUL::String result;
    std::lock_guard<std::mutex> lockGuard( m_filesFromDbMtx );
    auto it = m_filesFromDb.find( filePath );
    if( it != m_filesFromDb.end() )
    {
        result = it->second.modTime;
    }
    return result;
}

void DuplicateFinder::printCurrentMean()
{
    float sum = 0.f;
    std::lock_guard<std::mutex> lock( m_fileAddTasksDurationMtx );
    size_t vSize = m_fileAddTasksDuration.size();
    for( size_t i = 0; i < vSize; ++i )
    {
        sum += m_fileAddTasksDuration[i];
    }

    float mean = sum / vSize;

    m_culInterface->getLogger()->log( CUL::String( "Mean: " ) + CUL::String( mean ) );
}

void DuplicateFinder::getParametersFromDb( const CUL::String& filePath )
{
    CUL::String filePathNormalized = filePath;
    filePathNormalized.replace( "./", "" );
    filePathNormalized.removeAll( '\0' );
    std::string sqlQuery =
        std::string( "SELECT PATH, SIZE, MD5, LAST_MODIFICATION FROM FILES WHERE PATH='" ) + filePathNormalized.string() + "';";
    char* zErrMsg = nullptr;

    auto callback = []( void*, int argc, char** argv, char** ) {
        // DuplicateFinder::s_instance->callback( NotUsed, argc, argv, azColName );
        std::vector<CUL::String> values;
        for( int i = 0; i < argc; ++i )
        {
            CUL::String value = argv[i];
            values.push_back( value );
        }
        s_instance->addFileFromDb( values[0], values[1], values[2], values[3] );
        return 0;
    };

    std::lock_guard<std::mutex> sqliteLock( m_sqliteMtx );
    int rc = sqlite3_exec( m_db, sqlQuery.c_str(), callback, 0, &zErrMsg );

    if( rc != SQLITE_OK )
    {
        CUL::Assert::simple( false, "DB ERROR!" );
    }
}

void DuplicateFinder::addFileFromDb( const CUL::String& path, const CUL::String& size, const CUL::String& md, const CUL::String& modTime )
{
    std::lock_guard<std::mutex> lockGuard( m_filesFromDbMtx );
    FileDb fdb;
    fdb.size = size;
    fdb.md5 = md;
    fdb.modTime = modTime;
    m_filesFromDb[path] = fdb;
}

void DuplicateFinder::parseArguments( int argc, char** argv )
{
    m_consoleUtilities.setArgs( argc, argv );
}

void DuplicateFinder::addFileToDb( MD5Value md5, const CUL::String& filePath, const CUL::String& fileSize, const CUL::String& modTime )
{
    CUL::String result;
    CUL::String filePathNormalized = filePath;
    filePathNormalized.replace( "./", "" );
    filePathNormalized.removeAll('\0');
    CUL::String sqlQuery = "INSERT INTO FILES (PATH, SIZE, MD5, LAST_MODIFICATION ) VALUES ('" + filePathNormalized + "', '" + fileSize +
                           "', '" + md5 +
                           "', '" + modTime + "');";

    char* zErrMsg = nullptr;
    auto callback = []( void*, int , char** , char** )
    {
       // DuplicateFinder::s_instance->callback( NotUsed, argc, argv, azColName );
        return 0;
    };
    std::lock_guard<std::mutex> sqliteLock( m_sqliteMtx );
    int rc = sqlite3_exec( m_db, sqlQuery.cStr(), callback, 0, &zErrMsg );

    if( rc != SQLITE_OK )
    {
        CUL::Assert::simple( false, "DB ERROR!" );
    }
}

void DuplicateFinder::workerThreadMethod()
{
    while( getTasksLeft() > 0 )
    {
        std::function<void()> task = getTask();
        task();
    }
}

unsigned DuplicateFinder::getTasksLeft()
{
    std::lock_guard<std::mutex> functionLock( m_tasksMtx );
    return (unsigned)m_tasks.size();
}

void DuplicateFinder::addTask( std::function<void()> task )
{
    std::lock_guard<std::mutex> functionLock( m_tasksMtx );
    m_tasks.push_back(task);
}

std::function<void()> DuplicateFinder::getTask()
{
    std::lock_guard<std::mutex> functionLock( m_tasksMtx );
    std::function<void()> task = *m_tasks.rbegin();
    m_tasks.pop_back();
    m_culInterface->getLogger()->log( CUL::String( "Tasks left:" ) + CUL::String( (int)m_tasks.size() ) );
    return task;
}

DuplicateFinder::~DuplicateFinder()
{
    sqlite3_close( m_db );
}