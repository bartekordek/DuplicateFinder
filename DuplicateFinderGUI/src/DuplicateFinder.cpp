#include "DuplicateFinder.hpp"

#include "gameengine/IGameEngine.hpp"
#include "gameengine/Camera.hpp"
#include "gameengine/Sprite.hpp"
#include "gameengine/Primitives/Quad.hpp"
#include "gameengine/Cube.hpp"
#include "gameengine/Components/TransformComponent.hpp"

#include "LOGLWAdditionalDeps/ImportImgui.hpp"

#include "CUL/IMPORT_sqlite3.hpp"
#include "CUL/Filesystem/FileFactory.hpp"
#include "CUL/Filesystem/FSApi.hpp"
#include "CUL/GenericUtils/ConsoleUtilities.hpp"

#include "CUL/STL_IMPORTS/STD_future.hpp"


App* App::s_instance = nullptr;

App::App( bool fullscreen, unsigned width, unsigned height, int x, int y, const char* winName, const char* configPath )
    : LOGLW::IGameEngineApp( fullscreen, width, height, x, y, winName, configPath, false )
{
}

void App::onInit()
{
    s_instance = this;

    m_culInterface = m_oglw->getCul();

    m_oglw->getCamera().setEyePos( { 0.f, 0.f, 8.f } );
    m_oglw->getCamera().setZnear( 0.1f );
    m_oglw->getCamera().getCenter().z = -512.f;

    m_oglw->drawDebugInfo( true );
    m_oglw->setBackgroundColor( LOGLW::ColorE::BLUE );


    const CUL::MATH::Angle pitch( 90.f, CUL::MATH::Angle::Type::DEGREE );

    m_oglw->guiFrameDelegate.addDelegate( [this](){guiIteration();} );

    m_mouseData = m_oglw->getMouseData();

    m_camera = &m_oglw->getCamera();

    m_oglw->setFpsLimit( 60 );
    m_thread.setBody( [this]() {
        timerThread();
    } );

    initDb();

    m_thread.run();
}

void App::initDb()
{
    int rc = sqlite3_open( "FilesList.db", &m_db );

    std::string sqlQuery =
        "SELECT * \
    FROM FILES";

    auto callback = []( void*, int, char**, char** )
    {
        // DuplicateFinder::s_instance->callback(NotUsed, argc, argv, azColName);
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

int App::callback( void*, int argc, char** argv, char** azColName )
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

void App::onWindowEvent( const SDL2W::WindowEvent::Type e )
{
    if( e == SDL2W::WindowEvent::Type::CLOSE )
    {
        m_runTimer = false;
        close();
    }
}

void App::onKeyBoardEvent( const SDL2W::IKey& key )
{
    if( key.getKeyIsDown() && key.getKeyName() == "W" )
    {
        m_camera->m_pos += m_front * m_velocity;
    }

    if( key.getKeyIsDown() && key.getKeyName() == "S" )
    {
        m_camera->m_pos -= m_front * m_velocity;
    }

    if( key.getKeyIsDown() && key.getKeyName() == "A" )
    {
        m_camera->m_pos -= m_right * m_velocity;
    }

    if( key.getKeyIsDown() && key.getKeyName() == "D" )
    {
        m_camera->m_pos += m_right * m_velocity;
    }

    if( key.getKeyIsDown() && key.getKeyName() == "," )
    {
    }

    if( key.getKeyIsDown() && key.getKeyName() == "." )
    {
    }

    if( key.getKeyIsDown() && key.getKeyName() == "Q" )
    {
        m_runTimer = false;
        close();
    }

    static float lookDelta = 0.05f;
    if( key.getKeyIsDown() && key.getKeyName() == "Up" )
    {
        updateEuler( m_yawLast, m_pitchLast + lookDelta );
    }

    if( key.getKeyIsDown() && key.getKeyName() == "Down" )
    {
        updateEuler( m_yawLast, m_pitchLast - lookDelta );
    }

    if( key.getKeyIsDown() && key.getKeyName() == "Left" )
    {
        updateEuler( m_yawLast - lookDelta, m_pitchLast );
    }

    if( key.getKeyIsDown() && key.getKeyName() == "Right" )
    {
        updateEuler( m_yawLast + lookDelta, m_pitchLast );
    }

    if( key.getKeyIsDown() && key.getKeyName() == "M" )
    {
    }

    if( key.getKeyIsDown() && key.getKeyName() == "N" )
    {
    }
}

void App::timerThread()
{
    while( m_runTimer )
    {
        m_time += 0.01f;
        CUL::ITimer::sleepMiliSeconds(16);
    }
}

void App::customFrame()
{

}

void App::guiIteration()
{
    auto context = m_oglw->getGuiContext();
    ImGui::SetCurrentContext( context );

    CUL::String name = "MAIN";
    ImGui::Begin( name.cStr(), nullptr,
                  ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoTitleBar );

    auto winSize = IGameEngineApp::m_sdlw->getMainWindow()->getSize();

    ImGui::SetWindowPos( { winSize.w * 0.2f, 0 } );

    const auto targetWidht = (float)winSize.w * 0.8f;
    const auto targetHeight = (float)winSize.h * 1.f;

    ImGui::SetWindowSize( { targetWidht, targetHeight } );

    if( ImGui::Button("Run") )
    {
        m_logger->log( "RUN!" );
        m_searchThread = std::thread( &App::search, this, "D:/GuglDrajwu", "Result.txt" );
    }

    static ImGuiTableFlags flags = ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_RowBg | ImGuiTableFlags_Borders |
                                   ImGuiTableFlags_Hideable | ImGuiTableFlags_Resizable;
    if( ImGui::BeginTable( "table1", 3, flags ) )
    {
        ImGui::TableSetupColumn( "Path", ImGuiTableColumnFlags_None, 200.f );
        ImGui::TableSetupColumn( "MD5", ImGuiTableColumnFlags_None, 200.f );
        ImGui::TableSetupColumn( "Size", ImGuiTableColumnFlags_None, 200.f );

        ImGui::TableHeadersRow();

        {
            std::lock_guard<std::mutex> lockGuard( m_allFilesMtx );
            const size_t filesSize = m_allFiles.size();

            for( size_t row = 0; row < filesSize; ++row )
            {
                ImGui::TableNextRow();
                FileDb& fileDb = m_allFiles[row];

                ImGui::TableSetColumnIndex( 0 );
                const char* pathAsChar = fileDb.path.cStr();
                ImGui::Text( pathAsChar, 0 );

                ImGui::TableSetColumnIndex( 1 );
                ImGui::Text( fileDb.md5.cStr(), 0 );

                ImGui::TableSetColumnIndex( 2 );
                ImGui::Text( fileDb.size.cStr(), 0 );
            }
        }


        ImGui::EndTable();
    }

    ImGui::End();
}

void App::onMouseEvent( const SDL2W::MouseData& mouseData )
{
    static auto windowSize = m_oglw->getMainWindow()->getSize();
    if( true && mouseData.isButtonDown( 1 ) )
    {
        const auto& md = m_oglw->getMouseData();
        auto mouseX = md.getX() - windowSize.getWidth() / 2;
        auto mouseY = md.getY() - windowSize.getHeight() / 2;

        if( m_firstMouse )
        {
            m_mouseLastX = mouseX;
            m_mouseLastY = mouseY;
            m_firstMouse = false;
        }

        int xoffset = mouseX - m_mouseLastX;
        int yoffset = m_mouseLastY - mouseY;  // reversed since y-coordinates go from bottom to top

        static float sensitivity = 0.002f;
        float newX = xoffset * sensitivity;
        float newY = yoffset * sensitivity;

        updateEuler( m_yawLast += newX, m_pitchLast += newY );

        m_mouseLastX = mouseX;
        m_mouseLastY = mouseY;

        m_front = glm::normalize( m_oglw->getCamera().getCenter() );
        m_right = glm::normalize( glm::cross( m_front, m_oglw->getCamera().getUp() ) );
    }
}

void App::updateEuler( float yaw, float pitch )
{
    auto eye = m_oglw->getCamera().getEye();

    CUL::String logVal = "YAW: " + CUL::String( yaw ) + CUL::String( ", PITCH: " ) + CUL::String( pitch );
    m_logger->log( logVal );

    m_lookAngles.yaw.setValue( yaw, CUL::MATH::Angle::Type::RADIAN );
    m_lookAngles.pitch.setValue( pitch, CUL::MATH::Angle::Type::RADIAN );

    glm::vec3 NewCenter = moveOnSphere( m_lookAngles.yaw.getRad(), m_lookAngles.pitch.getRad(), 0.f, 100.f );
    NewCenter += eye;
    m_oglw->getCamera().setCenter( NewCenter );


    m_yawLast = yaw;
    m_pitchLast = pitch;
}

glm::vec3 App::moveOnSphere( float yaw, float pitch, float /*row*/, float rad )
{
    glm::vec3 result;

    result.x = rad * std::cos( yaw ) * std::cos( pitch );
    result.y = rad * std::sin( pitch );
    result.z = rad * std::sin( yaw ) * std::cos( pitch );

    return result;
}

void App::search( const std::string& path, const std::string& summaryFilePath )
{
    m_culInterface->getLogger()->log( CUL::String( "Start search." ) );

    removeDeletedFilesFromDB();

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
        std::thread fileThread( &App::workerThreadMethod, this );
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


    CUL::String text = CUL::String( "Files: " );
    m_culInterface->getLogger()->log( text );
    file->addLine( text );

    for( const auto& sameSizeGroup : m_filesPathsMap )
    {
        text = CUL::String( "\tSize: " ) + CUL::String( sameSizeGroup.first );
        m_culInterface->getLogger()->log( text );
        file->addLine( text );
        for( const auto& sameMD5Group : sameSizeGroup.second )
        {
            text = CUL::String( "\t\tMD5: " ) + CUL::String( sameMD5Group.first );
            m_culInterface->getLogger()->log( text );
            file->addLine( text );

            bool duplicatesFound = false;
            if( sameMD5Group.second.size() > 1 )
            {
                text = CUL::String( "\t\tPossible Duplicates");
                duplicatesFound = true;
            }

            for( const auto& path : sameMD5Group.second )
            {
                text = CUL::String( "\t\t\tFile: " ) + CUL::String( path );
                m_culInterface->getLogger()->log( text );
                file->addLine( text );

                if( duplicatesFound )
                {
                    m_duplicates[sameSizeGroup.first] = std::map<MD5Value, std::set<CUL::FS::Path>>();
                    std::map<MD5Value, std::set<CUL::FS::Path>>& md5PathList = m_duplicates[sameSizeGroup.first];
                    md5PathList[sameMD5Group.first].insert( path );
                }
            }
        }
    }


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

    sqlite3_close( m_db );

    m_culInterface->getLogger()->log( "\nDONE.");
}


void App::workerThreadMethod()
{
    while( getTasksLeft() > 0 )
    {
        std::function<void()> task = getTask();
        task();
    }
}


void App::addTask( std::function<void()> task )
{
    std::lock_guard<std::mutex> functionLock( m_tasksMtx );
    m_tasks.push_back( task );
}


std::function<void()> App::getTask()
{
    std::lock_guard<std::mutex> functionLock( m_tasksMtx );
    std::function<void()> task = *m_tasks.rbegin();
    m_tasks.pop_back();
    m_culInterface->getLogger()->log( CUL::String( "Tasks left:" ) + CUL::String( (int)m_tasks.size() ) );
    return task;
}

unsigned App::getTasksLeft()
{
    std::lock_guard<std::mutex> functionLock( m_tasksMtx );
    return (unsigned)m_tasks.size();
}

void App::removeDeletedFilesFromDB()
{
    getList();

    const size_t deletionListSize = m_deletionList.size();

    for( size_t i = 0; i < deletionListSize; ++i )
    {
        CUL::FS::Path path = m_deletionList[i];
        if( !path.exists() )
        {
            removeFileFromDB( path );
        }
    }
}


void App::removeFileFromDB( const CUL::String& path )
{
    std::string sqlQuery = std::string( "DELETE FROM FILES WHERE PATH='" ) + path.string() + "';";

    char* zErrMsg = nullptr;
    auto callback = []( void*, int, char**, char** )
    {
        // DuplicateFinder::s_instance->callback( NotUsed, argc, argv, azColName );
        return 0;
    };
    std::lock_guard<std::mutex> sqliteLock( m_sqliteMtx );
    int rc = sqlite3_exec( m_db, sqlQuery.c_str(), callback, 0, &zErrMsg );

    if( rc != SQLITE_OK )
    {
        CUL::Assert::simple( false, "DB ERROR!" );
    }
}


void App::addFile( const CUL::String& path )
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
        if( it == m_filesPathsMap.end() )
        {
            Value sameSizeFiles;
            sameSizeFiles[md5].push_back( path );
            m_filesPathsMap[sizeBytes] = sameSizeFiles;
        }
        else
        {
            std::map<MD5Value, std::vector<CUL::FS::Path>>& sameSizeFiles = it->second;
            sameSizeFiles[md5].push_back( path );
        }
    }

    float tasksLeft = (float)getTasksLeft();
    float percentage = 100.f * ( 1 - ( tasksLeft / m_filesCount ) );
    m_culInterface->getLogger()->log( CUL::String( percentage ) + "%, " + CUL::String( "Path: " ) + path + " Done." );
    m_frameTimer->stop();
    const auto& elapsed = m_frameTimer->getElapsed();
    const unsigned elapsedUi = elapsed.getMs();
    m_fileAddTasksDuration.push_back( elapsedUi );
    delete m_frameTimer;
    printCurrentMean();

    FileDb fileDb;
    fileDb.md5 = md5;
    fileDb.modTime = modTime.toString();
    fileDb.size = sizeBytes;
    fileDb.path = path;

    std::lock_guard<std::mutex> lockGuard( m_allFilesMtx );
    m_allFiles.push_back( fileDb );
    std::sort( m_allFiles.begin(), m_allFiles.end(),
               []( const FileDb& fdb1, const FileDb& fdb2 )
               {
                   return fdb1.md5 > fdb2.md5;
               } );
}

void App::addFileToDb( MD5Value md5, const CUL::String& filePath, const CUL::String& fileSize, const CUL::String& modTime )
{
    CUL::String result;
    CUL::String filePathNormalized = filePath;
    filePathNormalized.replace( "./", "" );
    filePathNormalized.removeAll( '\0' );
    CUL::String sqlQuery = "INSERT INTO FILES (PATH, SIZE, MD5, LAST_MODIFICATION ) VALUES ('" + filePathNormalized + "', '" + fileSize +
                           "', '" + md5 + "', '" + modTime + "');";

    char* zErrMsg = nullptr;
    auto callback = []( void*, int, char**, char** )
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

CUL::String App::getModTimeFromDb( const CUL::String& filePath )
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


void App::getParametersFromDb( const CUL::String& filePath )
{
    CUL::String filePathNormalized = filePath;
    filePathNormalized.replace( "./", "" );
    filePathNormalized.removeAll( '\0' );
    std::string sqlQuery =
        std::string( "SELECT PATH, SIZE, MD5, LAST_MODIFICATION FROM FILES WHERE PATH='" ) + filePathNormalized.string() + "';";
    char* zErrMsg = nullptr;

    auto callback = []( void*, int argc, char** argv, char** )
    {
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


void App::addFileFromDb( const CUL::String& path, const CUL::String& size, const CUL::String& md, const CUL::String& modTime )
{
    std::lock_guard<std::mutex> lockGuard( m_filesFromDbMtx );
    FileDb fdb;
    fdb.size = size;
    fdb.md5 = md;
    fdb.modTime = modTime;
    m_filesFromDb[path] = fdb;
}

void App::addForCheckForDeletionList( const CUL::String& path )
{
    m_deletionList.push_back( path );
}

void App::getList()
{
    std::string sqlQuery = std::string( "SELECT PATH, SIZE, MD5, LAST_MODIFICATION FROM FILES" );
    auto callback = []( void*, int /* argc */, char** argv, char** )
    {
        s_instance->addForCheckForDeletionList( argv[0] );
        return 0;
    };

    std::lock_guard<std::mutex> sqliteLock( m_sqliteMtx );
    char* zErrMsg = nullptr;
    int rc = sqlite3_exec( m_db, sqlQuery.c_str(), callback, 0, &zErrMsg );

    if( rc != SQLITE_OK )
    {
        CUL::Assert::simple( false, "DB ERROR!" );
    }
}

void App::printCurrentMean()
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

App::~App()
{
    // BYE!
}

int main( int argc, char* args[] )
{
    CUL::GUTILS::ConsoleUtilities cu;
    cu.setArgs( argc, args );
    auto width = cu.getFlagValue( "-w" );
    auto height = cu.getFlagValue( "-h" );

    if( width.empty() || height.empty() )
    {
        width = "1280";
        height = "800";
    }

    App app( false, std::stoul( width.string() ), std::stoul( height.string() ), 256, 127, cu.getArgs().getArgsValVec()[0].cStr(),
             "Config.txt" );
    app.run();

    return 0;
}